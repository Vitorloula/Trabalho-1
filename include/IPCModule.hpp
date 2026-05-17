#pragma once

#include "Utils.hpp"

#include <cstdint>
#include <string>

// ═══════════════════════════════════════════════════════════════════════════
// Structs fundamentais do RMI (Coulouris, Cap. 5)
// ═══════════════════════════════════════════════════════════════════════════

/// Referência remota a um objeto no servidor
struct RemoteObjectRef {
	std::string ip;
	int port;
	std::string objectId;
};

/// Tipo da mensagem RMI
enum class MessageType : int {
	Request = 0,
	Reply   = 1
};

/// Mensagem trocada entre Proxy e Dispatcher
struct RMIMessage {
	MessageType messageType;        // 0 = Request, 1 = Reply
	int requestId;
	RemoteObjectRef objectReference;
	std::string methodId;
	std::string arguments;          // JSON stringificado
};

// ═══════════════════════════════════════════════════════════════════════════
// IPCModule — encapsula toda a comunicação de rede
// ═══════════════════════════════════════════════════════════════════════════

class IPCModule {
public:
	IPCModule();
	~IPCModule();

	// ── Lado Cliente (Proxy) ──
	/// Conecta ao objeto remoto, envia request, bloqueia até reply, retorna resultado.
	std::string doOperation(const RemoteObjectRef& ref,
	                        const std::string& methodId,
	                        const std::string& arguments);

	// ── Lado Servidor (Dispatcher) ──
	/// Faz bind+listen na porta especificada.
	void initServer(int port);

	/// Aceita conexão e lê a mensagem de requisição completa.
	/// Retorna o JSON da mensagem RMI. O socket do cliente é devolvido via out_client_fd.
	std::string getRequest(SocketType& out_client_fd);

	/// Empacota o reply e envia no socket do cliente. Fecha o socket após envio.
	void sendReply(SocketType client_fd, const std::string& replyJson);

private:
	SocketType _server_fd;
	int _nextRequestId;

	/// Envia uma string com prefixo de 4 bytes (tamanho em network byte order).
	static void sendRaw(SocketType fd, const std::string& data);

	/// Recebe uma string com prefixo de 4 bytes de tamanho.
	static std::string recvRaw(SocketType fd);
};
