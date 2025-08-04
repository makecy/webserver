
#include "WebServer.hpp"
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
    
    WebServer server;
    
    if (!server.initialize(config_file)) {
        log_error("failed to initialize");
        return 1;
    }
    
    log_info("starting webserver...");
    server.run();
    
    return 0;
}