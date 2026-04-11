#pragma once

#include <windows.h>
#include <wininet.h>
#include <string>
#include <sstream>
#include <iostream>
#include "action_parser.hpp"

#pragma comment(lib, "wininet.lib")

namespace aries {

struct ReleaseInfo {
    std::string version;
    std::string publishedAt;
    std::string body;
    std::string htmlUrl;
    bool isPrerelease;
    bool success;
    std::string errorMessage;
};

class UpdateChecker {
public:
    // 检查GitHub releases最新版本
    static ReleaseInfo checkForUpdate(const std::string& repoUrl) {
        ReleaseInfo info;
        info.success = false;
        
        // 解析仓库地址获取 owner 和 repo
        std::string owner, repo;
        if (!parseRepoUrl(repoUrl, owner, repo)) {
            info.errorMessage = "无法解析仓库地址";
            return info;
        }
        
        // 构建GitHub API URL
        std::string apiUrl = "/repos/" + owner + "/" + repo + "/releases/latest";
        
        // 发送HTTP请求
        std::string response = sendGitHubApiRequest(apiUrl);
        if (response.empty()) {
            info.errorMessage = "无法连接到GitHub API";
            return info;
        }
        
        // 解析JSON响应
        return parseReleaseJson(response);
    }
    
    // 获取所有releases
    static std::vector<ReleaseInfo> getAllReleases(const std::string& repoUrl, int count = 5) {
        std::vector<ReleaseInfo> releases;
        
        std::string owner, repo;
        if (!parseRepoUrl(repoUrl, owner, repo)) {
            return releases;
        }
        
        std::string apiUrl = "/repos/" + owner + "/" + repo + "/releases?per_page=" + std::to_string(count);
        
        std::string response = sendGitHubApiRequest(apiUrl);
        if (response.empty()) {
            return releases;
        }
        
        // 解析JSON数组
        return parseReleasesArray(response);
    }
    
    // 比较版本号
    static int compareVersions(const std::string& v1, const std::string& v2) {
        std::vector<int> ver1 = parseVersion(v1);
        std::vector<int> ver2 = parseVersion(v2);
        
        size_t maxLen = std::max(ver1.size(), ver2.size());
        for (size_t i = 0; i < maxLen; i++) {
            int num1 = (i < ver1.size()) ? ver1[i] : 0;
            int num2 = (i < ver2.size()) ? ver2[i] : 0;
            
            if (num1 < num2) return -1;
            if (num1 > num2) return 1;
        }
        return 0;
    }

private:
    static bool parseRepoUrl(const std::string& url, std::string& owner, std::string& repo) {
        // 支持格式: https://github.com/owner/repo/releases
        // 或: https://github.com/owner/repo
        size_t githubPos = url.find("github.com/");
        if (githubPos == std::string::npos) {
            return false;
        }
        
        std::string path = url.substr(githubPos + 11); // 跳过 "github.com/"
        
        // 移除末尾的 "/releases" 等
        size_t releasesPos = path.find("/releases");
        if (releasesPos != std::string::npos) {
            path = path.substr(0, releasesPos);
        }
        
        // 解析 owner 和 repo
        size_t slashPos = path.find('/');
        if (slashPos == std::string::npos) {
            return false;
        }
        
        owner = path.substr(0, slashPos);
        repo = path.substr(slashPos + 1);
        
        // 移除末尾的斜杠
        if (!repo.empty() && repo.back() == '/') {
            repo.pop_back();
        }
        
        return !owner.empty() && !repo.empty();
    }
    
    static std::string sendGitHubApiRequest(const std::string& apiPath) {
        HINTERNET hInternet = InternetOpenA("Open-Aries-AI-UpdateChecker/1.0", 
                                            INTERNET_OPEN_TYPE_PRECONFIG, 
                                            NULL, NULL, 0);
        if (!hInternet) {
            return "";
        }
        
        HINTERNET hConnect = InternetConnectA(hInternet, 
                                              "api.github.com", 
                                              INTERNET_DEFAULT_HTTPS_PORT, 
                                              NULL, NULL, 
                                              INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) {
            InternetCloseHandle(hInternet);
            return "";
        }
        
        HINTERNET hRequest = HttpOpenRequestA(hConnect, 
                                              "GET", 
                                              apiPath.c_str(), 
                                              NULL, NULL, NULL, 
                                              INTERNET_FLAG_SECURE, 0);
        if (!hRequest) {
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return "";
        }
        
        // 添加User-Agent头（GitHub API要求）
        HttpAddRequestHeadersA(hRequest, 
                               "User-Agent: Open-Aries-AI-UpdateChecker\r\n", 
                               -1, HTTP_ADDREQ_FLAG_ADD);
        
        BOOL sent = HttpSendRequestA(hRequest, NULL, 0, NULL, 0);
        if (!sent) {
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return "";
        }
        
        // 读取响应
        std::string response;
        char buffer[4096];
        DWORD bytesRead;
        
        while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            response.append(buffer, bytesRead);
        }
        
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        
        return response;
    }
    
    static ReleaseInfo parseReleaseJson(const std::string& json) {
        ReleaseInfo info;
        info.success = false;
        
        // 简单JSON解析
        info.version = extractJsonString(json, "tag_name");
        info.publishedAt = extractJsonString(json, "published_at");
        info.body = extractJsonString(json, "body");
        info.htmlUrl = extractJsonString(json, "html_url");
        info.isPrerelease = (json.find("\"prerelease\":true") != std::string::npos);
        
        if (!info.version.empty()) {
            info.success = true;
        } else {
            info.errorMessage = "无法解析版本信息";
        }
        
        return info;
    }
    
    static std::vector<ReleaseInfo> parseReleasesArray(const std::string& json) {
        std::vector<ReleaseInfo> releases;
        
        // 简单解析JSON数组
        size_t pos = 0;
        while ((pos = json.find("{", pos)) != std::string::npos) {
            size_t endPos = findMatchingBrace(json, pos);
            if (endPos == std::string::npos) break;
            
            std::string releaseJson = json.substr(pos, endPos - pos + 1);
            ReleaseInfo info = parseReleaseJson(releaseJson);
            if (info.success) {
                releases.push_back(info);
            }
            
            pos = endPos + 1;
        }
        
        return releases;
    }
    
    static size_t findMatchingBrace(const std::string& str, size_t start) {
        int depth = 1;
        size_t pos = start + 1;
        
        while (pos < str.length() && depth > 0) {
            if (str[pos] == '{') depth++;
            else if (str[pos] == '}') depth--;
            else if (str[pos] == '"') {
                // 跳过字符串
                pos++;
                while (pos < str.length() && str[pos] != '"') {
                    if (str[pos] == '\\' && pos + 1 < str.length()) {
                        pos += 2;
                    } else {
                        pos++;
                    }
                }
            }
            pos++;
        }
        
        return (depth == 0) ? pos - 1 : std::string::npos;
    }
    
    static std::string extractJsonString(const std::string& json, const std::string& key) {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = json.find(searchKey);
        if (keyPos == std::string::npos) {
            return "";
        }
        
        size_t colonPos = json.find(":", keyPos);
        if (colonPos == std::string::npos) {
            return "";
        }
        
        size_t valueStart = json.find_first_of("\"", colonPos);
        if (valueStart == std::string::npos) {
            // 可能是布尔值或null
            size_t boolStart = json.find_first_not_of(" \t\n\r", colonPos + 1);
            if (boolStart != std::string::npos) {
                size_t boolEnd = json.find_first_of(",}\n", boolStart);
                if (boolEnd != std::string::npos) {
                    return json.substr(boolStart, boolEnd - boolStart);
                }
            }
            return "";
        }
        
        valueStart++; // 跳过开头的引号
        
        std::string value;
        for (size_t i = valueStart; i < json.length(); i++) {
            if (json[i] == '"' && (i == valueStart || json[i-1] != '\\')) {
                break;
            }
            if (json[i] == '\\' && i + 1 < json.length()) {
                char next = json[i + 1];
                switch (next) {
                    case 'n': value += '\n'; break;
                    case 'r': value += '\r'; break;
                    case 't': value += '\t'; break;
                    case '"': value += '"'; break;
                    case '\\': value += '\\'; break;
                    default: value += next;
                }
                i++;
            } else {
                value += json[i];
            }
        }
        
        return value;
    }
    
    static std::vector<int> parseVersion(const std::string& version) {
        std::vector<int> result;
        std::string cleanVersion = version;
        
        // 移除开头的 'v' 或 'V'
        if (!cleanVersion.empty() && (cleanVersion[0] == 'v' || cleanVersion[0] == 'V')) {
            cleanVersion = cleanVersion.substr(1);
        }
        
        // 解析版本号
        std::stringstream ss(cleanVersion);
        std::string part;
        while (std::getline(ss, part, '.')) {
            try {
                result.push_back(std::stoi(part));
            } catch (...) {
                result.push_back(0);
            }
        }
        
        return result;
    }
};

} // namespace aries
