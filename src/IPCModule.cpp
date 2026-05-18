#include "IPCModule.hpp"
#include "json.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

// ═══════════════════════════════════════════════════════════════════════════
// Helpers internos — serialização da RMIMessage para JSON
// ═══════════════════════════════════════════════════════════════════════════

namespace {

std::string SerializeMessage(const RMIMessage& msg) {
	json j;
	j["messageType"] = static_cast<int>(msg.messageType);
	j["requestId"]   = msg.requestId;
	j["objectReference"] = {
		{"ip",       msg.objectReference.ip},
		{"port",     msg.objectReference.port},
		{"objectId", msg.objectReference.objectId}
	};
	j["methodId"]   = msg.methodId;
	j["arguments"]  = msg.arguments;
	return j.dump();
}

RMIMessage DeserializeMessage(const std::string& raw) {
	json j = json::parse(raw);

	RMIMessage msg;
	msg.messageType = static_cast<MessageType>(j.at("messageType").get<int>());
	msg.requestId   = j.at("requestId").get<int>();

	const auto& ref = j.at("objectReference");
	msg.objectReference.ip       = ref.at("ip").get<std::string>();
	msg.objectReference.port     = ref.at("port").get<int>();
	msg.objectReference.objectId = ref.at("objectId").get<std::string>();

	msg.methodId  = j.at("methodId").get<std::string>();
	msg.arguments = j.at("arguments").get<std::string>();
	return msg;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
// IPCModule — implementação
// ═══════════════════════════════════════════════════════════════════════════

IPCModule::IPCModule()
	: _server_fd(kInvalidSocket), _nextRequestId(1) {}

IPCModule::~IPCModule() {
	if (_server_fd != kInvalidSocket) {
		SocketUtils::CloseSocket(_server_fd);
	}
}

// ── Helpers de framing: [4 bytes tamanho][payload] ──

void IPCModule::sendRaw(SocketType fd, const std::string& data) {
	SocketUtils::SendUint32(fd, static_cast<std::uint32_t>(data.size()));
	if (!data.empty()) {
		if (!SocketUtils::SendAll(fd, data.data(), data.size())) {
			throw std::runtime_error("IPCModule::sendRaw — falha ao enviar payload.");
		}
	}
}

std::string IPCModule::recvRaw(SocketType fd) {
	const std::uint32_t size = SocketUtils::ReceiveUint32(fd);
	if (size == 0) {
		return {};
	}

	std::string buffer(size, '\0');
	if (!SocketUtils::RecvAll(fd, &buffer[0], buffer.size())) {
		throw std::runtime_error("IPCModule::recvRaw — falha ao receber payload.");
	}
	return buffer;
}

// ── Lado Cliente (Proxy) ──

std::string IPCModule::doOperation(const RemoteObjectRef& ref,
                                   const std::string& methodId,
                                   const std::string& arguments) {
	// 1. Criar socket TCP e conectar ao servidor remoto
	SocketType sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == kInvalidSocket) {
		throw std::runtime_error("IPCModule::doOperation — falha ao criar socket.");
	}
	SocketUtils::SocketGuard guard(sock);

	sockaddr_in server_addr{};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port   = htons(static_cast<std::uint16_t>(ref.port));

	if (inet_pton(AF_INET, ref.ip.c_str(), &server_addr.sin_addr) <= 0) {
		throw std::runtime_error("IPCModule::doOperation — IP invalido: " + ref.ip);
	}

	if (connect(sock, reinterpret_cast<const sockaddr*>(&server_addr),
	            static_cast<int>(sizeof(server_addr))) != 0) {
		throw std::runtime_error(
			"IPCModule::doOperation — falha ao conectar em " +
			ref.ip + ":" + std::to_string(ref.port));
	}

	// 2. Montar e enviar a mensagem RMI (Request)
	RMIMessage request;
	request.messageType    = MessageType::Request;
	request.requestId      = _nextRequestId++;
	request.objectReference = ref;
	request.methodId       = methodId;
	request.arguments      = arguments;

	const std::string requestSerialized = SerializeMessage(request);
	// ── Log de auditoria RMI ──
	std::cout << "\n[RMI REQUEST ENVIADO] -> "
	          << json::parse(requestSerialized).dump(2) << std::endl;

	sendRaw(sock, requestSerialized);

	// 3. Receber a resposta (Reply)
	std::string replyRaw = recvRaw(sock);
	RMIMessage reply = DeserializeMessage(replyRaw);

	// ── Log de auditoria RMI ──
	std::cout << "[RMI REPLY RECEBIDO]  <- "
	          << json::parse(replyRaw).dump(2) << std::endl;

	return reply.arguments;
}

// ── Lado Servidor (Dispatcher) ──

void IPCModule::initServer(int port) {
	_server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_server_fd == kInvalidSocket) {
		throw std::runtime_error("IPCModule::initServer — falha ao criar socket.");
	}

#ifdef _WIN32
	int exclusive = 1;
	setsockopt(_server_fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
	           reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
#else
	int reuse = 1;
	setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR,
	           reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#endif

	sockaddr_in addr{};
	addr.sin_family      = AF_INET;
	addr.sin_port        = htons(static_cast<std::uint16_t>(port));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(_server_fd, reinterpret_cast<const sockaddr*>(&addr),
	         static_cast<int>(sizeof(addr))) < 0) {
		SocketUtils::CloseSocket(_server_fd);
		_server_fd = kInvalidSocket;
		throw std::runtime_error("IPCModule::initServer — falha no bind na porta " +
		                         std::to_string(port));
	}

	if (listen(_server_fd, SOMAXCONN) < 0) {
		SocketUtils::CloseSocket(_server_fd);
		_server_fd = kInvalidSocket;
		throw std::runtime_error("IPCModule::initServer — falha no listen.");
	}
}

std::string IPCModule::getRequest(SocketType& out_client_fd) {
	out_client_fd = accept(_server_fd, nullptr, nullptr);
	if (out_client_fd == kInvalidSocket) {
		throw std::runtime_error("IPCModule::getRequest — falha no accept.");
	}

	return recvRaw(out_client_fd);
}

void IPCModule::sendReply(SocketType client_fd, const std::string& replyJson) {
	RMIMessage reply;
	reply.messageType = MessageType::Reply;
	reply.requestId   = 0;
	reply.objectReference = {"", 0, ""};
	reply.methodId    = "";
	reply.arguments   = replyJson;

	sendRaw(client_fd, SerializeMessage(reply));
	SocketUtils::CloseSocket(client_fd);
}
