#pragma once

#include "StorageNode.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class File : public StorageNode {
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

	std::uint64_t getFolderId() const;
	void setFolderId(std::uint64_t folder_id);

	std::uint64_t getSizeBytes() const;
	void setSizeBytes(std::uint64_t size_bytes);

	const std::vector<char>& getContent() const;
	void setContent(std::vector<char> content);

	nlohmann::json toJson() const override;
	void fromJson(const nlohmann::json& j) override;

private:
	std::uint64_t _folder_id;
	std::uint64_t _size_bytes;
	std::vector<char> _content;
};
