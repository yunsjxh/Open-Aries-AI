#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace aries {

class StatusWindow {
public:
    static StatusWindow& getInstance() {
        static StatusWindow instance;
        return instance;
    }
    
    bool create() {
        if (hwnd_) return true;
        if (running_) return true;
        
        running_ = true;
        
        // 在单独线程创建窗口并运行消息循环
        windowThread_ = std::thread([this]() {
            WNDCLASSEXW wc = {};
            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.lpfnWndProc = WindowProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = L"AriesStatusWindow";
            wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 40));
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            
            RegisterClassExW(&wc);
            
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int windowWidth = 360;
            int windowHeight = 280;
            
            hwnd_ = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                L"AriesStatusWindow",
                L"Open-Aries-AI",
                WS_POPUP | WS_BORDER,
                screenWidth - windowWidth - 20,
                20,
                windowWidth,
                windowHeight,
                nullptr,
                nullptr,
                GetModuleHandle(nullptr),
                nullptr
            );
            
            if (!hwnd_) {
                running_ = false;
                return;
            }
            
            SetLayeredWindowAttributes(hwnd_, 0, 240, LWA_ALPHA);
            SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            ShowWindow(hwnd_, SW_SHOW);
            UpdateWindow(hwnd_);
            
            // 消息循环
            MSG msg;
            while (running_ && GetMessage(&msg, nullptr, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            
            if (hwnd_) {
                DestroyWindow(hwnd_);
                hwnd_ = nullptr;
            }
        });
        
        // 等待窗口创建完成
        for (int i = 0; i < 100 && !hwnd_; i++) {
            Sleep(10);
        }
        
        return hwnd_ != nullptr;
    }
    
    void destroy() {
        running_ = false;
        if (hwnd_) {
            PostMessage(hwnd_, WM_CLOSE, 0, 0);
        }
        if (windowThread_.joinable()) {
            windowThread_.detach();
        }
        hwnd_ = nullptr;
    }
    
    void setIteration(int current, int max) {
        iteration_ = current;
        maxIterations_ = max;
        invalidate();
    }
    
    void setCurrentAction(const std::string& action) {
        currentAction_ = action;
        invalidate();
    }
    
    void setActionDetail(const std::string& detail) {
        actionDetail_ = detail;
        invalidate();
    }
    
    void addLog(const std::string& log) {
        std::lock_guard<std::mutex> lock(mutex_);
        logs_.push_back(log);
        if (logs_.size() > 5) {
            logs_.erase(logs_.begin());
        }
        invalidate();
    }
    
    void clearLogs() {
        std::lock_guard<std::mutex> lock(mutex_);
        logs_.clear();
        currentAction_.clear();
        iteration_ = 0;
        maxIterations_ = 10;
        invalidate();
    }
    
    void setStopCallback(std::function<void()> callback) {
        stopCallback_ = callback;
    }
    
    HWND getHwnd() const { return hwnd_; }
    
private:
    StatusWindow() = default;
    ~StatusWindow() { destroy(); }
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        StatusWindow* self = reinterpret_cast<StatusWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                RECT rect;
                GetClientRect(hwnd, &rect);
                
                HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
                HFONT hFontBold = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
                
                SelectObject(hdc, hFontBold);
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);
                
                TextOutW(hdc, 15, 10, L"AI 执行状态", 7);
                
                SelectObject(hdc, hFont);
                
                wchar_t buf[256];
                swprintf(buf, 256, L"迭代: %d / %d", self ? self->iteration_ : 0, self ? self->maxIterations_ : 10);
                TextOutW(hdc, 15, 40, buf, wcslen(buf));
                
                if (self && !self->currentAction_.empty()) {
                    int len = MultiByteToWideChar(CP_UTF8, 0, self->currentAction_.c_str(), -1, nullptr, 0);
                    std::wstring waction(len, 0);
                    MultiByteToWideChar(CP_UTF8, 0, self->currentAction_.c_str(), -1, &waction[0], len);
                    TextOutW(hdc, 15, 65, L"当前动作:", 5);
                    TextOutW(hdc, 85, 65, waction.c_str(), waction.size() - 1);
                }
                
                // 显示动作详细信息
                if (self && !self->actionDetail_.empty()) {
                    int len = MultiByteToWideChar(CP_UTF8, 0, self->actionDetail_.c_str(), -1, nullptr, 0);
                    std::wstring wdetail(len, 0);
                    MultiByteToWideChar(CP_UTF8, 0, self->actionDetail_.c_str(), -1, &wdetail[0], len);
                    if (wdetail.size() > 40) wdetail = wdetail.substr(0, 40) + L"...";
                    TextOutW(hdc, 15, 90, L"详细信息:", 5);
                    TextOutW(hdc, 85, 90, wdetail.c_str(), wdetail.size());
                }
                
                TextOutW(hdc, 15, 120, L"最近日志:", 5);
                
                int y = 140;
                if (self) {
                    std::lock_guard<std::mutex> lock(self->mutex_);
                    for (const auto& log : self->logs_) {
                        int len = MultiByteToWideChar(CP_UTF8, 0, log.c_str(), -1, nullptr, 0);
                        std::wstring wlog(len, 0);
                        MultiByteToWideChar(CP_UTF8, 0, log.c_str(), -1, &wlog[0], len);
                        if (wlog.size() > 35) wlog = wlog.substr(0, 35) + L"...";
                        TextOutW(hdc, 20, y, wlog.c_str(), wlog.size());
                        y += 18;
                    }
                }
                
                DeleteObject(hFont);
                DeleteObject(hFontBold);
                
                // 绘制停止按钮
                RECT btnRect;
                GetClientRect(hwnd, &btnRect);
                int btnWidth = 60;
                int btnHeight = 25;
                int btnX = btnRect.right - btnWidth - 10;
                int btnY = btnRect.bottom - btnHeight - 10;
                
                btnRect = {btnX, btnY, btnX + btnWidth, btnY + btnHeight};
                HBRUSH hBtnBrush = CreateSolidBrush(RGB(239, 68, 68));
                FillRect(hdc, &btnRect, hBtnBrush);
                DeleteObject(hBtnBrush);
                
                // 绘制按钮文字
                HFONT hBtnFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
                SelectObject(hdc, hBtnFont);
                SetTextColor(hdc, RGB(255, 255, 255));
                SetTextAlign(hdc, TA_CENTER);
                TextOutW(hdc, btnX + btnWidth / 2, btnY + 5, L"停止", 2);
                DeleteObject(hBtnFont);
                
                EndPaint(hwnd, &ps);
                break;
            }
            
            case WM_LBUTTONDOWN: {
                if (self && self->stopCallback_) {
                    RECT rect;
                    GetClientRect(hwnd, &rect);
                    int btnWidth = 60;
                    int btnHeight = 25;
                    int btnX = rect.right - btnWidth - 10;
                    int btnY = rect.bottom - btnHeight - 10;
                    
                    POINT pt = {LOWORD(lParam), HIWORD(lParam)};
                    if (pt.x >= btnX && pt.x <= btnX + btnWidth &&
                        pt.y >= btnY && pt.y <= btnY + btnHeight) {
                        self->stopCallback_();
                    }
                }
                break;
            }
            
            case WM_DESTROY:
                PostQuitMessage(0);
                if (self) self->hwnd_ = nullptr;
                break;
            
            default:
                return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        return 0;
    }
    
    void invalidate() {
        if (hwnd_) {
            InvalidateRect(hwnd_, nullptr, TRUE);
        }
    }
    
    HWND hwnd_ = nullptr;
    std::atomic<bool> running_{false};
    std::thread windowThread_;
    std::mutex mutex_;
    int iteration_ = 0;
    int maxIterations_ = 10;
    std::string currentAction_;
    std::string actionDetail_;
    std::vector<std::string> logs_;
    std::function<void()> stopCallback_;
};

}
