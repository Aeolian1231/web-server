#include "timer_heap.hpp"
#include <algorithm>
#include <stdexcept>
#include <iostream>

void TimerHeap::addOrUpdate(int fd, TimePoint expiresAt) {
    // DEBUG
    std::cerr << "[timer_debug] addOrUpdate fd=" << fd
              << " expiresAt(ms)="
              << std::chrono::duration_cast<std::chrono::milliseconds>(expiresAt.time_since_epoch()).count()
              << "\n";

    auto it = index_.find(fd);
    if (it == index_.end()) {
        heap_.push_back({fd, expiresAt});
        size_t idx = heap_.size() - 1;
        index_[fd] = idx;
        siftUp(idx);
    } else {
        size_t idx = it->second;
        heap_[idx].expiresAt = expiresAt;
        // try siftDown then siftUp to restore heap property
        siftDown(idx);
        siftUp(idx);
    }
}

void TimerHeap::remove(int fd) {
    auto it = index_.find(fd);
    if (it == index_.end()) return;
    size_t idx = it->second;
    size_t last = heap_.size() - 1;
    if (idx != last) {
        swapNodes(idx, last);
    }
    heap_.pop_back();
    index_.erase(fd);
    if (idx < heap_.size()) {
        siftDown(idx);
        siftUp(idx);
    }
}

int TimerHeap::nextTimeoutMs(TimePoint now) const {
    if (heap_.empty()) return -1;
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(heap_.front().expiresAt - now).count();
    if (diff < 0) diff = 0;
    return static_cast<int>(diff);
}

std::vector<int> TimerHeap::popExpired(TimePoint now) {
    // DEBUG
    std::cerr << "[timer_debug] popExpired at(ms)="
              << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
              << " heap_size=" << heap_.size() << "\n";

    std::vector<int> expired;
    while (!heap_.empty()) {
        if (heap_.front().expiresAt <= now) {
            expired.push_back(heap_.front().fd);
            remove(heap_.front().fd);
        } else {
            break;
        }
    }
    return expired;
}

void TimerHeap::siftUp(size_t i) {
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (heap_[i].expiresAt < heap_[parent].expiresAt) {
            swapNodes(i, parent);
            i = parent;
        } else break;
    }
}

void TimerHeap::siftDown(size_t i) {
    size_t n = heap_.size();
    while (true) {
        size_t l = 2 * i + 1;
        size_t r = 2 * i + 2;
        size_t smallest = i;
        if (l < n && heap_[l].expiresAt < heap_[smallest].expiresAt) smallest = l;
        if (r < n && heap_[r].expiresAt < heap_[smallest].expiresAt) smallest = r;
        if (smallest != i) {
            swapNodes(i, smallest);
            i = smallest;
        } else break;
    }
}

void TimerHeap::swapNodes(size_t a, size_t b) {
    std::swap(heap_[a], heap_[b]);
    index_[heap_[a].fd] = a;
    index_[heap_[b].fd] = b;
}