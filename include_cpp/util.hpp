#pragma once
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

inline void throwSys(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

inline void setNonBlocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) throwSys("fcntl(F_GETFL)");
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) throwSys("fcntl(F_SETFL,O_NONBLOCK)");
}

inline void setReuseAddrPort(int fd) {
    int opt = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) throwSys("setsockopt(SO_REUSEADDR)");
#ifdef SO_REUSEPORT
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) throwSys("setsockopt(SO_REUSEPORT)");
#endif
}

inline void closeNoThrow(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}