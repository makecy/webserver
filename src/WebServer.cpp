#include "WebServer.hpp"
#include "Config.hpp"
#include "HttpRequest.hpp"
#include <sstream>

WebServer::WebServer() : _config(NULL) {
}

WebServer::~WebServer() {
    cleanup();
}

bool WebServer::initialize(const std::string& config_file) {
    _config = new Config();
    
    if (!_config->parseConfigFile(config_file)) {
        std::cout << "Using default configuration" << std::endl;
        _config->setDefaultConfig();
    }
    
    const std::vector<ServerConfig>& servers = _config->getServers();
    
    for (size_t i = 0; i < servers.size(); ++i) {
        int server_fd = createServerSocket(servers[i].host, servers[i].port);
        if (server_fd == -1) {
            std::cerr << "Failed to create server socket for " 
                     << servers[i].host << ":" << servers[i].port << std::endl;
            return false;
        }
        
        _server_sockets.push_back(server_fd);
        
        struct pollfd pfd;
        pfd.fd = server_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        _poll_fds.push_back(pfd);
        
        std::cout << "Server listening on " << servers[i].host 
                 << ":" << servers[i].port << std::endl;
    }
    
    return true;
}

int WebServer::createServerSocket(const std::string& host, int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        return -1;
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(server_fd);
        return -1;
    }
    
    if (fcntl(server_fd, F_SETFL, O_NONBLOCK) == -1) {
        close(server_fd);
        return -1;
    }
    
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 128) == -1) {
        close(server_fd);
        return -1;
    }
    
    return server_fd;
}

void WebServer::run() {
	std::cout << "server entering main loop..." << std::endl;	//debugging
    while (true) {
		std::cout << "\n---calling poll with " << _poll_fds.size() << " file descriptors..." << std::endl;	//debugging
        int poll_count = poll(&_poll_fds[0], _poll_fds.size(), -1);
        std::cout << "poll returned: " << poll_count << std::endl;	//debugging
        if (poll_count == -1) {
            std::cerr << "Poll error" << std::endl;
            break;
        }
        
        for (size_t i = 0; i < _poll_fds.size(); ++i) {
            if (_poll_fds[i].revents & POLLIN) {
				std::cout << "activity on fd " << _poll_fds[i].fd << std::endl;	//debugging
                bool is_server = false;
                for (size_t j = 0; j < _server_sockets.size(); ++j) {
                    if (_poll_fds[i].fd == _server_sockets[j]) {
						std::cout << "new connection on server socket " << _poll_fds[i].fd << std::endl;	//debugging
                        handleNewConnection(_poll_fds[i].fd);
                        is_server = true;
                        break;
                    }
                }
                
                if (!is_server) {
					std::cout << "client data on fd " << _poll_fds[i].fd << std::endl;	//debuggign
                    handleClientData(_poll_fds[i].fd, i);
                }
            }
        }
    }
}

void WebServer::handleNewConnection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    std::cout << "accepting connection on server_fd " << server_fd << std::endl; //debugging

    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd == -1) {
		std::cerr << "accept failed: " << strerror(errno) << std::endl; //debugging
        return;
    }

	std::cout << "nwe client connected: fd = " << client_fd << std::endl; // debugging
    
    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1) {
		std::cerr << "fcntl failed: " << strerror(errno) << std::endl; // debugg
        close(client_fd);
        return;
    }
    
    struct pollfd pfd;
    pfd.fd = client_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _poll_fds.push_back(pfd);
    
    _client_buffers[client_fd] = "";

	std::cout << "client " << client_fd << " added to poll list" << std::endl; // debugg
}

void WebServer::handleClientData(int client_fd, int poll_index) {
	std::cout << "reading data from client " << client_fd << std::endl; // debugg
    char buffer[8192];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

	std::cout << "recv() returned " << bytes_read << " bytes" << std::endl; // debugg

	if (bytes_read <= 0) {
		if (bytes_read == 0) {	//---added 30.07. chris
			std::cout << "client " << client_fd << " disconnected" << std::endl; // debugg
		} else {
			std::cout << "recv() error: " << strerror(errno) << std::endl; // debugg
		}						//---
        close(client_fd);
        _poll_fds.erase(_poll_fds.begin() + poll_index);
        _client_buffers.erase(client_fd);
        return;
    }
    
    buffer[bytes_read] = '\0';
    _client_buffers[client_fd] += buffer;
	std::cout << "buffer for client " << client_fd << ": [" << _client_buffers[client_fd] << "]" << std::endl; //debugg

    if ((_client_buffers[client_fd].find("\r\n\r\n") != std::string::npos)
			|| (_client_buffers[client_fd].find("\n\n") != std::string::npos)) { // -added 30.07. chris (for netcat)
		std::cout << "full http request received from client " << client_fd << std::endl; // debugg
        HttpRequest request;
        if (request.parseRequest(_client_buffers[client_fd])) {
			std::cout << "request parsed " << std::endl; //debugg
            std::string response = generateResponse(request);
			std::cout << "after generate response: [" << response << "]" << std::endl; //
            sendResponse(client_fd, response);
			std::cout << "after response sent to client " << client_fd << std::endl; //debugg
        }
		else {
			std::cout << "http request parse error" << std::endl; //  debugg
		}
        close(client_fd);
        _poll_fds.erase(_poll_fds.begin() + poll_index);
        _client_buffers.erase(client_fd);
		std::cout << "client " << client_fd << " connection closed" << std::endl; // debugg
    }
	else {
		std::cout << "waiting for more data..." << std::endl; // debugg
	}
}

void WebServer::sendResponse(int client_fd, const std::string& response) {
    send(client_fd, response.c_str(), response.length(), 0);
}

std::string WebServer::generateResponse(const HttpRequest& request) {
    std::string body = "<html><body>";
    body += "<h1>Webserv Response</h1>";
    body += "<p>Method: " + request.methodToString() + "</p>";
    body += "<p>URI: " + request.getUri() + "</p>";
    body += "<p>Version: " + request.getVersion() + "</p>";
    body += "</body></html>";
    
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/html\r\n";
    response += "Content-Length: " + toString(body.length()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;
    
    return response;
}

std::string WebServer::toString(size_t value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

void WebServer::cleanup() {
    for (size_t i = 0; i < _server_sockets.size(); ++i) {
        close(_server_sockets[i]);
    }
    
    delete _config;
    _config = NULL;
}
