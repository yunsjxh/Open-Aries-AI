#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>
#include <shlobj.h>

namespace aries {

struct SecurityConfig {
    bool allowExecute = false;
    bool allowFileWrite = false;
    bool allowFileDelete = false;
    bool allowFileRun = false;
    bool requireHighRiskConfirmation = true;
    bool allowInsecureHttps = false;  // only for dev/testing — skips SSL cert validation
    bool loadedFromFile = false;

    // Path access control
    std::vector<std::wstring> blockedPaths;  // paths the agent must NOT access
    std::vector<std::wstring> allowedPaths;  // paths the agent IS allowed to access (empty = use defaults)
};

class SecurityConfigLoader {
public:
    static SecurityConfig loadFromFileAndEnv(const std::string& filePath = "aries_config.json") {
        SecurityConfig config;

        std::ifstream in(filePath);
        if (in.is_open()) {
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            config.allowExecute = readBool(content, "allowExecute", config.allowExecute);
            config.allowFileWrite = readBool(content, "allowFileWrite", config.allowFileWrite);
            config.allowFileDelete = readBool(content, "allowFileDelete", config.allowFileDelete);
            config.allowFileRun = readBool(content, "allowFileRun", config.allowFileRun);
            config.requireHighRiskConfirmation = readBool(content, "requireHighRiskConfirmation", config.requireHighRiskConfirmation);
            config.allowInsecureHttps = readBool(content, "allowInsecureHttps", config.allowInsecureHttps);

            // Read path arrays
            readStringArray(content, "blockedPaths", config.blockedPaths);
            readStringArray(content, "allowedPaths", config.allowedPaths);

            config.loadedFromFile = true;
        }

        applyEnv(config.allowExecute, "ARIES_ALLOW_EXECUTE");
        applyEnv(config.allowFileWrite, "ARIES_ALLOW_FILE_WRITE");
        applyEnv(config.allowFileDelete, "ARIES_ALLOW_FILE_DELETE");
        applyEnv(config.allowFileRun, "ARIES_ALLOW_FILE_RUN");
        applyEnv(config.requireHighRiskConfirmation, "ARIES_REQUIRE_HIGH_RISK_CONFIRMATION");
        applyEnv(config.allowInsecureHttps, "ARIES_ALLOW_INSECURE_HTTPS");

        return config;
    }

private:
    static bool parseBool(const std::string& value, bool fallback) {
        std::string v = value;
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
        if (v == "0" || v == "false" || v == "no" || v == "off") return false;
        return fallback;
    }

    static bool readBool(const std::string& content, const std::string& key, bool fallback) {
        std::string token = "\"" + key + "\"";
        std::size_t keyPos = content.find(token);
        if (keyPos == std::string::npos) return fallback;

        std::size_t colonPos = content.find(':', keyPos + token.size());
        if (colonPos == std::string::npos) return fallback;

        std::size_t valueStart = content.find_first_not_of(" \t\r\n", colonPos + 1);
        if (valueStart == std::string::npos) return fallback;

        if (content.compare(valueStart, 4, "true") == 0) return true;
        if (content.compare(valueStart, 5, "false") == 0) return false;
        return fallback;
    }

    static void readStringArray(const std::string& content, const std::string& key,
                                 std::vector<std::wstring>& out) {
        std::string token = "\"" + key + "\"";
        std::size_t keyPos = content.find(token);
        if (keyPos == std::string::npos) return;

        std::size_t colonPos = content.find(':', keyPos + token.size());
        if (colonPos == std::string::npos) return;

        std::size_t bracketPos = content.find('[', colonPos + 1);
        if (bracketPos == std::string::npos) return;

        std::size_t closePos = content.find(']', bracketPos + 1);
        if (closePos == std::string::npos) return;

        std::size_t pos = bracketPos + 1;
        while (pos < closePos) {
            std::size_t qStart = content.find('"', pos);
            if (qStart == std::string::npos || qStart >= closePos) break;
            std::size_t qEnd = content.find('"', qStart + 1);
            if (qEnd == std::string::npos || qEnd >= closePos) break;

            std::string path = content.substr(qStart + 1, qEnd - qStart - 1);
            if (!path.empty()) {
                int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
                std::wstring wpath(wlen - 1, 0);
                MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);
                out.push_back(wpath);
            }
            pos = qEnd + 1;
        }
    }

    static void applyEnv(bool& target, const char* envName) {
        const char* raw = std::getenv(envName);
        if (!raw) return;
        target = parseBool(raw, target);
    }
};

// === Path validation ===

// Get canonical path (resolves . and .. to absolute, normalized form)
inline std::wstring canonicalPath(const std::wstring& path) {
    wchar_t buf[MAX_PATH];
    DWORD len = GetFullPathNameW(path.c_str(), MAX_PATH, buf, nullptr);
    if (len == 0 || len > MAX_PATH) return path;
    std::wstring result(buf, len);
    // Ensure consistent trailing-backslash handling
    for (auto& c : result) c = towlower(c);
    return result;
}

// Check if a path starts with a given prefix (case-insensitive on Windows)
inline bool pathStartsWith(const std::wstring& path, const std::wstring& prefix) {
    if (path.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); i++) {
        if (towlower(path[i]) != towlower(prefix[i])) return false;
    }
    return true;
}

// Default blocked directories — system-critical paths
inline std::vector<std::wstring> getDefaultBlockedPaths() {
    std::vector<std::wstring> blocked;
    wchar_t sysDir[MAX_PATH];

    if (GetSystemDirectoryW(sysDir, MAX_PATH)) {
        blocked.push_back(sysDir);                          // C:\Windows\System32
        // Parent = Windows dir
        std::wstring winDir(sysDir);
        size_t slash = winDir.rfind(L'\\');
        if (slash != std::wstring::npos) {
            winDir = winDir.substr(0, slash);
            blocked.push_back(winDir);                      // C:\Windows
        }
    }
    // SysWOW64
    if (GetSystemWow64DirectoryW(sysDir, MAX_PATH)) {
        blocked.push_back(sysDir);
    }

    // Program Files
    wchar_t progDir[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, progDir))) {
        blocked.push_back(progDir);
    }
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr, 0, progDir))) {
        blocked.push_back(progDir);
    }

    // System drive root
    wchar_t sysDrive[4] = {0};
    GetEnvironmentVariableW(L"SystemDrive", sysDrive, 4);
    if (sysDrive[0]) {
        blocked.push_back(std::wstring(sysDrive) + L"\\Boot");
        blocked.push_back(std::wstring(sysDrive) + L"\\Recovery");
        blocked.push_back(std::wstring(sysDrive) + L"\\EFI");
        blocked.push_back(std::wstring(sysDrive) + L"\\$Recycle.Bin");
    }

    return blocked;
}

// Default allowed directories — user data and app directory
inline std::vector<std::wstring> getDefaultAllowedPaths() {
    std::vector<std::wstring> allowed;
    wchar_t path[MAX_PATH];

    // User profile directories
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, path))) {
        allowed.push_back(path);  // C:\Users\<name>
    }
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, path))) {
        allowed.push_back(path);
    }
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, path))) {
        allowed.push_back(path);  // Documents
    }

    // App directory
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *slash = 0;
    allowed.push_back(path);

    // Temp directory (limited scope: only for app's own use)
    wchar_t tmpDir[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tmpDir)) {
        allowed.push_back(tmpDir);
    }

    return allowed;
}

enum class PathValidationResult {
    Ok,
    Blocked,          // Path is in the blocked list
    NotAllowed,       // Path is not in any allowed directory
    PathTraversal,    // Contains .. or suspicious patterns
    Invalid,          // Empty or malformed
    TooLong           // Path exceeds MAX_PATH
};

struct PathValidation {
    PathValidationResult result = PathValidationResult::Ok;
    std::string reason;
    std::wstring normalizedPath;
};

inline PathValidation validatePath(const std::wstring& rawPath,
                                    const SecurityConfig& config) {
    PathValidation pv;

    if (rawPath.empty()) {
        pv.result = PathValidationResult::Invalid;
        pv.reason = "路径为空";
        return pv;
    }

    // Quick length check
    if (rawPath.size() > 32767) {
        pv.result = PathValidationResult::TooLong;
        pv.reason = "路径过长";
        return pv;
    }

    // Path traversal check (before normalization)
    if (rawPath.find(L"..") != std::wstring::npos) {
        pv.result = PathValidationResult::PathTraversal;
        pv.reason = "路径包含 '..' 字符，禁止路径遍历";
        return pv;
    }

    // Suspicious patterns
    const wchar_t* suspicious[] = {L"\\\\?\\", L"\\\\.\\", L"COM", L"CON", L"AUX",
                                    L"NUL", L"PRN", L"LPT"};
    for (auto& s : suspicious) {
        if (rawPath.find(s) != std::wstring::npos) {
            pv.result = PathValidationResult::PathTraversal;
            pv.reason = "路径包含特殊设备名或前缀";
            return pv;
        }
    }

    // Normalize
    pv.normalizedPath = canonicalPath(rawPath);

    // Build blocked list (default + user config)
    auto blocked = getDefaultBlockedPaths();
    for (auto& bp : config.blockedPaths) blocked.push_back(bp);

    // Check blocked
    for (auto& b : blocked) {
        std::wstring bNorm = canonicalPath(b);
        if (pathStartsWith(pv.normalizedPath, bNorm)) {
            pv.result = PathValidationResult::Blocked;
            pv.reason = "拒绝访问：此目录受保护";
            return pv;
        }
    }

    // Build allowed list (default + user config)
    auto allowed = getDefaultAllowedPaths();
    for (auto& ap : config.allowedPaths) allowed.push_back(ap);

    // If allowedPaths is explicitly set in config, only use those (override defaults)
    if (!config.allowedPaths.empty()) {
        allowed = config.allowedPaths;
    }

    // Check allowed
    for (auto& a : allowed) {
        std::wstring aNorm = canonicalPath(a);
        if (pathStartsWith(pv.normalizedPath, aNorm)) {
            pv.result = PathValidationResult::Ok;
            return pv;
        }
    }

    pv.result = PathValidationResult::NotAllowed;
    pv.reason = "拒绝访问：路径不在允许的目录范围内";
    return pv;
}

} // namespace aries
