#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iostream>

namespace aries {
namespace mcp {

// ==================== MCP 协议类型 ====================

// 工具定义
struct ToolInputSchema {
    std::string type = "object";
    std::map<std::string, std::string> properties;  // name -> type
    std::vector<std::string> required;
};

struct Tool {
    std::string name;
    std::string description;
    ToolInputSchema inputSchema;
    std::string rawSchema;  // 原始 JSON schema
};

// 工具调用结果
struct ToolResult {
    std::string content;
    bool isError = false;
};

// 资源定义
struct Resource {
    std::string uri;
    std::string name;
    std::string description;
    std::string mimeType;
};

// 提示词定义
struct PromptArgument {
    std::string name;
    std::string description;
    bool required = false;
};

struct Prompt {
    std::string name;
    std::string description;
    std::vector<PromptArgument> arguments;
};

// 服务器信息
struct ServerInfo {
    std::string name;
    std::string version;
};

struct ServerCapabilities {
    bool tools = false;
    bool resources = false;
    bool prompts = false;
    bool logging = false;
};

struct InitializeResult {
    std::string protocolVersion;
    ServerCapabilities capabilities;
    ServerInfo serverInfo;
};

// ==================== 简单 JSON 辅助函数 ====================

inline std::string escapeJson(const std::string& s) {
    std::string result;
    for (char c : s) {
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

inline std::string extractString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    
    // 跳过空白
    while (pos < json.length() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    if (pos >= json.length() || json[pos] != '"') return "";
    pos++;
    
    std::string result;
    while (pos < json.length() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.length()) {
            pos++;
            switch (json[pos]) {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        pos++;
    }
    
    return result;
}

inline bool extractBool(const std::string& json, const std::string& key, bool defaultVal = false) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return defaultVal;
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return defaultVal;
    
    // 跳过空白
    while (pos < json.length() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    if (pos >= json.length()) return defaultVal;
    
    // 如果是对象或数组，认为存在该能力
    if (json[pos] == '{' || json[pos] == '[') return true;
    
    if (json[pos] == 't' || json[pos] == 'T') return true;
    if (json[pos] == 'f' || json[pos] == 'F') return false;
    
    return defaultVal;
}

inline std::string extractObject(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "{}";
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "{}";
    
    // 跳过空白
    while (pos < json.length() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    if (pos >= json.length() || json[pos] != '{') return "{}";
    
    int depth = 1;
    size_t start = pos;
    pos++;
    
    while (pos < json.length() && depth > 0) {
        if (json[pos] == '{') depth++;
        else if (json[pos] == '}') depth--;
        else if (json[pos] == '"') {
            pos++;
            while (pos < json.length() && json[pos] != '"') {
                if (json[pos] == '\\' && pos + 1 < json.length()) pos++;
                pos++;
            }
        }
        pos++;
    }
    
    return json.substr(start, pos - start);
}

inline std::string extractArray(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "[]";
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "[]";
    
    // 跳过空白
    while (pos < json.length() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    if (pos >= json.length() || json[pos] != '[') return "[]";
    
    int depth = 1;
    size_t start = pos;
    pos++;
    
    while (pos < json.length() && depth > 0) {
        if (json[pos] == '[') depth++;
        else if (json[pos] == ']') depth--;
        else if (json[pos] == '{') {
            int objDepth = 1;
            pos++;
            while (pos < json.length() && objDepth > 0) {
                if (json[pos] == '{') objDepth++;
                else if (json[pos] == '}') objDepth--;
                else if (json[pos] == '"') {
                    pos++;
                    while (pos < json.length() && json[pos] != '"') {
                        if (json[pos] == '\\' && pos + 1 < json.length()) pos++;
                        pos++;
                    }
                }
                pos++;
            }
            continue;
        }
        else if (json[pos] == '"') {
            pos++;
            while (pos < json.length() && json[pos] != '"') {
                if (json[pos] == '\\' && pos + 1 < json.length()) pos++;
                pos++;
            }
        }
        pos++;
    }
    
    return json.substr(start, pos - start);
}

// 解析工具列表
inline std::vector<Tool> parseTools(const std::string& json) {
    std::vector<Tool> tools;
    
    std::string toolsArray = extractArray(json, "tools");
    if (toolsArray == "[]") return tools;
    
    // 简单解析每个工具对象
    size_t pos = 0;
    while ((pos = toolsArray.find("{", pos)) != std::string::npos) {
        int depth = 1;
        size_t start = pos;
        pos++;
        
        while (pos < toolsArray.length() && depth > 0) {
            if (toolsArray[pos] == '{') depth++;
            else if (toolsArray[pos] == '}') depth--;
            else if (toolsArray[pos] == '"') {
                pos++;
                while (pos < toolsArray.length() && toolsArray[pos] != '"') {
                    if (toolsArray[pos] == '\\' && pos + 1 < toolsArray.length()) pos++;
                    pos++;
                }
            }
            pos++;
        }
        
        std::string toolJson = toolsArray.substr(start, pos - start);
        Tool tool;
        tool.name = extractString(toolJson, "name");
        tool.description = extractString(toolJson, "description");
        tool.rawSchema = extractObject(toolJson, "inputSchema");
        tools.push_back(tool);
    }
    
    return tools;
}

// 解析初始化结果
inline InitializeResult parseInitializeResult(const std::string& json) {
    InitializeResult result;
    result.protocolVersion = extractString(json, "protocolVersion");
    
    std::string caps = extractObject(json, "capabilities");
    if (!caps.empty()) {
        result.capabilities.tools = extractBool(caps, "tools");
        result.capabilities.resources = extractBool(caps, "resources");
        result.capabilities.prompts = extractBool(caps, "prompts");
        result.capabilities.logging = extractBool(caps, "logging");
    }
    
    std::string info = extractObject(json, "serverInfo");
    if (!info.empty()) {
        result.serverInfo.name = extractString(info, "name");
        result.serverInfo.version = extractString(info, "version");
    }
    
    return result;
}

// 构建 JSON-RPC 请求
inline std::string buildRequest(const std::string& method, const std::string& params, int id) {
    std::ostringstream oss;
    oss << "{\"jsonrpc\":\"2.0\",\"method\":\"" << method << "\"";
    if (!params.empty() && params != "{}") {
        oss << ",\"params\":" << params;
    }
    oss << ",\"id\":" << id << "}";
    return oss.str();
}

// 构建 JSON-RPC 通知（无 id）
inline std::string buildNotification(const std::string& method, const std::string& params) {
    std::ostringstream oss;
    oss << "{\"jsonrpc\":\"2.0\",\"method\":\"" << method << "\"";
    if (!params.empty() && params != "{}") {
        oss << ",\"params\":" << params;
    }
    oss << "}";
    return oss.str();
}

// 构建工具调用参数
inline std::string buildToolCallParams(const std::string& toolName, const std::string& args) {
    std::ostringstream oss;
    oss << "{\"name\":\"" << escapeJson(toolName) << "\"";
    if (!args.empty()) {
        oss << ",\"arguments\":" << args;
    }
    oss << "}";
    return oss.str();
}

// 从响应中提取结果
inline std::string extractResult(const std::string& json) {
    std::string result = extractObject(json, "result");
    if (result == "{}") {
        // 可能是字符串或其他类型
    }
    return result;
}

// 检查是否有错误
inline bool hasError(const std::string& json) {
    return json.find("\"error\"") != std::string::npos;
}

// 提取错误信息
inline std::string extractError(const std::string& json) {
    std::string error = extractObject(json, "error");
    if (error == "{}") return "Unknown error";
    return extractString(error, "message");
}

// 提取工具调用结果内容
inline std::string extractToolResultContent(const std::string& json) {
    std::string result = extractObject(json, "result");
    if (result == "{}") return "";
    
    std::string content = extractArray(result, "content");
    if (content == "[]") return "";
    
    // 提取第一个内容项的 text
    size_t pos = content.find("{");
    if (pos == std::string::npos) return "";
    
    std::string itemStart = content.substr(pos);
    return extractString(itemStart, "text");
}

} // namespace mcp
} // namespace aries
