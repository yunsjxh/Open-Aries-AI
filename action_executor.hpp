#pragma once

#include "action_parser.hpp"
#include <windows.h>
#include <iostream>
#include <functional>
#include <sstream>
#include <regex>
#include <vector>
#include <map>
#include <cctype>

namespace aries {

struct ExecutionResult {
    bool success;
    std::string message;
    
    ExecutionResult(bool s = false, const std::string& m = "") 
        : success(s), message(m) {}
};

using LogCallback = std::function<void(const std::string&)>;

class ActionExecutor {
public:
    struct Config {
        int screenWidth;
        int screenHeight;
        int tapAwaitWindowTimeoutMs;
        std::vector<std::string> sensitiveKeywords;
        
        Config() : screenWidth(1920), screenHeight(1080), tapAwaitWindowTimeoutMs(500) {}
    };

    ActionExecutor(const Config& config = Config()) : config_(config) {}

    void setLogCallback(LogCallback callback) {
        logCallback_ = callback;
    }

    static bool iequals(const std::string& a, const std::string& b) {
        if (a.length() != b.length()) return false;
        for (size_t i = 0; i < a.length(); ++i) {
            if (std::tolower(a[i]) != std::tolower(b[i])) return false;
        }
        return true;
    }

    ExecutionResult execute(const ParsedAgentAction& action) {
        if (!action.isValid()) {
            return ExecutionResult(false, "无效的动作");
        }

        if (iequals(action.action, "Tap") || iequals(action.action, "Click")) {
            return executeTap(action);
        } else if (iequals(action.action, "Type")) {
            return executeType(action);
        } else if (iequals(action.action, "Swipe")) {
            return executeSwipe(action);
        } else if (iequals(action.action, "Back")) {
            return executeBack(action);
        } else if (iequals(action.action, "Home")) {
            return executeHome(action);
        } else if (iequals(action.action, "Wait")) {
            return executeWait(action);
        } else if (iequals(action.action, "Installed")) {
            return executeInstalled(action);
        } else if (iequals(action.action, "Launch")) {
            return executeLaunch(action);
        } else if (iequals(action.action, "Execute")) {
            return executeExecute(action);
        } else if (iequals(action.action, "finish")) {
            return executeFinish(action);
        } else {
            return ExecutionResult(false, "未知的动作类型: " + action.action);
        }
    }

private:
    Config config_;
    LogCallback logCallback_;

    void log(const std::string& message) {
        if (logCallback_) {
            logCallback_(message);
        }
        std::cout << message << std::endl;
    }

    std::pair<int, int> parsePoint(const std::string& pointStr) {
        log("parsePoint 输入: '" + pointStr + "'");
        
        std::string cleaned = pointStr;
        if (!cleaned.empty() && (cleaned[0] == '\'' || cleaned[0] == '"')) {
            cleaned = cleaned.substr(1);
        }
        if (!cleaned.empty() && (cleaned.back() == '\'' || cleaned.back() == '"')) {
            cleaned.pop_back();
        }
        if (!cleaned.empty() && cleaned[0] == '[' && cleaned.back() != ']') {
            cleaned += ']';
        }
        
        log("parsePoint 清理后: '" + cleaned + "'");
        
        std::regex rectRegex(R"(\[(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\])");
        std::smatch rectMatch;
        if (std::regex_search(cleaned, rectMatch, rectRegex) && rectMatch.size() > 4) {
            int x1 = std::stoi(rectMatch[1].str());
            int y1 = std::stoi(rectMatch[2].str());
            int x2 = std::stoi(rectMatch[3].str());
            int y2 = std::stoi(rectMatch[4].str());
            // 取矩形中心点
            int centerX = (x1 + x2) / 2;
            int centerY = (y1 + y2) / 2;
            return {centerX, centerY};
        }
        
        std::regex pointRegex(R"(\[(\d+),\s*(\d+)\])");
        std::smatch match;
        if (std::regex_search(cleaned, match, pointRegex) && match.size() > 2) {
            int x = std::stoi(match[1].str());
            int y = std::stoi(match[2].str());
            return {x, y};
        }
        return {-1, -1};
    }

    std::pair<int, int> parsePointToScreen(int xRel, int yRel) {
        int x = (xRel * config_.screenWidth) / 1000;
        int y = (yRel * config_.screenHeight) / 1000;
        return {x, y};
    }

    std::string readField(const std::map<std::string, std::string>& fields, 
                          const std::vector<std::string>& keys) {
        for (const auto& key : keys) {
            auto it = fields.find(key);
            if (it != fields.end()) {
                return it->second;
            }
        }
        return "";
    }

    ExecutionResult executeTap(const ParsedAgentAction& action) {
        std::string elementStr = readField(action.fields, {"element", "point", "pos"});
        std::string xStr = readField(action.fields, {"x"});
        std::string yStr = readField(action.fields, {"y"});

        int xRel = -1, yRel = -1;

        if (!elementStr.empty()) {
            log("解析 element 字符串: '" + elementStr + "'");
            auto point = parsePoint(elementStr);
            log("解析结果: x=" + std::to_string(point.first) + ", y=" + std::to_string(point.second));
            if (point.first >= 0) {
                xRel = point.first;
                yRel = point.second;
            }
        }

        if (xRel < 0 && !xStr.empty()) {
            xRel = std::stoi(xStr);
        }
        if (yRel < 0 && !yStr.empty()) {
            yRel = std::stoi(yStr);
        }

        if (xRel < 0 || yRel < 0) {
            return ExecutionResult(false, "无效的点击坐标");
        }

        auto [x, y] = parsePointToScreen(xRel, yRel);
        log("执行操作: 点击(" + std::to_string(xRel) + "," + std::to_string(yRel) + ") -> 屏幕(" + 
            std::to_string(x) + "," + std::to_string(y) + ")");

        // 执行 Windows 点击
        if (!simulateMouseClick(x, y)) {
            return ExecutionResult(false, "模拟鼠标点击失败");
        }

        // 等待
        Sleep(config_.tapAwaitWindowTimeoutMs);

        return ExecutionResult(true, "点击成功");
    }

    // 执行输入
    ExecutionResult executeType(const ParsedAgentAction& action) {
        std::string text = readField(action.fields, {"text"});
        if (text.empty()) {
            return ExecutionResult(false, "输入内容为空");
        }

        log("执行操作: 输入文本 \"" + text + "\"");

        // 模拟键盘输入
        if (!simulateKeyboardInput(text)) {
            return ExecutionResult(false, "模拟键盘输入失败");
        }

        return ExecutionResult(true, "输入成功");
    }

    // 执行滑动
    ExecutionResult executeSwipe(const ParsedAgentAction& action) {
        std::string startStr = readField(action.fields, {"start"});
        std::string endStr = readField(action.fields, {"end"});

        auto startPoint = parsePoint(startStr);
        auto endPoint = parsePoint(endStr);

        if (startPoint.first < 0 || endPoint.first < 0) {
            return ExecutionResult(false, "无效的滑动坐标");
        }

        auto [x1, y1] = parsePointToScreen(startPoint.first, startPoint.second);
        auto [x2, y2] = parsePointToScreen(endPoint.first, endPoint.second);

        log("执行操作: 滑动从(" + std::to_string(x1) + "," + std::to_string(y1) + ")到(" + 
            std::to_string(x2) + "," + std::to_string(y2) + ")");

        if (!simulateMouseDrag(x1, y1, x2, y2)) {
            return ExecutionResult(false, "模拟鼠标拖动失败");
        }

        return ExecutionResult(true, "滑动成功");
    }

    // 执行返回
    ExecutionResult executeBack(const ParsedAgentAction& action) {
        log("执行操作: 返回");
        
        // 模拟 Alt+Left 或 Backspace
        INPUT inputs[4] = {};
        
        // Alt down
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_MENU;
        
        // Left down
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_LEFT;
        
        // Left up
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = VK_LEFT;
        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
        
        // Alt up
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = VK_MENU;
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
        
        SendInput(4, inputs, sizeof(INPUT));
        
        return ExecutionResult(true, "返回成功");
    }

    // 执行主页
    ExecutionResult executeHome(const ParsedAgentAction& action) {
        log("执行操作: 主页");
        
        // 模拟 Win 键
        INPUT inputs[2] = {};
        
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_LWIN;
        
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_LWIN;
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
        
        SendInput(2, inputs, sizeof(INPUT));
        
        return ExecutionResult(true, "主页成功");
    }

    // 执行等待
    ExecutionResult executeWait(const ParsedAgentAction& action) {
        std::string durationStr = readField(action.fields, {"duration"});
        int duration = durationStr.empty() ? 3 : std::stoi(durationStr);
        
        log("执行操作: 等待 " + std::to_string(duration) + " 秒");
        Sleep(duration * 1000);
        
        return ExecutionResult(true, "等待完成");
    }

    // 执行完成
    ExecutionResult executeFinish(const ParsedAgentAction& action) {
        std::string message = readField(action.fields, {"message"});
        log("执行操作: 完成任务 - " + message);
        return ExecutionResult(true, "任务完成: " + message);
    }

    // 执行获取已安装应用列表
    ExecutionResult executeInstalled(const ParsedAgentAction& action) {
        log("执行操作: 获取已安装应用列表");
        // 实际获取逻辑在主程序中处理
        return ExecutionResult(true, "已请求获取应用列表");
    }

    // 执行启动应用
    ExecutionResult executeLaunch(const ParsedAgentAction& action) {
        std::string app = readField(action.fields, {"app"});
        if (app.empty()) {
            return ExecutionResult(false, "应用名称为空");
        }

        log("执行操作: 启动应用 \"" + app + "\"");

        // 应用名称映射（中文名称 -> 实际命令）
        std::map<std::string, std::string> appMapping = {
            {"任务管理器", "taskmgr"},
            {"计算器", "calc"},
            {"记事本", "notepad"},
            {"画图", "mspaint"},
            {"命令提示符", "cmd"},
            {"PowerShell", "powershell"},
            {"资源管理器", "explorer"},
            {"控制面板", "control"},
            {"设置", "ms-settings:"},
            {"浏览器", "start msedge"},
            {"edge", "start msedge"},
            {"chrome", "start chrome"}
        };

        // 查找映射，如果没有映射则使用原名称
        std::string actualCommand = app;
        auto it = appMapping.find(app);
        if (it != appMapping.end()) {
            actualCommand = it->second;
            log("应用名称映射: " + app + " -> " + actualCommand);
        }

        // 优先使用 PowerShell 执行
        std::string psCommand = "Start-Process " + actualCommand;
        if (executePowerShellCommand(psCommand)) {
            return ExecutionResult(true, "通过 PowerShell 启动应用: " + app);
        }

        // 如果 PowerShell 失败，回退到 Win+R 方法
        log("PowerShell 执行失败，回退到 Win+R 方法");
        
        // 使用 Win+R 打开运行对话框，然后输入应用名称
        // 模拟 Win+R
        INPUT inputs[4] = {};
        
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_LWIN;
        
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = 'R';
        
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'R';
        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
        
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = VK_LWIN;
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
        
        SendInput(4, inputs, sizeof(INPUT));
        
        Sleep(500); // 等待运行对话框打开
        
        // 输入应用名称
        simulateKeyboardInput(actualCommand);
        
        Sleep(100);
        
        // 按回车键
        INPUT enterInput[2] = {};
        enterInput[0].type = INPUT_KEYBOARD;
        enterInput[0].ki.wVk = VK_RETURN;
        enterInput[1].type = INPUT_KEYBOARD;
        enterInput[1].ki.wVk = VK_RETURN;
        enterInput[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, enterInput, sizeof(INPUT));
        
        return ExecutionResult(true, "启动应用: " + app);
    }

    // 执行 PowerShell 命令并捕获输出
    ExecutionResult executeExecute(const ParsedAgentAction& action) {
        std::string command = readField(action.fields, {"command", "cmd"});
        if (command.empty()) {
            return ExecutionResult(false, "命令为空");
        }

        log("执行操作: 执行命令 \"" + command + "\"");

        // 执行命令并捕获输出
        std::string output = executePowerShellCommandWithOutput(command);
        
        if (!output.empty()) {
            // 截取输出，避免过长
            if (output.length() > 2000) {
                output = output.substr(0, 2000) + "\n... (输出已截断)";
            }
            return ExecutionResult(true, "执行命令: " + command + "\n输出:\n" + output);
        } else {
            return ExecutionResult(true, "执行命令: " + command + " (无输出)");
        }
    }

    // 模拟鼠标点击
    bool simulateMouseClick(int x, int y) {
        // 移动鼠标
        SetCursorPos(x, y);
        
        // 左键按下
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Sleep(50);
        // 左键释放
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        
        return true;
    }

    // 模拟鼠标拖动
    bool simulateMouseDrag(int x1, int y1, int x2, int y2) {
        // 移动鼠标到起始位置
        SetCursorPos(x1, y1);
        Sleep(100);
        
        // 左键按下
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Sleep(100);
        
        // 移动鼠标到结束位置
        SetCursorPos(x2, y2);
        Sleep(100);
        
        // 左键释放
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        
        return true;
    }

    // 模拟键盘输入
    bool simulateKeyboardInput(const std::string& text) {
        for (char c : text) {
            SHORT vk = VkKeyScanA(c);
            if (vk == -1) {
                // 字符无法映射，尝试直接发送
                continue;
            }
            
            BYTE vkCode = LOBYTE(vk);
            BYTE shiftState = HIBYTE(vk);
            
            INPUT inputs[2] = {};
            
            // 如果需要 Shift
            if (shiftState & 1) {
                INPUT shiftInput[2] = {};
                shiftInput[0].type = INPUT_KEYBOARD;
                shiftInput[0].ki.wVk = VK_SHIFT;
                shiftInput[1].type = INPUT_KEYBOARD;
                shiftInput[1].ki.wVk = VK_SHIFT;
                shiftInput[1].ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(1, &shiftInput[0], sizeof(INPUT));
            }
            
            // 按键按下
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = vkCode;
            
            // 按键释放
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = vkCode;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
            
            SendInput(2, inputs, sizeof(INPUT));
            
            // 释放 Shift
            if (shiftState & 1) {
                INPUT shiftInput[2] = {};
                shiftInput[0].type = INPUT_KEYBOARD;
                shiftInput[0].ki.wVk = VK_SHIFT;
                shiftInput[0].ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(1, &shiftInput[0], sizeof(INPUT));
            }
            
            Sleep(10);
        }
        
        return true;
    }

    // 执行 PowerShell 命令（无输出）
    bool executePowerShellCommand(const std::string& command) {
        // 构建 PowerShell 命令行
        std::string psCommand = "powershell.exe -Command \"" + command + "\"";
        
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        
        BOOL success = CreateProcessA(
            NULL,
            const_cast<char*>(psCommand.c_str()),
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi
        );
        
        if (!success) {
            return false;
        }
        
        // 等待进程完成
        WaitForSingleObject(pi.hProcess, 5000); // 最多等待5秒
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        return true;
    }

    // 执行 PowerShell 命令并捕获输出
    std::string executePowerShellCommandWithOutput(const std::string& command) {
        std::string output;
        
        // 创建管道用于捕获输出
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;
        
        HANDLE hReadPipe, hWritePipe;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            return "";
        }
        
        // 确保读取端不从子进程继承
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
        
        // 构建 PowerShell 命令行
        std::string psCommand = "powershell.exe -Command \"" + command + "\"";
        
        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        
        PROCESS_INFORMATION pi;
        
        BOOL success = CreateProcessA(
            NULL,
            const_cast<char*>(psCommand.c_str()),
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi
        );
        
        if (!success) {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return "";
        }
        
        // 关闭写入端，开始读取
        CloseHandle(hWritePipe);
        
        // 读取输出
        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }
        
        // 等待进程完成
        WaitForSingleObject(pi.hProcess, 10000); // 最多等待10秒
        
        CloseHandle(hReadPipe);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        return output;
    }
};

} // namespace aries
