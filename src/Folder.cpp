#include "Folder.hpp"

Folder::Folder()
	: StorageNode(), _parent_id(0) {}

Folder::Folder(std::uint64_t id, std::uint64_t parent_id, const std::string& name)
	: StorageNode(id, name), _parent_id(parent_id) {}

std::uint64_t Folder::getParentId() const {
	return _parent_id;
}

void Folder::setParentId(std::uint64_t parent_id) {
	_parent_id = parent_id;
}

nlohmann::json Folder::toJson() const {
	return {
		{"id", _id},
		{"name", _name},
		{"created_at", _created_at},
		{"parent_id", _parent_id}
	};
}

void Folder::fromJson(const nlohmann::json& j) {
	_id = j.at("id").get<std::uint64_t>();
	_name = j.at("name").get<std::string>();
	_created_at = j.value("created_at", std::uint64_t(0));
	_parent_id = j.at("parent_id").get<std::uint64_t>();
}
