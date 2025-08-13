/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   WebServer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: christian <christian@student.42.fr>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/03 07:04:49 by christian         #+#    #+#             */
/*   Updated: 2025/08/03 07:04:52 by christian        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "WebServer.hpp"
#include "Config.hpp"
#include "HttpRequest.hpp"
#include <sstream>
#include <dirent.h>

WebServer::WebServer() {
    _config = NULL;
    _cgi_handler = new CgiHandler();
}

WebServer::~WebServer() {
    cleanup();
    delete _config;
    delete _cgi_handler;
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
	LOG_DEBUG("Buffer for client " + toString(client_fd) + " now has " + toString(_client_buffers[client_fd].length()) + " bytes");

	std::string& client_buffer = _client_buffers[client_fd];
	size_t header_end_pos = client_buffer.find("\r\n\r\n");
	if (header_end_pos == std::string::npos) {
		header_end_pos = client_buffer.find("\n\n");
		if (header_end_pos != std::string::npos) {
			header_end_pos += 2;
		}
	} else {
		header_end_pos += 4;
	}

	if (header_end_pos == std::string::npos) {
		LOG_DEBUG("Headers not complete yet, waiting for more data from client " + toString(client_fd));
		return;
	}

	LOG_DEBUG("Headers complete, checking for request body...");

	std::string headers = client_buffer.substr(0, header_end_pos);
	size_t content_length = getContentLength(headers);
	
	LOG_DEBUG("Content-Length: " + toString(content_length));

	size_t expected_total_size = header_end_pos + content_length;
	size_t current_size = client_buffer.length();

	LOG_DEBUG("Expected total size: " + toString(expected_total_size) + ", current size: " + toString(current_size));

	if (current_size >= expected_total_size) {
		LOG_DEBUG("Complete HTTP request received from client " + toString(client_fd));
		
		if (current_size > expected_total_size) {
			client_buffer = client_buffer.substr(0, expected_total_size);
			LOG_DEBUG("Trimmed buffer to exact request size");
		}

		HttpRequest request;
		if (request.parseRequest(client_buffer)) {
			LOG_DEBUG("Request parsed successfully");
			std::string response = generateResponse(request);
			LOG_DEBUG("Generated response for client " + toString(client_fd));
			sendResponse(client_fd, response);
			LOG_DEBUG("Response sent to client " + toString(client_fd));
		} else {
			LOG_ERROR("HTTP request parse error for client " + toString(client_fd));
			std::string error_response = generateErrorResponse(400, "Bad Request");
			sendResponse(client_fd, error_response);
		}
		
		close(client_fd);
		_poll_fds.erase(_poll_fds.begin() + poll_index);
		_client_buffers.erase(client_fd);
		LOG_INFO("Client " + toString(client_fd) + " connection closed");
	} else {
		LOG_DEBUG("Waiting for " + toString(expected_total_size - current_size) + " more bytes from client " + toString(client_fd));
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
    std::string method = request.methodToString();
    std::string uri = request.getUri();
    
    std::cout << "Processing: " << method << " " << uri << std::endl;
    
    // Handle unknown methods
    if (method == "UNKNOWN") {
        return generateErrorResponse(405, "Method Not Allowed");
    }
    
    // Find server config (use first server for now - can be enhanced later)
    const ServerConfig* server_config = NULL;
    if (!_config->getServers().empty()) {
        server_config = &_config->getServers()[0];
    }
    
    if (!server_config) {
        return generateErrorResponse(500, "Internal Server Error");
    }
    
    // Find best matching location from config
    const LocationConfig* location = _config->findLocationConfig(*server_config, uri);
    if (location) {
        std::cout << "Matched location: " << location->path << std::endl;
        std::cout << "Location root: " << location->root << std::endl;
    } else {
        std::cout << "No location match found, using server defaults" << std::endl;
    }
    
    // Check method restrictions
    if (!_config->isMethodAllowed(method, location)) {
        std::cout << "Method " << method << " not allowed for this location" << std::endl;
        return generateErrorResponse(405, "Method Not Allowed");
    }
    
    // Route to method-specific handlers with location context
    if (request.getMethod() == GET) {
        return handleGetRequest(request, location);
    } else if (request.getMethod() == HEAD) {
        return handleHeadRequest(request, location);
    } else if (request.getMethod() == POST) {
        return handlePostRequest(request, location);
    } else if (request.getMethod() == DELETE) {
        return handleDeleteRequest(request, location);
    } else {
        return generateErrorResponse(405, "Method Not Allowed");
    }
}

std::string WebServer::toString(size_t value) {
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

size_t WebServer::getContentLength(const std::string& headers) {
	std::string headers_lower = headers;
	for (size_t i = 0; i < headers_lower.length(); ++i) {
		headers_lower[i] = std::tolower(headers_lower[i]);
	}
	
	size_t pos = headers_lower.find("content-length:");
	if (pos == std::string::npos) {
		return 0;
	}

	pos += 15;
	while (pos < headers.length() && (headers[pos] == ' ' || headers[pos] == '\t')) {
		pos++;
	}
	
	size_t end_pos = headers.find('\r', pos);
	if (end_pos == std::string::npos) {
		end_pos = headers.find('\n', pos);
	}
	if (end_pos == std::string::npos) {
		end_pos = headers.length();
	}
	
	std::string value = headers.substr(pos, end_pos - pos);
	
	size_t content_length = 0;
	std::istringstream iss(value);
	iss >> content_length;
	
	return content_length;
}

std::string WebServer::generateErrorResponse(int status_code, const std::string& status_text) {
	std::string body = "<html><body>";
	body += "<h1>" + toString(status_code) + " " + status_text + "</h1>";
	body += "</body></html>";
	
	std::string response = "HTTP/1.1 " + toString(status_code) + " " + status_text + "\r\n";
	response += "Content-Type: text/html\r\n";
	response += "Content-Length: " + toString(body.length()) + "\r\n";
	response += "Connection: close\r\n";
	response += "\r\n";
	response += body;
	
	return response;
}

std::string WebServer::getContentType(const std::string& file_path) {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }
    
    std::string extension = file_path.substr(dot_pos);
    
    for (size_t i = 0; i < extension.length(); ++i) {
        extension[i] = std::tolower(extension[i]);
    }
    
    if (extension == ".html" || extension == ".htm") return "text/html";
    if (extension == ".css") return "text/css";
    if (extension == ".js") return "application/javascript";
    if (extension == ".json") return "application/json";
    if (extension == ".txt") return "text/plain";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".ico") return "image/x-icon";
    
    return "application/octet-stream";
}

std::string WebServer::getFilePath(const std::string& uri, const LocationConfig* location) {
    std::string root;
    
    // Use location-specific root if available, otherwise server default
    if (location && !location->root.empty()) {
        root = location->root;
        // Remove the location path from URI to get relative path
        std::string relative_path = uri;
        if (uri.find(location->path) == 0) {
            relative_path = uri.substr(location->path.length());
            if (relative_path.empty()) relative_path = "/";
        }
        return root + relative_path;
    } else {
        // Use server root (your existing logic)
        root = "./www";
        if (uri == "/") {
            return root + "/index.html";
        }
        return root + uri;
    }
}


bool WebServer::fileExists(const std::string& path) {
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

bool WebServer::isDirectory(const std::string& path) {
	struct stat buffer;
	if (stat(path.c_str(), &buffer) != 0) {
		return false;
	}
	return S_ISDIR(buffer.st_mode);
}

std::string WebServer::readFile(const std::string& file_path) {
	std::ifstream file(file_path.c_str(), std::ios::binary);
	if (!file.is_open()) {
		LOG_ERROR("Cannot open file: " + file_path);
		return "";
	}
	
	file.seekg(0, std::ios::end);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	
	std::string buffer(size, '\0');
	if (!file.read(&buffer[0], size)) {
		LOG_ERROR("Failed to read file: " + file_path);
		return "";
	}
	
	file.close();
	return buffer;
}

std::string WebServer::handleGetRequest(const HttpRequest& request, const LocationConfig* location) {
    std::string uri = request.getUri();
    std::string file_path = getFilePath(uri, location);

	std::cout << "GET request - URI: " << uri << " -> File path: " << file_path << std::endl;
    // Check for CGI request first
	if (uri == "/cgi-bin/" || uri == "/cgi-bin"){
		std::string cgi_dir = "./www/cgi-bin";
		if (isDirectory(cgi_dir))
			return handleDirectoryRequest(cgi_dir, uri, location);
	}
    if (location && !location->cgi_path.empty() && 
        uri.find(location->cgi_extension) != std::string::npos) {
        // Use location-specific CGI handling
        return _cgi_handler->handleCgiRequest(request);
    } else if (_cgi_handler && _cgi_handler->isCgiRequest(uri)) {
        return _cgi_handler->handleCgiRequest(request);
    }
    
    // std::string file_path = getFilePath(uri, location);
    
    if (!fileExists(file_path)) {
        return generateErrorResponse(404, "Not Found");
    }
    
    if (isDirectory(file_path)) {
        return handleDirectoryRequest(file_path, uri, location);
    }
    
    if (access(file_path.c_str(), R_OK) != 0) {
        return generateErrorResponse(403, "Forbidden");
    }
    
    std::string content = readFile(file_path);
    if (content.empty() && fileExists(file_path)) {
        return generateErrorResponse(500, "Internal Server Error");
    }
    
    return generateSuccessResponse(content, getContentType(file_path));
}

std::string WebServer::handleDirectoryRequest(const std::string& dir_path, const std::string& uri , const LocationConfig* location) {
    // Use location-specific index if available
    std::vector<std::string> index_files;
    if (location && !location->index.empty()) {
        index_files.push_back(location->index);
    } else {
        index_files.push_back("index.html");
        index_files.push_back("index.htm");
    }
    
    for (size_t i = 0; i < index_files.size(); ++i) {
        std::string index_path = dir_path;
        if (index_path[index_path.length() - 1] != '/') {
            index_path += "/";
        }
        index_path += index_files[i];

		std::cout << "Trying index file: " << index_path << std::endl;
        
        if (fileExists(index_path) && access(index_path.c_str(), R_OK) == 0) {
            std::string content = readFile(index_path);
            if (!content.empty()) {
				std::cout << "Found index file: " << index_path << std::endl;
                return generateSuccessResponse(content, "text/html");
            }
        }
    }
	std::cout << "No index file found in directory: " << dir_path << std::endl;

	if (location && location->autoindex)
		return generateDirectoryListing(dir_path, uri);
    return generateErrorResponse(403, "Forbidden");
}

std::string WebServer::generateDirectoryListing(const std::string& dir_path, const std::string& uri) {
    std::ostringstream html;
    html << "<html><head><title>Index of " << uri << "</title></head><body>";
    html << "<h1>Index of " << uri << "</h1><hr><pre>";
    
    DIR* dir = opendir(dir_path.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            std::string name = entry->d_name;
            if (name != ".") {
                html << "<a href=\"" << uri;
                if (uri[uri.length()-1] != '/') html << "/";
                html << name << "\">" << name << "</a>\n";
            }
        }
        closedir(dir);
    }
    
    html << "</pre><hr></body></html>";
    return generateSuccessResponse(html.str(), "text/html");
}

std::string WebServer::handleHeadRequest(const HttpRequest& request, const LocationConfig* location) {
    std::string response = handleGetRequest(request, location);
    size_t header_end = response.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        return response.substr(0, header_end + 4);
    }
    return response;
}

std::string WebServer::generateSuccessResponse(const std::string& content, const std::string& content_type) {
    std::ostringstream response;
    
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "Server: Webserv/1.0\r\n";
    response << "\r\n";
    response << content;
    
    return response.str();
}

std::string WebServer::handlePostRequest(const HttpRequest& request, const LocationConfig* location) {
    std::string uri = request.getUri();
    
    // Check for CGI request first
    if (location && !location->cgi_path.empty() && 
        uri.find(location->cgi_extension) != std::string::npos) {
        return _cgi_handler->handleCgiRequest(request);
    } else if (_cgi_handler && _cgi_handler->isCgiRequest(uri)) {
        return _cgi_handler->handleCgiRequest(request);
    }
    
    std::string body = request.getBody();
    
    std::cout << "POST request for: " << uri << std::endl;
    std::cout << "Body length: " << body.length() << std::endl;
    
    // Simple file upload handling - save POST data to a file
    if (uri.find("/upload") == 0) {
        return handleFileUpload(request);
    }
    
    // Simple form processing
    if (uri.find("/form") == 0) {
        return handleFormSubmission(request);
    }
    
    // Default: echo the received data
    return handlePostEcho(request);
}

std::string WebServer::handleDeleteRequest(const HttpRequest& request, const LocationConfig* location) {
    std::string uri = request.getUri();
    std::string file_path = getFilePath(uri, location);
    
    std::cout << "DELETE request for: " << file_path << std::endl;
    
    if (!fileExists(file_path)) {
        return generateErrorResponse(404, "Not Found");
    }
    
    if (isDirectory(file_path)) {
        return generateErrorResponse(403, "Forbidden");
    }
    
    std::string parent_dir = file_path.substr(0, file_path.find_last_of('/'));
    if (access(parent_dir.c_str(), W_OK) != 0) {
        return generateErrorResponse(403, "Forbidden");
    }
    
    if (unlink(file_path.c_str()) == 0) {

        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: text/html\r\n";
        response << "Content-Length: 47\r\n";
        response << "Connection: close\r\n";
        response << "Server: Webserv/1.0\r\n";
        response << "\r\n";
        response << "<html><body><h1>File deleted</h1></body></html>";
        return response.str();
    } else {
        return generateErrorResponse(500, "Internal Server Error");
    }
}


std::string WebServer::handleFileUpload(const HttpRequest& request) {
    std::string body = request.getBody();
    std::string content_type = request.getHeader("Content-Type");
    
    std::string upload_dir = "./www/uploads";
    mkdir(upload_dir.c_str(), 0755);
    
    std::ostringstream filename;
    filename << "upload_" << time(NULL) << ".txt";
    
    std::string file_path = upload_dir + "/" + filename.str();
    
    std::ofstream outfile(file_path.c_str());
    if (!outfile.is_open()) {
        return generateErrorResponse(500, "Internal Server Error");
    }
    
    outfile << body;
    outfile.close();

	std::string body_content = "<html><body><h1>File uploaded successfully</h1>";
    body_content += "<p>Saved as: " + filename.str() + "</p></body></html>";
    
    std::ostringstream response;
    response << "HTTP/1.1 201 Created\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Content-Length: " << body_content.length() << "\r\n";
    response << "Location: /uploads/" << filename.str() << "\r\n";
    response << "Connection: close\r\n";
    response << "Server: Webserv/1.0\r\n";
    response << "\r\n";
    response << body_content;
    
    return response.str();
}

std::string WebServer::handleFormSubmission(const HttpRequest& request) {
    std::string body = request.getBody();
    
    std::cout << "Form data received: " << body << std::endl;
    
    std::ostringstream html;
    html << "<html><body>";
    html << "<h1>Form Submission Received</h1>";
    html << "<p>Data: " << body << "</p>";
    html << "<a href='/'>Back to home</a>";
    html << "</body></html>";
    
    return generateSuccessResponse(html.str(), "text/html");
}

std::string WebServer::handlePostEcho(const HttpRequest& request) {
    std::string body = request.getBody();
    
    std::ostringstream html;
    html << "<html><body>";
    html << "<h1>POST Request Received</h1>";
    html << "<p>URI: " << request.getUri() << "</p>";
    html << "<p>Body length: " << body.length() << " bytes</p>";
    html << "<pre>" << body << "</pre>";
    html << "</body></html>";
    
    return generateSuccessResponse(html.str(), "text/html");
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