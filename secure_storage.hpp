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
    // 保存加密的 API Key (支持多提供商)
    static bool saveApiKey(const std::string& apiKey, const std::string& provider = "default") {
        std::string hardwareId = getHardwareFingerprint();
        if (hardwareId.empty()) {
            return false;
        }
        
        std::string salt = generateSalt();
        std::string key = hardwareId + salt;
        std::string hashedKey = hashString(key);
        std::string encrypted = xorEncrypt(apiKey, hashedKey);
        std::string hexEncrypted = stringToHex(encrypted);
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
    
    // 读取并解密 API Key (支持多提供商)
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
        
        std::string hardwareId = getHardwareFingerprint();
        if (hardwareId.empty()) {
            return "";
        }
        
        std::string key = hardwareId + salt;
        std::string hashedKey = hashString(key);
        std::string hexEncrypted = deobfuscateRandomChars(obfuscated);
        std::string encrypted = hexToString(hexEncrypted);
        if (encrypted.empty()) {
            return "";
        }
        
        std::string decrypted = xorEncrypt(encrypted, hashedKey);
        
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

private:
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
};

} // namespace aries
