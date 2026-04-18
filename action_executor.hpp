#pragma once

#include "action_parser.hpp"
#include "file_manager.hpp"
#include "app_manager.hpp"
#include "ui_automation.hpp"
#include "mcp_client.hpp"
#include <windows.h>
#include <iostream>
#include <functional>
#include <sstream>
#include <regex>
#include <vector>
#include <map>
#include <cctype>
#include <algorithm>
#include <conio.h>

namespace aries {

struct ExecutionResult {
    bool success;
    std::string message;
    
    ExecutionResult(bool s = false, const std::string& m = "") 
        : success(s), message(m) {}
    
    // 兼容三个参数的构造函数（第三个参数被忽略）
    ExecutionResult(bool s, const std::string& m, const std::string&) 
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

        // 鼠标/键盘操作
        if (iequals(action.action, "Tap") || iequals(action.action, "Click")) {
            return executeTap(action);
        } else if (iequals(action.action, "RightClick")) {
            return executeRightClick(action);
        } else if (iequals(action.action, "Type")) {
            return executeType(action);
        } else if (iequals(action.action, "Swipe")) {
            return executeSwipe(action);
        
        // 导航操作
        } else if (iequals(action.action, "Back")) {
            return executeBack(action);
        } else if (iequals(action.action, "Home")) {
            return executeHome(action);
        } else if (iequals(action.action, "Wait")) {
            return executeWait(action);
        
        // 应用管理
        } else if (iequals(action.action, "Installed")) {
            return executeInstalled(action);
        } else if (iequals(action.action, "Launch")) {
            return executeLaunch(action);
        } else if (iequals(action.action, "Execute")) {
            return executeExecute(action);
        
        // 文件管理操作
        } else if (iequals(action.action, "FileList")) {
            return executeFileList(action);
        } else if (iequals(action.action, "FileRead")) {
            return executeFileRead(action);
        } else if (iequals(action.action, "FileHead")) {
            return executeFileHead(action);
        } else if (iequals(action.action, "FileTail")) {
            return executeFileTail(action);
        } else if (iequals(action.action, "FileRange")) {
            return executeFileRange(action);
        } else if (iequals(action.action, "FileWrite")) {
            return executeFileWrite(action);
        } else if (iequals(action.action, "FileAppend")) {
            return executeFileAppend(action);
        } else if (iequals(action.action, "FileMkdir")) {
            return executeFileMkdir(action);
        } else if (iequals(action.action, "FileDelete")) {
            return executeFileDelete(action);
        } else if (iequals(action.action, "FileMove")) {
            return executeFileMove(action);
        } else if (iequals(action.action, "FileCopy")) {
            return executeFileCopy(action);
        } else if (iequals(action.action, "FileInfo")) {
            return executeFileInfo(action);
        } else if (iequals(action.action, "FileSearch")) {
            return executeFileSearch(action);
        } else if (iequals(action.action, "FileTree")) {
            return executeFileTree(action);
        } else if (iequals(action.action, "FileRun")) {
            return executeFileRun(action);
        
        // 其他操作
        } else if (iequals(action.action, "Take_over")) {
            return executeTakeOver(action);
        } else if (iequals(action.action, "finish")) {
            return executeFinish(action);
        
        // UI Automation 操作
        } else if (iequals(action.action, "UIA_ListWindows")) {
            return executeUIAListWindows(action);
        } else if (iequals(action.action, "UIA_GetWindowTree")) {
            return executeUIAGetWindowTree(action);
        } else if (iequals(action.action, "UIA_GetActiveTree")) {
            return executeUIAGetActiveTree(action);
        } else if (iequals(action.action, "UIA_GetControlAtCursor")) {
            return executeUIAGetControlAtCursor(action);
        } else if (iequals(action.action, "UIA_GetControlAtPoint")) {
            return executeUIAGetControlAtPoint(action);
        } else if (iequals(action.action, "UIA_ClickControl")) {
            return executeUIAClickControl(action);
        
        // 窗口操作
        } else if (iequals(action.action, "Window_Minimize")) {
            return executeWindowMinimize(action);
        } else if (iequals(action.action, "Window_Maximize")) {
            return executeWindowMaximize(action);
        } else if (iequals(action.action, "Window_Restore")) {
            return executeWindowRestore(action);
        } else if (iequals(action.action, "Window_Close")) {
            return executeWindowClose(action);
        } else if (iequals(action.action, "Window_Activate")) {
            return executeWindowActivate(action);
        } else if (iequals(action.action, "Window_Topmost")) {
            return executeWindowTopmost(action);
        } else if (iequals(action.action, "Window_Move")) {
            return executeWindowMove(action);
        } else if (iequals(action.action, "Window_GetRect")) {
            return executeWindowGetRect(action);
        } else if (iequals(action.action, "Window_GetState")) {
            return executeWindowGetState(action);
        
        // MCP 工具调用
        } else if (iequals(action.action, "MCP_Tool")) {
            return executeMCPTool(action);
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

    // 命令安全检查：验证命令是否包含危险操作
    bool isCommandAllowed(const std::string& command) {
        // 转换为小写进行检查
        std::string lowerCmd = command;
        std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);
        
        // 危险命令黑名单模式
        static const std::vector<std::string> blockedPatterns = {
            // 文件删除相关
            "remove-item", "del ", "delete", "erase", "rmdir", "rd ",
            "rm -rf", "rm -r", "rm -f",
            // 格式化相关
            "format-volume", "format ", "clear-disk", "initialize-disk",
            // 执行策略绕过
            "set-executionpolicy", "-executionpolicy bypass",
            "-executionpolicy unrestricted", "-executionpolicy remotesigned",
            // 代码执行相关
            "invoke-expression", "iex", "invoke-command", "icm",
            // 网络下载相关
            "downloadstring", "downloadfile", "net.webclient",
            "system.net.webclient", "invoke-webrequest", "iwr",
            "start-bitstransfer",
            // 编码/混淆相关
            "-encodedcommand", "-enc ", "frombase64string",
            // 进程注入相关
            "virtualalloc", "createremotethread", "writeprocessmemory",
            // 注册表操作
            "set-itemproperty", "remove-itemproperty", "new-itemproperty",
            // 服务操作
            "new-service", "set-service", "remove-service",
            // WMI相关
            "invoke-wmimethod", "invoke-cimmethod",
            // 其他危险操作
            "start-process.*powershell", "start-process.*cmd",
            "[convert]::", "[system.convert]::"
        };
        
        for (const auto& pattern : blockedPatterns) {
            if (lowerCmd.find(pattern) != std::string::npos) {
                log("安全警告: 检测到危险命令模式 '" + std::string(pattern) + "'");
                return false;
            }
        }
        
        return true;
    }

    // 转义命令中的特殊字符，防止注入
    std::string escapeCommand(const std::string& command) {
        std::string escaped;
        escaped.reserve(command.length() * 2);
        
        for (char c : command) {
            switch (c) {
                case '"':
                    escaped += "`\"";  // PowerShell转义
                    break;
                case '`':
                    escaped += "``";
                    break;
                case '$':
                    escaped += "`$";
                    break;
                case '&':
                    escaped += "`&";
                    break;
                case '|':
                    escaped += "`|";
                    break;
                case ';':
                    escaped += "`;";
                    break;
                case '>':
                    escaped += "`>";
                    break;
                case '<':
                    escaped += "`<";
                    break;
                case '(':
                    escaped += "`(";
                    break;
                case ')':
                    escaped += "`)";
                    break;
                default:
                    escaped += c;
            }
        }
        
        return escaped;
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

    // 执行右键点击
    ExecutionResult executeRightClick(const ParsedAgentAction& action) {
        std::string element = readField(action.fields, {"element"});
        if (element.empty()) {
            return ExecutionResult(false, "未指定点击坐标");
        }

        auto point = parsePoint(element);
        if (point.first < 0) {
            return ExecutionResult(false, "无效的点击坐标: " + element);
        }

        auto [x, y] = parsePointToScreen(point.first, point.second);

        log("执行操作: 右键点击(" + std::to_string(x) + "," + std::to_string(y) + ")");

        // 移动鼠标到指定位置
        if (!SetCursorPos(x, y)) {
            return ExecutionResult(false, "移动鼠标失败");
        }

        // 等待
        Sleep(config_.tapAwaitWindowTimeoutMs);

        // 模拟右键点击
        INPUT inputs[2] = {};
        
        // 右键按下
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        
        // 右键释放
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        
        UINT result = SendInput(2, inputs, sizeof(INPUT));
        if (result != 2) {
            return ExecutionResult(false, "模拟右键点击失败");
        }

        // 等待
        Sleep(config_.tapAwaitWindowTimeoutMs);

        return ExecutionResult(true, "右键点击成功");
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

        // 检查是否是直接执行可执行文件（路径以 .exe 结尾）
        std::string trimmedCmd = command;
        // 去除首尾空格
        size_t start = trimmedCmd.find_first_not_of(" \t\r\n");
        size_t end = trimmedCmd.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            trimmedCmd = trimmedCmd.substr(start, end - start + 1);
        }
        
        // 如果是直接执行 exe 文件，使用 ShellExecute 而不是 PowerShell
        if (trimmedCmd.length() > 4 && 
            (trimmedCmd.substr(trimmedCmd.length() - 4) == ".exe" ||
             trimmedCmd.find(".exe ") != std::string::npos)) {
            log("检测到可执行文件，使用 ShellExecute 启动");
            
            // 提取文件路径（处理带空格的路径）
            std::string exePath = trimmedCmd;
            std::string arguments = "";
            
            // 如果路径被引号包围，提取引号内的内容
            if (exePath.front() == '"' && exePath.back() == '"') {
                exePath = exePath.substr(1, exePath.length() - 2);
            }
            
            // 使用 ShellExecute 执行
            HINSTANCE result = ShellExecuteA(NULL, "open", exePath.c_str(), 
                                              arguments.empty() ? NULL : arguments.c_str(), 
                                              NULL, SW_SHOWNORMAL);
            
            if (reinterpret_cast<INT_PTR>(result) > 32) {
                return ExecutionResult(true, "成功启动可执行文件: " + exePath);
            } else {
                return ExecutionResult(false, "启动可执行文件失败: " + exePath + 
                                       " (错误码: " + std::to_string(reinterpret_cast<INT_PTR>(result)) + ")");
            }
        }

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

    // ========== 文件管理操作 ==========
    
    ExecutionResult executeFileList(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        if (path.empty()) path = ".";
        
        log("执行操作: 列出目录 " + path);
        
        std::string result = FileManager::generateDirectoryDescription(path);
        std::cout << "\n" << result << std::endl;
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeFileRead(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        if (path.empty()) {
            return ExecutionResult(false, "错误: 未指定文件路径");
        }
        
        log("执行操作: 读取文件 " + path);
        
        std::string result = FileManager::readTextFile(path);
        // 检查是否返回错误
        if (result.find("Error:") == 0) {
            std::cerr << result << std::endl;
            return ExecutionResult(false, result);
        }
        std::cout << "\n文件内容:\n" << result << std::endl;
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeFileHead(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        if (path.empty()) {
            return ExecutionResult(false, "错误: 未指定文件路径");
        }
        
        int lines = 50;
        std::string linesStr = readField(action.fields, {"lines"});
        if (!linesStr.empty()) {
            try { lines = std::stoi(linesStr); } catch (...) {}
        }
        
        log("执行操作: 读取文件前 " + std::to_string(lines) + "行 " + path);
        
        std::string result = FileManager::readFileHead(path, lines);
        // 检查是否返回错误
        if (result.find("Error:") == 0) {
            std::cerr << result << std::endl;
            return ExecutionResult(false, result);
        }
        std::cout << "\n文件前 " << lines << " 行:\n" << result << std::endl;
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeFileTail(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        if (path.empty()) {
            return ExecutionResult(false, "错误: 未指定文件路径");
        }
        
        int lines = 50;
        std::string linesStr = readField(action.fields, {"lines"});
        if (!linesStr.empty()) {
            try { lines = std::stoi(linesStr); } catch (...) {}
        }
        
        log("执行操作: 读取文件后 " + std::to_string(lines) + "行 " + path);
        
        std::string result = FileManager::readFileTail(path, lines);
        // 检查是否返回错误
        if (result.find("Error:") == 0) {
            std::cerr << result << std::endl;
            return ExecutionResult(false, result);
        }
        std::cout << "\n文件后 " << lines << " 行:\n" << result << std::endl;
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeFileRange(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        if (path.empty()) {
            return ExecutionResult(false, "错误: 未指定文件路径");
        }
        
        int start = 1, end = 50;
        std::string startStr = readField(action.fields, {"start"});
        std::string endStr = readField(action.fields, {"end"});
        if (!startStr.empty()) {
            try { start = std::stoi(startStr); } catch (...) {}
        }
        if (!endStr.empty()) {
            try { end = std::stoi(endStr); } catch (...) {}
        }
        
        log("执行操作: 读取文件第 " + std::to_string(start) + "-" + std::to_string(end) + "行 " + path);
        
        std::string result = FileManager::readFileRange(path, start, end);
        // 检查是否返回错误
        if (result.find("Error:") == 0) {
            std::cerr << result << std::endl;
            return ExecutionResult(false, result);
        }
        std::cout << "\n文件第 " << start << "-" << end << " 行:\n" << result << std::endl;
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeFileWrite(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        std::string content = readField(action.fields, {"content"});
        
        if (path.empty()) {
            return ExecutionResult(false, "错误: 未指定文件路径");
        }
        
        log("执行操作: 写入文件 " + path);
        
        bool success = FileManager::writeTextFile(path, content, false);
        std::string result = success ? "文件写入成功" : "文件写入失败";
        std::cout << result << std::endl;
        return ExecutionResult(success, result);
    }
    
    ExecutionResult executeFileAppend(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        std::string content = readField(action.fields, {"content"});
        
        if (path.empty()) {
            return ExecutionResult(false, "错误: 未指定文件路径");
        }
        
        log("执行操作: 追加到文件 " + path);
        
        bool success = FileManager::writeTextFile(path, content, true);
        std::string result = success ? "文件追加成功" : "文件追加失败";
        std::cout << result << std::endl;
        return ExecutionResult(success, result);
    }
    
    ExecutionResult executeFileMkdir(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        
        if (path.empty()) {
            return ExecutionResult(false, "错误: 未指定目录路径");
        }
        
        log("执行操作: 创建目录 " + path);
        
        bool success = FileManager::createDirectory(path);
        std::string result = success ? "目录创建成功" : "目录创建失败";
        std::cout << result << std::endl;
        return ExecutionResult(success, result);
    }
    
    ExecutionResult executeFileDelete(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        
        if (path.empty()) {
            return ExecutionResult(false, "错误: 未指定文件路径");
        }
        
        log("执行操作: 删除文件/目录 " + path);
        
        bool success = FileManager::deleteFile(path);
        std::string result = success ? "删除成功" : "删除失败";
        std::cout << result << std::endl;
        return ExecutionResult(success, result);
    }
    
    ExecutionResult executeFileMove(const ParsedAgentAction& action) {
        std::string source = readField(action.fields, {"source", "src"});
        std::string destination = readField(action.fields, {"destination", "dest", "dst"});
        
        if (source.empty() || destination.empty()) {
            return ExecutionResult(false, "错误: 未指定源路径或目标路径");
        }
        
        log("执行操作: 移动 " + source + " -> " + destination);
        
        bool success = FileManager::move(source, destination);
        std::string result = success ? "移动成功" : "移动失败";
        std::cout << result << std::endl;
        return ExecutionResult(success, result);
    }
    
    ExecutionResult executeFileCopy(const ParsedAgentAction& action) {
        std::string source = readField(action.fields, {"source", "src"});
        std::string destination = readField(action.fields, {"destination", "dest", "dst"});
        
        if (source.empty() || destination.empty()) {
            return ExecutionResult(false, "错误: 未指定源路径或目标路径");
        }
        
        log("执行操作: 复制 " + source + " -> " + destination);
        
        bool success = FileManager::copyFile(source, destination, true);
        std::string result = success ? "复制成功" : "复制失败";
        std::cout << result << std::endl;
        return ExecutionResult(success, result);
    }
    
    ExecutionResult executeFileInfo(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        
        if (path.empty()) {
            return ExecutionResult(false, "错误: 未指定文件路径");
        }
        
        log("执行操作: 获取文件信息 " + path);
        
        std::string result = FileManager::getFileSummary(path);
        std::cout << "\n" << result << std::endl;
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeFileSearch(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        std::string pattern = readField(action.fields, {"pattern"});
        
        if (path.empty()) path = ".";
        if (pattern.empty()) {
            return ExecutionResult(false, "错误: 未指定搜索模式");
        }
        
        log("执行操作: 在 " + path + " 中搜索 " + pattern);
        
        auto results = FileManager::searchFiles(path, pattern, true);
        std::stringstream ss;
        ss << "搜索结果 (" << results.size() << " 个文件):\n";
        for (const auto& file : results) {
            ss << (file.isDirectory ? "[目录] " : "[文件] ") << file.fullPath << "\n";
        }
        std::string result = ss.str();
        std::cout << "\n" << result << std::endl;
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeFileTree(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        
        if (path.empty()) path = ".";
        
        int depth = 3;
        std::string depthStr = readField(action.fields, {"depth"});
        if (!depthStr.empty()) {
            try { depth = std::stoi(depthStr); } catch (...) {}
        }
        
        log("执行操作: 获取目录树 " + path + " (深度 " + std::to_string(depth) + ")");
        
        std::string result = FileManager::getDirectoryTree(path, depth);
        std::cout << "\n目录树 (深度 " << depth << "):\n" << result << std::endl;
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeFileRun(const ParsedAgentAction& action) {
        std::string path = readField(action.fields, {"path"});
        
        if (path.empty()) {
            return ExecutionResult(false, "错误: 未指定文件路径");
        }
        
        log("执行操作: 执行文件 " + path);
        
        // 将UTF-8路径转换为宽字符
        std::wstring wPath = AppManager::utf8ToWide(path);
        
        // 使用 ShellExecuteW 执行文件（支持Unicode路径）
        HINSTANCE result = ShellExecuteW(NULL, L"open", wPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        bool success = (reinterpret_cast<INT_PTR>(result) > 32);
        
        if (success) {
            std::string msg = "文件执行成功: " + path;
            std::cout << msg << std::endl;
            return ExecutionResult(true, msg);
        } else {
            std::string msg = "文件执行失败: " + path + " (错误码: " + std::to_string(reinterpret_cast<INT_PTR>(result)) + ")";
            std::cerr << msg << std::endl;
            return ExecutionResult(false, msg);
        }
    }
    
    ExecutionResult executeTakeOver(const ParsedAgentAction& action) {
        std::string message = readField(action.fields, {"message"});
        if (message.empty()) {
            message = "需要用户接管";
        }
        
        log("执行操作: 请求用户接管 - " + message);
        std::cout << "\n========================================" << std::endl;
        std::cout << "【用户接管请求】" << std::endl;
        std::cout << message << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "按任意键继续..." << std::endl;
        _getch();
        
        return ExecutionResult(true, "用户已接管");
    }

    // 模拟鼠标点击
    bool simulateMouseClick(int x, int y) {
        // 移动鼠标到指定位置
        SetCursorPos(x, y);
        Sleep(50);
        
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

    // 模拟键盘输入（支持中文）
    bool simulateKeyboardInput(const std::string& text) {
        // 将UTF-8字符串转换为宽字符字符串（Unicode）
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
        if (wideLen <= 0) {
            return false;
        }
        
        std::wstring wideText(wideLen - 1, 0); // -1 排除终止符
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wideText[0], wideLen);
        
        for (wchar_t wc : wideText) {
            // 对于ASCII字符，使用虚拟键码方式
            if (wc < 128) {
                char c = static_cast<char>(wc);
                SHORT vk = VkKeyScanA(c);
                if (vk == -1) {
                    continue;
                }
                
                BYTE vkCode = LOBYTE(vk);
                BYTE shiftState = HIBYTE(vk);
                
                // 如果需要 Shift
                if (shiftState & 1) {
                    INPUT shiftDown = {};
                    shiftDown.type = INPUT_KEYBOARD;
                    shiftDown.ki.wVk = VK_SHIFT;
                    SendInput(1, &shiftDown, sizeof(INPUT));
                }
                
                // 按键按下
                INPUT keyDown = {};
                keyDown.type = INPUT_KEYBOARD;
                keyDown.ki.wVk = vkCode;
                SendInput(1, &keyDown, sizeof(INPUT));
                
                // 按键释放
                INPUT keyUp = {};
                keyUp.type = INPUT_KEYBOARD;
                keyUp.ki.wVk = vkCode;
                keyUp.ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(1, &keyUp, sizeof(INPUT));
                
                // 释放 Shift
                if (shiftState & 1) {
                    INPUT shiftUp = {};
                    shiftUp.type = INPUT_KEYBOARD;
                    shiftUp.ki.wVk = VK_SHIFT;
                    shiftUp.ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(1, &shiftUp, sizeof(INPUT));
                }
            } else {
                // 对于非ASCII字符（如中文），使用Unicode输入
                INPUT inputs[2] = {};
                
                // 按键按下（Unicode）
                inputs[0].type = INPUT_KEYBOARD;
                inputs[0].ki.wScan = wc;
                inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
                
                // 按键释放（Unicode）
                inputs[1].type = INPUT_KEYBOARD;
                inputs[1].ki.wScan = wc;
                inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
                
                SendInput(2, inputs, sizeof(INPUT));
            }
            
            Sleep(10);
        }
        
        return true;
    }

    // 执行 PowerShell 命令（无输出）
    bool executePowerShellCommand(const std::string& command) {
        // 安全检查：验证命令是否允许执行
        if (!isCommandAllowed(command)) {
            log("命令被安全策略阻止: " + command);
            return false;
        }

        // 使用安全的命令构造函数
        std::string psCommand = ConstructPowerShellCommand(command);
        
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        
        // 创建Job Object以限制资源
        HANDLE hJob = CreateJobObject(NULL, NULL);
        if (hJob) {
            JOBOBJECT_BASIC_LIMIT_INFORMATION jobLimits = {0};
            jobLimits.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS | 
                                   JOB_OBJECT_LIMIT_PROCESS_TIME |
                                   JOB_OBJECT_LIMIT_WORKINGSET;
            jobLimits.ActiveProcessLimit = 3;  // 最多3个子进程
            jobLimits.PerProcessUserTimeLimit.QuadPart = 10000000LL * 10; // 10秒CPU时间
            jobLimits.MinimumWorkingSetSize = 0;
            jobLimits.MaximumWorkingSetSize = 100 * 1024 * 1024; // 100MB内存
            SetInformationJobObject(hJob, JobObjectBasicLimitInformation, &jobLimits, sizeof(jobLimits));
        }
        
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
            if (hJob) CloseHandle(hJob);
            return false;
        }
        
        // 将进程加入Job Object以应用限制
        if (hJob) {
            AssignProcessToJobObject(hJob, pi.hProcess);
        }
        
        // 等待进程完成
        WaitForSingleObject(pi.hProcess, 10000); // 最多等待10秒
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (hJob) CloseHandle(hJob);
        
        return true;
    }

    // 构造安全的 PowerShell 命令
    std::string ConstructPowerShellCommand(const std::string& script) {
        // 1. 强制进入受限语言模式
        std::string constrainedMode = "$ExecutionContext.SessionState.LanguageMode = 'ConstrainedLanguage'; ";
        // 2. 设置严格的执行策略
        std::string executionPolicy = " -ExecutionPolicy Restricted ";
        // 3. 禁止加载配置文件
        std::string noProfile = " -NoProfile ";
        // 4. 构造完整的命令
        return "powershell.exe" + noProfile + executionPolicy + "-Command \"& { " + constrainedMode + escapeCommand(script) + " }\"";
    }

    // 执行 PowerShell 命令并捕获输出
    std::string executePowerShellCommandWithOutput(const std::string& command) {
        // 安全检查：验证命令是否允许执行
        if (!isCommandAllowed(command)) {
            log("命令被安全策略阻止: " + command);
            return "错误: 命令被安全策略阻止";
        }

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
        
        // 使用安全的命令构造函数
        std::string psCommand = ConstructPowerShellCommand(command);
        
        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        
        PROCESS_INFORMATION pi;
        
        // 创建Job Object以限制资源
        HANDLE hJob = CreateJobObject(NULL, NULL);
        if (hJob) {
            JOBOBJECT_BASIC_LIMIT_INFORMATION jobLimits = {0};
            jobLimits.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS | 
                                   JOB_OBJECT_LIMIT_PROCESS_TIME |
                                   JOB_OBJECT_LIMIT_WORKINGSET;
            jobLimits.ActiveProcessLimit = 3;  // 最多3个子进程
            jobLimits.PerProcessUserTimeLimit.QuadPart = 10000000LL * 10; // 10秒CPU时间
            jobLimits.MinimumWorkingSetSize = 0;
            jobLimits.MaximumWorkingSetSize = 100 * 1024 * 1024; // 100MB内存
            SetInformationJobObject(hJob, JobObjectBasicLimitInformation, &jobLimits, sizeof(jobLimits));
        }
        
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
            if (hJob) CloseHandle(hJob);
            return "";
        }
        
        // 将进程加入Job Object以应用限制
        if (hJob) {
            AssignProcessToJobObject(hJob, pi.hProcess);
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
        WaitForSingleObject(pi.hProcess, 15000); // 最多等待15秒
        
        CloseHandle(hReadPipe);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (hJob) CloseHandle(hJob);
        
        return output;
    }

    // ==================== UI Automation 执行函数 ====================
    
    ExecutionResult executeUIAListWindows(const ParsedAgentAction& action) {
        UIAutomationTool uiaTool;
        if (!uiaTool.initialize()) {
            return ExecutionResult(false, "UI Automation 初始化失败");
        }
        
        std::string result = uiaTool.listTopLevelWindows();
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeUIAGetWindowTree(const ParsedAgentAction& action) {
        auto it = action.fields.find("window");
        if (it == action.fields.end()) {
            return ExecutionResult(false, "缺少 window 参数");
        }
        
        std::string windowTitle = it->second;
        int maxDepth = 3;
        
        auto depthIt = action.fields.find("depth");
        if (depthIt != action.fields.end()) {
            try {
                maxDepth = std::stoi(depthIt->second);
            } catch (...) {
                maxDepth = 3;
            }
        }
        
        UIAutomationTool uiaTool;
        if (!uiaTool.initialize()) {
            return ExecutionResult(false, "UI Automation 初始化失败");
        }
        
        std::string result = uiaTool.getWindowControlTree(windowTitle, maxDepth);
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeUIAGetActiveTree(const ParsedAgentAction& action) {
        int maxDepth = 3;
        
        auto depthIt = action.fields.find("depth");
        if (depthIt != action.fields.end()) {
            try {
                maxDepth = std::stoi(depthIt->second);
            } catch (...) {
                maxDepth = 3;
            }
        }
        
        UIAutomationTool uiaTool;
        if (!uiaTool.initialize()) {
            return ExecutionResult(false, "UI Automation 初始化失败");
        }
        
        std::string result = uiaTool.getActiveWindowControlTree(maxDepth);
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeUIAGetControlAtCursor(const ParsedAgentAction& action) {
        UIAutomationTool uiaTool;
        if (!uiaTool.initialize()) {
            return ExecutionResult(false, "UI Automation 初始化失败");
        }
        
        std::string result = uiaTool.getControlAtCursor();
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeUIAGetControlAtPoint(const ParsedAgentAction& action) {
        auto xIt = action.fields.find("x");
        auto yIt = action.fields.find("y");
        
        if (xIt == action.fields.end() || yIt == action.fields.end()) {
            return ExecutionResult(false, "缺少 x 或 y 参数");
        }
        
        int x, y;
        try {
            x = std::stoi(xIt->second);
            y = std::stoi(yIt->second);
        } catch (...) {
            return ExecutionResult(false, "x 或 y 参数格式错误");
        }
        
        UIAutomationTool uiaTool;
        if (!uiaTool.initialize()) {
            return ExecutionResult(false, "UI Automation 初始化失败");
        }
        
        std::string result = uiaTool.getControlAtPoint(x, y);
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeUIAClickControl(const ParsedAgentAction& action) {
        auto windowIt = action.fields.find("window");
        auto controlIt = action.fields.find("control");
        
        if (windowIt == action.fields.end() || controlIt == action.fields.end()) {
            return ExecutionResult(false, "缺少 window 或 control 参数");
        }
        
        std::string windowTitle = windowIt->second;
        std::string controlName = controlIt->second;
        
        UIAutomationTool uiaTool;
        if (!uiaTool.initialize()) {
            return ExecutionResult(false, "UI Automation 初始化失败");
        }
        
        bool success = uiaTool.clickControlByName(windowTitle, controlName);
        if (success) {
            return ExecutionResult(true, "成功点击控件: " + controlName);
        } else {
            return ExecutionResult(false, "无法点击控件: " + controlName);
        }
    }
    
    // ==================== 窗口操作执行函数 ====================
    
    ExecutionResult executeWindowMinimize(const ParsedAgentAction& action) {
        auto it = action.fields.find("window");
        std::string windowTitle = (it != action.fields.end()) ? it->second : "";
        
        UIAutomationTool uiaTool;
        if (uiaTool.minimizeWindow(windowTitle)) {
            return ExecutionResult(true, "窗口已最小化: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        } else {
            return ExecutionResult(false, "无法最小化窗口: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        }
    }
    
    ExecutionResult executeWindowMaximize(const ParsedAgentAction& action) {
        auto it = action.fields.find("window");
        std::string windowTitle = (it != action.fields.end()) ? it->second : "";
        
        UIAutomationTool uiaTool;
        if (uiaTool.maximizeWindow(windowTitle)) {
            return ExecutionResult(true, "窗口已最大化: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        } else {
            return ExecutionResult(false, "无法最大化窗口: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        }
    }
    
    ExecutionResult executeWindowRestore(const ParsedAgentAction& action) {
        auto it = action.fields.find("window");
        std::string windowTitle = (it != action.fields.end()) ? it->second : "";
        
        UIAutomationTool uiaTool;
        if (uiaTool.restoreWindow(windowTitle)) {
            return ExecutionResult(true, "窗口已还原: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        } else {
            return ExecutionResult(false, "无法还原窗口: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        }
    }
    
    ExecutionResult executeWindowClose(const ParsedAgentAction& action) {
        auto it = action.fields.find("window");
        std::string windowTitle = (it != action.fields.end()) ? it->second : "";
        
        UIAutomationTool uiaTool;
        if (uiaTool.closeWindow(windowTitle)) {
            return ExecutionResult(true, "窗口已关闭: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        } else {
            return ExecutionResult(false, "无法关闭窗口: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        }
    }
    
    ExecutionResult executeWindowActivate(const ParsedAgentAction& action) {
        auto it = action.fields.find("window");
        std::string windowTitle = (it != action.fields.end()) ? it->second : "";
        
        UIAutomationTool uiaTool;
        if (uiaTool.activateWindow(windowTitle)) {
            return ExecutionResult(true, "窗口已激活: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        } else {
            return ExecutionResult(false, "无法激活窗口: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        }
    }
    
    ExecutionResult executeWindowTopmost(const ParsedAgentAction& action) {
        auto it = action.fields.find("window");
        std::string windowTitle = (it != action.fields.end()) ? it->second : "";
        
        auto topmostIt = action.fields.find("topmost");
        bool topmost = true;
        if (topmostIt != action.fields.end()) {
            topmost = (topmostIt->second == "true" || topmostIt->second == "1");
        }
        
        UIAutomationTool uiaTool;
        if (uiaTool.setWindowTopmost(windowTitle, topmost)) {
            std::string status = topmost ? "置顶" : "取消置顶";
            return ExecutionResult(true, "窗口已" + status + ": " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        } else {
            return ExecutionResult(false, "无法设置窗口置顶状态: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        }
    }
    
    ExecutionResult executeWindowMove(const ParsedAgentAction& action) {
        auto it = action.fields.find("window");
        std::string windowTitle = (it != action.fields.end()) ? it->second : "";
        
        auto xIt = action.fields.find("x");
        auto yIt = action.fields.find("y");
        auto wIt = action.fields.find("width");
        auto hIt = action.fields.find("height");
        
        if (xIt == action.fields.end() || yIt == action.fields.end() ||
            wIt == action.fields.end() || hIt == action.fields.end()) {
            return ExecutionResult(false, "缺少位置或大小参数 (x, y, width, height)");
        }
        
        int x, y, width, height;
        try {
            x = std::stoi(xIt->second);
            y = std::stoi(yIt->second);
            width = std::stoi(wIt->second);
            height = std::stoi(hIt->second);
        } catch (...) {
            return ExecutionResult(false, "位置或大小参数格式错误");
        }
        
        UIAutomationTool uiaTool;
        if (uiaTool.moveWindow(windowTitle, x, y, width, height)) {
            return ExecutionResult(true, "窗口已移动到 (" + std::to_string(x) + ", " + std::to_string(y) + 
                                       ") 大小 " + std::to_string(width) + "x" + std::to_string(height));
        } else {
            return ExecutionResult(false, "无法移动窗口: " + (windowTitle.empty() ? "当前窗口" : windowTitle));
        }
    }
    
    ExecutionResult executeWindowGetRect(const ParsedAgentAction& action) {
        auto it = action.fields.find("window");
        std::string windowTitle = (it != action.fields.end()) ? it->second : "";
        
        UIAutomationTool uiaTool;
        std::string result = uiaTool.getWindowRect(windowTitle);
        return ExecutionResult(true, result);
    }
    
    ExecutionResult executeWindowGetState(const ParsedAgentAction& action) {
        auto it = action.fields.find("window");
        std::string windowTitle = (it != action.fields.end()) ? it->second : "";
        
        UIAutomationTool uiaTool;
        std::string result = uiaTool.getWindowState(windowTitle);
        return ExecutionResult(true, result);
    }
    
    // ==================== MCP 工具执行函数 ====================
    
    ExecutionResult executeMCPTool(const ParsedAgentAction& action) {
        auto serverIt = action.fields.find("server");
        auto toolIt = action.fields.find("tool");
        auto argsIt = action.fields.find("args");
        
        if (serverIt == action.fields.end()) {
            return ExecutionResult(false, "MCP_Tool 缺少 server 参数");
        }
        
        if (toolIt == action.fields.end()) {
            return ExecutionResult(false, "MCP_Tool 缺少 tool 参数");
        }
        
        std::string serverName = serverIt->second;
        std::string toolName = toolIt->second;
        std::string args = (argsIt != action.fields.end()) ? argsIt->second : "{}";
        
        // 获取 MCP 客户端（先尝试 stdio，再尝试 HTTP）
        auto& manager = mcp::MCPClientManager::getInstance();
        auto client = manager.getClient(serverName);
        auto httpClient = manager.getHttpClient(serverName);
        
        if (!client && !httpClient) {
            return ExecutionResult(false, "MCP 服务器未连接: " + serverName);
        }
        
        // 调用工具
        std::pair<bool, std::string> result;
        if (client) {
            result = client->callTool(toolName, args);
        } else {
            result = httpClient->callTool(toolName, args);
        }
        
        if (result.first) {
            return ExecutionResult(true, "MCP 工具调用成功: " + toolName + "\n结果: " + result.second);
        } else {
            return ExecutionResult(false, "MCP 工具调用失败: " + toolName + " - " + result.second);
        }
    }
};

} // namespace aries
