#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "WebServer.hpp"

struct ServerConfig {
    std::string host;
    int port;
    std::string server_name;
    std::string root;
    std::string index;
    size_t client_max_body_size;
    std::map<int, std::string> error_pages;
};

class Config {
private:
    std::vector<ServerConfig> _servers;
    void parseSimpleDirective(const std::string& line, ServerConfig& server);
    ServerConfig getDefaultServerConfig();
    
public:
    Config();
    ~Config();
    
    bool parseConfigFile(const std::string& filename);
    void setDefaultConfig();
    
    const std::vector<ServerConfig>& getServers() const { return _servers; }
};

#endif