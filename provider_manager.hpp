#pragma once

#include "ai_provider.hpp"
#include "openai_compatible_provider.hpp"
#include "secure_storage.hpp"
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>

namespace aries {

// 模型配置
struct ModelConfig {
    std::string name;
    bool supportsVision;
    bool supportsAudio;
    bool supportsVideo;
};

// 提供商配置
struct ProviderConfig {
    std::string name;
    std::string apiKey;
    std::string baseUrl;
    std::string modelName;
    bool supportsVision;
    bool supportsAudio;
    bool supportsVideo;
    std::vector<ModelConfig> availableModels;  // 可用模型列表
};

// 提供商管理器
class ProviderManager {
public:
    static ProviderManager& getInstance() {
        static ProviderManager instance;
        return instance;
    }
    
    // 注册内置提供商配置
    void registerBuiltInProviders() {
        // 智谱 AI
        registerProviderConfig({
            "zhipu",
            "",
            "https://open.bigmodel.cn/api/paas/v4",
            "glm-4.6v-flash",
            true, false, false,
            {
                {"glm-4.6v-flash", true, false, false},
                {"glm-4v", true, false, false},
                {"glm-4", false, false, false},
                {"glm-4-air", false, false, false},
                {"glm-4-flash", false, false, false},
                {"glm-4v-plus", true, false, false}
            }
        });
    }
    
    // 注册提供商配置
    void registerProviderConfig(const ProviderConfig& config) {
        configs_[config.name] = config;
    }
    
    // 获取所有提供商名称
    std::vector<std::string> getProviderNames() const {
        std::vector<std::string> names;
        for (const auto& [name, config] : configs_) {
            names.push_back(name);
        }
        return names;
    }
    
    // 获取提供商配置
    ProviderConfig* getConfig(const std::string& name) {
        auto it = configs_.find(name);
        if (it != configs_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    // 创建提供商实例
    AIProviderPtr createProvider(const std::string& name) {
        auto* config = getConfig(name);
        if (!config) {
            return nullptr;
        }
        
        // 尝试从安全存储加载 API Key
        std::string apiKey = SecureStorage::loadApiKey(name);
        if (!apiKey.empty()) {
            config->apiKey = apiKey;
        }
        
        if (config->apiKey.empty()) {
            return nullptr;
        }
        
        return std::make_shared<OpenAICompatibleProvider>(
            config->apiKey,
            config->baseUrl,
            config->modelName,
            config->supportsVision,
            config->supportsAudio,
            config->supportsVideo
        );
    }
    
    // 设置当前提供商
    void setCurrentProvider(const std::string& name) {
        currentProviderName_ = name;
        currentProvider_ = createProvider(name);
    }
    
    // 获取当前提供商
    AIProviderPtr getCurrentProvider() const {
        return currentProvider_;
    }
    
    // 获取当前提供商名称
    std::string getCurrentProviderName() const {
        return currentProviderName_;
    }
    
    // 保存提供商 API Key
    bool saveProviderApiKey(const std::string& name, const std::string& apiKey) {
        auto* config = getConfig(name);
        if (config) {
            config->apiKey = apiKey;
        }
        return SecureStorage::saveApiKey(apiKey, name);
    }
    
    // 检查是否有保存的 API Key
    bool hasSavedApiKey(const std::string& name) const {
        return SecureStorage::hasSavedApiKey(name);
    }
    
    // 获取提供商信息字符串
    std::string getProviderInfo(const std::string& name) const {
        auto it = configs_.find(name);
        if (it != configs_.end()) {
            const auto& c = it->second;
            std::string info = c.name + " - " + c.modelName;
            if (c.supportsVision) info += " [Vision]";
            if (hasSavedApiKey(name)) info += " [已配置]";
            return info;
        }
        return name;
    }
    
    // 获取可用模型列表
    std::vector<ModelConfig> getAvailableModels(const std::string& providerName) const {
        auto it = configs_.find(providerName);
        if (it != configs_.end()) {
            return it->second.availableModels;
        }
        return {};
    }
    
    // 设置当前模型
    void setCurrentModel(const std::string& providerName, const std::string& modelName) {
        auto* config = getConfig(providerName);
        if (config) {
            config->modelName = modelName;
            
            // 查找模型配置（如果在预设列表中）
            bool foundInList = false;
            for (const auto& model : config->availableModels) {
                if (model.name == modelName) {
                    config->supportsVision = model.supportsVision;
                    config->supportsAudio = model.supportsAudio;
                    config->supportsVideo = model.supportsVideo;
                    foundInList = true;
                    break;
                }
            }
            
            // 如果不在预设列表中，根据模型名称猜测是否支持视觉
            if (!foundInList) {
                std::string lowerModel = modelName;
                std::transform(lowerModel.begin(), lowerModel.end(), lowerModel.begin(), ::tolower);
                
                // 常见视觉模型关键词
                if (lowerModel.find("vision") != std::string::npos ||
                    lowerModel.find("vl") != std::string::npos ||
                    lowerModel.find("4v") != std::string::npos ||
                    lowerModel.find("gpt-4o") != std::string::npos ||
                    lowerModel.find("gemini") != std::string::npos ||
                    lowerModel.find("claude-3") != std::string::npos) {
                    config->supportsVision = true;
                } else {
                    config->supportsVision = false;
                }
                config->supportsAudio = false;
                config->supportsVideo = false;
            }
        }
    }
    
    // 获取当前模型名称
    std::string getCurrentModel(const std::string& providerName) const {
        auto it = configs_.find(providerName);
        if (it != configs_.end()) {
            return it->second.modelName;
        }
        return "";
    }

private:
    ProviderManager() = default;
    ~ProviderManager() = default;
    ProviderManager(const ProviderManager&) = delete;
    ProviderManager& operator=(const ProviderManager&) = delete;
    
    std::map<std::string, ProviderConfig> configs_;
    std::string currentProviderName_;
    AIProviderPtr currentProvider_;
};

} // namespace aries
