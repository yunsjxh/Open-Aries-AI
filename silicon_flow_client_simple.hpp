#pragma once

#include <string>
#include <vector>
#include <optional>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace silicon_flow {

// ================= 安全策略 =================

struct SecurityPolicy {
    std::string allowed_dir = "./images/";
    size_t max_file_size = 15 * 1024 * 1024; // 15MB
};


inline bool is_safe_path(const std::string& base, const std::string& path) {
    if (path.find("..") != std::string::npos) return false;
    if (path.find(':') != std::string::npos) return false;
    return path.rfind(base, 0) == 0;
}

// ================= Base64 =================

inline std::string base64_encode(const std::string& in) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    int val = 0, valb = -6;

    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(tbl[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6)
        out.push_back(tbl[((val << 8) >> (valb + 8)) & 0x3F]);

    while (out.size() % 4)
        out.push_back('=');

    return out;
}

// ================= 文件读取 =================

inline std::string image_to_base64(const std::string& path, const SecurityPolicy& policy) {
    if (!is_safe_path(policy.allowed_dir, path)) {
        throw std::runtime_error("unsafe path");
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("file open failed");
    }

    std::ostringstream ss;
    ss << file.rdbuf();

    std::string data = ss.str();

    if (data.size() > policy.max_file_size) {
        throw std::runtime_error("file too large");
    }

    return base64_encode(data);
}



class SafeJson {
public:
    static std::string extract_content(const std::string& json) {
        size_t pos = json.find("\"content\"");
        if (pos == std::string::npos) return "";

        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";

        pos = json.find('"', pos);
        if (pos == std::string::npos) return "";

        std::string result;
        bool escaped = false;

        for (size_t i = pos + 1; i < json.size(); i++) {
            char c = json[i];

            if (escaped) {
                switch (c) {
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case '\\': result += '\\'; break;
                    case '"': result += '"'; break;
                    default: result += c; break;
                }
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                break;
            } else {
                result += c;
            }
        }

        return result;
    }
};

// ================= HTTP 客户端 =================

class HttpClient {
public:
    static std::string post(const std::wstring& host,
                            const std::wstring& path,
                            const std::string& body,
                            const std::string& api_key) {

        HINTERNET hSession = WinHttpOpen(L"sf-client/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession) throw std::runtime_error("WinHttpOpen failed");

        HINTERNET hConnect = WinHttpConnect(
            hSession, host.c_str(),
            INTERNET_DEFAULT_HTTPS_PORT, 0);

        if (!hConnect) throw std::runtime_error("WinHttpConnect failed");

        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect, L"POST", path.c_str(),
            NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);

        if (!hRequest) throw std::runtime_error("WinHttpOpenRequest failed");

        std::wstring headers =
            L"Content-Type: application/json\r\nAuthorization: Bearer " +
            std::wstring(api_key.begin(), api_key.end());

        if (!WinHttpSendRequest(
                hRequest,
                headers.c_str(),
                -1,
                (LPVOID)body.c_str(),
                (DWORD)body.size(),
                (DWORD)body.size(),
                0)) {
            throw std::runtime_error("WinHttpSendRequest failed");
        }

        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            throw std::runtime_error("WinHttpReceiveResponse failed");
        }

        std::string response;
        DWORD size = 0;

        do {
            if (!WinHttpQueryDataAvailable(hRequest, &size)) break;
            if (size == 0) break;

            std::vector<char> buffer(size);
            DWORD downloaded = 0;

            if (!WinHttpReadData(hRequest, buffer.data(), size, &downloaded)) {
                break;
            }

            response.append(buffer.data(), downloaded);

        } while (size > 0);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return response;
    }
};



struct Message {
    std::string role;
    std::string content;
};

// ================= Client =================

class SiliconFlowClient {
public:
    SiliconFlowClient(std::string api_key, SecurityPolicy policy = {})
        : api_key_(std::move(api_key)), policy_(std::move(policy)) {}

    std::string chat(const std::vector<Message>& messages) {

        std::ostringstream body;
        body << "{\"model\":\"deepseek-ai/DeepSeek-V3\",\"messages\":[";

        for (size_t i = 0; i < messages.size(); i++) {
            if (i) body << ",";
            body << "{\"role\":\"" << escape(messages[i].role)
                 << "\",\"content\":\"" << escape(messages[i].content) << "\"}";
        }

        body << "]}";

        std::string resp = HttpClient::post(
            L"api.siliconflow.cn",
            L"/v1/chat/completions",
            body.str(),
            api_key_);

        return SafeJson::extract_content(resp);
    }

private:
    std::string api_key_;
    SecurityPolicy policy_;

    static std::string escape(const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
            }
        }
        return out;
    }
};

} // namespace silicon_flow
