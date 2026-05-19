#include "User.hpp"

User::User()
	: _id(0), _username(), _email() {}

User::User(std::uint64_t id, const std::string& username, const std::string& email)
	: _id(id), _username(username), _email(email) {}

std::uint64_t User::getId() const {
	return _id;
}

void User::setId(std::uint64_t id) {
	_id = id;
}

const std::string& User::getUsername() const {
	return _username;
}

void User::setUsername(const std::string& username) {
	_username = username;
}

const std::string& User::getEmail() const {
	return _email;
}

void User::setEmail(const std::string& email) {
	_email = email;
}

nlohmann::json User::toJson() const {
	return {
		{"id", _id},
		{"username", _username},
		{"email", _email}
	};
}

void User::fromJson(const nlohmann::json& j) {
	_id = j.at("id").get<std::uint64_t>();
	_username = j.at("username").get<std::string>();
	_email = j.at("email").get<std::string>();
}
