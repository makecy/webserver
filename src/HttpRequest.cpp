#include "HttpRequest.hpp"
#include <sstream>
#include <algorithm>

HttpRequest::HttpRequest() : _method(UNKNOWN), _is_complete(false) {
}

HttpRequest::~HttpRequest() {
}

bool HttpRequest::parseRequest(const std::string& raw_request) {
    std::istringstream stream(raw_request);
    std::string line;

    if (!std::getline(stream, line)) {
        return false;
    }
    if (!line.empty() && line[line.length() - 1] == '\r') {
        line.erase(line.length() - 1);
    }
    
    std::istringstream request_line(line);
    std::string method_str;
    
    if (!(request_line >> method_str >> _uri >> _version)) {
        return false;
    }
    if (method_str == "GET") {
        _method = GET;
    } else if (method_str == "POST") {
        _method = POST;
    } else if (method_str == "HEAD") {
        _method = HEAD;
    } else if (method_str == "DELETE") {
        _method = DELETE;
    } else {
        _method = UNKNOWN;
    }
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        if (!line.empty() && line[line.length() - 1] == '\r') {
            line.erase(line.length() - 1);
        }
        
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            while (!value.empty() && value[0] == ' ') {
                value.erase(0, 1);
            }
            
            _headers[key] = value;
        }
    }
    std::string body_line;
    while (std::getline(stream, body_line)) {
        _body += body_line + "\n";
    }
    
    _is_complete = true;
    return true;
}

std::string HttpRequest::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end()) {
        return it->second;
    }
    return "";
}

std::string HttpRequest::methodToString() const {
    switch (_method) {
        case GET: return "GET";
        case POST: return "POST";
        case DELETE: return "DELETE";
        case HEAD: return "HEAD";
        case UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}
