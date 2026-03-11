#pragma once
#include "noncopyable.hpp"
#include <sys/eventfd.h>
#include <mutex>
#include <queue>
#include <vector>

class Notifier : private NonCopyable {
public:
    Notifier();
    ~Notifier();

    int fd() const { return efd_; }

    // worker call: push fd 到队列并notify
    void notifyWrite(int connFd);

    // reactor call: pop all pending fds (thread-safe)
    std::vector<int> drainPending();

private:
    int efd_ = -1;
    std::mutex mtx_;
    std::queue<int> q_;
};