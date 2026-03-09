#include "epoller.hpp"
#include "util.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <vector>

static constexpr int kPort = 9999;

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

static void sendFixedResponseAndClose(int cfd) {
    // 最小可用响应
    const char body[] = "OK\n";
    char resp[256];
    int n = std::snprintf(resp, sizeof(resp),
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n\r\n"
                          "%s",
                          sizeof(body) - 1, body);
    (void)::send(cfd, resp, n, 0);
    ::close(cfd);
}

int main() {
    try {
        Epoller ep;
        int lfd = createListenFd();
        ep.addFd(lfd, EPOLLIN);

        std::vector<epoll_event> events(1024);

        while (true) {
            int n = ep.wait(events, -1);
            if (n < 0) {
                if (errno == EINTR) continue;
                throwSys("epoll_wait");
            }

            for (int i = 0; i < n; i++) {
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
                        ep.addFd(cfd, EPOLLIN | EPOLLRDHUP | EPOLLERR);
                    }
                    continue;
                }

                if (ev & (EPOLLERR | EPOLLRDHUP)) {
                    ::close(fd);
                    continue;
                }

                if (ev & EPOLLIN) {
                    char buf[4096];
                    while (true) {
                        ssize_t r = ::recv(fd, buf, sizeof(buf), 0);
                        if (r > 0) continue;
                        if (r == 0) { ::close(fd); break; }
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 收完了，回响应（里程碑1：固定响应）
                            sendFixedResponseAndClose(fd);
                            break;
                        }
                        ::close(fd);
                        break;
                    }
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << "\n";
        return 1;
    }
}