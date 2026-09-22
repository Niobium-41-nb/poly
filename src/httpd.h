#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace poly {

// multipart/form-data 里的一个文件。
struct HttpFile {
    std::string field;
    std::string filename;
    std::string contentType;
    std::string data;
};

struct HttpRequest {
    std::string method;
    std::string path;      // 已解码的路径
    std::string rawQuery;
    std::string remote;

    std::map<std::string, std::string> query;
    std::map<std::string, std::string> form;
    std::map<std::string, HttpFile> files;      // key = 字段名
    std::map<std::string, std::string> headers;  // key 统一小写

    std::string body;

    std::string param(const std::string& key, const std::string& def = std::string()) const;
    long long param_int(const std::string& key, long long def) const;
    bool has_param(const std::string& key) const;
    bool has_file(const std::string& field) const;
};

struct HttpResponse {
    int status = 200;
    std::string reason = "OK";
    std::string contentType = "text/html; charset=utf-8";
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;

    static HttpResponse html(std::string body);
    static HttpResponse text(std::string body);
    static HttpResponse redirect(const std::string& location);
    static HttpResponse download(std::string filename, std::string data,
                                 const std::string& contentType = "application/octet-stream");
    static HttpResponse fail(int status, const std::string& message);
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

// 极简 HTTP/1.1 服务器：阻塞 accept + 每连接一个线程，响应后关连接。
// 只服务于本机工作台，不追求 keep-alive、TLS、分块传输等能力。
class HttpServer {
public:
    HttpServer() = default;
    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void route(const std::string& method, const std::string& path, HttpHandler handler);
    // 前缀路由：/p/demo 交给注册了 /p/ 的处理器
    void route_prefix(const std::string& method, const std::string& prefix, HttpHandler handler);

    bool start(const std::string& host, int port, std::string& err);
    void run();
    void stop();

    int port() const { return port_; }

private:
    struct Route {
        std::string method;
        std::string prefix;
        bool byPrefix = false;
        HttpHandler handler;
    };

    void handle_connection(long long conn);
    HttpResponse dispatch(const HttpRequest& req) const;

    std::vector<Route> routes_;
    long long listen_ = -1;
    int port_ = 0;
    bool running_ = false;
};

// 供 webui 复用的小工具。
std::string url_decode(const std::string& s);
std::string html_escape_text(const std::string& s);
std::string http_query_escape(const std::string& s);

}  // namespace poly
