#pragma once

#include "ai_provider.hpp"
#include "openai_compatible_provider.hpp"
#include "secure_storage.hpp"

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cctype>
#include <mutex>

namespace aries {

// =======================
// Model Config
// =======================
struct ModelConfig {
    std::string name;
    bool supportsVision = false;
    bool supportsAudio = false;
    bool supportsVideo = false;
};

// =======================
// Provider Config
// =======================
struct ProviderConfig {
    std::string name;
    std::string apiKey;
    std::string baseUrl;
    std::string modelName;

    bool supportsVision = false;
    bool supportsAudio = false;
    bool supportsVideo = false;

    std::vector<ModelConfig> availableModels;
};

// =======================
// Provider Factory（纯构造）
// =======================
class ProviderFactory {
public:
    AIProviderPtr create(const ProviderConfig& cfg, const std::string& apiKey) {
        if (apiKey.empty()) return nullptr;

        return std::make_shared<OpenAICompatibleProvider>(
            apiKey,
            cfg.baseUrl,
            cfg.modelName,
            cfg.supportsVision,
            cfg.supportsAudio,
            cfg.supportsVideo
        );
    }
};


class ProviderRegistry {
public:
    void registerProvider(const ProviderConfig& cfg) {
        std::lock_guard<std::mutex> lock(mutex_);
        configs_[cfg.name] = cfg;
    }

    const ProviderConfig* get(const std::string& name) const {
        auto it = configs_.find(name);
        if (it == configs_.end()) return nullptr;
        return &it->second;
    }

    ProviderConfig* getMutable(const std::string& name) {
        auto it = configs_.find(name);
        if (it == configs_.end()) return nullptr;
        return &it->second;
    }

    std::vector<std::string> list() const {
        std::vector<std::string> out;
        for (auto& [k, _] : configs_) out.push_back(k);
        return out;
    }

private:
    std::map<std::string, ProviderConfig> configs_;
    mutable std::mutex mutex_;
};

// =======================
// Provider Context（运行态）
// =======================
class ProviderContext {
public:
    void set(const std::string& name, AIProviderPtr provider) {
        std::lock_guard<std::mutex> lock(mutex_);
        currentName_ = name;
        current_ = std::move(provider);
    }

    AIProviderPtr get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_;
    }

    std::string name() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentName_;
    }

private:
    mutable std::mutex mutex_;
    std::string currentName_;
    AIProviderPtr current_;
};

// =======================
// ProviderManager（协调层）
// =======================
class ProviderManager {
public:
    static ProviderManager& instance() {
        static ProviderManager inst;
        return inst;
    }

    // -----------------------
    // 注册内置 Provider
    // -----------------------
    void registerBuiltInProviders() {
        registry_.registerProvider({
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

    AIProviderPtr createProvider(const std::string& name) {
        const ProviderConfig* cfg = registry_.get(name);
        if (!cfg) return nullptr;

        std::string apiKey = SecureStorage::loadApiKey(name);
        if (apiKey.empty()) {
            // fallback to config memory
            apiKey = cfg->apiKey;
        }

        return factory_.create(*cfg, apiKey);
    }

    // -----------------------
    // 设置当前 Provider（保证一致性）
    // -----------------------
    bool setCurrentProvider(const std::string& name) {
        auto provider = createProvider(name);
        if (!provider) return false;

        context_.set(name, provider);
        return true;
    }

    AIProviderPtr getCurrentProvider() const {
        return context_.get();
    }

    std::string getCurrentProviderName() const {
        return context_.name();
    }

    // -----------------------
    // API Key 管理
    // -----------------------
    bool saveProviderApiKey(const std::string& name, const std::string& apiKey) {
        auto* cfg = registry_.getMutable(name);
        if (cfg) cfg->apiKey = apiKey;

        return SecureStorage::saveApiKey(apiKey, name);
    }

    bool hasSavedApiKey(const std::string& name) const {
        return SecureStorage::hasSavedApiKey(name);
    }

    std::vector<ModelConfig> getAvailableModels(const std::string& name) const {
        const auto* cfg = registry_.get(name);
        if (!cfg) return {};
        return cfg->availableModels;
    }

    void setCurrentModel(const std::string& provider, const std::string& model) {
        auto* cfg = registry_.getMutable(provider);
        if (!cfg) return;

        cfg->modelName = model;
        for (const auto& m : cfg->availableModels) {
            if (m.name == model) {
                cfg->supportsVision = m.supportsVision;
                cfg->supportsAudio = m.supportsAudio;
                cfg->supportsVideo = m.supportsVideo;
                return;
            }
        }
        std::string lower = model;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        cfg->supportsVision =
            lower.find("vision") != std::string::npos ||
            lower.find("vl") != std::string::npos ||
            lower.find("4v") != std::string::npos ||
            lower.find("gpt-4o") != std::string::npos;

        cfg->supportsAudio = false;
        cfg->supportsVideo = false;
    }

    std::string getCurrentModel(const std::string& name) const {
        const auto* cfg = registry_.get(name);
        return cfg ? cfg->modelName : "";
    }
    std::string getProviderInfo(const std::string& name) const {
        const auto* cfg = registry_.get(name);
        if (!cfg) return name;

        std::string info = cfg->name + " - " + cfg->modelName;
        if (cfg->supportsVision) info += " [Vision]";
        if (hasSavedApiKey(name)) info += " [Configured]";
        return info;
    }

    std::vector<std::string> listProviders() const {
        return registry_.list();
    }

private:
    ProviderManager() = default;

    ProviderRegistry registry_;
    ProviderFactory factory_;
    ProviderContext context_;
};

} // namespace aries
