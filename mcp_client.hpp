#pragma once

#include "mcp_protocol.hpp"
#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iostream>

#pragma comment(lib, "wininet.lib")

namespace aries {
namespace mcp {

// HTTP MCP 客户端
class MCPHttpClient {
public:
    MCPHttpClient() : requestId_(0), initialized_(false) {}
    
    ~MCPHttpClient() {
        disconnect();
    }
    
    // 连接到 HTTP MCP 服务器
    bool connect(const std::string& url) {
        baseUrl_ = url;
        
        // 初始化 MCP 协议
        if (!initialize()) {
            return false;
        }
        
        return true;
    }
    
    // 断开连接
    void disconnect() {
        initialized_ = false;
        tools_.clear();
    }
    
    // 是否已连接
    bool isConnected() const {
        return initialized_;
    }
    
    // 获取可用工具列表
    const std::vector<Tool>& getTools() const {
        return tools_;
    }
    
    // 获取服务器信息
    const ServerInfo& getServerInfo() const {
        return serverInfo_;
    }
    
    // 获取服务器能力
    const ServerCapabilities& getCapabilities() const {
        return capabilities_;
    }
    
    // 调用工具
    std::pair<bool, std::string> callTool(const std::string& toolName, const std::string& args = "{}") {
        if (!isConnected()) {
            return {false, "Not connected to MCP server"};
        }
        
        std::string params = buildToolCallParams(toolName, args);
        std::string request = buildRequest("tools/call", params, ++requestId_);
        
        std::string response = sendHttpRequest(request);
        if (response.empty()) {
            return {false, "HTTP request failed: " + lastError_};
        }
        
        if (hasError(response)) {
            std::string errorMsg = extractError(response);
            return {false, "MCP error: " + errorMsg};
        }
        
        std::string content = extractToolResultContent(response);
        return {true, content};
    }
    
    // 获取最后的错误信息
    std::string getLastError() const {
        return lastError_;
    }
    
    // 获取工具描述
    std::string getToolsDescription() const {
        std::ostringstream oss;
        oss << "MCP 工具列表 (来自 " << serverInfo_.name << "):\n\n";
        
        for (const auto& tool : tools_) {
            oss << "工具: " << tool.name << "\n";
            oss << "描述: " << tool.description << "\n";
            if (!tool.rawSchema.empty() && tool.rawSchema != "{}") {
                oss << "参数 Schema: " << tool.rawSchema << "\n";
            }
            oss << "\n";
        }
        
        return oss.str();
    }

private:
    std::string baseUrl_;
    std::string sessionId_;
    int requestId_;
    bool initialized_;
    
    std::vector<Tool> tools_;
    ServerInfo serverInfo_;
    ServerCapabilities capabilities_;
    std::string lastError_;
    
    // 解析 SSE 响应
    std::string parseSSEResponse(const std::string& response) {
        // 检查是否是 SSE 格式
        if (response.find("data:") == std::string::npos) {
            // 不是 SSE，直接返回原始响应
            return response;
        }
        
        // 解析 SSE 格式，提取 data 内容
        std::string result;
        std::istringstream iss(response);
        std::string line;
        
        while (std::getline(iss, line)) {
            // 移除 \r
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            if (line.find("data:") == 0) {
                std::string data = line.substr(5);
                // 去除前导空格
                size_t start = data.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    data = data.substr(start);
                }
                result += data;
            }
        }
        
        return result;
    }
    
    // 初始化 MCP 协议
    bool initialize() {
        std::string initParams = "{\"protocolVersion\":\"2024-11-05\","
            "\"capabilities\":{\"tools\":{}},"
            "\"clientInfo\":{\"name\":\"Open-Aries-AI\",\"version\":\"1.3.0.1\"}}";
        
        std::string request = buildRequest("initialize", initParams, ++requestId_);
        std::string response = sendHttpRequest(request);
        
        if (response.empty()) {
            lastError_ = "Failed to initialize: " + lastError_;
            return false;
        }
        
        if (hasError(response)) {
            lastError_ = "Initialize error: " + extractError(response);
            return false;
        }
        
        // 解析初始化结果
        InitializeResult initResult = parseInitializeResult(response);
        serverInfo_ = initResult.serverInfo;
        capabilities_ = initResult.capabilities;
        
        // 发送 initialized 通知
        std::string notification = buildNotification("notifications/initialized", "");
        sendHttpRequest(notification);
        
        // 如果服务器支持工具，获取工具列表
        if (capabilities_.tools) {
            if (!listTools()) {
                return false;
            }
        }
        
        initialized_ = true;
        return true;
    }
    
    // 获取工具列表
    bool listTools() {
        std::string request = buildRequest("tools/list", "{}", ++requestId_);
        std::string response = sendHttpRequest(request);
        
        if (response.empty()) {
            lastError_ = "Failed to list tools: " + lastError_;
            return false;
        }
        
        if (hasError(response)) {
            lastError_ = "List tools error: " + extractError(response);
            return false;
        }
        
        tools_ = parseTools(response);
        return true;
    }
    
    // 发送 HTTP 请求
    std::string sendHttpRequest(const std::string& jsonBody) {
        // 解析 URL
        std::string hostName, urlPath;
        int port = 443;
        bool isHttps = true;
        
        std::string url = baseUrl_;
        if (url.find("https://") == 0) {
            url = url.substr(8);
            isHttps = true;
            port = 443;
        } else if (url.find("http://") == 0) {
            url = url.substr(7);
            isHttps = false;
            port = 80;
        }
        
        size_t slashPos = url.find('/');
        if (slashPos != std::string::npos) {
            hostName = url.substr(0, slashPos);
            urlPath = url.substr(slashPos);
        } else {
            hostName = url;
            urlPath = "/mcp";
        }
        
        // 初始化 WinINet
        HINTERNET hInternet = InternetOpenA("AriesAI-MCP/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) {
            lastError_ = "Failed to initialize WinINet";
            return "";
        }
        
        // 连接服务器
        HINTERNET hConnect = InternetConnectA(hInternet, hostName.c_str(), 
            (INTERNET_PORT)port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) {
            DWORD error = GetLastError();
            std::ostringstream oss;
            oss << "Failed to connect to " << hostName << " (Error " << error << ")";
            lastError_ = oss.str();
            InternetCloseHandle(hInternet);
            return "";
        }
        
        // 创建请求
        DWORD flags = 0;
        if (isHttps) {
            flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | 
                    INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
        }
        
        HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", urlPath.c_str(),
            NULL, NULL, NULL, flags | INTERNET_FLAG_NO_CACHE_WRITE, 0);
        if (!hRequest) {
            lastError_ = "Failed to create HTTP request";
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return "";
        }
        
        // 设置超时
        DWORD timeout = 60000;  // 60 秒
        InternetSetOptionA(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hRequest, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
        
        // 添加请求头
        std::string contentType = "Content-Type: application/json";
        HttpAddRequestHeadersA(hRequest, contentType.c_str(), (DWORD)contentType.length(), HTTP_ADDREQ_FLAG_ADD);
        
        // 添加 Accept 头支持 SSE (streamable HTTP MCP)
        std::string acceptHeader = "Accept: application/json, text/event-stream";
        HttpAddRequestHeadersA(hRequest, acceptHeader.c_str(), (DWORD)acceptHeader.length(), HTTP_ADDREQ_FLAG_ADD);
        
        // 如果有会话 ID，添加到请求头
        if (!sessionId_.empty()) {
            std::string sessionHeader = "mcp-session-id: " + sessionId_;
            HttpAddRequestHeadersA(hRequest, sessionHeader.c_str(), (DWORD)sessionHeader.length(), HTTP_ADDREQ_FLAG_ADD);
        }
        
        // 发送请求
        if (!HttpSendRequestA(hRequest, NULL, 0, (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.length())) {
            DWORD error = GetLastError();
            std::ostringstream oss;
            oss << "Failed to send request (Error " << error << ")";
            lastError_ = oss.str();
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return "";
        }
        
        // 获取响应头中的 mcp-session-id
        char sessionIdBuf[256] = "mcp-session-id";
        DWORD sessionIdLen = sizeof(sessionIdBuf);
        if (HttpQueryInfoA(hRequest, HTTP_QUERY_CUSTOM, (LPVOID)sessionIdBuf, &sessionIdLen, NULL)) {
            sessionId_ = sessionIdBuf;
        }
        
        // 读取响应
        std::string response;
        char buffer[4096];
        DWORD bytesRead;
        
        while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            response += buffer;
        }
        
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        
        // 处理 SSE 响应
        return parseSSEResponse(response);
    }
};

// MCP 客户端（stdio 传输）
class MCPClient {
public:
    MCPClient() : hProcess_(nullptr), hReadPipe_(nullptr), hWritePipe_(nullptr), 
                  requestId_(0), initialized_(false) {}
    
    ~MCPClient() {
        disconnect();
    }
    
    // 连接到 MCP 服务器（启动子进程）
    bool connect(const std::string& command, const std::string& args = "") {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;
        
        HANDLE hChildStdinRead, hChildStdinWrite;
        HANDLE hChildStdoutRead, hChildStdoutWrite;
        
        // 创建管道用于子进程的 stdin
        if (!CreatePipe(&hChildStdinRead, &hChildStdinWrite, &sa, 0)) {
            lastError_ = "Failed to create stdin pipe";
            return false;
        }
        SetHandleInformation(hChildStdinWrite, HANDLE_FLAG_INHERIT, 0);
        
        // 创建管道用于子进程的 stdout
        if (!CreatePipe(&hChildStdoutRead, &hChildStdoutWrite, &sa, 0)) {
            CloseHandle(hChildStdinRead);
            CloseHandle(hChildStdinWrite);
            lastError_ = "Failed to create stdout pipe";
            return false;
        }
        SetHandleInformation(hChildStdoutRead, HANDLE_FLAG_INHERIT, 0);
        
        // 设置启动信息
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdError = hChildStdoutWrite;
        si.hStdOutput = hChildStdoutWrite;
        si.hStdInput = hChildStdinRead;
        si.dwFlags |= STARTF_USESTDHANDLES;
        ZeroMemory(&pi, sizeof(pi));
        
        // 构建命令行
        std::string cmdLine = command;
        if (!args.empty()) {
            cmdLine += " " + args;
        }
        
        // 创建可修改的命令行字符串
        char* cmdLineStr = new char[cmdLine.length() + 1];
        strcpy(cmdLineStr, cmdLine.c_str());
        
        // 创建子进程
        BOOL success = CreateProcessA(
            NULL,
            cmdLineStr,
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi
        );
        
        delete[] cmdLineStr;
        
        CloseHandle(hChildStdinRead);
        CloseHandle(hChildStdoutWrite);
        
        if (!success) {
            DWORD error = GetLastError();
            std::ostringstream oss;
            oss << "Failed to create process (Error " << error << "): " << command;
            lastError_ = oss.str();
            CloseHandle(hChildStdinWrite);
            CloseHandle(hChildStdoutRead);
            return false;
        }
        
        hProcess_ = pi.hProcess;
        hReadPipe_ = hChildStdoutRead;
        hWritePipe_ = hChildStdinWrite;
        
        CloseHandle(pi.hThread);
        
        // 初始化 MCP 协议
        if (!initialize()) {
            disconnect();
            return false;
        }
        
        return true;
    }
    
    // 断开连接
    void disconnect() {
        if (hProcess_) {
            // 发送关闭通知
            if (initialized_) {
                std::string notification = buildNotification("shutdown", "{}");
                sendMessage(notification);
            }
            
            TerminateProcess(hProcess_, 0);
            CloseHandle(hProcess_);
            hProcess_ = nullptr;
        }
        
        if (hReadPipe_) {
            CloseHandle(hReadPipe_);
            hReadPipe_ = nullptr;
        }
        
        if (hWritePipe_) {
            CloseHandle(hWritePipe_);
            hWritePipe_ = nullptr;
        }
        
        initialized_ = false;
        tools_.clear();
    }
    
    // 是否已连接
    bool isConnected() const {
        return hProcess_ != nullptr && initialized_;
    }
    
    // 获取可用工具列表
    const std::vector<Tool>& getTools() const {
        return tools_;
    }
    
    // 获取服务器信息
    const ServerInfo& getServerInfo() const {
        return serverInfo_;
    }
    
    // 获取服务器能力
    const ServerCapabilities& getCapabilities() const {
        return capabilities_;
    }
    
    // 调用工具
    std::pair<bool, std::string> callTool(const std::string& toolName, const std::string& args = "{}") {
        if (!isConnected()) {
            return {false, "Not connected to MCP server"};
        }
        
        std::string params = buildToolCallParams(toolName, args);
        std::string request = buildRequest("tools/call", params, ++requestId_);
        
        std::string response = sendRequest(request);
        if (response.empty()) {
            return {false, lastError_};
        }
        
        if (hasError(response)) {
            return {false, extractError(response)};
        }
        
        std::string content = extractToolResultContent(response);
        return {true, content};
    }
    
    // 获取最后的错误信息
    std::string getLastError() const {
        return lastError_;
    }
    
    // 获取工具描述（用于 AI 提示词）
    std::string getToolsDescription() const {
        std::ostringstream oss;
        oss << "MCP 工具列表 (来自 " << serverInfo_.name << "):\n\n";
        
        for (const auto& tool : tools_) {
            oss << "工具: " << tool.name << "\n";
            oss << "描述: " << tool.description << "\n";
            if (!tool.rawSchema.empty() && tool.rawSchema != "{}") {
                oss << "参数 Schema: " << tool.rawSchema << "\n";
            }
            oss << "\n";
        }
        
        return oss.str();
    }

private:
    HANDLE hProcess_;
    HANDLE hReadPipe_;
    HANDLE hWritePipe_;
    int requestId_;
    bool initialized_;
    
    std::vector<Tool> tools_;
    ServerInfo serverInfo_;
    ServerCapabilities capabilities_;
    std::string lastError_;
    
    // 初始化 MCP 协议
    bool initialize() {
        std::string initParams = "{\"protocolVersion\":\"2024-11-05\","
            "\"capabilities\":{\"tools\":{}},"
            "\"clientInfo\":{\"name\":\"Open-Aries-AI\",\"version\":\"1.3.0.1\"}}";
        
        std::string request = buildRequest("initialize", initParams, ++requestId_);
        std::string response = sendRequest(request);
        
        if (response.empty()) {
            lastError_ = "Failed to initialize: " + lastError_;
            return false;
        }
        
        if (hasError(response)) {
            lastError_ = "Initialize error: " + extractError(response);
            return false;
        }
        
        // 解析初始化结果
        InitializeResult initResult = parseInitializeResult(response);
        serverInfo_ = initResult.serverInfo;
        capabilities_ = initResult.capabilities;
        
        // 发送 initialized 通知
        std::string notification = buildNotification("notifications/initialized", "{}");
        sendMessage(notification);
        
        // 如果服务器支持工具，获取工具列表
        if (capabilities_.tools) {
            if (!listTools()) {
                return false;
            }
        }
        
        initialized_ = true;
        return true;
    }
    
    // 获取工具列表
    bool listTools() {
        std::string request = buildRequest("tools/list", "{}", ++requestId_);
        std::string response = sendRequest(request);
        
        if (response.empty()) {
            lastError_ = "Failed to list tools: " + lastError_;
            return false;
        }
        
        if (hasError(response)) {
            lastError_ = "List tools error: " + extractError(response);
            return false;
        }
        
        tools_ = parseTools(response);
        return true;
    }
    
    // 发送消息
    bool sendMessage(const std::string& message) {
        if (!hWritePipe_) return false;
        
        std::string msg = message + "\n";
        DWORD written;
        return WriteFile(hWritePipe_, msg.c_str(), (DWORD)msg.length(), &written, NULL) != 0;
    }
    
    // 接收消息
    std::string receiveMessage() {
        if (!hReadPipe_) return "";
        
        std::string result;
        char buffer[4096];
        DWORD bytesRead;
        
        // 设置超时
        COMMTIMEOUTS timeouts;
        timeouts.ReadIntervalTimeout = 5000;
        timeouts.ReadTotalTimeoutMultiplier = 5000;
        timeouts.ReadTotalTimeoutConstant = 5000;
        timeouts.WriteTotalTimeoutMultiplier = 5000;
        timeouts.WriteTotalTimeoutConstant = 5000;
        SetCommTimeouts(hReadPipe_, &timeouts);
        
        // 读取直到遇到换行符
        while (true) {
            char c;
            if (!ReadFile(hReadPipe_, &c, 1, &bytesRead, NULL) || bytesRead == 0) {
                break;
            }
            
            if (c == '\n') {
                break;
            }
            
            if (c != '\r') {
                result += c;
            }
            
            // 防止无限读取
            if (result.length() > 1024 * 1024) {
                break;
            }
        }
        
        return result;
    }
    
    // 发送请求并等待响应
    std::string sendRequest(const std::string& request) {
        if (!sendMessage(request)) {
            lastError_ = "Failed to send request";
            return "";
        }
        
        std::string response = receiveMessage();
        if (response.empty()) {
            lastError_ = "No response received";
            return "";
        }
        
        return response;
    }
};

// MCP 客户端管理器（支持多个 MCP 服务器）
class MCPClientManager {
public:
    static MCPClientManager& getInstance() {
        static MCPClientManager instance;
        return instance;
    }
    
    // 连接到 MCP 服务器 (stdio)
    bool connectServer(const std::string& name, const std::string& command, const std::string& args = "") {
        auto client = std::make_shared<MCPClient>();
        if (!client->connect(command, args)) {
            lastError_ = client->getLastError();
            return false;
        }
        
        clients_[name] = client;
        return true;
    }
    
    // 连接到 HTTP MCP 服务器
    bool connectHttpServer(const std::string& name, const std::string& url) {
        auto client = std::make_shared<MCPHttpClient>();
        if (!client->connect(url)) {
            lastError_ = client->getLastError();
            return false;
        }
        
        httpClients_[name] = client;
        return true;
    }
    
    // 断开指定服务器
    void disconnectServer(const std::string& name) {
        {
            auto it = clients_.find(name);
            if (it != clients_.end()) {
                it->second->disconnect();
                clients_.erase(it);
                return;
            }
        }
        {
            auto it = httpClients_.find(name);
            if (it != httpClients_.end()) {
                it->second->disconnect();
                httpClients_.erase(it);
            }
        }
    }
    
    // 断开所有服务器
    void disconnectAll() {
        for (auto& pair : clients_) {
            pair.second->disconnect();
        }
        clients_.clear();
        
        for (auto& pair : httpClients_) {
            pair.second->disconnect();
        }
        httpClients_.clear();
    }
    
    // 获取 stdio 客户端
    std::shared_ptr<MCPClient> getClient(const std::string& name) {
        auto it = clients_.find(name);
        if (it != clients_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // 获取 HTTP 客户端
    std::shared_ptr<MCPHttpClient> getHttpClient(const std::string& name) {
        auto it = httpClients_.find(name);
        if (it != httpClients_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // 检查服务器是否存在
    bool hasServer(const std::string& name) const {
        return clients_.find(name) != clients_.end() || 
               httpClients_.find(name) != httpClients_.end();
    }
    
    // 获取所有客户端名称
    std::vector<std::string> getClientNames() const {
        std::vector<std::string> names;
        for (const auto& pair : clients_) {
            names.push_back(pair.first);
        }
        for (const auto& pair : httpClients_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    // 获取所有工具（合并所有服务器的工具）
    std::vector<std::pair<std::string, Tool>> getAllTools() const {
        std::vector<std::pair<std::string, Tool>> allTools;
        for (const auto& pair : clients_) {
            for (const auto& tool : pair.second->getTools()) {
                allTools.push_back({pair.first, tool});
            }
        }
        for (const auto& pair : httpClients_) {
            for (const auto& tool : pair.second->getTools()) {
                allTools.push_back({pair.first, tool});
            }
        }
        return allTools;
    }
    
    // 调用工具
    std::pair<bool, std::string> callTool(const std::string& serverName, 
                                           const std::string& toolName, 
                                           const std::string& args = "{}") {
        // 先尝试 stdio 客户端
        auto client = getClient(serverName);
        if (client) {
            return client->callTool(toolName, args);
        }
        
        // 再尝试 HTTP 客户端
        auto httpClient = getHttpClient(serverName);
        if (httpClient) {
            return httpClient->callTool(toolName, args);
        }
        
        return {false, "Server not found: " + serverName};
    }
    
    // 获取所有工具的描述（用于 AI 提示词）
    std::string getAllToolsDescription() const {
        std::ostringstream oss;
        oss << "=== MCP 工具列表 ===\n\n";
        
        for (const auto& pair : clients_) {
            oss << "【服务器名称: " << pair.first << "】\n";
            oss << "调用时使用 server=\"" << pair.first << "\"\n";
            oss << pair.second->getToolsDescription() << "\n";
        }
        
        for (const auto& pair : httpClients_) {
            oss << "【服务器名称: " << pair.first << "】(HTTP)\n";
            oss << "调用时使用 server=\"" << pair.first << "\"\n";
            oss << pair.second->getToolsDescription() << "\n";
        }
        
        return oss.str();
    }
    
    // 获取服务器信息
    std::string getServerInfo(const std::string& name) const {
        {
            auto it = clients_.find(name);
            if (it != clients_.end()) {
                const auto& info = it->second->getServerInfo();
                return info.name + " v" + info.version + " (stdio)";
            }
        }
        {
            auto it = httpClients_.find(name);
            if (it != httpClients_.end()) {
                const auto& info = it->second->getServerInfo();
                return info.name + " v" + info.version + " (HTTP)";
            }
        }
        return "Unknown";
    }
    
    // 获取指定服务器的工具列表
    std::vector<Tool> getServerTools(const std::string& name) const {
        {
            auto it = clients_.find(name);
            if (it != clients_.end()) {
                return it->second->getTools();
            }
        }
        {
            auto it = httpClients_.find(name);
            if (it != httpClients_.end()) {
                return it->second->getTools();
            }
        }
        return {};
    }
    
    // 获取最后的错误
    std::string getLastError() const {
        return lastError_;
    }

private:
    MCPClientManager() = default;
    ~MCPClientManager() {
        disconnectAll();
    }
    
    std::map<std::string, std::shared_ptr<MCPClient>> clients_;
    std::map<std::string, std::shared_ptr<MCPHttpClient>> httpClients_;
    std::string lastError_;
};

} // namespace mcp
} // namespace aries
