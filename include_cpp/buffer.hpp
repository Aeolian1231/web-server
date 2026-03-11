#pragma once
#include <vector>
#include <string>
#include <cstddef>
#include <sys/types.h>

class Buffer {
public:
    Buffer();

    size_t readableBytes() const;
    const char* peek() const;

    void retrieve(size_t len);
    void retrieveAll();

    void append(const char* data, size_t len);
    void append(const std::string& s);

    // non-blocking read: 读到 EAGAIN/EWOULDBLOCK 结束
    // 返回：>0 本次读到的总字节数；0 表示对端关闭；-1 表示错误（errno在savedErrno）
    ssize_t readFd(int fd, int* savedErrno);

    // non-blocking write：尽量把 buffer 内容写出去
    // 返回：>=0 本次写出的字节数；-1 表示错误（errno在savedErrno）
    ssize_t writeFd(int fd, int* savedErrno);

private:
    std::vector<char> buf_;
    size_t readIdx_ = 0;
    size_t writeIdx_ = 0;

    size_t writableBytes() const;
    void ensureWritable(size_t len);
    void makeSpace(size_t len);
};