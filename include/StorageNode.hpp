#pragma once

#include <cstdint>
#include <string>
#include "json.hpp"

class StorageNode {
public:
	StorageNode();
	StorageNode(std::uint64_t id, const std::string& name, std::uint64_t created_at = 0);
	virtual ~StorageNode() = default;

	std::uint64_t getId() const;
	void setId(std::uint64_t id);

	const std::string& getName() const;
	void setName(const std::string& name);

	std::uint64_t getCreatedAt() const;
	void setCreatedAt(std::uint64_t created_at);

	virtual nlohmann::json toJson() const = 0;
	virtual void fromJson(const nlohmann::json& j) = 0;

protected:
	std::uint64_t _id;
	std::string _name;
	std::uint64_t _created_at;
};
