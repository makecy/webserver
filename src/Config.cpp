#include "Config.hpp"
#include "utils.hpp"
#include <fstream>
#include <iostream>

#include "Config.hpp"

Config::Config() {}

Config::~Config() {}

bool Config::parseConfigFile(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << filename << std::endl;
        return false;
    }

    bool in_server_block = false;
    ServerConfig current_server;
    std::string line;
    int line_number = 0;

    while (std::getline(file, line)) {
        line_number++;
        line = trim(line);

        if (shouldSkipLine(line)) {
            continue;
        }

        if (isServerStart(line)) {
            if (!handleServerStart(in_server_block, current_server, line_number, file)) {
                file.close();
                return false;
            }
        } else if (isServerEnd(line)) {
            if (!handleServerEnd(in_server_block, current_server, line_number, file)) {
                file.close();
                return false;
            }
        } else {
            if (!handleDirective(in_server_block, line, current_server, line_number, file)) {
                file.close();
                return false;
            }
        }
    }

    file.close();
    return finalizeConfig(in_server_block);
}

void Config::setDefaultConfig() {
    _servers.clear();
    ServerConfig default_server = getDefaultServerConfig();
    
    // Add default location
    LocationConfig default_location;
    default_location.path = "/";
    default_location.root = "./www";
    default_location.index = "index.html";
    default_location.autoindex = false;
    default_location.allowed_methods.push_back("GET");
    default_location.allowed_methods.push_back("POST");
    default_location.allowed_methods.push_back("DELETE");
    
    // Add CGI location
    LocationConfig cgi_location;
    cgi_location.path = "/cgi-bin";
    cgi_location.root = "./www";
    cgi_location.cgi_extension = ".py";
    cgi_location.cgi_path = "/usr/bin/python3";
    cgi_location.allowed_methods.push_back("GET");
    cgi_location.allowed_methods.push_back("POST");
    
    default_server.locations.push_back(default_location);
    default_server.locations.push_back(cgi_location);
    
    _servers.push_back(default_server);
}

ServerConfig Config::getDefaultServerConfig() {
    ServerConfig server;
    server.host = "127.0.0.1";
    server.port = 8080;
    server.server_name = "localhost";
    server.root = "./www";
    server.index = "index.html";
    server.client_max_body_size = 1048576; // 1MB
    server.error_pages[404] = "/error/404.html";
    server.error_pages[500] = "/error/500.html";
    return server;
}

bool Config::shouldSkipLine(const std::string& line) {
    return line.empty() || line[0] == '#';
}

bool Config::isServerStart(const std::string& line) {
    return line == "server" || line == "server {";
}

bool Config::isServerEnd(const std::string& line) {
    return line == "}";
}

bool Config::isLocationStart(const std::string& line) {
    return line.find("location") == 0 && line.find("{") != std::string::npos;
}

bool Config::isLocationEnd(const std::string& line) {
    return line == "}";
}

std::string Config::extractLocationPath(const std::string& line) {
    size_t start = line.find("location");
    if (start == std::string::npos) return "/";
    
    start += 8; // length of "location"
    size_t end = line.find("{");
    if (end == std::string::npos) return "/";
    
    std::string path = line.substr(start, end - start);
    return trim(path);
}

bool Config::handleServerStart(bool& in_server_block, ServerConfig& current_server, int line_number, std::ifstream& /* file */) {
    if (in_server_block) {
        std::cerr << "Error line " << line_number << ": nested server blocks not allowed" << std::endl;
        return false;
    }
    
    in_server_block = true;
    current_server = getDefaultServerConfig();
    
    return true;
}

bool Config::handleServerEnd(bool& in_server_block, ServerConfig& current_server, int line_number, std::ifstream& /* file */) {
    if (!in_server_block) {
        std::cerr << "Error line " << line_number << ": unexpected '}' outside server block" << std::endl;
        return false;
    }
    
    in_server_block = false;
    _servers.push_back(current_server);
    return true;
}

bool Config::handleDirective(bool in_server_block, const std::string& line, ServerConfig& current_server, int line_number, std::ifstream& file) {
    if (isLocationStart(line)) {
        if (!in_server_block) {
            std::cerr << "Error line " << line_number << ": location directive outside server block" << std::endl;
            return false;
        }
        std::string location_path = extractLocationPath(line);
        return parseLocationBlock(file, current_server, location_path, line_number);
    }
    
    if (in_server_block) {
        parseSimpleDirective(line, current_server);
    } else {
        std::cerr << "Error line " << line_number << ": directive outside server block: " << line << std::endl;
        return false;
    }
    
    return true;
}

bool Config::parseLocationBlock(std::ifstream& file, ServerConfig& server, const std::string& location_path, int& line_number) {
    LocationConfig location;
    location.path = location_path;
    location.root = server.root;
    location.index = server.index;
    
    std::string line;
    while (std::getline(file, line)) {
        line_number++;
        line = trim(line);
        
        if (shouldSkipLine(line)) continue;
        
        if (isLocationEnd(line)) {
            server.locations.push_back(location);
            return true;
        }
        
        parseSimpleDirective(line, location);
    }
    
    std::cerr << "Error: Unclosed location block starting at line " << line_number << std::endl;
    return false;
}

void Config::parseSimpleDirective(const std::string& line, ServerConfig& server) {
    std::vector<std::string> tokens = splitLine(line);
    if (tokens.empty()) return;
    
    std::string directive = tokens[0];
    
    if (directive == "listen" && tokens.size() >= 2) {
        std::string listen_value = tokens[1];
        size_t colon_pos = listen_value.find(':');
        
        if (colon_pos != std::string::npos) {
            server.host = listen_value.substr(0, colon_pos);
            server.port = std::atoi(listen_value.substr(colon_pos + 1).c_str());
        } else {
            server.port = std::atoi(listen_value.c_str());
        }
    } else if (directive == "server_name" && tokens.size() >= 2) {
        server.server_name = tokens[1];
    } else if (directive == "root" && tokens.size() >= 2) {
        server.root = tokens[1];
    } else if (directive == "index" && tokens.size() >= 2) {
        server.index = tokens[1];
    } else if (directive == "client_max_body_size" && tokens.size() >= 2) {
        server.client_max_body_size = std::atoi(tokens[1].c_str());
    } else if (directive == "error_page") {
        parseErrorPage(line, server.error_pages);
    }
}

void Config::parseSimpleDirective(const std::string& line, LocationConfig& location) {
    std::vector<std::string> tokens = splitLine(line);
    if (tokens.empty()) return;
    
    std::string directive = tokens[0];
    
    if (directive == "root" && tokens.size() >= 2) {
        location.root = tokens[1];
    } else if (directive == "index" && tokens.size() >= 2) {
        location.index = tokens[1];
    } else if (directive == "autoindex" && tokens.size() >= 2) {
        location.autoindex = (tokens[1] == "on");
    } else if (directive == "allow_methods" || directive == "methods") {
        parseAllowedMethods(line, location.allowed_methods);
    } else if (directive == "cgi_extension" && tokens.size() >= 2) {
        location.cgi_extension = tokens[1];
    } else if (directive == "cgi_path" && tokens.size() >= 2) {
        location.cgi_path = tokens[1];
    } else if (directive == "upload_path" && tokens.size() >= 2) {
        location.upload_path = tokens[1];
    } else if (directive == "error_page") {
        parseErrorPage(line, location.error_pages);
    } else if (directive == "return" && tokens.size() >= 2) {
        location.redirect = tokens[1];
    }
}

void Config::parseErrorPage(const std::string& line, std::map<int, std::string>& error_pages) {
    std::vector<std::string> tokens = splitLine(line);
    
    if (tokens.size() >= 3) {
        int error_code = std::atoi(tokens[1].c_str());
        std::string error_page = tokens[2];
        error_pages[error_code] = error_page;
    }
}

void Config::parseAllowedMethods(const std::string& line, std::vector<std::string>& methods) {
    std::vector<std::string> tokens = splitLine(line);
    methods.clear();
    
    for (size_t i = 1; i < tokens.size(); ++i) {
        std::string method = tokens[i];
        if (method == "GET" || method == "POST" || method == "DELETE" || method == "HEAD") {
            methods.push_back(method);
        }
    }
    
    if (methods.empty()) {
        methods.push_back("GET");
    }
}

std::vector<std::string> Config::splitLine(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    
    while (iss >> token) {
        // Remove semicolon from last token (C++98 compatible)
        if (!token.empty() && token[token.length() - 1] == ';') {
            token = token.substr(0, token.length() - 1);
        }
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string Config::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool Config::finalizeConfig(bool in_server_block) {
    if (in_server_block) {
        std::cerr << "Error: Unclosed server block" << std::endl;
        return false;
    }
    
    if (_servers.empty()) {
        std::cerr << "Warning: No servers configured, using default" << std::endl;
        setDefaultConfig();
    }
    
    return validateConfig();
}

const ServerConfig* Config::findServerConfig(const std::string& host, int port, const std::string& server_name) const {
    for (std::vector<ServerConfig>::const_iterator it = _servers.begin(); it != _servers.end(); ++it) {
        if (it->host == host && it->port == port) {
            if (server_name.empty() || it->server_name == server_name) {
                return &(*it);
            }
        }
    }
    
    for (std::vector<ServerConfig>::const_iterator it = _servers.begin(); it != _servers.end(); ++it) {
        if (it->port == port) {
            return &(*it);
        }
    }
    
    return _servers.empty() ? NULL : &_servers[0];
}

const LocationConfig* Config::findLocationConfig(const ServerConfig& server, const std::string& path) const {
    const LocationConfig* bestMatch = NULL;
    size_t bestMatchLength = 0;
    
    for (std::vector<LocationConfig>::const_iterator it = server.locations.begin(); 
         it != server.locations.end(); ++it) {
        const std::string& locPath = it->path;
        
        // Exact match has highest priority
        if (path == locPath) {
            return &(*it);
        }
        
        // Check if this location is a prefix match
        if (path.substr(0, locPath.length()) == locPath) {
            // For prefix matches, ensure we're matching at path boundaries
            if (locPath == "/" || path.length() == locPath.length() || path[locPath.length()] == '/') {
                // Prefer longer matches (more specific)
                if (locPath.length() > bestMatchLength) {
                    bestMatch = &(*it);
                    bestMatchLength = locPath.length();
                }
            }
        }
    }
    
    return bestMatch;
}

bool Config::validateConfig() const {
    for (std::vector<ServerConfig>::const_iterator it = _servers.begin(); it != _servers.end(); ++it) {
        if (it->port <= 0 || it->port > 65535) {
            std::cerr << "Error: Invalid port number " << it->port << std::endl;
            return false;
        }
        
        if (it->root.empty()) {
            std::cerr << "Error: Empty root directory" << std::endl;
            return false;
        }
        
        if (it->client_max_body_size == 0) {
            std::cerr << "Error: Invalid client_max_body_size" << std::endl;
            return false;
        }
    }
    
    return true;
}

void Config::printConfig() const {
    for (size_t i = 0; i < _servers.size(); ++i) {
        const ServerConfig& server = _servers[i];
        std::cout << "Server " << i << ":" << std::endl;
        std::cout << "  Host: " << server.host << std::endl;
        std::cout << "  Port: " << server.port << std::endl;
        std::cout << "  Server Name: " << server.server_name << std::endl;
        std::cout << "  Root: " << server.root << std::endl;
        std::cout << "  Index: " << server.index << std::endl;
        std::cout << "  Max Body Size: " << server.client_max_body_size << std::endl;
        
        for (size_t j = 0; j < server.locations.size(); ++j) {
            const LocationConfig& loc = server.locations[j];
            std::cout << "  Location " << loc.path << ":" << std::endl;
            std::cout << "    Root: " << loc.root << std::endl;
            std::cout << "    Methods: ";
            for (size_t k = 0; k < loc.allowed_methods.size(); ++k) {
                std::cout << loc.allowed_methods[k] << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
}

bool Config::isMethodAllowed(const std::string& method, const LocationConfig* location) const {
    if (!location) return true; // No location restrictions
    
    const std::vector<std::string>& allowedMethods = location->allowed_methods;
    if (allowedMethods.empty()) return true; // No method restrictions
    
    // Check if method is in allowed list
    for (std::vector<std::string>::const_iterator it = allowedMethods.begin(); 
         it != allowedMethods.end(); ++it) {
        if (*it == method) return true;
    }
    return false;
}