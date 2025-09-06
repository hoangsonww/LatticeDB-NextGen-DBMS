#pragma once
#include <string>
#include <thread>
#include <functional>
#include <atomic>

class SimpleHttpServer {
public:
    using Handler = std::function<std::string(const std::string& method,
                                              const std::string& path,
                                              const std::string& body,
                                              int& status,
                                              std::string& content_type,
                                              std::string& extra_headers)>;

    explicit SimpleHttpServer(int port);
    ~SimpleHttpServer();

    void set_handler(Handler h) { handler_ = std::move(h); }
    bool start();
    void stop();

private:
    int port_;
    std::thread th_;
    std::atomic<bool> running_{false};
    Handler handler_;

    void run_();

    // platform sockets
    int create_listen_socket_();
    bool recv_all_(int fd, std::string& request);
    bool send_all_(int fd, const std::string& response);
    void close_fd_(int fd);
};
