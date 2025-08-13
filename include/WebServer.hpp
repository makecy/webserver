#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

struct LocationConfig;
struct ServerConfig;

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
#include <sys/types.h>
#include "Config.hpp"
#include "utils.hpp"
#include "CgiHandler.hpp"

class Config;
class HttpRequest;
class CgiHandler;

class WebServer {
	private:
    std::vector<struct pollfd> _poll_fds;
    std::vector<int> _server_sockets;
    std::map<int, std::string> _client_buffers;
    Config* _config;
    CgiHandler* _cgi_handler;
    
    int createServerSocket(const std::string& host, int port);
    void handleNewConnection(int server_fd);
    void handleClientData(int client_fd, int poll_index);
    void sendResponse(int client_fd, const std::string& response);
    std::string generateResponse(const HttpRequest& request);
    std::string generateErrorResponse(int statusCode, const std::string& statusMessage); // new
    std::string intToString(int value); // new
    std::string getStatusMessage(int code);
    std::string toString(size_t value);
    size_t getContentLength(const std::string& headers);

    // File operations
    std::string getContentType(const std::string& file_path);
    std::string getFilePath(const std::string& uri, const LocationConfig* location = NULL);
    bool fileExists(const std::string& path);
    bool isDirectory(const std::string& path);
    std::string readFile(const std::string& file_path);

    // HTTP method handlers
    std::string handleGetRequest(const HttpRequest& request, const LocationConfig* location = NULL);
    std::string handleHeadRequest(const HttpRequest& request, const LocationConfig* location = NULL);
    std::string handlePostRequest(const HttpRequest& request, const LocationConfig* location = NULL);
    std::string handleDeleteRequest(const HttpRequest& request, const LocationConfig* location = NULL);
    std::string handleDirectoryRequest(const std::string& dir_path, const std::string& uri, const LocationConfig* location = NULL);
    std::string generateDirectoryListing(const std::string& dir_path, const std::string& uri);
    std::string generateSuccessResponse(const std::string& content, const std::string& content_type);

    // POST request handlers
    std::string handleFileUpload(const HttpRequest& request);
    std::string handleFormSubmission(const HttpRequest& request);
    std::string handlePostEcho(const HttpRequest& request);


public:
    WebServer();
    ~WebServer();
    
    bool initialize(const std::string& config_file);
    void run();
    void cleanup();
};

#endif