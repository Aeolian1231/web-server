// REPLACEMENT webserver_epoll.cpp with TimerHeap integrated and epoll_wait timeout cap
#include "conn.hpp"
#include "epoller.hpp"
#include "util.hpp"
#include "threadpool.hpp"
#include "notifier.hpp"
#include "http.hpp"
#include "http_response.hpp"
#include "timer_heap.hpp"
#include "async_logger.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

static constexpr int kPort = 9999;
static const std::string kDocRoot = "./static_site";
// idle timeout for connections
static const std::chrono::seconds kIdleTimeout = std::chrono::seconds(5);
// maximum epoll_wait sleep if timer says longer or timer empty (ms)
static const int kMaxEpollWaitMs = 1000;

static int createListenFd() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throwSys("socket");

    setReuseAddrPort(fd);
    setNonBlocking(fd);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kPort);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        closeNoThrow(fd);
        throwSys("bind");
    }
    if (::listen(fd, 1024) < 0) {
        closeNoThrow(fd);
        throwSys("listen");
    }
    return fd;
}

static void closeConn(Epoller& ep, std::unordered_map<int, std::shared_ptr<Conn>>& conns, TimerHeap& timer, int fd) {
    try { ep.delFd(fd); } catch(...) {}
    timer.remove(fd);
    auto it = conns.find(fd);
    if (it != conns.end()) {
        std::cerr << "[closeConn] removing fd=" << fd << " peer=" << it->second->peerIp() << "\n";
        it->second->closeFd();
        conns.erase(it);
    } else {
        std::cerr << "[closeConn] fd=" << fd << " not in conns, calling close()\n";
        ::close(fd);
    }
}

int main() {
    try {
        Epoller ep;
        int lfd = createListenFd();
        ep.addFd(lfd, EPOLLIN);

        unsigned int hw = std::thread::hardware_concurrency();
        size_t nthreads = hw ? static_cast<size_t>(hw) : 4;
        ThreadPool pool(nthreads);

        Notifier notifier;
        ep.addFd(notifier.fd(), EPOLLIN);

        AsyncLogger logger;
        logger.start();

        std::unordered_map<int, std::shared_ptr<Conn>> conns;
        std::vector<epoll_event> events(1024);
        TimerHeap timer;

        while (true) {
            // compute timeout from timer heap
            auto now = TimerHeap::Clock::now();
            int timeoutMs = timer.nextTimeoutMs(now);

            // CAP the epoll_wait timeout so we wake periodically even if timer is empty or large
            if (timeoutMs < 0 || timeoutMs > kMaxEpollWaitMs) timeoutMs = kMaxEpollWaitMs;

            // DEBUG: print next timeout and heap size
            std::cerr << "[timer_debug] nextTimeoutMs=" << timeoutMs
                      << " timer_size=" << timer.size() << "\n";

            int n = ep.wait(events, timeoutMs);
            if (n < 0) {
                if (errno == EINTR) continue;
                throwSys("epoll_wait");
            }

            now = TimerHeap::Clock::now();
            auto expired = timer.popExpired(now);

            // DEBUG: print expired list and conns size
            std::cerr << "[timer_debug] popExpired returned " << expired.size()
                    << " entries; conns.size()=" << conns.size() << " now(ms)="
                    << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
                    << "\n";

            for (int fd_exp : expired) {
                auto cit = conns.find(fd_exp);
                if (cit != conns.end()) {
                    std::cerr << "[timer] closing idle fd=" << fd_exp << " (found in conns)\n";
                    closeConn(ep, conns, timer, fd_exp);
                } else {
                    // expired timer but no active conn in map ¡ª log and ensure OS fd closed to be safe
                    std::cerr << "[timer] expired fd=" << fd_exp << " not found in conns; attempting close() to be safe\n";
                    // try safe close and remove timer entry (timer.remove already called by popExpired)
                    ::close(fd_exp); // best-effort; may be invalid but harmless
                }
            }

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                uint32_t ev = events[i].events;

                if (fd == lfd) {
                    while (true) {
                        sockaddr_in cli{};
                        socklen_t len = sizeof(cli);
                        int cfd = ::accept(lfd, reinterpret_cast<sockaddr*>(&cli), &len);
                        if (cfd < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                            throwSys("accept");
                        }
                        setNonBlocking(cfd);
                        char peerbuf[INET_ADDRSTRLEN] = {0};
                        inet_ntop(AF_INET, &cli.sin_addr, peerbuf, sizeof(peerbuf));
                        std::cerr << "[accept] fd=" << cfd << " peer=" << peerbuf << ":" << ntohs(cli.sin_port) << "\n";
                        auto connPtr = std::make_shared<Conn>(cfd, cli);
                        conns.emplace(cfd, connPtr);
                        ep.addFd(cfd, EPOLLIN | EPOLLRDHUP | EPOLLERR);
                        timer.addOrUpdate(cfd, TimerHeap::Clock::now() + kIdleTimeout);
                    }
                    continue;
                }

                if (fd == notifier.fd()) {
                    std::vector<int> pending = notifier.drainPending();
                    for (int wfd : pending) {
                        auto it = conns.find(wfd);
                        if (it == conns.end()) continue;
                        try { ep.modFd(wfd, EPOLLOUT | EPOLLRDHUP | EPOLLERR); } catch(...) {}
                    }
                    continue;
                }

                if (ev & (EPOLLERR | EPOLLRDHUP)) {
                    std::cerr << "[event] EPOLLERR/EPOLLRDHUP on fd=" << fd << "\n";
                    closeConn(ep, conns, timer, fd);
                    continue;
                }

                auto it = conns.find(fd);
                if (it == conns.end()) { ::close(fd); continue; }
                auto connPtr = it->second;

                if (ev & EPOLLIN) {
                    ssize_t header_len = connPtr->readIntoBuffer();
                    if (header_len < 0) {
                        closeConn(ep, conns, timer, fd);
                        continue;
                    }
                    // any read activity -> refresh timer
                    timer.addOrUpdate(fd, TimerHeap::Clock::now() + kIdleTimeout);

                    if (header_len == 0) {
                        // incomplete header
                        continue;
                    }

                    size_t bytes = static_cast<size_t>(header_len);
                    std::string headerCopy = connPtr->peekReadableAsString(bytes);

                    std::shared_ptr<Conn> workerConn = connPtr;
                    pool.submit([workerConn, headerCopy, &notifier, &logger]() {
                        HttpRequest req;
                        size_t consumed = 0;
                        ParseResult pr = parseHttpRequest(headerCopy.data(), headerCopy.size(), consumed, req);
                        std::string responseStr;
                        BuiltResponse br;
                        if (pr != ParseResult::Ok) {
                            responseStr = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                            br.status = 400;
                            br.body_length = 0;
                        } else {
                            br = buildStaticFileResponse(req, kDocRoot);
                            responseStr = br.bytes;
                        }
                        // set response and notify reactor
                        workerConn->setResponseFromWorker(responseStr);
                        // Log access asynchronously
                        std::string client_ip = workerConn->peerIp();
                        std::string request_line = req.method + " " + req.uri + " " + req.version;
                        logger.logAccess(client_ip, request_line, br.status, br.body_length);
                        notifier.notifyWrite(workerConn->fd());
                    });

                    // Reactor consumes header bytes
                    connPtr->retrieve(bytes);
                }

                if (ev & EPOLLOUT) {
                    bool shouldClose = connPtr->writeOut();
                    if (!shouldClose) {
                        timer.addOrUpdate(fd, TimerHeap::Clock::now() + kIdleTimeout);
                    }
                    if (shouldClose) {
                        std::cerr << "[close] EPOLLOUT finished, closing fd=" << fd << "\n";
                        closeConn(ep, conns, timer, fd);
                        continue;
                    }
                }
            } // for events
        } // while

        // unreachable here, but for completeness
        logger.stop();
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}