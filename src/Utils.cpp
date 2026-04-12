#include "Utils.hpp"
#include <stdexcept>

namespace SocketUtils {

    #ifdef _WIN32
        WinsockContext::WinsockContext() {
            WSADATA data{};
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
                throw std::runtime_error("Falha no WSAStartup.");
            }
        }

        WinsockContext::~WinsockContext() {
            WSACleanup();
        }
    #endif

    void CloseSocket(SocketType socket_fd) {
        #ifdef _WIN32
            closesocket(socket_fd);
        #else
            close(socket_fd);
        #endif
    }

    SocketGuard::SocketGuard(SocketType socket_fd) : _socket_fd(socket_fd) {}

    SocketGuard::~SocketGuard() {
        if (_socket_fd != kInvalidSocket) {
            CloseSocket(_socket_fd);
        }
    }

    SocketType SocketGuard::get() const {
        return _socket_fd;
    }

    bool SendAll(SocketType socket_fd, const void* data, std::size_t length) {
        const char* cursor = static_cast<const char*>(data);
        std::size_t pending = length;

        while (pending > 0) {
            const int sent = send(socket_fd, cursor, static_cast<int>(pending), 0);
            if (sent <= 0) return false;
            cursor += sent;
            pending -= static_cast<std::size_t>(sent);
        }
        return true;
    }

    bool RecvAll(SocketType socket_fd, void* data, std::size_t length) {
        char* cursor = static_cast<char*>(data);
        std::size_t pending = length;

        while (pending > 0) {
            const int received = recv(socket_fd, cursor, static_cast<int>(pending), 0);
            if (received <= 0) return false;
            cursor += received;
            pending -= static_cast<std::size_t>(received);
        }
        return true;
    }

    void SendUint32(SocketType socket_fd, std::uint32_t value) {
        const std::uint32_t network_value = htonl(value);
        if (!SendAll(socket_fd, &network_value, sizeof(network_value))) {
            throw std::runtime_error("Falha ao enviar uint32 no socket.");
        }
    }

    std::uint32_t ReceiveUint32(SocketType socket_fd) {
        std::uint32_t network_value = 0;
        if (!RecvAll(socket_fd, &network_value, sizeof(network_value))) {
            throw std::runtime_error("Falha ao receber uint32 no socket.");
        }
        return ntohl(network_value);
    }

}