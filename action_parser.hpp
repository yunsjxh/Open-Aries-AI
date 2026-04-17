#pragma once

#include <string>
#include <map>
#include <regex>
#include <algorithm>
#include <optional>

namespace aries {

// 解析后的动作结构
struct ParsedAgentAction {
    std::string action;
    std::optional<std::string> target;
    std::map<std::string, std::string> fields;
    std::string raw;

    ParsedAgentAction() = default;
    ParsedAgentAction(const std::string& a, const std::optional<std::string>& t, 
                      const std::map<std::string, std::string>& f, const std::string& r)
        : action(a), target(t), fields(f), raw(r) {}

    bool isValid() const {
        return !action.empty();
    }
};

class ActionParser {
public:
    ParsedAgentAction parse(const std::string& raw) {
        // 先解码 Unicode 转义序列
        std::string decoded = decodeUnicodeEscapes(raw);
        std::string original = trim(decoded);

        // 检测截断迹象
        bool hasTruncationSign = original.find("\uFFFD") != std::string::npos ||
                                 original.find("...") != std::string::npos ||
                                 original.find("…") != std::string::npos ||
                                 (original.find("我需要") != std::string::npos && 
                                  original.find("do(") == std::string::npos && 
                                  original.find("finish(") == std::string::npos);

        // 检测长文本无动作的情况
        if (original.find("text=\"") != std::string::npos && 
            std::count(original.begin(), original.end(), '=') > 10 &&
            original.find("do(") == std::string::npos && 
            original.find("finish(") == std::string::npos) {
            return ParsedAgentAction("unknown", std::nullopt, {}, original.substr(0, 200));
        }

        // 截断检测
        if (hasTruncationSign && original.find("do(") == std::string::npos && 
            original.find("finish(") == std::string::npos) {
            return ParsedAgentAction("unknown", std::nullopt, {}, "输出被截断，未包含动作");
        }

        // 尝试从 <answer> 标签中提取动作
        std::string answerFromTag = extractFromAnswerTag(original);
        if (!answerFromTag.empty()) {
            return parseActionFromString(answerFromTag);
        }

        // 尝试从 <|begin_of_box|> 标签中提取动作
        std::string answerFromBox = extractFromBoxTag(original);
        if (!answerFromBox.empty()) {
            return parseActionFromString(answerFromBox);
        }

        // 尝试从 <tool_call> 标签中提取动作
        std::string answerFromToolCall = extractFromToolCallTag(original);
        if (!answerFromToolCall.empty()) {
            return parseActionFromString(answerFromToolCall);
        }

        // 尝试从【回答开始】【回答结束】标签提取
        std::string answerFromChinese = extractFromChineseTag(original, "回答");
        if (!answerFromChinese.empty()) {
            return parseActionFromString(answerFromChinese);
        }

        // 查找动作开始位置（兜底）
        size_t finishIndex = original.rfind("finish(");
        size_t doIndex = original.rfind("do(");
        
        size_t startIndex;
        if (finishIndex != std::string::npos && doIndex != std::string::npos) {
            startIndex = std::max(finishIndex, doIndex);
        } else if (finishIndex != std::string::npos) {
            startIndex = finishIndex;
        } else if (doIndex != std::string::npos) {
            startIndex = doIndex;
        } else {
            startIndex = std::string::npos;
        }

        std::string trimmed = (startIndex != std::string::npos) ? 
                              original.substr(startIndex) : original;

        return parseActionFromString(trimmed);
    }

    // 添加公共接口用于测试解码
    std::string testDecode(const std::string& raw) {
        return decodeUnicodeEscapes(raw);
    }

private:
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }

    // 解码 Unicode 转义序列 (\u003c 或 u003c -> <)
    std::string decodeUnicodeEscapes(const std::string& str) {
        std::string result;
        size_t i = 0;
        while (i < str.length()) {
            // 检查 \uXXXX 格式
            if (i + 6 <= str.length() && str[i] == '\\' && str[i+1] == 'u') {
                std::string hex = str.substr(i+2, 4);
                bool valid = true;
                for (char c : hex) {
                    if (!std::isxdigit(c)) {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    int code = std::stoi(hex, nullptr, 16);
                    result += static_cast<char>(code);
                    i += 6;
                    continue;
                }
            }
            // 检查 uXXXX 格式 (没有反斜杠)
            if (i + 4 <= str.length() && str[i] == 'u') {
                std::string hex = str.substr(i+1, 4);
                bool valid = true;
                for (char c : hex) {
                    if (!std::isxdigit(c)) {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    int code = std::stoi(hex, nullptr, 16);
                    result += static_cast<char>(code);
                    i += 5;
                    continue;
                }
            }
            result += str[i];
            i++;
        }
        return result;
    }

    std::string extractFromAnswerTag(const std::string& original) {
        // 使用简单的字符串查找而不是 regex dotall
        size_t start = original.find("<answer>");
        size_t end = original.find("</answer>");
        if (start != std::string::npos && end != std::string::npos && start < end) {
            return trim(original.substr(start + 8, end - start - 8));
        }
        return "";
    }

    std::string extractFromBoxTag(const std::string& original) {
        // 支持 <|begin_of_box|> 和 <|end_of_box|> 标签
        std::string startTag = "<|begin_of_box|>";
        std::string endTag = "<|end_of_box|>";
        size_t start = original.find(startTag);
        size_t end = original.find(endTag);
        if (start != std::string::npos && end != std::string::npos && start < end) {
            return trim(original.substr(start + startTag.length(), end - start - startTag.length()));
        }
        return "";
    }

    std::string extractFromToolCallTag(const std::string& original) {
        // 支持 <tool_call> 和 </tool_call> 或 <tool_call> 和 <|end_of_box|> 标签
        size_t start = original.find("<tool_call>");
        if (start == std::string::npos) return "";
        
        size_t end = original.find("</tool_call>");
        if (end == std::string::npos) {
            end = original.find("<|end_of_box|>");
        }
        if (end != std::string::npos && start < end) {
            return trim(original.substr(start + 11, end - start - 11));
        }
        return "";
    }

    std::string extractFromChineseTag(const std::string& original, const std::string& tagName) {
        std::string startTag = "【" + tagName + "开始】";
        std::string endTag = "【" + tagName + "结束】";
        
        size_t start = original.find(startTag);
        size_t end = original.find(endTag);
        
        if (start != std::string::npos && end != std::string::npos && start < end) {
            return trim(original.substr(start + startTag.length(), end - start - startTag.length()));
        }
        return "";
    }

    ParsedAgentAction parseActionFromString(const std::string& actionStr) {
        std::string trimmed = trim(actionStr);
        
        // 检测 finish 动作
        if (trimmed.find("finish(") != std::string::npos) {
            return parseFinishAction(trimmed);
        }
        
        // 检测 do 动作
        if (trimmed.find("do(") != std::string::npos) {
            return parseDoAction(trimmed);
        }

        return ParsedAgentAction("unknown", std::nullopt, {}, trimmed);
    }

    ParsedAgentAction parseFinishAction(const std::string& actionStr) {
        std::map<std::string, std::string> fields = parseFields(actionStr);
        return ParsedAgentAction("finish", std::nullopt, fields, actionStr);
    }

    ParsedAgentAction parseDoAction(const std::string& actionStr) {
        std::map<std::string, std::string> fields = parseFields(actionStr);
        
        std::string actionType = "unknown";
        auto it = fields.find("action");
        if (it != fields.end()) {
            actionType = it->second;
            fields.erase(it);
        }

        // 支持 UI Automation 相关动作
        // uia_list_windows, uia_get_window_tree, uia_get_active_tree, 
        // uia_get_control_at_cursor, uia_get_control_at_point, uia_click_control

        return ParsedAgentAction(actionType, std::nullopt, fields, actionStr);
    }

    std::map<std::string, std::string> parseFields(const std::string& actionStr) {
        std::map<std::string, std::string> fields;
        
        // 跳过 do( 或 finish( 前缀
        size_t pos = 0;
        if (actionStr.find("do(") == 0) {
            pos = 3; // 跳过 "do("
        } else if (actionStr.find("finish(") == 0) {
            pos = 7; // 跳过 "finish("
        }
        
        // 手动解析 key="value" 或 key='value' 或 key=value
        while (pos < actionStr.length()) {
            // 查找 key
            size_t keyStart = actionStr.find_first_not_of(" \t\n\r,()", pos);
            if (keyStart == std::string::npos) break;
            
            size_t eqPos = actionStr.find('=', keyStart);
            if (eqPos == std::string::npos) break;
            
            std::string key = trim(actionStr.substr(keyStart, eqPos - keyStart));
            
            // 查找 value
            size_t valStart = actionStr.find_first_not_of(" \t", eqPos + 1);
            if (valStart == std::string::npos) break;
            
            std::string value;
            if (actionStr[valStart] == '"') {
                // 双引号字符串
                size_t valEnd = actionStr.find('"', valStart + 1);
                if (valEnd == std::string::npos) {
                    // 未找到结束引号，取到字符串末尾或右括号（不是逗号，因为值内部可能有逗号）
                    valEnd = actionStr.find(')', valStart + 1);
                    if (valEnd == std::string::npos) valEnd = actionStr.length();
                    value = actionStr.substr(valStart + 1, valEnd - valStart - 1);
                } else {
                    value = actionStr.substr(valStart + 1, valEnd - valStart - 1);
                }
                pos = valEnd + 1;
            } else if (actionStr[valStart] == '\'') {
                // 单引号字符串
                size_t valEnd = actionStr.find('\'', valStart + 1);
                if (valEnd == std::string::npos) {
                    // 未找到结束引号，取到字符串末尾或右括号（不是逗号，因为值内部可能有逗号）
                    valEnd = actionStr.find(')', valStart + 1);
                    if (valEnd == std::string::npos) valEnd = actionStr.length();
                    value = actionStr.substr(valStart + 1, valEnd - valStart - 1);
                } else {
                    value = actionStr.substr(valStart + 1, valEnd - valStart - 1);
                }
                pos = valEnd + 1;
            } else {
                // 无引号值
                // 特殊处理：如果值以 '[' 开头，需要找到匹配的 ']'
                if (actionStr[valStart] == '[') {
                    size_t bracketEnd = actionStr.find(']', valStart);
                    if (bracketEnd != std::string::npos) {
                        value = trim(actionStr.substr(valStart, bracketEnd - valStart + 1));
                        pos = bracketEnd + 1;
                    } else {
                        // 未找到匹配的 ']'，取到下一个逗号或右括号
                        size_t valEnd = actionStr.find_first_of(",)", valStart);
                        if (valEnd == std::string::npos) valEnd = actionStr.length();
                        value = trim(actionStr.substr(valStart, valEnd - valStart));
                        pos = valEnd;
                    }
                } else {
                    // 普通无引号值
                    size_t valEnd = actionStr.find_first_of(",)", valStart);
                    if (valEnd == std::string::npos) valEnd = actionStr.length();
                    value = trim(actionStr.substr(valStart, valEnd - valStart));
                    pos = valEnd;
                }
            }
            
            fields[key] = value;
        }

        return fields;
    }
};

} // namespace aries
