#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

class File {
public:
	File();
	File(
		std::uint64_t id,
		std::uint64_t folder_id,
		const std::string& name,
		std::uint64_t size_bytes);
	File(
		std::uint64_t id,
		std::uint64_t folder_id,
		const std::string& name,
		std::uint64_t size_bytes,
		std::vector<char> content);

	std::uint64_t getId() const;
	void setId(std::uint64_t id);

	std::uint64_t getFolderId() const;
	void setFolderId(std::uint64_t folder_id);

	const std::string& getName() const;
	void setName(const std::string& name);

	std::uint64_t getSizeBytes() const;
	void setSizeBytes(std::uint64_t size_bytes);

	const std::vector<char>& getContent() const;
	void setContent(std::vector<char> content);

private:
	std::uint64_t _id;
	std::uint64_t _folder_id;
	std::string _name;
	std::uint64_t _size_bytes;
	std::vector<char> _content;
};

class FileOutputStream {
public:
	FileOutputStream(const File* files, std::size_t count, std::ostream& destination);

	void write();

private:
	const File* _files;
	std::size_t _count;
	std::ostream& _destination;
};

class FileInputStream {
public:
	explicit FileInputStream(std::istream& source);

	std::vector<File> readFiles(int count);

private:
	std::istream& _source;
};
