#pragma once
// ============================================================================
// permission_system.hpp — OpenCode-style permission system for Aries AI
//
// Ported from OpenCode:
//   permission/index.ts (evaluate, ask, ruleset merging)
//   permission/wildcard.ts (Wildcard.match)
//
// Pattern: rule-based allow/deny/ask with wildcard matching.
// Multiple rulesets are merged; the LAST matching rule wins.
// ============================================================================

#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <map>
#include <mutex>
#include <fstream>
#include <sstream>

namespace aries {

// =========================================================================
// 1. Permission Rule
// =========================================================================

struct PermissionRule {
    enum Action {
        Allow = 0,
        Deny  = 1,
        Ask   = 2
    };

    Action action = Ask;
    std::string permission; // namespace: "read", "write", "edit", "execute"
    std::string pattern;    // glob: "*", "*.txt", "bash(git:*)", "C:\\Users\\*"
    bool temporary = false; // session-only, not persisted

    // Serialize to a compact string for persistence
    std::string serialize() const {
        const char* act = action == Allow ? "allow" : (action == Deny ? "deny" : "ask");
        return std::string(act) + ":" + permission + ":" + pattern;
    }

    // Deserialize from compact string
    static PermissionRule deserialize(const std::string& s) {
        PermissionRule r;
        size_t p1 = s.find(':');
        if (p1 == std::string::npos) return r;
        size_t p2 = s.find(':', p1 + 1);
        if (p2 == std::string::npos) return r;
        std::string act = s.substr(0, p1);
        if (act == "allow") r.action = Allow;
        else if (act == "deny") r.action = Deny;
        else r.action = Ask;
        r.permission = s.substr(p1 + 1, p2 - p1 - 1);
        r.pattern = s.substr(p2 + 1);
        return r;
    }
};

// =========================================================================
// 2. Wildcard matching (ported from OpenCode Wildcard.match)
// =========================================================================

inline bool wildcardMatch(const std::string& pattern, const std::string& value) {
    // Exact match
    if (pattern == value) return true;
    // Match-all
    if (pattern == "*") return true;

    size_t starPos = pattern.find('*');
    if (starPos == std::string::npos) {
        // No wildcard: case-insensitive exact match
        if (pattern.size() != value.size()) return false;
        for (size_t i = 0; i < pattern.size(); i++) {
            if (tolower((unsigned char)pattern[i]) != tolower((unsigned char)value[i]))
                return false;
        }
        return true;
    }

    // prefix*suffix
    std::string prefix = pattern.substr(0, starPos);
    std::string suffix = pattern.substr(starPos + 1);

    // Prefix match (case-insensitive)
    if (!prefix.empty()) {
        if (value.size() < prefix.size()) return false;
        for (size_t i = 0; i < prefix.size(); i++) {
            if (tolower((unsigned char)prefix[i]) != tolower((unsigned char)value[i]))
                return false;
        }
    }

    // Suffix match (case-insensitive)
    if (!suffix.empty()) {
        if (value.size() < suffix.size()) return false;
        size_t offset = value.size() - suffix.size();
        for (size_t i = 0; i < suffix.size(); i++) {
            if (tolower((unsigned char)suffix[i]) != tolower((unsigned char)value[offset + i]))
                return false;
        }
    }

    return true;
}

// =========================================================================
// 3. Permission Ruleset
// =========================================================================

struct PermissionRuleset {
    std::string name;
    std::vector<PermissionRule> rules;

    PermissionRuleset() = default;
    PermissionRuleset(const std::string& n) : name(n) {}
};

// =========================================================================
// 4. Permission System
// =========================================================================

class PermissionSystem {
public:
    PermissionSystem() {
        rulesets_.reserve(4);
    }

    // --- Rule evaluation ---
    // Merges all rulesets, finds the LAST rule where permission + pattern both
    // match. Returns its action. If nothing matches, returns Ask.
    PermissionRule::Action evaluate(
            const std::string& permission,
            const std::string& pattern) const {
        std::lock_guard<std::mutex> lk(mutex_);

        const PermissionRule* best = nullptr;
        for (auto& rs : rulesets_) {
            for (auto& rule : rs.rules) {
                if (wildcardMatch(rule.permission, permission) &&
                    wildcardMatch(rule.pattern, pattern)) {
                    best = &rule;
                }
            }
        }

        return best ? best->action : PermissionRule::Ask;
    }

    // --- Ask callback factory ---
    // Returns a function suitable for ToolContext::ask.
    // Evaluates rules first: Allow→true, Deny→false, Ask→calls the prompt callback.
    std::function<bool(const std::string& permission, const std::string& pattern)>
    makeAsk(std::function<bool(const std::string& permission, const std::string& pattern,
                               const std::vector<std::string>& always)> promptFn) const {
        // Capture a shared_ptr to this system to allow the callback to outlive the caller
        auto self = std::make_shared<const PermissionSystem*>(this);
        auto prompt = std::make_shared<
            std::function<bool(const std::string&, const std::string&, const std::vector<std::string>&)>
        >(std::move(promptFn));

        return [self, prompt](const std::string& permission, const std::string& pattern) -> bool {
            // 1. Check rules
            PermissionRule::Action action = (*self)->evaluate(permission, pattern);

            if (action == PermissionRule::Allow) return true;
            if (action == PermissionRule::Deny)  return false;

            // 2. Ask user
            return (*prompt)(permission, pattern, std::vector<std::string>{"*"});
        };
    }

    // --- Add / remove rules ---

    void addRule(const std::string& rulesetName, PermissionRule rule) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto* rs = getOrCreate(rulesetName);
        // Replace existing rule with same permission+pattern
        for (auto& r : rs->rules) {
            if (r.permission == rule.permission && r.pattern == rule.pattern) {
                r = rule;
                return;
            }
        }
        rs->rules.push_back(std::move(rule));
    }

    void removeRule(const std::string& rulesetName,
                    const std::string& permission,
                    const std::string& pattern) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto* rs = getExisting(rulesetName);
        if (!rs) return;
        rs->rules.erase(
            std::remove_if(rs->rules.begin(), rs->rules.end(),
                [&](const PermissionRule& r) {
                    return r.permission == permission && r.pattern == pattern;
                }),
            rs->rules.end());
    }

    // --- Ruleset management ---

    void setRuleset(const std::string& name, std::vector<PermissionRule> rules) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto* rs = getOrCreate(name);
        rs->rules = std::move(rules);
    }

    std::vector<PermissionRule> getRuleset(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto* rs = getExisting(name);
        return rs ? rs->rules : std::vector<PermissionRule>{};
    }

    const std::vector<PermissionRuleset>& rulesets() const { return rulesets_; }
    std::vector<PermissionRuleset>& rulesets() { return rulesets_; }

    // --- Persistence ---

    // Save to a simple text file (one rule per line: allow:read:*)
    void saveToFile(const std::string& path) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::ofstream of(path);
        if (!of.is_open()) return;
        for (auto& rs : rulesets_) {
            for (auto& rule : rs.rules) {
                if (!rule.temporary) {
                    of << rule.serialize() << " [" << rs.name << "]\n";
                }
            }
        }
    }

    // Load from file
    void loadFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in.is_open()) return;
        std::lock_guard<std::mutex> lk(mutex_);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            // Format: allow:read:* [agent]
            size_t br = line.find(" [");
            if (br == std::string::npos) continue;
            size_t be = line.find(']', br);
            if (be == std::string::npos) continue;
            std::string ruleStr = line.substr(0, br);
            std::string rsName = line.substr(br + 2, be - br - 2);
            auto rule = PermissionRule::deserialize(ruleStr);
            getOrCreate(rsName)->rules.push_back(rule);
        }
    }

    // Clear all temporary rules
    void clearTemporary() {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& rs : rulesets_) {
            rs.rules.erase(
                std::remove_if(rs.rules.begin(), rs.rules.end(),
                    [](const PermissionRule& r) { return r.temporary; }),
                rs.rules.end());
        }
    }

private:
    mutable std::mutex mutex_;
    std::vector<PermissionRuleset> rulesets_;

    PermissionRuleset* getOrCreate(const std::string& name) {
        for (auto& rs : rulesets_) {
            if (rs.name == name) return &rs;
        }
        rulesets_.push_back(PermissionRuleset(name));
        return &rulesets_.back();
    }

    const PermissionRuleset* getExisting(const std::string& name) const {
        for (auto& rs : rulesets_) {
            if (rs.name == name) return &rs;
        }
        return nullptr;
    }

    PermissionRuleset* getExisting(const std::string& name) {
        for (auto& rs : rulesets_) {
            if (rs.name == name) return &rs;
        }
        return nullptr;
    }
};

// =========================================================================
// 5. Default rulesets for common agent types
// =========================================================================

// "chat" agent: deny execute, allow everything else
inline std::vector<PermissionRule> chatAgentDefaults() {
    return {
        {PermissionRule::Deny,  "execute", "*"},
        {PermissionRule::Allow, "*",        "*"},
    };
}

// "agent" mode: ask for destructive, allow read/search
inline std::vector<PermissionRule> agentModeDefaults() {
    return {
        {PermissionRule::Ask,   "execute", "*"},
        {PermissionRule::Ask,   "write",   "*"},
        {PermissionRule::Ask,   "edit",    "*"},
        {PermissionRule::Ask,   "delete",  "*"},
        {PermissionRule::Allow, "*",       "*"},
    };
}

// "build" agent (full access): allow everything
inline std::vector<PermissionRule> buildAgentDefaults() {
    return {
        {PermissionRule::Allow, "*", "*"},
    };
}

} // namespace aries
