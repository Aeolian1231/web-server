#pragma once
#include "http.hpp"
#include <string>

struct BuiltResponse {
    int status = 200;
    std::string bytes;     // 完整响应（header + body 或 header-only）
    bool close = true;     // 阶段2固定 close=true
};

BuiltResponse buildStaticFileResponse(const HttpRequest& req, const std::string& docRoot);