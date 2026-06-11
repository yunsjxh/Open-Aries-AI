#pragma once
// ============================================================================
// mcp_client.hpp — MCP (Model Context Protocol) client for Aries AI
//
// Supports stdio transport: spawns a server process, communicates via
// JSON-RPC 2.0 over stdin/stdout (newline-delimited).
//
// Protocol version: 2024-11-05
// Methods: initialize, tools/list, tools/call
// ============================================================================

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <functional>
#include <cstdio>

#include "tool_system.hpp"

namespace aries {

// =========================================================================
// 1. JSON-RPC Helpers
// =========================================================================

inline std::string rpcEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '\\': o += "\\\\"; break;
            case '"':  o += "\\\""; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:   o += c;
        }
    }
    return o;
}

inline std::string rpcBuildRequest(const std::string& method, const std::string& params, int id) {
    std::ostringstream s;
    s << "{\"jsonrpc\":\"2.0\",\"method\":\"" << rpcEscape(method) << "\"";
    if (!params.empty()) {
        s << ",\"params\":" << params;
    } else {
        s << ",\"params\":{}";
    }
    s << ",\"id\":" << id << "}";
    return s.str();
}

// =========================================================================
// 2. Stdio Transport
// =========================================================================

class StdioTransport {
public:
    ~StdioTransport() { disconnect(); }

    // Spawn a process and connect to its stdin/stdout
    bool connect(const std::wstring& command) {
        command_ = command;

        // Create pipes
        HANDLE hChildStdinRd, hChildStdinWr;
        HANDLE hChildStdoutRd, hChildStdoutWr;
        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };

        if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0)) return false;
        if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0)) {
            CloseHandle(hChildStdinRd); CloseHandle(hChildStdinWr);
            return false;
        }
        // Child must not inherit the parent's write/read ends
        SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);

        // Create process
        PROCESS_INFORMATION pi = {};
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = hChildStdinRd;
        si.hStdOutput = hChildStdoutWr;
        si.hStdError = hChildStdoutWr;

        // Use a mutable copy
        std::wstring cmdCopy = command;
        if (!CreateProcessW(nullptr, cmdCopy.data(), nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(hChildStdinRd); CloseHandle(hChildStdinWr);
            CloseHandle(hChildStdoutRd); CloseHandle(hChildStdoutWr);
            return false;
        }

        CloseHandle(pi.hThread);
        hProcess_ = pi.hProcess;
        hStdinWrite_ = hChildStdinWr;
        hStdoutRead_ = hChildStdoutRd;
        // Close our copy of child's ends
        CloseHandle(hChildStdinRd);
        CloseHandle(hChildStdoutWr);

        return true;
    }

    void disconnect() {
        if (hStdinWrite_)  { CloseHandle(hStdinWrite_); hStdinWrite_ = nullptr; }
        if (hStdoutRead_)  { CloseHandle(hStdoutRead_); hStdoutRead_ = nullptr; }
        if (hProcess_) {
            TerminateProcess(hProcess_, 0);
            WaitForSingleObject(hProcess_, 3000);
            CloseHandle(hProcess_);
            hProcess_ = nullptr;
        }
    }

    bool isConnected() const { return hProcess_ != nullptr; }

    // Send a JSON-RPC message and wait for response (newline-delimited).
    // Returns empty string on timeout or error.
    std::string sendAndReceive(const std::string& request, DWORD timeoutMs = 5000) {
        if (!isConnected()) return "";

        // Write request + newline
        std::string req = request + "\n";
        DWORD written = 0;
        if (!WriteFile(hStdinWrite_, req.data(), (DWORD)req.size(), &written, nullptr))
            return "";

        // Read response with timeout
        std::string response;
        char buf[1];
        DWORD read = 0;
        DWORD startTick = GetTickCount();

        while (true) {
            // Check if process is still alive
            if (hProcess_) {
                DWORD exitCode = STILL_ACTIVE;
                GetExitCodeProcess(hProcess_, &exitCode);
                if (exitCode != STILL_ACTIVE) break; // process died
            }

            // Check if data is available with timeout
            DWORD elapsed = GetTickCount() - startTick;
            DWORD remaining = (elapsed >= timeoutMs) ? 0 : (timeoutMs - elapsed);
            DWORD waitResult = WaitForSingleObject(hStdoutRead_, remaining > 0 ? remaining : 1);
            if (waitResult == WAIT_TIMEOUT || waitResult == WAIT_FAILED) {
                break; // timeout
            }

            if (!ReadFile(hStdoutRead_, buf, 1, &read, nullptr) || read == 0) {
                break; // pipe closed or error
            }
            if (buf[0] == '\n') break;
            response += buf[0];

            // Guard against infinite response
            if (response.size() > 1048576) break; // 1MB max
        }

        return response;
    }

    HANDLE process() const { return hProcess_; }

private:
    std::wstring command_;
    HANDLE hProcess_ = nullptr;
    HANDLE hStdinWrite_ = nullptr;
    HANDLE hStdoutRead_ = nullptr;
};

// =========================================================================
// 3. MCP Client
// =========================================================================

class McpClient {
public:
    McpClient() = default;
    ~McpClient() { disconnect(); }

    // Connect to an MCP server via stdio
    bool connect(const std::wstring& command, const std::string& label = "") {
        label_ = label.empty() ? std::to_string(reinterpret_cast<uintptr_t>(this)) : label;

        if (!transport_.connect(command)) return false;

        // Send initialize
        std::string initReq = rpcBuildRequest("initialize",
            "{\"protocolVersion\":\"2024-11-05\","
            "\"capabilities\":{\"tools\":{}},"
            "\"clientInfo\":{\"name\":\"AriesAI\",\"version\":\"1.0.0\"}}",
            nextId_++);
        std::string initResp = transport_.sendAndReceive(initReq);
        if (initResp.empty() || initResp.find("\"error\"") != std::string::npos) {
            transport_.disconnect();
            return false;
        }

        // Send initialized notification (no id)
        std::string notif = "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\",\"params\":{}}";
        transport_.sendAndReceive(notif);

        return true;
    }

    void disconnect() { transport_.disconnect(); }
    bool isConnected() const { return transport_.isConnected(); }
    const std::string& label() const { return label_; }

    // Discover tools from the server
    std::vector<ToolDef> discoverTools() {
        std::vector<ToolDef> tools;
        if (!isConnected()) return tools;

        std::string req = rpcBuildRequest("tools/list", "{}", nextId_++);
        std::string resp = transport_.sendAndReceive(req);
        if (resp.empty()) return tools;

        // Parse tools array from response
        // Find the "tools" array in the result
        size_t toolsPos = resp.find("\"tools\"");
        if (toolsPos == std::string::npos) return tools;
        size_t arrStart = resp.find('[', toolsPos);
        if (arrStart == std::string::npos) return tools;

        // Extract each tool object
        size_t pos = arrStart + 1;
        while (pos < resp.size()) {
            while (pos < resp.size() && resp[pos] != '{' && resp[pos] != ']') pos++;
            if (pos >= resp.size() || resp[pos] == ']') break;

            size_t objEnd = findMatchingBrace(resp, pos);
            if (objEnd == std::string::npos) break;

            std::string toolJson = resp.substr(pos, objEnd - pos + 1);
            ToolDef def = parseToolDef(toolJson);
            if (!def.id.empty()) {
                def.id = "mcp:" + label_ + ":" + def.id; // namespace: mcp:<server>:<tool>
                tools.push_back(std::move(def));
            }

            pos = objEnd + 1;
        }

        return tools;
    }

    // Call a tool and get result
    ToolResult callTool(const std::string& fullId, const std::string& argsJson) {
        if (!isConnected())
            return ToolResult::error("MCP断开", "MCP 服务器未连接");

        // Strip mcp:label: prefix to get the server-side tool name
        std::string toolName = fullId;
        size_t secondColon = toolName.find(':', 4); // skip "mcp:"
        if (secondColon != std::string::npos)
            toolName = toolName.substr(secondColon + 1);

        std::ostringstream params;
        params << "{\"name\":\"" << rpcEscape(toolName) << "\"";
        if (!argsJson.empty()) {
            params << ",\"arguments\":" << argsJson;
        }
        params << "}";

        std::string req = rpcBuildRequest("tools/call", params.str(), nextId_++);
        std::string resp = transport_.sendAndReceive(req);
        if (resp.empty())
            return ToolResult::error("MCP超时", "MCP 服务器无响应");

        // Check for JSON-RPC error
        if (resp.find("\"error\"") != std::string::npos) {
            size_t msgPos = resp.find("\"message\":\"");
            std::string errMsg = "MCP 错误";
            if (msgPos != std::string::npos) {
                errMsg = extractJsonString(resp, msgPos + 10);
            }
            return ToolResult::error("MCP错误", errMsg);
        }

        // Parse result content
        return parseToolResult(resp, toolName);
    }

    // Utility: find matching closing brace (handles nested strings/braces)
    static size_t findMatchingBrace(const std::string& s, size_t start) {
        if (start >= s.size() || s[start] != '{') return std::string::npos;
        int depth = 0;
        for (size_t i = start; i < s.size(); i++) {
            if (s[i] == '"') {
                i++;
                while (i < s.size()) {
                    if (s[i] == '\\') i++;
                    else if (s[i] == '"') break;
                    i++;
                }
                continue;
            }
            if (s[i] == '{') depth++;
            else if (s[i] == '}') {
                depth--;
                if (depth == 0) return i;
            }
        }
        return std::string::npos;
    }

private:
    StdioTransport transport_;
    std::string label_;
    int nextId_ = 1;

    static std::string extractJsonString(const std::string& json, size_t start) {
        size_t q = json.find('"', start);
        if (q == std::string::npos) return "";
        q++;
        std::string val;
        while (q < json.size()) {
            if (json[q] == '\\' && q + 1 < json.size()) {
                switch (json[q + 1]) {
                    case '"': val += '"'; break;
                    case '\\': val += '\\'; break;
                    case 'n': val += '\n'; break;
                    case 'r': val += '\r'; break;
                    case 't': val += '\t'; break;
                    default: val += json[q + 1]; break;
                }
                q += 2;
            } else if (json[q] == '"') {
                break;
            } else {
                val += json[q];
                q++;
            }
        }
        return val;
    }

    // Parse a single tool definition JSON object from tools/list response
    static ToolDef parseToolDef(const std::string& json) {
        ToolDef def;
        def.id = extractJsonString(json, json.find("\"name\":\""));
        // Skip if id parsing failed
        if (def.id.empty()) {
            // Try numeric index based approach
            size_t namePos = json.find("\"name\"");
            if (namePos != std::string::npos) {
                size_t valStart = json.find('"', namePos + 6);
                if (valStart != std::string::npos) {
                    def.id = extractJsonString(json, valStart);
                }
            }
        }

        def.description = extractJsonString(json, json.find("\"description\":\""));

        // Parse inputSchema if present
        size_t schemaPos = json.find("\"inputSchema\"");
        if (schemaPos != std::string::npos) {
            // Extract properties
            size_t propsPos = json.find("\"properties\"", schemaPos);
            if (propsPos != std::string::npos) {
                size_t objStart = json.find('{', propsPos);
                if (objStart != std::string::npos) {
                    size_t objEnd = findMatchingBrace(json, objStart);
                    if (objEnd != std::string::npos) {
                        std::string propsJson = json.substr(objStart, objEnd - objStart + 1);
                        parseProperties(propsJson, def);
                    }
                }
            }

            // Extract required array
            size_t reqPos = json.find("\"required\"", schemaPos);
            if (reqPos != std::string::npos) {
                size_t arrStart = json.find('[', reqPos);
                if (arrStart != std::string::npos && arrStart < reqPos + 30) {
                    size_t arrEnd = json.find(']', arrStart);
                    if (arrEnd != std::string::npos) {
                        std::string reqArr = json.substr(arrStart + 1, arrEnd - arrStart - 1);
                        // Parse required names
                        size_t rp = 0;
                        while (rp < reqArr.size()) {
                            size_t qs = reqArr.find('"', rp);
                            if (qs == std::string::npos) break;
                            size_t qe = reqArr.find('"', qs + 1);
                            if (qe == std::string::npos) break;
                            std::string reqName = reqArr.substr(qs + 1, qe - qs - 1);
                            // Mark matching param as required
                            for (auto& p : def.parameters) {
                                if (p.name == reqName) { p.required = true; break; }
                            }
                            rp = qe + 1;
                        }
                    }
                }
            }
        }

        return def;
    }

    static void parseProperties(const std::string& propsJson, ToolDef& def) {
        size_t pos = 0;
        while (pos < propsJson.size()) {
            // Find a quoted key
            size_t keyStart = propsJson.find('"', pos);
            if (keyStart == std::string::npos) break;
            size_t keyEnd = propsJson.find('"', keyStart + 1);
            if (keyEnd == std::string::npos) break;
            std::string key = propsJson.substr(keyStart + 1, keyEnd - keyStart - 1);
            if (key == "type" || key == "required") { pos = keyEnd + 1; continue; }

            // Find the value object for this key
            size_t valObjStart = propsJson.find('{', keyEnd);
            if (valObjStart == std::string::npos || valObjStart > keyEnd + 50) {
                pos = keyEnd + 1;
                continue;
            }
            size_t valObjEnd = findMatchingBrace(propsJson, valObjStart);
            if (valObjEnd == std::string::npos) break;
            std::string valObj = propsJson.substr(valObjStart, valObjEnd - valObjStart + 1);

            ParamDef param;
            param.name = key;

            // Type
            std::string type = extractJsonString(valObj, valObj.find("\"type\":\""));
            if (type == "number" || type == "integer") param.type = ParamType::Number;
            else if (type == "boolean") param.type = ParamType::Boolean;
            else param.type = ParamType::String;

            // Description
            param.description = extractJsonString(valObj, valObj.find("\"description\":\""));

            def.parameters.push_back(param);
            pos = valObjEnd + 1;
        }
    }

    static ToolResult parseToolResult(const std::string& resp, const std::string& toolName) {
        // Find the "content" array in the result
        size_t contentPos = resp.find("\"content\"");
        if (contentPos == std::string::npos)
            return ToolResult::ok(toolName, resp);

        size_t arrStart = resp.find('[', contentPos);
        if (arrStart == std::string::npos)
            return ToolResult::ok(toolName, resp);

        // Accumulate text content items
        std::string output;
        size_t pos = arrStart + 1;
        while (pos < resp.size()) {
            while (pos < resp.size() && resp[pos] != '{' && resp[pos] != ']') pos++;
            if (pos >= resp.size() || resp[pos] == ']') break;
            size_t itemEnd = findMatchingBrace(resp, pos);
            if (itemEnd == std::string::npos) break;
            std::string item = resp.substr(pos, itemEnd - pos + 1);

            std::string itemType = extractJsonString(item, item.find("\"type\":\""));
            if (itemType == "text") {
                std::string text = extractJsonString(item, item.find("\"text\":\""));
                if (!output.empty()) output += "\n";
                output += text;
            } else if (itemType == "image") {
                if (!output.empty()) output += "\n";
                output += "[图片数据]";
            }

            pos = itemEnd + 1;
        }

        if (output.empty()) output = resp;
        return ToolResult::ok(toolName, output);
    }
};

// =========================================================================
// 4. MCP Registry — manages multiple MCP clients and their tools
// =========================================================================

class McpRegistry {
public:
    // Add a server connection. Server tools are discovered and registered.
    // Returns number of tools discovered, or -1 on connection failure.
    int addServer(const std::wstring& command, const std::string& label,
                  ToolRegistry& toolRegistry) {
        auto client = std::make_shared<McpClient>();
        if (!client->connect(command, label)) {
            return -1;
        }

        auto tools = client->discoverTools();
        for (auto& tool : tools) {
            std::string toolId = tool.id;
            auto exec = [client, toolId](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
                return client->callTool(toolId, argsJson);
            };
            toolRegistry.registerTool(std::move(tool), std::move(exec));
        }

        clients_.push_back(client);
        return (int)tools.size();
    }

    // Load servers from a JSON config file.
    // Format: {"servers":[{"label":"name","command":"path/to/server.exe"}]}
    // Relative paths are resolved against the exe directory.
    // Returns total number of tools discovered.
    int loadFromJsonFile(const std::wstring& configPath, ToolRegistry& toolRegistry) {
        // Read file
        HANDLE h = CreateFileW(configPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return 0;

        DWORD size = GetFileSize(h, nullptr);
        if (size == 0 || size > 65536) { CloseHandle(h); return 0; }
        std::string json(size + 1, 0);
        DWORD read = 0;
        ReadFile(h, &json[0], size, &read, nullptr);
        CloseHandle(h);
        json.resize(read);

        // Resolve exe directory for relative paths
        wchar_t exeDir[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
        wchar_t* sd = wcsrchr(exeDir, L'\\');
        if (sd) *(sd + 1) = 0;

        int totalTools = 0;

        // Parse "servers" array
        size_t arrPos = json.find("\"servers\"");
        if (arrPos == std::string::npos) return 0;
        size_t arrStart = json.find('[', arrPos);
        if (arrStart == std::string::npos) return 0;

        size_t pos = arrStart + 1;
        while (pos < json.size()) {
            while (pos < json.size() && json[pos] != '{' && json[pos] != ']') pos++;
            if (pos >= json.size() || json[pos] == ']') break;

            size_t objEnd = McpClient::findMatchingBrace(json, pos);
            if (objEnd == std::string::npos) break;
            std::string entry = json.substr(pos, objEnd - pos + 1);

            std::string label = extractStr(entry, "label");
            std::string cmd = extractStr(entry, "command");
            std::string args = extractStr(entry, "args");

            if (!label.empty() && !cmd.empty()) {
                // Resolve relative path
                std::wstring wcmd;
                if (cmd.find(':') != std::string::npos) {
                    // Absolute path
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, nullptr, 0);
                    wcmd.resize(wlen - 1);
                    MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, &wcmd[0], wlen);
                } else {
                    // Relative to exe dir
                    wcmd = exeDir;
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, nullptr, 0);
                    std::wstring wrel(wlen - 1, 0);
                    MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, &wrel[0], wlen);
                    wcmd += wrel;
                }

                // Append args if present
                if (!args.empty()) {
                    wcmd += L" ";
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, args.c_str(), -1, nullptr, 0);
                    std::wstring wargs(wlen - 1, 0);
                    MultiByteToWideChar(CP_UTF8, 0, args.c_str(), -1, &wargs[0], wlen);
                    wcmd += wargs;
                }

                int n = addServer(wcmd, label, toolRegistry);
                if (n >= 0) totalTools += n;
            }

            pos = objEnd + 1;
        }

        return totalTools;
    }

    void disconnectAll() {
        for (auto& c : clients_) c->disconnect();
        clients_.clear();
    }

    size_t serverCount() const { return clients_.size(); }

private:
    std::vector<std::shared_ptr<McpClient>> clients_;

    static std::string extractStr(const std::string& json, const std::string& key) {
        size_t pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";
        pos++;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        if (pos >= json.size() || json[pos] != '"') return "";
        pos++;
        std::string val;
        while (pos < json.size()) {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                switch (json[pos + 1]) {
                    case '"': val += '"'; break;
                    case '\\': val += '\\'; break;
                    case 'n': val += '\n'; break;
                    case 'r': val += '\r'; break;
                    case 't': val += '\t'; break;
                    default: val += json[pos + 1]; break;
                }
                pos += 2;
            } else if (json[pos] == '"') {
                break;
            } else {
                val += json[pos];
                pos++;
            }
        }
        return val;
    }
};

} // namespace aries
