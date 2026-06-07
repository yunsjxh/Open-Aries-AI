#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <ctime>
#include <algorithm>

using Microsoft::WRL::ComPtr;

// ============================================================================
// Base64
// ============================================================================
static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= (uint32_t)data[i + 2];
        out.push_back(B64[(n >> 18) & 0x3F]);
        out.push_back(B64[(n >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? B64[(n >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? B64[n & 0x3F] : '=');
    }
    return out;
}

// ============================================================================
// JSON helpers
// ============================================================================
std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out.push_back(c);
        }
    }
    return out;
}

// Find "key":"value" and return value (unescaped)
std::string json_get_string(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return "";

    pos += pat.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;
    if (pos >= json.size() || json[pos] != ':') return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;
    if (pos >= json.size() || json[pos] != '"') return "";

    pos++; // skip opening quote
    std::string val;
    while (pos < json.size()) {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            char next = json[pos + 1];
            switch (next) {
                case '"':  val += '"';  break;
                case '\\': val += '\\'; break;
                case '/':  val += '/';  break;
                case 'b':  val += '\b'; break;
                case 'f':  val += '\f'; break;
                case 'n':  val += '\n'; break;
                case 'r':  val += '\r'; break;
                case 't':  val += '\t'; break;
                default:   val += next;  break;
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

// Extract the raw value for a key — handles object {}, array [], string, number, null
std::string json_get_raw_value(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return "";
    pos += pat.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;
    if (pos >= json.size() || json[pos] != ':') return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;
    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        size_t start = pos;
        pos++;
        while (pos < json.size()) {
            if (json[pos] == '\\') { pos += 2; continue; }
            if (json[pos] == '"') break;
            pos++;
        }
        return json.substr(start, pos - start + 1);
    }
    if (json[pos] == '{' || json[pos] == '[') {
        char open = json[pos], close = (open == '{') ? '}' : ']';
        int depth = 0;
        size_t start = pos;
        while (pos < json.size()) {
            char c = json[pos];
            if (c == open) depth++;
            else if (c == close) { depth--; if (depth == 0) { pos++; break; } }
            else if (c == '"') {
                pos++;
                while (pos < json.size()) {
                    if (json[pos] == '\\') pos++;
                    else if (json[pos] == '"') break;
                    pos++;
                }
            }
            pos++;
        }
        return json.substr(start, pos - start);
    }
    if (json.compare(pos, 4, "null") == 0 || json.compare(pos, 4, "Null") == 0)
        return "null";
    size_t end = pos;
    while (end < json.size() && (isdigit((unsigned char)json[end]) || json[end] == '-' || json[end] == '.'))
        end++;
    return json.substr(pos, end - pos);
}

// Extract the raw "id" value from a JSON-RPC message (number, quoted string, or "null")
std::string json_get_id_raw(const std::string& json) {
    size_t pos = json.find("\"id\"");
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;
    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        size_t end = pos + 1;
        while (end < json.size()) {
            if (json[end] == '\\') { end += 2; continue; }
            if (json[end] == '"') break;
            end++;
        }
        return json.substr(pos, end - pos + 1); // include quotes
    }
    if (json.compare(pos, 4, "null") == 0 || json.compare(pos, 4, "Null") == 0)
        return "null";

    size_t end = pos;
    while (end < json.size() && (isdigit((unsigned char)json[end]) || json[end] == '-' || json[end] == '.'))
        end++;
    return json.substr(pos, end - pos);
}

// ============================================================================
// Wide-char to UTF-8 helper
// ============================================================================
std::string wchar_to_utf8(const wchar_t* wstr) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr);
    return result;
}

std::wstring utf8_to_wchar(const std::string& str) {
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], len);
    return result;
}

// ============================================================================
// Display & Window helpers
// ============================================================================

struct DisplayInfo {
    RECT rect;
    bool primary;
    int index;
};

static std::vector<DisplayInfo> g_displays;

BOOL CALLBACK enum_monitors_proc(HMONITOR hMon, HDC, LPRECT, LPARAM) {
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoA(hMon, &mi);
    DisplayInfo d;
    d.rect = mi.rcMonitor;
    d.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    d.index = (int)g_displays.size();
    g_displays.push_back(d);
    return TRUE;
}

std::vector<DisplayInfo> get_displays() {
    g_displays.clear();
    EnumDisplayMonitors(nullptr, nullptr, enum_monitors_proc, 0);
    return g_displays;
}

RECT get_display_rect(int index) {
    auto displays = get_displays();
    if (index < 0 || index >= (int)displays.size())
        throw std::runtime_error("Display index " + std::to_string(index) +
            " out of range. " + std::to_string(displays.size()) + " display(s) available.");
    return displays[index].rect;
}

std::string displays_json() {
    auto displays = get_displays();
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < displays.size(); i++) {
        if (i) ss << ",";
        auto& d = displays[i];
        ss << "{\"index\":" << d.index
           << ",\"primary\":" << (d.primary ? "true" : "false")
           << ",\"left\":" << d.rect.left
           << ",\"top\":" << d.rect.top
           << ",\"width\":" << (d.rect.right - d.rect.left)
           << ",\"height\":" << (d.rect.bottom - d.rect.top) << "}";
    }
    ss << "]";
    return ss.str();
}

struct WindowEntry {
    std::string title;
    RECT rect;
    DWORD pid;
};

static std::vector<WindowEntry> g_window_list;
static std::wstring g_find_title_w;
static HWND g_found_hwnd;

BOOL CALLBACK enum_windows_proc(HWND hwnd, LPARAM) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t buf[512];
    GetWindowTextW(hwnd, buf, sizeof(buf) / sizeof(wchar_t));
    if (wcslen(buf) == 0) return TRUE;
    WindowEntry w;
    w.title = wchar_to_utf8(buf);
    GetWindowRect(hwnd, &w.rect);
    GetWindowThreadProcessId(hwnd, &w.pid);
    g_window_list.push_back(w);
    return TRUE;
}

BOOL CALLBACK find_window_proc(HWND hwnd, LPARAM) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t buf[512];
    GetWindowTextW(hwnd, buf, sizeof(buf) / sizeof(wchar_t));
    if (wcslen(buf) == 0) return TRUE;
    if (wcsstr(buf, g_find_title_w.c_str())) {
        g_found_hwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

std::string list_windows_json() {
    g_window_list.clear();
    EnumWindows(enum_windows_proc, 0);
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < g_window_list.size(); i++) {
        if (i) ss << ",";
        auto& w = g_window_list[i];
        ss << "{\"title\":\"" << json_escape(w.title) << "\","
           << "\"rect\":{\"left\":" << w.rect.left
           << ",\"top\":" << w.rect.top
           << ",\"right\":" << w.rect.right
           << ",\"bottom\":" << w.rect.bottom << "},"
           << "\"pid\":" << w.pid << "}";
    }
    ss << "]";
    return ss.str();
}

HWND find_window_by_title(const std::string& title) {
    g_find_title_w = utf8_to_wchar(title);
    g_found_hwnd = nullptr;
    EnumWindows(find_window_proc, 0);
    return g_found_hwnd;
}

void click_at(int x, int y) {
    SetCursorPos(x, y);
    Sleep(30);
    INPUT down = {};
    down.type = INPUT_MOUSE;
    down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &down, sizeof(INPUT));
    Sleep(10);
    INPUT up = {};
    up.type = INPUT_MOUSE;
    up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &up, sizeof(INPUT));
}

void move_window_to_rect(HWND hwnd, const RECT& target) {
    int tw = target.right - target.left;
    int th = target.bottom - target.top;
    // Get normal (restored) rect even if minimized/maximized
    WINDOWPLACEMENT wp = {sizeof(wp)};
    GetWindowPlacement(hwnd, &wp);
    if (wp.showCmd == SW_SHOWMINIMIZED || wp.showCmd == SW_SHOWMAXIMIZED)
        ShowWindow(hwnd, SW_RESTORE);
    Sleep(100);
    RECT& cur = wp.rcNormalPosition;
    int cw = cur.right - cur.left;
    int ch = cur.bottom - cur.top;
    if (cw <= 0 || ch <= 0) { cw = 800; ch = 600; } // fallback
    int nw = std::min(cw, tw);
    int nh = std::min(ch, th);
    int nx = target.left + (tw - nw) / 2;
    int ny = target.top + (th - nh) / 2;
    SetWindowPos(hwnd, nullptr, nx, ny, nw, nh,
        SWP_NOZORDER | SWP_SHOWWINDOW);
}

// ============================================================================
// WIC PNG encoding (BGRA in memory → PNG bytes)
// ============================================================================
std::vector<uint8_t> encode_png(const uint8_t* bgra, UINT width, UINT height, UINT stride) {
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory));
    if (FAILED(hr)) throw std::runtime_error("Failed to create WIC imaging factory");

    ComPtr<IStream> stream;
    hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
    if (FAILED(hr)) throw std::runtime_error("Failed to create memory stream");

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) throw std::runtime_error("Failed to create PNG encoder");

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) throw std::runtime_error("Failed to initialize PNG encoder");

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(&frame, &props);
    if (FAILED(hr)) throw std::runtime_error("Failed to create PNG frame");

    hr = frame->Initialize(props.Get());
    if (FAILED(hr)) throw std::runtime_error("Failed to initialize PNG frame");

    hr = frame->SetSize(width, height);
    if (FAILED(hr)) throw std::runtime_error("Failed to set PNG frame size");

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) throw std::runtime_error("Failed to set PNG pixel format");

    hr = frame->WritePixels(height, stride, stride * height, const_cast<BYTE*>(bgra));
    if (FAILED(hr)) throw std::runtime_error("Failed to write PNG pixels");

    hr = frame->Commit();
    if (FAILED(hr)) throw std::runtime_error("Failed to commit PNG frame");

    hr = encoder->Commit();
    if (FAILED(hr)) throw std::runtime_error("Failed to commit PNG encoder");

    // Read back encoded data from stream
    LARGE_INTEGER zero = {};
    hr = stream->Seek(zero, STREAM_SEEK_END, nullptr);
    if (FAILED(hr)) throw std::runtime_error("Failed to seek stream");

    ULARGE_INTEGER size;
    hr = stream->Seek(zero, STREAM_SEEK_CUR, &size);
    if (FAILED(hr)) throw std::runtime_error("Failed to get stream size");

    hr = stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    if (FAILED(hr)) throw std::runtime_error("Failed to seek stream to start");

    std::vector<uint8_t> result((size_t)size.QuadPart);
    ULONG bytesRead = 0;
    hr = stream->Read(result.data(), (ULONG)result.size(), &bytesRead);
    if (FAILED(hr)) throw std::runtime_error("Failed to read PNG data from stream");

    return result;
}

// ============================================================================
// DXGI capture — returns base64-encoded PNG
// ============================================================================
std::vector<uint8_t> capture_second_display_raw() {
    ComPtr<ID3D11Device> d3dDevice;
    ComPtr<ID3D11DeviceContext> d3dContext;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                    nullptr, 0, D3D11_SDK_VERSION,
                                    &d3dDevice, &fl, &d3dContext);
    if (FAILED(hr)) throw std::runtime_error("Failed to create D3D11 device");

    ComPtr<IDXGIDevice> dxgiDevice;
    d3dDevice.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(&adapter);

    ComPtr<IDXGIFactory1> factory;
    adapter->GetParent(__uuidof(IDXGIFactory1), &factory);

    struct Display { ComPtr<IDXGIOutput> output; DXGI_OUTPUT_DESC desc; };
    std::vector<Display> displays;

    ComPtr<IDXGIAdapter1> enumAdapter;
    UINT adapterIdx = 0;
    while (factory->EnumAdapters1(adapterIdx, &enumAdapter) != DXGI_ERROR_NOT_FOUND) {
        ComPtr<IDXGIOutput> output;
        UINT outputIdx = 0;
        while (enumAdapter->EnumOutputs(outputIdx, &output) != DXGI_ERROR_NOT_FOUND) {
            DXGI_OUTPUT_DESC desc;
            output->GetDesc(&desc);
            if (desc.AttachedToDesktop)
                displays.push_back({output, desc});
            outputIdx++;
        }
        adapterIdx++;
    }

    if (displays.size() < 2) {
        std::ostringstream ss;
        ss << "Only " << displays.size()
           << " attached display(s) found. At least 2 required (physical + virtual).";
        throw std::runtime_error(ss.str());
    }

    auto& target = displays[1];
    ComPtr<IDXGIOutput1> output1;
    hr = target.output.As(&output1);
    if (FAILED(hr)) throw std::runtime_error("IDXGIOutput1 not supported on second display");

    ComPtr<IDXGIOutputDuplication> dup;
    hr = output1->DuplicateOutput(d3dDevice.Get(), &dup);
    if (hr == E_ACCESSDENIED)
        throw std::runtime_error("Access denied. Run the MCP server as administrator.");
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << "DuplicateOutput failed: 0x" << std::hex << (uint32_t)hr;
        throw std::runtime_error(ss.str());
    }

    DXGI_OUTDUPL_DESC dupDesc;
    dup->GetDesc(&dupDesc);
    UINT w = dupDesc.ModeDesc.Width;
    UINT h = dupDesc.ModeDesc.Height;

    // Acquire current frame (no discarding — static displays won't produce new frames)
    ComPtr<IDXGIResource> desktopResource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    hr = dup->AcquireNextFrame(3000, &frameInfo, &desktopResource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        throw std::runtime_error("No frame on second display. Open a window or move something onto the virtual display.");
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << "AcquireNextFrame failed: 0x" << std::hex << (uint32_t)hr;
        throw std::runtime_error(ss.str());
    }

    ComPtr<ID3D11Texture2D> desktopTexture;
    desktopResource.As(&desktopTexture);

    D3D11_TEXTURE2D_DESC stagingDesc = {};
    desktopTexture->GetDesc(&stagingDesc);
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> stagingTexture;
    hr = d3dDevice->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) throw std::runtime_error("Failed to create staging texture");

    d3dContext->CopyResource(stagingTexture.Get(), desktopTexture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = d3dContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) throw std::runtime_error("Failed to map staging texture");

    std::vector<uint8_t> png = encode_png(
        static_cast<const uint8_t*>(mapped.pData), w, h, mapped.RowPitch);

    d3dContext->Unmap(stagingTexture.Get(), 0);
    dup->ReleaseFrame();

    return png;
}

std::string capture_second_display_b64() {
    auto png = capture_second_display_raw();
    return base64_encode(png.data(), png.size());
}

// ============================================================================
// JSON-RPC response builders
// ============================================================================
std::string build_response(const std::string& id, const std::string& result_json) {
    std::ostringstream ss;
    ss << "{\"jsonrpc\":\"2.0\",\"id\":" << (id.empty() ? "null" : id)
       << ",\"result\":" << result_json << "}";
    return ss.str();
}

std::string build_error(const std::string& id, int code, const std::string& message) {
    std::ostringstream ss;
    ss << "{\"jsonrpc\":\"2.0\",\"id\":" << (id.empty() ? "null" : id)
       << ",\"error\":{\"code\":" << code
       << ",\"message\":\"" << json_escape(message) << "\"}}";
    return ss.str();
}

// ============================================================================
// MCP message handler
// ============================================================================
std::string handle_message(const std::string& line) {
    std::string method = json_get_string(line, "method");
    std::string id    = json_get_id_raw(line);

    if (method.empty() && id.empty())
        return ""; // unparseable, ignore

    // --- initialize ---
    if (method == "initialize") {
        return build_response(id,
            "{\"protocolVersion\":\"2024-11-05\","
            "\"capabilities\":{\"tools\":{}},"
            "\"serverInfo\":{\"name\":\"capture-virtual-display\",\"version\":\"1.0.0\"}}");
    }

    // --- tools/list ---
    if (method == "tools/list") {
        return build_response(id,
            "{\"tools\":[{"
            "\"name\":\"capture_second_display\","
            "\"description\":\"Capture the current frame from the second attached display "
            "(virtual display) and return it as a PNG image. Also saved to temp file -- if you cannot view the image data, read the PNG file from the given path.\","
            "\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"required\":[]}},{"
            "\"name\":\"get_display_info\","
            "\"description\":\"Get information about all connected displays (index, position, size, primary).\","
            "\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"required\":[]}},{"
            "\"name\":\"list_windows\","
            "\"description\":\"List all visible windows with titles, positions, and sizes.\","
            "\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"required\":[]}},{"
            "\"name\":\"move_window_to_display\","
            "\"description\":\"Move a window (matched by title substring) to a specific display.\","
            "\"inputSchema\":{\"type\":\"object\","
            "\"properties\":{"
            "\"title\":{\"type\":\"string\",\"description\":\"Substring to match the window title\"},"
            "\"display_index\":{\"type\":\"integer\",\"description\":\"Target display index (0=primary, 1=second, etc.)\"}"
            "},\"required\":[\"title\"]}},{"
            "\"name\":\"click_on_display\","
            "\"description\":\"Click at coordinates on a specific display.\","
            "\"inputSchema\":{\"type\":\"object\","
            "\"properties\":{"
            "\"x\":{\"type\":\"integer\",\"description\":\"X coordinate relative to the target display\"},"
            "\"y\":{\"type\":\"integer\",\"description\":\"Y coordinate relative to the target display\"},"
            "\"display_index\":{\"type\":\"integer\",\"description\":\"Target display index (0=primary, default 1=second)\"}"
            "},\"required\":[\"x\",\"y\"]}}"
            "]}");
    }

    // --- tools/call ---
    if (method == "tools/call") {
        std::string params = json_get_raw_value(line, "params");
        std::string toolName = json_get_string(params, "name");

        if (toolName == "capture_second_display") {
            try {
                auto png = capture_second_display_raw();
                std::string b64png = base64_encode(png.data(), png.size());

                // Save to temp file
                char tmpPath[MAX_PATH];
                GetTempPathA(MAX_PATH, tmpPath);
                std::string filePath = std::string(tmpPath) + "capture_" +
                    std::to_string(std::time(nullptr)) + ".png";
                std::ofstream out(filePath, std::ios::binary);
                out.write(reinterpret_cast<const char*>(png.data()), png.size());
                out.close();

                std::ostringstream result;
                result << "{\"content\":["
                       << "{\"type\":\"text\",\"text\":\"To see the screenshot, read this image file and describe its contents: "
                       << json_escape(filePath) << "\"},"

                       << "{\"type\":\"image\","
                       << "\"data\":\"" << b64png << "\","
                       << "\"mimeType\":\"image/png\"}]}";
                return build_response(id, result.str());
            } catch (const std::exception& e) {
                return build_error(id, -32603, e.what());
            }
        }

        // --- get_display_info ---
        if (toolName == "get_display_info") {
            std::ostringstream result;
            result << "{\"content\":[{\"type\":\"text\",\"text\":"
                   << "\"Displays:\\n" << json_escape(displays_json()) << "\"}]}";
            return build_response(id, result.str());
        }

        // --- list_windows ---
        if (toolName == "list_windows") {
            std::ostringstream result;
            result << "{\"content\":[{\"type\":\"text\",\"text\":"
                   << "\"Windows:\\n" << json_escape(list_windows_json()) << "\"}]}";
            return build_response(id, result.str());
        }

        // --- move_window_to_display ---
        if (toolName == "move_window_to_display") {
            try {
                std::string argsStr = json_get_raw_value(params, "arguments");
                std::string title = json_get_string(argsStr, "title");
                if (title.empty())
                    return build_error(id, -32602, "Missing required parameter: title");

                std::string dispIdxStr = json_get_raw_value(argsStr, "display_index");
                int dispIdx = dispIdxStr.empty() ? 1 : std::stoi(dispIdxStr);

                RECT targetRect = get_display_rect(dispIdx);
                HWND hwnd = find_window_by_title(title);
                if (!hwnd)
                    return build_error(id, -32602, "Window not found: " + title);

                wchar_t wbuf[512];
                GetWindowTextW(hwnd, wbuf, sizeof(wbuf) / sizeof(wchar_t));
                std::string winTitle = wchar_to_utf8(wbuf);
                move_window_to_rect(hwnd, targetRect);
                Sleep(200); // let the window settle

                std::ostringstream result;
                result << "{\"content\":[{\"type\":\"text\",\"text\":"
                       << "\"Moved window '" << json_escape(winTitle)
                       << "' to display " << dispIdx << ".\"}]}";
                return build_response(id, result.str());
            } catch (const std::exception& e) {
                return build_error(id, -32603, e.what());
            }
        }

        // --- click_on_display ---
        if (toolName == "click_on_display") {
            try {
                std::string argsStr = json_get_raw_value(params, "arguments");
                std::string xStr = json_get_raw_value(argsStr, "x");
                std::string yStr = json_get_raw_value(argsStr, "y");
                if (xStr.empty() || yStr.empty())
                    return build_error(id, -32602, "Missing required parameters: x, y");

                std::string dispIdxStr = json_get_raw_value(argsStr, "display_index");
                int dispIdx = dispIdxStr.empty() ? 1 : std::stoi(dispIdxStr);

                RECT r = get_display_rect(dispIdx);
                int absX = r.left + std::stoi(xStr);
                int absY = r.top + std::stoi(yStr);

                click_at(absX, absY);

                std::ostringstream result;
                result << "{\"content\":[{\"type\":\"text\",\"text\":"
                       << "\"Clicked at display " << dispIdx
                       << " (" << absX << ", " << absY << " abs).\"}]}";
                return build_response(id, result.str());
            } catch (const std::exception& e) {
                return build_error(id, -32603, e.what());
            }
        }

        return build_error(id, -32602, "Unknown tool: " + toolName);
    }

    // Notification (no id) → no response
    if (id.empty())
        return "";

    return build_error(id, -32601, "Method not found: " + method);
}

// ============================================================================
// Main
// ============================================================================
int main() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        fprintf(stderr, "[mcp_server] CoInitializeEx failed: 0x%08X\n", (unsigned)hr);
        return 1;
    }

    setvbuf(stdout, nullptr, _IONBF, 0); // unbuffered stdout for JSON-RPC

    fflush(stdout);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::string resp = handle_message(line);
        if (!resp.empty()) {
            fprintf(stdout, "%s\n", resp.c_str());
            fflush(stdout);
        }
    }

    CoUninitialize();
    return 0;
}
