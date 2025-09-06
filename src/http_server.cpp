#include "http_server.h"
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iostream>

#ifdef _WIN32
  #define _WINSOCK_DEPRECATED_NO_WARNINGS
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socklen_t = int;
#else
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <netinet/in.h>
  #include <fcntl.h>
#endif

static std::string http_response(int status, const std::string& type, const std::string& body,
                                 const std::string& extra_headers = "") {
    std::ostringstream os;
    const char* msg = "OK";
    if (status==200) msg = "OK";
    else if (status==204) msg = "No Content";
    else if (status==400) msg = "Bad Request";
    else if (status==404) msg = "Not Found";
    else if (status==500) msg = "Internal Server Error";
    os << "HTTP/1.1 " << status << " " << msg << "\r\n";
    os << "Content-Type: " << type << "\r\n";
    os << "Access-Control-Allow-Origin: *\r\n";
    os << "Access-Control-Allow-Headers: *\r\n";
    os << "Access-Control-Allow-Methods: *\r\n";
    if (!extra_headers.empty()) os << extra_headers;
    os << "Content-Length: " << body.size() << "\r\n\r\n";
    os << body;
    return os.str();
}

SimpleHttpServer::SimpleHttpServer(int port) : port_(port) {}

SimpleHttpServer::~SimpleHttpServer() {
    stop();
}

bool SimpleHttpServer::start() {
    if (running_) return false;
    running_ = true;
    th_ = std::thread([this]{ run_(); });
    return true;
}

void SimpleHttpServer::stop() {
    if (!running_) return;
    running_ = false;
#ifdef _WIN32
    // poke the server by connecting to it to unblock accept
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s != INVALID_SOCKET) {
        sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons((uint16_t)port_); a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        connect(s,(sockaddr*)&a,sizeof(a));
        closesocket(s);
    }
#else
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s>=0) {
        sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons((uint16_t)port_); a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        connect(s,(sockaddr*)&a,sizeof(a));
        ::close(s);
    }
#endif
    if (th_.joinable()) th_.join();
}

int SimpleHttpServer::create_listen_socket_() {
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    int fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int opt=1;
#ifdef _WIN32
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY); addr.sin_port = htons((uint16_t)port_);
    if (bind(fd, (sockaddr*)&addr, sizeof(addr))<0) {
#ifdef _WIN32
        closesocket(fd);
#else
        ::close(fd);
#endif
        return -1;
    }
    if (listen(fd, 64)<0) {
#ifdef _WIN32
        closesocket(fd);
#else
        ::close(fd);
#endif
        return -1;
    }
    return fd;
}

bool SimpleHttpServer::recv_all_(int fd, std::string& request) {
    request.clear();
    char buf[8192];
    // read headers
    std::string headers;
    while (true) {
#ifdef _WIN32
        int n = recv(fd, buf, sizeof(buf), 0);
#else
        int n = (int)recv(fd, buf, sizeof(buf), 0);
#endif
        if (n<=0) return false;
        headers.append(buf, buf+n);
        auto pos = headers.find("\r\n\r\n");
        if (pos != std::string::npos) {
            request = headers.substr(0, pos+4);
            // Check for Content-Length
            std::string hl = headers.substr(0, pos);
            size_t clp = hl.find("Content-Length:");
            size_t expected = 0;
            if (clp != std::string::npos) {
                size_t eol = hl.find("\r\n", clp);
                std::string v = hl.substr(clp+15, eol-(clp+15));
                expected = (size_t)std::stoul(v);
            }
            std::string body = headers.substr(pos+4);
            while (body.size() < expected) {
#ifdef _WIN32
                int m = recv(fd, buf, sizeof(buf), 0);
#else
                int m = (int)recv(fd, buf, sizeof(buf), 0);
#endif
                if (m<=0) break;
                body.append(buf, buf+m);
            }
            request += body;
            return true;
        }
        if (headers.size() > 8*1024*1024) return false; // too big
    }
}

bool SimpleHttpServer::send_all_(int fd, const std::string& response) {
    size_t off=0;
    while (off < response.size()) {
#ifdef _WIN32
        int n = send(fd, response.data()+off, (int)(response.size()-off), 0);
#else
        int n = (int)send(fd, response.data()+off, response.size()-off, 0);
#endif
        if (n<=0) return false;
        off += (size_t)n;
    }
    return true;
}

void SimpleHttpServer::close_fd_(int fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    ::close(fd);
#endif
}

void SimpleHttpServer::run_() {
    int lfd = create_listen_socket_();
    if (lfd<0) {
        std::cerr << "HTTP: failed to bind on port " << port_ << "\n";
        running_ = false;
        return;
    }
    std::cout << "HTTP server listening on http://127.0.0.1:" << port_ << "\n";
    while (running_) {
        sockaddr_in cli{}; socklen_t cl = sizeof(cli);
#ifdef _WIN32
        int cfd = accept(lfd, (sockaddr*)&cli, &cl);
#else
        int cfd = (int)accept(lfd, (sockaddr*)&cli, &cl);
#endif
        if (cfd<0) continue;
        std::string req;
        if (!recv_all_(cfd, req)) { close_fd_(cfd); continue; }
        // parse method, path
        std::istringstream is(req);
        std::string method, path, version;
        is >> method >> path >> version;
        int status = 200; std::string ctype = "application/json";
        std::string extra_headers;
        // body
        auto pos = req.find("\r\n\r\n");
        std::string body = (pos==std::string::npos? "" : req.substr(pos+4));

        if (method=="OPTIONS") {
            auto resp = http_response(204, "text/plain", "", "");
            send_all_(cfd, resp); close_fd_(cfd); continue;
        }

        std::string resp_body = "{\"error\":\"no handler\"}";
        if (handler_) {
            try {
                resp_body = handler_(method, path, body, status, ctype, extra_headers);
            } catch (const std::exception& e) {
                status = 500;
                resp_body = std::string("{\"ok\":false,\"error\":\"") + e.what() + "\"}";
            }
        }
        auto resp = http_response(status, ctype, resp_body, extra_headers);
        send_all_(cfd, resp);
        close_fd_(cfd);
    }
#ifdef _WIN32
    closesocket(lfd);
    WSACleanup();
#else
    ::close(lfd);
#endif
}
