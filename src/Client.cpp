#include "File.hpp"
#include "Workspace.hpp"
#include "IPCModule.hpp"
#include "json.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
	#include <windows.h>
	#include <commdlg.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;


// ═══════════════════════════════════════════════════════════════════════════
// SaveBoxProxy — Proxy RMI (camada de aplicação do cliente)
//
// Cada método serializa seus argumentos em JSON, invoca
// ipc.doOperation() e desserializa a resposta. Em nenhum
// momento o Proxy (ou o main) enxerga sockets.
// ═══════════════════════════════════════════════════════════════════════════

class SaveBoxProxy {
public:
	SaveBoxProxy(int serverPort)
		: _ipc() {
		_serverRef.ip = discoverServerIp();
		_serverRef.port = serverPort;
		_serverRef.objectId = "SaveBoxServer";
	}

	// ────────────────────────────────────────────────────────────
	// Descoberta automática do servidor via Multicast (3 tentativas)
	// ────────────────────────────────────────────────────────────
	std::string discoverServerIp() {
		std::cout << "Procurando servidor na rede (Multicast)..." << std::endl;

		SocketType udp = SocketUtils::CreateUdpMulticastListener(
			SocketUtils::kMulticastGroup, SocketUtils::kMulticastPort);

		if (udp == kInvalidSocket) {
			throw std::runtime_error("Falha ao criar listener multicast para descoberta.");
		}
		SocketUtils::SocketGuard guard(udp);

		std::string msg;
		std::string senderIp;

		for (int attempt = 1; attempt <= 3; ++attempt) {
			auto startTime = std::chrono::steady_clock::now();
			// Aguarda até 2 segundos por tentativa (heartbeat é a cada 1 seg)
			while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(2)) {
				if (SocketUtils::ReceiveDatagram(udp, msg, senderIp, 500)) {
					if (msg.find("HEARTBEAT|LIDER") == 0) {
						std::cout << "Servidor encontrado em: " << senderIp << std::endl;
						return senderIp;
					}
				}
			}
			std::cout << "Tentativa " << attempt << "/3: Servidor nao encontrado." << std::endl;
		}

		throw std::runtime_error("Servidor nao encontrado apos 3 tentativas.");
	}

	// ────────────────────────────────────────────────────────────
	// 1. getWorkspaceRef — Passagem por REFERÊNCIA
	//    Retorna um RemoteObjectRef que identifica o workspace remoto
	// ────────────────────────────────────────────────────────────
	RemoteObjectRef getWorkspaceRef(std::uint64_t userId) {
		json args;
		args["user_id"] = userId;

		std::string replyJson = _ipc.doOperation(_serverRef, "getWorkspaceRef", args.dump());
		json reply = json::parse(replyJson);

		RemoteObjectRef ref;
		ref.ip       = reply.at("ip").get<std::string>();
		ref.port     = reply.at("port").get<int>();
		ref.objectId = reply.at("objectId").get<std::string>();
		return ref;
	}

	// ────────────────────────────────────────────────────────────
	// 2. uploadFile — Passagem por VALOR
	//    Serializa o objeto File inteiro (metadados + conteúdo em Base64)
	// ────────────────────────────────────────────────────────────
	std::string uploadFile(const RemoteObjectRef& workspaceRef, const File& f) {
		json args;
		args["workspace_object_id"] = workspaceRef.objectId;
		args["file"] = f.toJson();

		std::string replyJson = _ipc.doOperation(_serverRef, "uploadFile", args.dump());
		json reply = json::parse(replyJson);
		return reply.value("message", reply.value("status", "OK"));
	}

	// ────────────────────────────────────────────────────────────
	// 3. listFiles — retorna metadados dos arquivos do workspace
	// ────────────────────────────────────────────────────────────
	std::vector<File> listFiles(const RemoteObjectRef& workspaceRef) {
		json args;
		args["workspace_object_id"] = workspaceRef.objectId;

		std::string replyJson = _ipc.doOperation(_serverRef, "listFiles", args.dump());
		json reply = json::parse(replyJson);

		std::vector<File> files;
		if (reply.contains("files")) {
			for (const auto& fj : reply["files"]) {
				File f;
				f.fromJson(fj);
				files.push_back(std::move(f));
			}
		}
		return files;
	}

	// ────────────────────────────────────────────────────────────
	// 4. deleteFile — Passagem por VALOR (ID simples)
	// ────────────────────────────────────────────────────────────
	std::string deleteFile(std::uint64_t fileId) {
		json args;
		args["file_id"] = fileId;

		std::string replyJson = _ipc.doOperation(_serverRef, "deleteFile", args.dump());
		json reply = json::parse(replyJson);
		return reply.value("status", "OK");
	}

private:
	IPCModule _ipc;
	RemoteObjectRef _serverRef;
};


// ═══════════════════════════════════════════════════════════════════════════
// Funções auxiliares do cliente
// ═══════════════════════════════════════════════════════════════════════════

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
		return "";  // usuário cancelou
	}

	return std::string(filename);
#else
	std::string filepath;
	std::cout << "Digite o caminho do arquivo: ";
	std::getline(std::cin, filepath);
	return filepath;
#endif
}

static void PrintSeparator(char c = '=', int width = 55) {
	std::cout << std::string(width, c) << std::endl;
}

static void PrintMenu(const RemoteObjectRef& wsRef, std::uint64_t userId) {
	std::cout << "\n";
	PrintSeparator();
	std::cout << "  SAVEBOX — Usuario ID: " << userId << "\n";
	std::cout << "  Workspace: " << wsRef.objectId << "\n";
	PrintSeparator();
	std::cout << "  1. Fazer Upload de Arquivo\n";
	std::cout << "  2. Listar Arquivos do Workspace\n";
	std::cout << "  3. Sair\n";
	PrintSeparator();
	std::cout << "  Opcao: ";
}


// ═══════════════════════════════════════════════════════════════════════════
// main — O cliente NÃO enxerga sockets, apenas usa o SaveBoxProxy
// ═══════════════════════════════════════════════════════════════════════════

int main() {
	try {
		#ifdef _WIN32
			SocketUtils::WinsockContext winsock_context;
		#endif

		// ── Identificação do Usuário ──────────────────────────────────────
		PrintSeparator('*');
		std::cout << "  Bem-vindo ao SaveBox!\n";
		PrintSeparator('*');

		std::cout << "  Digite seu ID de usuario: ";
		std::uint64_t userId = 0;
		std::cin >> userId;
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

		// ── Descoberta e conexão ao servidor ─────────────────────────────
		SaveBoxProxy proxy(8080);

		// ── Obter referência remota ao workspace (passagem por referência) ─
		std::cout << "\nObtendo referencia remota ao workspace..." << std::endl;
		RemoteObjectRef wsRef = proxy.getWorkspaceRef(userId);
		std::cout << "Workspace: " << wsRef.objectId
		          << "  [" << wsRef.ip << ":" << wsRef.port << "]\n";

		// ── Loop do menu interativo ────────────────────────────────────────
		while (true) {
			PrintMenu(wsRef, userId);

			int opcao = 0;
			std::cin >> opcao;
			std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

			if (opcao == 1) {
				// ── Upload de arquivo ──────────────────────────────────────
				const std::string filepath = SelectFile();
				if (filepath.empty()) {
					std::cout << "  Nenhum arquivo selecionado.\n";
					continue;
				}

				const fs::path file_path(filepath);
				if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
					std::cerr << "  Erro: arquivo nao encontrado: " << filepath << "\n";
					continue;
				}

				const std::string file_name = file_path.filename().string();
				const std::uint64_t file_size =
					static_cast<std::uint64_t>(fs::file_size(file_path));

				std::cout << "  Arquivo: " << file_name
				          << " (" << file_size << " bytes)\n";

				std::ifstream ifs(file_path, std::ios::binary);
				if (!ifs) {
					std::cerr << "  Erro: falha ao abrir arquivo.\n";
					continue;
				}

				std::vector<char> content(static_cast<std::size_t>(file_size));
				if (file_size > 0) {
					ifs.read(content.data(),
					         static_cast<std::streamsize>(file_size));
					if (!ifs) {
						std::cerr << "  Erro: falha ao ler arquivo.\n";
						continue;
					}
				}
				ifs.close();

				File file_to_upload(userId, 0, file_name, file_size,
				                    std::move(content));

				std::cout << "  Enviando...\n";
				std::string result = proxy.uploadFile(wsRef, file_to_upload);
				std::cout << "  Servidor: " << result << "\n";

			} else if (opcao == 2) {
				// ── Listagem de arquivos ───────────────────────────────────
				std::cout << "  Consultando servidor...\n";
				auto files = proxy.listFiles(wsRef);

				std::cout << "\n";
				PrintSeparator('-');
				std::cout << "  Arquivos no workspace (" << files.size() << "):\n";
				PrintSeparator('-');

				if (files.empty()) {
					std::cout << "  (nenhum arquivo encontrado)\n";
				} else {
					std::cout << "  " << std::left
					          << std::setw(6) << "ID"
					          << std::setw(32) << "Nome"
					          << "Tamanho\n";
					PrintSeparator('-');
					for (const auto& f : files) {
						std::cout << "  " << std::left
						          << std::setw(6)  << f.getId()
						          << std::setw(32) << f.getName()
						          << f.getSizeBytes() << " bytes\n";
					}
				}
				PrintSeparator('-');

			} else if (opcao == 3) {
				// ── Sair ──────────────────────────────────────────────────
				std::cout << "  Encerrando SaveBox!\n";
				PrintSeparator('*');
				break;

			} else {
				std::cout << "  Opcao invalida. Tente novamente.\n";
			}
		}

		return 0;
	} catch (const std::exception& ex) {
		std::cerr << "Erro no cliente: " << ex.what() << std::endl;
		return 1;
	}
}
