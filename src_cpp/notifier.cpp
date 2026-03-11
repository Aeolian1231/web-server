#include "notifier.hpp"
#include <unistd.h>
#include <cstdint>

Notifier::Notifier() {
    efd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd_ < 0) throw std::runtime_error("eventfd failed");
}
Notifier::~Notifier() {
    if (efd_ >= 0) ::close(efd_);
}

void Notifier::notifyWrite(int connFd) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        q_.push(connFd);
    }
    // write 1 to eventfd to wake reactor
    uint64_t v = 1;
    ::write(efd_, &v, sizeof(v)); // ignore errors
}

std::vector<int> Notifier::drainPending() {
    // read eventfd counter to reset
    uint64_t v;
    ::read(efd_, &v, sizeof(v)); // ignore errors

    std::vector<int> res;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        while (!q_.empty()) {
            res.push_back(q_.front());
            q_.pop();
        }
    }
    return res;
}