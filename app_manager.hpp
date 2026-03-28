#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <regex>
#include <locale>
#include <codecvt>

namespace aries {

struct InstalledApp {
    std::string name;
    std::string displayName;
    std::string installLocation;
    std::string executablePath;
    std::string uninstallString;
};

class AppManager {
public:
    // 获取所有已安装的应用
    static std::vector<InstalledApp> getInstalledApps() {
        std::vector<InstalledApp> apps;
        
        // 从注册表获取已安装应用
        getAppsFromRegistry(HKEY_LOCAL_MACHINE, 
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", apps);
        getAppsFromRegistry(HKEY_LOCAL_MACHINE, 
            L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", apps);
        getAppsFromRegistry(HKEY_CURRENT_USER, 
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", apps);
        
        // 添加一些常见的 Windows 应用
        addCommonWindowsApps(apps);
        
        return apps;
    }
    
    // 查找应用
    static InstalledApp* findApp(std::vector<InstalledApp>& apps, const std::string& query) {
        // 先精确匹配
        for (auto& app : apps) {
            if (app.name == query || app.displayName == query) {
                return &app;
            }
        }
        
        // 再模糊匹配
        std::string lowerQuery = toLower(query);
        for (auto& app : apps) {
            if (toLower(app.name).find(lowerQuery) != std::string::npos ||
                toLower(app.displayName).find(lowerQuery) != std::string::npos) {
                return &app;
            }
        }
        
        return nullptr;
    }
    
    // 获取应用列表的字符串表示（用于上报给AI）
    static std::string getAppsListString(const std::vector<InstalledApp>& apps) {
        std::stringstream ss;
        ss << "# 已安装的应用列表\n\n";
        
        int count = 0;
        for (const auto& app : apps) {
            if (!app.displayName.empty() && !app.executablePath.empty()) {
                ss << "- " << app.displayName;
                if (!app.executablePath.empty()) {
                    ss << " (可执行: " << app.executablePath << ")";
                }
                ss << "\n";
                count++;
                if (count >= 100) break; // 限制数量避免过长
            }
        }
        
        ss << "\n共 " << count << " 个应用\n";
        return ss.str();
    }
    
    // 启动应用
    static bool launchApp(const InstalledApp& app) {
        if (!app.executablePath.empty()) {
            return launchExecutable(app.executablePath);
        }
        
        // 尝试从安装位置查找可执行文件
        if (!app.installLocation.empty()) {
            std::string exePath = findExecutableInDirectory(app.installLocation);
            if (!exePath.empty()) {
                return launchExecutable(exePath);
            }
        }
        
        return false;
    }
    
    // 通过名称启动应用
    static bool launchAppByName(const std::string& name) {
        auto apps = getInstalledApps();
        auto* app = findApp(apps, name);
        if (app) {
            return launchApp(*app);
        }
        return false;
    }

private:
    static std::string toLower(const std::string& str) {
        std::string result = str;
        for (auto& c : result) {
            c = std::tolower(c);
        }
        return result;
    }
    
    // 宽字符转UTF-8
    static std::string wstringToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string str(size_needed - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, NULL, NULL);
        return str;
    }
    
    static void getAppsFromRegistry(HKEY rootKey, const std::wstring& subKey, 
                                     std::vector<InstalledApp>& apps) {
        HKEY hKey;
        if (RegOpenKeyExW(rootKey, subKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
            return;
        }
        
        wchar_t keyName[256];
        DWORD keyNameSize;
        DWORD index = 0;
        
        while (true) {
            keyNameSize = sizeof(keyName) / sizeof(keyName[0]);
            LONG result = RegEnumKeyExW(hKey, index, keyName, &keyNameSize, 
                                         NULL, NULL, NULL, NULL);
            if (result != ERROR_SUCCESS) break;
            
            HKEY hSubKey;
            std::wstring fullSubKey = subKey + L"\\" + keyName;
            if (RegOpenKeyExW(rootKey, fullSubKey.c_str(), 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                InstalledApp app;
                app.name = wstringToUtf8(keyName);
                app.displayName = wstringToUtf8(getRegistryStringW(hSubKey, L"DisplayName"));
                app.installLocation = wstringToUtf8(getRegistryStringW(hSubKey, L"InstallLocation"));
                app.executablePath = wstringToUtf8(getRegistryStringW(hSubKey, L"DisplayIcon"));
                app.uninstallString = wstringToUtf8(getRegistryStringW(hSubKey, L"UninstallString"));
                
                // 从 DisplayIcon 提取可执行路径
                if (!app.executablePath.empty()) {
                    // DisplayIcon 可能包含路径和图标索引，如 "C:\app.exe,0"
                    size_t commaPos = app.executablePath.find(',');
                    if (commaPos != std::string::npos) {
                        app.executablePath = app.executablePath.substr(0, commaPos);
                    }
                }
                
                // 如果 DisplayIcon 为空，尝试从 InstallLocation 推断
                if (app.executablePath.empty() && !app.installLocation.empty()) {
                    app.executablePath = findExecutableInDirectory(app.installLocation);
                }
                
                if (!app.displayName.empty()) {
                    apps.push_back(app);
                }
                
                RegCloseKey(hSubKey);
            }
            
            index++;
        }
        
        RegCloseKey(hKey);
    }
    
    static std::wstring getRegistryStringW(HKEY hKey, const std::wstring& valueName) {
        wchar_t buffer[1024];
        DWORD bufferSize = sizeof(buffer);
        DWORD type;
        
        if (RegQueryValueExW(hKey, valueName.c_str(), NULL, &type, 
                             (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            if (type == REG_SZ || type == REG_EXPAND_SZ) {
                return std::wstring(buffer);
            }
        }
        return L"";
    }
    
    static void addCommonWindowsApps(std::vector<InstalledApp>& apps) {
        std::vector<std::pair<std::string, std::string>> commonApps = {
            {"任务管理器", "taskmgr"},
            {"计算器", "calc"},
            {"记事本", "notepad"},
            {"画图", "mspaint"},
            {"命令提示符", "cmd"},
            {"PowerShell", "powershell"},
            {"Windows PowerShell", "powershell"},
            {"资源管理器", "explorer"},
            {"文件资源管理器", "explorer"},
            {"控制面板", "control"},
            {"设置", "ms-settings:"},
            {"Microsoft Edge", "msedge"},
            {"Edge", "msedge"},
            {"Google Chrome", "chrome"},
            {"Chrome", "chrome"},
            {"Firefox", "firefox"},
            {"微信", "WeChat"},
            {"QQ", "QQ"},
            {"钉钉", "DingTalk"},
            {"企业微信", "WXWork"},
            {"VS Code", "code"},
            {"Visual Studio Code", "code"},
            {"Steam", "steam"}
        };
        
        for (const auto& [name, exe] : commonApps) {
            InstalledApp app;
            app.name = exe;
            app.displayName = name;
            app.executablePath = exe;
            apps.push_back(app);
        }
    }
    
    static std::string findExecutableInDirectory(const std::string& directory) {
        WIN32_FIND_DATAA findData;
        HANDLE hFind;
        std::string searchPath = directory + "\\*.exe";
        
        hFind = FindFirstFileA(searchPath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            std::string exePath = directory + "\\" + findData.cFileName;
            FindClose(hFind);
            return exePath;
        }
        
        return "";
    }
    
    static bool launchExecutable(const std::string& exePath) {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        
        BOOL success = CreateProcessA(
            NULL,
            const_cast<char*>(exePath.c_str()),
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            NULL,
            &si,
            &pi
        );
        
        if (success) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        }
        
        return false;
    }
};

} // namespace aries
