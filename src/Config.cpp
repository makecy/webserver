#include "Config.hpp"
#include <fstream>
#include <iostream>

Config::Config() {
}

Config::~Config() {
}

bool Config::parseConfigFile(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        return false;
    }
    file.close();
    return false;
}

void Config::setDefaultConfig() {
    ServerConfig default_server;
    default_server.host = "127.0.0.1";
    default_server.port = 8080;
    default_server.server_name = "localhost";
    default_server.root = "./www";
    default_server.index = "index.html";
    default_server.client_max_body_size = 1024 * 1024;
    
    _servers.push_back(default_server);
}