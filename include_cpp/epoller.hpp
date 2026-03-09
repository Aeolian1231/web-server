#pragma once
#include "noncopyable.hpp"

#include <sys/epoll.h>
#include <vector>

class Epoller : private NonCopyable {
public:
    Epoller();
    ~Epoller();

    void addFd(int fd, uint32_t events);
    void modFd(int fd, uint32_t events);
    void delFd(int fd);

    int wait(std::vector<epoll_event>& active, int timeoutMs);

private:
    int epfd_ = -1;
};