#include "File.hpp"
#include "Utils.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
	#include <windows.h>
	#include <commdlg.h>
#endif

namespace fs = std::filesystem;

static constexpr int kMaxRetries = 5;
static constexpr int kRetryDelaySeconds = 2;

static std::string SelectFile() {
#ifdef _WIN32
	char filename[MAX_PATH] = "";

	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = "Todos os Arquivos\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = "Selecione o arquivo para enviar";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetOpenFileNameA(&ofn)) {
		std::cout << "Nenhum arquivo selecionado. Encerrando." << std::endl;
		std::exit(0);
	}

	return std::string(filename);
#else
	std::string filepath;
	std::cout << "Digite o caminho do arquivo a enviar: ";
	std::getline(std::cin, filepath);

	if (filepath.empty()) {
		std::cout << "Nenhum arquivo informado. Encerrando." << std::endl;
		std::exit(0);
	}

	return filepath;
#endif
}

static SocketType ConnectWithRetry(const char* server_ip, std::uint16_t port) {
	for (int attempt = 1; attempt <= kMaxRetries; ++attempt) {
		SocketType sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == kInvalidSocket) {
			throw std::runtime_error("Falha ao criar socket TCP do cliente.");
		}

		sockaddr_in server_addr{};
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(port);

		if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
			SocketUtils::CloseSocket(sock);
			throw std::runtime_error("Falha ao converter o IP do servidor.");
		}

		if (connect(sock, reinterpret_cast<const sockaddr*>(&server_addr),
					static_cast<int>(sizeof(server_addr))) == 0) {
			std::cout << "Conectado ao servidor " << server_ip << ":" << port << std::endl;
			return sock;
		}

		SocketUtils::CloseSocket(sock);

		if (attempt < kMaxRetries) {
			std::cerr << "Tentativa " << attempt << "/" << kMaxRetries
					  << " falhou. Tentando novamente em " << kRetryDelaySeconds << "s..."
					  << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(kRetryDelaySeconds));
		}
	}

	throw std::runtime_error(
		"Falha ao conectar em " + std::string(server_ip) + ":" +
		std::to_string(port) + " apos " + std::to_string(kMaxRetries) + " tentativas.");
}


int main() {
	try {
		#ifdef _WIN32
			SocketUtils::WinsockContext winsock_context;
		#endif

		const std::string filepath = SelectFile();

		const fs::path file_path(filepath);
		if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
			throw std::runtime_error("Arquivo nao encontrado ou nao e um arquivo regular: " + filepath);
		}

		const std::string file_name = file_path.filename().string();
		const std::uint64_t file_size = static_cast<std::uint64_t>(fs::file_size(file_path));

		std::cout << "Arquivo: " << file_name << " (" << file_size << " bytes)" << std::endl;

		std::ifstream ifs(file_path, std::ios::binary);
		if (!ifs) {
			throw std::runtime_error("Falha ao abrir o arquivo: " + filepath);
		}

		std::vector<char> content(static_cast<std::size_t>(file_size));
		if (file_size > 0) {
			ifs.read(content.data(), static_cast<std::streamsize>(file_size));
			if (!ifs) {
				throw std::runtime_error("Falha ao ler o conteudo do arquivo: " + filepath);
			}
		}
		ifs.close();

		SocketUtils::SocketGuard client_socket(
			ConnectWithRetry(SocketUtils::kServerIp, SocketUtils::kPort));

		std::vector<File> files = {
			File(1, 0, file_name, file_size, std::move(content))};

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
