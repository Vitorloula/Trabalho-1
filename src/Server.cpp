#include "File.hpp"
#include "Utils.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>


static SocketType TryBindTcp(std::uint16_t port) {
    SocketType tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp == kInvalidSocket) return kInvalidSocket;

#ifdef _WIN32
    int exclusive = 1;
    setsockopt(tcp, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
               reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
#else
    int reuse = 1;
    setsockopt(tcp, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#endif

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(tcp, reinterpret_cast<const sockaddr*>(&addr),
             static_cast<int>(sizeof(addr))) < 0) {
        SocketUtils::CloseSocket(tcp);
        return kInvalidSocket;
    }

    if (listen(tcp, SOMAXCONN) < 0) {
        SocketUtils::CloseSocket(tcp);
        return kInvalidSocket;
    }
    return tcp;
}


static void HeartbeatThread(std::atomic<bool>& running) {
    while (running.load()) {
        SocketUtils::SendUdpMulticast(
            SocketUtils::kMulticastGroup,
            SocketUtils::kMulticastPort,
            "HEARTBEAT|LIDER");

        std::this_thread::sleep_for(
            std::chrono::milliseconds(SocketUtils::kHeartbeatIntervalMs));
    }
}


static void HandleClient(SocketType client_fd) {
    SocketUtils::SocketGuard client(client_fd);

    const std::uint32_t file_count   = SocketUtils::ReceiveUint32(client.get());
    const std::uint32_t payload_size = SocketUtils::ReceiveUint32(client.get());

    std::string payload(payload_size, '\0');
    if (payload_size > 0 &&
        !SocketUtils::RecvAll(client.get(), &payload[0], payload.size())) {
        throw std::runtime_error("Falha ao receber payload serializado do cliente.");
    }

    std::stringstream payload_stream(std::ios::in | std::ios::out | std::ios::binary);
    if (!payload.empty()) {
        payload_stream.write(payload.data(),
                             static_cast<std::streamsize>(payload.size()));
        payload_stream.seekg(0, std::ios::beg);
    }

    if (file_count > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Quantidade de arquivos recebida excede int.");
    }

    FileInputStream file_input_stream(payload_stream);
    const std::vector<File> files =
        file_input_stream.readFiles(static_cast<int>(file_count));

    for (const File& f : files) {
        std::cout << "        ID=" << f.getId()
                  << " Nome=" << f.getName() << std::endl;
    }

    std::string sync_data;
    for (const File& f : files) {
        if (!sync_data.empty()) sync_data += ';';
        sync_data += std::to_string(f.getId()) + "," + f.getName();
    }
    SocketUtils::SendUdpMulticast(
        SocketUtils::kMulticastGroup,
        SocketUtils::kMulticastPort,
        "SYNC|" + sync_data);

    const std::string reply = SocketUtils::kReplyMessage;
    if (reply.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Mensagem de resposta excede uint32.");
    }
    SocketUtils::SendUint32(client.get(),
                            static_cast<std::uint32_t>(reply.size()));
    if (!reply.empty() &&
        !SocketUtils::SendAll(client.get(), reply.data(), reply.size())) {
        throw std::runtime_error("Falha ao enviar resposta para o cliente.");
    }
}


static void RunAsLeader(SocketType tcp_fd) {
    SocketUtils::SocketGuard server(tcp_fd);

    std::cout << "Servidor ativo na porta " << SocketUtils::kPort << std::endl;

    std::atomic<bool> hb_running{true};
    std::thread hb_thread(HeartbeatThread, std::ref(hb_running));

    while (true) {

        SocketType client_fd = accept(server.get(), nullptr, nullptr);
        if (client_fd == kInvalidSocket) {
            continue;
        }

        try {
            HandleClient(client_fd);
        } catch (const std::exception& ex) {
            std::cerr << "Erro ao processar cliente: "
                      << ex.what() << std::endl;
        }
    }

    hb_running.store(false);
    if (hb_thread.joinable()) hb_thread.join();
}


static bool RunAsBackup() {
    std::cout << "Modo backup. Ouvindo heartbeat..." << std::endl;

    SocketType udp = SocketUtils::CreateUdpMulticastListener(
        SocketUtils::kMulticastGroup, SocketUtils::kMulticastPort);
    if (udp == kInvalidSocket) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return true;
    }

    auto last_heartbeat = std::chrono::steady_clock::now();
    std::string msg;

    while (true) {
        if (SocketUtils::ReceiveDatagram(udp, msg, 500)) {
            if (msg.rfind("HEARTBEAT|", 0) == 0) {
                last_heartbeat = std::chrono::steady_clock::now();
            } else if (msg.rfind("SYNC|", 0) == 0) {
                std::string dados = msg.substr(5);
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - last_heartbeat).count();

        if (elapsed > SocketUtils::kHeartbeatTimeoutMs) {
            std::cout << "Heartbeat expirado" << std::endl;

            SocketUtils::CloseSocket(udp);
            return true;
        }
    }
}


int main() {
    try {
#ifdef _WIN32
        SocketUtils::WinsockContext winsock_context;
#endif

        while (true) {
            SocketType tcp_fd = TryBindTcp(SocketUtils::kPort);

            if (tcp_fd != kInvalidSocket) {
                RunAsLeader(tcp_fd);
            } else {
                bool should_retry = RunAsBackup();
                if (!should_retry) break;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Erro fatal no servidor: " << ex.what() << std::endl;
        return 1;
    }
}
