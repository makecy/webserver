#include "Config.hpp"
#include "utils.hpp"
#include <fstream>
#include <iostream>

Config::Config() {
}

Config::~Config() {
}

ServerConfig Config::getDefaultServerConfig() {
	ServerConfig default_server;
	default_server.host = "127.0.0.1";
	default_server.port = 8080;
	default_server.server_name = "localhost";
	default_server.root = "./www";
	default_server.index = "index.html";
	default_server.client_max_body_size = 1024 * 1024;
	return default_server;
}

void Config::setDefaultConfig() {
	_servers.push_back(getDefaultServerConfig());
	log_info("using default configuration");
}

void Config::parseSimpleDirective(const std::string& line, ServerConfig& server) {
	std::istringstream iss(line);
	std::string key, value;
	iss >> key >> value;

	if (!value.empty() && value[value.length() - 1] == ';') {
		value.erase(value.length() - 1);
	}
	
	if (key == "listen") {
		// handle host and port
		size_t colon_pos = value.find(':');
		if (colon_pos != std::string::npos) {
			server.host = value.substr(0, colon_pos);
			server.port = atoi(value.substr(colon_pos + 1).c_str());
		} else {
			server.port = atoi(value.c_str());
		}
		log_debug("parsed listen: " + server.host + ":" + int_to_string(server.port));
	} else if (key == "server_name") {
		server.server_name = value;
		log_debug("parsed server_name: " + value);
	} else if (key == "root") {
		server.root = value;
		log_debug("parsed root: " + value);
	} else if (key == "index") {
		server.index = value;
		log_debug("parsed index: " + value);
	} else {
		log_debug("unknown dir: " + key);
	}
}

bool Config::parseConfigFile(const std::string& filename) {
	std::ifstream file(filename.c_str());
	if (!file.is_open()) {
		log_error("cant open config file: " + filename);
		return false;
	}
	
	std::string line;
	ServerConfig current_server;
	bool in_server_block = false;

	while (std::getline(file, line)) {
		// Skip empty lines and comments
		if (line.empty() || line[0] == '#' || line.find_first_not_of(" \t") == std::string::npos) // jump empty lines and comments
			continue;
		if (line.find("server {") != std::string::npos) {
			in_server_block = true;
			current_server = getDefaultServerConfig();
			log_debug("found server block");
			continue;
		}
		if (line.find("}") != std::string::npos && in_server_block) {
			in_server_block = false;
			_servers.push_back(current_server);
			log_info("parsed server: " + current_server.host + ":" + int_to_string(current_server.port));
			continue;
		}
		
		if (in_server_block)
			parseSimpleDirective(line, current_server);
	}
	
	file.close();
	if (_servers.empty()) {
		log_error("no server blocks found in config");
		return false;
	}
	log_info("success parsing " + size_t_to_string(_servers.size()) + " server(s)");
	return true;
}