#include "Utils.hpp"
#include <stdexcept>
#include <array>


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


    // Multicast
    bool SendUdpMulticast(const std::string& groupAddress, std::uint16_t port, const std::string& payload) {
        SocketGuard udp(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
        if (udp.get() == kInvalidSocket) return false;

        unsigned char ttl = 1;
        setsockopt(udp.get(), IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl));

        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_port = htons(port);
        inet_pton(AF_INET, groupAddress.c_str(), &target.sin_addr);

        int sent = sendto(udp.get(), payload.data(), static_cast<int>(payload.size()), 0,
                          reinterpret_cast<const sockaddr*>(&target), sizeof(target));
        return sent >= 0;
    }

    SocketType CreateUdpMulticastListener(const std::string& groupAddress, std::uint16_t port) {
        SocketType udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udp == kInvalidSocket) return kInvalidSocket;

        int reuse = 1;
        setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(port);

        if (bind(udp, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0) {
            CloseSocket(udp);
            return kInvalidSocket;
        }

        ip_mreq group{};
        inet_pton(AF_INET, groupAddress.c_str(), &group.imr_multiaddr);
        group.imr_interface.s_addr = htonl(INADDR_ANY);
        setsockopt(udp, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&group), sizeof(group));

        return udp;
    }

    bool ReceiveDatagram(SocketType socket_fd, std::string& outPayload, int timeoutMs) {
        outPayload.clear();
        
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket_fd, &readSet);

        timeval timeout{};
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        int ready = select(static_cast<int>(socket_fd) + 1, &readSet, nullptr, nullptr, &timeout);
        if (ready <= 0) return false;

        std::array<char, 4096> buffer{};
        int received = recvfrom(socket_fd, buffer.data(), static_cast<int>(buffer.size() - 1), 0, nullptr, nullptr);
        
        if (received <= 0) return false;
        
        buffer[static_cast<std::size_t>(received)] = '\0';
        outPayload.assign(buffer.data(), static_cast<std::size_t>(received));
        return true;
    }

}