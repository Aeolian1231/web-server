#include "http_response.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>

static std::string mimeFromPath(const std::string& path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return "application/octet-stream";
    std::string ext = path.substr(dot + 1);
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "txt") return "text/plain";
    return "application/octet-stream";
}

static bool hasPathTraversal(const std::string& uri) {
    return uri.find("..") != std::string::npos;
}

static BuiltResponse simpleStatus(int code) {
    BuiltResponse r;
    r.status = code;
    r.body_length = 0;
    std::ostringstream oss;
    if (code == 400) oss << "HTTP/1.1 400 Bad Request\r\n";
    else if (code == 404) oss << "HTTP/1.1 404 Not Found\r\n";
    else if (code == 405) oss << "HTTP/1.1 405 Method Not Allowed\r\n";
    else oss << "HTTP/1.1 500 Internal Server Error\r\n";

    // 错误分支使用 Connection: close，简化错误处理
    oss << "Content-Length: 0\r\n"
        << "Connection: close\r\n\r\n";
    r.bytes = oss.str();
    r.close = true;
    return r;
}

// helper: case-insensitive header lookup
static bool headerContains(const std::unordered_map<std::string, std::string>& headers, const std::string& key, const std::string& token) {
    for (const auto& kv : headers) {
        std::string k = kv.first;
        std::string v = kv.second;
        // lower-case both
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        std::transform(v.begin(), v.end(), v.begin(), ::tolower);
        if (k == key) {
            if (v.find(token) != std::string::npos) return true;
        }
    }
    return false;
}

BuiltResponse buildStaticFileResponse(const HttpRequest& req, const std::string& docRoot) {
    if (!(req.method == "GET" || req.method == "HEAD")) {
        return simpleStatus(405);
    }
    if (!(req.version == "HTTP/1.1" || req.version == "HTTP/1.0")) {
        return simpleStatus(400);
    }
    if (hasPathTraversal(req.uri)) {
        return simpleStatus(404);
    }

    std::string path = req.uri;
    if (path == "/") path = "/index.html";
    std::string full = docRoot + path;

    std::ifstream in(full, std::ios::binary);
    if (!in) return simpleStatus(404);

    // get file length
    in.seekg(0, std::ios::end);
    std::streamoff fileLen = in.tellg();
    if (fileLen < 0) fileLen = 0;
    in.seekg(0, std::ios::beg);

    std::string body;
    if (req.method == "GET") {
        std::ostringstream ss;
        ss << in.rdbuf();
        body = ss.str();
    } else {
        body.clear();
    }

    // Decide keep-alive based on request version and Connection header
    bool clientKeepAlive = false;
    if (req.version == "HTTP/1.1") {
        clientKeepAlive = true; // default for HTTP/1.1
    } else {
        clientKeepAlive = false; // HTTP/1.0 default is close
    }

    // If client explicitly specified Connection header, honor it (case-insensitive)
    if (headerContains(req.headers, "connection", "close")) {
        clientKeepAlive = false;
    } else if (headerContains(req.headers, "connection", "keep-alive")) {
        clientKeepAlive = true;
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << mimeFromPath(full) << "\r\n"
        << "Content-Length: " << static_cast<long long>(fileLen) << "\r\n";

    if (clientKeepAlive) {
        oss << "Connection: keep-alive\r\n\r\n";
    } else {
        oss << "Connection: close\r\n\r\n";
    }

    BuiltResponse r;
    r.status = 200;
    r.body_length = static_cast<long long>(fileLen);
    r.bytes = oss.str();
    if (req.method == "GET") r.bytes += body;
    r.close = !clientKeepAlive;
    return r;
}