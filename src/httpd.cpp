// 极简 HTTP/1.1 服务器，只为 poly 的本地工作台服务：
// bind 127.0.0.1 → accept → 每连接一个线程 → 读一个请求 → 回一个响应 → 关连接。
#include "httpd.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <thread>

#include "format.h"
#include "log.h"
#include "strutil.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace poly {

namespace {

#ifdef _WIN32
using Socket = SOCKET;
const Socket kBadSocket = INVALID_SOCKET;
#else
using Socket = int;
const Socket kBadSocket = -1;
#endif

void close_socket(Socket s) {
    if (s == kBadSocket) return;
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}

bool socket_error_is_timeout() {
#ifdef _WIN32
    return WSAGetLastError() == WSAETIMEDOUT;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

// 请求体上限：题目包可能很大，给到 512 MiB。
const size_t kMaxBody = static_cast<size_t>(512) * 1024 * 1024;
const size_t kMaxHeader = static_cast<size_t>(1024) * 1024;

std::string lower_key(const std::string& s) { return to_lower(trim(s)); }

void parse_urlencoded(const std::string& text, std::map<std::string, std::string>& out) {
    for (const std::string& pair : split(text, '&', false)) {
        if (pair.empty()) continue;
        size_t eq = pair.find('=');
        if (eq == std::string::npos) {
            out[url_decode(pair)] = std::string();
            continue;
        }
        // form 里的 '+' 表示空格
        std::string raw = pair.substr(eq + 1);
        std::string value = url_decode(replace_all(raw, "+", " "));
        out[url_decode(pair.substr(0, eq))] = value;
    }
}

std::string header_param(const std::string& header, const char* key) {
    std::string lower = to_lower(header);
    std::string needle = to_lower(std::string(key)) + "=";
    size_t p = lower.find(needle);
    if (p == std::string::npos) return std::string();
    size_t q = p + needle.size();
    if (q >= header.size()) return std::string();
    if (header[q] == '"') {
        size_t end = header.find('"', q + 1);
        if (end == std::string::npos) return std::string();
        return header.substr(q + 1, end - q - 1);
    }
    size_t end = header.find(';', q);
    return trim(header.substr(q, end == std::string::npos ? std::string::npos : end - q));
}

void parse_multipart(const std::string& body, const std::string& boundary, HttpRequest& req) {
    std::string delim = "--" + boundary;
    size_t pos = body.find(delim);
    while (pos != std::string::npos) {
        size_t after = pos + delim.size();
        if (body.compare(after, 2, "--") == 0) break;  // 结束标记
        size_t lineEnd = body.find("\r\n", after);
        if (lineEnd == std::string::npos) break;
        size_t headerEnd = body.find("\r\n\r\n", lineEnd);
        if (headerEnd == std::string::npos) break;
        std::string partHeaders = body.substr(lineEnd + 2, headerEnd - lineEnd - 2);
        size_t dataStart = headerEnd + 4;
        size_t next = body.find(delim, dataStart);
        if (next == std::string::npos) break;
        size_t dataEnd = next;
        if (dataEnd >= 2 && body.compare(dataEnd - 2, 2, "\r\n") == 0) dataEnd -= 2;
        std::string data = body.substr(dataStart, dataEnd - dataStart);

        std::string disposition;
        std::string partType;
        for (const std::string& line : split_lines(strip_cr(partHeaders))) {
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string key = lower_key(line.substr(0, colon));
            std::string value = trim(line.substr(colon + 1));
            if (key == "content-disposition") disposition = value;
            else if (key == "content-type") partType = value;
        }
        std::string name = header_param(disposition, "name");
        std::string filename = header_param(disposition, "filename");
        if (name.empty()) {
            pos = next;
            continue;
        }
        if (filename.empty()) {
            req.form[name] = data;
        } else {
            HttpFile file;
            file.field = name;
            file.filename = filename;
            file.contentType = partType;
            file.data = std::move(data);
            req.files[name] = std::move(file);
        }
        pos = next;
    }
}

bool recv_into(Socket conn, std::string& buffer, size_t want, bool& closed) {
    char chunk[65536];
    while (buffer.size() < want) {
        int n = ::recv(conn, chunk, static_cast<int>(sizeof(chunk)), 0);
        if (n == 0) {
            closed = true;
            return false;
        }
        if (n < 0) {
            if (socket_error_is_timeout()) {
                closed = true;
                return false;
            }
            closed = true;
            return false;
        }
        buffer.append(chunk, static_cast<size_t>(n));
    }
    return true;
}

bool read_request(Socket conn, HttpRequest& req, std::string& errText) {
    std::string buffer;
    bool closed = false;
    // 先读到请求头结束
    while (buffer.find("\r\n\r\n") == std::string::npos) {
        if (buffer.size() > kMaxHeader) {
            errText = "请求头过大";
            return false;
        }
        if (!recv_into(conn, buffer, buffer.size() + 1, closed) && closed) {
            errText = "连接已关闭";
            return false;
        }
    }
    size_t headerEnd = buffer.find("\r\n\r\n");
    std::string head = buffer.substr(0, headerEnd);
    std::string rest = buffer.substr(headerEnd + 4);

    std::vector<std::string> headLines = split_lines(strip_cr(head));
    if (headLines.empty()) {
        errText = "空请求";
        return false;
    }
    std::vector<std::string> parts = split_words(headLines[0]);
    if (parts.size() < 2) {
        errText = "请求行格式错误";
        return false;
    }
    req.method = to_upper(parts[0]);
    std::string target = parts[1];
    size_t qmark = target.find('?');
    if (qmark == std::string::npos) {
        req.path = url_decode(target);
    } else {
        req.path = url_decode(target.substr(0, qmark));
        req.rawQuery = target.substr(qmark + 1);
        parse_urlencoded(req.rawQuery, req.query);
    }
    for (size_t i = 1; i < headLines.size(); i++) {
        size_t colon = headLines[i].find(':');
        if (colon == std::string::npos) continue;
        req.headers[lower_key(headLines[i].substr(0, colon))] = trim(headLines[i].substr(colon + 1));
    }

    size_t contentLength = 0;
    auto it = req.headers.find("content-length");
    if (it != req.headers.end()) {
        contentLength = static_cast<size_t>(std::max<long long>(0, to_int(it->second, 0)));
    }
    if (contentLength > kMaxBody) {
        errText = str("请求体过大（{} > 512 MiB）", contentLength);
        return false;
    }
    // 大文件上传时浏览器/curl 可能先发 Expect: 100-continue
    auto expect = req.headers.find("expect");
    if (expect != req.headers.end() && to_lower(expect->second).find("100-continue") != std::string::npos) {
        const char* cont = "HTTP/1.1 100 Continue\r\n\r\n";
        ::send(conn, cont, static_cast<int>(std::strlen(cont)), 0);
    }
    req.body = std::move(rest);
    if (req.body.size() < contentLength) {
        if (!recv_into(conn, req.body, contentLength, closed) && closed) {
            errText = "请求体不完整";
            return false;
        }
    }
    if (req.body.size() > contentLength) req.body.resize(contentLength);

    auto ctype = req.headers.find("content-type");
    if (ctype != req.headers.end()) {
        std::string lower = to_lower(ctype->second);
        if (lower.find("application/x-www-form-urlencoded") != std::string::npos) {
            parse_urlencoded(req.body, req.form);
        } else if (lower.find("multipart/form-data") != std::string::npos) {
            std::string boundary = header_param(ctype->second, "boundary");
            if (!boundary.empty()) parse_multipart(req.body, boundary, req);
        }
    }
    return true;
}

void send_all(Socket conn, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = ::send(conn, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return;
        sent += static_cast<size_t>(n);
    }
}

const char* reason_phrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 204: return "No Content";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

}  // namespace

std::string url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hex(s[i + 1]);
            int lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>(hi * 16 + lo));
                i += 2;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

std::string html_escape_text(const std::string& s) { return escape_html(s); }

std::string http_query_escape(const std::string& s) {
    static const char* kHexDigits = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHexDigits[(c >> 4) & 0xF]);
            out.push_back(kHexDigits[c & 0xF]);
        }
    }
    return out;
}

std::string HttpRequest::param(const std::string& key, const std::string& def) const {
    auto f = form.find(key);
    if (f != form.end()) return f->second;
    auto q = query.find(key);
    if (q != query.end()) return q->second;
    return def;
}

long long HttpRequest::param_int(const std::string& key, long long def) const {
    if (!has_param(key)) return def;
    return to_int(param(key), def);
}

bool HttpRequest::has_param(const std::string& key) const {
    return form.count(key) > 0 || query.count(key) > 0;
}

bool HttpRequest::has_file(const std::string& field) const { return files.count(field) > 0; }

HttpResponse HttpResponse::html(std::string body) {
    HttpResponse r;
    r.contentType = "text/html; charset=utf-8";
    r.body = std::move(body);
    return r;
}

HttpResponse HttpResponse::text(std::string body) {
    HttpResponse r;
    r.contentType = "text/plain; charset=utf-8";
    r.body = std::move(body);
    return r;
}

HttpResponse HttpResponse::redirect(const std::string& location) {
    HttpResponse r;
    r.status = 302;
    r.reason = "Found";
    r.body.clear();
    r.headers.push_back({"Location", location});
    return r;
}

HttpResponse HttpResponse::download(std::string filename, std::string data,
                                    const std::string& contentType) {
    HttpResponse r;
    r.contentType = contentType;
    r.body = std::move(data);
    // RFC 5987：给中文文件名一个 filename*，同时保留 ASCII 回退
    std::string ascii;
    for (char c : filename) ascii.push_back((static_cast<unsigned char>(c) < 0x80) ? c : '_');
    r.headers.push_back({"Content-Disposition",
                         "attachment; filename=\"" + ascii + "\"; filename*=UTF-8''" +
                             http_query_escape(filename)});
    return r;
}

HttpResponse HttpResponse::fail(int status, const std::string& message) {
    HttpResponse r;
    r.status = status;
    r.reason = reason_phrase(status);
    r.contentType = "text/plain; charset=utf-8";
    r.body = message + "\n";
    return r;
}

HttpServer::~HttpServer() { stop(); }

void HttpServer::route(const std::string& method, const std::string& path, HttpHandler handler) {
    Route r;
    r.method = to_upper(method);
    r.prefix = path;
    r.byPrefix = false;
    r.handler = std::move(handler);
    routes_.push_back(std::move(r));
}

void HttpServer::route_prefix(const std::string& method, const std::string& prefix,
                              HttpHandler handler) {
    Route r;
    r.method = to_upper(method);
    r.prefix = prefix;
    r.byPrefix = true;
    r.handler = std::move(handler);
    routes_.push_back(std::move(r));
}

bool HttpServer::start(const std::string& host, int port, std::string& err) {
#ifdef _WIN32
    static bool wsaReady = false;
    if (!wsaReady) {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            err = "WSAStartup 失败";
            return false;
        }
        wsaReady = true;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    Socket s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kBadSocket) {
        err = "创建 socket 失败";
        return false;
    }
    int yes = 1;
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(port));
    if (host.empty() || host == "localhost") {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close_socket(s);
        err = str("无效的监听地址 {}", host);
        return false;
    }
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket(s);
        err = str("绑定端口 {} 失败（可能已被占用）", port);
        return false;
    }
    if (::listen(s, 32) != 0) {
        close_socket(s);
        err = "listen 失败";
        return false;
    }
    sockaddr_in bound{};
#ifdef _WIN32
    int len = sizeof(bound);
#else
    socklen_t len = sizeof(bound);
#endif
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&bound), &len) == 0) {
        port_ = ntohs(bound.sin_port);
    } else {
        port_ = port;
    }
    listen_ = static_cast<long long>(s);
    running_ = true;
    return true;
}

void HttpServer::stop() {
    if (!running_) return;
    running_ = false;
    if (listen_ >= 0) {
        close_socket(static_cast<Socket>(listen_));
        listen_ = -1;
    }
}

HttpResponse HttpServer::dispatch(const HttpRequest& req) const {
    for (const Route& r : routes_) {
        if (r.method != req.method) continue;
        if (r.byPrefix) {
            if (req.path.compare(0, r.prefix.size(), r.prefix) == 0) return r.handler(req);
        } else if (req.path == r.prefix) {
            return r.handler(req);
        }
    }
    return HttpResponse::fail(404, "没有这个页面：" + req.path);
}

void HttpServer::handle_connection(long long connRaw) {
    Socket conn = static_cast<Socket>(connRaw);
    HttpRequest req;
    std::string errText;
    if (read_request(conn, req, errText)) {
        HttpResponse resp;
        try {
            resp = dispatch(req);
        } catch (const std::exception& e) {
            resp = HttpResponse::fail(500, std::string("内部错误: ") + e.what());
        } catch (...) {
            resp = HttpResponse::fail(500, "内部错误");
        }
        std::string head = str("HTTP/1.1 {} {}\r\n", resp.status, reason_phrase(resp.status));
        head += "Content-Type: " + resp.contentType + "\r\n";
        head += str("Content-Length: {}\r\n", resp.body.size());
        head += "Cache-Control: no-store\r\n";
        head += "Connection: close\r\n";
        for (const std::pair<std::string, std::string>& h : resp.headers) {
            head += h.first + ": " + h.second + "\r\n";
        }
        head += "\r\n";
        std::string out = head + resp.body;
        send_all(conn, out);
    } else {
        log_debug(str("HTTP 请求读取失败：{}", errText));
    }
    close_socket(conn);
}

void HttpServer::run() {
    while (running_) {
        sockaddr_in peer{};
#ifdef _WIN32
        int len = sizeof(peer);
#else
        socklen_t len = sizeof(peer);
#endif
        Socket conn = ::accept(static_cast<Socket>(listen_), reinterpret_cast<sockaddr*>(&peer), &len);
        if (conn == kBadSocket) {
            if (!running_) break;
            continue;
        }
        char ip[64] = {0};
        if (inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip)) == nullptr) ip[0] = '\0';
        long long raw = static_cast<long long>(conn);
        std::thread([this, raw, ipStr = std::string(ip)]() {
            (void)ipStr;
            handle_connection(raw);
        }).detach();
    }
}

}  // namespace poly
