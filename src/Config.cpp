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

	if (!value.empty() && value[value.length() - 1] == ';')
		value.erase(value.length() - 1);
	
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
	} else
		log_debug("unknown dir: " + key);
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
	int line_number = 0;

	while (std::getline(file, line)) {
		line_number++;
		if (shouldSkipLine(line))
			continue;
		if (isServerStart(line)) {
			if (!handleServerStart(in_server_block, current_server, line_number, file))
				return false;
			continue;
		}
		if (isServerEnd(line)) {
			if (!handleServerEnd(in_server_block, current_server, line_number, file))
				return false;
			continue;
		}
		if (!handleDirective(in_server_block, line, current_server, line_number, file))
			return false;
	}
	file.close();
	return finalizeConfig(in_server_block);
}
