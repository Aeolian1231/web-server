#include "http.hpp"
#include <algorithm>

static inline std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t')) b++;
    size_t e = s.size();
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) e--;
    return s.substr(b, e - b);
}

ParseResult parseHttpRequest(const char* data, size_t len, size_t& consumed, HttpRequest& out) {
    consumed = 0;
    out = HttpRequest{};

    // 找到 header 结束
    const char* end = nullptr;
    for (size_t i = 3; i < len; i++) {
        if (data[i - 3] == '\r' && data[i - 2] == '\n' && data[i - 1] == '\r' && data[i] == '\n') {
            end = data + i + 1;
            consumed = (i + 1);
            break;
        }
    }
    if (!end) return ParseResult::Incomplete;

    // 逐行解析（简单实现）
    size_t pos = 0;
    auto readLine = [&](std::string& line) -> bool {
        if (pos >= consumed) return false;
        size_t start = pos;
        while (pos + 1 < consumed) {
            if (data[pos] == '\r' && data[pos + 1] == '\n') {
                line.assign(data + start, data + pos);
                pos += 2;
                return true;
            }
            pos++;
        }
        return false;
    };

    std::string line;
    if (!readLine(line)) return ParseResult::Bad;
    if (line.empty()) return ParseResult::Bad;

    // request line: METHOD SP URI SP VERSION
    {
        size_t p1 = line.find(' ');
        size_t p2 = (p1 == std::string::npos) ? std::string::npos : line.find(' ', p1 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos) return ParseResult::Bad;
        out.method = line.substr(0, p1);
        out.uri = line.substr(p1 + 1, p2 - (p1 + 1));
        out.version = line.substr(p2 + 1);
        if (out.method.empty() || out.uri.empty() || out.version.empty()) return ParseResult::Bad;
    }

    // headers
    while (readLine(line)) {
        if (line.empty()) break; // 读到空行
        size_t c = line.find(':');
        if (c == std::string::npos) continue; // 容错：跳过非法 header 行
        std::string key = trim(line.substr(0, c));
        std::string val = trim(line.substr(c + 1));
        if (!key.empty()) out.headers[key] = val;
    }

    return ParseResult::Ok;
}