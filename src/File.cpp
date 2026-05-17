#include "File.hpp"

#include <stdexcept>
#include <vector>

// ── Base64 helpers (para serializar conteúdo binário em JSON) ──

namespace {

static const char kBase64Chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

std::string Base64Encode(const std::vector<char>& data) {
	std::string encoded;
	if (data.empty()) return encoded;

	const auto* bytes = reinterpret_cast<const unsigned char*>(data.data());
	std::size_t len = data.size();
	encoded.reserve(((len + 2) / 3) * 4);

	for (std::size_t i = 0; i < len; i += 3) {
		unsigned int triple = static_cast<unsigned int>(bytes[i]) << 16;
		if (i + 1 < len) triple |= static_cast<unsigned int>(bytes[i + 1]) << 8;
		if (i + 2 < len) triple |= static_cast<unsigned int>(bytes[i + 2]);

		encoded.push_back(kBase64Chars[(triple >> 18) & 0x3F]);
		encoded.push_back(kBase64Chars[(triple >> 12) & 0x3F]);
		encoded.push_back((i + 1 < len) ? kBase64Chars[(triple >> 6) & 0x3F] : '=');
		encoded.push_back((i + 2 < len) ? kBase64Chars[triple & 0x3F] : '=');
	}
	return encoded;
}

static int Base64CharIndex(char c) {
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

std::vector<char> Base64Decode(const std::string& encoded) {
	std::vector<char> decoded;
	if (encoded.empty()) return decoded;

	decoded.reserve((encoded.size() / 4) * 3);

	for (std::size_t i = 0; i < encoded.size(); i += 4) {
		int a = Base64CharIndex(encoded[i]);
		int b = (i + 1 < encoded.size()) ? Base64CharIndex(encoded[i + 1]) : -1;
		int c = (i + 2 < encoded.size()) ? Base64CharIndex(encoded[i + 2]) : -1;
		int d = (i + 3 < encoded.size()) ? Base64CharIndex(encoded[i + 3]) : -1;

		if (a < 0 || b < 0) break;

		decoded.push_back(static_cast<char>((a << 2) | (b >> 4)));
		if (c >= 0) decoded.push_back(static_cast<char>(((b & 0x0F) << 4) | (c >> 2)));
		if (d >= 0) decoded.push_back(static_cast<char>(((c & 0x03) << 6) | d));
	}
	return decoded;
}

} // namespace


// ── File ──

File::File()
	: StorageNode(), _folder_id(0), _size_bytes(0), _content() {}

File::File(
	std::uint64_t id,
	std::uint64_t folder_id,
	const std::string& name,
	std::uint64_t size_bytes)
	: StorageNode(id, name), _folder_id(folder_id), _size_bytes(size_bytes), _content() {}

File::File(
	std::uint64_t id,
	std::uint64_t folder_id,
	const std::string& name,
	std::uint64_t size_bytes,
	std::vector<char> content)
	: StorageNode(id, name), _folder_id(folder_id), _size_bytes(size_bytes), _content(std::move(content)) {}

std::uint64_t File::getFolderId() const {
	return _folder_id;
}

void File::setFolderId(std::uint64_t folder_id) {
	_folder_id = folder_id;
}

std::uint64_t File::getSizeBytes() const {
	return _size_bytes;
}

void File::setSizeBytes(std::uint64_t size_bytes) {
	_size_bytes = size_bytes;
}

const std::vector<char>& File::getContent() const {
	return _content;
}

void File::setContent(std::vector<char> content) {
	_content = std::move(content);
}

nlohmann::json File::toJson() const {
	return {
		{"id", _id},
		{"name", _name},
		{"created_at", _created_at},
		{"folder_id", _folder_id},
		{"size_bytes", _size_bytes},
		{"content_base64", Base64Encode(_content)}
	};
}

void File::fromJson(const nlohmann::json& j) {
	_id = j.at("id").get<std::uint64_t>();
	_name = j.at("name").get<std::string>();
	_created_at = j.value("created_at", std::uint64_t(0));
	_folder_id = j.at("folder_id").get<std::uint64_t>();
	_size_bytes = j.at("size_bytes").get<std::uint64_t>();

	if (j.contains("content_base64") && !j["content_base64"].get<std::string>().empty()) {
		_content = Base64Decode(j["content_base64"].get<std::string>());
	} else {
		_content.clear();
	}
}
