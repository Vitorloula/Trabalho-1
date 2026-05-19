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


static Workspace g_workspace;
static bool g_workspaceInitialized = false;

static void InitWorkspace() {
	if (!g_workspaceInitialized) {
		User owner(1, "admin", "admin@savebox.com");
		g_workspace = Workspace(1, "MainWorkspace", owner);
		
		const fs::path output_dir("./arquivos");
		if (fs::exists(output_dir) && fs::is_directory(output_dir)) {
			std::uint64_t id = 1;
			for (const auto& entry : fs::directory_iterator(output_dir)) {
				if (entry.is_regular_file()) {
					File f(id++, 0, entry.path().filename().string(),
					       static_cast<std::uint64_t>(entry.file_size()));
					g_workspace.addFile(f);
				}
			}
		}
		
		g_workspaceInitialized = true;
	}
}


// Handlers

static std::string handleUploadFile(const json& args) {
	InitWorkspace();

	File f;
	f.fromJson(args.at("file"));

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

	std::uint64_t newFileId = g_workspace.getFiles().size() + 1;
	File meta(newFileId, f.getFolderId(), f.getName(), f.getSizeBytes());
	g_workspace.addFile(meta);

	std::cout << "[uploadFile] Arquivo salvo: " << dest_path
	          << " (" << f.getSizeBytes() << " bytes)" << std::endl;

	return json({{"status", "OK"}, {"message", "Arquivo recebido com sucesso: " + f.getName()}}).dump();
}


static std::string handleListFiles(const json& args) {
	InitWorkspace();

	const std::string workspaceObjId = args.value("workspace_object_id", "");
	std::cout << "[listFiles] workspace=" << workspaceObjId << std::endl;

	json filesJson = json::array();
	for (const auto& f : g_workspace.getFiles()) {
		filesJson.push_back({
			{"id",         f.getId()},
			{"name",       f.getName()},
			{"size_bytes", f.getSizeBytes()}
		});
	}

	std::cout << "[listFiles] " << filesJson.size() << " arquivo(s) listado(s)." << std::endl;
	return json({{"status", "OK"}, {"files", filesJson}}).dump();
}


static std::string handleDeleteFile(const json& args) {
	InitWorkspace();

	const std::uint64_t fileId = args.at("file_id").get<std::uint64_t>();

	auto& files = const_cast<std::vector<File>&>(g_workspace.getFiles());
	std::string deletedName;

	for (auto it = files.begin(); it != files.end(); ++it) {
		if (it->getId() == fileId) {
			deletedName = it->getName();
			files.erase(it);
			break;
		}
	}

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


	std::string objectId = "workspace:" + std::to_string(g_workspace.getId()) +
	                       ":user:" + std::to_string(userId);

	std::cout << "[getWorkspaceRef] Referencia remota criada: " << objectId << std::endl;

	return json({
		{"ip", "127.0.0.1"},
		{"port", 8080},
		{"objectId", objectId}
	}).dump();
}


// Dispatcher RMI 

static void RunAsLeader(int port) {
	IPCModule ipc;
	ipc.initServer(port);

	std::cout << "Servidor RMI ativo na porta " << port << std::endl;

	
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

			
			json msg = json::parse(requestRaw);
			std::string methodId = msg.at("methodId").get<std::string>();
			json args = json::parse(msg.at("arguments").get<std::string>());

			std::cout << "\n[Dispatcher] methodId = " << methodId << std::endl;

			std::string replyJson;

			// Dispatch por methodId 
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



// Modo Backup 


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



// Main


int main() {
	try {
#ifdef _WIN32
		SocketUtils::WinsockContext winsock_context;
#endif

		while (true) {
			try {
				RunAsLeader(SocketUtils::kPort);
			} catch (const std::runtime_error&) {
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
