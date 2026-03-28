#include "silicon_flow_client_simple.hpp"
#include "prompt_templates.hpp"
#include "action_parser.hpp"
#include "action_executor.hpp"
#include "app_manager.hpp"
#include "secure_storage.hpp"
#include <iostream>
#include <cstdlib>
#include <windows.h>
#include <gdiplus.h>
#include <conio.h>
#include <fstream>
#include <ctime>
#include <iomanip>
#pragma comment(lib, "gdiplus.lib")

using namespace silicon_flow;
using namespace aries;

const std::string LOG_FILE = "aries_agent.log";

std::string getCurrentTime() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void logMessage(const std::string& message) {
    std::ofstream logFile(LOG_FILE, std::ios::app);
    if (logFile.is_open()) {
        logFile << "[" << getCurrentTime() << "] " << message << std::endl;
        logFile.close();
    }
}

void clearLog() {
    std::ofstream logFile(LOG_FILE, std::ios::trunc);
    if (logFile.is_open()) {
        logFile << "[" << getCurrentTime() << "] 日志已清空" << std::endl;
        logFile.close();
    }
}

void setup_console() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

bool save_bitmap_to_png(HBITMAP hBitmap, const std::string& filepath) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    bool result = false;
    {
        Gdiplus::Bitmap bitmap(hBitmap, NULL);
        CLSID pngClsid;
        
        // 获取 PNG encoder CLSID
        UINT numEncoders = 0;
        UINT size = 0;
        Gdiplus::GetImageEncodersSize(&numEncoders, &size);
        if (size > 0) {
            Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)malloc(size);
            Gdiplus::GetImageEncoders(numEncoders, size, pImageCodecInfo);
            
            for (UINT i = 0; i < numEncoders; i++) {
                if (wcscmp(pImageCodecInfo[i].MimeType, L"image/png") == 0) {
                    pngClsid = pImageCodecInfo[i].Clsid;
                    break;
                }
            }
            free(pImageCodecInfo);
        }
        
        std::wstring wfilepath(filepath.begin(), filepath.end());
        result = (bitmap.Save(wfilepath.c_str(), &pngClsid, NULL) == Gdiplus::Ok);
    }
    
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return result;
}

bool capture_screen(const std::string& filepath) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenWidth, screenHeight);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
    
    BitBlt(hMemoryDC, 0, 0, screenWidth, screenHeight, hScreenDC, 0, 0, SRCCOPY);
    
    bool result = save_bitmap_to_png(hBitmap, filepath);
    
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
    
    return result;
}

int main(int argc, char* argv[]) {
    setup_console();
    
    // 启动前清空日志
    clearLog();
    logMessage("程序启动");
    
    std::cout << "=== Aries AI Windows 自动化助手 ===" << std::endl;
    std::cout << std::endl;

    std::string api_key_str;
    bool saveKey = false;
    
    if (SecureStorage::hasSavedApiKey()) {
        api_key_str = SecureStorage::loadApiKey();
        if (!api_key_str.empty()) {
            std::cout << "已从安全存储加载 API Key" << std::endl;
            logMessage("已从安全存储加载 API Key");
        }
    }
    
    if (api_key_str.empty()) {
        const char* api_key = std::getenv("SILICON_FLOW_API_KEY");
        
        if (!api_key) {
            if (argc > 1) {
                api_key_str = argv[1];
            } else {
                std::cout << "请输入 Silicon Flow API Key: ";
                
                HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
                DWORD mode;
                GetConsoleMode(hStdin, &mode);
                SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT) & (~ENABLE_LINE_INPUT));
                
                char ch;
                DWORD read;
                while (true) {
                    ReadConsoleA(hStdin, &ch, 1, &read, NULL);
                    if (ch == '\r' || ch == '\n') {
                        break;
                    } else if (ch == '\b') {
                        if (!api_key_str.empty()) {
                            api_key_str.pop_back();
                            std::cout << "\b \b";
                        }
                    } else {
                        api_key_str += ch;
                        std::cout << '*';
                    }
                }
                
                SetConsoleMode(hStdin, mode);
                std::cout << std::endl;
                
                if (api_key_str.empty()) {
                    std::cerr << "Error: API Key 不能为空" << std::endl;
                    std::cout << "\n按任意键退出..." << std::endl;
                    _getch();
                    return 1;
                }
                
                // 询问是否保存
                std::cout << "是否保存 API Key 到本地? (y/n): ";
                char choice;
                std::cin >> choice;
                if (choice == 'y' || choice == 'Y') {
                    saveKey = true;
                }
                std::cin.ignore(); // 清除换行符
            }
        } else {
            api_key_str = api_key;
        }
    }
    
    // 保存 API Key（如果需要）
    if (saveKey) {
        if (SecureStorage::saveApiKey(api_key_str)) {
            std::cout << "API Key 已安全保存" << std::endl;
            logMessage("API Key 已安全保存");
            // 清空内存中的 API Key
            std::fill(api_key_str.begin(), api_key_str.end(), '0');
            api_key_str.clear();
            api_key_str = "[已安全存储，内存已清空]";
        } else {
            std::cerr << "警告: API Key 保存失败" << std::endl;
            logMessage("警告: API Key 保存失败");
        }
    }
    
    if (saveKey && SecureStorage::hasSavedApiKey()) {
        api_key_str = SecureStorage::loadApiKey();
        if (api_key_str.empty()) {
            std::cerr << "错误: 无法从安全存储加载 API Key" << std::endl;
            logMessage("错误: 无法从安全存储加载 API Key");
            return 1;
        }
    }
    
    const char* api_key = api_key_str.c_str();

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    std::cout << "屏幕尺寸: " << screenWidth << "x" << screenHeight << std::endl;

    ActionExecutor::Config config;
    config.screenWidth = screenWidth;
    config.screenHeight = screenHeight;
    ActionExecutor executor(config);
    
    executor.setLogCallback([](const std::string& msg) {
        std::cout << "[执行器] " << msg << std::endl;
    });

    ActionParser parser;

    ClientConfig client_config;
    client_config.api_key = api_key;
    SiliconFlowClient client(client_config);

    std::cout << std::endl;
    std::cout << "输入任务目标 (或 'quit' 退出, 'clear' 清除保存的API Key): ";
    std::string user_goal;
    std::getline(std::cin, user_goal);
    
    logMessage("用户输入: " + user_goal);
    
    if (user_goal == "quit" || user_goal == "exit") {
        logMessage("用户退出");
        return 0;
    }
    
    if (user_goal == "clear") {
        if (SecureStorage::hasSavedApiKey()) {
            if (SecureStorage::deleteApiKey()) {
                std::cout << "已清除保存的 API Key" << std::endl;
                logMessage("已清除保存的 API Key");
            } else {
                std::cerr << "清除 API Key 失败" << std::endl;
                logMessage("清除 API Key 失败");
            }
        } else {
            std::cout << "没有保存的 API Key" << std::endl;
            logMessage("没有保存的 API Key 需要清除");
        }
        std::cout << "\n按任意键退出..." << std::endl;
        _getch();
        return 0;
    }

    // 预加载应用列表
    std::vector<InstalledApp> installedApps;
    std::string appsListString;
    bool appsLoaded = false;
    
    // 存储上一次执行结果
    std::string lastExecuteOutput;

    // 主循环
    int max_iterations = 10;
    for (int iteration = 0; iteration < max_iterations; iteration++) {
        std::cout << std::endl;
        std::cout << "=== 迭代 " << (iteration + 1) << "/" << max_iterations << " ===" << std::endl;
        logMessage("=== 迭代 " + std::to_string(iteration + 1) + "/" + std::to_string(max_iterations) + " ===");

        // 截取屏幕
        std::cout << "正在截取屏幕..." << std::endl;
        std::string screenshot_path = "screenshot.png";
        
        if (!capture_screen(screenshot_path)) {
            std::cerr << "Error: 截屏失败" << std::endl;
            logMessage("错误: 截屏失败");
            std::cout << "\n按任意键退出..." << std::endl;
            _getch();
            return 1;
        }
        
        std::cout << "屏幕已保存到: " << screenshot_path << std::endl;
        logMessage("屏幕已保存");

        std::string system_prompt = PromptTemplates::buildSystemPrompt(screenWidth, screenHeight, false);
        
        std::string user_message = "任务目标: " + user_goal;
        
        if (!lastExecuteOutput.empty()) {
            user_message += "\n\n上一次命令执行结果:\n" + lastExecuteOutput;
            lastExecuteOutput.clear();
        }
        
        if (appsLoaded && !appsListString.empty()) {
            user_message += "\n\n" + appsListString;
        }
        
        user_message += "\n\n请分析当前屏幕并给出下一步操作。";

        std::cout << "正在发送请求到 AI..." << std::endl;
        logMessage("发送请求到 AI...");

        auto result = client.chat_with_images(user_message, {screenshot_path}, system_prompt);

        DeleteFileA(screenshot_path.c_str());

        if (result.first) {
            std::cout << std::endl;
            std::cout << "AI 响应:" << std::endl;
            if (!result.second.choices.empty()) {
                std::string ai_response = result.second.choices[0].message.content;
                std::cout << ai_response << std::endl;
                logMessage("AI 响应: " + ai_response);
                std::cout << std::endl;
                std::cout << "Token 使用: " << result.second.usage.total_tokens 
                          << " (提示: " << result.second.usage.prompt_tokens 
                          << ", 生成: " << result.second.usage.completion_tokens << ")" << std::endl;

                ParsedAgentAction action = parser.parse(ai_response);
                
                if (!action.isValid()) {
                    std::cerr << "无法解析 AI 响应" << std::endl;
                    logMessage("错误: 无法解析 AI 响应");
                    continue;
                }

                std::cout << std::endl;
                std::cout << "解析到动作: " << action.action << std::endl;
                logMessage("解析到动作: " + action.action);

                if (action.action == "Installed") {
                    std::cout << "正在获取已安装应用列表..." << std::endl;
                    logMessage("获取已安装应用列表...");
                    installedApps = AppManager::getInstalledApps();
                    appsListString = AppManager::getAppsListString(installedApps);
                    appsLoaded = true;
                    std::cout << "已获取 " << installedApps.size() << " 个应用" << std::endl;
                    logMessage("已获取 " + std::to_string(installedApps.size()) + " 个应用");
                    
                    // 检查AI是否指定了要打开的应用
                    std::string appName = action.fields.count("app") ? action.fields.at("app") : "";
                    if (!appName.empty()) {
                        std::cout << "尝试启动应用: " << appName << std::endl;
                        logMessage("尝试启动应用: " + appName);
                        auto* app = AppManager::findApp(installedApps, appName);
                        if (app) {
                            if (AppManager::launchApp(*app)) {
                                std::cout << "成功启动应用: " << app->displayName << std::endl;
                                logMessage("成功启动应用: " + app->displayName);
                            } else {
                                std::cerr << "启动应用失败: " << app->displayName << std::endl;
                                logMessage("启动应用失败: " + app->displayName);
                            }
                        } else {
                            std::cerr << "未找到应用: " << appName << std::endl;
                            logMessage("未找到应用: " + appName);
                        }
                    }
                    
                    Sleep(1000);
                    continue;
                }

                if (action.action == "Launch") {
                    std::string appName = action.fields.count("app") ? action.fields.at("app") : "";
                    if (!appName.empty()) {
                        logMessage("Launch 动作，应用: " + appName);
                        if (!appsLoaded) {
                            std::cout << "正在获取已安装应用列表..." << std::endl;
                            logMessage("获取已安装应用列表...");
                            installedApps = AppManager::getInstalledApps();
                            appsListString = AppManager::getAppsListString(installedApps);
                            appsLoaded = true;
                        }
                        
                        auto* app = AppManager::findApp(installedApps, appName);
                        if (app) {
                            std::cout << "找到应用，直接启动: " << app->displayName << std::endl;
                            logMessage("找到应用，直接启动: " + app->displayName);
                            if (AppManager::launchApp(*app)) {
                                std::cout << "成功启动应用!" << std::endl;
                                logMessage("成功启动应用!");
                                Sleep(1000);
                                continue;
                            } else {
                                std::cerr << "直接启动失败，回退到执行器..." << std::endl;
                                logMessage("直接启动失败，回退到执行器");
                            }
                        } else {
                            std::cout << "应用列表中未找到，使用执行器启动..." << std::endl;
                            logMessage("应用列表中未找到，使用执行器启动");
                        }
                    }
                }

                ExecutionResult exec_result = executor.execute(action);
                logMessage("执行动作: " + action.action + " - " + (exec_result.success ? "成功" : "失败") + " - " + exec_result.message);
                
                if (action.action == "Execute" && exec_result.success) {
                    lastExecuteOutput = exec_result.message;
                    std::cout << "\n命令输出:\n" << lastExecuteOutput << std::endl;
                }
                
                Sleep(500);
                
                if (!exec_result.success) {
                    std::cerr << "执行失败: " << exec_result.message << std::endl;
                    logMessage("执行失败: " + exec_result.message);
                    
                    std::string repair_prompt = PromptTemplates::buildActionRepairPrompt(action.raw, false);
                    break;
                }

                if (action.action == "finish") {
                    std::cout << std::endl;
                    std::cout << "任务完成!" << std::endl;
                    logMessage("任务完成!");
                    break;
                }
            } else {
                std::cerr << "AI 响应为空" << std::endl;
                logMessage("错误: AI 响应为空");
            }

        } else {
            if (client.last_error().has_value()) {
                std::cerr << "请求失败: " << client.last_error()->message << std::endl;
                logMessage("请求失败: " + client.last_error()->message);
            }
            std::cout << "\n按任意键退出..." << std::endl;
            _getch();
            return 1;
        }

        Sleep(1000);
    }

    logMessage("程序结束");
    std::cout << std::endl;
    std::cout << "=== 完成 ===" << std::endl;
    std::cout << "按任意键退出..." << std::endl;
    _getch();

    return 0;
}
