#include "File.hpp"
#include "Workspace.hpp"
#include "IPCModule.hpp"
#include "json.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
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
	SaveBoxProxy(const std::string& serverIp, int serverPort)
		: _ipc()
		, _serverRef{serverIp, serverPort, "SaveBoxServer"} {}

	// ────────────────────────────────────────────────────────────
	// 1. uploadFile  — Passagem por VALOR
	//    Serializa o objeto File inteiro (metadados + conteúdo em base64)
	// ────────────────────────────────────────────────────────────
	std::string uploadFile(const File& f) {
		json args;
		args["file"] = f.toJson();

		std::string replyJson = _ipc.doOperation(_serverRef, "uploadFile", args.dump());
		json reply = json::parse(replyJson);
		return reply.value("status", "OK");
	}

	// ────────────────────────────────────────────────────────────
	// 2. listFiles — Passagem por VALOR (retorna lista)
	// ────────────────────────────────────────────────────────────
	std::vector<File> listFiles() {
		json args;
		args["workspace_id"] = 1;

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
	// 3. deleteFile — Passagem por VALOR (ID simples)
	// ────────────────────────────────────────────────────────────
	std::string deleteFile(std::uint64_t fileId) {
		json args;
		args["file_id"] = fileId;

		std::string replyJson = _ipc.doOperation(_serverRef, "deleteFile", args.dump());
		json reply = json::parse(replyJson);
		return reply.value("status", "OK");
	}

	// ────────────────────────────────────────────────────────────
	// 4. getWorkspaceRef — Passagem por REFERÊNCIA
	//    Não retorna o objeto Workspace, mas sim um RemoteObjectRef
	//    que o cliente pode usar em chamadas subsequentes.
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


// ═══════════════════════════════════════════════════════════════════════════
// main — O cliente NÃO enxerga sockets, apenas usa o SaveBoxProxy
// ═══════════════════════════════════════════════════════════════════════════

int main() {
	try {
		#ifdef _WIN32
			SocketUtils::WinsockContext winsock_context;
		#endif

		SaveBoxProxy proxy("127.0.0.1", 8080);

		// ── 1. Obter referência remota ao workspace (passagem por referência) ──
		std::cout << "Obtendo referencia remota ao workspace..." << std::endl;
		RemoteObjectRef wsRef = proxy.getWorkspaceRef(1);
		std::cout << "Workspace remoto: " << wsRef.objectId
		          << " em " << wsRef.ip << ":" << wsRef.port << std::endl;

		// ── 2. Upload de arquivo (passagem por valor) ──
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

		File file_to_upload(1, 0, file_name, file_size, std::move(content));

		std::string result = proxy.uploadFile(file_to_upload);
		std::cout << "Resposta do servidor: " << result << std::endl;

		// ── 3. Listar arquivos ──
		std::cout << "\nListando arquivos no servidor..." << std::endl;
		auto files = proxy.listFiles();
		for (const auto& f : files) {
			std::cout << "  - " << f.getName() << " (" << f.getSizeBytes() << " bytes)" << std::endl;
		}

		return 0;
	} catch (const std::exception& ex) {
		std::cerr << "Erro no cliente: " << ex.what() << std::endl;
		return 1;
	}
}
