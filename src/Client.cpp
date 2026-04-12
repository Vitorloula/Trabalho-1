#include "File.hpp"
#include "Utils.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>


int main() {
	try {
		#ifdef _WIN32
			SocketUtils::WinsockContext winsock_context;
		#endif

		SocketUtils::SocketGuard client_socket(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
		if (client_socket.get() == kInvalidSocket) {
			throw std::runtime_error("Falha ao criar socket TCP do cliente.");
		}

		sockaddr_in server_addr{};
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(SocketUtils::kPort);

		if (inet_pton(AF_INET, SocketUtils::kServerIp, &server_addr.sin_addr) <= 0) {
			throw std::runtime_error("Falha ao converter o IP do servidor.");
		}

		if (connect(client_socket.get(), reinterpret_cast<const sockaddr*>(&server_addr),
					static_cast<int>(sizeof(server_addr))) < 0) {
			throw std::runtime_error("Falha ao conectar em " + std::string(SocketUtils::kServerIp) + ":" +
									 std::to_string(SocketUtils::kPort));
		}

		std::vector<File> files = {
			File(1001, 10, "relatorio.pdf", 1500),
			File(1002, 10, "imagem.png", 24000),
			File(1003, 11, "anotacoes.txt", 800)};

		std::stringstream payload_stream(std::ios::in | std::ios::out | std::ios::binary);
		FileOutputStream file_output_stream(files.data(), files.size(), payload_stream);
		file_output_stream.write();

		const std::string payload = payload_stream.str();

		if (files.size() > std::numeric_limits<std::uint32_t>::max()) {
			throw std::runtime_error("Quantidade de arquivos excede uint32.");
		}

		if (payload.size() > std::numeric_limits<std::uint32_t>::max()) {
			throw std::runtime_error("Payload excede tamanho maximo de uint32.");
		}

		SocketUtils::SendUint32(client_socket.get(), static_cast<std::uint32_t>(files.size()));
		SocketUtils::SendUint32(client_socket.get(), static_cast<std::uint32_t>(payload.size()));

		if (!payload.empty() &&
			!SocketUtils::SendAll(client_socket.get(), payload.data(), payload.size())) {
			throw std::runtime_error("Falha ao enviar payload serializado.");
		}

		const std::uint32_t reply_size = SocketUtils::ReceiveUint32(client_socket.get());
		std::string reply(reply_size, '\0');

		if (reply_size > 0 &&
			!SocketUtils::RecvAll(client_socket.get(), &reply[0], reply.size())) {
			throw std::runtime_error("Falha ao receber resposta do servidor.");
		}

		std::cout << "Resposta do servidor: " << reply << std::endl;
		return 0;
	} catch (const std::exception& ex) {
		std::cerr << "Erro no cliente: " << ex.what() << std::endl;
		return 1;
	}
}