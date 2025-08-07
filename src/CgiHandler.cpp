#include "../include/CgiHandler.hpp"
#include "../include/CgiExecutor.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <sstream>

CgiHandler::CgiHandler() : _cgi_bin_path("./www/cgi-bin") {
    initializeInterpreters();
}

CgiHandler::CgiHandler(const std::string& cgi_bin_path) : _cgi_bin_path(cgi_bin_path) {
    initializeInterpreters();
}

CgiHandler::~CgiHandler() {
}

void CgiHandler::initializeInterpreters() {
    _interpreters[".py"] = "/usr/bin/python3";
    _interpreters[".pl"] = "/usr/bin/perl";
    _interpreters[".sh"] = "/bin/bash";
    _interpreters[".rb"] = "/usr/bin/ruby";
    _interpreters[".php"] = "/usr/bin/php";
    _interpreters[".cgi"] = "";
}

bool CgiHandler::isCgiRequest(const std::string& uri) const {
    // Check if URI starts with /cgi-bin/
    if (uri.find("/cgi-bin/") == 0) {
        return true;
    }
    
    // Check file extensions
    for (std::map<std::string, std::string>::const_iterator it = _interpreters.begin();
         it != _interpreters.end(); ++it) {
        if (uri.find(it->first) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

std::string CgiHandler::getScriptPath(const std::string& uri) const {
    if (uri.find("/cgi-bin/") == 0) {
        return _cgi_bin_path + uri.substr(8); // Remove "/cgi-bin" prefix
    }
    return "./www" + uri; // Default web root
}

bool CgiHandler::isExecutable(const std::string& path) const {
    return access(path.c_str(), X_OK) == 0;
}

std::string CgiHandler::generateErrorResponse(int status_code, const std::string& status_text) const {
    std::ostringstream response;
    std::ostringstream body;
    
    body << "<html><body><h1>" << status_code << " " << status_text << "</h1></body></html>";
    std::string body_str = body.str();
    
    response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Content-Length: " << body_str.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "Server: Webserv/1.0\r\n";
    response << "\r\n";
    response << body_str;
    
    return response.str();
}

std::string CgiHandler::handleCgiRequest(const HttpRequest& request) const {
    std::string uri = request.getUri();
    std::string script_path = getScriptPath(uri);
    
    std::cout << "CGI request for: " << script_path << std::endl;
    
    // Check if script exists
    struct stat buffer;
    if (stat(script_path.c_str(), &buffer) != 0) {
        return generateErrorResponse(404, "CGI Script Not Found");
    }
    
    // Check if script is executable
    if (!isExecutable(script_path)) {
        return generateErrorResponse(403, "CGI Script Not Executable");
    }
    
    // Execute the CGI script
    CgiExecutor executor;
    return executor.execute(script_path, request, _interpreters);
}

void CgiHandler::setCgiBinPath(const std::string& path) {
    _cgi_bin_path = path;
}