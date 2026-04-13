#pragma once

#include <cstdint>
#include <string>

class Folder {
public:
	Folder();
	Folder(std::uint64_t id, std::uint64_t parent_id, const std::string& name);

	std::uint64_t getId() const;
	void setId(std::uint64_t id);

	std::uint64_t getParentId() const;
	void setParentId(std::uint64_t parent_id);

	const std::string& getName() const;
	void setName(const std::string& name);

private:
	std::uint64_t _id;
	std::uint64_t _parent_id;
	std::string _name;
};
