#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <random>
#include <algorithm>
#include <intrin.h>
#include <map>

namespace aries {

class SecureStorage {
public:
    // 保存加密的 API Key (支持多提供商) - 使用 DPAPI + 硬件绑定双重加密
    static bool saveApiKey(const std::string& apiKey, const std::string& provider = "default") {
        // 第一层加密：硬件绑定加密
        std::string hardwareId = getHardwareFingerprint();
        if (hardwareId.empty()) {
            return false;
        }
        
        std::string salt = generateSalt();
        std::string key = hardwareId + salt;
        std::string hashedKey = hashString(key);
        std::string hardwareEncrypted = xorEncrypt(apiKey, hashedKey);
        
        // 第二层加密：DPAPI 加密
        std::string dpapiEncrypted = dpapiEncrypt(hardwareEncrypted);
        if (dpapiEncrypted.empty()) {
            return false;
        }
        
        // 混淆和存储
        std::string hexEncrypted = stringToHex(dpapiEncrypted);
        std::string obfuscated = obfuscateWithRandomChars(hexEncrypted);
        
        std::ofstream file(getStoragePath(provider), std::ios::out);
        if (!file.is_open()) {
            return false;
        }
        
        file << salt << std::endl;
        file << obfuscated << std::endl;
        file.close();
        
        return true;
    }
    
    // 读取并解密 API Key (支持多提供商) - 使用 DPAPI + 硬件绑定双重解密
    static std::string loadApiKey(const std::string& provider = "default") {
        std::ifstream file(getStoragePath(provider), std::ios::in);
        if (!file.is_open()) {
            return "";
        }
        
        std::string salt;
        std::string obfuscated;
        
        std::getline(file, salt);
        std::getline(file, obfuscated);
        file.close();
        
        if (salt.empty() || obfuscated.empty()) {
            return "";
        }
        
        // 反混淆
        std::string hexEncrypted = deobfuscateRandomChars(obfuscated);
        std::string dpapiEncrypted = hexToString(hexEncrypted);
        if (dpapiEncrypted.empty()) {
            return "";
        }
        
        // 第一层解密：DPAPI 解密
        std::string hardwareEncrypted = dpapiDecrypt(dpapiEncrypted);
        if (hardwareEncrypted.empty()) {
            return "";
        }
        
        // 第二层解密：硬件绑定解密
        std::string hardwareId = getHardwareFingerprint();
        if (hardwareId.empty()) {
            return "";
        }
        
        std::string key = hardwareId + salt;
        std::string hashedKey = hashString(key);
        std::string decrypted = xorEncrypt(hardwareEncrypted, hashedKey);
        
        return decrypted;
    }
    
    // 检查是否已保存 API Key (支持多提供商)
    static bool hasSavedApiKey(const std::string& provider = "default") {
        std::ifstream file(getStoragePath(provider));
        return file.good();
    }
    
    // 删除保存的 API Key (支持多提供商)
    static bool deleteApiKey(const std::string& provider = "default") {
        return DeleteFileA(getStoragePath(provider).c_str()) != 0;
    }
    
    // 删除所有保存的 API Key
    static bool deleteAllApiKeys() {
        return deleteApiKey("siliconflow");
    }
    
    // 保存自定义提供商配置（支持多个）- 使用 DPAPI + 硬件绑定双重加密
    // 格式: baseUrl|modelName|apiKey
    static bool saveCustomProvider(const std::string& name, const std::string& baseUrl, 
                                    const std::string& modelName, const std::string& apiKey) {
        // 第一层加密：硬件绑定加密
        std::string hardwareId = getHardwareFingerprint();
        if (hardwareId.empty()) {
            return false;
        }
        
        std::string salt = generateSalt();
        std::string key = hardwareId + salt;
        std::string hashedKey = hashString(key);
        
        // 格式: baseUrl|modelName|apiKey
        std::string data = baseUrl + "|" + modelName + "|" + apiKey;
        std::string hardwareEncrypted = xorEncrypt(data, hashedKey);
        
        // 第二层加密：DPAPI 加密
        std::string dpapiEncrypted = dpapiEncrypt(hardwareEncrypted);
        if (dpapiEncrypted.empty()) {
            return false;
        }
        
        // 混淆和存储
        std::string hexEncrypted = stringToHex(dpapiEncrypted);
        std::string obfuscated = obfuscateWithRandomChars(hexEncrypted);
        
        std::ofstream file(getCustomProviderPath(name), std::ios::out);
        if (!file.is_open()) {
            return false;
        }
        
        file << salt << std::endl;
        file << obfuscated << std::endl;
        file.close();
        
        // 同时更新自定义提供商列表
        addCustomProviderToList(name);
        
        return true;
    }
    
    // 加载自定义提供商配置 - 使用 DPAPI + 硬件绑定双重解密
    // 返回: {baseUrl, modelName, apiKey}
    static std::tuple<std::string, std::string, std::string> loadCustomProvider(const std::string& name) {
        std::ifstream file(getCustomProviderPath(name), std::ios::in);
        if (!file.is_open()) {
            return {"", "", ""};
        }
        
        std::string salt;
        std::string obfuscated;
        
        std::getline(file, salt);
        std::getline(file, obfuscated);
        file.close();
        
        if (salt.empty() || obfuscated.empty()) {
            return {"", "", ""};
        }
        
        // 反混淆
        std::string hexEncrypted = deobfuscateRandomChars(obfuscated);
        std::string dpapiEncrypted = hexToString(hexEncrypted);
        if (dpapiEncrypted.empty()) {
            return {"", "", ""};
        }
        
        // 第一层解密：DPAPI 解密
        std::string hardwareEncrypted = dpapiDecrypt(dpapiEncrypted);
        if (hardwareEncrypted.empty()) {
            return {"", "", ""};
        }
        
        // 第二层解密：硬件绑定解密
        std::string hardwareId = getHardwareFingerprint();
        if (hardwareId.empty()) {
            return {"", "", ""};
        }
        
        std::string key = hardwareId + salt;
        std::string hashedKey = hashString(key);
        std::string decrypted = xorEncrypt(hardwareEncrypted, hashedKey);
        
        // 解析 baseUrl|modelName|apiKey
        std::vector<std::string> parts;
        size_t start = 0;
        size_t end = decrypted.find('|');
        while (end != std::string::npos) {
            parts.push_back(decrypted.substr(start, end - start));
            start = end + 1;
            end = decrypted.find('|', start);
        }
        parts.push_back(decrypted.substr(start));
        
        if (parts.size() >= 3) {
            return {parts[0], parts[1], parts[2]};
        }
        
        return {"", "", ""};
    }
    
    // 检查是否已保存指定名称的自定义提供商
    static bool hasCustomProvider(const std::string& name) {
        std::ifstream file(getCustomProviderPath(name));
        return file.good();
    }
    
    // 删除指定名称的自定义提供商
    static bool deleteCustomProvider(const std::string& name) {
        removeCustomProviderFromList(name);
        return DeleteFileA(getCustomProviderPath(name).c_str()) != 0;
    }
    
    // 获取所有自定义提供商名称列表
    static std::vector<std::string> getCustomProviderList() {
        std::vector<std::string> providers;
        std::ifstream file(getCustomProviderListPath(), std::ios::in);
        if (file.is_open()) {
            std::string name;
            while (std::getline(file, name)) {
                if (!name.empty()) {
                    providers.push_back(name);
                }
            }
            file.close();
        }
        return providers;
    }
    
    // 获取自定义提供商数量
    static int getCustomProviderCount() {
        return (int)getCustomProviderList().size();
    }

private:
    // 添加自定义提供商到列表
    static void addCustomProviderToList(const std::string& name) {
        auto providers = getCustomProviderList();
        // 检查是否已存在
        if (std::find(providers.begin(), providers.end(), name) == providers.end()) {
            providers.push_back(name);
            // 保存列表
            std::ofstream file(getCustomProviderListPath(), std::ios::out);
            if (file.is_open()) {
                for (const auto& p : providers) {
                    file << p << std::endl;
                }
                file.close();
            }
        }
    }
    
    // 从列表中移除自定义提供商
    static void removeCustomProviderFromList(const std::string& name) {
        auto providers = getCustomProviderList();
        auto it = std::find(providers.begin(), providers.end(), name);
        if (it != providers.end()) {
            providers.erase(it);
            // 保存列表
            std::ofstream file(getCustomProviderListPath(), std::ios::out);
            if (file.is_open()) {
                for (const auto& p : providers) {
                    file << p << std::endl;
                }
                file.close();
            }
        }
    }
    
    // 获取自定义提供商存储路径
    static std::string getCustomProviderPath(const std::string& name) {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        std::string exePath(path);
        size_t lastSlash = exePath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return exePath.substr(0, lastSlash) + "\\.custom_provider_" + name + "_secure";
        }
        return ".custom_provider_" + name + "_secure";
    }
    
    // 获取自定义提供商列表文件路径
    static std::string getCustomProviderListPath() {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        std::string exePath(path);
        size_t lastSlash = exePath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return exePath.substr(0, lastSlash) + "\\.custom_provider_list";
        }
        return ".custom_provider_list";
    }
    // 获取存储路径 (支持多提供商)
    static std::string getStoragePath(const std::string& provider) {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        std::string exePath(path);
        size_t lastSlash = exePath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return exePath.substr(0, lastSlash) + "\\.api_key_" + provider + "_secure";
        }
        return ".api_key_" + provider + "_secure";
    }
    
    // ==================== DPAPI 加密/解密 ====================
    
    // 使用 DPAPI 加密数据（用户级别，绑定当前 Windows 用户账户）
    static std::string dpapiEncrypt(const std::string& plaintext) {
        if (plaintext.empty()) {
            return "";
        }
        
        DATA_BLOB dataIn;
        DATA_BLOB dataOut;
        
        dataIn.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()));
        dataIn.cbData = static_cast<DWORD>(plaintext.length());
        
        // 使用 CryptProtectData 进行加密
        // CRYPTPROTECT_UI_FORBIDDEN: 禁止显示 UI
        // CRYPTPROTECT_LOCAL_MACHINE: 绑定到本地机器（可选，这里使用用户级别）
        if (!CryptProtectData(
                &dataIn,
                L"Open-Aries-AI API Key",  // 描述字符串
                nullptr,                    // 可选的熵值
                nullptr,                    // 保留
                nullptr,                    // 提示结构
                CRYPTPROTECT_UI_FORBIDDEN,
                &dataOut)) {
            return "";
        }
        
        // 将加密后的数据转换为字符串
        std::string result(reinterpret_cast<char*>(dataOut.pbData), dataOut.cbData);
        
        // 释放 DPAPI 分配的内存
        LocalFree(dataOut.pbData);
        
        return result;
    }
    
    // 使用 DPAPI 解密数据
    static std::string dpapiDecrypt(const std::string& ciphertext) {
        if (ciphertext.empty()) {
            return "";
        }
        
        DATA_BLOB dataIn;
        DATA_BLOB dataOut;
        
        dataIn.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(ciphertext.data()));
        dataIn.cbData = static_cast<DWORD>(ciphertext.length());
        
        // 使用 CryptUnprotectData 进行解密
        if (!CryptUnprotectData(
                &dataIn,
                nullptr,                    // 描述字符串（可选输出）
                nullptr,                    // 可选的熵值
                nullptr,                    // 保留
                nullptr,                    // 提示结构
                CRYPTPROTECT_UI_FORBIDDEN,
                &dataOut)) {
            return "";
        }
        
        // 将解密后的数据转换为字符串
        std::string result(reinterpret_cast<char*>(dataOut.pbData), dataOut.cbData);
        
        // 释放 DPAPI 分配的内存
        LocalFree(dataOut.pbData);
        
        return result;
    }
    
    // ==================== 硬件指纹 ====================
    
    // 获取硬件指纹（CPU + 硬盘 + 主板序列号）
    static std::string getHardwareFingerprint() {
        std::string fingerprint;
        
        std::string cpuId = getCpuId();
        fingerprint += cpuId;
        
        std::string diskId = getDiskSerialNumber();
        fingerprint += diskId;
        
        std::string boardId = getBoardSerialNumber();
        fingerprint += boardId;
        
        return fingerprint;
    }
    
    // 获取 CPU ID
    static std::string getCpuId() {
        int cpuInfo[4] = {0};
        __cpuid(cpuInfo, 0);
        
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (int i = 0; i < 4; i++) {
            oss << std::setw(8) << cpuInfo[i];
        }
        return oss.str();
    }
    
    // 获取硬盘序列号
    static std::string getDiskSerialNumber() {
        HANDLE hDevice = CreateFileA(
            "\\\\.\\PhysicalDrive0",
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        
        if (hDevice == INVALID_HANDLE_VALUE) {
            char volumeName[MAX_PATH];
            char fileSystemName[MAX_PATH];
            DWORD serialNumber;
            DWORD maxComponentLen;
            DWORD fileSystemFlags;
            
            if (GetVolumeInformationA(
                "C:\\",
                volumeName,
                MAX_PATH,
                &serialNumber,
                &maxComponentLen,
                &fileSystemFlags,
                fileSystemName,
                MAX_PATH
            )) {
                std::ostringstream oss;
                oss << std::hex << serialNumber;
                return oss.str();
            }
            return "DISK";
        }
        
        STORAGE_PROPERTY_QUERY query;
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        
        STORAGE_DESCRIPTOR_HEADER header;
        DWORD bytesReturned;
        
        if (!DeviceIoControl(
            hDevice,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query,
            sizeof(query),
            &header,
            sizeof(header),
            &bytesReturned,
            NULL
        )) {
            CloseHandle(hDevice);
            return "DISK";
        }
        
        std::vector<BYTE> buffer(header.Size);
        if (!DeviceIoControl(
            hDevice,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query,
            sizeof(query),
            buffer.data(),
            buffer.size(),
            &bytesReturned,
            NULL
        )) {
            CloseHandle(hDevice);
            return "DISK";
        }
        
        CloseHandle(hDevice);
        
        STORAGE_DEVICE_DESCRIPTOR* descriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
        if (descriptor->SerialNumberOffset > 0) {
            return reinterpret_cast<char*>(buffer.data() + descriptor->SerialNumberOffset);
        }
        
        return "DISK";
    }
    
    // 获取主板序列号
    static std::string getBoardSerialNumber() {
        std::string serialNumber;
        
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
            "HARDWARE\\DESCRIPTION\\System\\BIOS", 
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            
            char value[256];
            DWORD valueSize = sizeof(value);
            DWORD type;
            
            if (RegQueryValueExA(hKey, "BaseBoardSerialNumber", NULL, &type, 
                                 (LPBYTE)value, &valueSize) == ERROR_SUCCESS) {
                serialNumber = value;
            } else if (RegQueryValueExA(hKey, "SystemSerialNumber", NULL, &type, 
                                        (LPBYTE)value, &valueSize) == ERROR_SUCCESS) {
                serialNumber = value;
            }
            
            RegCloseKey(hKey);
        }
        
        if (serialNumber.empty()) {
            serialNumber = "BOARD";
        }
        
        return serialNumber;
    }
    
    // 生成固定盐值
    static std::string generateSalt() {
        std::string hardwareId = getHardwareFingerprint();
        if (hardwareId.length() >= 16) {
            return hardwareId.substr(0, 16);
        }
        return hardwareId + std::string(16 - hardwareId.length(), '0');
    }
    
    // 简单的哈希函数（FNV-1a）
    static std::string hashString(const std::string& input) {
        const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
        const uint64_t FNV_PRIME = 1099511628211ULL;
        
        uint64_t hash = FNV_OFFSET_BASIS;
        for (char c : input) {
            hash ^= static_cast<uint64_t>(c);
            hash *= FNV_PRIME;
        }
        
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(16) << hash;
        return oss.str();
    }
    
    // XOR 加密/解密
    static std::string xorEncrypt(const std::string& input, const std::string& key) {
        std::string output;
        output.reserve(input.length());
        
        for (size_t i = 0; i < input.length(); i++) {
            output += input[i] ^ key[i % key.length()];
        }
        
        return output;
    }
    
    // 字符串转十六进制
    static std::string stringToHex(const std::string& input) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (unsigned char c : input) {
            oss << std::setw(2) << static_cast<int>(c);
        }
        return oss.str();
    }
    
    // 十六进制转字符串
    static std::string hexToString(const std::string& hex) {
        std::string output;
        output.reserve(hex.length() / 2);
        
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byteString = hex.substr(i, 2);
            char byte = static_cast<char>(std::stoi(byteString, nullptr, 16));
            output += byte;
        }
        
        return output;
    }
    
    // 在第 2 和第 6 个位置添加随机字符
    static std::string obfuscateWithRandomChars(const std::string& input) {
        const char chars[] = "0123456789ABCDEF";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        
        std::string output = input;
        
        if (output.length() >= 1) {
            output.insert(1, 1, chars[dis(gen)]);
        }
        
        if (output.length() >= 5) {
            output.insert(5, 1, chars[dis(gen)]);
        }
        
        return output;
    }
    
    // 去除第 2 和第 6 个位置的随机字符
    static std::string deobfuscateRandomChars(const std::string& input) {
        std::string output = input;
        
        if (output.length() > 5) {
            output.erase(5, 1);
        }
        
        if (output.length() > 1) {
            output.erase(1, 1);
        }
        
        return output;
    }
    
    // ==================== JSON 格式保存/加载提供商配置 ====================
    
public:
    // 获取 JSON 配置文件路径
    static std::string getJsonConfigPath() {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        std::string exePath(path);
        size_t lastSlash = exePath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return exePath.substr(0, lastSlash) + "\\providers.json";
        }
        return "providers.json";
    }
    
    // JSON 字符串转义
    static std::string jsonEscape(const std::string& str) {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c;
            }
        }
        return result;
    }
    
    // JSON 字符串反转义
    static std::string jsonUnescape(const std::string& str) {
        std::string result;
        for (size_t i = 0; i < str.length(); i++) {
            if (str[i] == '\\' && i + 1 < str.length()) {
                char next = str[i + 1];
                switch (next) {
                    case '"': result += '"'; i++; break;
                    case '\\': result += '\\'; i++; break;
                    case 'n': result += '\n'; i++; break;
                    case 'r': result += '\r'; i++; break;
                    case 't': result += '\t'; i++; break;
                    default: result += str[i]; break;
                }
            } else {
                result += str[i];
            }
        }
        return result;
    }
    
    // 从 JSON 字符串中提取字段值
    static std::string extractJsonField(const std::string& json, const std::string& key) {
        std::string searchKey = "\"" + key + "\"";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return "";
        
        pos = json.find(":", pos);
        if (pos == std::string::npos) return "";
        
        while (pos < json.length() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
            pos++;
        }
        
        if (pos >= json.length()) return "";
        
        if (json[pos] == '"') {
            pos++;
            size_t endPos = pos;
            while (endPos < json.length()) {
                if (json[endPos] == '\\' && endPos + 1 < json.length()) {
                    endPos += 2;
                } else if (json[endPos] == '"') {
                    break;
                } else {
                    endPos++;
                }
            }
            return jsonUnescape(json.substr(pos, endPos - pos));
        }
        
        return "";
    }
    
    // 保存所有提供商配置到 JSON 文件（加密）
    static bool saveProvidersToJson(const std::map<std::string, std::tuple<std::string, std::string, std::string>>& providers) {
        // 构建 JSON 字符串
        std::ostringstream json;
        json << "{\n";
        json << "  \"version\": 1,\n";
        json << "  \"providers\": {\n";
        
        bool first = true;
        for (const auto& [name, data] : providers) {
            if (!first) json << ",\n";
            first = false;
            
            const auto& [baseUrl, modelName, apiKey] = data;
            
            // 加密 API Key
            std::string encryptedApiKey = encryptValue(apiKey);
            
            json << "    \"" << jsonEscape(name) << "\": {\n";
            json << "      \"baseUrl\": \"" << jsonEscape(baseUrl) << "\",\n";
            json << "      \"modelName\": \"" << jsonEscape(modelName) << "\",\n";
            json << "      \"apiKey\": \"" << jsonEscape(encryptedApiKey) << "\"\n";
            json << "    }";
        }
        
        json << "\n  }\n";
        json << "}\n";
        
        // 写入文件
        std::ofstream file(getJsonConfigPath(), std::ios::out);
        if (!file.is_open()) {
            return false;
        }
        
        file << json.str();
        file.close();
        
        return true;
    }
    
    // 从 JSON 文件加载所有提供商配置（解密）
    static std::map<std::string, std::tuple<std::string, std::string, std::string>> loadProvidersFromJson() {
        std::map<std::string, std::tuple<std::string, std::string, std::string>> providers;
        
        std::ifstream file(getJsonConfigPath(), std::ios::in);
        if (!file.is_open()) {
            return providers;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        // 解析 providers 对象
        size_t providersPos = content.find("\"providers\"");
        if (providersPos == std::string::npos) return providers;
        
        size_t providersStart = content.find("{", providersPos);
        if (providersStart == std::string::npos) return providers;
        
        // 找到每个提供商
        size_t pos = providersStart + 1;
        while (pos < content.length()) {
            // 找到提供商名称
            size_t nameStart = content.find("\"", pos);
            if (nameStart == std::string::npos) break;
            
            size_t nameEnd = content.find("\"", nameStart + 1);
            if (nameEnd == std::string::npos) break;
            
            std::string name = jsonUnescape(content.substr(nameStart + 1, nameEnd - nameStart - 1));
            
            // 找到提供商对象
            size_t objStart = content.find("{", nameEnd);
            if (objStart == std::string::npos) break;
            
            size_t objEnd = content.find("}", objStart);
            if (objEnd == std::string::npos) break;
            
            std::string objContent = content.substr(objStart, objEnd - objStart + 1);
            
            // 提取字段
            std::string baseUrl = extractJsonField(objContent, "baseUrl");
            std::string modelName = extractJsonField(objContent, "modelName");
            std::string encryptedApiKey = extractJsonField(objContent, "apiKey");
            
            // 解密 API Key
            std::string apiKey = decryptValue(encryptedApiKey);
            
            providers[name] = {baseUrl, modelName, apiKey};
            
            pos = objEnd + 1;
            
            // 跳过逗号
            while (pos < content.length() && (content[pos] == ',' || content[pos] == ' ' || content[pos] == '\n' || content[pos] == '\r' || content[pos] == '\t')) {
                pos++;
            }
        }
        
        return providers;
    }
    
    // 加密单个值
    static std::string encryptValue(const std::string& value) {
        if (value.empty()) return "";
        
        std::string hardwareId = getHardwareFingerprint();
        if (hardwareId.empty()) return "";
        
        std::string salt = generateSalt();
        std::string key = hardwareId + salt;
        std::string hashedKey = hashString(key);
        
        std::string encrypted = xorEncrypt(value, hashedKey);
        std::string dpapiEncrypted = dpapiEncrypt(encrypted);
        
        if (dpapiEncrypted.empty()) return "";
        
        return salt + ":" + stringToHex(dpapiEncrypted);
    }
    
    // 解密单个值
    static std::string decryptValue(const std::string& encrypted) {
        if (encrypted.empty()) {
            std::cerr << "[调试] decryptValue: 输入为空" << std::endl;
            return "";
        }
        
        size_t colonPos = encrypted.find(":");
        if (colonPos == std::string::npos) {
            std::cerr << "[调试] decryptValue: 找不到冒号分隔符" << std::endl;
            return "";
        }
        
        std::string salt = encrypted.substr(0, colonPos);
        std::string hexEncrypted = encrypted.substr(colonPos + 1);
        
        std::cerr << "[调试] decryptValue: salt长度=" << salt.length() << ", hexEncrypted长度=" << hexEncrypted.length() << std::endl;
        
        std::string dpapiEncrypted = hexToString(hexEncrypted);
        if (dpapiEncrypted.empty()) {
            std::cerr << "[调试] decryptValue: hexToString失败" << std::endl;
            return "";
        }
        
        std::string decrypted = dpapiDecrypt(dpapiEncrypted);
        if (decrypted.empty()) {
            std::cerr << "[调试] decryptValue: dpapiDecrypt失败" << std::endl;
            return "";
        }
        
        std::string hardwareId = getHardwareFingerprint();
        if (hardwareId.empty()) {
            std::cerr << "[调试] decryptValue: getHardwareFingerprint失败" << std::endl;
            return "";
        }
        
        std::string key = hardwareId + salt;
        std::string hashedKey = hashString(key);
        
        std::string result = xorEncrypt(decrypted, hashedKey);
        std::cerr << "[调试] decryptValue: 解密成功, 结果长度=" << result.length() << std::endl;
        return result;
    }
    
private:
};

} // namespace aries
