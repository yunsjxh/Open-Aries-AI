#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <regex>
#include <locale>
#include <codecvt>
#include <algorithm>
#include <cstdio>

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
        std::cerr << "[调试] findApp: 查找应用 '" << query << "'，应用列表大小: " << apps.size() << std::endl;
        
        // 先精确匹配
        for (auto& app : apps) {
            if (app.name == query || app.displayName == query) {
                std::cerr << "[调试] findApp: 精确匹配成功: '" << app.displayName << "' (name='" << app.name << "')" << std::endl;
                std::cerr << "[调试] findApp:   executablePath='" << app.executablePath << "'" << std::endl;
                std::cerr << "[调试] findApp:   installLocation='" << app.installLocation << "'" << std::endl;
                return &app;
            }
        }
        
        // 再模糊匹配
        std::string lowerQuery = toLower(query);
        std::cerr << "[调试] findApp: 尝试模糊匹配，lowerQuery='" << lowerQuery << "'" << std::endl;
        for (auto& app : apps) {
            std::string lowerName = toLower(app.name);
            std::string lowerDisplayName = toLower(app.displayName);
            if (lowerName.find(lowerQuery) != std::string::npos ||
                lowerDisplayName.find(lowerQuery) != std::string::npos) {
                std::cerr << "[调试] findApp: 模糊匹配成功: '" << app.displayName << "' (name='" << app.name << "')" << std::endl;
                std::cerr << "[调试] findApp:   executablePath='" << app.executablePath << "'" << std::endl;
                std::cerr << "[调试] findApp:   installLocation='" << app.installLocation << "'" << std::endl;
                return &app;
            }
        }
        
        std::cerr << "[调试] findApp: 未找到匹配的应用 '" << query << "'" << std::endl;
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
        std::cerr << "[调试] launchApp: 尝试启动应用 '" << app.displayName << "' (名称: '" << app.name << "')" << std::endl;
        std::cerr << "[调试] launchApp: executablePath='" << app.executablePath << "'" << std::endl;
        std::cerr << "[调试] launchApp: installLocation='" << app.installLocation << "'" << std::endl;
        std::cerr << "[调试] launchApp: uninstallString='" << app.uninstallString << "'" << std::endl;
        
        // 首先检查是否是 UWP 应用
        if (!app.executablePath.empty()) {
            if (app.executablePath.find("WindowsApps") != std::string::npos ||
                app.executablePath.find("Microsoft.WindowsStore") != std::string::npos) {
                std::cerr << "[调试] launchApp: 检测到 UWP 应用，使用 launchUWPApp" << std::endl;
                return launchUWPApp(app.name);
            }
        }
        
        // 如果 executablePath 是 .ico 文件（图标），需要找到真正的可执行文件
        if (!app.executablePath.empty() && app.executablePath.find(".ico") != std::string::npos) {
            std::cerr << "[调试] launchApp: executablePath 是图标文件，尝试找到真正的可执行文件" << std::endl;
            
            // 从 uninstallString 推断安装目录
            if (!app.uninstallString.empty()) {
                std::string installDir = extractDirectoryFromPath(app.uninstallString);
                std::cerr << "[调试] launchApp: 从 uninstallString 推断安装目录: " << installDir << std::endl;
                
                if (!installDir.empty()) {
                    // 直接查找主程序（排除卸载程序）
                    std::string mainExe = findMainExecutable(installDir, app.uninstallString);
                    if (!mainExe.empty()) {
                        std::cerr << "[调试] launchApp: 找到主程序: " << mainExe << std::endl;
                        if (launchExecutable(mainExe)) {
                            std::cerr << "[调试] launchApp: 启动成功" << std::endl;
                            return true;
                        }
                    }
                    
                    // 如果上面的方法失败，尝试查找任何可执行文件（但排除卸载程序）
                    std::cerr << "[调试] launchApp: 尝试查找任何可执行文件（排除卸载程序）" << std::endl;
                    std::string exePath = findExecutableInDirectoryExcludingUninstall(installDir, app.uninstallString);
                    if (!exePath.empty()) {
                        std::cerr << "[调试] launchApp: 找到可执行文件: " << exePath << std::endl;
                        if (launchExecutable(exePath)) {
                            std::cerr << "[调试] launchApp: 启动成功" << std::endl;
                            return true;
                        }
                    }
                }
            }
        }
        
        // 正常启动流程
        if (!app.executablePath.empty()) {
            // 检查是否是 UWP 应用（路径包含 WindowsApps）
            if (app.executablePath.find("WindowsApps") != std::string::npos ||
                app.executablePath.find("Microsoft.WindowsStore") != std::string::npos) {
                std::cerr << "[调试] launchApp: 检测到 UWP 应用，使用 launchUWPApp" << std::endl;
                return launchUWPApp(app.name);
            }
            std::cerr << "[调试] launchApp: 尝试启动 executablePath: " << app.executablePath << std::endl;
            if (launchExecutable(app.executablePath)) {
                std::cerr << "[调试] launchApp: 通过 executablePath 启动成功" << std::endl;
                return true;
            }
            std::cerr << "[调试] launchApp: 通过 executablePath 启动失败" << std::endl;
        }
        
        // 尝试从安装位置查找可执行文件
        if (!app.installLocation.empty()) {
            std::cerr << "[调试] launchApp: 尝试从 installLocation 查找可执行文件" << std::endl;
            std::string exePath = findExecutableInDirectory(app.installLocation);
            if (!exePath.empty()) {
                std::cerr << "[调试] launchApp: 找到可执行文件: " << exePath << std::endl;
                return launchExecutable(exePath);
            }
            std::cerr << "[调试] launchApp: 在 installLocation 中未找到可执行文件" << std::endl;
        }
        
        // 从 uninstallString 推断安装目录并查找
        if (!app.uninstallString.empty()) {
            std::string installDir = extractDirectoryFromPath(app.uninstallString);
            if (!installDir.empty()) {
                std::cerr << "[调试] launchApp: 尝试从 uninstallString 目录查找: " << installDir << std::endl;
                std::string exePath = findExecutableInDirectory(installDir);
                if (!exePath.empty()) {
                    std::cerr << "[调试] launchApp: 找到可执行文件: " << exePath << std::endl;
                    return launchExecutable(exePath);
                }
            }
        }
        
        // 最后尝试通过名称启动（可能是命令行工具或 UWP 应用）
        std::cerr << "[调试] launchApp: 尝试通过名称启动" << std::endl;
        if (launchByName(app.name)) {
            std::cerr << "[调试] launchApp: 通过名称启动成功" << std::endl;
            return true;
        }
        std::cerr << "[调试] launchApp: 通过名称启动失败，尝试 launchUWPApp" << std::endl;
        return launchUWPApp(app.name);
    }
    
    static bool launchByName(const std::string& name) {
        // 尝试直接启动
        if (launchExecutable(name)) {
            return true;
        }
        
        // 尝试用 where 命令查找
        std::string cmd = "where " + name + " 2>nul";
        char buffer[512];
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string path(buffer);
                // 去除换行符
                path.erase(std::remove(path.begin(), path.end(), '\n'), path.end());
                path.erase(std::remove(path.begin(), path.end(), '\r'), path.end());
                _pclose(pipe);
                if (!path.empty()) {
                    return launchExecutable(path);
                }
            }
            _pclose(pipe);
        }
        
        return false;
    }
    
    // 从完整路径中提取目录
    static std::string extractDirectoryFromPath(const std::string& fullPath) {
        size_t lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return fullPath.substr(0, lastSlash);
        }
        return "";
    }
    
    // 宽字符转UTF-8字符串
    static std::string wideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, NULL, NULL);
        return result;
    }
    
    // UTF-8字符串转宽字符
    static std::wstring utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
        return result;
    }
    
    // 宽字符转ANSI字符串（用于控制台显示）
    static std::string wideToAnsi(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &result[0], size, NULL, NULL);
        return result;
    }
    
    // ANSI字符串转宽字符
    static std::wstring ansiToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &result[0], size);
        return result;
    }
    
    // 获取目录中的所有可执行文件列表（使用宽字符版本，返回UTF-8编码）
    static std::vector<std::pair<std::string, std::string>> getAllExecutablesInDirectory(const std::string& directory) {
        std::vector<std::pair<std::string, std::string>> executables; // {显示名称, 完整路径}
        WIN32_FIND_DATAW findData;
        HANDLE hFind;
        
        // 将目录路径转换为宽字符
        std::wstring wDirectory = ansiToWide(directory);
        std::wstring wSearchPath = wDirectory + L"\\*.exe";
        
        hFind = FindFirstFileW(wSearchPath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                // 获取宽字符文件名
                std::wstring wFileName = findData.cFileName;
                
                // 构建完整宽字符路径
                std::wstring wFullPath = wDirectory + L"\\" + wFileName;
                
                // 将文件名转换为UTF-8（用于显示给AI）
                std::string fileNameUtf8 = wideToUtf8(wFileName);
                
                // 将完整路径转换为UTF-8（用于发送给AI）
                std::string fullPathUtf8 = wideToUtf8(wFullPath);
                
                executables.push_back({fileNameUtf8, fullPathUtf8});
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
        
        return executables;
    }
    
    static bool launchUWPApp(const std::string& appName) {
        std::cerr << "[调试] launchUWPApp: 尝试启动 UWP 应用 '" << appName << "'" << std::endl;
        
        // 首先尝试查找正确的 UWP 应用 ID
        std::string uwpAppId = findUWPAppId(appName);
        std::string targetApp = uwpAppId.empty() ? appName : uwpAppId;
        
        std::cerr << "[调试] launchUWPApp: appName='" << appName << "', uwpAppId='" << uwpAppId << "', targetApp='" << targetApp << "'" << std::endl;
        
        // 方式1: 使用 start 命令启动 UWP 应用
        std::string startCmd = "cmd /c start \"\" \"shell:AppsFolder\\" + targetApp + "\"";
        std::cerr << "[调试] launchUWPApp: 尝试方式1: " << startCmd << std::endl;
        if (launchExecutable(startCmd)) {
            std::cerr << "[调试] launchUWPApp: 方式1成功" << std::endl;
            return true;
        }
        std::cerr << "[调试] launchUWPApp: 方式1失败" << std::endl;
        
        // 方式2: 使用 explorer 启动
        std::string explorerCmd = "explorer.exe shell:AppsFolder\\" + targetApp;
        std::cerr << "[调试] launchUWPApp: 尝试方式2: " << explorerCmd << std::endl;
        if (launchExecutable(explorerCmd)) {
            std::cerr << "[调试] launchUWPApp: 方式2成功" << std::endl;
            return true;
        }
        std::cerr << "[调试] launchUWPApp: 方式2失败" << std::endl;
        
        // 方式3: 直接使用 ShellExecute
        std::string shellCmd = "shell:AppsFolder\\" + targetApp;
        std::cerr << "[调试] launchUWPApp: 尝试方式3: ShellExecute with '" << shellCmd << "'" << std::endl;
        HINSTANCE result = ShellExecuteA(NULL, "open", shellCmd.c_str(), NULL, NULL, SW_SHOWNORMAL);
        bool success = (reinterpret_cast<INT_PTR>(result) > 32);
        std::cerr << "[调试] launchUWPApp: 方式3结果: " << (success ? "成功" : "失败") << " (result=" << reinterpret_cast<INT_PTR>(result) << ")" << std::endl;
        return success;
    }
    
    // 查找 UWP 应用 ID
    static std::string findUWPAppId(const std::string& appName) {
        std::cerr << "[调试] findUWPAppId: 查找应用 '" << appName << "'" << std::endl;
        
        // 常见 UWP 应用映射表
        static const std::map<std::string, std::string> uwpAppMap = {
            {"哔哩哔哩", "Bilibili.\u54d4\u54e9\u54d4\u54e9_uwp!App"},
            {"bilibili", "Bilibili.\u54d4\u54e9\u54d4\u54e9_uwp!App"},
            {"Bilibili", "Bilibili.\u54d4\u54e9\u54d4\u54e9_uwp!App"},
            {"微信", "TencentWeChat.WeChatUWP_wekyb3d8bbwe!App"},
            {"wechat", "TencentWeChat.WeChatUWP_wekyb3d8bbwe!App"},
            {"QQ", "TencentQQ.UWP_9d6p1p5q2z3z0!App"},
            {"qq", "TencentQQ.UWP_9d6p1p5q2z3z0!App"},
            {"网易云音乐", "NetEase.CloudMusic_6r8b5d5k8q8q8!App"},
            {"音乐", "Microsoft.ZuneMusic_8wekyb3d8bbwe!Microsoft.ZuneMusic"},
            {"视频", "Microsoft.ZuneVideo_8wekyb3d8bbwe!Microsoft.ZuneVideo"},
            {"照片", "Microsoft.Windows.Photos_8wekyb3d8bbwe!App"},
            {"计算器", "Microsoft.WindowsCalculator_8wekyb3d8bbwe!App"},
            {"设置", "windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft.windows.immersivecontrolpanel"},
            {"商店", "Microsoft.WindowsStore_8wekyb3d8bbwe!App"},
            {"Microsoft Store", "Microsoft.WindowsStore_8wekyb3d8bbwe!App"}
        };
        
        // 直接匹配
        auto it = uwpAppMap.find(appName);
        if (it != uwpAppMap.end()) {
            std::cerr << "[调试] findUWPAppId: 直接匹配成功: " << it->second << std::endl;
            return it->second;
        }
        
        // 模糊匹配
        std::string lowerAppName = toLower(appName);
        for (const auto& [key, value] : uwpAppMap) {
            if (toLower(key).find(lowerAppName) != std::string::npos ||
                lowerAppName.find(toLower(key)) != std::string::npos) {
                std::cerr << "[调试] findUWPAppId: 模糊匹配成功: " << value << " (匹配键: " << key << ")" << std::endl;
                return value;
            }
        }
        
        std::cerr << "[调试] findUWPAppId: 未找到匹配，返回空" << std::endl;
        return "";
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
    
    // 查找主程序（排除卸载程序）
    static std::string findMainExecutable(const std::string& directory, const std::string& uninstallPath) {
        std::string uninstallExe = extractFileName(uninstallPath);
        std::transform(uninstallExe.begin(), uninstallExe.end(), uninstallExe.begin(), ::tolower);
        
        WIN32_FIND_DATAA findData;
        HANDLE hFind;
        std::string searchPath = directory + "\\*.exe";
        
        hFind = FindFirstFileA(searchPath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                std::string fileName = findData.cFileName;
                std::string lowerFileName = fileName;
                std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);
                
                // 排除卸载程序
                if (lowerFileName.find("uninstall") == std::string::npos &&
                    lowerFileName.find("卸载") == std::string::npos &&
                    lowerFileName != uninstallExe) {
                    std::string exePath = directory + "\\" + fileName;
                    FindClose(hFind);
                    return exePath;
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        return "";
    }
    
    // 从完整路径中提取文件名
    static std::string extractFileName(const std::string& fullPath) {
        size_t lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return fullPath.substr(lastSlash + 1);
        }
        return fullPath;
    }
    
    // 查找可执行文件（排除卸载程序）
    static std::string findExecutableInDirectoryExcludingUninstall(const std::string& directory, const std::string& uninstallPath) {
        std::string uninstallExe = extractFileName(uninstallPath);
        std::transform(uninstallExe.begin(), uninstallExe.end(), uninstallExe.begin(), ::tolower);
        
        WIN32_FIND_DATAA findData;
        HANDLE hFind;
        std::string searchPath = directory + "\\*.exe";
        
        hFind = FindFirstFileA(searchPath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                std::string fileName = findData.cFileName;
                std::string lowerFileName = fileName;
                std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);
                
                // 排除卸载程序
                if (lowerFileName.find("uninstall") == std::string::npos &&
                    lowerFileName.find("卸载") == std::string::npos &&
                    lowerFileName != uninstallExe) {
                    std::string exePath = directory + "\\" + fileName;
                    FindClose(hFind);
                    return exePath;
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        return "";
    }
    
    static bool launchExecutable(const std::string& exePath) {
        // 先尝试用 CreateProcess 启动
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        
        BOOL success = CreateProcessA(
            exePath.c_str(),
            NULL,
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
        
        // 如果失败，尝试用 ShellExecute 启动（支持 UWP 应用）
        HINSTANCE result = ShellExecuteA(NULL, "open", exePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return (reinterpret_cast<INT_PTR>(result) > 32);
    }
};

} // namespace aries
