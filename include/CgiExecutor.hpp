#ifndef CGIEXECUTOR_HPP
#define CGIEXECUTOR_HPP

#include <string>
#include <vector>
#include <map>
#include "HttpRequest.hpp"

class CgiExecutor {
private:
    std::vector<std::string> setupEnvironment(const HttpRequest& request, const std::string& script_path) const;
    std::string getInterpreter(const std::string& script_path, const std::map<std::string, std::string>& interpreters) const;
    std::string parseCgiOutput(const std::string& raw_output) const;
    std::string generateCgiResponse(const std::string& cgi_headers, const std::string& body) const;
    std::string readFromPipe(int pipe_fd) const;
    std::string toString(size_t value) const;
    std::string generateErrorResponse(int status_code, const std::string& status_text) const;

public:
    CgiExecutor();
    ~CgiExecutor();
    
    std::string execute(const std::string& script_path, 
                       const HttpRequest& request,
                       const std::map<std::string, std::string>& interpreters) const;
};

#endif