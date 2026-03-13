#pragma once
#include "http.hpp"
#include <string>

struct BuiltResponse {
    int status = 200;
    std::string bytes;     // 完整响应（header + (optional) body）
    long body_length = 0;  // 响应体长度（字节），HEAD 也要设置为真实文件长度
    bool close = true;
};

BuiltResponse buildStaticFileResponse(const HttpRequest& req, const std::string& docRoot);