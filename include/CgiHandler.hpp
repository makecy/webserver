#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <vector>
#include <map>
#include "HttpRequest.hpp"

class CgiHandler {
private:
    std::string _cgi_bin_path;
    std::map<std::string, std::string> _interpreters;
    
    void initializeInterpreters();
    bool isExecutable(const std::string& path) const;
    std::string getScriptPath(const std::string& uri) const;
    std::string generateErrorResponse(int status_code, const std::string& status_text) const;

public:
    CgiHandler();
    CgiHandler(const std::string& cgi_bin_path);
    ~CgiHandler();
    
    bool isCgiRequest(const std::string& uri) const;
    std::string handleCgiRequest(const HttpRequest& request) const;
    void setCgiBinPath(const std::string& path);
};

#endif