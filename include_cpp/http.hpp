#pragma once
#include <string>
#include <unordered_map>
#include <cstddef>

struct HttpRequest {
    std::string method;
    std::string uri;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
};

enum class ParseResult { Incomplete, Ok, Bad };

// 输入 data/len，若解析到完整 header，consumed 返回 header 总长度（到 \r\n\r\n 末尾）
// 阶段2：只解析 header，不处理 body
ParseResult parseHttpRequest(const char* data, size_t len, size_t& consumed, HttpRequest& out);