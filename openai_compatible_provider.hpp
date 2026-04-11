#pragma once

#include "ai_provider.hpp"
#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#pragma comment(lib, "wininet.lib")

namespace aries {

// Base64 编码
static std::string base64Encode(const std::string& data) {
    static const char base64Chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    size_t i = 0;
    unsigned char charArray3[3];
    unsigned char charArray4[4];
    
    for (size_t inLen = data.size(); i < inLen;) {
        size_t toRead = std::min(size_t(3), inLen - i);
        for (size_t j = 0; j < toRead; j++) {
            charArray3[j] = data[i++];
        }
        
        charArray4[0] = (charArray3[0] & 0xfc) >> 2;
        charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((toRead > 1 ? charArray3[1] : 0) >> 4);
        charArray4[2] = toRead > 1 ? ((charArray3[1] & 0x0f) << 2) + ((toRead > 2 ? charArray3[2] : 0) >> 6) : 0;
        charArray4[3] = toRead > 2 ? (charArray3[2] & 0x3f) : 0;
        
        for (size_t j = 0; j < (toRead + 1); j++) {
            encoded += base64Chars[charArray4[j]];
        }
        
        while (toRead++ < 3) {
            encoded += '=';
        }
    }
    
    return encoded;
}

// 读取文件为 Base64
static std::string readFileAsBase64(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    std::string data((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    file.close();
    
    return base64Encode(data);
}

// JSON 字符串转义（正确处理UTF-8）
static std::string escapeJson(const std::string& str) {
    std::string escaped;
    for (size_t i = 0; i < str.length(); ) {
        unsigned char c = str[i];
        
        // 处理ASCII字符（单字节）
        if (c < 0x80) {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\b': escaped += "\\b"; break;
                case '\f': escaped += "\\f"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default:
                    if (c < 0x20) {
                        std::ostringstream oss;
                        oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                        escaped += oss.str();
                    } else {
                        escaped += c;
                    }
            }
            i++;
        }
        // 处理UTF-8多字节字符
        else {
            // 计算UTF-8字符的字节数
            size_t charLen = 0;
            if ((c & 0xE0) == 0xC0) charLen = 2;      // 110xxxxx - 2字节
            else if ((c & 0xF0) == 0xE0) charLen = 3; // 1110xxxx - 3字节（中文）
            else if ((c & 0xF8) == 0xF0) charLen = 4; // 11110xxx - 4字节
            else charLen = 1; // 非法UTF-8，当作单字节处理
            
            // 确保不越界
            if (i + charLen > str.length()) {
                charLen = str.length() - i;
            }
            
            // 直接复制UTF-8字节（JSON支持UTF-8原文）
            escaped.append(str.substr(i, charLen));
            i += charLen;
        }
    }
    return escaped;
}

// OpenAI 兼容的 Provider 实现
class OpenAICompatibleProvider : public AIProvider {
public:
    OpenAICompatibleProvider(
        const std::string& apiKey,
        const std::string& baseUrl,
        const std::string& modelName,
        bool vision = true,
        bool audio = false,
        bool video = false
    ) : apiKey_(apiKey),
        baseUrl_(baseUrl),
        modelName_(modelName),
        supportsVision_(vision),
        supportsAudio_(audio),
        supportsVideo_(video) {}
    
    std::string getProviderName() const override {
        return "OpenAI";
    }
    
    std::string getModelName() const override {
        return modelName_;
    }
    
    bool supportsVision() const override {
        return supportsVision_;
    }
    
    bool supportsAudio() const override {
        return supportsAudio_;
    }
    
    bool supportsVideo() const override {
        return supportsVideo_;
    }
    
    std::pair<bool, std::string> sendMessage(
        const std::vector<ChatMessage>& messages,
        const std::string& systemPrompt = "") override {
        
        std::ostringstream json;
        json << "{";
        json << "\"model\":\"" << escapeJson(modelName_) << "\",";
        json << "\"stream\":false,";
        json << "\"messages\":[";
        
        bool first = true;
        
        if (!systemPrompt.empty()) {
            json << "{\"role\":\"system\",\"content\":\"" << escapeJson(systemPrompt) << "\"}";
            first = false;
        }
        
        for (const auto& msg : messages) {
            if (!first) json << ",";
            json << "{\"role\":\"" << msg.role << "\",\"content\":\"" << escapeJson(msg.content) << "\"}";
            first = false;
        }
        
        json << "]}";
        
        return sendRequest(json.str());
    }
    
    std::pair<bool, std::string> sendMessageWithImages(
        const std::string& text,
        const std::vector<std::string>& imagePaths,
        const std::string& systemPrompt = "") override {
        
        std::ostringstream json;
        json << "{";
        json << "\"model\":\"" << escapeJson(modelName_) << "\",";
        json << "\"stream\":false,";
        json << "\"messages\":[";
        
        bool first = true;
        
        if (!systemPrompt.empty()) {
            json << "{\"role\":\"system\",\"content\":\"" << escapeJson(systemPrompt) << "\"}";
            first = false;
        }
        
        // User message with multimodal content
        if (!first) json << ",";
        json << "{\"role\":\"user\",\"content\":[";
        
        // Add text
        json << "{\"type\":\"text\",\"text\":\"" << escapeJson(text) << "\"}";
        
        // Add images
        for (const auto& imagePath : imagePaths) {
            std::string base64Image = readFileAsBase64(imagePath);
            if (!base64Image.empty()) {
                json << ",{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/png;base64," << base64Image << "\"}}";
            }
        }
        
        json << "]}]";
        json << "}";
        
        return sendRequest(json.str());
    }
    
    std::string getLastError() const override {
        return lastError_;
    }
    
    // 验证 API Key 是否有效
    bool validateApiKey() {
        // 构建简单的测试请求
        std::ostringstream json;
        json << "{";
        json << "\"model\":\"" << escapeJson(modelName_) << "\",";
        json << "\"messages\":[{\"role\":\"user\",\"content\":\"Reply with OK.\"}],";
        json << "\"stream\":false,";
        json << "\"max_tokens\":32";
        json << "}";
        
        auto result = sendRequest(json.str());
        return result.first;
    }

private:
    std::string apiKey_;
    std::string baseUrl_;
    std::string modelName_;
    bool supportsVision_;
    bool supportsAudio_;
    bool supportsVideo_;
    mutable std::string lastError_;
    
    // 从 JSON 中提取字符串值（处理转义引号）
    std::string extractJsonString(const std::string& json, size_t startPos) {
        // 找到第一个引号
        size_t startQuote = json.find("\"", startPos);
        if (startQuote == std::string::npos) return "";
        
        std::string result;
        size_t i = startQuote + 1;
        
        while (i < json.length()) {
            if (json[i] == '\\' && i + 1 < json.length()) {
                // 处理转义字符
                char next = json[i + 1];
                switch (next) {
                    case '"': result += '"'; i += 2; break;
                    case '\\': result += '\\'; i += 2; break;
                    case 'n': result += '\n'; i += 2; break;
                    case 'r': result += '\r'; i += 2; break;
                    case 't': result += '\t'; i += 2; break;
                    case 'b': result += '\b'; i += 2; break;
                    case 'f': result += '\f'; i += 2; break;
                    case '/': result += '/'; i += 2; break;
                    case 'u': {
                        // Unicode escape
                        if (i + 5 < json.length()) {
                            std::string hex = json.substr(i + 2, 4);
                            try {
                                int code = std::stoi(hex, nullptr, 16);
                                if (code < 0x80) {
                                    result += static_cast<char>(code);
                                } else if (code < 0x800) {
                                    result += static_cast<char>(0xC0 | (code >> 6));
                                    result += static_cast<char>(0x80 | (code & 0x3F));
                                } else {
                                    result += static_cast<char>(0xE0 | (code >> 12));
                                    result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                                    result += static_cast<char>(0x80 | (code & 0x3F));
                                }
                            } catch (...) {
                                result += json[i];
                            }
                            i += 6;
                        } else {
                            result += json[i];
                            i++;
                        }
                        break;
                    }
                    default: result += json[i]; i++; break;
                }
            } else if (json[i] == '"') {
                // 结束引号
                break;
            } else {
                result += json[i];
                i++;
            }
        }
        
        return result;
    }
    
    // 简单 JSON 解析 - 提取 content 和 reasoning_content 字段
    std::string parseContent(const std::string& json) {
        size_t choicesPos = json.find("\"choices\"");
        if (choicesPos == std::string::npos) return "";
        
        // 提取 reasoning_content (智谱 AI 推理模型的思考过程)
        std::string reasoningContent;
        size_t reasoningPos = json.find("\"reasoning_content\":", choicesPos);
        if (reasoningPos != std::string::npos) {
            reasoningContent = extractJsonString(json, reasoningPos + 20);
        }
        
        // 提取 content
        size_t contentPos = json.find("\"content\":", choicesPos);
        if (contentPos == std::string::npos) return "";
        
        std::string content = extractJsonString(json, contentPos + 10);
        
        // 如果有 reasoning_content，合并到结果中
        if (!reasoningContent.empty()) {
            return "<思考过程>\n" + reasoningContent + "\n</思考过程>\n\n" + content;
        }
        
        return content;
    }
    
    std::pair<bool, std::string> sendRequest(const std::string& jsonBody) {
        std::string url = baseUrl_ + "/chat/completions";
        
        HINTERNET hInternet = InternetOpenA("AriesAI/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) {
            lastError_ = "Failed to initialize WinINet";
            return {false, ""};
        }
        
        URL_COMPONENTSA urlComp;
        char hostName[256];
        char urlPath[1024];
        
        memset(&urlComp, 0, sizeof(urlComp));
        urlComp.dwStructSize = sizeof(urlComp);
        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = sizeof(hostName);
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = sizeof(urlPath);
        
        if (!InternetCrackUrlA(url.c_str(), 0, 0, &urlComp)) {
            lastError_ = "Failed to parse URL";
            InternetCloseHandle(hInternet);
            return {false, ""};
        }
        
        HINTERNET hConnect = InternetConnectA(hInternet, hostName, 
            urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) {
            lastError_ = "Failed to connect to server";
            InternetCloseHandle(hInternet);
            return {false, ""};
        }
        
        const char* acceptTypes[] = {"application/json", NULL};
        HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", urlPath, 
            NULL, NULL, acceptTypes, 
            (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_FLAG_SECURE : 0, 0);
        
        if (!hRequest) {
            lastError_ = "Failed to create request";
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return {false, ""};
        }
        
        std::string authHeader = "Authorization: Bearer " + apiKey_;
        std::string contentType = "Content-Type: application/json";
        
        HttpAddRequestHeadersA(hRequest, authHeader.c_str(), (DWORD)authHeader.length(), HTTP_ADDREQ_FLAG_ADD);
        HttpAddRequestHeadersA(hRequest, contentType.c_str(), (DWORD)contentType.length(), HTTP_ADDREQ_FLAG_ADD);
        
        if (!HttpSendRequestA(hRequest, NULL, 0, (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.length())) {
            lastError_ = "Failed to send request";
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return {false, ""};
        }
        
        std::string response;
        char buffer[4096];
        DWORD bytesRead;
        
        while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            response.append(buffer, bytesRead);
        }
        
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        
        // 检查错误
        if (response.find("\"error\"") != std::string::npos) {
            size_t msgPos = response.find("\"message\":\"");
            if (msgPos != std::string::npos) {
                size_t start = msgPos + 11;
                size_t end = response.find("\"", start);
                lastError_ = response.substr(start, end - start);
            } else {
                lastError_ = "API error";
            }
            return {false, ""};
        }
        
        std::string content = parseContent(response);
        if (content.empty()) {
            lastError_ = "Invalid response format";
            return {false, ""};
        }
        
        return {true, content};
    }
};

} // namespace aries
