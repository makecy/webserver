#include "WebServer.hpp"
#include "Config.hpp"
#include <iostream>

int main(int argc, char** argv) {
    std::string config_file = "config/default.conf";
    
    if (argc > 2) {
        log_error("usage: ./webserv <config file>");
        return 1;
    }
    
    if (argc == 2) {
        config_file = argv[1];
    }
    
    // Parse configuration first
    Config config;
    if (!config.parseConfigFile(config_file)) {
        log_error("failed to parse config file, using defaults");
        config.setDefaultConfig();
    }
    
    // Show what was parsed (optional - remove in production)
    std::cout << "=== Loaded Configuration ===" << std::endl;
    config.printConfig();
    std::cout << "============================" << std::endl;
    
    WebServer server;
    
    // Pass the parsed config instead of the file path
    if (!server.initialize(config_file)) {
        log_error("failed to initialize");
        return 1;
    }
    
    log_info("starting webserver...");
    server.run();
    
    return 0;
}