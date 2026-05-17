#include "Workspace.hpp"

Workspace::Workspace()
	: _id(0), _name(), _owner(), _files() {}

Workspace::Workspace(std::uint64_t id, const std::string& name, const User& owner)
	: _id(id), _name(name), _owner(owner), _files() {}

std::uint64_t Workspace::getId() const {
	return _id;
}

void Workspace::setId(std::uint64_t id) {
	_id = id;
}

const std::string& Workspace::getName() const {
	return _name;
}

void Workspace::setName(const std::string& name) {
	_name = name;
}

const User& Workspace::getOwner() const {
	return _owner;
}

void Workspace::setOwner(const User& owner) {
	_owner = owner;
}

const std::vector<File>& Workspace::getFiles() const {
	return _files;
}

void Workspace::setFiles(std::vector<File> files) {
	_files = std::move(files);
}

void Workspace::addFile(const File& file) {
	_files.push_back(file);
}

nlohmann::json Workspace::toJson() const {
	nlohmann::json filesJson = nlohmann::json::array();
	for (const auto& f : _files) {
		filesJson.push_back(f.toJson());
	}

	return {
		{"id", _id},
		{"name", _name},
		{"owner", _owner.toJson()},
		{"files", filesJson}
	};
}

void Workspace::fromJson(const nlohmann::json& j) {
	_id = j.at("id").get<std::uint64_t>();
	_name = j.at("name").get<std::string>();

	_owner.fromJson(j.at("owner"));

	_files.clear();
	if (j.contains("files")) {
		for (const auto& fj : j.at("files")) {
			File f;
			f.fromJson(fj);
			_files.push_back(std::move(f));
		}
	}
}
