#include "File.hpp"
#include "Workspace.hpp"
#include "User.hpp"
#include "IPCModule.hpp"
#include "json.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;


// ═══════════════════════════════════════════════════════════════════════════
// Estado simulado do servidor (in-memory)
// ═══════════════════════════════════════════════════════════════════════════

static Workspace g_workspace;
static bool g_workspaceInitialized = false;

static void InitWorkspace() {
	if (!g_workspaceInitialized) {
		User owner(1, "admin", "admin@savebox.com");
		g_workspace = Workspace(1, "MainWorkspace", owner);
		g_workspaceInitialized = true;
	}
}


// ═══════════════════════════════════════════════════════════════════════════
// Handlers dos métodos remotos (lógica de aplicação do servidor)
// ═══════════════════════════════════════════════════════════════════════════

static std::string handleUploadFile(const json& args) {
	InitWorkspace();

	File f;
	f.fromJson(args.at("file"));

	// Salvar no disco
	const fs::path output_dir("./arquivos");
	fs::create_directories(output_dir);

	const fs::path dest_path = output_dir / f.getName();
	std::ofstream ofs(dest_path, std::ios::binary | std::ios::trunc);
	if (!ofs) {
		return json({{"status", "ERRO"}, {"message", "Falha ao criar arquivo: " + dest_path.string()}}).dump();
	}

	const auto& content = f.getContent();
	if (!content.empty()) {
		ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
		if (!ofs) {
			return json({{"status", "ERRO"}, {"message", "Falha ao gravar conteudo em: " + dest_path.string()}}).dump();
		}
	}
	ofs.close();

	// Registrar no workspace (sem conteúdo binário para economia de memória)
	File meta(f.getId(), f.getFolderId(), f.getName(), f.getSizeBytes());
	g_workspace.addFile(meta);

	std::cout << "[uploadFile] Arquivo salvo: " << dest_path
	          << " (" << f.getSizeBytes() << " bytes)" << std::endl;

	return json({{"status", "OK"}, {"message", "Arquivo recebido com sucesso: " + f.getName()}}).dump();
}


static std::string handleListFiles(const json& args) {
	InitWorkspace();

	const std::string workspaceObjId = args.value("workspace_object_id", "");
	std::cout << "[listFiles] workspace=" << workspaceObjId << std::endl;

	// Listar do disco para incluir arquivos de sessões anteriores
	const fs::path output_dir("./arquivos");
	json filesJson = json::array();

	if (fs::exists(output_dir) && fs::is_directory(output_dir)) {
		std::uint64_t id = 1;
		for (const auto& entry : fs::directory_iterator(output_dir)) {
			if (entry.is_regular_file()) {
				// Retorna apenas metadados — sem conteúdo binário
				filesJson.push_back({
					{"id",         id++},
					{"name",       entry.path().filename().string()},
					{"size_bytes", static_cast<std::uint64_t>(entry.file_size())}
				});
			}
		}
	}

	std::cout << "[listFiles] " << filesJson.size() << " arquivo(s) listado(s)." << std::endl;
	return json({{"status", "OK"}, {"files", filesJson}}).dump();
}


static std::string handleDeleteFile(const json& args) {
	InitWorkspace();

	const std::uint64_t fileId = args.at("file_id").get<std::uint64_t>();

	// Procurar e remover do workspace
	auto& files = const_cast<std::vector<File>&>(g_workspace.getFiles());
	std::string deletedName;

	for (auto it = files.begin(); it != files.end(); ++it) {
		if (it->getId() == fileId) {
			deletedName = it->getName();
			files.erase(it);
			break;
		}
	}

	// Tentar remover do disco
	if (!deletedName.empty()) {
		const fs::path filePath = fs::path("./arquivos") / deletedName;
		if (fs::exists(filePath)) {
			fs::remove(filePath);
		}
		std::cout << "[deleteFile] Arquivo removido: " << deletedName << std::endl;
		return json({{"status", "OK"}, {"message", "Arquivo removido: " + deletedName}}).dump();
	}

	return json({{"status", "NOT_FOUND"}, {"message", "Arquivo com ID " + std::to_string(fileId) + " nao encontrado."}}).dump();
}


static std::string handleGetWorkspaceRef(const json& args) {
	InitWorkspace();

	const std::uint64_t userId = args.at("user_id").get<std::uint64_t>();

	// Retorna uma referência remota ao workspace (passagem por referência)
	// O cliente recebe um RemoteObjectRef, não o objeto físico
	std::string objectId = "workspace:" + std::to_string(g_workspace.getId()) +
	                       ":user:" + std::to_string(userId);

	std::cout << "[getWorkspaceRef] Referencia remota criada: " << objectId << std::endl;

	return json({
		{"ip", "127.0.0.1"},
		{"port", 8080},
		{"objectId", objectId}
	}).dump();
}


// ═══════════════════════════════════════════════════════════════════════════
// Dispatcher RMI — loop principal do servidor
//
// Usa APENAS o IPCModule para comunicação de rede.
// Faz parse do methodId e despacha para o handler correto.
// ═══════════════════════════════════════════════════════════════════════════

static void RunAsLeader(int port) {
	IPCModule ipc;
	ipc.initServer(port);

	std::cout << "Servidor RMI ativo na porta " << port << std::endl;

	// Heartbeat thread (usa SocketUtils internamente — infraestrutura, não RMI)
	std::atomic<bool> hb_running{true};
	std::thread hb_thread([&hb_running]() {
		while (hb_running.load()) {
			SocketUtils::SendUdpMulticast(
				SocketUtils::kMulticastGroup,
				SocketUtils::kMulticastPort,
				"HEARTBEAT|LIDER");
			std::this_thread::sleep_for(
				std::chrono::milliseconds(SocketUtils::kHeartbeatIntervalMs));
		}
	});

	while (true) {
		try {
			SocketType clientFd;
			std::string requestRaw = ipc.getRequest(clientFd);

			// Parse da mensagem RMI
			json msg = json::parse(requestRaw);
			std::string methodId = msg.at("methodId").get<std::string>();
			json args = json::parse(msg.at("arguments").get<std::string>());

			std::cout << "\n[Dispatcher] methodId = " << methodId << std::endl;

			std::string replyJson;

			// ── Dispatch por methodId ──
			if (methodId == "uploadFile") {
				replyJson = handleUploadFile(args);
			} else if (methodId == "listFiles") {
				replyJson = handleListFiles(args);
			} else if (methodId == "deleteFile") {
				replyJson = handleDeleteFile(args);
			} else if (methodId == "getWorkspaceRef") {
				replyJson = handleGetWorkspaceRef(args);
			} else {
				replyJson = json({{"status", "ERRO"}, {"message", "methodId desconhecido: " + methodId}}).dump();
			}

			ipc.sendReply(clientFd, replyJson);

		} catch (const std::exception& ex) {
			std::cerr << "[Dispatcher] Erro ao processar cliente: "
			          << ex.what() << std::endl;
		}
	}

	hb_running.store(false);
	if (hb_thread.joinable()) hb_thread.join();
}


// ═══════════════════════════════════════════════════════════════════════════
// Modo Backup (heartbeat listener — infraestrutura de coordenação)
// ═══════════════════════════════════════════════════════════════════════════

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
	std::string senderIp;

	while (true) {
		if (SocketUtils::ReceiveDatagram(udp, msg, senderIp, 500)) {
			if (msg.rfind("HEARTBEAT|", 0) == 0) {
				last_heartbeat = std::chrono::steady_clock::now();
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


// ═══════════════════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════════════════

int main() {
	try {
#ifdef _WIN32
		SocketUtils::WinsockContext winsock_context;
#endif

		while (true) {
			try {
				RunAsLeader(SocketUtils::kPort);
			} catch (const std::runtime_error&) {
				// Falha no bind → outra instância é líder
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
