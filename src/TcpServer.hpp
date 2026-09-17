#pragma once
#include <chrono>
#include <optional>
#include <string>

// Transport only. The main loop owns commands, controller, and physical state.
class TcpServer {
public:
    explicit TcpServer(int port);
    ~TcpServer();
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    std::optional<std::string> poll();
    void reply(std::string message);

private:
    void closeClient();
    int listener_ = -1;
    int client_ = -1;
    std::string incoming_;
    std::string outgoing_;
    bool awaitingReply_ = false;
    std::chrono::steady_clock::time_point deadline_;
};
