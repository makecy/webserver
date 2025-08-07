#include "../include/CgiExecutor.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <cstdlib>  // Add this for exit()

CgiExecutor::CgiExecutor() {
}

CgiExecutor::~CgiExecutor() {
}

std::string CgiExecutor::toString(size_t value) const {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

std::string CgiExecutor::generateErrorResponse(int status_code, const std::string& status_text) const {
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

std::string CgiExecutor::execute(const std::string& script_path, 
                                const HttpRequest& request,
                                const std::map<std::string, std::string>& interpreters) const {
    
    int pipe_stdout[2];
    int pipe_stdin[2];
    
    if (pipe(pipe_stdout) == -1) {
        return generateErrorResponse(500, "Internal Server Error - Pipe Creation Failed");
    }
    
    if (pipe(pipe_stdin) == -1) {
        close(pipe_stdout[0]);
        close(pipe_stdout[1]);
        return generateErrorResponse(500, "Internal Server Error - Stdin Pipe Creation Failed");
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_stdout[0]);
        close(pipe_stdout[1]);
        close(pipe_stdin[0]);
        close(pipe_stdin[1]);
        return generateErrorResponse(500, "Internal Server Error - Fork Failed");
    }
    
    if (pid == 0) {
        // Child process
        close(pipe_stdout[0]); // Close read end of stdout pipe
        close(pipe_stdin[1]);  // Close write end of stdin pipe
        
        // Redirect stdout to pipe
        dup2(pipe_stdout[1], STDOUT_FILENO);
        close(pipe_stdout[1]);
        
        // Redirect stdin from pipe
        dup2(pipe_stdin[0], STDIN_FILENO);
        close(pipe_stdin[0]);
        
        // Setup environment variables
        std::vector<std::string> env_vars = setupEnvironment(request, script_path);
        
        // Convert to char* array
        std::vector<char*> env_ptrs;
        for (size_t i = 0; i < env_vars.size(); ++i) {
            env_ptrs.push_back(const_cast<char*>(env_vars[i].c_str()));
        }
        env_ptrs.push_back(NULL);
        
        // Get interpreter and execute
        std::string interpreter = getInterpreter(script_path, interpreters);
        if (!interpreter.empty()) {
            execle(interpreter.c_str(), interpreter.c_str(), script_path.c_str(), NULL, &env_ptrs[0]);
        } else {
            execle(script_path.c_str(), script_path.c_str(), NULL, &env_ptrs[0]);
        }
        
        // If we reach here, exec failed
        exit(1);
    }
    
    // Parent process (moved outside the if/else to fix control flow)
    close(pipe_stdout[1]); // Close write end of stdout pipe
    close(pipe_stdin[0]);  // Close read end of stdin pipe
    
    // Write POST data to script's stdin if needed
    if (request.getMethod() == POST && !request.getBody().empty()) {
        const std::string& body = request.getBody();
        write(pipe_stdin[1], body.c_str(), body.length());
    }
    close(pipe_stdin[1]); // Close stdin pipe
    
    // Read script output
    std::string output = readFromPipe(pipe_stdout[0]);
    close(pipe_stdout[0]);
    
    // Wait for child process
    int status;
    waitpid(pid, &status, 0);
    
    if (WEXITSTATUS(status) != 0) {
        return generateErrorResponse(500, "CGI Script Execution Error");
    }
    
    // Parse and return CGI output
    return parseCgiOutput(output);
}

std::vector<std::string> CgiExecutor::setupEnvironment(const HttpRequest& request, const std::string& script_path) const {
    std::vector<std::string> env_vars;
    
    env_vars.push_back("REQUEST_METHOD=" + request.methodToString());
    env_vars.push_back("REQUEST_URI=" + request.getUri());
    env_vars.push_back("SERVER_NAME=localhost");
    env_vars.push_back("SERVER_PORT=8080");
    env_vars.push_back("SCRIPT_NAME=" + script_path);
    env_vars.push_back("SERVER_SOFTWARE=Webserv/1.0");
    env_vars.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env_vars.push_back("REDIRECT_STATUS=200");
    
    if (request.getMethod() == POST) {
        env_vars.push_back("CONTENT_LENGTH=" + toString(request.getBody().length()));
        std::string content_type = request.getHeader("Content-Type");
        if (!content_type.empty()) {
            env_vars.push_back("CONTENT_TYPE=" + content_type);
        } else {
            env_vars.push_back("CONTENT_TYPE=application/x-www-form-urlencoded");
        }
    }
    
    // Add query string if present
    std::string uri = request.getUri();
    size_t query_pos = uri.find('?');
    if (query_pos != std::string::npos) {
        env_vars.push_back("QUERY_STRING=" + uri.substr(query_pos + 1));
    } else {
        env_vars.push_back("QUERY_STRING=");
    }
    
    return env_vars;
}

std::string CgiExecutor::getInterpreter(const std::string& script_path, const std::map<std::string, std::string>& interpreters) const {
    for (std::map<std::string, std::string>::const_iterator it = interpreters.begin();
         it != interpreters.end(); ++it) {
        if (script_path.find(it->first) != std::string::npos) {
            return it->second;
        }
    }
    return "";
}

std::string CgiExecutor::readFromPipe(int pipe_fd) const {
    std::string output;
    char buffer[1024];
    ssize_t bytes_read;
    
    while ((bytes_read = read(pipe_fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        output += buffer;
    }
    
    return output;
}

std::string CgiExecutor::parseCgiOutput(const std::string& raw_output) const {
    // Find the separation between headers and body
    size_t header_end = raw_output.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = raw_output.find("\n\n");
        if (header_end == std::string::npos) {
            // No headers, treat everything as body
            return generateCgiResponse("", raw_output);
        }
        header_end += 2; // For \n\n
    } else {
        header_end += 4; // For \r\n\r\n
    }
    
    std::string headers = raw_output.substr(0, header_end - (header_end > 2 ? (raw_output[header_end - 3] == '\r' ? 4 : 2) : 0));
    std::string body = raw_output.substr(header_end);
    
    return generateCgiResponse(headers, body);
}

std::string CgiExecutor::generateCgiResponse(const std::string& cgi_headers, const std::string& body) const {
    std::ostringstream response;
    
    response << "HTTP/1.1 200 OK\r\n";
    
    // Parse CGI headers
    if (!cgi_headers.empty()) {
        // Add CGI headers, ensuring proper line endings
        std::string headers = cgi_headers;
        if (headers[headers.length() - 1] != '\n') {
            headers += "\r\n";
        }
        response << headers;
    }
    
    // Add default headers if not present
    if (cgi_headers.find("Content-Type:") == std::string::npos && 
        cgi_headers.find("content-type:") == std::string::npos) {
        response << "Content-Type: text/html\r\n";
    }
    
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "Server: Webserv/1.0\r\n";
    response << "\r\n";
    response << body;
    
    return response.str();
}