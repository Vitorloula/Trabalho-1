#pragma once

#include "StorageNode.hpp"

#include <cstdint>
#include <string>

class Folder : public StorageNode {
public:
	Folder();
	Folder(std::uint64_t id, std::uint64_t parent_id, const std::string& name);

	std::uint64_t getParentId() const;
	void setParentId(std::uint64_t parent_id);

	nlohmann::json toJson() const override;
	void fromJson(const nlohmann::json& j) override;

private:
	std::uint64_t _parent_id;
};
