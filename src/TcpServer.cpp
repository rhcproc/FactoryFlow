#include "TcpServer.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace {
bool nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}
}

TcpServer::TcpServer(int port) {
    listener_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_ < 0) throw std::runtime_error("Cannot create TCP socket");
    int enabled = 1;
    setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (!nonblocking(listener_) ||
        bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(listener_, 8) < 0) {
        close(listener_);
        throw std::runtime_error("Cannot listen on 127.0.0.1:" + std::to_string(port));
    }
}

TcpServer::~TcpServer() {
    closeClient();
    close(listener_);
}

void TcpServer::closeClient() {
    if (client_ >= 0) close(client_);
    client_ = -1;
    incoming_.clear();
    outgoing_.clear();
    awaitingReply_ = false;
}

std::optional<std::string> TcpServer::poll() {
    const auto now = std::chrono::steady_clock::now();
    if (client_ >= 0 && now >= deadline_) closeClient();
    if (client_ < 0) {
        client_ = accept(listener_, nullptr, nullptr);
        if (client_ < 0) return std::nullopt;
        if (!nonblocking(client_)) { closeClient(); return std::nullopt; }
        deadline_ = now + std::chrono::seconds(1);
    }
    // Nonblocking, bounded work: a slow/disconnected client cannot stall scans.
    if (!outgoing_.empty()) {
        const auto sent = send(client_, outgoing_.data(), outgoing_.size(), 0);
        if (sent > 0) {
            outgoing_.erase(0, static_cast<std::size_t>(sent));
            if (outgoing_.empty()) closeClient();
        } else if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            closeClient();
        }
        return std::nullopt;
    }
    if (awaitingReply_) return std::nullopt;
    char buffer[129];
    const auto received = recv(client_, buffer, sizeof(buffer), 0);
    if (received == 0) { closeClient(); return std::nullopt; }
    if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) closeClient();
        return std::nullopt;
    }
    incoming_.append(buffer, static_cast<std::size_t>(received));
    const auto newline = incoming_.find('\n');
    if (incoming_.size() > 128 || (newline != std::string::npos && newline + 1 != incoming_.size())) {
        reply("{\"error\":\"invalid_request\"}");
        return std::nullopt;
    }
    if (newline == std::string::npos) return std::nullopt;
    awaitingReply_ = true;
    return incoming_.substr(0, newline);
}

void TcpServer::reply(std::string message) {
    outgoing_ = std::move(message) + '\n';
    awaitingReply_ = true;
}
