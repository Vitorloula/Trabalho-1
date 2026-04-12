#include "File.hpp"

#include <array>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

void WriteUint32(std::ostream& out, std::uint32_t value) {
	std::array<std::uint8_t, 4> buffer = {
		static_cast<std::uint8_t>((value >> 24) & 0xFF),
		static_cast<std::uint8_t>((value >> 16) & 0xFF),
		static_cast<std::uint8_t>((value >> 8) & 0xFF),
		static_cast<std::uint8_t>(value & 0xFF)};

	out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
	if (!out) {
		throw std::runtime_error("Falha ao escrever uint32 no stream de destino.");
	}
}

void WriteUint64(std::ostream& out, std::uint64_t value) {
	std::array<std::uint8_t, 8> buffer = {
		static_cast<std::uint8_t>((value >> 56) & 0xFF),
		static_cast<std::uint8_t>((value >> 48) & 0xFF),
		static_cast<std::uint8_t>((value >> 40) & 0xFF),
		static_cast<std::uint8_t>((value >> 32) & 0xFF),
		static_cast<std::uint8_t>((value >> 24) & 0xFF),
		static_cast<std::uint8_t>((value >> 16) & 0xFF),
		static_cast<std::uint8_t>((value >> 8) & 0xFF),
		static_cast<std::uint8_t>(value & 0xFF)};

	out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
	if (!out) {
		throw std::runtime_error("Falha ao escrever uint64 no stream de destino.");
	}
}

std::uint32_t ReadUint32(std::istream& in) {
	std::array<std::uint8_t, 4> buffer{};
	in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
	if (!in) {
		throw std::runtime_error("Falha ao ler uint32 do stream de origem.");
	}

	return (static_cast<std::uint32_t>(buffer[0]) << 24) |
		   (static_cast<std::uint32_t>(buffer[1]) << 16) |
		   (static_cast<std::uint32_t>(buffer[2]) << 8) |
		   static_cast<std::uint32_t>(buffer[3]);
}

std::uint64_t ReadUint64(std::istream& in) {
	std::array<std::uint8_t, 8> buffer{};
	in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
	if (!in) {
		throw std::runtime_error("Falha ao ler uint64 do stream de origem.");
	}

	return (static_cast<std::uint64_t>(buffer[0]) << 56) |
		   (static_cast<std::uint64_t>(buffer[1]) << 48) |
		   (static_cast<std::uint64_t>(buffer[2]) << 40) |
		   (static_cast<std::uint64_t>(buffer[3]) << 32) |
		   (static_cast<std::uint64_t>(buffer[4]) << 24) |
		   (static_cast<std::uint64_t>(buffer[5]) << 16) |
		   (static_cast<std::uint64_t>(buffer[6]) << 8) |
		   static_cast<std::uint64_t>(buffer[7]);
}

void WriteString(std::ostream& out, const std::string& value) {
	if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
		throw std::runtime_error("String grande demais para serializacao (max uint32). ");
	}

	const auto length = static_cast<std::uint32_t>(value.size());
	WriteUint32(out, length);

	if (length == 0) {
		return;
	}

	out.write(value.data(), static_cast<std::streamsize>(length));
	if (!out) {
		throw std::runtime_error("Falha ao escrever string no stream de destino.");
	}
}

std::string ReadString(std::istream& in) {
	const std::uint32_t length = ReadUint32(in);

	if (length == 0) {
		return {};
	}

	std::string value(length, '\0');
	in.read(&value[0], static_cast<std::streamsize>(length));
	if (!in) {
		throw std::runtime_error("Falha ao ler string do stream de origem.");
	}

	return value;
}

} // namespace

File::File()
	: _id(0), _folder_id(0), _name(), _size_bytes(0) {}

File::File(
	std::uint64_t id,
	std::uint64_t folder_id,
	const std::string& name,
	std::uint64_t size_bytes)
	: _id(id), _folder_id(folder_id), _name(name), _size_bytes(size_bytes) {}

std::uint64_t File::getId() const {
	return _id;
}

void File::setId(std::uint64_t id) {
	_id = id;
}

std::uint64_t File::getFolderId() const {
	return _folder_id;
}

void File::setFolderId(std::uint64_t folder_id) {
	_folder_id = folder_id;
}

const std::string& File::getName() const {
	return _name;
}

void File::setName(const std::string& name) {
	_name = name;
}

std::uint64_t File::getSizeBytes() const {
	return _size_bytes;
}

void File::setSizeBytes(std::uint64_t size_bytes) {
	_size_bytes = size_bytes;
}


//FileOutputStream


FileOutputStream::FileOutputStream(const File* files, std::size_t count, std::ostream& destination)
	: _files(files), _count(count), _destination(destination) {
	if (_count > 0 && _files == nullptr) {
		throw std::invalid_argument("O array de File nao pode ser nulo quando count > 0.");
	}
}


void FileOutputStream::write() {
	for (std::size_t i = 0; i < _count; ++i) {
		WriteUint64(_destination, _files[i].getId());
		WriteUint64(_destination, _files[i].getFolderId());
		WriteString(_destination, _files[i].getName());
		WriteUint64(_destination, _files[i].getSizeBytes());
	}
}


//FIleInputStream


FileInputStream::FileInputStream(std::istream& source)
	: _source(source) {}

std::vector<File> FileInputStream::readFiles(int count) {
	if (count < 0) {
		throw std::invalid_argument("count nao pode ser negativo.");
	}

	std::vector<File> files;
	files.reserve(static_cast<std::size_t>(count));

	for (int i = 0; i < count; ++i) {
		const std::uint64_t id = ReadUint64(_source);
		const std::uint64_t folder_id = ReadUint64(_source);
		const std::string name = ReadString(_source);
		const std::uint64_t size_bytes = ReadUint64(_source);

		files.emplace_back(id, folder_id, name, size_bytes);
	}

	return files;
}