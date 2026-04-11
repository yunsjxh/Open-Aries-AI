#include "ai_provider.hpp"
#include "openai_compatible_provider.hpp"
#include "provider_manager.hpp"
#include "prompt_templates.hpp"
#include "action_parser.hpp"
#include "action_executor.hpp"
#include "app_manager.hpp"
#include "secure_storage.hpp"
#include "file_manager.hpp"
#include <iostream>
#include <cstdlib>
#include <windows.h>
#include <gdiplus.h>
#include <conio.h>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <algorithm>
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

// 从AI响应中提取思考过程
std::string extractThinkingFromResponse(const std::string& response) {
    std::string thinking;
    
    // 尝试提取 <思考过程> 标签内的内容
    size_t thinkStart = response.find("<思考过程>");
    size_t thinkEnd = response.find("</思考过程>");
    
    if (thinkStart != std::string::npos && thinkEnd != std::string::npos && thinkEnd > thinkStart) {
        thinking = response.substr(thinkStart + 10, thinkEnd - thinkStart - 10); // 10 = len("<思考过程>")
    } else {
        // 尝试提取 <|begin_of_box|> 之前的内容作为思考
        size_t boxStart = response.find("<|begin_of_box|>");
        if (boxStart != std::string::npos && boxStart > 0) {
            thinking = response.substr(0, boxStart);
        } else {
            // 如果没有找到标签，返回前200个字符
            thinking = response.substr(0, std::min(size_t(200), response.length()));
        }
    }
    
    // 清理思考内容
    // 去除前后空白
    size_t start = thinking.find_first_not_of(" \t\n\r");
    size_t end = thinking.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos) {
        thinking = thinking.substr(start, end - start + 1);
    }
    
    // 如果思考内容太长，截断并添加省略号
    if (thinking.length() > 300) {
        thinking = thinking.substr(0, 300) + "...";
    }
    
    return thinking.empty() ? "(无思考过程)" : thinking;
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
// 返回值: 0=失败, 1=成功使用默认提供商, 2=成功使用自定义提供商
int configureProvider(ProviderManager& manager, const std::string& providerName, const std::string& modelName, std::string& outProviderName) {
    outProviderName = providerName; // 默认使用传入的提供商
    std::cout << "\n配置 " << providerName << " API Key" << std::endl;

    // 检查是否已有保存的 key
    if (manager.hasSavedApiKey(providerName)) {
        std::cout << "已保存 API Key，是否重新配置? (y/n): ";
        char choice;
        std::cin >> choice;
        std::cin.ignore();
        if (choice != 'y' && choice != 'Y') {
            // 如果是 custom 提供商，返回 2，否则返回 1
            return (providerName == "custom") ? 2 : 1;
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
        return 0; // 失败
    }

    // 如果输入的是 "provider"，使用自定义提供商
    if (apiKey == "provider") {
        std::cout << "\n=== 配置自定义提供商 ===" << std::endl;
        
        // 获取所有已保存的自定义提供商
        auto customProviders = SecureStorage::getCustomProviderList();
        std::string selectedProviderName;
        std::string customBaseUrl;
        std::string customModel;
        std::string customApiKey;
        
        if (!customProviders.empty()) {
            std::cout << "\n已保存的自定义提供商:" << std::endl;
            for (size_t i = 0; i < customProviders.size(); i++) {
                auto [baseUrl, modelName, apiKey] = SecureStorage::loadCustomProvider(customProviders[i]);
                std::cout << "  " << (i + 1) << ". " << customProviders[i] << std::endl;
                std::cout << "     URL: " << baseUrl << std::endl;
                std::cout << "     模型: " << modelName << std::endl;
            }
            std::cout << "  " << (customProviders.size() + 1) << ". 添加新提供商" << std::endl;
            std::cout << "\n请选择 (1-" << (customProviders.size() + 1) << "): ";
            
            int choice;
            std::string input;
            std::getline(std::cin, input);
            try {
                choice = std::stoi(input);
            } catch (...) {
                choice = customProviders.size() + 1; // 默认添加新提供商
            }
            
            if (choice >= 1 && choice <= (int)customProviders.size()) {
                // 选择已保存的提供商
                selectedProviderName = customProviders[choice - 1];
                auto [baseUrl, modelName, apiKey] = SecureStorage::loadCustomProvider(selectedProviderName);
                customBaseUrl = baseUrl;
                customModel = modelName;
                customApiKey = apiKey;
                
                std::cout << "\n已选择: " << selectedProviderName << std::endl;
                std::cout << "URL: " << customBaseUrl << std::endl;
                std::cout << "模型: " << customModel << std::endl;
                
                // 询问是否修改模型
                std::cout << "\n是否修改模型? (y/n): ";
                char modifyModel;
                std::cin >> modifyModel;
                std::cin.ignore();
                if (modifyModel == 'y' || modifyModel == 'Y') {
                    std::cout << "请输入新模型名称 (当前: " << customModel << "): ";
                    std::string newModel;
                    std::getline(std::cin, newModel);
                    if (!newModel.empty()) {
                        customModel = newModel;
                        // 保存更新后的配置
                        SecureStorage::saveCustomProvider(selectedProviderName, customBaseUrl, customModel, customApiKey);
                        std::cout << "模型已更新为: " << customModel << std::endl;
                    }
                }
            } else {
                // 添加新提供商
                selectedProviderName = "";
            }
        }
        
        // 如果是新提供商或没有已保存的提供商
        if (selectedProviderName.empty()) {
            std::cout << "\n请输入新提供商名称: ";
            std::getline(std::cin, selectedProviderName);
            if (selectedProviderName.empty()) {
                std::cerr << "错误: 提供商名称不能为空" << std::endl;
                return 0;
            }
            
            std::cout << "请输入 API Base URL (例如: https://api.example.com/v1): ";
            std::getline(std::cin, customBaseUrl);
            if (customBaseUrl.empty()) {
                std::cerr << "错误: API Base URL 不能为空" << std::endl;
                return 0;
            }
            
            std::cout << "请输入模型名称: ";
            std::getline(std::cin, customModel);
            if (customModel.empty()) {
                customModel = "gpt-3.5-turbo";
            }
            
            // 输入 API Key
            std::cout << "请输入 API Key: ";
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
                    if (!customApiKey.empty()) {
                        customApiKey.pop_back();
                        std::cout << "\b \b";
                    }
                } else {
                    customApiKey += ch;
                    std::cout << '*';
                }
            }
            SetConsoleMode(hStdin, mode);
            std::cout << std::endl;
            
            if (customApiKey.empty()) {
                std::cerr << "错误: API Key 不能为空" << std::endl;
                return 0;
            }
            
            // 保存新提供商配置
            SecureStorage::saveCustomProvider(selectedProviderName, customBaseUrl, customModel, customApiKey);
            std::cout << "提供商 '" << selectedProviderName << "' 已保存" << std::endl;
        }
        
        // 注册提供商到管理器
        ProviderConfig config;
        config.name = selectedProviderName;
        config.baseUrl = customBaseUrl;
        config.modelName = customModel;
        config.supportsVision = false;
        config.supportsAudio = false;
        config.supportsVideo = false;
        manager.registerProviderConfig(config);
        
        // 保存 API Key 到标准存储（用于兼容性）
        manager.saveProviderApiKey(selectedProviderName, customApiKey);
        
        // 更新输出提供商名称
        outProviderName = selectedProviderName;
        return 2; // 返回 2 表示使用自定义提供商
    }

    if (apiKey.empty()) {
        std::cerr << "Error: API Key 不能为空" << std::endl;
        return 0; // 失败
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
            return 0; // 失败
        }
        std::cout << "强制使用未验证的 API Key" << std::endl;
    } else {
        std::cout << "API Key 验证成功!" << std::endl;
    }

    if (manager.saveProviderApiKey(providerName, apiKey)) {
        std::cout << "API Key 已安全保存" << std::endl;
        logMessage(providerName + " API Key 已保存");
        return 1;
    } else {
        std::cerr << "警告: API Key 保存失败" << std::endl;
        return 0; // 失败
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

    // 加载所有自定义提供商
    auto customProviders = SecureStorage::getCustomProviderList();
    for (const auto& name : customProviders) {
        auto [baseUrl, modelName, apiKey] = SecureStorage::loadCustomProvider(name);
        if (!baseUrl.empty() && !modelName.empty()) {
            ProviderConfig config;
            config.name = name;
            config.baseUrl = baseUrl;
            config.modelName = modelName;
            config.supportsVision = false;
            config.supportsAudio = false;
            config.supportsVideo = false;
            manager.registerProviderConfig(config);
            // 同时保存 API Key
            manager.saveProviderApiKey(name, apiKey);
            logMessage("加载自定义提供商: " + name + ", URL: " + baseUrl + ", 模型: " + modelName);
        }
    }

    // 默认使用智谱 AI
    std::string providerName = "zhipu";
    std::string modelName = "glm-4.6v-flash";
    
    std::cout << "使用提供商: 智谱 AI (zhipu)" << std::endl;
    logMessage("使用提供商: zhipu");
    
    // 默认使用 glm-4.6v-flash 模型
    manager.setCurrentModel(providerName, modelName);
    std::cout << "使用模型: " << modelName << std::endl;
    logMessage("使用模型: " + modelName);

    // 配置 API Key
    std::string actualProviderName = providerName;
    bool configSuccess = false;
    while (!configSuccess) {
        // 检查当前提供商是否已有保存的 key
        if (manager.hasSavedApiKey(actualProviderName)) {
            configSuccess = true;
            break;
        }
        
        int result = configureProvider(manager, providerName, modelName, actualProviderName);
        if (result == 0) {
            std::cout << "\n配置失败，是否重试? (y/n): ";
            char retryChoice;
            std::cin >> retryChoice;
            std::cin.ignore();
            if (retryChoice != 'y' && retryChoice != 'Y') {
                std::cout << "\n按任意键退出..." << std::endl;
                _getch();
                return 1;
            }
            // 重试，重新显示配置提示
            std::cout << std::endl;
        } else {
            // 配置成功，如果使用自定义提供商，更新 providerName
            if (result == 2) {
                providerName = actualProviderName;
                modelName = manager.getConfig(providerName)->modelName;
                std::cout << "已切换到自定义提供商: " << providerName << std::endl;
                std::cout << "使用模型: " << modelName << std::endl;
            }
            configSuccess = true;
            break; // 配置成功，退出循环
        }
    }
    
    // 设置当前提供商
    std::cout << "正在初始化提供商: " << providerName << std::endl;
    logMessage("正在初始化提供商: " + providerName);
    
    // 调试：检查配置是否存在
    auto* providerConfig = manager.getConfig(providerName);
    if (!providerConfig) {
        std::cerr << "错误: 找不到提供商配置 (" << providerName << ")" << std::endl;
        logMessage("错误: 找不到提供商配置 (" + providerName + ")");
        std::cout << "\n按任意键退出..." << std::endl;
        _getch();
        return 1;
    }
    std::cout << "调试: 找到配置，baseUrl=" << providerConfig->baseUrl << ", model=" << providerConfig->modelName << std::endl;
    
    // 调试：检查是否有保存的 API Key
    bool hasKey = manager.hasSavedApiKey(providerName);
    std::cout << "调试: hasSavedApiKey=" << (hasKey ? "true" : "false") << std::endl;
    
    manager.setCurrentProvider(providerName);
    AIProviderPtr provider = manager.getCurrentProvider();
    
    if (!provider) {
        std::cerr << "错误: 无法初始化 AI 提供商 (" << providerName << ")" << std::endl;
        logMessage("错误: 无法初始化 AI 提供商 (" + providerName + ")");
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
        // 同时清除所有自定义提供商
        auto customProviders = SecureStorage::getCustomProviderList();
        for (const auto& name : customProviders) {
            SecureStorage::deleteCustomProvider(name);
        }
        std::cout << "已清除所有保存的 API Key 和配置" << std::endl;
        logMessage("已清除所有 API Key 和配置");
        std::cout << "\n按任意键退出..." << std::endl;
        _getch();
        return 0;
    }
    
    if (user_goal == "provider") {
        providerName = selectProvider(manager);
        std::string tempProviderName = providerName;
        if (!manager.hasSavedApiKey(providerName)) {
            int result = configureProvider(manager, providerName, modelName, tempProviderName);
            if (result == 2) {
                providerName = tempProviderName;
                modelName = manager.getConfig(providerName)->modelName;
            }
        }
        manager.setCurrentProvider(providerName);
        provider = manager.getCurrentProvider();
        std::cout << "已切换到: " << providerName << std::endl;
    }

    std::vector<InstalledApp> installedApps;
    std::string appsListString;
    bool appsLoaded = false;
    std::string lastExecuteOutput;

    // 动作历史记录（保存最近2次）
    struct ActionHistory {
        std::string action;
        std::string thinking;
        std::string timestamp;
    };
    std::vector<ActionHistory> actionHistory;

    int max_iterations = 10;
    int iteration = 0;
    int totalSteps = 0;  // 总步数计数器（不重置）
    bool continueExecution = true;
    bool shouldRetry = false;  // 重试标志
    
    while (continueExecution) {
        std::cout << std::endl;
        
        // 如果不是重试，且不是第一步，等待1秒
        if (!shouldRetry && totalSteps > 0) {
            std::cout << "等待 1 秒..." << std::endl;
            Sleep(1000);
        }
        
        // 重置重试标志
        shouldRetry = false;

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
        
        // 添加动作历史反馈（最近2次）
        if (!actionHistory.empty()) {
            user_message += "\n\n【动作历史 - 最近 " + std::to_string(actionHistory.size()) + " 次】";
            for (size_t i = 0; i < actionHistory.size(); i++) {
                user_message += "\n\n第 " + std::to_string(i + 1) + " 次:";
                user_message += "\n执行动作: " + actionHistory[i].action;
                user_message += "\nAI思考: " + actionHistory[i].thinking;
            }
            user_message += "\n\n注意: 如果上述动作已经连续多次执行但界面没有变化，请尝试不同的动作（如滑动、等待、返回等），避免重复无效操作。";
        }
        
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
            
            // 提取思考过程并保存到历史记录
            std::string thinking = extractThinkingFromResponse(ai_response);
            ActionHistory history;
            history.action = action.raw;
            history.thinking = thinking;
            history.timestamp = getCurrentTime();
            actionHistory.push_back(history);
            // 只保留最近2次
            if (actionHistory.size() > 2) {
                actionHistory.erase(actionHistory.begin());
            }

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
                        // 记录详细应用信息到日志
                        logMessage("应用详细信息:");
                        logMessage("  name: " + app->name);
                        logMessage("  displayName: " + app->displayName);
                        logMessage("  executablePath: " + app->executablePath);
                        logMessage("  installLocation: " + app->installLocation);
                        logMessage("  uninstallString: " + app->uninstallString);
                        
                        // 检查 executablePath 是否是 .ico 文件
                        if (!app->executablePath.empty() && app->executablePath.find(".ico") != std::string::npos) {
                            std::cout << "检测到图标文件，需要获取所有可执行文件列表..." << std::endl;
                            logMessage("检测到图标文件，获取所有可执行文件列表");
                            
                            // 从 uninstallString 推断安装目录
                            if (!app->uninstallString.empty()) {
                                std::string installDir = AppManager::extractDirectoryFromPath(app->uninstallString);
                                if (!installDir.empty()) {
                                    // 获取所有可执行文件
                                    auto exeList = AppManager::getAllExecutablesInDirectory(installDir);
                                    if (!exeList.empty()) {
                                        std::cout << "找到 " << exeList.size() << " 个可执行文件，发送给 AI 判断..." << std::endl;
                                        logMessage("找到 " + std::to_string(exeList.size()) + " 个可执行文件");
                                        
                                        // 构建 exe 列表字符串
                                        std::string exeListStr = "\n\n该应用的安装目录下有多个可执行文件，请判断应该启动哪个主程序:\n";
                                        for (size_t i = 0; i < exeList.size(); i++) {
                                            // exeList[i].first 是文件名，exeList[i].second 是完整路径
                                            exeListStr += std::to_string(i + 1) + ". 文件名: " + exeList[i].first + "\n";
                                            exeListStr += "   路径: \"" + exeList[i].second + "\"\n";
                                        }
                                        exeListStr += "\n请使用 FileRun 动作启动主程序，例如: do(action=\"FileRun\", path=\"完整路径\", desc=\"启动程序\")\n";
                                        exeListStr += "提示: 通常主程序的名称会包含应用名称，卸载程序通常包含'uninstall'或'卸载'字样。";
                                        
                                        // 将 exe 列表添加到上一次执行结果中，让 AI 在下次迭代中看到
                                        lastExecuteOutput = "应用 '" + app->displayName + "' 的 executablePath 是图标文件。" + exeListStr;
                                        std::cout << lastExecuteOutput << std::endl;
                                        
                                        // 跳过本次迭代的其他处理，让 AI 在下次迭代中决定
                                        Sleep(1000);
                                        continue;
                                    }
                                }
                            }
                        }
                        
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

            // 处理文件管理动作
            ExecutionResult exec_result;
            bool isFileAction = false;
            std::string fileResult;
            
            if (action.action == "FileList") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : ".";
                fileResult = FileManager::generateDirectoryDescription(path);
                std::cout << "\n" << fileResult << std::endl;
                exec_result = ExecutionResult{true, fileResult};
            } else if (action.action == "FileRead") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : "";
                if (!path.empty()) {
                    fileResult = FileManager::readTextFile(path);
                    std::cout << "\n文件内容:\n" << fileResult << std::endl;
                } else {
                    fileResult = "错误: 未指定文件路径";
                    std::cerr << fileResult << std::endl;
                }
                exec_result = ExecutionResult{!path.empty(), fileResult};
            } else if (action.action == "FileHead") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : "";
                int lines = 50;
                if (action.fields.count("lines")) {
                    try { lines = std::stoi(action.fields.at("lines")); } catch (...) {}
                }
                if (!path.empty()) {
                    fileResult = FileManager::readFileHead(path, lines);
                    std::cout << "\n文件前 " << lines << " 行:\n" << fileResult << std::endl;
                } else {
                    fileResult = "错误: 未指定文件路径";
                    std::cerr << fileResult << std::endl;
                }
                exec_result = ExecutionResult{!path.empty(), fileResult};
            } else if (action.action == "FileTail") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : "";
                int lines = 50;
                if (action.fields.count("lines")) {
                    try { lines = std::stoi(action.fields.at("lines")); } catch (...) {}
                }
                if (!path.empty()) {
                    fileResult = FileManager::readFileTail(path, lines);
                    std::cout << "\n文件后 " << lines << " 行:\n" << fileResult << std::endl;
                } else {
                    fileResult = "错误: 未指定文件路径";
                    std::cerr << fileResult << std::endl;
                }
                exec_result = ExecutionResult{!path.empty(), fileResult, ""};
            } else if (action.action == "FileRange") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : "";
                int start = 1, end = 50;
                if (action.fields.count("start")) {
                    try { start = std::stoi(action.fields.at("start")); } catch (...) {}
                }
                if (action.fields.count("end")) {
                    try { end = std::stoi(action.fields.at("end")); } catch (...) {}
                }
                if (!path.empty()) {
                    fileResult = FileManager::readFileRange(path, start, end);
                    std::cout << "\n文件第 " << start << "-" << end << " 行:\n" << fileResult << std::endl;
                } else {
                    fileResult = "错误: 未指定文件路径";
                    std::cerr << fileResult << std::endl;
                }
                exec_result = ExecutionResult{!path.empty(), fileResult, ""};
            } else if (action.action == "FileWrite") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : "";
                std::string content = action.fields.count("content") ? action.fields.at("content") : "";
                if (!path.empty()) {
                    bool success = FileManager::writeTextFile(path, content, false);
                    fileResult = success ? "文件写入成功" : "文件写入失败";
                    std::cout << fileResult << std::endl;
                    exec_result = ExecutionResult{success, fileResult};
                } else {
                    fileResult = "错误: 未指定文件路径";
                    std::cerr << fileResult << std::endl;
                    exec_result = ExecutionResult{false, fileResult};
                }
            } else if (action.action == "FileAppend") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : "";
                std::string content = action.fields.count("content") ? action.fields.at("content") : "";
                if (!path.empty()) {
                    bool success = FileManager::writeTextFile(path, content, true);
                    fileResult = success ? "文件追加成功" : "文件追加失败";
                    std::cout << fileResult << std::endl;
                    exec_result = ExecutionResult{success, fileResult, ""};
                } else {
                    fileResult = "错误: 未指定文件路径";
                    std::cerr << fileResult << std::endl;
                    exec_result = ExecutionResult{false, fileResult, ""};
                }
            } else if (action.action == "FileMkdir") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : "";
                if (!path.empty()) {
                    bool success = FileManager::createDirectory(path);
                    fileResult = success ? "目录创建成功" : "目录创建失败";
                    std::cout << fileResult << std::endl;
                    exec_result = ExecutionResult{success, fileResult, ""};
                } else {
                    fileResult = "错误: 未指定目录路径";
                    std::cerr << fileResult << std::endl;
                    exec_result = ExecutionResult{false, fileResult, ""};
                }
            } else if (action.action == "FileDelete") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : "";
                if (!path.empty()) {
                    bool success = FileManager::deleteFile(path);
                    fileResult = success ? "文件删除成功" : "文件删除失败";
                    std::cout << fileResult << std::endl;
                    exec_result = ExecutionResult{success, fileResult, ""};
                } else {
                    fileResult = "错误: 未指定文件路径";
                    std::cerr << fileResult << std::endl;
                    exec_result = ExecutionResult{false, fileResult, ""};
                }
            } else if (action.action == "FileMove") {
                isFileAction = true;
                std::string source = action.fields.count("source") ? action.fields.at("source") : "";
                std::string destination = action.fields.count("destination") ? action.fields.at("destination") : "";
                if (!source.empty() && !destination.empty()) {
                    bool success = FileManager::move(source, destination);
                    fileResult = success ? "文件移动成功" : "文件移动失败";
                    std::cout << fileResult << std::endl;
                    exec_result = ExecutionResult{success, fileResult, ""};
                } else {
                    fileResult = "错误: 未指定源路径或目标路径";
                    std::cerr << fileResult << std::endl;
                    exec_result = ExecutionResult{false, fileResult, ""};
                }
            } else if (action.action == "FileCopy") {
                isFileAction = true;
                std::string source = action.fields.count("source") ? action.fields.at("source") : "";
                std::string destination = action.fields.count("destination") ? action.fields.at("destination") : "";
                if (!source.empty() && !destination.empty()) {
                    bool success = FileManager::copyFile(source, destination, true);
                    fileResult = success ? "文件复制成功" : "文件复制失败";
                    std::cout << fileResult << std::endl;
                    exec_result = ExecutionResult{success, fileResult, ""};
                } else {
                    fileResult = "错误: 未指定源路径或目标路径";
                    std::cerr << fileResult << std::endl;
                    exec_result = ExecutionResult{false, fileResult, ""};
                }
            } else if (action.action == "FileInfo") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : "";
                if (!path.empty()) {
                    fileResult = FileManager::getFileSummary(path);
                    std::cout << "\n" << fileResult << std::endl;
                } else {
                    fileResult = "错误: 未指定文件路径";
                    std::cerr << fileResult << std::endl;
                }
                exec_result = ExecutionResult{!path.empty(), fileResult, ""};
            } else if (action.action == "FileSearch") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : ".";
                std::string pattern = action.fields.count("pattern") ? action.fields.at("pattern") : "";
                if (!pattern.empty()) {
                    auto results = FileManager::searchFiles(path, pattern, true);
                    std::stringstream ss;
                    ss << "搜索结果 (" << results.size() << " 个文件):\n";
                    for (const auto& file : results) {
                        ss << (file.isDirectory ? "[目录] " : "[文件] ") << file.fullPath << "\n";
                    }
                    fileResult = ss.str();
                    std::cout << "\n" << fileResult << std::endl;
                    exec_result = ExecutionResult{true, fileResult, ""};
                } else {
                    fileResult = "错误: 未指定搜索模式";
                    std::cerr << fileResult << std::endl;
                    exec_result = ExecutionResult{false, fileResult, ""};
                }
            } else if (action.action == "FileTree") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : ".";
                int depth = 3;
                if (action.fields.count("depth")) {
                    try { depth = std::stoi(action.fields.at("depth")); } catch (...) {}
                }
                fileResult = FileManager::getDirectoryTree(path, depth);
                std::cout << "\n目录树 (深度 " << depth << "):\n" << fileResult << std::endl;
                exec_result = ExecutionResult{true, fileResult, ""};
            } else if (action.action == "FileRun") {
                isFileAction = true;
                std::string path = action.fields.count("path") ? action.fields.at("path") : "";
                if (!path.empty()) {
                    std::cout << "执行文件: " << path << std::endl;
                    logMessage("执行文件: " + path);
                    
                    // 将UTF-8路径转换为宽字符
                    std::wstring wPath = AppManager::utf8ToWide(path);
                    
                    // 使用 ShellExecuteW 执行文件（支持Unicode路径）
                    HINSTANCE result = ShellExecuteW(NULL, L"open", wPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    bool success = (reinterpret_cast<INT_PTR>(result) > 32);
                    
                    if (success) {
                        fileResult = "文件执行成功: " + path;
                        std::cout << fileResult << std::endl;
                        logMessage(fileResult);
                    } else {
                        fileResult = "文件执行失败: " + path + " (错误码: " + std::to_string(reinterpret_cast<INT_PTR>(result)) + ")";
                        std::cerr << fileResult << std::endl;
                        logMessage(fileResult);
                    }
                    exec_result = ExecutionResult{success, fileResult, ""};
                } else {
                    fileResult = "错误: 未指定文件路径";
                    std::cerr << fileResult << std::endl;
                    exec_result = ExecutionResult{false, fileResult, ""};
                }
            } else {
                // 非文件管理动作，使用默认执行器
                exec_result = executor.execute(action);
            }
            
            logMessage("执行动作: " + action.action + " - " + (exec_result.success ? "成功" : "失败") + " - " + exec_result.message);
            
            if (action.action == "Execute" && exec_result.success) {
                lastExecuteOutput = exec_result.message;
                std::cout << "\n命令输出:\n" << lastExecuteOutput << std::endl;
            } else if (isFileAction && exec_result.success) {
                // 文件操作结果已经在上面输出，这里设置 lastExecuteOutput 供下次迭代使用
                lastExecuteOutput = fileResult;
            }
            
            Sleep(500);
            
            if (!exec_result.success) {
                std::cerr << "执行失败: " << exec_result.message << std::endl;
                logMessage("执行失败: " + exec_result.message);
                
                // 检查是否是坐标无效错误
                if (exec_result.message.find("无效的点击坐标") != std::string::npos ||
                    exec_result.message.find("无效的滑动坐标") != std::string::npos) {
                    // 给 AI 反馈，让它纠正
                    lastExecuteOutput = "错误: " + exec_result.message + "\n\n" +
                                       "提示: element/start/end 参数必须使用 [x,y] 坐标格式，或者是 [x1,y1,x2,y2] 矩形区域格式。\n" +
                                       "例如: element=[500,300] 或 element=[100,200,300,400]\n" +
                                       "请重新提供正确的坐标。";
                    std::cout << lastExecuteOutput << std::endl;
                    logMessage("坐标格式错误，已反馈给AI");
                    // 继续循环，让AI纠正
                    continue;
                }
                
                std::string repair_prompt = PromptTemplates::buildActionRepairPrompt(action.raw, false);
                break;
            }

            if (action.action == "finish") {
                std::cout << std::endl;
                std::cout << "任务完成!" << std::endl;
                logMessage("任务完成!");
                continueExecution = false;
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
                    std::cout << "等待 1 秒后重试..." << std::endl;
                    Sleep(1000);
                    shouldRetry = true;  // 设置重试标志
                    continue;  // 直接继续循环，不增加计数器
                }
            }

            std::cout << "\n按任意键退出..." << std::endl;
            _getch();
            return 1;
        }

        // 增加迭代计数
        iteration++;
        totalSteps++;
        
        // 检查是否达到最大迭代次数
        if (iteration >= max_iterations) {
            std::cout << std::endl;
            std::cout << "已达到最大迭代次数 (" << max_iterations << ")" << std::endl;
            std::cout << "是否继续执行? (y/n): ";
            char continueChoice;
            std::cin >> continueChoice;
            std::cin.ignore();
            
            if (continueChoice == 'y' || continueChoice == 'Y') {
                std::cout << "继续执行任务..." << std::endl;
                logMessage("用户选择继续执行任务");
                iteration = 0; // 重置迭代计数
            } else {
                continueExecution = false;
            }
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
