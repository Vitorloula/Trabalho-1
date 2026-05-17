#include "StorageNode.hpp"

StorageNode::StorageNode()
	: _id(0), _name(), _created_at(0) {}

StorageNode::StorageNode(std::uint64_t id, const std::string& name, std::uint64_t created_at)
	: _id(id), _name(name), _created_at(created_at) {}

std::uint64_t StorageNode::getId() const {
	return _id;
}

void StorageNode::setId(std::uint64_t id) {
	_id = id;
}

const std::string& StorageNode::getName() const {
	return _name;
}

void StorageNode::setName(const std::string& name) {
	_name = name;
}

std::uint64_t StorageNode::getCreatedAt() const {
	return _created_at;
}

void StorageNode::setCreatedAt(std::uint64_t created_at) {
	_created_at = created_at;
}
