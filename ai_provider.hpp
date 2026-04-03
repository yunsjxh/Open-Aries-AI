#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace aries {

// 聊天消息
struct ChatMessage {
    std::string role;
    std::string content;
    
    ChatMessage(const std::string& r, const std::string& c) 
        : role(r), content(c) {}
};

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
    
    // 获取最后错误
    virtual std::string getLastError() const = 0;
    
    // 验证 API Key 是否有效
    virtual bool validateApiKey() = 0;
};

using AIProviderPtr = std::shared_ptr<AIProvider>;

} // namespace aries
