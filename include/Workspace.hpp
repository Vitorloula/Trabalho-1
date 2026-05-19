#pragma once

#include "File.hpp"
#include "User.hpp"

#include <cstdint>
#include <string>
#include <vector>

class Workspace {
public:
	Workspace();
	Workspace(std::uint64_t id, const std::string& name, const User& owner);

	std::uint64_t getId() const;
	void setId(std::uint64_t id);

	const std::string& getName() const;
	void setName(const std::string& name);

	const User& getOwner() const;
	void setOwner(const User& owner);

	const std::vector<File>& getFiles() const;
	void setFiles(std::vector<File> files);
	void addFile(const File& file);

	nlohmann::json toJson() const;
	void fromJson(const nlohmann::json& j);

private:
	std::uint64_t _id;
	std::string _name;
	User _owner;                
	std::vector<File> _files;  
};
