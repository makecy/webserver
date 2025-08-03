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
		LOG_INFO("Using default configuration");
		_config->setDefaultConfig();
	}
	
	const std::vector<ServerConfig>& servers = _config->getServers();
	
	for (size_t i = 0; i < servers.size(); ++i) {
		int server_fd = createServerSocket(servers[i].host, servers[i].port);
		if (server_fd == -1) {
			LOG_ERROR("Failed to create server socket for " + servers[i].host + ":" + toString(servers[i].port));
			return false;
		}
		
		_server_sockets.push_back(server_fd);
		
		struct pollfd pfd;
		pfd.fd = server_fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		_poll_fds.push_back(pfd);
		
		LOG_INFO("Server listening on " + servers[i].host + ":" + toString(servers[i].port));
	}
	
	return true;
}

int WebServer::createServerSocket(const std::string& host, int port) {
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		LOG_ERROR("Failed to create socket");
		return -1;
	}
	
	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		LOG_ERROR("Failed to set socket options");
		close(server_fd);
		return -1;
	}
	
	if (fcntl(server_fd, F_SETFL, O_NONBLOCK) == -1) {
		LOG_ERROR("Failed to set socket to non-blocking");
		close(server_fd);
		return -1;
	}
	
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(host.c_str());
	
	if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		LOG_ERROR("Failed to bind socket to " + host + ":" + toString(port));
		close(server_fd);
		return -1;
	}
	
	if (listen(server_fd, 128) == -1) {
		LOG_ERROR("Failed to listen on socket");
		close(server_fd);
		return -1;
	}
	
	return server_fd;
}

void WebServer::run() {
	LOG_INFO("Server entering main loop...");
	while (true) {
		LOG_DEBUG("Calling poll with " + toString(_poll_fds.size()) + " file descriptors...");
		int poll_count = poll(&_poll_fds[0], _poll_fds.size(), -1);
		LOG_DEBUG("Poll returned: " + toString(poll_count));
		
		if (poll_count == -1) {
			LOG_ERROR("Poll error: " + std::string(strerror(errno)));
			break;
		}
		
		for (size_t i = 0; i < _poll_fds.size(); ++i) {
			if (_poll_fds[i].revents & POLLIN) {
				LOG_DEBUG("Activity on fd " + toString(_poll_fds[i].fd));
				bool is_server = false;
				for (size_t j = 0; j < _server_sockets.size(); ++j) {
					if (_poll_fds[i].fd == _server_sockets[j]) {
						LOG_DEBUG("New connection on server socket " + toString(_poll_fds[i].fd));
						handleNewConnection(_poll_fds[i].fd);
						is_server = true;
						break;
					}
				}
				
				if (!is_server) {
					LOG_DEBUG("Client data on fd " + toString(_poll_fds[i].fd));
					handleClientData(_poll_fds[i].fd, i);
				}
			}
		}
	}
}

void WebServer::handleNewConnection(int server_fd) {
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	LOG_DEBUG("Accepting connection on server_fd " + toString(server_fd));

	int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
	if (client_fd == -1) {
		LOG_ERROR("Accept failed: " + std::string(strerror(errno)));
		return;
	}

	LOG_INFO("New client connected: fd = " + toString(client_fd));
	
	if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1) {
		LOG_ERROR("fcntl failed: " + std::string(strerror(errno)));
		close(client_fd);
		return;
	}
	
	struct pollfd pfd;
	pfd.fd = client_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_poll_fds.push_back(pfd);
	
	_client_buffers[client_fd] = "";

	LOG_DEBUG("Client " + toString(client_fd) + " added to poll list");
}

void WebServer::handleClientData(int client_fd, int poll_index) {
	LOG_DEBUG("Reading data from client " + toString(client_fd));
	char buffer[8192];
	ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

	LOG_DEBUG("recv() returned " + toString(bytes_read) + " bytes");

	if (bytes_read <= 0) {
		if (bytes_read == 0) {
			LOG_INFO("Client " + toString(client_fd) + " disconnected");
		} else {
			LOG_ERROR("recv() error: " + std::string(strerror(errno)));
		}
		close(client_fd);
		_poll_fds.erase(_poll_fds.begin() + poll_index);
		_client_buffers.erase(client_fd);
		return;
	}
	
	buffer[bytes_read] = '\0';
	_client_buffers[client_fd] += buffer;
	LOG_DEBUG("Buffer for client " + toString(client_fd) + ": [" + _client_buffers[client_fd] + "]");

	if ((_client_buffers[client_fd].find("\r\n\r\n") != std::string::npos)
			|| (_client_buffers[client_fd].find("\n\n") != std::string::npos)) {
		LOG_DEBUG("Full HTTP request received from client " + toString(client_fd));
		HttpRequest request;
		if (request.parseRequest(_client_buffers[client_fd])) {
			LOG_DEBUG("Request parsed successfully");
			std::string response = generateResponse(request);
			LOG_DEBUG("Generated response for client " + toString(client_fd));
			sendResponse(client_fd, response);
			LOG_DEBUG("Response sent to client " + toString(client_fd));
		}
		else {
			LOG_ERROR("HTTP request parse error for client " + toString(client_fd));
		}
		close(client_fd);
		_poll_fds.erase(_poll_fds.begin() + poll_index);
		_client_buffers.erase(client_fd);
		LOG_INFO("Client " + toString(client_fd) + " connection closed");
	}
	else {
		LOG_DEBUG("Waiting for more data from client " + toString(client_fd) + "...");
	}
}

void WebServer::sendResponse(int client_fd, const std::string& response) {
	ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
	if (bytes_sent == -1) {
		LOG_ERROR("Failed to send response to client " + toString(client_fd) + ": " + std::string(strerror(errno)));
	} else {
		LOG_DEBUG("Sent " + toString(bytes_sent) + " bytes to client " + toString(client_fd));
	}
}

std::string WebServer::generateResponse(const HttpRequest& request) {
	LOG_DEBUG("Generating response for " + request.methodToString() + " " + request.getUri());
	
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
	LOG_INFO("Cleaning up WebServer...");
	for (size_t i = 0; i < _server_sockets.size(); ++i) {
		close(_server_sockets[i]);
		LOG_DEBUG("Closed server socket " + toString(_server_sockets[i]));
	}
	
	delete _config;
	_config = NULL;
	LOG_INFO("WebServer cleanup complete");
}