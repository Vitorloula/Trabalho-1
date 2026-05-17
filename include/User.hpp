#pragma once

#include <cstdint>
#include <string>
#include "json.hpp"

class User {
public:
	User();
	User(std::uint64_t id, const std::string& username, const std::string& email);

	std::uint64_t getId() const;
	void setId(std::uint64_t id);

	const std::string& getUsername() const;
	void setUsername(const std::string& username);

	const std::string& getEmail() const;
	void setEmail(const std::string& email);

	nlohmann::json toJson() const;
	void fromJson(const nlohmann::json& j);

private:
	std::uint64_t _id;
	std::string _username;
	std::string _email;
};
