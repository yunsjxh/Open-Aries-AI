#pragma once

#include <string>
#include <sstream>
#include <numeric>
#include <regex>
#include <algorithm>

namespace aries {

class PromptTemplates {
public:
    // 计算最大公约数
    static int gcd(int a, int b) {
        while (b != 0) {
            int temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

    // 构建系统提示词
    static std::string buildSystemPrompt(int screenW, int screenH, bool enforceDesc = false) {
        std::string ratio;
        if (screenW > 0 && screenH > 0) {
            int d = gcd(screenW, screenH);
            ratio = std::to_string(screenW / d) + ":" + std::to_string(screenH / d);
        } else {
            ratio = "1:1";
        }

        std::string descRuleText = buildSystemDescRule(enforceDesc);
        std::string intentTextRuleText = buildSystemIntentTextRule(enforceDesc);

        std::stringstream ss;
        ss << R"(# 移动 UI 自动化核心提示词

请直接输出动作，不要输出其他说明。
输出格式：
<answer>
	do(action="操作名", 参数="值", desc="动作简述")
</answer>

可接受输出：
<answer>
	finish(message="完成任务")
</answer>

动作说明：
- 查看已安装应用: do(action="Installed", desc="获取应用列表") - 返回已安装应用列表，下次迭代会带上列表
- 启动应用: do(action="Launch", app="应用名", desc="启动XXX") - 优先使用此动作启动应用
- 执行命令: do(action="Execute", command="命令", desc="执行XXX") - 使用PowerShell执行命令
- 点击: do(action="Tap", element=[500,300], desc="点击搜索框")
- 输入: do(action="Type", text="内容", desc="输入内容")
- 滑动: do(action="Swipe", start=[500,800], end=[500,200], desc="向上滑动")
- 返回: do(action="Back", desc="返回上一级")
- 主页: do(action="Home", desc="回到主页")
- 等待: do(action="Wait", duration="3", desc="等待页面加载")
- 人工接管: do(action="Take_over", message="需要用户确认", desc="请求用户接管")
- 完成: finish(message="任务完成")

执行策略（重要）：
1. 启动应用时，系统会优先查找已安装应用列表并直接启动
2. 如果需要查看可启动的应用，先返回 Installed 动作获取列表
3. 优先使用 PowerShell 执行命令（通过 Execute 动作）
4. 只有在 PowerShell 无法完成时（如需要与特定UI元素交互），才使用模拟键盘（Type动作）
5. 对于打开应用、执行系统命令等操作，不要直接使用 Type 输入命令，而应该使用 Launch 或 Execute

坐标规则 0-1000。
建议流程：
1. 直接执行可执行动作
2. 执行点击/输入/滑动等
3. 每个动作先做截图再决策
4. )" << descRuleText << R"(
4.1 )" << intentTextRuleText << R"(
5. 若输入失败，优先"先 Tap 输入框再 Type"，避免直接 Type 失败
6. 如果页面停滞，先做一次 Scroll 或等待再重试
7. 避免连续无效 Tap；若连续两次 Tap 后界面无变化，必须改用 Swipe/Back/Wait/Launch 等其他动作

正常示例：
<answer>
	do(action="Installed", desc="获取已安装应用列表")
</answer>

<answer>
	do(action="Launch", app="任务管理器", desc="启动任务管理器")
</answer>

错误示例：
<answer>
	do(action="Type", text="taskmgr", desc="输入命令")
</answer>

当前屏幕比例：)" << ratio << R"(
范围 0-1000，优先输出下一步动作。)";

        return ss.str();
    }

    // 构建动作修复提示词
    static std::string buildActionRepairPrompt(const std::string& failedAction, bool enforceDesc = false) {
        std::string descRuleText = buildRepairDescRule(enforceDesc);
        std::string intentTextRuleText = buildRepairIntentTextRule(enforceDesc);
        std::string failedTypeText = extractFailedTypeText(failedAction);
        bool forceKeyboardTap = !failedTypeText.empty() && std::any_of(failedTypeText.begin(), failedTypeText.end(), 
            [](char c) { return static_cast<unsigned char>(c) > 127; });
        std::string targetKey = failedTypeText.empty() ? "" : std::string(1, failedTypeText[0]);

        std::string inputRepairRule;
        std::string repairExample;
        std::string optionalOutput;

        if (forceKeyboardTap) {
            inputRepairRule = std::string(R"(- 当前失败动作是中文 Type，本次先执行"Tap 输入框重新聚焦"
- 下一步优先尝试 Type（不要长期只输出 Tap）
- 仅当 Type 再次失败时，才改为 Tap 软键盘按键输入
- 若改为键盘 Tap，优先点击")") + targetKey + R"("键；多字文本按顺序逐字 Tap)";
            repairExample = R"(do(action="Tap", element=[500,500], desc="点击目标输入框重新聚焦"))";
            optionalOutput = std::string(R"(do(action="Type", text=")") + failedTypeText + R"(", desc="重新尝试输入文本"))";
        } else {
            inputRepairRule = "- 若为输入场景，先 Tap 再 Type，或使用可点击输入框后再 Type";
            repairExample = R"(do(action="Tap", element=[500,150], desc="点击顶部搜索框"))";
            optionalOutput = R"(do(action="Type", text="需要输入的内容", desc="输入用户名"))";
        }

        std::stringstream ss;
        ss << R"(# 动作执行失败修复

上一步骤失败动作：)" << failedAction << R"(

请给出可直接执行的新动作，要求如下：
- 如果上一步失败，先补齐关键参数
)" << inputRepairRule << R"(
- 页面切换失败可尝试 Wait 或 Swipe 后重试
- 可选输出 finish 提前结束
- )" << descRuleText << R"(
- )" << intentTextRuleText << R"(

<answer>
	)" << repairExample << R"(
</answer>

可选输出：
<answer>
	)" << optionalOutput << R"(
</answer>
)";

        return ss.str();
    }

    // 构建修复提示词
    static std::string buildRepairPrompt(bool enforceDesc = false) {
        std::string descRuleText = buildRepairDescRule(enforceDesc);

        return std::string(R"(# 输出格式错误修复

如果上一步输出不规范，请重新输出。
示例：
<answer>
	do(action="Tap", element=[500,500], desc="点击中间按钮")
</answer>

<answer>
	finish(message="任务完成")
</answer>

注意：
- do() 与 finish() 均是合法输出
- )") + descRuleText + R"(
- 只返回一次完整动作，不要解释
)";
    }

private:
    // 提取失败动作中的文本
    static std::string extractFailedTypeText(const std::string& failedAction) {
        std::regex typeRegex(R"(action\s*=\s*["']?(type|input|text|type_name)["']?)", std::regex::icase);
        if (!std::regex_search(failedAction, typeRegex)) {
            return "";
        }

        std::regex textRegex(R"!(text\s*=\s*"([^"]*)")!");
        std::smatch match;
        if (std::regex_search(failedAction, match, textRegex) && match.size() > 1) {
            std::string text = match[1].str();
            // trim
            size_t start = text.find_first_not_of(" \t\n\r");
            if (start == std::string::npos) return "";
            size_t end = text.find_last_not_of(" \t\n\r");
            text = text.substr(start, end - start + 1);
            if (!text.empty()) {
                return text;
            }
        }
        return "";
    }

    static std::string buildSystemDescRule(bool enforceDesc) {
        if (enforceDesc) {
            return "每个 do(...) 必须包含 desc 字段";
        } else {
            return "每个 do(...) 可选包含 desc 字段（建议包含，便于修复），不允许影响执行";
        }
    }

    static std::string buildSystemIntentTextRule(bool enforceDesc) {
        if (enforceDesc) {
            return "第三方模型可选 text 字段；若提供，应简洁描述动作意图";
        } else {
            return R"(默认模型请尽量在 do(...) 中额外携带 text="本次动作意图"；Type 动作的 text 保留为实际输入内容)";
        }
    }

    static std::string buildRepairDescRule(bool enforceDesc) {
        if (enforceDesc) {
            return "do() 与 finish() 应包含 desc 字段，帮助模型保持稳定输出";
        } else {
            return "do() 与 finish() 的 desc 字段建议包含（可选），不包含时仍应给出可执行动作";
        }
    }

    static std::string buildRepairIntentTextRule(bool enforceDesc) {
        if (enforceDesc) {
            return "修复时 text 字段可选；若输出请简洁说明动作意图";
        } else {
            return R"(修复时优先补充 text="动作意图"（Type 动作除外），便于日志展示与排错)";
        }
    }
};

} // namespace aries
