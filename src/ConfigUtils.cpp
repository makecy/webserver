#include "Config.hpp"

std::string Config::trim(const std::string& str) {
	size_t start = str.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return "";
	size_t end = str.find_last_not_of(" \t\r\n");
	return str.substr(start, end - start + 1);
}

bool Config::shouldSkipLine(const std::string& line) {
	std::string trimmed = trim(line);
	return trimmed.empty() || trimmed[0] == '#';
}

bool Config::isServerStart(const std::string& line) {
	return trim(line) == "server {";
}

bool Config::isServerEnd(const std::string& line) {
	return trim(line) == "}";
}

bool Config::handleDirective(bool in_server_block, const std::string& line, ServerConfig& current_server, int line_number, std::ifstream& file) {
	if (!in_server_block) {
		log_error("directive outside server block (line " + int_to_string(line_number) + ")");
		file.close();
		return false;
	}
	parseSimpleDirective(line, current_server);
	return true;
}

bool Config::finalizeConfig(bool in_server_block) {
	if (in_server_block) {
		log_error("unclosed server block");
		return false;
	}
		
	if (_servers.empty()) {
		log_error("no server blocks found in config");
		return false;
	}
		
	log_info("success parsing " + size_t_to_string(_servers.size()) + " server(s)");
	return true;
}

bool Config::handleServerStart(bool& in_server_block, ServerConfig& current_server, int line_number, std::ifstream& file) {
	if (in_server_block) {
		log_error("nested server blocks not allowed (line " + int_to_string(line_number) + ")");
		file.close();
		return false;
	}
	in_server_block = true;
	current_server = getDefaultServerConfig();
	log_debug("found server block");
	return true;
}

bool Config::handleServerEnd(bool& in_server_block, ServerConfig& current_server, int line_number, std::ifstream& file) {
	if (!in_server_block) {
		log_error("unexpected '}' (line " + int_to_string(line_number) + ")");
		file.close();
		return false;
	}
	in_server_block = false;
	_servers.push_back(current_server);
	log_info("parsed server: " + current_server.host + ":" + int_to_string(current_server.port));
	return true;
}