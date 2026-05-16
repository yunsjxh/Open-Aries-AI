#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <sstream>
#include <fstream>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

namespace aries {
namespace web {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::map<std::string, std::string> headers;
    std::string body;
    std::map<std::string, std::string> params;
};

struct HttpResponse {
    int statusCode = 200;
    std::string statusText = "OK";
    std::map<std::string, std::string> headers;
    std::string body;
    
    void setJson(const std::string& json) {
        body = json;
        headers["Content-Type"] = "application/json; charset=utf-8";
    }
    
    void setHtml(const std::string& html) {
        body = html;
        headers["Content-Type"] = "text/html; charset=utf-8";
    }
    
    void setError(int code, const std::string& message) {
        statusCode = code;
        statusText = message;
        setJson("{\"success\":false,\"error\":\"" + message + "\"}");
    }
};

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

class WebServer {
public:
    WebServer(int port = 8080) : port_(port), running_(false) {}
    
    ~WebServer() {
        stop();
    }
    
    void setStaticDir(const std::string& path) {
        staticDir_ = path;
    }
    
    void get(const std::string& path, RequestHandler handler) {
        routes_["GET:" + path] = handler;
    }
    
    void post(const std::string& path, RequestHandler handler) {
        routes_["POST:" + path] = handler;
    }
    
    bool start() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }
        
        serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket_ == INVALID_SOCKET) {
            std::cerr << "socket() failed" << std::endl;
            WSACleanup();
            return false;
        }
        
        int opt = 1;
        setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port_);
        
        if (bind(serverSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "bind() failed" << std::endl;
            closesocket(serverSocket_);
            WSACleanup();
            return false;
        }
        
        if (listen(serverSocket_, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "listen() failed" << std::endl;
            closesocket(serverSocket_);
            WSACleanup();
            return false;
        }
        
        running_ = true;
        serverThread_ = std::thread(&WebServer::serverLoop, this);
        
        std::cout << "Web server started on port " << port_ << std::endl;
        return true;
    }
    
    void stop() {
        if (running_) {
            running_ = false;
            closesocket(serverSocket_);
            if (serverThread_.joinable()) {
                serverThread_.join();
            }
            WSACleanup();
        }
    }
    
    bool isRunning() const {
        return running_;
    }
    
    int getPort() const {
        return port_;
    }

private:
    int port_;
    SOCKET serverSocket_;
    std::atomic<bool> running_;
    std::thread serverThread_;
    std::string staticDir_;
    std::map<std::string, RequestHandler> routes_;
    
    void serverLoop() {
        while (running_) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(serverSocket_, &readSet);
            
            timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            
            int selectResult = select(0, &readSet, NULL, NULL, &timeout);
            if (selectResult == SOCKET_ERROR || !running_) {
                break;
            }
            
            if (selectResult == 0) {
                continue;
            }
            
            sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            SOCKET clientSocket = accept(serverSocket_, (sockaddr*)&clientAddr, &clientAddrSize);
            
            if (clientSocket == INVALID_SOCKET) {
                continue;
            }
            
            std::thread(&WebServer::handleClient, this, clientSocket).detach();
        }
    }
    
    void handleClient(SOCKET clientSocket) {
        char buffer[8192];
        int received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (received <= 0) {
            closesocket(clientSocket);
            return;
        }
        
        buffer[received] = '\0';
        
        HttpRequest request = parseRequest(buffer);
        HttpResponse response = routeRequest(request);
        
        std::string responseStr = buildResponse(response);
        send(clientSocket, responseStr.c_str(), (int)responseStr.length(), 0);
        
        closesocket(clientSocket);
    }
    
    HttpRequest parseRequest(const char* data) {
        HttpRequest request;
        std::istringstream iss(data);
        std::string line;
        
        if (std::getline(iss, line)) {
            std::istringstream lineStream(line);
            lineStream >> request.method >> request.path;
            
            size_t queryPos = request.path.find('?');
            if (queryPos != std::string::npos) {
                request.query = request.path.substr(queryPos + 1);
                request.path = request.path.substr(0, queryPos);
                parseQuery(request.query, request.params);
            }
        }
        
        while (std::getline(iss, line) && line != "\r" && line != "") {
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 1);
                while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                    value = value.substr(1);
                }
                while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
                    value.pop_back();
                }
                request.headers[key] = value;
            }
        }
        
        std::string contentLengthStr = request.headers["Content-Length"];
        if (!contentLengthStr.empty()) {
            int contentLength = std::stoi(contentLengthStr);
            std::string::size_type bodyStart = std::string(data).find("\r\n\r\n");
            if (bodyStart != std::string::npos) {
                request.body = std::string(data).substr(bodyStart + 4, contentLength);
            }
        }
        
        return request;
    }
    
    void parseQuery(const std::string& query, std::map<std::string, std::string>& params) {
        std::istringstream iss(query);
        std::string pair;
        while (std::getline(iss, pair, '&')) {
            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string key = pair.substr(0, eqPos);
                std::string value = pair.substr(eqPos + 1);
                params[urlDecode(key)] = urlDecode(value);
            }
        }
    }
    
    std::string urlDecode(const std::string& str) {
        std::string result;
        for (size_t i = 0; i < str.length(); i++) {
            if (str[i] == '%' && i + 2 < str.length()) {
                int hex;
                std::istringstream iss(str.substr(i + 1, 2));
                iss >> std::hex >> hex;
                result += (char)hex;
                i += 2;
            } else if (str[i] == '+') {
                result += ' ';
            } else {
                result += str[i];
            }
        }
        return result;
    }
    
    HttpResponse routeRequest(const HttpRequest& request) {
        std::string routeKey = request.method + ":" + request.path;
        
        auto it = routes_.find(routeKey);
        if (it != routes_.end()) {
            return it->second(request);
        }
        
        if (request.method == "GET") {
            if (request.path == "/" || request.path == "/index.html") {
                return serveFile("/index.html");
            }
            return serveFile(request.path);
        }
        
        HttpResponse response;
        response.setError(404, "Not Found");
        return response;
    }
    
    HttpResponse serveFile(const std::string& path) {
        HttpResponse response;
        
        std::string filePath = staticDir_ + path;
        
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            response.setError(404, "File Not Found");
            return response;
        }
        
        std::ostringstream oss;
        oss << file.rdbuf();
        response.body = oss.str();
        
        if (path.find(".html") != std::string::npos) {
            response.headers["Content-Type"] = "text/html; charset=utf-8";
        } else if (path.find(".css") != std::string::npos) {
            response.headers["Content-Type"] = "text/css; charset=utf-8";
        } else if (path.find(".js") != std::string::npos) {
            response.headers["Content-Type"] = "application/javascript; charset=utf-8";
        } else if (path.find(".png") != std::string::npos) {
            response.headers["Content-Type"] = "image/png";
        } else if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) {
            response.headers["Content-Type"] = "image/jpeg";
        } else {
            response.headers["Content-Type"] = "application/octet-stream";
        }
        
        return response;
    }
    
    std::string buildResponse(const HttpResponse& response) {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << response.statusCode << " " << response.statusText << "\r\n";
        
        for (const auto& header : response.headers) {
            oss << header.first << ": " << header.second << "\r\n";
        }
        
        oss << "Content-Length: " << response.body.length() << "\r\n";
        oss << "Connection: close\r\n";
        oss << "Access-Control-Allow-Origin: *\r\n";
        oss << "\r\n";
        oss << response.body;
        
        return oss.str();
    }
};

inline std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    
    while (pos < json.length() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    if (pos >= json.length()) return "";
    
    if (json[pos] == '"') {
        pos++;
        size_t endPos = pos;
        while (endPos < json.length()) {
            if (json[endPos] == '\\' && endPos + 1 < json.length()) {
                endPos += 2;
            } else if (json[endPos] == '"') {
                break;
            } else {
                endPos++;
            }
        }
        std::string value = json.substr(pos, endPos - pos);
        std::string result;
        for (size_t i = 0; i < value.length(); i++) {
            if (value[i] == '\\' && i + 1 < value.length()) {
                char next = value[i + 1];
                if (next == '"') { result += '"'; i++; }
                else if (next == '\\') { result += '\\'; i++; }
                else if (next == 'n') { result += '\n'; i++; }
                else if (next == 'r') { result += '\r'; i++; }
                else if (next == 't') { result += '\t'; i++; }
                else { result += value[i]; }
            } else {
                result += value[i];
            }
        }
        return result;
    }
    
    return "";
}

inline bool extractJsonBool(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return false;
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return false;
    
    while (pos < json.length() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    if (pos >= json.length()) return false;
    
    return (json[pos] == 't' || json[pos] == 'T');
}

inline std::string escapeJson(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

} // namespace web
} // namespace aries
