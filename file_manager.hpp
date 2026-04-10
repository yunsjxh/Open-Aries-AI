#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>

namespace aries {

// 文件信息结构
struct FileInfo {
    std::string name;
    std::string fullPath;
    std::string extension;
    long long size;
    bool isDirectory;
    std::string createdTime;
    std::string modifiedTime;
    bool isHidden;
    bool isSystem;
};

// 文件管理器 - 专为AI设计
class FileManager {
public:
    // 编码转换函数
    static std::string wideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, NULL, NULL);
        return result;
    }
    
    static std::wstring utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
        return result;
    }
    
    static std::string wideToAnsi(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &result[0], size, NULL, NULL);
        return result;
    }
    
    static std::wstring ansiToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &result[0], size);
        return result;
    }
    
    // 获取当前工作目录
    static std::string getCurrentDirectory() {
        wchar_t buffer[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, buffer);
        return wideToUtf8(buffer);
    }
    
    // 设置当前工作目录
    static bool setCurrentDirectory(const std::string& path) {
        std::wstring wPath = utf8ToWide(path);
        return SetCurrentDirectoryW(wPath.c_str()) != 0;
    }
    
    // 列出目录内容（使用宽字符版本）
    static std::vector<FileInfo> listDirectory(const std::string& path = ".") {
        std::vector<FileInfo> files;
        WIN32_FIND_DATAW findData;
        HANDLE hFind;
        
        // 转换为宽字符路径
        std::wstring wSearchPath = utf8ToWide(path) + L"\\*";
        hFind = FindFirstFileW(wSearchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                // 将宽字符文件名转换为UTF-8
                std::string fileName = wideToUtf8(findData.cFileName);
                
                // 跳过 . 和 ..
                if (fileName == "." || fileName == "..") continue;
                
                FileInfo info;
                info.name = fileName;
                info.fullPath = path + "\\" + fileName;
                info.isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                info.isHidden = (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
                info.isSystem = (findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
                
                // 获取扩展名
                size_t dotPos = fileName.find_last_of('.');
                if (dotPos != std::string::npos && !info.isDirectory) {
                    info.extension = fileName.substr(dotPos + 1);
                }
                
                // 获取文件大小
                if (!info.isDirectory) {
                    ULARGE_INTEGER fileSize;
                    fileSize.LowPart = findData.nFileSizeLow;
                    fileSize.HighPart = findData.nFileSizeHigh;
                    info.size = (long long)fileSize.QuadPart;
                } else {
                    info.size = 0;
                }
                
                // 获取时间信息
                info.createdTime = fileTimeToString(findData.ftCreationTime);
                info.modifiedTime = fileTimeToString(findData.ftLastWriteTime);
                
                files.push_back(info);
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
        
        // 排序：目录在前，文件在后，按名称排序
        std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
            if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
            return a.name < b.name;
        });
        
        return files;
    }
    
    // 读取文本文件
    static std::string readTextFile(const std::string& path, int maxLines = -1) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return "Error: 无法打开文件 " + path;
        }
        
        std::stringstream buffer;
        std::string line;
        int lineCount = 0;
        
        while (std::getline(file, line) && (maxLines < 0 || lineCount < maxLines)) {
            buffer << line << "\n";
            lineCount++;
        }
        
        file.close();
        return buffer.str();
    }
    
    // 读取文件的前N行
    static std::string readFileHead(const std::string& path, int lines = 50) {
        return readTextFile(path, lines);
    }
    
    // 读取文件的后N行
    static std::string readFileTail(const std::string& path, int lines = 50) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return "Error: 无法打开文件 " + path;
        }
        
        // 读取所有行
        std::vector<std::string> allLines;
        std::string line;
        while (std::getline(file, line)) {
            allLines.push_back(line);
        }
        file.close();
        
        // 返回后N行
        std::stringstream buffer;
        int start = std::max(0, (int)allLines.size() - lines);
        for (int i = start; i < (int)allLines.size(); i++) {
            buffer << allLines[i] << "\n";
        }
        
        return buffer.str();
    }
    
    // 读取文件的特定行范围
    static std::string readFileRange(const std::string& path, int startLine, int endLine) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return "Error: 无法打开文件 " + path;
        }
        
        std::stringstream buffer;
        std::string line;
        int currentLine = 0;
        
        while (std::getline(file, line)) {
            currentLine++;
            if (currentLine >= startLine && currentLine <= endLine) {
                buffer << currentLine << ": " << line << "\n";
            }
            if (currentLine > endLine) break;
        }
        
        file.close();
        return buffer.str();
    }
    
    // 写入文本文件
    static bool writeTextFile(const std::string& path, const std::string& content, bool append = false) {
        std::ofstream file(path, std::ios::out | std::ios::binary | (append ? std::ios::app : std::ios::trunc));
        if (!file.is_open()) {
            return false;
        }
        file << content;
        file.close();
        return true;
    }
    
    // 创建目录
    static bool createDirectory(const std::string& path) {
        return CreateDirectoryA(path.c_str(), NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
    }
    
    // 删除文件
    static bool deleteFile(const std::string& path) {
        return DeleteFileA(path.c_str()) != 0;
    }
    
    // 删除目录
    static bool deleteDirectory(const std::string& path) {
        return RemoveDirectoryA(path.c_str()) != 0;
    }
    
    // 移动/重命名文件或目录
    static bool move(const std::string& source, const std::string& destination) {
        return MoveFileA(source.c_str(), destination.c_str()) != 0;
    }
    
    // 复制文件
    static bool copyFile(const std::string& source, const std::string& destination, bool overwrite = false) {
        return CopyFileA(source.c_str(), destination.c_str(), !overwrite) != 0;
    }
    
    // 检查文件是否存在
    static bool fileExists(const std::string& path) {
        DWORD attributes = GetFileAttributesA(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    }
    
    // 检查目录是否存在
    static bool directoryExists(const std::string& path) {
        DWORD attributes = GetFileAttributesA(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
    }
    
    // 获取文件大小（人类可读格式）
    static std::string formatFileSize(long long size) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        double sizeD = (double)size;
        
        while (sizeD >= 1024.0 && unitIndex < 4) {
            sizeD /= 1024.0;
            unitIndex++;
        }
        
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%.2f %s", sizeD, units[unitIndex]);
        return std::string(buffer);
    }
    
    // 搜索文件
    static std::vector<FileInfo> searchFiles(const std::string& directory, const std::string& pattern, bool recursive = false) {
        std::vector<FileInfo> results;
        searchFilesRecursive(directory, pattern, recursive, results);
        return results;
    }
    
    // 获取目录树结构
    static std::string getDirectoryTree(const std::string& path, int maxDepth = 3, int currentDepth = 0) {
        if (currentDepth > maxDepth) return "";
        
        std::stringstream result;
        auto files = listDirectory(path);
        
        for (const auto& file : files) {
            // 添加缩进
            for (int i = 0; i < currentDepth; i++) {
                result << "  ";
            }
            
            if (file.isDirectory) {
                result << "[D] " << file.name << "/\n";
                // 递归获取子目录
                if (currentDepth < maxDepth) {
                    result << getDirectoryTree(file.fullPath, maxDepth, currentDepth + 1);
                }
            } else {
                result << "[F] " << file.name << " (" << formatFileSize(file.size) << ")\n";
            }
        }
        
        return result.str();
    }
    
    // 获取文件信息摘要
    static std::string getFileSummary(const std::string& path) {
        if (!fileExists(path)) {
            return "文件不存在: " + path;
        }
        
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(path.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            return "无法获取文件信息: " + path;
        }
        FindClose(hFind);
        
        std::stringstream result;
        result << "文件: " << path << "\n";
        
        ULARGE_INTEGER fileSize;
        fileSize.LowPart = findData.nFileSizeLow;
        fileSize.HighPart = findData.nFileSizeHigh;
        result << "大小: " << formatFileSize((long long)fileSize.QuadPart) << "\n";
        result << "创建时间: " << fileTimeToString(findData.ftCreationTime) << "\n";
        result << "修改时间: " << fileTimeToString(findData.ftLastWriteTime) << "\n";
        result << "属性: ";
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) result << "只读 ";
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) result << "隐藏 ";
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) result << "系统 ";
        result << "\n";
        
        return result.str();
    }
    
    // 为AI生成目录内容描述
    static std::string generateDirectoryDescription(const std::string& path = ".") {
        auto files = listDirectory(path);
        std::stringstream result;
        
        result << "目录: " << (path == "." ? getCurrentDirectory() : path) << "\n";
        result << "共 " << files.size() << " 个项目\n\n";
        
        int dirCount = 0;
        int fileCount = 0;
        
        for (const auto& file : files) {
            if (file.isDirectory) {
                result << "[目录] " << file.name << "/\n";
                dirCount++;
            } else {
                result << "[文件] " << file.name;
                if (!file.extension.empty()) {
                    result << " (" << file.extension << ")";
                }
                result << " - " << formatFileSize(file.size);
                result << " - 修改: " << file.modifiedTime << "\n";
                fileCount++;
            }
        }
        
        result << "\n总计: " << dirCount << " 个目录, " << fileCount << " 个文件\n";
        
        return result.str();
    }

private:
    // 将FILETIME转换为字符串
    static std::string fileTimeToString(const FILETIME& ft) {
        SYSTEMTIME st;
        FileTimeToSystemTime(&ft, &st);
        
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                 st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond);
        return std::string(buffer);
    }
    
    // 递归搜索文件
    static void searchFilesRecursive(const std::string& directory, const std::string& pattern, 
                                      bool recursive, std::vector<FileInfo>& results) {
        auto files = listDirectory(directory);
        
        for (const auto& file : files) {
            // 检查是否匹配
            if (file.name.find(pattern) != std::string::npos) {
                results.push_back(file);
            }
            
            // 递归搜索子目录
            if (recursive && file.isDirectory) {
                searchFilesRecursive(file.fullPath, pattern, recursive, results);
            }
        }
    }
};

} // namespace aries
