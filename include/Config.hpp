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
    bool finalizeConfig(bool in_server_block);

    //conf utility
    bool shouldSkipLine(const std::string& line);
    bool isServerStart(const std::string& line);
    bool isServerEnd(const std::string& line);
    bool handleServerStart(bool& in_server_block, ServerConfig& current_server, int line_number, std::ifstream& file);
    bool handleServerEnd(bool& in_server_block, ServerConfig& current_server, int line_number, std::ifstream& file);
    bool handleDirective(bool in_server_block, const std::string& line, ServerConfig& current_server, int line_number, std::ifstream& file);
    std::string trim(const std::string& str);
    
public:
    Config();
    ~Config();
    
    bool parseConfigFile(const std::string& filename);
    void setDefaultConfig();
    
    const std::vector<ServerConfig>& getServers() const { return _servers; }
};

#endif