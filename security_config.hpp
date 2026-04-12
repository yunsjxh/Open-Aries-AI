#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>

namespace aries {

struct SecurityConfig {
    bool allowExecute = false;
    bool allowFileWrite = false;
    bool allowFileDelete = false;
    bool allowFileRun = false;
    bool requireHighRiskConfirmation = true;
    bool loadedFromFile = false;
};

class SecurityConfigLoader {
public:
    static SecurityConfig loadFromFileAndEnv(const std::string& filePath = "aries_config.json") {
        SecurityConfig config;

        std::ifstream in(filePath);
        if (in.is_open()) {
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            config.allowExecute = readBool(content, "allowExecute", config.allowExecute);
            config.allowFileWrite = readBool(content, "allowFileWrite", config.allowFileWrite);
            config.allowFileDelete = readBool(content, "allowFileDelete", config.allowFileDelete);
            config.allowFileRun = readBool(content, "allowFileRun", config.allowFileRun);
            config.requireHighRiskConfirmation = readBool(content, "requireHighRiskConfirmation", config.requireHighRiskConfirmation);
            config.loadedFromFile = true;
        }

        applyEnv(config.allowExecute, "ARIES_ALLOW_EXECUTE");
        applyEnv(config.allowFileWrite, "ARIES_ALLOW_FILE_WRITE");
        applyEnv(config.allowFileDelete, "ARIES_ALLOW_FILE_DELETE");
        applyEnv(config.allowFileRun, "ARIES_ALLOW_FILE_RUN");
        applyEnv(config.requireHighRiskConfirmation, "ARIES_REQUIRE_HIGH_RISK_CONFIRMATION");

        return config;
    }

private:
    static bool parseBool(const std::string& value, bool fallback) {
        std::string v = value;
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
        if (v == "0" || v == "false" || v == "no" || v == "off") return false;
        return fallback;
    }

    static bool readBool(const std::string& content, const std::string& key, bool fallback) {
        std::string token = "\"" + key + "\"";
        std::size_t keyPos = content.find(token);
        if (keyPos == std::string::npos) return fallback;

        std::size_t colonPos = content.find(':', keyPos + token.size());
        if (colonPos == std::string::npos) return fallback;

        std::size_t valueStart = content.find_first_not_of(" \t\r\n", colonPos + 1);
        if (valueStart == std::string::npos) return fallback;

        if (content.compare(valueStart, 4, "true") == 0) return true;
        if (content.compare(valueStart, 5, "false") == 0) return false;
        return fallback;
    }

    static void applyEnv(bool& target, const char* envName) {
        const char* raw = std::getenv(envName);
        if (!raw) return;
        target = parseBool(raw, target);
    }
};

} // namespace aries

