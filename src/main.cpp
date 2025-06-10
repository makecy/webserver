#include "WebServer.hpp"
#include <iostream>

int main(int argc, char** argv) {
    std::string config_file = "config/default.conf";
    
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [configuration file]" << std::endl;
        return 1;
    }
    
    if (argc == 2) {
        config_file = argv[1];
    }
    
    WebServer server;
    
    if (!server.initialize(config_file)) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }
    
    std::cout << "Starting webserver..." << std::endl;
    server.run();
    
    return 0;
}