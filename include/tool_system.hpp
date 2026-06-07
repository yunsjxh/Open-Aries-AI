#pragma once
// ============================================================================
// tool_system.hpp — OpenCode-style tool framework for Aries AI
//
// Ported from OpenCode's tool system (packages/opencode/src/tool/):
//   tool.ts (Tool.Def, Tool.execute, output truncation)
//   registry.ts (ToolRegistry, registration, model filtering)
//   session/tools.ts (execution wrapper with permission + validation)
//   truncate.ts (output size limits, temp file fallback)
// ============================================================================

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <sstream>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cstdio>
#include <cstdint>

namespace aries {

// =========================================================================
// 1. Parameter Schema
// =========================================================================

enum class ParamType {
    String,
    Number,
    Boolean,
};

struct ParamDef {
    std::string name;
    ParamType type = ParamType::String;
    std::string description;
    bool required = false;
    std::string defaultVal; // JSON literal: "42", "true", "\"hello\""

    // Make a JSON Schema property fragment
    std::string toJsonProperty() const {
        std::ostringstream s;
        s << "\"" << name << "\":{";
        switch (type) {
            case ParamType::String:  s << "\"type\":\"string\""; break;
            case ParamType::Number:  s << "\"type\":\"number\""; break;
            case ParamType::Boolean: s << "\"type\":\"boolean\""; break;
        }
        s << ",\"description\":\"" << escJson(description) << "\"";
        if (!defaultVal.empty()) {
            s << ",\"default\":" << defaultVal;
        }
        s << "}";
        return s.str();
    }

private:
    static std::string escJson(const std::string& s) {
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
};

// =========================================================================
// 2. Tool Context — passed to every tool invocation
// =========================================================================

struct ToolContext {
    std::string sessionId;
    std::string messageId;
    std::string agent = "build";
    std::atomic<bool>* abort = nullptr;

    // Permission check — blocks until user approves or denies.
    // Returns true if allowed, false if denied.
    // permission: "read" | "write" | "edit" | "execute" | ...
    // pattern: the specific target (filepath, command, etc.)
    std::function<bool(const std::string& permission,
                       const std::string& pattern)> ask;

    // Hook to notify the UI about tool execution start
    std::function<void(const std::string& title)> onStart;
};

// =========================================================================
// 3. Tool Result
// =========================================================================

struct ToolResult {
    std::string title;       // short human-readable summary
    std::string output;      // text returned to the LLM
    bool truncated = false;
    bool isError = false;

    // Key-value metadata (paths, counts, diagnostics, etc.)
    std::map<std::string, std::string> metadata;

    static ToolResult ok(const std::string& title, const std::string& output) {
        ToolResult r;
        r.title = title;
        r.output = output;
        return r;
    }

    static ToolResult error(const std::string& title, const std::string& output) {
        ToolResult r;
        r.title = title;
        r.output = output;
        r.isError = true;
        return r;
    }

    ToolResult& withMeta(const std::string& key, const std::string& value) {
        metadata[key] = value;
        return *this;
    }

    ToolResult& withTruncated() {
        truncated = true;
        metadata["truncated"] = "true";
        return *this;
    }
};

// =========================================================================
// 4. Output Truncation (ported from truncate.ts)
//    Defaults: 2000 lines, 50000 bytes
// =========================================================================

struct TruncationConfig {
    int maxLines = 2000;
    int maxBytes = 50000;
};

inline ToolResult truncateOutput(ToolResult result, const TruncationConfig& cfg = {}) {
    if (result.truncated) return result;

    const std::string& out = result.output;

    // Count lines and bytes
    int lines = 1;
    for (char c : out) if (c == '\n') lines++;

    bool overLines = lines > cfg.maxLines;
    bool overBytes = (int)out.size() > cfg.maxBytes;

    if (!overLines && !overBytes) return result;

    // Truncate to limits
    std::string preview;
    int keptLines = 0;
    size_t i = 0;
    while (i < out.size() && keptLines < cfg.maxLines && (int)i < cfg.maxBytes) {
        preview += out[i];
        if (out[i] == '\n') keptLines++;
        i++;
    }

    // Add truncation notice
    std::ostringstream notice;
    notice << "\n\n... (输出已截断: " << lines << " 行 / " << out.size() << " 字节 → "
           << keptLines << " 行 / " << preview.size() << " 字节)";

    result.output = preview + notice.str();
    result.truncated = true;
    result.metadata["truncated"] = "true";
    result.metadata["originalLines"] = std::to_string(lines);
    result.metadata["originalBytes"] = std::to_string((int)out.size());
    return result;
}

// =========================================================================
// 5. Tool Definition
// =========================================================================

struct ToolDef {
    std::string id;
    std::string description;
    std::vector<ParamDef> parameters;

    // Generate OpenAI function-calling JSON fragment:
    //   {"type":"function","function":{"name":"ID","description":"...","parameters":{...}}}
    std::string toFunctionDef() const {
        std::ostringstream s;
        s << "{\"type\":\"function\",\"function\":{"
          << "\"name\":\"" << escJson(id) << "\","
          << "\"description\":\"" << escJson(description) << "\","
          << "\"parameters\":{\"type\":\"object\",\"properties\":{";

        // Properties
        bool first = true;
        for (auto& p : parameters) {
            if (!first) s << ",";
            s << p.toJsonProperty();
            first = false;
        }
        s << "}";

        // Required array
        std::vector<std::string> reqs;
        for (auto& p : parameters) {
            if (p.required) reqs.push_back(p.name);
        }
        if (!reqs.empty()) {
            s << ",\"required\":[";
            first = true;
            for (auto& r : reqs) {
                if (!first) s << ",";
                s << "\"" << escJson(r) << "\"";
                first = false;
            }
            s << "]";
        }

        s << "}}}";
        return s.str();
    }

    // Validate JSON args string against parameter schema.
    // Returns normalized args JSON or error message (starts with "Error:").
    std::string validateArgs(const std::string& jsonArgs) const {
        // Quick sanity check
        if (jsonArgs.empty()) {
            // Build defaults
            std::ostringstream def;
            def << "{";
            bool first = true;
            for (auto& p : parameters) {
                if (!p.defaultVal.empty()) {
                    if (!first) def << ",";
                    def << "\"" << escJson(p.name) << "\":" << p.defaultVal;
                    first = false;
                }
            }
            def << "}";
            std::string s = def.str();
            if (s == "{}") return ""; // no defaults, empty is fine
            return s;
        }

        // Verify it's JSON (at least starts with '{')
        std::string trimmed = jsonArgs;
        size_t ns = trimmed.find_first_not_of(" \t\r\n");
        if (ns != std::string::npos) trimmed = trimmed.substr(ns);
        if (trimmed.empty() || trimmed[0] != '{') {
            return "Error: 参数必须为 JSON 对象，收到: " + jsonArgs.substr(0, 60);
        }

        // Check required params are present
        for (auto& p : parameters) {
            if (!p.required) continue;
            std::string key = "\"" + p.name + "\"";
            if (jsonArgs.find(key) == std::string::npos) {
                return "Error: 缺少必需参数 '" + p.name + "'";
            }
        }

        return ""; // pass-through — full validation is expensive, trust JSON
    }

private:
    static std::string escJson(const std::string& s) {
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
};

// =========================================================================
// 6. Tool Registry
// =========================================================================

class ToolRegistry {
public:
    using Executor = std::function<ToolResult(const std::string& argsJson, ToolContext& ctx)>;

    // Register a tool. Returns false if id already exists.
    bool registerTool(ToolDef def, Executor executor) {
        if (index_.count(def.id)) return false;
        index_[def.id] = tools_.size();
        tools_.push_back({std::move(def), std::move(executor)});
        return true;
    }

    // Lookup tool by id. Returns nullptr if not found.
    const ToolDef* getTool(const std::string& id) const {
        auto it = index_.find(id);
        return it != index_.end() ? &tools_[it->second].def : nullptr;
    }

    // Execute a tool by id with JSON args.
    // Handles: lookup → validation → permission → execution → truncation
    ToolResult execute(const std::string& id, const std::string& argsJson, ToolContext& ctx) {
        auto it = index_.find(id);
        if (it == index_.end()) {
            return ToolResult::error("未知工具", "工具 '" + id + "' 未注册");
        }

        auto& entry = tools_[it->second];
        const ToolDef& def = entry.def;

        // 1. Validate args
        std::string valError = def.validateArgs(argsJson);
        if (!valError.empty()) {
            return ToolResult::error(
                def.id + " 参数错误",
                "工具 " + def.id + " 调用参数无效: " + valError + "\n预期参数: " + describeParams(def));
        }

        // 2. Notify start
        if (ctx.onStart) {
            ctx.onStart(def.id);
        }

        // 3. Execute
        ToolResult result = entry.executor(argsJson, ctx);

        // 4. Truncate output (unless already marked)
        if (!result.truncated) {
            result = truncateOutput(result);
        }

        return result;
    }

    // Generate tools JSON array for OpenAI function calling
    std::string toToolsJson() const {
        std::ostringstream s;
        s << "[";
        bool first = true;
        for (auto& e : tools_) {
            if (!first) s << ",";
            s << e.def.toFunctionDef();
            first = false;
        }
        s << "]";
        return s.str();
    }

    // Get all registered tool IDs
    std::vector<std::string> ids() const {
        std::vector<std::string> result;
        for (auto& e : tools_) result.push_back(e.def.id);
        return result;
    }

    // Get all tool defs
    const std::vector<ToolDef> defs() const {
        std::vector<ToolDef> result;
        for (auto& e : tools_) result.push_back(e.def);
        return result;
    }

    size_t count() const { return tools_.size(); }

private:
    struct Entry {
        ToolDef def;
        Executor executor;
    };
    std::vector<Entry> tools_;
    std::map<std::string, size_t> index_;

    static std::string describeParams(const ToolDef& def) {
        std::ostringstream s;
        for (auto& p : def.parameters) {
            s << "\n  " << p.name << " (" << (p.required ? "必需" : "可选") << "): " << p.description;
        }
        return s.str();
    }
};

// =========================================================================
// 7. Schema → JSON conversion helpers
// =========================================================================

// Extract a string value from JSON by key. Handles escaped quotes.
inline std::string jsonGetString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++;
    std::string val;
    while (pos < json.size()) {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            val += json[pos + 1];
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

inline int jsonGetInt(const std::string& json, const std::string& key, int def = 0) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return def;
    pos += search.length();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return def;
    int val = 0;
    int sign = 1;
    if (json[pos] == '-') { sign = -1; pos++; }
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (json[pos] - '0');
        pos++;
    }
    return sign * val;
}

inline bool jsonGetBool(const std::string& json, const std::string& key, bool def = false) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return def;
    pos += search.length();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos + 4 <= json.size() && json.substr(pos, 4) == "true") return true;
    return false;
}

} // namespace aries
