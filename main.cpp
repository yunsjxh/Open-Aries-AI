#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <tlhelp32.h>
#include <atomic>
#include <functional>
#include <vector>
#include <cstdio>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <fstream>
#include <gdiplus.h>
#include <objidl.h>
#include "WebView2.h"
#include "ui_html.h"
#pragma comment(lib, "gdiplus.lib")

// Embedded DLL via objcopy
extern "C" const char _binary_WebView2Loader_dll_start[];
extern "C" const char _binary_WebView2Loader_dll_end[];
extern "C" const char _binary_WebView2Loader_dll_size[];

static void ensureDll(const wchar_t* dllPath);

#define WINDOW_CLASS L"OpenAriesAI"
#define BORDER_WIDTH  12
#define WM_AI_DELTA     (WM_APP + 1)
#define WM_TOOL_RESULT  (WM_APP + 2)
#define WM_TEST_RESULT  (WM_APP + 3)

// Test result storage (cross-thread)
std::mutex  g_testMutex;
bool        g_testOk = false;
std::string g_testMsg;

// === AI Provider includes ===
namespace aries { std::atomic<bool> g_abortFlag{false}; }
#include "ai_provider.hpp"
#include "security_config.hpp"
#include "openai_compatible_provider.hpp"
#include "tool_system.hpp"
#include "permission_system.hpp"
#include "mcp_client.hpp"

using namespace aries;

// Debug
FILE* g_log = nullptr;
#define LOG(fmt, ...) do { if (g_log) { fprintf(g_log, "[%lu] " fmt "\n", GetCurrentThreadId(), ##__VA_ARGS__); fflush(g_log); } } while(0)

// Type for CreateCoreWebView2EnvironmentWithOptions
using CreateEnvFn = HRESULT (*)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
                                 ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

// Globals
HMODULE             g_wv2Dll     = nullptr;
CreateEnvFn         g_pCreateEnv = nullptr;
ICoreWebView2Controller* g_controller = nullptr;
ICoreWebView2*           g_webview    = nullptr;
HWND                 g_hwnd       = nullptr;

// Forward
LRESULT CALLBACK ChildSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
struct ToolCall;
ToolCall parseToolCall(const std::string& text);
ToolCall parseToolCallArgs(const ToolCallInfo& tci);
std::string executeTool(const ToolCall& tc);
std::string executePowerShell(const std::string& script);
std::string listProcesses(const std::string& filter);
DWORD findProcessByName(const std::string& name);
std::string getForegroundWindowInfo();
std::string listWindows();
std::string resolvePidToName(DWORD pid);
std::string hbmpToPngBase64(HBITMAP hBitmap);
std::string captureWindowToBase64(const std::string& windowTitle);

// AI
AIProviderPtr        g_provider   = nullptr;
std::string          g_modelName;
bool                 g_aiBusy     = false;
// Tool system + Permission
ToolRegistry          g_toolRegistry;
PermissionSystem      g_permission;
McpRegistry           g_mcpRegistry;
std::mutex            g_permMutex;
// Last permission ask result (set by JS callback, read by ctx.ask)
bool                  g_permReady   = false;
bool                  g_permApproved = false;
bool                  g_permRemember = false;
std::condition_variable g_permCv;

// Agent mode
bool g_agentMode = false;
bool g_agentRunning = false;
const int AGENT_MAX_ROUNDS = 10;
std::wstring g_agentPrompt;

// Session-based conversation history
std::map<std::wstring, std::vector<ChatMessage>> g_sessions;
std::wstring g_activeSession = L"default";
std::mutex   g_historyMutex;

// Pending file attachments
struct PendingFile {
    std::string name;
    std::string content;
};
std::vector<PendingFile> g_pendingFiles;
std::mutex               g_fileMutex;

// Streaming queue
std::queue<std::wstring> g_streamQueue;
std::mutex               g_streamMutex;
bool                     g_streamDone = false;
std::wstring             g_streamFull;

// Pending screenshot for multimodal agent requests
std::string g_pendingScreenshotB64;
std::mutex  g_screenshotMutex;

// PowerShell confirmation
std::mutex              g_psMutex;
std::condition_variable g_psCv;
bool                    g_psReady    = false;
bool                    g_psApproved = false;

bool load_webview2() {
    // Extract embedded DLL to exe directory if missing
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(nullptr, dllPath, MAX_PATH);
    wchar_t* s = wcsrchr(dllPath, L'\\');
    if (s) { *(s + 1) = 0; }
    wcscat(dllPath, L"WebView2Loader.dll");
    ensureDll(dllPath);

    g_wv2Dll = LoadLibraryW(dllPath);
    if (!g_wv2Dll) { LOG("FAILED to load WebView2Loader.dll"); return false; }
    LOG("WebView2Loader.dll loaded at %p", g_wv2Dll);
    g_pCreateEnv = (CreateEnvFn)GetProcAddress(g_wv2Dll, "CreateCoreWebView2EnvironmentWithOptions");
    return g_pCreateEnv != nullptr;
}

// === Config parser ===
struct AppConfig {
    std::string apiKey;
    std::string apiHost;
    std::string model;
    bool valid = false;
};

AppConfig g_config;

AppConfig loadConfig() {
    AppConfig cfg;
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* slash = wcsrchr(exePath, L'\\');
    if (slash) *(slash + 1) = 0;
    wcscat(exePath, L"config.txt");

    // Use Windows API for Unicode path support
    HANDLE hFile = CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG("Config not found (err=%lu)", GetLastError());
        return cfg;
    }

    DWORD size = GetFileSize(hFile, nullptr);
    std::string content(size + 1, 0);
    ReadFile(hFile, &content[0], size, &size, nullptr);
    CloseHandle(hFile);

    // Parse
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;
        // Strip \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        // trim
        size_t p = key.find_first_not_of(" \t");
        if (p != std::string::npos) key.erase(0, p);
        p = key.find_last_not_of(" \t");
        if (p != std::string::npos) key.erase(p + 1);
        p = val.find_first_not_of(" \t");
        if (p != std::string::npos) val.erase(0, p);
        p = val.find_last_not_of(" \t\r");
        if (p != std::string::npos) val.erase(p + 1);

        if (key == "api_key") cfg.apiKey = val;
        else if (key == "api_host") cfg.apiHost = val;
        else if (key == "model") cfg.model = val;
    }

    if (!cfg.apiKey.empty() && !cfg.apiHost.empty()) {
        cfg.valid = true;
        if (cfg.apiHost.find("://") == std::string::npos) {
            cfg.apiHost = "https://" + cfg.apiHost;
        }
        if (cfg.model.empty()) cfg.model = "gpt-3.5-turbo";
    }

    LOG("Config: host=%s model=%s key=***%s valid=%d",
        cfg.apiHost.c_str(), cfg.model.c_str(),
        cfg.apiKey.length() > 4 ? cfg.apiKey.substr(cfg.apiKey.length() - 4).c_str() : "",
        cfg.valid);
    return cfg;
}

// === Window utils ===
std::string ws_to_utf8(const std::wstring& ws) {
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], len, nullptr, nullptr);
    return s;
}

std::wstring utf8_to_ws(const std::string& s) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    return ws;
}

void execJs(const wchar_t* fmt, ...) {
    if (!g_webview) return;
    wchar_t buf[4096];
    va_list args;
    va_start(args, fmt);
    vswprintf(buf, 4096, fmt, args);
    va_end(args);
    g_webview->ExecuteScript(buf, nullptr);
}

void execJsRaw(const std::wstring& script) {
    if (!g_webview) return;
    g_webview->ExecuteScript(script.c_str(), nullptr);
}

// === Session persistence ===

static std::wstring getSessionsDir() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* slash = wcsrchr(exePath, L'\\');
    if (slash) *(slash + 1) = 0;
    wcscat(exePath, L"sessions\\");
    return exePath;
}

static std::string escJson(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        switch (c) {
            case '\\': o += "\\\\"; break;
            case '"':  o += "\\\""; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    snprintf(buf, 8, "\\u%04x", (unsigned char)c);
                    o += buf;
                } else o += c;
        }
    }
    return o;
}

static void saveSession(const std::wstring& sessionId) {
    std::wstring dir = getSessionsDir();
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring fp = dir + sessionId + L".json";
    std::string jsonStr;
    {
        std::lock_guard<std::mutex> lk(g_historyMutex);
        auto it = g_sessions.find(sessionId);
        if (it == g_sessions.end()) return;
        std::ostringstream js;
        js << "{\"id\":\"" << escJson(ws_to_utf8(sessionId)) << "\",\"messages\":[";
        bool first = true;
        for (auto& m : it->second) {
            if (!first) js << ",";
            js << "{\"role\":\"" << escJson(m.role)
               << "\",\"content\":\"" << escJson(m.content) << "\"}";
            first = false;
        }
        js << "]}";
        jsonStr = js.str();
    }
    HANDLE h = CreateFileW(fp.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(h, jsonStr.data(), (DWORD)jsonStr.size(), &written, nullptr);
        CloseHandle(h);
    }
}

static std::string unescJson(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i+1]) {
                case '\\': o += '\\'; i++; break;
                case '"':  o += '"'; i++; break;
                case 'n':  o += '\n'; i++; break;
                case 'r':  o += '\r'; i++; break;
                case 't':  o += '\t'; i++; break;
                default:   o += s[i+1]; i++; break;
            }
        } else {
            o += s[i];
        }
    }
    return o;
}

// Forward declaration — definition is after all helpers
static void registerAllTools();

static std::string getToolsJson() {
    if (g_toolRegistry.count() == 0) registerAllTools();
    return g_toolRegistry.toToolsJson();
}

// registerAllTools() is defined after all helpers — see bottom of file

static void loadHistoryAndPush() {
    std::wstring dir = getSessionsDir();
    std::wstring pattern = dir + L"*.json";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    std::ostringstream jsArray;
    jsArray << "[";
    bool first = true;
    int count = 0;
    std::wstring firstId;

    do {
        std::wstring fileName(fd.cFileName);
        if (fileName.size() < 6) continue;
        std::wstring sid = fileName.substr(0, fileName.size() - 5);
        std::wstring fp = dir + fileName;

        HANDLE h = CreateFileW(fp.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;
        DWORD size = GetFileSize(h, nullptr);
        if (size == 0 || size > 2 * 1024 * 1024) { CloseHandle(h); continue; }
        std::string content(size + 1, 0);
        DWORD rd;
        ReadFile(h, &content[0], size, &rd, nullptr);
        CloseHandle(h);
        content.resize(rd);

        if (!first) jsArray << ",";
        jsArray << content;
        first = false;
        count++;
        if (firstId.empty()) firstId = sid;

        // Parse messages into g_sessions
        size_t mp = content.find("\"messages\"");
        if (mp != std::string::npos) {
            size_t ba = content.find('[', mp);
            size_t be = content.rfind(']');
            if (ba != std::string::npos && be != std::string::npos && be > ba) {
                std::vector<ChatMessage> msgs;
                size_t pos = ba + 1;
                while (pos < be) {
                    while (pos < be && content[pos] != '{' && content[pos] != ']') pos++;
                    if (pos >= be || content[pos] == ']') break;
                    size_t oe = content.find('}', pos);
                    if (oe == std::string::npos || oe >= be) break;
                    std::string rk = "\"role\":\"";
                    size_t rp = content.find(rk, pos);
                    std::string role, cont;
                    if (rp < oe) {
                        rp += rk.length();
                        size_t re = content.find('"', rp);
                        if (re < oe) role = content.substr(rp, re - rp);
                    }
                    std::string ck = "\"content\":\"";
                    size_t cp = content.find(ck, pos);
                    if (cp < oe) {
                        cp += ck.length();
                        if (cp < oe) {
                            size_t ce = cp;
                            while (ce < oe) {
                                if (content[ce] == '"') {
                                    int bs = 0;
                                    for (size_t k = ce; k > cp && content[k - 1] == '\\'; k--) bs++;
                                    if (bs % 2 == 0) break;
                                }
                                ce++;
                            }
                            if (ce < oe) cont = unescJson(content.substr(cp, ce - cp));
                        }
                    }
                    if (!role.empty()) msgs.emplace_back(role, cont);
                    pos = oe + 1;
                }
                if (!msgs.empty()) {
                    std::lock_guard<std::mutex> lk(g_historyMutex);
                    g_sessions[sid] = msgs;
                }
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    jsArray << "]";
    if (count == 0) return;

    std::wstring wjson = utf8_to_ws(jsArray.str());
    std::wstring escaped;
    for (wchar_t c : wjson) {
        switch (c) {
            case L'\\': escaped += L"\\\\"; break;
            case L'\'': escaped += L"\\'";  break;
            case L'\n': escaped += L"\\n";  break;
            case L'\r': break;
            default:    escaped += c;
        }
    }
    std::wstring activeEsc;
    for (wchar_t c : firstId) {
        switch (c) {
            case L'\\': activeEsc += L"\\\\"; break;
            case L'\'': activeEsc += L"\\'";  break;
            default:    activeEsc += c;
        }
    }
    execJsRaw(L"if(window.loadSavedSessions)window.loadSavedSessions('" + escaped + L"','" + activeEsc + L"')");
}

// Extract embedded DLL (objcopy symbol) to file if not present
static void ensureDll(const wchar_t* dllPath) {
    if (GetFileAttributesW(dllPath) != INVALID_FILE_ATTRIBUTES) return;
    size_t sz = (size_t)_binary_WebView2Loader_dll_size;
    if (sz == 0 || sz > 100 * 1024 * 1024) return; // sanity check
    HANDLE hf = CreateFileW(dllPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(hf, _binary_WebView2Loader_dll_start, (DWORD)sz, &written, nullptr);
    CloseHandle(hf);
}

// === WebMessage handler ===
// Tool call struct
struct ToolCall {
    std::string tool;
    std::string path;
    std::string name;
    std::string content;
    std::string source;  // for COPY_FILE / MOVE_FILE
};

class WebMsgHandler : public ICoreWebView2WebMessageReceivedEventHandler {
    LONG m_ref = 1;
public:
    ULONG STDMETHODCALLTYPE AddRef() override  { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualGUID(riid, IID_ICoreWebView2WebMessageReceivedEventHandler) ||
            IsEqualGUID(riid, IID_IUnknown)) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        LPWSTR msg;
        if (FAILED(args->TryGetWebMessageAsString(&msg))) return S_OK;
        LOG("WebMsg: %ls", msg);

        if (wcsstr(msg, L"\"resize\"")) {
            std::wstring wmsg(msg);
            auto extractStr = [&](const wchar_t* field) -> std::string {
                std::wstring key = std::wstring(L"\"") + field + L"\":\"";
                size_t s = wmsg.find(key);
                if (s == std::wstring::npos) return "";
                s += key.length();
                size_t e = wmsg.find(L'"', s);
                if (e == std::wstring::npos) return "";
                return ws_to_utf8(wmsg.substr(s, e - s));
            };
            auto extractInt = [&](const wchar_t* field) -> int {
                std::wstring key = std::wstring(L"\"") + field + L"\":";
                size_t s = wmsg.find(key);
                if (s == std::wstring::npos) return 0;
                s += key.length();
                while (s < wmsg.size() && !iswdigit(wmsg[s]) && wmsg[s] != L'-') s++;
                int sign = 1;
                if (s < wmsg.size() && wmsg[s] == L'-') { sign = -1; s++; }
                int val = 0;
                while (s < wmsg.size() && iswdigit(wmsg[s])) { val = val * 10 + (wmsg[s] - L'0'); s++; }
                return sign * val;
            };
            std::string edge = extractStr(L"edge");
            int dx = extractInt(L"dx"), dy = extractInt(L"dy");
            LOG("RESIZE edge=%s dx=%d dy=%d", edge.c_str(), dx, dy);

            RECT rc; GetWindowRect(g_hwnd, &rc);
            int L = rc.left, T = rc.top, R = rc.right, B = rc.bottom;
            int minW = 480, minH = 360;

            if (edge.find('e') != std::string::npos) R = std::max(R + dx, L + minW);
            if (edge.find('w') != std::string::npos) L = std::min(L + dx, R - minW);
            if (edge.find('s') != std::string::npos) B = std::max(B + dy, T + minH);
            if (edge.find('n') != std::string::npos) T = std::min(T + dy, B - minH);

            MoveWindow(g_hwnd, L, T, R - L, B - T, TRUE);
        } else if (wcsstr(msg, L"\"abort\"")) {
            g_abortFlag.store(true);
            LOG("ABORT requested by user");
        } else if (wcsstr(msg, L"\"sendMessage\"")) {
            // Extract content field
            std::wstring wmsg(msg);
            size_t cPos = wmsg.find(L"\"content\"");
            if (cPos != std::wstring::npos) {
                size_t start = wmsg.find(L'"', cPos + 9);
                size_t end = start + 1 < wmsg.size() ? wmsg.find(L'"', start + 1) + 1 : std::wstring::npos;
                if (start != std::wstring::npos && end != std::wstring::npos && end > start + 1) {
                    std::wstring content = wmsg.substr(start + 1, end - start - 2);
                    handleMessage(content);
                }
            }
        } else if (wcsstr(msg, L"\"dragStart\"")) {
            ReleaseCapture();
            SendMessageW(g_hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
        } else if (wcsstr(msg, L"\"minimize\"")) {
            ShowWindow(g_hwnd, SW_MINIMIZE);
        } else if (wcsstr(msg, L"\"maximize\"")) {
            if (IsZoomed(g_hwnd)) ShowWindow(g_hwnd, SW_RESTORE);
            else                 ShowWindow(g_hwnd, SW_MAXIMIZE);
        } else if (wcsstr(msg, L"\"close\"")) {
            PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
        } else if (wcsstr(msg, L"\"switchSession\"")) {
            std::wstring wmsg(msg);
            size_t idPos = wmsg.find(L"\"sessionId\"");
            if (idPos != std::wstring::npos) {
                size_t q1 = wmsg.find(L'"', idPos + 11);
                size_t q2 = q1 + 1 < wmsg.size() ? wmsg.find(L'"', q1 + 1) : std::wstring::npos;
                if (q1 != std::wstring::npos && q2 != std::wstring::npos && q2 > q1 + 1) {
                    handleSwitchSession(wmsg.substr(q1 + 1, q2 - q1 - 1));
                }
            }
        } else if (wcsstr(msg, L"\"attachFile\"")) {
            handleAttachFile();
        } else if (wcsstr(msg, L"\"toggleAgent\"")) {
            g_agentMode = !g_agentMode;
            std::wstring js = L"if(window.setAgentMode){window.setAgentMode(" +
                              std::wstring(g_agentMode ? L"true" : L"false") + L")}";
            execJsRaw(js);
            LOG("Agent mode: %d", g_agentMode);
        } else if (wcsstr(msg, L"\"newSession\"")) {
            std::wstring newId = L"s_" + std::to_wstring(GetTickCount64());
            handleSwitchSession(newId);
            std::wstring js = L"if(window.addSession){window.addSession('" + newId + L"','新对话')}";
            execJsRaw(js);
        } else if (wcsstr(msg, L"\"getConfig\"")) {
            auto esc = [](const std::string& s) -> std::wstring {
                std::wstring out;
                for (char c : s) {
                    if (c == '\\') out += L"\\\\";
                    else if (c == '\'') out += L"\\'";
                    else if (c == '\n') out += L"\\n";
                    else out += (wchar_t)(unsigned char)c;
                }
                return out;
            };
            std::wstring js = L"if(window.showConfig){window.showConfig({host:'"
                + esc(g_config.apiHost) + L"',key:'" + esc(g_config.apiKey)
                + L"',model:'" + esc(g_config.model) + L"'})}";
            execJsRaw(js);
        } else if (wcsstr(msg, L"\"saveConfig\"")) {
            std::wstring wmsg(msg);
            auto extract = [&](const wchar_t* field) -> std::string {
                std::wstring key = std::wstring(L"\"") + field + L"\":\"";
                size_t s = wmsg.find(key);
                if (s == std::wstring::npos) return "";
                s += key.length();
                size_t e = wmsg.find(L'"', s);
                if (e == std::wstring::npos) return "";
                return ws_to_utf8(wmsg.substr(s, e - s));
            };
            std::string host = extract(L"host");
            std::string key2 = extract(L"key");
            std::string model = extract(L"model");
            if (!host.empty()) g_config.apiHost = host;
            if (!key2.empty()) g_config.apiKey = key2;
            if (!model.empty()) g_config.model = model;
            g_config.valid = !g_config.apiKey.empty() && !g_config.apiHost.empty();
            // Write config.txt
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            wchar_t* slash = wcsrchr(exePath, L'\\');
            if (slash) *(slash + 1) = 0;
            wcscat(exePath, L"config.txt");
            HANDLE hFile = CreateFileW(exePath, GENERIC_WRITE, 0, nullptr,
                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                std::string content = "# Aries AI Agent 配置文件\n# 此文件由应用自动生成和更新\n\n"
                    "api_host=" + g_config.apiHost + "\n"
                    "api_key=" + g_config.apiKey + "\n"
                    "model=" + g_config.model + "\n";
                DWORD written;
                WriteFile(hFile, content.data(), (DWORD)content.size(), &written, nullptr);
                CloseHandle(hFile);
            }
            LOG("Config saved: host=%s model=%s", g_config.apiHost.c_str(), g_config.model.c_str());
            // Auto-detect model capabilities
            bool vis = (g_config.model.find("Kimi") != std::string::npos ||
                        g_config.model.find("kimi") != std::string::npos ||
                        g_config.model.find("gemini") != std::string::npos ||
                        g_config.model.find("GLM") != std::string::npos ||
                        g_config.model.find("gpt") != std::string::npos ||
                        g_config.model.find("claude") != std::string::npos);
            // Recreate AI provider with new config
            g_provider = std::make_shared<OpenAICompatibleProvider>(
                g_config.apiKey, g_config.apiHost, g_config.model, vis, false, false);
            g_modelName = g_config.model;
            g_aiBusy = false;
            LOG("AI provider recreated: %s @ %s", g_config.model.c_str(), g_config.apiHost.c_str());
            // Notify JS
            std::wstring js = L"if(window.showToast){window.showToast('配置已更新，模型: " +
                utf8_to_ws(g_config.model) + L"')}";
            execJsRaw(js);
        } else if (wcsstr(msg, L"\"confirmRunPs\"")) {
            bool approved = wcsstr(msg, L"\"approved\":true") != nullptr;
            {
                std::lock_guard<std::mutex> lk(g_psMutex);
                g_psApproved = approved;
                g_psReady = true;
            }
            g_psCv.notify_one();
        } else if (wcsstr(msg, L"\"confirmKill\"")) {
            bool approved = wcsstr(msg, L"\"approved\":true") != nullptr;
            {
                std::lock_guard<std::mutex> lk(g_psMutex);
                g_psApproved = approved;
                g_psReady = true;
            }
            g_psCv.notify_one();
        } else if (wcsstr(msg, L"\"confirmPerm\"")) {
            bool approved = wcsstr(msg, L"\"approved\":true") != nullptr;
            bool remember = wcsstr(msg, L"\"remember\":true") != nullptr;
            {
                std::lock_guard<std::mutex> lk(g_permMutex);
                g_permApproved = approved;
                g_permRemember = remember;
                g_permReady = true;
            }
            g_permCv.notify_one();
        } else if (wcsstr(msg, L"\"testConnection\"")) {
            std::wstring wmsg(msg);
            auto extract = [&](const wchar_t* field) -> std::string {
                std::wstring key = std::wstring(L"\"") + field + L"\":\"";
                size_t s = wmsg.find(key);
                if (s == std::wstring::npos) return "";
                s += key.length();
                size_t e = wmsg.find(L'"', s);
                if (e == std::wstring::npos) return "";
                return ws_to_utf8(wmsg.substr(s, e - s));
            };
            std::string host = extract(L"host");
            std::string key2 = extract(L"key");
            std::string model = extract(L"model");
            std::thread([host, key2, model]() {
                CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                std::string url = host;
                if (url.find("://") == std::string::npos) url = "https://" + url;
                auto testProvider = std::make_shared<OpenAICompatibleProvider>(key2, url, model, false, false, false);
                providerSetLog(g_log);
                // Step 1: basic connectivity
                bool ok = testProvider->validateApiKey();
                if (!ok) {
                    std::string err = testProvider->getLastError();
                    CoUninitialize();
                    std::lock_guard<std::mutex> lk(g_testMutex);
                    g_testOk = false;
                    g_testMsg = "连接失败: " + err;
                    PostMessageW(g_hwnd, WM_TEST_RESULT, 0, 0);
                    return;
                }
                // Step 2: test vision support
                bool hasVision = testProvider->testVisionSupport();
                CoUninitialize();
                {
                    std::lock_guard<std::mutex> lk(g_testMutex);
                    g_testOk = true;
                    g_testMsg = hasVision
                        ? "连接成功！视觉模型（支持图片输入）"
                        : "连接成功！仅文本模型（不支持图片）";
                }
                PostMessageW(g_hwnd, WM_TEST_RESULT, 0, 0);
            }).detach();
            LOG("Test connection started: host=%s model=%s", host.c_str(), model.c_str());
        } else if (wcsstr(msg, L"\"getMcpConfig\"")) {
            // Read mcp_servers.json and send to JS
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            wchar_t* s2 = wcsrchr(exePath, L'\\');
            if (s2) *(s2 + 1) = 0;
            wcscat(exePath, L"mcp_servers.json");
            HANDLE h = CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            std::string json = "[]";
            if (h != INVALID_HANDLE_VALUE) {
                DWORD size = GetFileSize(h, nullptr);
                if (size > 0 && size <= 65536) {
                    json.resize(size);
                    DWORD rd;
                    ReadFile(h, &json[0], size, &rd, nullptr);
                    json.resize(rd);
                }
                CloseHandle(h);
            }
            // Extract just the servers array — or send whole config as raw
            std::wstring wjson = utf8_to_ws(json);
            std::wstring escaped;
            for (wchar_t c : wjson) {
                switch (c) {
                    case L'\\': escaped += L"\\\\"; break;
                    case L'\'': escaped += L"\\'"; break;
                    case L'\n': escaped += L"\\n"; break;
                    case L'\r': break;
                    default: escaped += c;
                }
            }
            std::wstring js = L"if(window.showMcpConfig){var s='';try{s=JSON.parse('" + escaped + L"').servers||[]}catch(e){}window.showMcpConfig(s)}";
            execJsRaw(js);
        } else if (wcsstr(msg, L"\"saveMcpConfig\"")) {
            // Parse servers array from JSON and write to mcp_servers.json
            std::wstring wmsg(msg);
            // Extract the "servers" value — look for the JSON array after "servers":
            size_t svPos = wmsg.find(L"\"servers\":");
            if (svPos != std::wstring::npos) {
                size_t arrStart = wmsg.find(L'[', svPos);
                if (arrStart != std::wstring::npos) {
                    // Find matching ]
                    int depth = 0;
                    size_t arrEnd = arrStart;
                    for (size_t i = arrStart; i < wmsg.size(); i++) {
                        if (wmsg[i] == L'"') { i++; while (i < wmsg.size() && wmsg[i] != L'"') { if (wmsg[i] == L'\\') i++; i++; } }
                        else if (wmsg[i] == L'[') depth++;
                        else if (wmsg[i] == L']') { depth--; if (depth == 0) { arrEnd = i; break; } }
                    }
                    std::string serversJson = ws_to_utf8(wmsg.substr(arrStart, arrEnd - arrStart + 1));
                    // Write config file
                    wchar_t exePath[MAX_PATH];
                    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                    wchar_t* s3 = wcsrchr(exePath, L'\\');
                    if (s3) *(s3 + 1) = 0;
                    wcscat(exePath, L"mcp_servers.json");
                    std::string content = "{\n  \"servers\": " + serversJson + "\n}\n";
                    HANDLE hFile = CreateFileW(exePath, GENERIC_WRITE, 0, nullptr,
                                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD written;
                        WriteFile(hFile, content.data(), (DWORD)content.size(), &written, nullptr);
                        CloseHandle(hFile);
                        LOG("MCP config saved: %zu bytes", content.size());
                        execJsRaw(L"if(window.showToast){window.showToast('MCP 配置已保存，重启后生效')}");
                    }
                }
            }
        }

        CoTaskMemFree(msg);
        return S_OK;
    }

    void handleMessage(const std::wstring& userMsg) {
        if (g_aiBusy) return;
        if (!g_provider) {
            execJsRaw(L"if(window.openSettings)window.openSettings()");
            execJsRaw(L"if(window.showToast)window.showToast('请先配置 API 连接')");
            return;
        }
        g_aiBusy = true;
        g_abortFlag.store(false);
        std::string text = ws_to_utf8(userMsg);

        // Append pending file contents
        {
            std::lock_guard<std::mutex> lk(g_fileMutex);
            if (!g_pendingFiles.empty()) {
                text += "\n\n以下为用户附加的文件内容：";
                for (auto& f : g_pendingFiles) {
                    text += "\n\n=== " + f.name + " ===\n" + f.content;
                }
                g_pendingFiles.clear();
            }
        }

        // Add user message to active session history
        {
            std::lock_guard<std::mutex> lk(g_historyMutex);
            g_sessions[g_activeSession].emplace_back("user", text);
        }
        saveSession(g_activeSession);

        // Clear queue
        {
            std::lock_guard<std::mutex> lk(g_streamMutex);
            while (!g_streamQueue.empty()) g_streamQueue.pop();
            g_streamFull.clear();
            g_streamDone = false;
        }

        // Show loading skeleton
        execJs(L"window.setLoading(true)");

        // Capture state for thread
        std::wstring sessionId = g_activeSession;
        bool agentMode = g_agentMode;

        // Async AI call with agent loop
        if (agentMode) g_agentRunning = true;
        LOG("SendMessage: agent=%d | session=%ls | input=\"%s\"",
            agentMode, sessionId.c_str(), text.substr(0, 200).c_str());
        std::thread([sessionId, agentMode]() {
            // Build messages from session history
            auto getMsgs = [&]() -> std::vector<ChatMessage> {
                std::lock_guard<std::mutex> lk(g_historyMutex);
                auto it = g_sessions.find(sessionId);
                return it != g_sessions.end() ? it->second : std::vector<ChatMessage>{};
            };

            std::string systemPrompt;
            std::string toolsJson;
            if (agentMode) {
                systemPrompt = ws_to_utf8(g_agentPrompt);
                toolsJson = getToolsJson();
            } else {
                systemPrompt = "你是一个乐于助人的AI助手。请用中文简洁地回答用户的问题。";
            }

            // Helper: build multimodal JSON body when screenshot is pending
            auto buildMultimodalJson = [&](const std::vector<ChatMessage>& msgs,
                                             const std::string& b64,
                                             const std::string& sysPrompt) -> std::string {
                auto esc = [](const std::string& s) {
                    std::string o;
                    for (char c : s) {
                        switch (c) {
                            case '\\': o += "\\\\"; break;
                            case '"':  o += "\\\""; break;
                            case '\n': o += "\\n"; break;
                            case '\r': break;
                            case '\t': o += "\\t"; break;
                            default:
                                if ((unsigned char)c < 0x20) { char buf[8]; snprintf(buf, 8, "\\u%04x", (unsigned char)c); o += buf; }
                                else o += c;
                        }
                    }
                    return o;
                };
                std::ostringstream js;
                js << "{\"model\":\"" << esc(g_config.model) << "\",\"stream\":false,\"messages\":[";
                bool first = true;
                if (!sysPrompt.empty()) {
                    js << "{\"role\":\"system\",\"content\":\"" << esc(sysPrompt) << "\"}";
                    first = false;
                }
                for (size_t i = 0; i < msgs.size(); i++) {
                    if (!first) js << ",";
                    js << "{\"role\":\"" << msgs[i].role << "\",\"content\":\"" << esc(msgs[i].content) << "\"}";
                    first = false;
                }
                // Append a new user multimodal message with the screenshot
                if (!first) js << ",";
                js << "{\"role\":\"user\",\"content\":[";
                js << "{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/png;base64," << b64 << "\",\"detail\":\"high\"}},";
                js << "{\"type\":\"text\",\"text\":\"请根据这张截图内容回答我的问题。\"}";
                js << "]}";
                js << "],\"max_tokens\":4096";
                js << "}";
                return js.str();
            };

            // Agent loop: up to AGENT_MAX_ROUNDS iterations
            for (int round = 0; round < (agentMode ? AGENT_MAX_ROUNDS : 1); round++) {
                if (g_abortFlag.load()) {
                    LOG("Agent aborted by user at round %d", round);
                    execJs(L"window.setLoading(false)");
                    break;
                }
                LOG("Agent round %d/%d | session=%ls | history=%zu msgs",
                    round + 1, agentMode ? AGENT_MAX_ROUNDS : 1,
                    sessionId.c_str(), getMsgs().size());

                // Reset stream state for this round
                {
                    std::lock_guard<std::mutex> lk(g_streamMutex);
                    while (!g_streamQueue.empty()) g_streamQueue.pop();
                    g_streamFull.clear();
                    g_streamDone = false;
                }

                std::vector<ChatMessage> msgs = getMsgs();

                std::string accumulatedThinking;
                std::string accumulatedContent;

                auto streamCb = [&](const std::string& delta, bool done) {
                    if (g_abortFlag.load()) return; // user requested stop
                    if (!delta.empty()) {
                        if (agentMode) {
                            if (delta[0] == '\x01') {
                            accumulatedThinking += delta.substr(1);
                            std::lock_guard<std::mutex> lk(g_streamMutex);
                            g_streamQueue.push(utf8_to_ws(delta));
                        }
                            else if (delta[0] == '\x02') {
                                std::lock_guard<std::mutex> lk(g_streamMutex);
                                g_streamQueue.push(L"__TOOL_CALLING__" + utf8_to_ws(delta.substr(1)));
                            }
                            else if (delta[0] == '\x03') {
                                std::lock_guard<std::mutex> lk(g_streamMutex);
                                g_streamQueue.push(L"__TOOL_ARGS__" + utf8_to_ws(delta.substr(1)));
                            }
                            else {
                                accumulatedContent += delta;
                                // Stream text deltas to UI in real-time (like non-agent mode)
                                std::lock_guard<std::mutex> lk(g_streamMutex);
                                g_streamQueue.push(utf8_to_ws(delta));
                            }
                        } else {
                            std::lock_guard<std::mutex> lk(g_streamMutex);
                            g_streamQueue.push(utf8_to_ws(delta));
                            g_streamFull += utf8_to_ws(delta);
                        }
                    }
                    if (done) {
                        std::lock_guard<std::mutex> lk(g_streamMutex);
                        g_streamDone = true;
                    }
                    PostMessageW(g_hwnd, WM_AI_DELTA, 0, 0);
                };

                // Check for pending screenshot → multimodal path
                std::string screenshotB64;
                {
                    std::lock_guard<std::mutex> lk(g_screenshotMutex);
                    screenshotB64 = g_pendingScreenshotB64;
                }

                std::pair<bool, std::string> result;
                std::vector<ToolCallInfo> nativeToolCalls;
                if (!screenshotB64.empty()) {
                    {
                        std::lock_guard<std::mutex> lk(g_screenshotMutex);
                        g_pendingScreenshotB64.clear();
                    }
                    std::string jsonBody = buildMultimodalJson(msgs, screenshotB64, systemPrompt);
                    LOG("Agent round %d: multimodal mode (image %zu chars, non-streaming)",
                        round + 1, screenshotB64.length());
                    // Non-streaming: use sendRequest (same as sendMessageWithImages)
                    result = g_provider->sendRawRequest(jsonBody);
                    if (result.first && !result.second.empty()) {
                        std::string full = result.second;
                        const char* thinkTag = "<思考过程>\n";
                        const char* thinkEndTag = "\n</思考过程>\n\n";
                        size_t ts = full.find(thinkTag);
                        if (ts != std::string::npos) {
                            size_t tcStart = ts + strlen(thinkTag);
                            size_t te = full.find(thinkEndTag, tcStart);
                            if (te != std::string::npos) {
                                accumulatedThinking = full.substr(tcStart, te - tcStart);
                                accumulatedContent = full.substr(te + strlen(thinkEndTag));
                            } else {
                                accumulatedContent = full;
                            }
                        } else {
                            accumulatedContent = full;
                        }
                        {
                            std::lock_guard<std::mutex> lk(g_streamMutex);
                            if (!accumulatedThinking.empty()) {
                                g_streamQueue.push(std::wstring(L"\x01") + utf8_to_ws(accumulatedThinking));
                            }
                            g_streamQueue.push(utf8_to_ws(accumulatedContent));
                            g_streamDone = true;
                        }
                        PostMessageW(g_hwnd, WM_AI_DELTA, 0, 0);
                    }
                } else {
                    if (agentMode && !toolsJson.empty()) {
                        result = g_provider->sendMessageStreamWithTools(msgs, streamCb, systemPrompt, toolsJson, nativeToolCalls);
                    } else {
                        result = g_provider->sendMessageStream(msgs, streamCb, systemPrompt);
                    }
                }

                if (!result.first) {
                    LOG("Agent round %d FAILED: %s",
                        round + 1, g_provider ? g_provider->getLastError().c_str() : "unknown");
                    std::lock_guard<std::mutex> lk(g_streamMutex);
                    g_streamQueue.push(utf8_to_ws("\n\n[请求失败]"));
                    g_streamDone = true;
                    PostMessageW(g_hwnd, WM_AI_DELTA, 0, 0);
                    break;
                }

                // Wait for stream to complete
                Sleep(200);

                if (!agentMode) {
                    // Non-agent: content already streamed, save to history (strip markers)
                    std::string full;
                    {
                        std::lock_guard<std::mutex> lk(g_streamMutex);
                        full = ws_to_utf8(g_streamFull);
                    }
                    // Strip \x01 markers
                    std::string clean;
                    for (char c : full) if (c != '\x01') clean += c;
                    if (!clean.empty()) {
                        {
                            std::lock_guard<std::mutex> lk(g_historyMutex);
                            g_sessions[sessionId].emplace_back("assistant", clean);
                        }
                        saveSession(sessionId);
                    }
                    break;
                }

                // Agent mode: process accumulated response
                LOG("Agent round %d response: thinking=%zu chars, content=%zu chars",
                    round + 1, accumulatedThinking.length(), accumulatedContent.length());
                if (accumulatedThinking.empty() && accumulatedContent.empty()) {
                    LOG("Agent round %d: empty response, breaking", round + 1);
                    // Push fallback message to UI
                    {
                        std::lock_guard<std::mutex> lk(g_streamMutex);
                        g_streamDone = false;
                        std::string emptyMsg = "Model returned empty response. Please try again.";
                    g_streamQueue.push(utf8_to_ws(emptyMsg));
                        g_streamDone = true;
                        PostMessageW(g_hwnd, WM_AI_DELTA, 0, 0);
                    }
                    {
                        std::lock_guard<std::mutex> lk(g_historyMutex);
                        g_sessions[sessionId].emplace_back("assistant",
                            "（模型返回空响应，请重试。）");
                    }
                    saveSession(sessionId);
                    break;
                }

                // Check for native tool calls first, fall back to text parsing
                ToolCall tc;
                if (!nativeToolCalls.empty()) {
                    tc = parseToolCallArgs(nativeToolCalls[0]);
                    LOG("Agent round %d: native TOOL=%s args=%s",
                        round + 1, tc.tool.c_str(), nativeToolCalls[0].arguments.c_str());
                } else {
                    tc = parseToolCall(accumulatedContent);
                }
                if (tc.tool.empty()) {
                    // No tool call — final answer (thinking+content already streamed via deltas)
                    LOG("Agent round %d: no tool call → final answer", round + 1);
                    {
                        std::lock_guard<std::mutex> lk(g_streamMutex);
                        g_streamDone = true;
                        PostMessageW(g_hwnd, WM_AI_DELTA, 0, 0);
                    }
                    {
                        std::lock_guard<std::mutex> lk(g_historyMutex);
                        g_sessions[sessionId].emplace_back("assistant", accumulatedContent);
                    }
                    saveSession(sessionId);
                    break;
                }

                // Tool call found (native function calling — no fake text in history)
                LOG("Agent round %d: TOOL=%s path=%s name=%s source=%s content_len=%zu",
                    round + 1, tc.tool.c_str(), tc.path.c_str(), tc.name.c_str(), tc.source.c_str(), tc.content.length());

                // Execute tool and push result via addToolBlock
                std::string toolResult = executeTool(tc);
                LOG("Agent round %d: TOOL RESULT (%s) → %zu chars",
                    round + 1, tc.tool.c_str(), toolResult.length());
                // Log first 500 chars of result
                LOG("Agent round %d: RESULT preview: %s",
                    round + 1, toolResult.substr(0, 500).c_str());
                {
                    std::lock_guard<std::mutex> lk(g_streamMutex);
                    g_streamDone = false;
                    g_streamFull.clear();
                    // Use __TOOL__ prefix: name\x1fresult
                    std::string toolDesc = tc.tool;
                    if (!tc.name.empty()) toolDesc += " → " + tc.name;
                    else if (!tc.path.empty()) toolDesc += " → " + tc.path;
                    std::wstring toolLabel = L"__TOOL__" + utf8_to_ws(toolDesc) + L"\x1f" + utf8_to_ws(toolResult);
                    g_streamQueue.push(toolLabel);
                    g_streamDone = true;
                    PostMessageW(g_hwnd, WM_AI_DELTA, 0, 0);
                }

                // Add tool result to history for next round
                {
                    std::lock_guard<std::mutex> lk(g_historyMutex);
                    g_sessions[sessionId].emplace_back("system",
                        "工具执行结果 (" + tc.tool + "):\n" + toolResult +
                        "\n\n请继续处理。如果任务已完成，**必须**用中文给用户一个清晰的总结，即使工具已经列出了数据也要总结一遍，不要回复空内容。");
                }
                saveSession(sessionId);

                Sleep(300);
            }

            g_agentRunning = false;
            g_aiBusy = false;
            LOG("Agent loop finished | session=%ls", sessionId.c_str());
            PostMessageW(g_hwnd, WM_AI_DELTA, 0, 0);
        }).detach();
    }

    void handleSwitchSession(const std::wstring& sessionId) {
        std::lock_guard<std::mutex> lk(g_historyMutex);
        g_activeSession = sessionId;
        if (g_sessions.find(sessionId) == g_sessions.end()) {
            g_sessions[sessionId] = {};
        }
        LOG("Switched to session: %ls", sessionId.c_str());
    }

    void handleAttachFile() {
        wchar_t fileBuf[8192] = {};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hwnd;
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = 8192;
        ofn.lpstrFilter = L"All Files\0*.*\0Text & Code\0*.txt;*.md;*.cpp;*.hpp;*.h;*.c;*.py;*.js;*.ts;*.html;*.css;*.json;*.xml;*.yaml;*.yml;*.log;*.csv;*.ini;*.cfg\0Images\0*.png;*.jpg;*.jpeg;*.gif;*.bmp;*.webp\0\0";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
        ofn.lpstrTitle = L"选择文件发送给 AI";

        if (!GetOpenFileNameW(&ofn)) return;

        // Collect selected files
        wchar_t* p = fileBuf;
        std::wstring dir = p;
        if (dir.length() > 0 && fileBuf[dir.length()] == 0) {
            // Single file: fileBuf is full path
            attachSingleFile(p);
        } else {
            // Multi-select: first string is dir, rest are filenames
            p += dir.length() + 1;
            while (*p) {
                std::wstring fullPath = dir + L"\\" + p;
                attachSingleFile(fullPath);
                p += wcslen(p) + 1;
            }
        }
    }

    void attachSingleFile(const std::wstring& filePath) {
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;

        DWORD size = GetFileSize(hFile, nullptr);
        if (size > 512000) {
            CloseHandle(hFile);
            execJsRaw(L"if(window.showToast){window.showToast('文件过大，最大500KB')}");
            return;
        }

        std::string content(size + 1, 0);
        DWORD read = 0;
        ReadFile(hFile, &content[0], size, &read, nullptr);
        CloseHandle(hFile);
        content.resize(read);

        // Check if text (no null bytes in first 8KB)
        bool isText = true;
        for (size_t i = 0; i < std::min<size_t>(read, (DWORD)8000); i++) {
            if (content[i] == 0) { isText = false; break; }
        }
        if (!isText) {
            execJsRaw(L"if(window.showToast){window.showToast('暂不支持二进制文件')}");
            return;
        }

        // Extract filename
        size_t lastSlash = filePath.find_last_of(L"\\/");
        std::wstring fileName = lastSlash != std::wstring::npos ? filePath.substr(lastSlash + 1) : filePath;
        std::string name = ws_to_utf8(fileName);

        // Store in C++ pending list
        {
            std::lock_guard<std::mutex> lk(g_fileMutex);
            g_pendingFiles.push_back({name, content});
        }

        // Notify JS with just name + first line preview
        std::string preview = content.substr(0, std::min<size_t>(content.find('\n'), 100));
        std::wstring wpreview = utf8_to_ws(preview);
        std::wstring escaped;
        for (wchar_t c : wpreview) {
            switch (c) {
            case L'\\': escaped += L"\\\\"; break;
            case L'\'': escaped += L"\\'"; break;
            case L'"': escaped += L"\\\""; break;
            case L'\n': escaped += L"\\n"; break;
            case L'\r': break;
            default: escaped += c;
            }
        }

        std::wstring js = L"if(window.addFileContext){window.addFileContext('" +
            fileName + L"','" + escaped + L"')}";
        execJsRaw(js);

        LOG("Attached file: %ls (%lu bytes)", fileName.c_str(), read);
    }
};

// === Agent Mode: Tool Execution ===
// (ToolCall struct defined before class, functions defined here)

ToolCall parseToolCall(const std::string& text) {
    ToolCall tc;
    const char* tools[] = {"READ_FILE", "WRITE_FILE", "COPY_FILE", "DELETE_FILE", "MOVE_FILE", "LIST_DIR", "LIST_APPS", "OPEN_APP", "UNINSTALL_APP", "OPEN_APP_LOCATION", "RUN_PS", "LIST_PROCESSES", "KILL_PROCESS", "GET_FOREGROUND_WINDOW", "LIST_WINDOWS", "CAPTURE_WINDOW"};
    for (auto t : tools) {
        std::string tag = "[" + std::string(t) + "]";
        size_t start = text.find(tag);
        if (start == std::string::npos) continue;
        size_t bodyStart = start + tag.length();
        // Skip optional newline/space after tag
        while (bodyStart < text.size() && (text[bodyStart] == '\n' || text[bodyStart] == ' ')) bodyStart++;

        // Find end marker: try [END] first, then bare END at end of text
        size_t end = text.find("[END]", bodyStart);
        if (end == std::string::npos) {
            // Try bare "END" — look for it at end of text or before a newline
            size_t endBare = text.rfind("END");
            if (endBare != std::string::npos && endBare >= bodyStart) {
                // Accept if END is at the very end of text or followed only by whitespace/newline
                size_t afterEnd = endBare + 3;
                bool atEnd = true;
                for (size_t i = afterEnd; i < text.size(); i++) {
                    if (text[i] != ' ' && text[i] != '\n' && text[i] != '\r') { atEnd = false; break; }
                }
                if (atEnd) end = endBare;
            }
        }
        if (end == std::string::npos) continue;

        tc.tool = t;
        std::string body = text.substr(bodyStart, end - bodyStart);
        // Remove trailing \n
        while (!body.empty() && body.back() == '\n') body.pop_back();

        // Parse key=value pairs
        size_t pos = 0;
        while (pos < body.length()) {
            size_t eq = body.find('=', pos);
            if (eq == std::string::npos) break;
            std::string key = body.substr(pos, eq - pos);
            // Trim key
            while (!key.empty() && (key.back() == ' ' || key.back() == '\r')) key.pop_back();
            while (!key.empty() && key.front() == ' ') key.erase(0, 1);

            size_t valEnd = body.find('\n', eq + 1);
            if (valEnd == std::string::npos) valEnd = body.length();
            std::string val = body.substr(eq + 1, valEnd - eq - 1);
            while (!val.empty() && val.back() == '\r') val.pop_back();
            while (!val.empty() && val.front() == ' ') val.erase(0, 1);

            if (key == "path") tc.path = val;
            else if (key == "name") tc.name = val;
            else if (key == "content") tc.content = val;
            else if (key == "source") tc.source = val;

            pos = valEnd + 1;
            if (pos >= body.length()) break;
        }
        break;
    }
    return tc;
}

// Parse ToolCallInfo arguments JSON into ToolCall (native function calling → legacy struct)
ToolCall parseToolCallArgs(const ToolCallInfo& tci) {
    ToolCall tc;
    tc.tool = tci.name;
    const auto& json = tci.arguments;

    auto extractStr = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.length();
        // Skip optional whitespace between : and "
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        if (pos >= json.size() || json[pos] != '"') return "";
        pos++; // skip opening quote
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
    };

    tc.path = extractStr("path");
    tc.name = extractStr("name");
    if (tc.name.empty()) tc.name = extractStr("destination");
    tc.content = extractStr("content");
    tc.source = extractStr("source");
    return tc;
}

// === App management helpers ===
std::string resolveShortcut(const std::wstring& lnkPath) {
    IShellLinkW* psl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_IShellLinkW, (void**)&psl))) return "";
    IPersistFile* ppf = nullptr;
    if (FAILED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) { psl->Release(); return ""; }
    if (FAILED(ppf->Load(lnkPath.c_str(), STGM_READ))) { ppf->Release(); psl->Release(); return ""; }
    wchar_t target[MAX_PATH];
    HRESULT hr = psl->GetPath(target, MAX_PATH, nullptr, SLGP_RAWPATH);
    ppf->Release(); psl->Release();
    if (FAILED(hr)) return "";
    return ws_to_utf8(target);
}

void scanStartMenu(const std::wstring& dir, std::string& result, int& count) {
    if (count >= 500) return;
    std::wstring pattern = dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (count >= 500) break;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring fullPath = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scanStartMenu(fullPath, result, count);
        } else {
            std::wstring nm(fd.cFileName);
            if (nm.length() < 5) continue;
            std::wstring ext = nm.substr(nm.length() - 4);
            for (auto& c : ext) c = towlower(c);
            if (ext == L".lnk") {
                std::string target = resolveShortcut(fullPath);
                result += "  " + ws_to_utf8(nm.substr(0, nm.length() - 4));
                if (!target.empty()) result += "  →  " + target;
                result += "\n";
                count++;
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

std::wstring findAppByName(const std::string& name) {
    std::string lowerName = name;
    for (auto& c : lowerName) c = (char)tolower((unsigned char)c);
    // Search Start Menu directories
    wchar_t dirs[2][MAX_PATH];
    int dirCount = 0;
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, dirs[dirCount]))) dirCount++;
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, dirs[dirCount]))) dirCount++;
    // Also check common install locations
    wchar_t progFiles[MAX_PATH], progFilesX86[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, progFiles))) {
        // Will search directly for .exe in subdirs
    }
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr, 0, progFilesX86))) {
        // Will search directly for .exe in subdirs
    }

    // Recursive search in Start Menu for .lnk matching name
    for (int di = 0; di < dirCount; di++) {
        std::vector<std::wstring> stack;
        stack.push_back(dirs[di]);
        while (!stack.empty()) {
            std::wstring dir = stack.back();
            stack.pop_back();
            std::wstring pattern = dir + L"\\*";
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) continue;
            do {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                std::wstring fullPath = dir + L"\\" + fd.cFileName;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    stack.push_back(fullPath);
                } else {
                    std::wstring fn(fd.cFileName);
                    if (fn.length() < 5) continue;
                    std::wstring extW = fn.substr(fn.length() - 4);
                    for (auto& c : extW) c = towlower(c);
                    if (extW == L".lnk") {
                        std::string displayName = ws_to_utf8(fn.substr(0, fn.length() - 4));
                        std::string ldn = displayName;
                        for (auto& c : ldn) c = (char)tolower((unsigned char)c);
                        if (ldn.find(lowerName) != std::string::npos) {
                            std::string target = resolveShortcut(fullPath);
                            if (!target.empty()) return utf8_to_ws(target);
                        }
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }
    return L"";
}

std::string findUninstallString(const std::string& appName) {
    std::string lowerName = appName;
    for (auto& c : lowerName) c = (char)tolower((unsigned char)c);
    // Query registry uninstall keys
    const wchar_t* keys[] = {
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
    };
    for (auto keyPath : keys) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS) continue;
        DWORD idx = 0;
        wchar_t subKey[256];
        DWORD sz = 256;
        while (RegEnumKeyExW(hKey, idx++, subKey, &sz, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            HKEY hSub;
            if (RegOpenKeyExW(hKey, subKey, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                wchar_t display[256] = {}, uninst[2048] = {};
                DWORD ds = sizeof(display), us = sizeof(uninst);
                RegQueryValueExW(hSub, L"DisplayName", nullptr, nullptr, (BYTE*)display, &ds);
                RegQueryValueExW(hSub, L"UninstallString", nullptr, nullptr, (BYTE*)uninst, &us);
                if (display[0] && uninst[0]) {
                    std::string dn = ws_to_utf8(display);
                    std::string ldn = dn;
                    for (auto& c : ldn) c = (char)tolower((unsigned char)c);
                    if (ldn.find(lowerName) != std::string::npos) {
                        RegCloseKey(hSub);
                        RegCloseKey(hKey);
                        return ws_to_utf8(uninst);
                    }
                }
                RegCloseKey(hSub);
            }
            sz = 256;
        }
        RegCloseKey(hKey);
    }
    return "";
}

// Path validation wrapper — returns empty string on success, error message on failure
static std::string checkPath(const std::string& utf8Path, const std::string& operation) {
    std::wstring wpath = utf8_to_ws(utf8Path);
    auto result = validatePath(wpath, SecurityConfigLoader::loadFromFileAndEnv());
    if (result.result != PathValidationResult::Ok) {
        return "安全限制：" + result.reason + " (" + operation + ": " + utf8Path + ")";
    }
    return "";
}

// Legacy wrapper (delegates to ToolRegistry)
std::string executeTool(const ToolCall& tc) {
    // Build JSON args from ToolCall fields
    std::ostringstream json;
    json << "{";
    bool first = true;
    auto addStr = [&](const char* key, const std::string& val) {
        if (val.empty()) return;
        if (!first) json << ",";
        json << "\"" << key << "\":\"";
        for (char c : val) {
            switch (c) { case '\\': json << "\\\\"; break; case '"': json << "\\\""; break; case '\n': json << "\\n"; break; case '\r': break; default: json << c; }
        }
        json << "\"";
        first = false;
    };
    addStr("path", tc.path);
    addStr("name", tc.name);
    addStr("content", tc.content);
    addStr("source", tc.source);
    json << "}";
    std::string args = json.str();
    // Build ToolContext with permission wired to existing confirmation flow
    ToolContext ctx;
    ctx.sessionId = ws_to_utf8(g_activeSession);
    ctx.agent = g_agentMode ? "agent" : "chat";
    ctx.ask = [&](const std::string& permission, const std::string& pattern) -> bool {
        // 1. Check permission rules
        PermissionRule::Action action = g_permission.evaluate(permission, pattern);
        if (action == PermissionRule::Allow) {
            LOG("PERM allow (rule): %s / %s", permission.c_str(), pattern.c_str());
            return true;
        }
        if (action == PermissionRule::Deny) {
            LOG("PERM deny (rule): %s / %s", permission.c_str(), pattern.c_str());
            execJsRaw(L"if(window.showToast){window.showToast('操作被安全策略拒绝: " +
                       utf8_to_ws(permission) + L"')}");
            return false;
        }

        // 2. Ask user via confirmation dialog
        LOG("PERM ask: %s / %s", permission.c_str(), pattern.c_str());
        std::wstring wperm = utf8_to_ws(permission);
        std::wstring wpat = utf8_to_ws(pattern);
        // Escape for JS string
        auto esc = [](const std::wstring& s) {
            std::wstring o;
            for (wchar_t c : s) {
                switch (c) {
                    case L'\\': o += L"\\\\"; break;
                    case L'\'': o += L"\\'"; break;
                    case L'\n': o += L"\\n"; break;
                    case L'\r': break;
                    default: o += c;
                }
            }
            return o;
        };

        {
            std::lock_guard<std::mutex> lk(g_permMutex);
            g_permReady = false;
            g_permApproved = false;
            g_permRemember = false;
        }
        execJsRaw(L"if(window.confirmPerm){window.confirmPerm('" +
                   esc(wperm) + L"','" + esc(wpat) + L"')}");
        {
            std::unique_lock<std::mutex> lock(g_permMutex);
            g_permCv.wait_for(lock, std::chrono::seconds(120), []{ return g_permReady; });
        }
        bool approved = false;
        bool remember = false;
        {
            std::lock_guard<std::mutex> lk(g_permMutex);
            approved = g_permApproved;
            remember = g_permRemember;
            g_permReady = false;
            g_permApproved = false;
            g_permRemember = false;
        }

        // 3. If user wants to remember, add a persistent rule
        if (remember && approved) {
            g_permission.addRule("user", {PermissionRule::Allow, permission, pattern, false});
            LOG("PERM saved: allow %s / %s", permission.c_str(), pattern.c_str());
        } else if (remember && !approved) {
            g_permission.addRule("user", {PermissionRule::Deny, permission, pattern, false});
            LOG("PERM saved: deny %s / %s", permission.c_str(), pattern.c_str());
        }
        // Persist to disk
        if (remember) {
            wchar_t permPath[MAX_PATH];
            GetModuleFileNameW(nullptr, permPath, MAX_PATH);
            wchar_t* s2 = wcsrchr(permPath, L'\\');
            if (s2) *(s2 + 1) = 0;
            wcscat(permPath, L"permissions.txt");
            g_permission.saveToFile(ws_to_utf8(permPath));
        }

        return approved;
    };
    ToolResult result = g_toolRegistry.execute(tc.tool, args, ctx);
    std::string out = result.output;
    if (result.isError) {
        if (out.rfind("错误", 0) != 0 && out.rfind("Error", 0) != 0 && out.rfind("安全", 0) != 0) {
            out = result.title + ": " + out;
        }
    }
    return out;
}

// === Tool impls (dead code, kept for reference — now handled by registerAllTools) ===
static std::string executeTool_old(const ToolCall& tc) {
    if (tc.tool == "READ_FILE") {
        std::string pathErr = checkPath(tc.path, "读取文件");
        if (!pathErr.empty()) return pathErr;
        std::wstring wpath = utf8_to_ws(tc.path);
        HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND) return "错误：文件不存在 - " + tc.path;
            if (err == ERROR_ACCESS_DENIED) return "错误：拒绝访问 - " + tc.path;
            return "错误：无法打开文件 (code=" + std::to_string(err) + ") - " + tc.path;
        }
        DWORD size = GetFileSize(h, nullptr);
        if (size > 1048576) { CloseHandle(h); return "错误：文件过大 (>1MB)"; }
        std::string content(size + 1, 0);
        DWORD read = 0;
        ReadFile(h, &content[0], size, &read, nullptr);
        CloseHandle(h);
        content.resize(read);
        // Truncate for very long files
        if (content.length() > 60000) {
            content = content.substr(0, 60000) + "\n\n... (文件过长，已截断)";
        }
        return "文件内容 (" + tc.path + "):\n" + content;
    }

    if (tc.tool == "COPY_FILE") {
        std::string src = tc.source.empty() ? tc.path : tc.source;
        std::string dst = tc.name.empty() ? tc.content : tc.name;
        if (src.empty() || dst.empty()) return "错误：请指定源文件路径(source)和目标路径(destination)";
        std::string srcErr = checkPath(src, "复制文件(源)");
        if (!srcErr.empty()) return srcErr;
        std::string dstErr = checkPath(dst, "复制文件(目标)");
        if (!dstErr.empty()) return dstErr;
        std::wstring wsrc = utf8_to_ws(src);
        if (dst.find(':') == std::string::npos) {
            // destination is a directory or relative, append filename
            size_t lastSlash = src.find_last_of("\\/");
            if (lastSlash != std::string::npos) dst = dst + "\\" + src.substr(lastSlash + 1);
        }
        std::wstring wdst = utf8_to_ws(dst);
        if (CopyFileW(wsrc.c_str(), wdst.c_str(), FALSE))
            return "文件已复制: " + src + " -> " + dst;
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) return "错误：源文件不存在 - " + src;
        if (err == ERROR_FILE_EXISTS) return "错误：目标文件已存在 - " + dst;
        return "错误：复制失败 (code=" + std::to_string(err) + ")";
    }

    if (tc.tool == "DELETE_FILE") {
        if (tc.path.empty()) return "错误：请指定文件路径(path)";
        std::string pathErr = checkPath(tc.path, "删除文件");
        if (!pathErr.empty()) return pathErr;
        std::wstring wpath = utf8_to_ws(tc.path);
        DWORD attrs = GetFileAttributesW(wpath.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) return "错误：文件不存在 - " + tc.path;
        bool isDir = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
        BOOL ok;
        if (isDir) {
            // Use SHFileOperation for directory deletion (to recycle bin)
            ok = RemoveDirectoryW(wpath.c_str());
        } else {
            ok = DeleteFileW(wpath.c_str());
        }
        if (ok) return "已删除: " + tc.path + (isDir ? " (目录)" : "");
        DWORD err = GetLastError();
        return "错误：删除失败 (code=" + std::to_string(err) + ") - " + tc.path;
    }

    if (tc.tool == "MOVE_FILE") {
        std::string src = tc.source.empty() ? tc.path : tc.source;
        std::string dst = tc.name.empty() ? tc.content : tc.name;
        if (src.empty() || dst.empty()) return "错误：请指定源文件路径(source)和目标路径(destination)";
        std::string srcErr = checkPath(src, "移动文件(源)");
        if (!srcErr.empty()) return srcErr;
        std::string dstErr = checkPath(dst, "移动文件(目标)");
        if (!dstErr.empty()) return dstErr;
        std::wstring wsrc = utf8_to_ws(src);
        if (dst.find(':') == std::string::npos) {
            size_t lastSlash = src.find_last_of("\\/");
            if (lastSlash != std::string::npos) dst = dst + "\\" + src.substr(lastSlash + 1);
        }
        std::wstring wdst = utf8_to_ws(dst);
        if (MoveFileW(wsrc.c_str(), wdst.c_str()))
            return "文件已移动: " + src + " -> " + dst;
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) return "错误：源文件不存在 - " + src;
        if (err == ERROR_FILE_EXISTS) return "错误：目标文件已存在 - " + dst;
        return "错误：移动失败 (code=" + std::to_string(err) + ")";
    }

    if (tc.tool == "LIST_DIR") {
        std::string pathErr = checkPath(tc.path, "列出目录");
        if (!pathErr.empty()) return pathErr;
        std::wstring wpath = utf8_to_ws(tc.path);
        if (wpath.back() != L'\\') wpath += L'\\';
        std::wstring pattern = wpath + L"*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) {
            return "错误：无法访问目录 - " + tc.path;
        }
        std::string result = "目录内容 (" + tc.path + "):\n";
        int count = 0;
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            std::string name = ws_to_utf8(fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) name += "/";
            result += "  " + name + "\n";
            count++;
            if (count >= 200) { result += "  ... (超过200项，已截断)\n"; break; }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
        return result.empty() ? "目录为空" : result;
    }

    if (tc.tool == "WRITE_FILE") {
        std::string pathErr = checkPath(tc.path, "写入文件");
        if (!pathErr.empty()) return pathErr;
        std::wstring wpath = utf8_to_ws(tc.path);
        // Create directory if needed
        size_t lastSlash = tc.path.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            std::wstring dir = utf8_to_ws(tc.path.substr(0, lastSlash));
            // Try to create directory (ignore error - might already exist)
            CreateDirectoryW(dir.c_str(), nullptr);
        }
        HANDLE h = CreateFileW(wpath.c_str(), GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            return "错误：无法写入文件 - " + tc.path;
        }
        DWORD written = 0;
        WriteFile(h, tc.content.data(), (DWORD)tc.content.size(), &written, nullptr);
        CloseHandle(h);
        return "文件已写入: " + tc.path + " (" + std::to_string(written) + " 字节)";
    }

    if (tc.tool == "LIST_APPS") {
        std::string result;
        int count = 0;
        // Scan both user and all-users Start Menu
        wchar_t userStart[MAX_PATH], commonStart[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, userStart))) {
            result += "=== 用户开始菜单 ===\n";
            scanStartMenu(userStart, result, count);
        }
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, commonStart))) {
            result += "=== 所有用户开始菜单 ===\n";
            scanStartMenu(commonStart, result, count);
        }
        if (result.empty()) result = "未找到已安装的应用程序";
        return result;
    }

    if (tc.tool == "OPEN_APP") {
        if (tc.name.empty()) return "错误：请指定应用程序名称（name=应用名）";
        std::wstring target = findAppByName(tc.name);
        if (target.empty()) return "未找到应用程序: " + tc.name;
        // Launch the app
        HINSTANCE hi = ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)hi <= 32) return "无法启动应用: " + tc.name + " (错误码: " + std::to_string((INT_PTR)hi) + ")";
        return "已启动: " + tc.name + "\n路径: " + ws_to_utf8(target);
    }

    if (tc.tool == "UNINSTALL_APP") {
        if (tc.name.empty()) return "错误：请指定应用程序名称（name=应用名）";
        std::string uninst = findUninstallString(tc.name);
        if (uninst.empty()) return "未找到卸载程序: " + tc.name;
        // Execute uninstall command
        std::wstring wcmd = utf8_to_ws(uninst);
        STARTUPINFOW si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        if (CreateProcessW(nullptr, &wcmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return "已启动卸载程序: " + tc.name;
        }
        // Try ShellExecute as fallback
        HINSTANCE hi = ShellExecuteW(nullptr, L"open", wcmd.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)hi > 32) return "已启动卸载程序: " + tc.name;
        return "无法启动卸载: " + tc.name;
    }

    if (tc.tool == "OPEN_APP_LOCATION") {
        if (tc.name.empty()) return "错误：请指定应用程序名称（name=应用名）";
        std::wstring target = findAppByName(tc.name);
        if (target.empty()) return "未找到应用程序: " + tc.name;
        // Open Explorer at the file's directory
        std::wstring cmd = L"/select,\"" + target + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", cmd.c_str(), nullptr, SW_SHOWNORMAL);
        return "已打开文件位置: " + tc.name + "\n路径: " + ws_to_utf8(target);
    }

    if (tc.tool == "LIST_PROCESSES") {
        return listProcesses(tc.name);
    }

    if (tc.tool == "KILL_PROCESS") {
        if (tc.name.empty()) return "错误：请指定进程名称（name=进程名）";
        // Find PID by name
        DWORD pid = findProcessByName(tc.name);
        if (pid == 0) return "未找到进程: " + tc.name;
        // Push confirmation to UI
        {
            std::lock_guard<std::mutex> lk(g_streamMutex);
            std::wstring msg = L"__CONFIRM_KILL__" + utf8_to_ws(tc.name) + L"|" + std::to_wstring(pid);
            g_streamQueue.push(msg);
            PostMessageW(g_hwnd, WM_AI_DELTA, 0, 0);
        }
        // Wait for user confirmation
        {
            std::unique_lock<std::mutex> lk(g_psMutex);
            g_psReady = false;
            g_psCv.wait(lk, []{ return g_psReady; });
        }
        if (!g_psApproved) return "用户取消了终止进程操作";
        // Terminate
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (!hProc) return "无法打开进程 (PID=" + std::to_string(pid) + ", code=" + std::to_string(GetLastError()) + ")";
        BOOL terminated = TerminateProcess(hProc, 1);
        CloseHandle(hProc);
        if (terminated) return "已终止进程: " + tc.name + " (PID=" + std::to_string(pid) + ")";
        return "终止进程失败: " + tc.name;
    }

    if (tc.tool == "RUN_PS") {
        if (tc.content.empty()) return "错误：请指定 PowerShell 命令（content=命令）";
        // Push confirmation to UI
        {
            std::lock_guard<std::mutex> lk(g_streamMutex);
            g_streamQueue.push(L"__CONFIRM_PS__" + utf8_to_ws(tc.content));
            PostMessageW(g_hwnd, WM_AI_DELTA, 0, 0);
        }
        // Wait for user confirmation (agent thread blocks here)
        {
            std::unique_lock<std::mutex> lk(g_psMutex);
            g_psReady = false;
            g_psCv.wait(lk, []{ return g_psReady; });
        }
        if (!g_psApproved) return "用户取消了 PowerShell 执行";
        return executePowerShell(tc.content);
    }

    if (tc.tool == "GET_FOREGROUND_WINDOW") {
        return getForegroundWindowInfo();
    }

    if (tc.tool == "LIST_WINDOWS") {
        return listWindows();
    }

    if (tc.tool == "CAPTURE_WINDOW") {
        if (tc.name.empty()) return "错误：请指定窗口标题（name=窗口标题，支持部分匹配）";
        std::string b64 = captureWindowToBase64(tc.name);
        if (b64.empty()) return "错误：未找到包含 '" + tc.name + "' 的可见窗口，请先用 LIST_WINDOWS 查看所有窗口";
        {
            std::lock_guard<std::mutex> lk(g_screenshotMutex);
            g_pendingScreenshotB64 = b64;
        }
        {
            std::lock_guard<std::mutex> lk(g_streamMutex);
            g_streamQueue.push(L"__IMAGE__" + utf8_to_ws(tc.name) + L"\x1f" + utf8_to_ws(b64));
            PostMessageW(g_hwnd, WM_AI_DELTA, 0, 0);
        }
        return "窗口截图已捕获: " + tc.name + " (PNG base64, " + std::to_string(b64.length()) + " 字符)\n图片将在下一轮分析中发送给视觉模型。";
    }

    return "未知工具: " + tc.tool;
}

std::string listProcesses(const std::string& filter) {
    std::string result;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return "错误：无法枚举进程";
    PROCESSENTRY32W pe = { sizeof(pe) };
    int count = 0;
    std::string lowerFilter = filter;
    for (auto& c : lowerFilter) c = (char)tolower((unsigned char)c);
    if (Process32FirstW(hSnap, &pe)) {
        result += "PID\t线程数\t进程名\n";
        result += "---\t---\t---\n";
        do {
            std::string pname = ws_to_utf8(pe.szExeFile);
            if (!filter.empty()) {
                std::string lp = pname;
                for (auto& c : lp) c = (char)tolower((unsigned char)c);
                if (lp.find(lowerFilter) == std::string::npos) continue;
            }
            result += std::to_string(pe.th32ProcessID) + "\t"
                   + std::to_string(pe.cntThreads) + "\t"
                   + pname + "\n";
            if (++count >= 200) { result += "...(超过200个进程，已截断)\n"; break; }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    if (count == 0) return filter.empty() ? "未找到进程" : "未找到匹配 '" + filter + "' 的进程";
    return result;
}

std::string getForegroundWindowInfo() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return "无法获取前台窗口";
    wchar_t title[512] = {};
    GetWindowTextW(hwnd, title, 512);
    wchar_t className[256] = {};
    GetClassNameW(hwnd, className, 256);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    RECT rect;
    GetWindowRect(hwnd, &rect);
    std::string info;
    info += "窗口标题: " + ws_to_utf8(title) + "\n";
    info += "窗口类名: " + ws_to_utf8(className) + "\n";
    info += "所属进程: " + resolvePidToName(pid) + " (PID=" + std::to_string(pid) + ")\n";
    info += "窗口位置: left=" + std::to_string(rect.left) + " top=" + std::to_string(rect.top)
         + " width=" + std::to_string(rect.right - rect.left)
         + " height=" + std::to_string(rect.bottom - rect.top);
    return info;
}

std::string resolvePidToName(DWORD pid) {
    if (!pid) return "(未知)";
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return "(未知)";
    std::string name = "(未知)";
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                name = ws_to_utf8(pe.szExeFile);
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return name;
}

std::string listWindows() {
    struct Ctx {
        std::string* out;
        int count;
    } ctx = {nullptr, 0};
    std::string result;
    ctx.out = &result;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* ctx = (Ctx*)lParam;
        if (ctx->count >= 300) return TRUE;
        if (!IsWindowVisible(hwnd)) return TRUE;
        wchar_t title[512] = {};
        GetWindowTextW(hwnd, title, 512);
        if (title[0] == 0) return TRUE; // skip empty titles
        wchar_t className[256] = {};
        GetClassNameW(hwnd, className, 256);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        std::string procName = resolvePidToName(pid);

        char buf[1024];
        snprintf(buf, sizeof(buf), "%-45s | %-25s | %-30s | PID=%lu\n",
                 std::string(ws_to_utf8(title)).substr(0, 43).c_str(),
                 std::string(ws_to_utf8(className)).substr(0, 23).c_str(),
                 procName.substr(0, 28).c_str(),
                 pid);
        ctx->out->append(buf);
        ctx->count++;
        return TRUE;
    }, (LPARAM)&ctx);

    if (result.empty()) return "未找到可见窗口";
    std::string header = "窗口标题                                          | 类名                      | 进程                           | PID\n";
    header +=        "--------------------------------------------------+---------------------------+--------------------------------+-------\n";
    return header + result;
}

DWORD findProcessByName(const std::string& name) {
    std::string lower = name;
    for (auto& c : lower) c = (char)tolower((unsigned char)c);
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            std::string pname = ws_to_utf8(pe.szExeFile);
            std::string lp = pname;
            for (auto& c : lp) c = (char)tolower((unsigned char)c);
            if (lp.find(lower) != std::string::npos) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return pid;
}

// ------------------------------------------------------------------
// GDI+ helpers for window screenshot → PNG → base64
// ------------------------------------------------------------------

// Forward-declare the 2-arg base64Encode (defined later) to avoid
// ambiguity with aries::base64Encode(const std::string&) brought in
// by using namespace aries.
std::string base64Encode(const unsigned char* data, size_t len);

static void ensureGdiplus() {
    static ULONG_PTR token = 0;
    static bool init = false;
    if (!init) {
        Gdiplus::GdiplusStartupInput inp;
        Gdiplus::GdiplusStartup(&token, &inp, NULL);
        init = true;
    }
}

static bool getPngEncoderClsid(CLSID& clsid) {
    static CLSID cached = {};
    static bool found = false;
    if (found) { clsid = cached; return true; }
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (!size) return false;
    auto* info = (Gdiplus::ImageCodecInfo*)malloc(size);
    if (!info) return false;
    Gdiplus::GetImageEncoders(num, size, info);
    for (UINT i = 0; i < num; i++) {
        if (wcscmp(info[i].MimeType, L"image/png") == 0) {
            cached = info[i].Clsid;
            found = true;
            break;
        }
    }
    free(info);
    clsid = cached;
    return found;
}

std::string hbmpToPngBase64(HBITMAP hBitmap) {
    ensureGdiplus();
    CLSID pngClsid;
    if (!getPngEncoderClsid(pngClsid)) return "";
    Gdiplus::Bitmap bitmap(hBitmap, NULL);
    if (bitmap.GetLastStatus() != Gdiplus::Ok) return "";
    IStream* pStream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &pStream))) return "";
    if (bitmap.Save(pStream, &pngClsid, NULL) != Gdiplus::Ok) {
        pStream->Release();
        return "";
    }
    STATSTG stat;
    if (FAILED(pStream->Stat(&stat, STATFLAG_NONAME))) { pStream->Release(); return ""; }
    ULONG size = stat.cbSize.QuadPart;
    LARGE_INTEGER li = {};
    pStream->Seek(li, STREAM_SEEK_SET, NULL);
    std::string pngBytes(size, 0);
    ULONG bytesRead = 0;
    pStream->Read(&pngBytes[0], size, &bytesRead);
    pStream->Release();
    if (bytesRead != size) return "";
    return base64Encode((const unsigned char*)pngBytes.data(), pngBytes.size());
}

struct FindWndCtx { std::string search; HWND hwnd; };

static BOOL CALLBACK findWndCb(HWND hwnd, LPARAM lParam) {
    auto* ctx = (FindWndCtx*)lParam;
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t title[512] = {};
    GetWindowTextW(hwnd, title, 512);
    if (title[0] == 0) return TRUE;
    std::string wTitle = ws_to_utf8(title);
    // Case-insensitive partial match
    for (auto& c : wTitle) c = (char)tolower((unsigned char)c);
    std::string q = ctx->search;
    for (auto& c : q) c = (char)tolower((unsigned char)c);
    if (wTitle.find(q) != std::string::npos) {
        ctx->hwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

std::string captureWindowToBase64(const std::string& windowTitle) {
    FindWndCtx ctx = { windowTitle, nullptr };
    EnumWindows(findWndCb, (LPARAM)&ctx);
    if (!ctx.hwnd) return "";

    HWND hwnd = ctx.hwnd;
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);

    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 1) w = 1; if (h < 1) h = 1;

    const int MAX_W = 1024;
    float scale = 1.0f;
    int outW = w, outH = h;
    if (outW > MAX_W) { scale = (float)MAX_W / outW; outW = MAX_W; outH = (int)(h * scale); }
    // Always scale down to keep API payload small for multimodal
    else { scale = 1.0f; }

    HDC hScrDC = GetDC(NULL);
    HDC hCapDC = CreateCompatibleDC(hScrDC);
    HBITMAP hCapBmp = CreateCompatibleBitmap(hScrDC, w, h);
    HBITMAP hOldCap = (HBITMAP)SelectObject(hCapDC, hCapBmp);

    // Strategy 1: PrintWindow (works in background for traditional Win32 apps)
    BOOL pwOk = PrintWindow(hwnd, hCapDC, PW_CLIENTONLY);

    if (!pwOk) {
        // Strategy 2: Flash topmost → BitBlt → restore
        // Works for hardware-accelerated windows (Chrome, Edge, etc.)
        LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        Sleep(50);
        BitBlt(hCapDC, 0, 0, w, h, hScrDC, rc.left, rc.top, SRCCOPY);
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        // Restore original exstyle if needed
        if (!(ex & WS_EX_TOPMOST)) {
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
        }
    }

    // Scale to output size
    HBITMAP hFinal;
    if (scale < 1.0f) {
        hFinal = CreateCompatibleBitmap(hScrDC, outW, outH);
        HDC hScaleDC = CreateCompatibleDC(hScrDC);
        HBITMAP hOldS = (HBITMAP)SelectObject(hScaleDC, hFinal);
        SetStretchBltMode(hScaleDC, HALFTONE);
        StretchBlt(hScaleDC, 0, 0, outW, outH, hCapDC, 0, 0, w, h, SRCCOPY);
        SelectObject(hScaleDC, hOldS);
        DeleteDC(hScaleDC);
    } else {
        hFinal = hCapBmp;
    }

    std::string result = hbmpToPngBase64(hFinal);

    if (scale < 1.0f) DeleteObject(hFinal);
    SelectObject(hCapDC, hOldCap);
    DeleteObject(hCapBmp);
    DeleteDC(hCapDC);
    ReleaseDC(NULL, hScrDC);
    return result;
}

std::string base64Encode(const unsigned char* data, size_t len) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned n = (unsigned)data[i] << 16;
        if (i + 1 < len) n |= (unsigned)data[i + 1] << 8;
        if (i + 2 < len) n |= (unsigned)data[i + 2];
        out += tbl[(n >> 18) & 0x3F];
        out += tbl[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? tbl[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? tbl[n & 0x3F] : '=';
    }
    return out;
}

std::string executePowerShell(const std::string& script) {
    // Build script with encoding fix
    std::string fullScript = "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8\n" + script;

    // Convert to UTF-16LE for PowerShell -EncodedCommand
    int wlen = MultiByteToWideChar(CP_UTF8, 0, fullScript.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> wbuf(wlen);
    MultiByteToWideChar(CP_UTF8, 0, fullScript.c_str(), -1, wbuf.data(), wlen);
    std::string b64 = base64Encode((const unsigned char*)wbuf.data(), (wlen - 1) * sizeof(wchar_t));

    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    std::wstring cmdLine = L"powershell.exe -NoProfile -EncodedCommand " + utf8_to_ws(b64);
    std::vector<wchar_t> cmdBuf(cmdLine.size() + 1);
    wcscpy(cmdBuf.data(), cmdLine.c_str());

    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);

    if (!ok) {
        CloseHandle(hRead);
        return "错误：无法启动 PowerShell (code=" + std::to_string(GetLastError()) + ")";
    }

    // Read output with 30s timeout
    std::string output;
    char buf[4096];
    DWORD startTick = GetTickCount();
    while (true) {
        DWORD avail = 0;
        PeekNamedPipe(hRead, nullptr, 0, nullptr, &avail, nullptr);
        if (avail > 0) {
            DWORD rd = 0;
            ReadFile(hRead, buf, (std::min)(sizeof(buf) - 1, (size_t)avail), &rd, nullptr);
            if (rd > 0) { buf[rd] = 0; output += buf; }
        }
        DWORD exitCode = STILL_ACTIVE;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        if (exitCode != STILL_ACTIVE && avail == 0) break;
        if (GetTickCount() - startTick > 30000) {
            TerminateProcess(pi.hProcess, 1);
            output += "\n[超时，已终止]";
            break;
        }
        Sleep(50);
    }

    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (output.empty()) output = "(无输出)";
    if (output.length() > 10000) output = output.substr(0, 10000) + "\n\n...(输出过长，已截断)";
    return output;
}

// ===========================================================================
// Tool Registry — registered after all helper functions are defined.
// Called lazily by getToolsJson() on first use.
// ===========================================================================
static void registerAllTools() {
    using P = ParamType;

    // -- READ_FILE --
    g_toolRegistry.registerTool(
        {"READ_FILE", "Read the contents of a file at the given Windows absolute path.",
         {{"path", P::String, "Full Windows absolute path to the file", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string path = jsonGetString(argsJson, "path");
            std::string pathErr = checkPath(path, "读取文件");
            if (!pathErr.empty()) return ToolResult::error("安全限制", pathErr);
            std::wstring wpath = utf8_to_ws(path);
            HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE) {
                DWORD err = GetLastError();
                if (err == ERROR_FILE_NOT_FOUND) return ToolResult::error("文件不存在", path);
                if (err == ERROR_ACCESS_DENIED) return ToolResult::error("拒绝访问", path);
                return ToolResult::error("打开失败", "code=" + std::to_string(err) + " - " + path);
            }
            DWORD size = GetFileSize(h, nullptr);
            if (size > 1048576) { CloseHandle(h); return ToolResult::error("文件过大", ">1MB"); }
            std::string content(size + 1, 0);
            DWORD read = 0;
            ReadFile(h, &content[0], size, &read, nullptr);
            CloseHandle(h);
            content.resize(read);
            if (content.length() > 60000)
                content = content.substr(0, 60000) + "\n\n... (文件过长，已截断)";
            return ToolResult::ok(path, "文件内容 (" + path + "):\n" + content)
                .withMeta("path", path).withMeta("bytes", std::to_string(read));
        }
    );

    // -- WRITE_FILE --
    g_toolRegistry.registerTool(
        {"WRITE_FILE", "Write content to a file at the given Windows absolute path.",
         {{"path", P::String, "Full Windows absolute path for the file", true},
          {"content", P::String, "Content to write", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string path = jsonGetString(argsJson, "path");
            std::string content = jsonGetString(argsJson, "content");
            std::string pathErr = checkPath(path, "写入文件");
            if (!pathErr.empty()) return ToolResult::error("安全限制", pathErr);
            size_t ls = path.find_last_of("\\/");
            if (ls != std::string::npos)
                CreateDirectoryW(utf8_to_ws(path.substr(0, ls)).c_str(), nullptr);
            std::wstring wpath = utf8_to_ws(path);
            HANDLE h = CreateFileW(wpath.c_str(), GENERIC_WRITE, 0,
                                   nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE)
                return ToolResult::error("写入失败", path);
            DWORD written = 0;
            WriteFile(h, content.data(), (DWORD)content.size(), &written, nullptr);
            CloseHandle(h);
            return ToolResult::ok(path, "文件已写入: " + path + " (" + std::to_string(written) + " 字节)")
                .withMeta("path", path).withMeta("bytes", std::to_string(written));
        }
    );

    // -- COPY_FILE --
    g_toolRegistry.registerTool(
        {"COPY_FILE", "Copy a file from source to destination.",
         {{"source", P::String, "Source file path", true},
          {"destination", P::String, "Destination path", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string src = jsonGetString(argsJson, "source");
            std::string dst = jsonGetString(argsJson, "destination");
            if (src.empty() || dst.empty())
                return ToolResult::error("参数错误", "需要 source 和 destination");
            std::string se = checkPath(src, "复制文件(源)");
            if (!se.empty()) return ToolResult::error("安全限制", se);
            std::string de = checkPath(dst, "复制文件(目标)");
            if (!de.empty()) return ToolResult::error("安全限制", de);
            if (dst.find(':') == std::string::npos) {
                size_t ls = src.find_last_of("\\/");
                if (ls != std::string::npos) dst = dst + "\\" + src.substr(ls + 1);
            }
            if (CopyFileW(utf8_to_ws(src).c_str(), utf8_to_ws(dst).c_str(), FALSE))
                return ToolResult::ok("复制完成", "文件已复制: " + src + " -> " + dst);
            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND) return ToolResult::error("源文件不存在", src);
            if (err == ERROR_FILE_EXISTS) return ToolResult::error("目标已存在", dst);
            return ToolResult::error("复制失败", "code=" + std::to_string(err));
        }
    );

    // -- DELETE_FILE --
    g_toolRegistry.registerTool(
        {"DELETE_FILE", "Delete a file or empty directory at the given path.",
         {{"path", P::String, "Full Windows absolute path to delete", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string path = jsonGetString(argsJson, "path");
            std::string pathErr = checkPath(path, "删除文件");
            if (!pathErr.empty()) return ToolResult::error("安全限制", pathErr);
            std::wstring wpath = utf8_to_ws(path);
            DWORD attrs = GetFileAttributesW(wpath.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES)
                return ToolResult::error("文件不存在", path);
            bool isDir = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
            BOOL ok = isDir ? RemoveDirectoryW(wpath.c_str()) : DeleteFileW(wpath.c_str());
            if (ok)
                return ToolResult::ok("删除完成", "已删除: " + path + (isDir ? " (目录)" : ""));
            return ToolResult::error("删除失败", "code=" + std::to_string(GetLastError()));
        }
    );

    // -- MOVE_FILE --
    g_toolRegistry.registerTool(
        {"MOVE_FILE", "Move or rename a file from source to destination.",
         {{"source", P::String, "Source file path", true},
          {"destination", P::String, "Destination path", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string src = jsonGetString(argsJson, "source");
            std::string dst = jsonGetString(argsJson, "destination");
            if (src.empty() || dst.empty())
                return ToolResult::error("参数错误", "需要 source 和 destination");
            std::string se = checkPath(src, "移动文件(源)");
            if (!se.empty()) return ToolResult::error("安全限制", se);
            std::string de = checkPath(dst, "移动文件(目标)");
            if (!de.empty()) return ToolResult::error("安全限制", de);
            if (dst.find(':') == std::string::npos) {
                size_t ls = src.find_last_of("\\/");
                if (ls != std::string::npos) dst = dst + "\\" + src.substr(ls + 1);
            }
            if (MoveFileW(utf8_to_ws(src).c_str(), utf8_to_ws(dst).c_str()))
                return ToolResult::ok("移动完成", "文件已移动: " + src + " -> " + dst);
            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND) return ToolResult::error("源文件不存在", src);
            if (err == ERROR_FILE_EXISTS) return ToolResult::error("目标已存在", dst);
            return ToolResult::error("移动失败", "code=" + std::to_string(err));
        }
    );

    // -- LIST_DIR --
    g_toolRegistry.registerTool(
        {"LIST_DIR", "List files and subdirectories in a directory (up to 200 entries).",
         {{"path", P::String, "Full Windows absolute path to the directory", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string path = jsonGetString(argsJson, "path");
            std::string pathErr = checkPath(path, "列出目录");
            if (!pathErr.empty()) return ToolResult::error("安全限制", pathErr);
            std::wstring wpath = utf8_to_ws(path);
            if (wpath.back() != L'\\') wpath += L'\\';
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW((wpath + L"*").c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE)
                return ToolResult::error("无法访问", path);
            std::string result = "目录内容 (" + path + "):\n";
            int count = 0;
            do {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                std::string name = ws_to_utf8(fd.cFileName);
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) name += "/";
                result += "  " + name + "\n";
                if (++count >= 200) { result += "  ... (超过200项，已截断)\n"; break; }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
            if (count == 0) result = "目录为空";
            return ToolResult::ok(path, result).withMeta("path", path).withMeta("entries", std::to_string(count));
        }
    );

    // -- LIST_APPS --
    g_toolRegistry.registerTool(
        {"LIST_APPS", "List installed applications from the Start Menu (up to 500).", {}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string result;
            int count = 0;
            wchar_t buf[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, buf))) {
                result += "=== 用户开始菜单 ===\n";
                scanStartMenu(buf, result, count);
            }
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, buf))) {
                result += "=== 所有用户开始菜单 ===\n";
                scanStartMenu(buf, result, count);
            }
            if (result.empty()) result = "未找到已安装的应用程序";
            return ToolResult::ok("应用列表", result).withMeta("count", std::to_string(count));
        }
    );

    // -- OPEN_APP --
    g_toolRegistry.registerTool(
        {"OPEN_APP", "Launch an application by name.",
         {{"name", P::String, "Application name (e.g. Chrome, 记事本)", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string name = jsonGetString(argsJson, "name");
            std::wstring target = findAppByName(name);
            if (target.empty()) return ToolResult::error("未找到", name);
            HINSTANCE hi = ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            if ((INT_PTR)hi <= 32)
                return ToolResult::error("启动失败", "code=" + std::to_string((INT_PTR)hi));
            return ToolResult::ok(name, "已启动: " + name + "\n路径: " + ws_to_utf8(target));
        }
    );

    // -- UNINSTALL_APP --
    g_toolRegistry.registerTool(
        {"UNINSTALL_APP", "Uninstall an application by name via registry.",
         {{"name", P::String, "Application name", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string name = jsonGetString(argsJson, "name");
            std::string uninst = findUninstallString(name);
            if (uninst.empty()) return ToolResult::error("未找到", name);
            HINSTANCE hi = ShellExecuteW(nullptr, L"open", utf8_to_ws(uninst).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            if ((INT_PTR)hi <= 32)
                return ToolResult::error("卸载失败", uninst);
            return ToolResult::ok("卸载中", "已启动卸载: " + name + "\n命令: " + uninst);
        }
    );

    // -- OPEN_APP_LOCATION --
    g_toolRegistry.registerTool(
        {"OPEN_APP_LOCATION", "Open the folder containing an application in Explorer.",
         {{"name", P::String, "Application name", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string name = jsonGetString(argsJson, "name");
            std::wstring target = findAppByName(name);
            if (target.empty()) return ToolResult::error("未找到", name);
            std::wstring explore = L"/select,\"" + target + L"\"";
            ShellExecuteW(nullptr, L"open", L"explorer.exe", explore.c_str(), nullptr, SW_SHOWNORMAL);
            return ToolResult::ok(name, "已在资源管理器中打开: " + name);
        }
    );

    // -- RUN_PS --
    g_toolRegistry.registerTool(
        {"RUN_PS", "Execute a PowerShell command. Requires user confirmation.",
         {{"content", P::String, "PowerShell command to execute", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string cmd = jsonGetString(argsJson, "content");
            if (ctx.ask && !ctx.ask("execute", "powershell: " + cmd.substr(0, 120)))
                return ToolResult::error("用户拒绝", "PowerShell 执行被拒绝");
            std::string result = executePowerShell(cmd);
            bool isErr = result.rfind("错误", 0) == 0;
            return isErr ? ToolResult::error("执行失败", result)
                         : ToolResult::ok("PowerShell", result);
        }
    );

    // -- LIST_PROCESSES --
    g_toolRegistry.registerTool(
        {"LIST_PROCESSES", "List running processes, optionally filtered by name.",
         {{"name", P::String, "Optional process name filter", false}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string filter = jsonGetString(argsJson, "name");
            return ToolResult::ok("进程列表", listProcesses(filter));
        }
    );

    // -- KILL_PROCESS --
    g_toolRegistry.registerTool(
        {"KILL_PROCESS", "Terminate a process by name. Requires user confirmation.",
         {{"name", P::String, "Process name to terminate", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string name = jsonGetString(argsJson, "name");
            DWORD pid = findProcessByName(name);
            if (pid == 0 && name.find('.') == std::string::npos)
                pid = findProcessByName(name + ".exe");
            if (pid == 0)
                return ToolResult::error("未找到进程", name);
            if (ctx.ask && !ctx.ask("execute", "kill: " + name + " (PID=" + std::to_string(pid) + ")"))
                return ToolResult::error("用户拒绝", "进程终止被拒绝");
            HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (!hProc) return ToolResult::error("打开进程失败", "PID=" + std::to_string(pid));
            BOOL term = TerminateProcess(hProc, 1);
            CloseHandle(hProc);
            if (term) return ToolResult::ok("已终止", "进程已终止: " + name + " (PID=" + std::to_string(pid) + ")");
            return ToolResult::error("终止失败", name);
        }
    );

    // -- GET_FOREGROUND_WINDOW --
    g_toolRegistry.registerTool(
        {"GET_FOREGROUND_WINDOW", "Get the current foreground window info.", {}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            return ToolResult::ok("前台窗口", getForegroundWindowInfo());
        }
    );

    // -- LIST_WINDOWS --
    g_toolRegistry.registerTool(
        {"LIST_WINDOWS", "List all visible desktop windows.", {}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            return ToolResult::ok("窗口列表", listWindows());
        }
    );

    // -- CAPTURE_WINDOW --
    g_toolRegistry.registerTool(
        {"CAPTURE_WINDOW", "Capture a screenshot of a window by partial title match.",
         {{"name", P::String, "Window title (partial match)", true}}},
        [](const std::string& argsJson, ToolContext& ctx) -> ToolResult {
            std::string name = jsonGetString(argsJson, "name");
            std::string b64 = captureWindowToBase64(name);
            if (b64.empty())
                return ToolResult::error("截图失败", "未找到包含 '" + name + "' 的可见窗口");
            {
                std::lock_guard<std::mutex> lk(g_screenshotMutex);
                g_pendingScreenshotB64 = b64;
            }
            {
                std::lock_guard<std::mutex> lk(g_streamMutex);
                g_streamQueue.push(L"__IMAGE__" + utf8_to_ws(name) + L"\x1f" + utf8_to_ws(b64));
                PostMessageW(g_hwnd, WM_AI_DELTA, 0, 0);
            }
            return ToolResult::ok("截图完成", "窗口截图已捕获: " + name + " (" + std::to_string(b64.length()) + " 字符)");
        }
    );

    LOG("ToolRegistry: %zu tools registered", g_toolRegistry.count());
}

// Navigation completed handler
class NavCompletedHandler : public ICoreWebView2NavigationCompletedEventHandler {
    LONG m_ref = 1;
public:
    ULONG STDMETHODCALLTYPE AddRef() override  { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualGUID(riid, IID_ICoreWebView2NavigationCompletedEventHandler) ||
            IsEqualGUID(riid, IID_IUnknown)) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) override {
        BOOL ok; args->get_IsSuccess(&ok);
        LOG("NavComplete: %d", ok);
        if (ok) {
            EventRegistrationToken t;
            sender->add_WebMessageReceived(new WebMsgHandler(), &t);
            LOG("WebMessage handler registered");
            loadHistoryAndPush();
            // Init permission system
            {
                wchar_t permPath[MAX_PATH];
                GetModuleFileNameW(nullptr, permPath, MAX_PATH);
                wchar_t* s = wcsrchr(permPath, L'\\');
                if (s) *(s + 1) = 0;
                wcscat(permPath, L"permissions.txt");
                g_permission.loadFromFile(ws_to_utf8(permPath));
                // Set defaults (lower priority — user rules override)
                g_permission.setRuleset("chat-defaults", chatAgentDefaults());
                g_permission.setRuleset("agent-defaults", agentModeDefaults());
                LOG("Permission system loaded: %zu rulesets",
                    g_permission.rulesets().size());
            }
            // Init MCP servers in background (don't block UI)
            std::thread([]() {
                CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                wchar_t exeDir[MAX_PATH];
                GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
                wchar_t* sd = wcsrchr(exeDir, L'\\');
                if (sd) *(sd + 1) = 0;

                std::wstring cfgPath = std::wstring(exeDir) + L"mcp_servers.json";
                int n = g_mcpRegistry.loadFromJsonFile(cfgPath, g_toolRegistry);
                LOG("MCP: loaded %d tools from config", n);

                // Fallback: if no config, auto-discover bundled server
                if (n == 0) {
                    std::wstring mcpPath = std::wstring(exeDir) + L"demo\\mcp_server.exe";
                    if (GetFileAttributesW(mcpPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        n = g_mcpRegistry.addServer(mcpPath, "desktop", g_toolRegistry);
                        LOG("MCP auto-discover 'desktop': %d tools", n);
                    }
                }

                LOG("MCP: %zu servers, %zu tools total",
                    g_mcpRegistry.serverCount(), g_toolRegistry.count());
                CoUninitialize();
            }).detach();
            // First launch: auto-open settings if no valid config
            if (!g_config.valid) {
                LOG("First launch detected — opening settings");
                execJsRaw(L"setTimeout(function(){if(window.openSettings)window.openSettings()},500)");
            }
        }
        return S_OK;
    }
};

// === Combined init handler (matches webview.h pattern) ===
class WebView2Handler
    : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
      public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    LONG m_ref = 1;
    HWND m_hwnd;
    std::atomic_flag* m_flag;
    unsigned int m_maxAttempts = 5;
    unsigned int m_attempts = 0;
    std::function<HRESULT()> m_attemptFn;

public:
    WebView2Handler(HWND h, std::atomic_flag* f) : m_hwnd(h), m_flag(f) {}
    void setAttemptFn(std::function<HRESULT()> fn) { m_attemptFn = fn; }

    ULONG STDMETHODCALLTYPE AddRef() override  { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualGUID(riid, IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) ||
            IsEqualGUID(riid, IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) ||
            IsEqualGUID(riid, IID_IUnknown)) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* env) override {
        if (SUCCEEDED(result)) {
            result = env->CreateCoreWebView2Controller(m_hwnd, this);
            if (SUCCEEDED(result)) return S_OK;
        }
        tryCreate();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* controller) override {
        if (FAILED(result)) {
            if (result == HRESULT_FROM_WIN32(ERROR_INVALID_STATE) || result == E_ABORT)
                return S_OK;
            tryCreate();
            return S_OK;
        }

        g_controller = controller; controller->AddRef();
        g_controller->get_CoreWebView2(&g_webview); g_webview->AddRef();

        EventRegistrationToken t;
        g_webview->add_NavigationCompleted(new NavCompletedHandler(), &t);

        g_controller->put_IsVisible(TRUE);
        RECT bounds; GetClientRect(m_hwnd, &bounds);
        g_controller->put_Bounds(bounds);

        // Subclass all WebView2 children for WM_NCHITTEST forwarding
        HWND child = FindWindowExW(m_hwnd, nullptr, nullptr, nullptr);
        int scid = 1;
        while (child) {
            SetWindowSubclass(child, ChildSubclassProc, scid++, 0);
            child = FindWindowExW(m_hwnd, child, nullptr, nullptr);
        }

        // Navigate to embedded HTML (UTF-8 -> wide string)
        g_webview->NavigateToString(utf8_to_ws(g_htmlUI).c_str());
        m_flag->clear();
        return S_OK;
    }

    void tryCreate() {
        if (m_attempts >= m_maxAttempts) { m_flag->clear(); return; }
        m_attempts++;
        HRESULT hr = m_attemptFn();
        if (SUCCEEDED(hr)) return;
        if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_STATE)) return;
        tryCreate();
    }
};

LRESULT CALLBACK ChildSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_NCHITTEST) return SendMessageW(g_hwnd, msg, wp, lp);
    return DefSubclassProc(hwnd, msg, wp, lp);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wp == TRUE) return 0;
        break;

    case WM_NCHITTEST: {
        POINT pt = { LOWORD(lp), HIWORD(lp) };
        RECT rc; GetClientRect(hwnd, &rc);
        POINT tl = { rc.left, rc.top }, br = { rc.right, rc.bottom };
        ClientToScreen(hwnd, &tl); ClientToScreen(hwnd, &br);
        int x = pt.x, y = pt.y, L = tl.x, T = tl.y, R = br.x, B = br.y;
        if (x < L + BORDER_WIDTH && y < T + BORDER_WIDTH)    return HTTOPLEFT;
        if (x > R - BORDER_WIDTH && y < T + BORDER_WIDTH)    return HTTOPRIGHT;
        if (x < L + BORDER_WIDTH && y > B - BORDER_WIDTH)    return HTBOTTOMLEFT;
        if (x > R - BORDER_WIDTH && y > B - BORDER_WIDTH)    return HTBOTTOMRIGHT;
        if (x < L + BORDER_WIDTH)  return HTLEFT;
        if (x > R - BORDER_WIDTH)  return HTRIGHT;
        if (y < T + BORDER_WIDTH)  return HTTOP;
        if (y > B - BORDER_WIDTH)  return HTBOTTOM;
        if (y < T + 48) return HTCAPTION;
        return HTCLIENT;
    }

    case WM_AI_DELTA: {
        // Process ONE item from stream queue per message to allow browser repaints between deltas
        std::wstring delta;
        bool hasMore = false;
        bool done = false;
        {
            std::lock_guard<std::mutex> lk(g_streamMutex);
            if (!g_streamQueue.empty()) {
                delta = std::move(g_streamQueue.front());
                g_streamQueue.pop();
                hasMore = !g_streamQueue.empty();
            }
            done = g_streamDone;
        }

        if (!delta.empty()) {
            // Escape for JS
            auto escapeJs = [](const std::wstring& s) -> std::wstring {
                std::wstring escaped;
                for (wchar_t c : s) {
                    switch (c) {
                    case L'\\': escaped += L"\\\\"; break;
                    case L'\'': escaped += L"\\'"; break;
                    case L'"': escaped += L"\\\""; break;
                    case L'\n': escaped += L"\\n"; break;
                    case L'\r': escaped += L""; break;
                    case L'\t': escaped += L"\\t"; break;
                    default: escaped += c;
                    }
                }
                return escaped;
            };

            // Check for confirmations
            if (delta.find(L"__CONFIRM_PS__") == 0) {
                std::wstring script = delta.substr(14);
                std::wstring escaped = escapeJs(script);
                execJsRaw(L"if(window.confirmPowerShell){window.confirmPowerShell('" + escaped + L"')}");
            } else if (delta.find(L"__CONFIRM_KILL__") == 0) {
                std::wstring body = delta.substr(17);
                size_t sep = body.find(L'|');
                std::wstring name = escapeJs(body.substr(0, sep));
                std::wstring pid  = escapeJs(body.substr(sep + 1));
                execJsRaw(L"if(window.confirmKill){window.confirmKill('" + name + L"','" + pid + L"')}");
            } else if (delta.find(L"__IMAGE__") == 0) {
                std::wstring body = delta.substr(9);
                size_t sep = body.find(L'\x1f');
                if (sep != std::wstring::npos) {
                    std::wstring name = escapeJs(body.substr(0, sep));
                    std::wstring b64  = escapeJs(body.substr(sep + 1));
                    execJsRaw(L"if(window.addImageBlock){window.addImageBlock('" + name + L"','" + b64 + L"')}");
                }
            } else if (delta.find(L"__TOOL_CALLING__") == 0) {
                std::wstring toolName = escapeJs(delta.substr(17));
                execJsRaw(L"if(window.showToolCalling){window.showToolCalling('" + toolName + L"')}");
            } else if (delta.find(L"__TOOL_ARGS__") == 0) {
                std::wstring argsText = escapeJs(delta.substr(14));
                execJsRaw(L"if(window.appendToolArgs){window.appendToolArgs('" + argsText + L"')}");
            } else if (delta.find(L"__TOOL__") == 0) {
                std::wstring body = delta.substr(8);
                size_t sep = body.find(L'\x1f');
                if (sep != std::wstring::npos) {
                    std::wstring name = escapeJs(body.substr(0, sep));
                    std::wstring result = escapeJs(body.substr(sep + 1));
                    execJsRaw(L"if(window.addToolBlock)window.addToolBlock('" + name + L"','" + result + L"')");
                }
            } else if (!delta.empty() && delta[0] == L'\x01') {
                std::wstring think = escapeJs(delta.substr(1));
                execJsRaw(L"if(window.appendThinking){window.appendThinking('a','" + think + L"')}");
            } else if (!delta.empty()) {
                std::wstring escaped = escapeJs(delta);
                execJsRaw(L"if(window.appendStreamMessage){window.appendStreamMessage({id:'a',type:'agent',name:'Open Aries AI',content:'"
                    + escaped + L"',showActions:true})}");
            }
        }

        if (hasMore) {
            PostMessageW(hwnd, WM_AI_DELTA, 0, 0);
        } else if (done && !g_agentRunning) {
            g_aiBusy = false;
            execJs(L"window.setLoading(false)");
        }
        return 0;
    }

    case WM_TOOL_RESULT: {
        std::wstring* pName = (std::wstring*)wp;
        std::wstring* pResult = (std::wstring*)lp;
        if (pName && pResult) {
            auto esc = [](const std::wstring& s) -> std::wstring {
                std::wstring e;
                for (wchar_t c : s) {
                    switch (c) {
                    case L'\\': e += L"\\\\"; break;
                    case L'\'': e += L"\\'"; break;
                    case L'"': e += L"\\\""; break;
                    case L'\n': e += L"\\n"; break;
                    case L'\r': break;
                    case L'\t': e += L"\\t"; break;
                    default: e += c;
                    }
                }
                return e;
            };
            execJsRaw(L"if(window.addToolBlock)window.addToolBlock('" + esc(*pName) + L"','" + esc(*pResult) + L"')");
            delete pName;
            delete pResult;
        }
        return 0;
    }

    case WM_TEST_RESULT: {
        bool ok;
        std::string msg;
        {
            std::lock_guard<std::mutex> lk(g_testMutex);
            ok = g_testOk;
            msg = g_testMsg;
        }
        // Truncate long error messages for JS
        if (msg.length() > 200) msg = msg.substr(0, 200) + "...";
        std::wstring wmsg = utf8_to_ws(msg);
        // Escape for JS string
        std::wstring escaped;
        for (wchar_t c : wmsg) {
            switch (c) {
            case L'\\': escaped += L"\\\\"; break;
            case L'\'': escaped += L"\\'"; break;
            case L'"': escaped += L"\\\""; break;
            case L'\n': escaped += L"\\n"; break;
            case L'\r': break;
            default: escaped += c;
            }
        }
        execJsRaw(L"if(window.showTestResult){window.showTestResult("
                  + std::wstring(ok ? L"true" : L"false")
                  + L",'" + escaped + L"')}");
        return 0;
    }

    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        mmi->ptMinTrackSize = { 480, 320 };
        // Constrain maximize to work area (excludes taskbar)
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfoW(mon, &mi)) {
            mmi->ptMaxPosition.x = mi.rcWork.left;
            mmi->ptMaxPosition.y = mi.rcWork.top;
            mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0;
    }

    case WM_SIZE:
        if (g_controller) {
            RECT bounds; GetClientRect(hwnd, &bounds);
            g_controller->put_Bounds(bounds);
        }
        return 0;

    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    // Init log
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* slash = wcsrchr(exePath, L'\\');
    if (slash) { *(slash + 1) = 0; }
    wcscat(exePath, L"webview2_debug.log");
    g_log = _wfopen(exePath, L"w");
    LOG("=== Open Aries AI ===");

    // Load config
    auto cfg = loadConfig();
    g_config = cfg;
    if (!cfg.valid) {
        LOG("First launch: no valid config, will prompt for settings in UI");
    } else {
        // Auto-detect model capabilities
        bool vis = (cfg.model.find("Kimi") != std::string::npos ||
                    cfg.model.find("kimi") != std::string::npos ||
                    cfg.model.find("gemini") != std::string::npos ||
                    cfg.model.find("GLM") != std::string::npos ||
                    cfg.model.find("gpt") != std::string::npos ||
                    cfg.model.find("claude") != std::string::npos);
        // Create AI provider only if config is valid
        g_provider = std::make_shared<OpenAICompatibleProvider>(
            cfg.apiKey, cfg.apiHost, cfg.model, vis, false, false);
        g_modelName = cfg.model;
        providerSetLog(g_log);
        LOG("AI provider created: %s @ %s", cfg.model.c_str(), cfg.apiHost.c_str());
    }

    // Build agent prompt with actual Windows paths
    {
        wchar_t userProfile[MAX_PATH] = {};
        wchar_t desktop[MAX_PATH] = {};
        wchar_t documents[MAX_PATH] = {};
        GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH);
        if (userProfile[0]) {
            wcscpy(desktop, userProfile); wcscat(desktop, L"\\Desktop");
            wcscpy(documents, userProfile); wcscat(documents, L"\\Documents");
        }
        std::wstring up = userProfile[0] ? userProfile : L"C:\\Users";
        std::wstring dt = desktop[0] ? desktop : up + L"\\Desktop";
        std::wstring doc = documents[0] ? documents : up + L"\\Documents";

        g_agentPrompt = L"你是一个具有文件系统访问能力的AI助手Agent。你在Windows系统上运行。\n"
            L"你可以通过函数调用（function calling）来使用工具，这是唯一的方式。\n\n"
            L"你的用户主目录是: " + up + L"\n"
            L"用户桌面路径: " + dt + L"\n"
            L"用户文档路径: " + doc + L"\n\n"
            L"使用方法：当你需要使用工具时，直接调用对应的函数即可。\n"
            L"一次只能调用一个工具。系统会执行该工具并将结果返回给你。\n"
            L"如果你不需要使用工具，直接回复最终结果即可。\n"
            L"完成所有任务后，**必须**用中文总结结果回复用户，即使工具已经列出了信息也必须基于工具结果给用户一个清晰的总结。\n\n"
            L"！！工具选择规则（必须遵守）：\n"
            L"- 复制文件**必须**用 COPY_FILE(source, destination)\n"
            L"- 删除文件**必须**用 DELETE_FILE(path)\n"
            L"- 移动/重命名文件**必须**用 MOVE_FILE(source, destination)\n"
            L"- 列出目录**必须**用 LIST_DIR\n"
            L"- 读取文件**必须**用 READ_FILE\n"
            L"- 写入文件**必须**用 WRITE_FILE\n"
            L"- 以上工具直接执行，无需用户确认\n"
            L"- **禁止**对文件复制/删除/移动/列表/读写操作使用 RUN_PS！！RUN_PS 仅限纯系统管理（如注册表、服务管理）\n\n"
            L"重要：所有路径必须是完整的Windows绝对路径（如 C:\\Users\\用户名\\Desktop），不要使用 ~/ 或 / 开头的Unix路径。";
    }

    if (!load_webview2()) {
        wchar_t exeDir[MAX_PATH];
        GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
        wchar_t* slash = wcsrchr(exeDir, L'\\');
        if (slash) *(slash + 1) = 0;
        wcscat(exeDir, L"WebView2Loader.dll");

        std::wstring msg = L"无法加载 WebView2Loader.dll\n\n"
                           L"请确保该文件存在于以下任一位置：\n"
                           L"  1. 应用程序所在目录\n"
                           L"     ";
        msg += exeDir;
        msg += L"\n  2. 系统 PATH 目录\n\n"
               L"可从 NuGet 下载：\n"
               L"  https://www.nuget.org/packages/Microsoft.Web.WebView2";

        MessageBoxW(nullptr, msg.c_str(), L"DLL 缺失", MB_ICONERROR);
        if (g_log) fclose(g_log);
        return 1;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS;
    RegisterClassExW(&wc);

    int winW = 960, winH = 640;
    int winX = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
    int winY = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;
    g_hwnd = CreateWindowExW(
        0, WINDOW_CLASS, L"Open Aries AI",
        WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
        winX, winY, winW, winH,
        nullptr, nullptr, hInst, nullptr);

    int cornerPref = 2;
    DwmSetWindowAttribute(g_hwnd, 33, &cornerPref, sizeof(cornerPref));
    MARGINS margins = { 0, 0, 0, 0 };
    DwmExtendFrameIntoClientArea(g_hwnd, &margins);

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    // Load app icon from PNG (next to exe)
    {
        // Ensure GDI+ is up
        static ULONG_PTR gdiToken = 0;
        static bool gdiInit = false;
        if (!gdiInit) {
            Gdiplus::GdiplusStartupInput inp;
            Gdiplus::GdiplusStartup(&gdiToken, &inp, nullptr);
            gdiInit = true;
        }
        wchar_t iconPath[MAX_PATH];
        GetModuleFileNameW(nullptr, iconPath, MAX_PATH);
        wchar_t* s = wcsrchr(iconPath, L'\\');
        if (s) *(s + 1) = 0;
        wcscat(iconPath, L"app_icon.png");
        Gdiplus::Bitmap bmp(iconPath);
        HICON hIcon = nullptr;
        if (bmp.GetLastStatus() == Gdiplus::Ok) {
            bmp.GetHICON(&hIcon);
        }
        if (hIcon) {
            SendMessageW(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessageW(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    std::atomic_flag initFlag = ATOMIC_FLAG_INIT;
    initFlag.test_and_set();

    wchar_t userData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, userData))) {
        wcscat(userData, L"\\OpenAriesAI");
    } else {
        wcscpy(userData, L"C:\\Users\\44338\\AppData\\Local\\OpenAriesAI");
    }

    auto handler = new WebView2Handler(g_hwnd, &initFlag);
    handler->AddRef();
    handler->setAttemptFn([&]() {
        return g_pCreateEnv(nullptr, userData, nullptr, handler);
    });
    g_pCreateEnv(nullptr, userData, nullptr, handler);

    MSG msg = {};
    while (initFlag.test_and_set() && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (!g_controller || !g_webview) {
        // Check if WebView2 Runtime is installed
        bool hasRuntime = false;
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            hasRuntime = true; RegCloseKey(hKey);
        }
        if (!hasRuntime && RegOpenKeyExW(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            hasRuntime = true; RegCloseKey(hKey);
        }

        std::wstring msg;
        if (!hasRuntime) {
            msg = L"WebView2 Runtime 未安装。\n\n"
                  L"请下载并安装 Evergreen WebView2 Runtime：\n"
                  L"https://developer.microsoft.com/microsoft-edge/webview2/\n\n"
                  L"选择 \"Evergreen Standalone Installer\" 下载安装即可。";
        } else {
            msg = L"WebView2 初始化失败。\n\n"
                  L"可能的原因：\n"
                  L"  1. WebView2Loader.dll 缺失或版本不匹配\n"
                  L"  2. WebView2 Runtime 需要更新\n"
                  L"  3. 用户数据目录不可写\n\n"
                  L"请查看 webview2_debug.log 了解详情。";
        }
        MessageBoxW(g_hwnd, msg.c_str(), L"WebView2 初始化失败", MB_ICONERROR);

        // Try to open the download page
        if (!hasRuntime) {
            ShellExecuteW(nullptr, L"open",
                L"https://developer.microsoft.com/microsoft-edge/webview2/",
                nullptr, nullptr, SW_SHOW);
        }

        handler->Release();
        CoUninitialize();
        if (g_log) fclose(g_log);
        return 1;
    }

    handler->Release();
    LOG("Ready. Model: %s", g_modelName.c_str());

    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_webview) g_webview->Release();
    if (g_controller) g_controller->Release();
    FreeLibrary(g_wv2Dll);
    CoUninitialize();
    if (g_log) fclose(g_log);
    return 0;
}
