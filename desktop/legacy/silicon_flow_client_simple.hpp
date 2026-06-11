#pragma once

#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <array>
#include <memory>
#include <stdexcept>
#include <windows.h>

namespace silicon_flow {

// Base64 编码函数
inline std::string base64_encode(const std::string& data) {
    static const char base64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    size_t i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    for (size_t in_len = data.size(); i < in_len;) {
        size_t to_read = std::min(size_t(3), in_len - i);
        for (size_t j = 0; j < to_read; j++) {
            char_array_3[j] = data[i++];
        }
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((to_read > 1 ? char_array_3[1] : 0) >> 4);
        char_array_4[2] = to_read > 1 ? ((char_array_3[1] & 0x0f) << 2) + ((to_read > 2 ? char_array_3[2] : 0) >> 6) : 0;
        char_array_4[3] = to_read > 2 ? (char_array_3[2] & 0x3f) : 0;
        
        for (size_t j = 0; j < (to_read + 1); j++) {
            encoded += base64_chars[char_array_4[j]];
        }
        
        while (to_read++ < 3) {
            encoded += '=';
        }
    }
    
    return encoded;
}

// 从文件读取并转为 base64
inline std::string image_to_base64(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return "";
    }
    
    std::string data((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    file.close();
    
    return base64_encode(data);
}

// 获取文件扩展名对应的 MIME 类型
inline std::string get_mime_type(const std::string& filepath) {
    size_t dot_pos = filepath.rfind('.');
    if (dot_pos == std::string::npos) {
        return "image/jpeg";
    }
    
    std::string ext = filepath.substr(dot_pos + 1);
    if (ext == "png" || ext == "PNG") return "image/png";
    if (ext == "jpg" || ext == "jpeg" || ext == "JPG" || ext == "JPEG") return "image/jpeg";
    if (ext == "gif" || ext == "GIF") return "image/gif";
    if (ext == "webp" || ext == "WEBP") return "image/webp";
    
    return "image/jpeg";
}

// 消息内容项（支持文本和图片）
struct ContentItem {
    std::string type;  // "text" 或 "image_url"
    std::string text;  // type="text" 时使用
    std::string image_url;  // type="image_url" 时使用 (base64 或 URL)
    std::string mime_type;  // 图片的 MIME 类型
};

// 消息结构体
struct Message {
    std::string role;
    std::string content;  // 简单文本内容
    std::vector<ContentItem> content_items;  // 多模态内容
    bool is_multimodal = false;
    
    Message() = default;
    Message(const std::string& r, const std::string& c) : role(r), content(c), is_multimodal(false) {}
};

// 使用统计
struct Usage {
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;
};

// 选择结果
struct Choice {
    int index = 0;
    Message message;
    std::string finish_reason;
};

// 响应结构
struct ChatResponse {
    std::string id;
    std::string object;
    int created = 0;
    std::string model;
    std::vector<Choice> choices;
    Usage usage;
    std::string trace_id;
};

// 客户端配置
struct ClientConfig {
    std::string api_key;
    std::string base_url = "https://api.siliconflow.cn/v1";
    long timeout_ms = 60000;
};

// 错误信息
struct ApiError {
    int http_code = 0;
    std::string message;
    std::string type;
};

// 简单的 JSON 解析器
class SimpleJson {
public:
    static std::string extract_string(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        
        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";
        
        pos = json.find('"', pos);
        if (pos == std::string::npos) return "";
        
        size_t end = json.find('"', pos + 1);
        if (end == std::string::npos) return "";
        
        return json.substr(pos + 1, end - pos - 1);
    }
    
    static int extract_int(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return 0;
        
        pos = json.find(':', pos);
        if (pos == std::string::npos) return 0;
        
        while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
            pos++;
        }
        
        size_t end = pos;
        while (end < json.size() && (json[end] >= '0' && json[end] <= '9')) {
            end++;
        }
        
        if (end == pos) return 0;
        return std::stoi(json.substr(pos, end - pos));
    }
    
    static std::string extract_content(const std::string& json) {
        std::string search = "\"content\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        
        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";
        
        while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
            pos++;
        }
        
        if (pos >= json.size() || json[pos] != '"') return "";
        
        pos++;
        std::string result;
        bool escaped = false;
        
        while (pos < json.size()) {
            char c = json[pos];
            if (escaped) {
                switch (c) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += c; break;
                }
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                break;
            } else {
                result += c;
            }
            pos++;
        }
        
        return result;
    }
};

// 执行命令并获取输出
inline std::string exec(const char* cmd) {
    std::array<char, 4096> buffer;
    std::string result;
    
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) {
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    return result;
}

// HTTP 客户端 - 使用 curl 命令
class HttpClient {
public:
    static std::pair<bool, std::string> post(const std::string& url, 
                                              const std::string& body,
                                              const std::vector<std::string>& headers,
                                              std::string& out_trace_id) {
        // 创建临时目录
        char temp_path[MAX_PATH];
        GetTempPathA(MAX_PATH, temp_path);
        
        // 创建临时文件存储请求体
        char body_file[MAX_PATH];
        GetTempFileNameA(temp_path, "body", 0, body_file);
        
        // 创建临时文件存储响应
        char temp_file[MAX_PATH];
        GetTempFileNameA(temp_path, "resp", 0, temp_file);
        
        // 将请求体写入文件
        std::ofstream body_out(body_file, std::ios::binary);
        body_out.write(body.c_str(), body.size());
        body_out.close();
        
        // 构建 curl 命令 - 使用文件传递数据
        std::ostringstream cmd;
        cmd << "curl -s -X POST ";
        cmd << "\"" << url << "\" ";
        cmd << "-H \"Content-Type: application/json\" ";
        
        for (const auto& h : headers) {
            cmd << "-H \"" << h << "\" ";
        }
        
        cmd << "-d @\"" << body_file << "\" ";
        cmd << "-D - -o \"" << temp_file << "\"";
        cmd << " 2>nul";
        
        // 执行命令获取响应头
        std::string header_output = exec(cmd.str().c_str());
        
        // 读取响应体
        std::ifstream response_file(temp_file);
        std::string response((std::istreambuf_iterator<char>(response_file)),
                              std::istreambuf_iterator<char>());
        response_file.close();
        
        // 删除临时文件
        DeleteFileA(body_file);
        DeleteFileA(temp_file);
        
        // 解析 HTTP 状态码
        int http_code = 0;
        size_t code_pos = header_output.find("HTTP/");
        if (code_pos != std::string::npos) {
            size_t space_pos = header_output.find(' ', code_pos);
            if (space_pos != std::string::npos) {
                http_code = std::stoi(header_output.substr(space_pos + 1, 3));
            }
        }
        
        // 提取 trace_id
        size_t trace_pos = header_output.find("x-siliconcloud-trace-id:");
        if (trace_pos != std::string::npos) {
            size_t start = header_output.find_first_not_of(" \t", trace_pos + 24);
            size_t end = header_output.find("\r\n", start);
            if (start != std::string::npos && end != std::string::npos) {
                out_trace_id = header_output.substr(start, end - start);
            }
        }
        
        if (http_code != 200) {
            return {false, "HTTP " + std::to_string(http_code) + ": " + response};
        }
        
        return {true, response};
    }
};

// Silicon Flow API 客户端类
class SiliconFlowClient {
public:
    explicit SiliconFlowClient(const ClientConfig& config) : config_(config) {}

    std::pair<bool, ChatResponse> chat(const std::vector<Message>& messages, 
                                        const std::string& model = "deepseek-ai/DeepSeek-V3") {
        std::string json_body = build_request_json(messages, model, false);
        
        std::vector<std::string> headers;
        headers.push_back("Authorization: Bearer " + config_.api_key);
        
        std::string trace_id;
        std::string url = config_.base_url + "/chat/completions";
        
        auto result = HttpClient::post(url, json_body, headers, trace_id);
        
        if (!result.first) {
            last_error_ = ApiError{0, result.second, "http_error"};
            return {false, ChatResponse()};
        }
        
        return {true, parse_response(result.second, trace_id)};
    }

    std::pair<bool, std::string> simple_chat(const std::string& user_message,
                                              const std::string& system_message = "",
                                              const std::string& model = "deepseek-ai/DeepSeek-V3") {
        std::vector<Message> messages;
        if (!system_message.empty()) {
            messages.push_back(Message("system", system_message));
        }
        messages.push_back(Message("user", user_message));

        auto result = chat(messages, model);
        
        if (!result.first || result.second.choices.empty()) {
            return {false, ""};
        }

        return {true, result.second.choices[0].message.content};
    }

    // 多模态聊天（支持图片）
    std::pair<bool, ChatResponse> chat_with_images(const std::string& text,
                                                    const std::vector<std::string>& image_paths,
                                                    const std::string& system_message = "",
                                                    const std::string& model = "zai-org/GLM-4.5V") {
        std::vector<Message> messages;
        
        if (!system_message.empty()) {
            messages.push_back(Message("system", system_message));
        }
        
        // 构建多模态消息
        Message user_msg;
        user_msg.role = "user";
        user_msg.is_multimodal = true;
        
        // 添加文本
        ContentItem text_item;
        text_item.type = "text";
        text_item.text = text;
        user_msg.content_items.push_back(text_item);
        
        // 添加图片
        for (const auto& path : image_paths) {
            std::string base64_data = image_to_base64(path);
            if (base64_data.empty()) {
                last_error_ = ApiError{0, "Failed to read image: " + path, "file_error"};
                return {false, ChatResponse()};
            }
            
            ContentItem image_item;
            image_item.type = "image_url";
            image_item.mime_type = get_mime_type(path);
            image_item.image_url = "data:" + image_item.mime_type + ";base64," + base64_data;
            user_msg.content_items.push_back(image_item);
        }
        
        messages.push_back(user_msg);
        
        return chat(messages, model);
    }

    const std::optional<ApiError>& last_error() const { return last_error_; }

private:
    std::string build_request_json(const std::vector<Message>& messages, 
                                   const std::string& model,
                                   bool stream) {
        std::ostringstream oss;
        oss << "{";
        oss << "\"model\":\"" << escape_json(model) << "\",";
        oss << "\"stream\":" << (stream ? "true" : "false") << ",";
        oss << "\"messages\":" << "[";
        
        for (size_t i = 0; i < messages.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "{";
            oss << "\"role\":\"" << escape_json(messages[i].role) << "\",";
            
            if (messages[i].is_multimodal) {
                // 多模态内容
                oss << "\"content\":" << "[";
                for (size_t j = 0; j < messages[i].content_items.size(); ++j) {
                    if (j > 0) oss << ",";
                    const auto& item = messages[i].content_items[j];
                    oss << "{";
                    oss << "\"type\":\"" << item.type << "\"";
                    if (item.type == "text") {
                        oss << ",\"text\":\"" << escape_json(item.text) << "\"";
                    } else if (item.type == "image_url") {
                        oss << ",\"image_url\":{\"url\":\"" << item.image_url << "\"}";
                    }
                    oss << "}";
                }
                oss << "]";
            } else {
                // 纯文本内容
                oss << "\"content\":\"" << escape_json(messages[i].content) << "\"";
            }
            
            oss << "}";
        }
        
        oss << "]";
        oss << "}";
        return oss.str();
    }
    
    std::string escape_json(const std::string& str) {
        std::ostringstream oss;
        for (size_t i = 0; i < str.size(); ++i) {
            unsigned char c = str[i];
            switch (c) {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                default:
                    if (c < 0x20) {
                        char buf[7];
                        snprintf(buf, sizeof(buf), "\\u%04x", c);
                        oss << buf;
                    } else {
                        oss << c;
                    }
                    break;
            }
        }
        return oss.str();
    }
    
    ChatResponse parse_response(const std::string& json, const std::string& trace_id) {
        ChatResponse response;
        response.id = SimpleJson::extract_string(json, "id");
        response.object = SimpleJson::extract_string(json, "object");
        response.created = SimpleJson::extract_int(json, "created");
        response.model = SimpleJson::extract_string(json, "model");
        response.trace_id = trace_id;
        
        response.usage.prompt_tokens = SimpleJson::extract_int(json, "prompt_tokens");
        response.usage.completion_tokens = SimpleJson::extract_int(json, "completion_tokens");
        response.usage.total_tokens = SimpleJson::extract_int(json, "total_tokens");
        
        size_t choices_pos = json.find("\"choices\"");
        if (choices_pos != std::string::npos) {
            size_t arr_start = json.find('[', choices_pos);
            size_t arr_end = json.find(']', arr_start);
            if (arr_start != std::string::npos && arr_end != std::string::npos) {
                std::string choices_arr = json.substr(arr_start, arr_end - arr_start + 1);
                
                size_t msg_pos = 0;
                while ((msg_pos = choices_arr.find("\"message\"", msg_pos)) != std::string::npos) {
                    Choice choice;
                    choice.index = SimpleJson::extract_int(choices_arr.substr(msg_pos, 200), "index");
                    
                    size_t content_start = choices_arr.find("\"content\"", msg_pos);
                    if (content_start != std::string::npos) {
                        choice.message.content = SimpleJson::extract_content(
                            choices_arr.substr(content_start, 5000));
                    }
                    
                    choice.message.role = SimpleJson::extract_string(
                        choices_arr.substr(msg_pos, 500), "role");
                    choice.finish_reason = SimpleJson::extract_string(
                        choices_arr.substr(msg_pos, 500), "finish_reason");
                    
                    response.choices.push_back(choice);
                    msg_pos++;
                }
            }
        }
        
        return response;
    }

    ClientConfig config_;
    std::optional<ApiError> last_error_;
};

inline Message system_msg(const std::string& content) {
    return Message("system", content);
}

inline Message user_msg(const std::string& content) {
    return Message("user", content);
}

inline Message assistant_msg(const std::string& content) {
    return Message("assistant", content);
}

} // namespace silicon_flow
