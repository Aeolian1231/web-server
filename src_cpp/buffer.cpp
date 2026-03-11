#include "buffer.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/uio.h>
#include <unistd.h>

Buffer::Buffer() {
    buf_.resize(4096);
}

size_t Buffer::readableBytes() const {
    return writeIdx_ - readIdx_;
}

size_t Buffer::writableBytes() const {
    return buf_.size() - writeIdx_;
}

const char* Buffer::peek() const {
    return buf_.data() + readIdx_;
}

void Buffer::retrieve(size_t len) {
    if (len >= readableBytes()) {
        retrieveAll();
        return;
    }
    readIdx_ += len;
}

void Buffer::retrieveAll() {
    readIdx_ = 0;
    writeIdx_ = 0;
}

void Buffer::ensureWritable(size_t len) {
    if (writableBytes() >= len) return;
    makeSpace(len);
}

void Buffer::makeSpace(size_t len) {
    // 先尝试搬移可读数据到前面
    if (readIdx_ > 0) {
        size_t readable = readableBytes();
        std::memmove(buf_.data(), buf_.data() + readIdx_, readable);
        readIdx_ = 0;
        writeIdx_ = readable;
    }
    if (writableBytes() >= len) return;

    // 再扩容
    buf_.resize(writeIdx_ + len);
}

void Buffer::append(const char* data, size_t len) {
    ensureWritable(len);
    std::memcpy(buf_.data() + writeIdx_, data, len);
    writeIdx_ += len;
}

void Buffer::append(const std::string& s) {
    append(s.data(), s.size());
}

ssize_t Buffer::readFd(int fd, int* savedErrno) {
    char extra[65536]; // 大块栈缓冲，减少扩容次数
    ssize_t total = 0;

    while (true) {
        struct iovec vec[2];
        vec[0].iov_base = buf_.data() + writeIdx_;
        vec[0].iov_len  = writableBytes();
        vec[1].iov_base = extra;
        vec[1].iov_len  = sizeof(extra);

        ssize_t n = ::readv(fd, vec, 2);
        if (n > 0) {
            total += n;
            if (static_cast<size_t>(n) <= vec[0].iov_len) {
                writeIdx_ += static_cast<size_t>(n);
            } else {
                writeIdx_ = buf_.size();
                append(extra, static_cast<size_t>(n - vec[0].iov_len));
            }
            continue; // 继续读到 EAGAIN
        }

        if (n == 0) return 0; // peer closed

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return total; // 读完了
        }

        if (savedErrno) *savedErrno = errno;
        return -1;
    }
}

ssize_t Buffer::writeFd(int fd, int* savedErrno) {
    ssize_t total = 0;
    while (readableBytes() > 0) {
        ssize_t n = ::write(fd, peek(), readableBytes());
        if (n > 0) {
            total += n;
            retrieve(static_cast<size_t>(n));
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return total;
        }
        if (savedErrno) *savedErrno = errno;
        return -1;
    }
    return total;
}