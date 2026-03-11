#include "http_response.hpp"

#include <fstream>
#include <sstream>

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
    // 阶段2：空 body
    std::ostringstream oss;
    if (code == 400) oss << "HTTP/1.1 400 Bad Request\r\n";
    else if (code == 404) oss << "HTTP/1.1 404 Not Found\r\n";
    else if (code == 405) oss << "HTTP/1.1 405 Method Not Allowed\r\n";
    else oss << "HTTP/1.1 500 Internal Server Error\r\n";

    oss << "Content-Length: 0\r\n"
        << "Connection: close\r\n\r\n";
    r.bytes = oss.str();
    return r;
}

BuiltResponse buildStaticFileResponse(const HttpRequest& req, const std::string& docRoot) {
    // method check
    if (!(req.method == "GET" || req.method == "HEAD")) {
        return simpleStatus(405);
    }
    // version check（简单）
    if (!(req.version == "HTTP/1.1" || req.version == "HTTP/1.0")) {
        return simpleStatus(400);
    }
    // basic path traversal defense
    if (hasPathTraversal(req.uri)) {
        return simpleStatus(404);
    }

    std::string path = req.uri;
    if (path == "/") path = "/index.html";

    std::string full = docRoot + path;

    // read file
    std::ifstream in(full, std::ios::binary);
    if (!in) return simpleStatus(404);

    std::string body;
    if (req.method == "GET") {
        std::ostringstream ss;
        ss << in.rdbuf();
        body = ss.str();
    } else {
        // HEAD：不读 body（也可以读但不返回；阶段2按要求返回空body即可）
        body.clear();
    }

    // file size：HEAD 也应返回真实长度。这里简单起见：HEAD 读文件长度但不返回 body
    // 为了正确 Content-Length，我们用 seek 获取长度（不依赖上面 body）
    in.clear();
    in.seekg(0, std::ios::end);
    std::streamoff fileLen = in.tellg();
    if (fileLen < 0) fileLen = 0;

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << mimeFromPath(full) << "\r\n"
        << "Content-Length: " << static_cast<long long>(fileLen) << "\r\n"
        << "Connection: close\r\n\r\n";

    BuiltResponse r;
    r.status = 200;
    r.bytes = oss.str();
    if (req.method == "GET") r.bytes += body;
    return r;
}