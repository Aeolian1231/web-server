#include "conn.hpp"
#include <arpa/inet.h>
#include <unistd.h>

Conn::Conn(int fd, const sockaddr_in& addr) : fd_(fd), addr_(addr) {}

ssize_t Conn::readIntoBuffer() {
    int savedErr = 0;
    ssize_t n = in_.readFd(fd_, &savedErr);
    if (n < 0) return -1;
    if (n == 0) return -1;
    size_t rd = in_.readableBytes();
    if (rd < 4) return 0;
    const char* p = in_.peek();
    for (size_t i = 3; i < rd; ++i) {
        if (p[i-3] == '\r' && p[i-2] == '\n' && p[i-1] == '\r' && p[i] == '\n') {
            return static_cast<ssize_t>(i+1);
        }
    }
    return 0;
}

size_t Conn::readableBytes() const { return in_.readableBytes(); }

std::string Conn::peekReadableAsString(size_t len) const {
    size_t rd = in_.readableBytes();
    if (len > rd) len = rd;
    const char* p = in_.peek();
    return std::string(p, p + len);
}

void Conn::retrieve(size_t len) { in_.retrieve(len); }

void Conn::setResponseFromWorker(const std::string& response) {
    std::lock_guard<std::mutex> lk(out_mtx_);
    out_.retrieveAll();
    out_.append(response.data(), response.size());
    responseReady_ = true;
}

bool Conn::writeOut() {
    std::lock_guard<std::mutex> lk(out_mtx_);
    int saved = 0;
    ssize_t n = out_.writeFd(fd_, &saved);
    if (n < 0) return true;
    if (out_.readableBytes() == 0) return true;
    return false;
}

void Conn::closeFd() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

std::string Conn::peerIp() const {
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return std::string(buf);
}