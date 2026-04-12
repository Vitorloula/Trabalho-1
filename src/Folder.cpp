#include "Folder.hpp"

Folder::Folder()
	: _id(0), _parent_id(0), _name() {}

Folder::Folder(std::uint64_t id, std::uint64_t parent_id, const std::string& name)
	: _id(id), _parent_id(parent_id), _name(name) {}

std::uint64_t Folder::getId() const {
	return _id;
}

void Folder::setId(std::uint64_t id) {
	_id = id;
}

std::uint64_t Folder::getParentId() const {
	return _parent_id;
}

void Folder::setParentId(std::uint64_t parent_id) {
	_parent_id = parent_id;
}

const std::string& Folder::getName() const {
	return _name;
}

void Folder::setName(const std::string& name) {
	_name = name;
}