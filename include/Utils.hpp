#pragma once

#include <cstdint>
#include <cstddef>

#ifdef _WIN32
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
    using SocketType = SOCKET;
    constexpr SocketType kInvalidSocket = INVALID_SOCKET;
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    using SocketType = int;
    constexpr SocketType kInvalidSocket = -1;
#endif


namespace SocketUtils {

    constexpr std::uint16_t kPort = 8080;
    constexpr const char* kServerIp = "127.0.0.1";
    constexpr const char* kReplyMessage = "Arquivos recebidos com sucesso!";

#ifdef _WIN32
    class WinsockContext {
    public:
        WinsockContext();
        ~WinsockContext();
    };
#endif

    void CloseSocket(SocketType socket_fd);

    class SocketGuard {
    public:
        explicit SocketGuard(SocketType socket_fd = kInvalidSocket);
        ~SocketGuard();
        SocketType get() const;

        SocketGuard(const SocketGuard&) = delete;
        SocketGuard& operator=(const SocketGuard&) = delete;
    private:
        SocketType _socket_fd;
    };

    bool SendAll(SocketType socket_fd, const void* data, std::size_t length);
    bool RecvAll(SocketType socket_fd, void* data, std::size_t length);
    void SendUint32(SocketType socket_fd, std::uint32_t value);
    std::uint32_t ReceiveUint32(SocketType socket_fd);

}