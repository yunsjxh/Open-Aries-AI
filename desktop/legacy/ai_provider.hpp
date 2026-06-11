#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>

namespace aries {

// 聊天消息
struct ChatMessage {
    std::string role;
    std::string content;

    ChatMessage(const std::string& r, const std::string& c)
        : role(r), content(c) {}
};

// Token 用量统计
struct TokenUsage {
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;

    bool valid() const { return total_tokens > 0; }
};

// 流式响应回调：delta 为增量文本，done 表示流结束
using StreamDeltaCallback = std::function<void(const std::string& delta, bool done)>;

// AI Provider 接口
class AIProvider {
public:
    virtual ~AIProvider() = default;

    virtual std::string getProviderName() const = 0;
    virtual std::string getModelName() const = 0;
    virtual bool supportsVision() const = 0;
    virtual bool supportsAudio() const = 0;
    virtual bool supportsVideo() const = 0;

    // 发送消息（非流式）
    virtual std::pair<bool, std::string> sendMessage(
        const std::vector<ChatMessage>& messages,
        const std::string& systemPrompt = "") = 0;

    // 发送消息（带图片）
    virtual std::pair<bool, std::string> sendMessageWithImages(
        const std::string& text,
        const std::vector<std::string>& imagePaths,
        const std::string& systemPrompt = "") = 0;

    // 流式发送消息，delta 通过回调实时推送，返回完整文本
    virtual std::pair<bool, std::string> sendMessageStream(
        const std::vector<ChatMessage>& messages,
        StreamDeltaCallback onDelta,
        const std::string& systemPrompt = "") {
        // 默认实现：回退到非流式，一次性回调
        auto result = sendMessage(messages, systemPrompt);
        if (result.first && onDelta) {
            onDelta(result.second, true);
        }
        return result;
    }

    // 流式发送消息（带图片）
    virtual std::pair<bool, std::string> sendMessageWithImagesStream(
        const std::string& text,
        const std::vector<std::string>& imagePaths,
        StreamDeltaCallback onDelta,
        const std::string& systemPrompt = "") {
        // 默认实现：回退到非流式
        auto result = sendMessageWithImages(text, imagePaths, systemPrompt);
        if (result.first && onDelta) {
            onDelta(result.second, true);
        }
        return result;
    }

    // 获取最后错误
    virtual std::string getLastError() const = 0;

    // 获取最后一次调用的 Token 用量
    virtual TokenUsage getLastTokenUsage() const = 0;

    // 验证 API Key 是否有效
    virtual bool validateApiKey() = 0;
};

using AIProviderPtr = std::shared_ptr<AIProvider>;

} // namespace aries
