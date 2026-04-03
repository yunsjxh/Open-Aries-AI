#include "ai_provider.hpp"
#include "openai_compatible_provider.hpp"
#include "provider_manager.hpp"
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

// 显示提供商选择菜单
std::string selectProvider(ProviderManager& manager) {
    auto providers = manager.getProviderNames();
    
    std::cout << "\n=== 选择 AI 提供商 ===" << std::endl;
    std::cout << "可用的提供商:" << std::endl;
    
    int index = 1;
    int defaultChoice = 1;
    for (const auto& name : providers) {
        std::string info = manager.getProviderInfo(name);
        std::cout << "  " << index << ". " << info << std::endl;
        if (manager.hasSavedApiKey(name)) {
            defaultChoice = index;
        }
        index++;
    }
    
    std::cout << "\n请选择 (1-" << providers.size() << ", 默认 " << defaultChoice << "): ";
    std::string input;
    std::getline(std::cin, input);
    
    int choice = defaultChoice;
    if (!input.empty()) {
        try {
            choice = std::stoi(input);
            if (choice < 1 || choice > (int)providers.size()) {
                choice = defaultChoice;
            }
        } catch (...) {
            choice = defaultChoice;
        }
    }
    
    return providers[choice - 1];
}

// 选择模型
std::string selectModel(ProviderManager& manager, const std::string& providerName) {
    auto models = manager.getAvailableModels(providerName);
    
    std::cout << "\n=== 选择模型 ===" << std::endl;
    
    // 如果有预设模型，显示列表
    if (!models.empty()) {
        std::cout << "可用的模型:" << std::endl;
        
        int index = 1;
        std::string currentModel = manager.getCurrentModel(providerName);
        int defaultChoice = 1;
        
        for (const auto& model : models) {
            std::string info = "  " + std::to_string(index) + ". " + model.name;
            if (model.supportsVision) info += " [Vision]";
            if (model.name == currentModel) {
                info += " (当前)";
                defaultChoice = index;
            }
            std::cout << info << std::endl;
            index++;
        }
        
        std::cout << "  " << index << ". 自定义输入模型名称" << std::endl;
        
        std::cout << "\n请选择 (1-" << index << ", 默认 " << defaultChoice << "): ";
        std::string input;
        std::getline(std::cin, input);
        
        int choice = defaultChoice;
        if (!input.empty()) {
            try {
                choice = std::stoi(input);
                if (choice < 1 || choice > index) {
                    choice = defaultChoice;
                }
            } catch (...) {
                choice = defaultChoice;
            }
        }
        
        // 如果选择了自定义输入
        if (choice == index) {
            std::cout << "请输入模型名称: ";
            std::string customModel;
            std::getline(std::cin, customModel);
            return customModel.empty() ? currentModel : customModel;
        }
        
        return models[choice - 1].name;
    } else {
        // 没有预设模型，直接输入
        std::string currentModel = manager.getCurrentModel(providerName);
        std::cout << "当前模型: " << (currentModel.empty() ? "(未设置)" : currentModel) << std::endl;
        std::cout << "请输入模型名称" << (currentModel.empty() ? "" : " (直接回车保持当前)") << ": ";
        std::string customModel;
        std::getline(std::cin, customModel);
        return customModel.empty() ? currentModel : customModel;
    }
}

// 配置自定义提供商
bool configureCustomProvider(ProviderManager& manager, std::string& providerName, std::string& baseUrl, std::string& modelName) {
    std::cout << "\n=== 配置自定义提供商 ===" << std::endl;
    
    std::cout << "请输入提供商名称: ";
    std::getline(std::cin, providerName);
    if (providerName.empty()) {
        std::cerr << "错误: 提供商名称不能为空" << std::endl;
        return false;
    }
    
    std::cout << "请输入 API Base URL (例如: https://api.example.com/v1): ";
    std::getline(std::cin, baseUrl);
    if (baseUrl.empty()) {
        std::cerr << "错误: API Base URL 不能为空" << std::endl;
        return false;
    }
    
    std::cout << "请输入默认模型名称: ";
    std::getline(std::cin, modelName);
    if (modelName.empty()) {
        modelName = "gpt-3.5-turbo";
    }
    
    // 注册自定义提供商
    ProviderConfig config;
    config.name = providerName;
    config.baseUrl = baseUrl;
    config.modelName = modelName;
    config.supportsVision = false;
    config.supportsAudio = false;
    config.supportsVideo = false;
    manager.registerProviderConfig(config);
    
    return true;
}

// 配置提供商 API Key
bool configureProvider(ProviderManager& manager, const std::string& providerName, const std::string& modelName) {
    std::cout << "\n配置 " << providerName << " API Key" << std::endl;

    // 检查是否已有保存的 key
    if (manager.hasSavedApiKey(providerName)) {
        std::cout << "已保存 API Key，是否重新配置? (y/n): ";
        char choice;
        std::cin >> choice;
        std::cin.ignore();
        if (choice != 'y' && choice != 'Y') {
            return true;
        }
    }

    std::cout << "请输入 API Key (输入 'key' 打开获取页面, 输入 'provider' 自定义): ";

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT) & (~ENABLE_LINE_INPUT));

    std::string apiKey;
    char ch;
    DWORD read;
    while (true) {
        ReadConsoleA(hStdin, &ch, 1, &read, NULL);
        if (ch == '\r' || ch == '\n') {
            break;
        } else if (ch == '\b') {
            if (!apiKey.empty()) {
                apiKey.pop_back();
                std::cout << "\b \b";
            }
        } else {
            apiKey += ch;
            std::cout << '*';
        }
    }

    SetConsoleMode(hStdin, mode);
    std::cout << std::endl;

    // 如果输入的是 "key"，打开浏览器获取 API Key
    if (apiKey == "key") {
        std::cout << "正在打开浏览器获取 API Key..." << std::endl;
        ShellExecuteA(NULL, "open", "https://open.bigmodel.cn/login?redirect=%2Fusercenter%2Fproj-mgmt%2Fapikeys", NULL, NULL, SW_SHOWNORMAL);
        std::cout << "请获取 API Key 后重新运行程序" << std::endl;
        return false;
    }

    // 如果输入的是 "provider"，使用自定义提供商
    if (apiKey == "provider") {
        std::cout << "\n=== 配置自定义提供商 ===" << std::endl;
        std::cout << "请输入 API Base URL (例如: https://api.example.com/v1): ";
        std::string customBaseUrl;
        std::getline(std::cin, customBaseUrl);
        if (customBaseUrl.empty()) {
            std::cerr << "Error: API Base URL 不能为空" << std::endl;
            return false;
        }

        std::cout << "请输入模型名称: ";
        std::string customModel;
        std::getline(std::cin, customModel);
        if (customModel.empty()) {
            customModel = "gpt-3.5-turbo";
        }

        // 注册自定义提供商
        ProviderConfig config;
        config.name = "custom";
        config.baseUrl = customBaseUrl;
        config.modelName = customModel;
        config.supportsVision = false;
        config.supportsAudio = false;
        config.supportsVideo = false;
        manager.registerProviderConfig(config);

        // 重新调用配置，使用自定义提供商
        return configureProvider(manager, "custom", customModel);
    }

    if (apiKey.empty()) {
        std::cerr << "Error: API Key 不能为空" << std::endl;
        return false;
    }

    // 验证 API Key
    std::cout << "正在验证 API Key..." << std::endl;
    auto tempProvider = std::make_shared<OpenAICompatibleProvider>(
        apiKey,
        manager.getConfig(providerName)->baseUrl,
        modelName,
        false, false, false
    );

    if (!tempProvider->validateApiKey()) {
        std::cerr << "Error: API Key 验证失败 - " << tempProvider->getLastError() << std::endl;
        std::cout << "是否强制保存并使用此 API Key? (y/n): ";
        char choice;
        std::cin >> choice;
        std::cin.ignore();
        if (choice != 'y' && choice != 'Y') {
            std::cout << "请检查 API Key 是否正确" << std::endl;
            return false;
        }
        std::cout << "强制使用未验证的 API Key" << std::endl;
    } else {
        std::cout << "API Key 验证成功!" << std::endl;
    }

    if (manager.saveProviderApiKey(providerName, apiKey)) {
        std::cout << "API Key 已安全保存" << std::endl;
        logMessage(providerName + " API Key 已保存");
        return true;
    } else {
        std::cerr << "警告: API Key 保存失败" << std::endl;
        return false;
    }
}

int main(int argc, char* argv[]) {
    setup_console();
    clearLog();
    logMessage("程序启动");
    
    std::cout << R"(
      ___           ___                       ___           ___                    ___
     /\  \         /\  \          ___        /\  \         /\  \                  /\  \          ___
    /::\  \       /::\  \        /\  \      /::\  \       /::\  \                /::\  \        /\  \
   /:/\:\  \     /:/\:\  \       \:\  \    /:/\:\  \     /:/\ \  \              /:/\:\  \       \:\  \
  /::\~\:\  \   /::\~\:\  \      /::\__\  /::\~\:\  \   _\:\~\ \  \            /::\~\:\  \      /::\__\
 /:/\:\ \:\__\ /:/\:\ \:\__\  __/:/\/__/ /:/\:\ \:\__\ /\ \:\ \ \__\          /:/\:\ \:\__\  __/:/\/__/
 \/__\:\/:/  / \/_|::\/:/  / /\/:/  /    \:\~\:\ \/__/ \:\ \:\ \/__/          \/__\:\/:/  / /\/:/  /
      \::/  /     |:|::/  /  \::/__/      \:\ \:\__\    \:\ \:\__\                 \::/  /  \::/__/
      /:/  /      |:|\/__/    \:\__\       \:\ \/__/     \:\/:/  /                 /:/  /    \:\__\
     /:/  /       |:|  |       \/__/        \:\__\        \::/  /                 /:/  /      \/__/
     \/__/         \|__|                     \/__/         \/__/                  \/__/
)" << std::endl;
    std::cout << std::endl;
    
    // 初始化提供商管理器
    ProviderManager& manager = ProviderManager::getInstance();
    manager.registerBuiltInProviders();

    // 默认使用智谱 AI
    std::string providerName = "zhipu";
    std::cout << "使用提供商: 智谱 AI (zhipu)" << std::endl;
    logMessage("使用提供商: zhipu");

    // 默认使用 glm-4.6v-flash 模型
    std::string modelName = "glm-4.6v-flash";
    manager.setCurrentModel(providerName, modelName);
    std::cout << "使用模型: " << modelName << std::endl;
    logMessage("使用模型: " + modelName);

    // 配置 API Key
    if (!manager.hasSavedApiKey(providerName)) {
        if (!configureProvider(manager, providerName, modelName)) {
            std::cout << "\n按任意键退出..." << std::endl;
            _getch();
            return 1;
        }
    }
    
    // 设置当前提供商
    manager.setCurrentProvider(providerName);
    AIProviderPtr provider = manager.getCurrentProvider();
    
    if (!provider) {
        std::cerr << "错误: 无法初始化 AI 提供商" << std::endl;
        logMessage("错误: 无法初始化 AI 提供商");
        std::cout << "\n按任意键退出..." << std::endl;
        _getch();
        return 1;
    }
    
    std::cout << "提供商: " << provider->getProviderName() << std::endl;
    std::cout << "模型: " << provider->getModelName() << std::endl;
    std::cout << "视觉支持: " << (provider->supportsVision() ? "是" : "否") << std::endl;

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

    std::cout << std::endl;
    std::cout << "输入任务目标 (或 'quit' 退出, 'clear' 清除所有API Key, 'provider' 切换提供商): ";
    std::string user_goal;
    std::getline(std::cin, user_goal);
    
    logMessage("用户输入: " + user_goal);
    
    if (user_goal == "quit" || user_goal == "exit") {
        logMessage("用户退出");
        return 0;
    }
    
    if (user_goal == "clear") {
        auto providers = manager.getProviderNames();
        for (const auto& name : providers) {
            if (manager.hasSavedApiKey(name)) {
                SecureStorage::deleteApiKey(name);
            }
        }
        std::cout << "已清除所有保存的 API Key" << std::endl;
        logMessage("已清除所有 API Key");
        std::cout << "\n按任意键退出..." << std::endl;
        _getch();
        return 0;
    }
    
    if (user_goal == "provider") {
        providerName = selectProvider(manager);
        if (!manager.hasSavedApiKey(providerName)) {
            configureProvider(manager, providerName, modelName);
        }
        manager.setCurrentProvider(providerName);
        provider = manager.getCurrentProvider();
        std::cout << "已切换到: " << providerName << std::endl;
    }

    std::vector<InstalledApp> installedApps;
    std::string appsListString;
    bool appsLoaded = false;
    std::string lastExecuteOutput;

    int max_iterations = 10;
    for (int iteration = 0; iteration < max_iterations; iteration++) {
        std::cout << std::endl;
        // 第一次迭代不等待，后续迭代等待 3 秒
        if (iteration > 0) {
            std::cout << "等待 3 秒..." << std::endl;
            Sleep(3000);
        }

        std::cout << "=== 迭代 " << (iteration + 1) << "/" << max_iterations << " ===" << std::endl;
        logMessage("=== 迭代 " + std::to_string(iteration + 1) + "/" + std::to_string(max_iterations) + " ===");

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

        auto result = provider->sendMessageWithImages(user_message, {screenshot_path}, system_prompt);

        DeleteFileA(screenshot_path.c_str());

        if (result.first) {
            std::cout << std::endl;
            std::cout << "AI 响应:" << std::endl;
            std::string ai_response = result.second;
            std::cout << ai_response << std::endl;
            logMessage("AI 响应: " + ai_response);

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
            std::string errorMsg = provider->getLastError();
            std::cerr << "请求失败: " << errorMsg << std::endl;
            logMessage("请求失败: " + errorMsg);

            // 检查是否是访问量过大等可重试错误
            if (errorMsg.find("访问量过大") != std::string::npos ||
                errorMsg.find("rate limit") != std::string::npos ||
                errorMsg.find("too many requests") != std::string::npos ||
                errorMsg.find("timeout") != std::string::npos ||
                errorMsg.find("503") != std::string::npos ||
                errorMsg.find("429") != std::string::npos ||
                errorMsg.find("500") != std::string::npos) {
                std::cout << "\n是否重试? (y/n): ";
                char retryChoice;
                std::cin >> retryChoice;
                std::cin.ignore();
                if (retryChoice == 'y' || retryChoice == 'Y') {
                    std::cout << "等待 3 秒后重试..." << std::endl;
                    Sleep(3000);
                    iteration--; // 重试当前迭代
                    continue;
                }
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
