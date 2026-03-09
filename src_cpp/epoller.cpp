#include "epoller.hpp"

#include <stdexcept>
#include <string>

#include <sys/epoll.h>
#include <unistd.h>

static void throwEpoll(const char* what) {
    throw std::runtime_error(std::string(what));
}

Epoller::Epoller() {
    epfd_ = ::epoll_create1(0);
    if (epfd_ < 0) 
        throwEpoll("epoll_create1 failed");
}

Epoller::~Epoller() {
    if (epfd_ >= 0) 
        ::close(epfd_);
}

void Epoller::addFd(int fd, uint32_t events) {
    //把某个 fd 注册到 epoll 中，并指定要监听的事件
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) < 0) 
        throwEpoll("epoll_ctl ADD failed");
}

void Epoller::modFd(int fd, uint32_t events) {
    //用 EPOLL_CTL_MOD 修改已注册 fd 的监听事件
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    if (::epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) < 0) 
        throwEpoll("epoll_ctl MOD failed");
}

void Epoller::delFd(int fd) {
    //从 epoll 中删除某个 fd
    if (::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) < 0) 
        throwEpoll("epoll_ctl DEL failed");
}

int Epoller::wait(std::vector<epoll_event>& active, int timeoutMs) {
    //调用 epoll_wait 等待事件发生，把活跃事件写入 active 数组
    if (active.empty()) 
        active.resize(1024);
    int n = ::epoll_wait(epfd_, active.data(), static_cast<int>(active.size()), timeoutMs);
    if (n < 0) 
        return -1;
    return n;
}