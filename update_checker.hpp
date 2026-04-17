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
        
        // 查找版本号开始的位置（查找 'v' 或数字）
        size_t versionStart = std::string::npos;
        
        // 先尝试查找 "v" 前缀（如 Open-Aries-v1.2 中的 v）
        size_t vPos = cleanVersion.find('v');
        if (vPos != std::string::npos) {
            // 检查 v 后面是否跟着数字
            if (vPos + 1 < cleanVersion.length() && std::isdigit(cleanVersion[vPos + 1])) {
                versionStart = vPos + 1; // 跳过 'v'，从数字开始
            }
        }
        
        // 如果没找到 v+数字 模式，尝试找第一个数字
        if (versionStart == std::string::npos) {
            for (size_t i = 0; i < cleanVersion.length(); i++) {
                if (std::isdigit(cleanVersion[i])) {
                    versionStart = i;
                    break;
                }
            }
        }
        
        // 提取版本号部分
        if (versionStart != std::string::npos) {
            cleanVersion = cleanVersion.substr(versionStart);
        }
        
        // 解析版本号（支持 x.y.z 格式）
        std::stringstream ss(cleanVersion);
        std::string part;
        while (std::getline(ss, part, '.')) {
            // 提取数字部分（处理类似 "1-alpha" 这样的情况）
            std::string numberPart;
            for (char c : part) {
                if (std::isdigit(c)) {
                    numberPart += c;
                } else {
                    break; // 遇到非数字字符就停止
                }
            }
            
            try {
                if (!numberPart.empty()) {
                    result.push_back(std::stoi(numberPart));
                }
            } catch (...) {
                result.push_back(0);
            }
        }
        
        // 确保至少有一个版本号
        if (result.empty()) {
            result.push_back(0);
        }
        
        return result;
    }
    
public:
    // 从GitHub releases下载最新版本
    static bool downloadLatestRelease(const std::string& repoUrl, const std::string& downloadPath, std::string& outDownloadUrl) {
        // 首先获取最新版本信息
        ReleaseInfo info = checkForUpdate(repoUrl);
        if (!info.success) {
            std::cerr << "无法获取版本信息: " << info.errorMessage << std::endl;
            return false;
        }
        
        // 解析仓库地址
        std::string owner, repo;
        if (!parseRepoUrl(repoUrl, owner, repo)) {
            std::cerr << "无法解析仓库地址" << std::endl;
            return false;
        }
        
        // 构建下载URL (假设exe文件名为 aries_agent.exe)
        std::string downloadUrl = "https://github.com/" + owner + "/" + repo + "/releases/download/" + info.version + "/aries_agent.exe";
        outDownloadUrl = downloadUrl;
        
        std::cout << "正在下载最新版本: " << info.version << std::endl;
        std::cout << "下载地址: " << downloadUrl << std::endl;
        
        // 下载文件
        return downloadFile(downloadUrl, downloadPath);
    }
    
    // 下载文件
    static bool downloadFile(const std::string& url, const std::string& savePath) {
        HINTERNET hInternet = InternetOpenA("Open-Aries-AI-Updater/1.0", 
                                            INTERNET_OPEN_TYPE_PRECONFIG, 
                                            NULL, NULL, 0);
        if (!hInternet) {
            std::cerr << "无法初始化网络连接" << std::endl;
            return false;
        }
        
        // 解析URL
        std::string server, path;
        if (!parseDownloadUrl(url, server, path)) {
            std::cerr << "无法解析下载地址" << std::endl;
            InternetCloseHandle(hInternet);
            return false;
        }
        
        HINTERNET hConnect = InternetConnectA(hInternet, 
                                              server.c_str(), 
                                              INTERNET_DEFAULT_HTTPS_PORT, 
                                              NULL, NULL, 
                                              INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) {
            std::cerr << "无法连接到服务器" << std::endl;
            InternetCloseHandle(hInternet);
            return false;
        }
        
        HINTERNET hRequest = HttpOpenRequestA(hConnect, 
                                              "GET", 
                                              path.c_str(), 
                                              NULL, NULL, NULL, 
                                              INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
        if (!hRequest) {
            std::cerr << "无法创建请求" << std::endl;
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return false;
        }
        
        // 添加User-Agent头
        HttpAddRequestHeadersA(hRequest, 
                               "User-Agent: Open-Aries-AI-Updater\r\n", 
                               -1, HTTP_ADDREQ_FLAG_ADD);
        
        BOOL sent = HttpSendRequestA(hRequest, NULL, 0, NULL, 0);
        if (!sent) {
            std::cerr << "无法发送请求" << std::endl;
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return false;
        }
        
        // 检查HTTP状态码
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL);
        
        if (statusCode != 200) {
            std::cerr << "服务器返回错误状态码: " << statusCode << std::endl;
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return false;
        }
        
        // 创建文件
        std::ofstream outFile(savePath, std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "无法创建文件: " << savePath << std::endl;
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return false;
        }
        
        // 下载数据
        char buffer[8192];
        DWORD bytesRead;
        DWORD totalBytes = 0;
        
        std::cout << "下载中..." << std::endl;
        
        while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            outFile.write(buffer, bytesRead);
            totalBytes += bytesRead;
            std::cout << "\r已下载: " << (totalBytes / 1024) << " KB" << std::flush;
        }
        
        std::cout << std::endl;
        std::cout << "下载完成: " << (totalBytes / 1024) << " KB" << std::endl;
        
        outFile.close();
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        
        return totalBytes > 0;
    }
    
    // 创建更新脚本（用于替换正在运行的exe）
    static bool createUpdateScript(const std::string& currentExePath, const std::string& newExePath, const std::string& scriptPath) {
        std::ofstream script(scriptPath);
        if (!script.is_open()) {
            return false;
        }
        
        // 创建PowerShell脚本
        script << "# Open-Aries-AI 更新脚本" << std::endl;
        script << "$currentExe = \"" << currentExePath << "\"" << std::endl;
        script << "$newExe = \"" << newExePath << "\"" << std::endl;
        script << "$backupExe = \"" << currentExePath + ".backup" << "\"" << std::endl;
        script << std::endl;
        script << "# 等待原程序退出" << std::endl;
        script << "Start-Sleep -Seconds 2" << std::endl;
        script << std::endl;
        script << "# 备份原文件" << std::endl;
        script << "if (Test-Path $currentExe) {" << std::endl;
        script << "    Move-Item -Path $currentExe -Destination $backupExe -Force" << std::endl;
        script << "}" << std::endl;
        script << std::endl;
        script << "# 替换为新版本" << std::endl;
        script << "Move-Item -Path $newExe -Destination $currentExe -Force" << std::endl;
        script << std::endl;
        script << "# 启动新版本" << std::endl;
        script << "Start-Process -FilePath $currentExe" << std::endl;
        script << std::endl;
        script << "# 删除备份（可选）" << std::endl;
        script << "# Remove-Item -Path $backupExe -Force" << std::endl;
        script << std::endl;
        script << "# 删除自身" << std::endl;
        script << "Remove-Item -Path \"" << scriptPath << "\" -Force" << std::endl;
        
        script.close();
        return true;
    }
    
    // 执行更新
    static bool performUpdate(const std::string& repoUrl, const std::string& currentExePath) {
        std::string tempDownloadPath = currentExePath + ".new";
        std::string scriptPath = currentExePath + "_update.ps1";
        std::string downloadUrl;
        
        // 下载新版本
        if (!downloadLatestRelease(repoUrl, tempDownloadPath, downloadUrl)) {
            // 删除临时文件
            DeleteFileA(tempDownloadPath.c_str());
            return false;
        }
        
        // 创建更新脚本
        if (!createUpdateScript(currentExePath, tempDownloadPath, scriptPath)) {
            std::cerr << "无法创建更新脚本" << std::endl;
            DeleteFileA(tempDownloadPath.c_str());
            return false;
        }
        
        std::cout << "更新准备完成，正在启动更新程序..." << std::endl;
        std::cout << "程序将自动重启" << std::endl;
        
        // 启动PowerShell执行更新脚本
        std::string cmd = "powershell.exe -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + scriptPath + "\"";
        
        STARTUPINFOA si = {0};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {0};
        
        BOOL created = CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, 
                                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        
        if (created) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        } else {
            std::cerr << "无法启动更新程序" << std::endl;
            DeleteFileA(tempDownloadPath.c_str());
            DeleteFileA(scriptPath.c_str());
            return false;
        }
    }
    
private:
    static bool parseDownloadUrl(const std::string& url, std::string& server, std::string& path) {
        // 解析 https://github.com/... 格式的URL
        if (url.find("https://") != 0) {
            return false;
        }
        
        size_t serverStart = 8; // 跳过 "https://"
        size_t pathStart = url.find('/', serverStart);
        
        if (pathStart == std::string::npos) {
            return false;
        }
        
        server = url.substr(serverStart, pathStart - serverStart);
        path = url.substr(pathStart);
        
        return !server.empty() && !path.empty();
    }
};

} // namespace aries
