#pragma once
#include "noncopyable.hpp"
#include <chrono>
#include <unordered_map>
#include <vector>

class TimerHeap : private NonCopyable {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimerHeap() = default;

    // Add a new timer or update existing one for fd -> expiresAt.
    void addOrUpdate(int fd, TimePoint expiresAt);

    // Remove timer for fd (if exists).
    void remove(int fd);

    // Return next timeout in milliseconds for use with epoll_wait.
    // If no timers, return -1 (meaning epoll_wait can block indefinitely).
    int nextTimeoutMs(TimePoint now) const;

    // Pop and return all fds that have expired at 'now'.
    std::vector<int> popExpired(TimePoint now);

    // For debugging: number of timers
    size_t size() const { return heap_.size(); }

private:
    struct Node {
        int fd;
        TimePoint expiresAt;
    };

    std::vector<Node> heap_;
    std::unordered_map<int, size_t> index_; // fd -> index in heap_

    void siftUp(size_t i);
    void siftDown(size_t i);
    void swapNodes(size_t a, size_t b);
};