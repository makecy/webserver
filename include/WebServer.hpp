#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <sstream>
#include <fstream>
#include <sys/stat.h>

#include "WebServer.hpp"
#include "Config.hpp"
#include "utils.hpp"

class Config;
class HttpRequest;

class WebServer {
	private:
    std::vector<struct pollfd> _poll_fds;
    std::vector<int> _server_sockets;
    std::map<int, std::string> _client_buffers;
    Config* _config;
    
    int createServerSocket(const std::string& host, int port);
    void handleNewConnection(int server_fd);
    void handleClientData(int client_fd, int poll_index);
    void sendResponse(int client_fd, const std::string& response);
    std::string generateResponse(const HttpRequest& request);
    std::string generateErrorResponse(int status_code, const std::string& status_text);
    std::string getStatusMessage(int code);
    std::string toString(size_t value);
    size_t getContentLength(const std::string& headers);

    std::string getContentType(const std::string& file_path);
    std::string getFilePath(const std::string& uri);
    bool fileExists(const std::string& path);
    bool isDirectory(const std::string& path);
    std::string readFile(const std::string& file_path);

    std::string handleGetRequest(const HttpRequest& request);
    std::string handlePostRequest(const HttpRequest& request);
    std::string handleDeleteRequest(const HttpRequest& request);
    std::string handleDirectoryRequest(const std::string& dir_path, const std::string& uri);
    std::string generateSuccessResponse(const std::string& content, const std::string& content_type);

    std::string handleHeadRequest(const HttpRequest& request);


public:
    WebServer();
    ~WebServer();
    
    bool initialize(const std::string& config_file);
    void run();
    void cleanup();
};

#endif