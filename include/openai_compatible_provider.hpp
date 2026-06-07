#pragma once

#include "ai_provider.hpp"
#include "security_config.hpp"
#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <atomic>
#include <cstdio>

#pragma comment(lib, "wininet.lib")

namespace aries {

// Shared log file (set by host application)
static FILE* g_providerLog = nullptr;
inline void providerSetLog(FILE* f) { g_providerLog = f; }
#define PLOG(fmt, ...) do { if (g_providerLog) { fprintf(g_providerLog, "[%lu][API] " fmt "\n", GetCurrentThreadId(), ##__VA_ARGS__); fflush(g_providerLog); } } while(0)

// CRC32 计算 (与 PNG / zlib 一致)
static uint32_t crc32Table[256];
static bool crc32Inited = false;
static void initCrc32() {
    if (crc32Inited) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ ((c & 1) ? 0xEDB88320u : 0);
        crc32Table[i] = c;
    }
    crc32Inited = true;
}
static uint32_t crc32(const void* data, size_t len) {
    initCrc32();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        c = crc32Table[(c ^ ((const uint8_t*)data)[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static std::string base64Encode(const std::string& data);

// 构建最小的有效 PNG (1x1 蓝色像素)，返回 base64 编码
static std::string createTinyPngBase64() {
    std::string png;
    png += "\x89PNG\r\n\x1a\n";

    auto writeBE = [&](uint32_t v) {
        uint8_t be[4] = {(uint8_t)(v>>24),(uint8_t)(v>>16),(uint8_t)(v>>8),(uint8_t)v};
        png.append((const char*)be, 4);
    };
    auto addChunk = [&](const char* type, const void* data, uint32_t len) {
        writeBE(len);
        png.append(type, 4);
        if (len) png.append((const char*)data, len);
        std::string crcInput(type, 4);
        if (len) crcInput.append((const char*)data, len);
        writeBE(crc32(crcInput.data(), crcInput.size()));
    };

    // IHDR: 1x1, 8-bit RGB
    uint8_t ihdr[13] = {0,0,0,1, 0,0,0,1, 8,2,0,0,0};
    addChunk("IHDR", ihdr, 13);

    // Raw image data: filter(0x00) + BGR or RGB...
    // Actually PNG uses RGB order for color type 2: R, G, B
    uint8_t raw[4] = {0x00, 0x00, 0x00, 0xFF}; // filter=none, R=0, G=0, B=255 (blue)

    // Build zlib stream: header + deflate stored block + adler32
    uint32_t a = 1, b = 0; // adler-32
    for (int i = 0; i < 4; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    uint32_t adler = (b << 16) | a;

    // Deflate stored block: BFINAL=1,BTYPE=00 → 0x01, LEN=4, NLEN=~4
    uint8_t deflate[] = {0x01, 0x04,0x00, 0xFB,0xFF, 0x00, 0x00,0x00,0xFF};

    // Zlib = header(0x78,0x9C) + deflate + adler32
    std::string zlibData;
    zlibData += (char)0x78;
    zlibData += (char)0x9C;
    zlibData.append((const char*)deflate, sizeof(deflate));
    zlibData += (char)(adler >> 24);
    zlibData += (char)(adler >> 16);
    zlibData += (char)(adler >> 8);
    zlibData += (char)adler;

    addChunk("IDAT", zlibData.data(), (uint32_t)zlibData.size());
    addChunk("IEND", nullptr, 0);
    return base64Encode(png);
}

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
        
        std::cerr << "[调试] sendMessageWithImages: model=" << modelName_ << ", baseUrl=" << baseUrl_ << std::endl;
        std::cerr << "[调试] sendMessageWithImages: apiKey长度=" << apiKey_.length() << std::endl;
        std::cerr << "[调试] sendMessageWithImages: imagePaths数量=" << imagePaths.size() << std::endl;
        
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
        int imageCount = 0;
        for (const auto& imagePath : imagePaths) {
            std::string base64Image = readFileAsBase64(imagePath);
            std::cerr << "[调试] 读取图片: " << imagePath << ", base64长度=" << base64Image.length() << std::endl;
            if (!base64Image.empty()) {
                json << ",{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/png;base64," << base64Image << "\"}}";
                imageCount++;
            }
        }
        
        json << "]}]";
        json << ",\"max_tokens\":4096";
        json << "}";
        
        std::string jsonBody = json.str();
        std::cerr << "[调试] 请求体大小: " << jsonBody.length() << " bytes" << std::endl;
        std::cerr << "[调试] 请求体前500字符: " << jsonBody.substr(0, 500) << "..." << std::endl;
        
        auto result = sendRequest(jsonBody);
        std::cerr << "[调试] 请求结果: success=" << (result.first ? "true" : "false") << std::endl;
        if (!result.first) {
            std::cerr << "[调试] 错误信息: " << lastError_ << std::endl;
        }
        return result;
    }
    
    std::string getLastError() const override {
        return lastError_;
    }

    TokenUsage getLastTokenUsage() const override {
        return lastTokenUsage_;
    }

    // ── 流式发送消息 ──
    std::pair<bool, std::string> sendMessageStream(
        const std::vector<ChatMessage>& messages,
        StreamDeltaCallback onDelta,
        const std::string& systemPrompt = "") override {

        std::ostringstream json;
        json << "{";
        json << "\"model\":\"" << escapeJson(modelName_) << "\",";
        json << "\"stream\":true,";
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

        return sendStreamRequest(json.str(), onDelta);
    }

    std::pair<bool, std::string> sendMessageWithImagesStream(
        const std::string& text,
        const std::vector<std::string>& imagePaths,
        StreamDeltaCallback onDelta,
        const std::string& systemPrompt = "") override {

        std::ostringstream json;
        json << "{";
        json << "\"model\":\"" << escapeJson(modelName_) << "\",";
        json << "\"stream\":true,";
        json << "\"messages\":[";

        bool first = true;
        if (!systemPrompt.empty()) {
            json << "{\"role\":\"system\",\"content\":\"" << escapeJson(systemPrompt) << "\"}";
            first = false;
        }
        if (!first) json << ",";
        json << "{\"role\":\"user\",\"content\":[";
        json << "{\"type\":\"text\",\"text\":\"" << escapeJson(text) << "\"}";
        for (const auto& imagePath : imagePaths) {
            std::string b64 = readFileAsBase64(imagePath);
            if (!b64.empty()) {
                json << ",{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/png;base64," << b64 << "\"}}";
            }
        }
        json << "]}]";
        json << ",\"max_tokens\":4096";
        json << "}";

        return sendStreamRequest(json.str(), onDelta);
    }

    // Public wrapper for calling sendStreamRequest from outside the class
    std::pair<bool, std::string> sendRawStreamRequest(const std::string& jsonBody, StreamDeltaCallback onDelta) {
        return sendStreamRequest(jsonBody, onDelta);
    }

    // Public wrapper for calling sendRequest from outside the class
    std::pair<bool, std::string> sendRawRequest(const std::string& jsonBody) override {
        return sendRequest(jsonBody);
    }

    // Raw stream request with tools output
    std::pair<bool, std::string> sendRawStreamRequestWithTools(
        const std::string& jsonBody,
        StreamDeltaCallback onDelta,
        std::vector<ToolCallInfo>& outToolCalls) override {
        return sendStreamRequest(jsonBody, onDelta, &outToolCalls);
    }

    // Native function calling: stream with tools, return tool calls via outToolCalls
    std::pair<bool, std::string> sendMessageStreamWithTools(
        const std::vector<ChatMessage>& messages,
        StreamDeltaCallback onDelta,
        const std::string& systemPrompt,
        const std::string& toolsJson,
        std::vector<ToolCallInfo>& outToolCalls) override {

        std::ostringstream json;
        json << "{";
        json << "\"model\":\"" << escapeJson(modelName_) << "\",";
        json << "\"stream\":true,";
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
        json << "]";
        if (!toolsJson.empty()) {
            json << ",\"tools\":" << toolsJson;
            json << ",\"tool_choice\":\"auto\"";
        }
        json << ",\"stream_options\":{\"include_usage\":true}";
        json << "}";

        return sendStreamRequest(json.str(), onDelta, &outToolCalls);
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

    // 测试是否为视觉模型（发送 1x1 像素 PNG 测试图片）
    bool testVisionSupport() {
        std::string tinyPng = createTinyPngBase64();
        PLOG("VISION TEST PNG base64 len=%zu", tinyPng.length());

        std::ostringstream json;
        json << "{";
        json << "\"model\":\"" << escapeJson(modelName_) << "\",";
        json << "\"stream\":false,";
        json << "\"messages\":[{";
        json << "\"role\":\"user\",";
        json << "\"content\":[";
        json << "{\"type\":\"text\",\"text\":\"Reply with just OK\"},";
        json << "{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/png;base64," << tinyPng << "\"}}";
        json << "]}]";
        json << ",\"max_tokens\":16";
        json << "}";

        // Try up to 2 times (SiliconFlow image validator is sometimes flaky)
        for (int attempt = 1; attempt <= 2; attempt++) {
            PLOG("VISION TEST attempt %d", attempt);
            auto result = sendRequest(json.str());
            if (result.first) {
                PLOG("VISION TEST OK: %s", result.second.c_str());
                return true;
            }
            PLOG("VISION TEST attempt %d FAILED: %s", attempt, lastError_.c_str());
            if (attempt < 2) Sleep(1500); // wait 1.5s before retry
        }
        return false;
    }

private:
    std::string apiKey_;
    std::string baseUrl_;
    std::string modelName_;
    bool supportsVision_;
    bool supportsAudio_;
    bool supportsVideo_;
    mutable std::string lastError_;
    mutable TokenUsage lastTokenUsage_;

    // 从 JSON 中提取 token usage
    void parseTokenUsage(const std::string& json) {
        lastTokenUsage_ = TokenUsage();
        size_t usagePos = json.find("\"usage\"");
        if (usagePos == std::string::npos) return;

        // prompt_tokens
        size_t pPos = json.find("\"prompt_tokens\":", usagePos);
        if (pPos != std::string::npos) {
            pPos += 17;
            while (pPos < json.length() && (json[pPos] == ' ' || json[pPos] == '\t')) pPos++;
            while (pPos < json.length() && json[pPos] >= '0' && json[pPos] <= '9') {
                lastTokenUsage_.prompt_tokens = lastTokenUsage_.prompt_tokens * 10 + (json[pPos] - '0');
                pPos++;
            }
        }

        // completion_tokens
        size_t cPos = json.find("\"completion_tokens\":", usagePos);
        if (cPos != std::string::npos) {
            cPos += 21;
            while (cPos < json.length() && (json[cPos] == ' ' || json[cPos] == '\t')) cPos++;
            while (cPos < json.length() && json[cPos] >= '0' && json[cPos] <= '9') {
                lastTokenUsage_.completion_tokens = lastTokenUsage_.completion_tokens * 10 + (json[cPos] - '0');
                cPos++;
            }
        }

        // total_tokens
        size_t tPos = json.find("\"total_tokens\":", usagePos);
        if (tPos != std::string::npos) {
            tPos += 15;
            while (tPos < json.length() && (json[tPos] == ' ' || json[tPos] == '\t')) tPos++;
            while (tPos < json.length() && json[tPos] >= '0' && json[tPos] <= '9') {
                lastTokenUsage_.total_tokens = lastTokenUsage_.total_tokens * 10 + (json[tPos] - '0');
                tPos++;
            }
        }
    }

    // SSE 流式请求
    std::pair<bool, std::string> sendStreamRequest(const std::string& jsonBody, StreamDeltaCallback onDelta,
                                                    std::vector<ToolCallInfo>* outToolCalls = nullptr) {
        std::string url = baseUrl_ + "/chat/completions";
        lastTokenUsage_ = TokenUsage();

        HINTERNET hInternet = InternetOpenA("AriesAI/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) {
            lastError_ = "Failed to initialize WinINet";
            return {false, ""};
        }

        char hostName[256] = {};
        char urlPath[1024] = {};
        URL_COMPONENTSA urlComp = {};
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
            lastError_ = "Failed to connect";
            InternetCloseHandle(hInternet);
            return {false, ""};
        }

        const char* acceptTypes[] = {"text/event-stream", "application/json", NULL};
        DWORD flags = 0;
        if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
            flags = INTERNET_FLAG_SECURE;
            auto secCfg = SecurityConfigLoader::loadFromFileAndEnv();
            if (secCfg.allowInsecureHttps) {
                flags |= INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
                         INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
            }
        }
        HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", urlPath,
            NULL, NULL, acceptTypes, flags, 0);
        if (!hRequest) {
            lastError_ = "Failed to create request";
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return {false, ""};
        }

        DWORD timeout = 180000; // 3 minutes for streaming
        InternetSetOptionA(hRequest, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

        std::string authHeader = "Authorization: Bearer " + apiKey_;
        std::string contentType = "Content-Type: application/json";
        HttpAddRequestHeadersA(hRequest, authHeader.c_str(), (DWORD)authHeader.length(), HTTP_ADDREQ_FLAG_ADD);
        HttpAddRequestHeadersA(hRequest, contentType.c_str(), (DWORD)contentType.length(), HTTP_ADDREQ_FLAG_ADD);

        if (!HttpSendRequestA(hRequest, NULL, 0, (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.length())) {
            DWORD err = GetLastError();
            std::ostringstream oss;
            oss << "Failed to send request (Error " << err << ")";
            lastError_ = oss.str();
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            PLOG("STREAM REQUEST FAILED: %s (err=%lu)", lastError_.c_str(), err);
            return {false, ""};
        }

        PLOG("STREAM REQ → %s | model=%s | body=%zu bytes",
             url.c_str(), modelName_.c_str(), jsonBody.length());
        PLOG("STREAM BODY: %s", jsonBody.c_str());

        // Tool call accumulation (native function calling)
        std::vector<ToolCallInfo> toolCallAcc;  // accumulated by index

        // Read SSE stream incrementally
        std::string fullContent;
        std::string sseBuffer;
        char buffer[4096];
        DWORD bytesRead;
        bool streamDone = false;
        size_t consumed = 0;  // cursor avoids O(n²) per-event erase
        bool reasoningActive = false;  // track reasoning blocks to prefix marker only once
        bool textToolNotified = false;  // avoid duplicate \x02 for text-based tool calls

        while (!streamDone && !g_abortFlag.load() && InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            sseBuffer.append(buffer, bytesRead);

            // Process complete SSE events from cursor forward
            size_t pos = 0;
            while ((pos = sseBuffer.find("\n\n", consumed)) != std::string::npos) {
                std::string event = sseBuffer.substr(consumed, pos - consumed);
                consumed = pos + 2;

                // Parse SSE lines: look for "data: {...}"
                std::string dataJson;
                size_t lineStart = 0;
                while (lineStart < event.length()) {
                    size_t lineEnd = event.find('\n', lineStart);
                    if (lineEnd == std::string::npos) lineEnd = event.length();
                    std::string line = event.substr(lineStart, lineEnd - lineStart);

                    if (line.rfind("data: ", 0) == 0) {
                        dataJson += line.substr(6);
                    } else if (line.rfind("data:", 0) == 0) {
                        dataJson += line.substr(5);
                    }

                    lineStart = lineEnd + 1;
                }

                if (dataJson.empty()) continue;
                if (dataJson == "[DONE]") { PLOG("STREAM END (DONE) | total_content=%zu chars", fullContent.length()); streamDone = true; break; }

                PLOG("SSE ← %s", dataJson.substr(0, 300).c_str());

                // Parse delta content from streaming chunk
                size_t choicesPos = dataJson.find("\"choices\"");
                if (choicesPos == std::string::npos) continue;

                size_t deltaPos = dataJson.find("\"delta\"", choicesPos);
                if (deltaPos == std::string::npos) continue;

                size_t contentPos = dataJson.find("\"content\":", deltaPos);
                // Check if content is null (GLM-5 thinking chunks) vs a string
                std::string deltaText;
                if (contentPos != std::string::npos) {
                    size_t valStart = dataJson.find_first_not_of(" \t\r\n", contentPos + 10);
                    if (valStart != std::string::npos && dataJson[valStart] == '"') {
                        deltaText = extractJsonString(dataJson, contentPos + 10);
                    }
                }

                // Also extract reasoning_content (for GLM-5 thinking models)
                size_t reasoningPos = dataJson.find("\"reasoning_content\":", deltaPos);
                if (reasoningPos != std::string::npos) {
                    size_t rValStart = dataJson.find_first_not_of(" \t\r\n", reasoningPos + 20);
                    if (rValStart != std::string::npos && dataJson[rValStart] == '"') {
                        std::string reasoningDelta = extractJsonString(dataJson, reasoningPos + 20);
                        if (!reasoningDelta.empty() && onDelta) {
                            if (!reasoningActive) {
                                onDelta(std::string("\x01") + reasoningDelta, false);
                                reasoningActive = true;
                            } else {
                                onDelta(std::string("\x01") + reasoningDelta, false);
                            }
                        }
                    }
                }

                if (!deltaText.empty()) {
                    reasoningActive = false;  // content starts → reasoning block ends
                    fullContent += deltaText;
                    if (onDelta) {
                        // Text-based tool call detection (for models that don't support native function calling)
                        if (outToolCalls && !textToolNotified) {
                            size_t tb = fullContent.find('[');
                            if (tb != std::string::npos) {
                                size_t te = fullContent.find(']', tb);
                                if (te != std::string::npos && te > tb + 1) {
                                    std::string maybe = fullContent.substr(tb + 1, te - tb - 1);
                                    if (maybe != "END" && maybe.find('[') == std::string::npos) {
                                        bool known = false;
                                        const char* knownTools[] = {
                                            "READ_FILE","WRITE_FILE","LIST_DIR","LIST_APPS","OPEN_APP",
                                            "UNINSTALL_APP","OPEN_APP_LOCATION","RUN_PS","LIST_PROCESSES",
                                            "KILL_PROCESS","GET_FOREGROUND_WINDOW","LIST_WINDOWS","CAPTURE_WINDOW"
                                        };
                                        for (auto& kt : knownTools) { if (maybe == kt) { known = true; break; } }
                                        if (known) {
                                            onDelta(std::string("\x02") + maybe, false);
                                            textToolNotified = true;
                                            // Forward remaining text in this delta as \x03 args (after the tool tag)
                                            size_t afterTag = deltaText.find(']');
                                            if (afterTag != std::string::npos && afterTag + 1 < deltaText.size()) {
                                                onDelta(std::string("\x03") + deltaText.substr(afterTag + 1), false);
                                            }
                                            goto content_done;
                                        }
                                    }
                                }
                            }
                            onDelta(deltaText, false);
                        } else if (textToolNotified) {
                            // After detecting text-based tool, route content as argument deltas
                            onDelta(std::string("\x03") + deltaText, false);
                        } else {
                            onDelta(deltaText, false);
                        }
                    }
                    content_done:;
                }

                // Parse tool_calls from delta (native function calling)
                if (outToolCalls) {
                    size_t tcPos = dataJson.find("\"tool_calls\"");
                    if (tcPos != std::string::npos) {
                        // Parse each tool call entry in the array
                        size_t searchFrom = tcPos;
                        while ((searchFrom = dataJson.find("\"index\":", searchFrom)) != std::string::npos) {
                            int tcIndex = -1;
                            size_t numStart = searchFrom + 8;
                            while (numStart < dataJson.size() && (dataJson[numStart] == ' ' || dataJson[numStart] == '\t')) numStart++;
                            while (numStart < dataJson.size() && dataJson[numStart] >= '0' && dataJson[numStart] <= '9') {
                                tcIndex = (tcIndex < 0 ? 0 : tcIndex) * 10 + (dataJson[numStart] - '0');
                                numStart++;
                            }
                            if (tcIndex < 0) { searchFrom++; continue; }
                            if ((int)toolCallAcc.size() <= tcIndex) toolCallAcc.resize(tcIndex + 1);
                            auto& tc = toolCallAcc[tcIndex];

                            size_t idPos = dataJson.find("\"id\":\"", searchFrom);
                            if (idPos != std::string::npos && idPos < dataJson.size() && tc.id.empty()) {
                                tc.id = extractJsonString(dataJson, idPos + 4);
                            }
                            size_t fnPos = dataJson.find("\"function\":", searchFrom);
                            if (fnPos != std::string::npos) {
                                size_t namePos = dataJson.find("\"name\":\"", fnPos);
                                if (namePos != std::string::npos && namePos < dataJson.size() && tc.name.empty()) {
                                    tc.name = extractJsonString(dataJson, namePos + 7);
                                    // Real-time notification: tool call streaming started
                                    if (onDelta && !tc.name.empty()) {
                                        onDelta(std::string("\x02") + tc.name, false);
                                    }
                                }
                                size_t argsPos = dataJson.find("\"arguments\":\"", fnPos);
                                if (argsPos != std::string::npos) {
                                    std::string argFrag = extractJsonString(dataJson, argsPos + 12);
                                    tc.arguments += argFrag;
                                    // Stream argument deltas to UI in real-time
                                    if (onDelta && !argFrag.empty()) {
                                        onDelta(std::string("\x03") + argFrag, false);
                                    }
                                }
                            }
                            searchFrom = numStart;  // advance past this index
                        }
                    }
                }

                // Check for usage in final chunk
                if (dataJson.find("\"usage\"") != std::string::npos) {
                    parseTokenUsage(dataJson);
                }
            }

            // Periodic cleanup: free consumed portion to bound memory
            if (consumed > 65536) {
                sseBuffer.erase(0, consumed);
                consumed = 0;
            }
        }

        // If no SSE events were found, treat as non-streaming response
        if (fullContent.empty() && sseBuffer.size() > consumed) {
            std::string remaining = sseBuffer.substr(consumed);
            // Try parsing as regular JSON response
            if (remaining.find("\"error\"") != std::string::npos) {
                size_t msgPos = remaining.find("\"message\":\"");
                if (msgPos != std::string::npos) {
                    lastError_ = extractJsonString(remaining, msgPos + 10);
                } else {
                    lastError_ = "API error";
                }
                InternetCloseHandle(hRequest);
                InternetCloseHandle(hConnect);
                InternetCloseHandle(hInternet);
                return {false, ""};
            }

            fullContent = parseContent(remaining);
            parseTokenUsage(remaining);
            if (!fullContent.empty() && onDelta) {
                onDelta(fullContent, true);
            }
        }

        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);

        if (onDelta) onDelta("", true); // signal done

        // Empty content is not an error — model may legitimately produce nothing
        if (fullContent.empty()) {
            PLOG("STREAM empty content (model said nothing)");
        }

        if (outToolCalls) {
            *outToolCalls = std::move(toolCallAcc);
            PLOG("STREAM tool_calls: %zu functions", outToolCalls->size());
            for (auto& tc : *outToolCalls) {
                PLOG("  TOOL: id=%s name=%s args=%s", tc.id.c_str(), tc.name.c_str(), tc.arguments.c_str());
            }
        }

        return {true, fullContent};
    }
    
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
            size_t rValStart = json.find_first_not_of(" \t\r\n", reasoningPos + 20);
            if (rValStart != std::string::npos && json[rValStart] == '"') {
                reasoningContent = extractJsonString(json, reasoningPos + 20);
            }
        }

        // 提取 content
        size_t contentPos = json.find("\"content\":", choicesPos);
        std::string content;
        if (contentPos != std::string::npos) {
            size_t valStart = json.find_first_not_of(" \t\r\n", contentPos + 10);
            if (valStart != std::string::npos && json[valStart] == '"') {
                content = extractJsonString(json, contentPos + 10);
            }
        }
        
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
            DWORD errorCode = GetLastError();
            std::ostringstream oss;
            oss << "Failed to connect to server (Error " << errorCode << "): " << hostName;
            lastError_ = oss.str();
            InternetCloseHandle(hInternet);
            return {false, ""};
        }
        
        const char* acceptTypes[] = {"application/json", NULL};
        DWORD flags = 0;
        if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
            flags = INTERNET_FLAG_SECURE;
            auto secCfg = SecurityConfigLoader::loadFromFileAndEnv();
            if (secCfg.allowInsecureHttps) {
                flags |= INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
                         INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
            }
        }
        HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", urlPath,
            NULL, NULL, acceptTypes, flags, 0);
        
        if (!hRequest) {
            DWORD errorCode = GetLastError();
            std::ostringstream oss;
            oss << "Failed to create request (Error " << errorCode << ")";
            lastError_ = oss.str();
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return {false, ""};
        }
        
        DWORD timeout = 120000;  // 增加到 120 秒
        InternetSetOptionA(hRequest, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
        
        std::string authHeader = "Authorization: Bearer " + apiKey_;
        std::string contentType = "Content-Type: application/json";
        
        HttpAddRequestHeadersA(hRequest, authHeader.c_str(), (DWORD)authHeader.length(), HTTP_ADDREQ_FLAG_ADD);
        HttpAddRequestHeadersA(hRequest, contentType.c_str(), (DWORD)contentType.length(), HTTP_ADDREQ_FLAG_ADD);
        
        if (!HttpSendRequestA(hRequest, NULL, 0, (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.length())) {
            DWORD errorCode = GetLastError();
            std::ostringstream oss;
            oss << "Failed to send request (Error " << errorCode << "): ";
            
            char* errorMsg = nullptr;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                          NULL, errorCode, 0, (LPSTR)&errorMsg, 0, NULL);
            if (errorMsg) {
                std::string msg(errorMsg);
                LocalFree(errorMsg);
                size_t end = msg.find_last_not_of("\r\n");
                if (end != std::string::npos) {
                    msg = msg.substr(0, end + 1);
                }
                oss << msg;
            } else {
                oss << "Unknown error";
            }
            
            oss << " [URL: " << baseUrl_ << ", Body size: " << jsonBody.length() << " bytes]";
            lastError_ = oss.str();
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

        PLOG("REQ → %s | model=%s | body=%zu bytes", url.c_str(), modelName_.c_str(), jsonBody.length());
        PLOG("REQ BODY: %s", jsonBody.c_str());
        PLOG("RSP ← %zu bytes", response.length());
        PLOG("RSP BODY: %s", response.substr(0, 2000).c_str());

        // 检查错误
        if (response.find("\"error\"") != std::string::npos) {
            size_t msgPos = response.find("\"message\":\"");
            if (msgPos != std::string::npos) {
                lastError_ = extractJsonString(response, msgPos + 10);
            } else {
                lastError_ = "API error";
            }
            return {false, ""};
        }
        
        // 检查 SiliconFlow 等平台的错误格式 {"code":xxx,"message":"xxx","data":null}
        if (response.find("\"code\":") != std::string::npos && response.find("\"message\":") != std::string::npos) {
            size_t codePos = response.find("\"code\":");
            size_t msgPos = response.find("\"message\":");
            if (codePos != std::string::npos && msgPos != std::string::npos) {
                // 检查 code 是否为错误码（非0）
                size_t codeStart = codePos + 7;
                while (codeStart < response.length() && (response[codeStart] == ' ' || response[codeStart] == '\t')) {
                    codeStart++;
                }
                if (codeStart < response.length()) {
                    int code = 0;
                    while (codeStart < response.length() && response[codeStart] >= '0' && response[codeStart] <= '9') {
                        code = code * 10 + (response[codeStart] - '0');
                        codeStart++;
                    }
                    if (code != 0) {
                        size_t msgStart = response.find("\"", msgPos + 10);
                        if (msgStart != std::string::npos) {
                            lastError_ = extractJsonString(response, msgStart) + " [完整响应: " + response.substr(0, 500) + "...]";
                        } else {
                            lastError_ = "API error (code: " + std::to_string(code) + ") [响应: " + response.substr(0, 500) + "...]";
                        }
                        return {false, ""};
                    }
                }
            }
        }
        
        std::string content = parseContent(response);
        if (content.empty()) {
            // 调试输出：记录原始响应的前 500 字符
            std::cerr << "[调试] API 响应解析失败，原始响应: "
                      << response.substr(0, 500) << (response.length() > 500 ? "..." : "") << std::endl;
            lastError_ = "Invalid response format";
            return {false, ""};
        }

        parseTokenUsage(response);
        return {true, content};
    }
};

} // namespace aries
