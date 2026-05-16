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
            wc.hbrBackground = CreateSolidBrush(RGB(22, 22, 38));
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            
            RegisterClassExW(&wc);
            
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int windowWidth = 270;
            int windowHeight = 200;
            
            hwnd_ = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
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

    void setTokenUsage(int prompt, int completion, int total) {
        tokenPrompt_ = prompt;
        tokenCompletion_ = completion;
        tokenTotal_ = total;
        invalidate();
    }

    void setPlanSteps(const std::vector<std::string>& steps) {
        addLog("[Plan] " + std::to_string(steps.size()) + " steps");
        for (size_t i = 0; i < steps.size() && i < 3; i++) {
            std::string s = steps[i];
            if (s.length() > 32) s = s.substr(0, 32) + "...";
            addLog(std::to_string(i + 1) + ". " + s);
        }
        if (steps.size() > 3) {
            addLog("  ...+" + std::to_string(steps.size() - 3) + " more");
        }
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
                RECT rc;
                GetClientRect(hwnd, &rc);
                int W = rc.right, H = rc.bottom;

                COLORREF cBg    = RGB(6, 6, 14);
                COLORREF cCyan  = RGB(0, 230, 250);
                COLORREF cGreen = RGB(0, 255, 130);
                COLORREF cText  = RGB(200, 210, 225);
                COLORREF cDim   = RGB(80, 85, 100);
                COLORREF cRed   = RGB(255, 55, 85);
                COLORREF cBar   = RGB(25, 25, 45);
                COLORREF cPanel = RGB(14, 14, 26);

                HFONT hMono  = CreateFontW(14, 0,0,0, FW_SEMIBOLD, 0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, L"Consolas");
                HFONT hSmall = CreateFontW(12, 0,0,0, FW_NORMAL,   0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH|FF_DONTCARE, L"Microsoft YaHei");
                HFONT hMini  = CreateFontW(11, 0,0,0, FW_NORMAL,   0,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH|FF_DONTCARE, L"Microsoft YaHei");

                SetBkMode(hdc, TRANSPARENT);

                // 背景
                HBRUSH hBg = CreateSolidBrush(cBg);
                FillRect(hdc, &rc, hBg);
                DeleteObject(hBg);

                // 顶边亮线
                HPEN hPen = CreatePen(PS_SOLID, 1, cCyan);
                SelectObject(hdc, hPen);
                MoveToEx(hdc, 0, 0, nullptr); LineTo(hdc, W, 0);
                // 底边暗线
                HPEN hPen2 = CreatePen(PS_SOLID, 1, RGB(20,20,40));
                SelectObject(hdc, hPen2);
                MoveToEx(hdc, 0, H-1, nullptr); LineTo(hdc, W, H-1);

                // 标题行背景
                RECT hdr = {0, 1, W, 30};
                HBRUSH hHdr = CreateSolidBrush(cPanel);
                FillRect(hdc, &hdr, hHdr);
                DeleteObject(hHdr);

                // 标题
                SelectObject(hdc, hMono);
                SetTextColor(hdc, cCyan);
                TextOutW(hdc, 10, 7, L"ARIES", 5);
                SetTextColor(hdc, cText);
                TextOutW(hdc, 62, 7, L"AI", 2);

                // Token / 状态
                if (self) {
                    SelectObject(hdc, hMini);
                    wchar_t tb[60];
                    if (self->tokenTotal_ > 0) {
                        SetTextColor(hdc, cGreen);
                        swprintf(tb, 60, L"%d+%d=%d", self->tokenPrompt_, self->tokenCompletion_, self->tokenTotal_);
                    } else {
                        SetTextColor(hdc, cDim);
                        wcscpy(tb, L"IDLE");
                    }
                    SIZE ts;
                    GetTextExtentPoint32W(hdc, tb, wcslen(tb), &ts);
                    TextOutW(hdc, W - ts.cx - 10, 9, tb, wcslen(tb));
                }

                // 分隔线
                SetTextColor(hdc, cDim);
                SelectObject(hdc, hPen);
                MoveToEx(hdc, 8, 30, nullptr); LineTo(hdc, W-8, 30);

                // 进度条
                int barY = 36, barH = 2;
                RECT barBgR = {10, barY, W-10, barY+barH};
                HBRUSH hBarBg = CreateSolidBrush(cBar);
                FillRect(hdc, &barBgR, hBarBg);
                DeleteObject(hBarBg);
                if (self && self->maxIterations_ > 0) {
                    float pct = (float)self->iteration_ / self->maxIterations_;
                    if (pct > 1) pct = 1;
                    int fw = (int)((W-20)*pct); if (fw<2)fw=2;
                    RECT barFr = {10, barY, 10+fw, barY+barH};
                    HBRUSH hBarF = CreateSolidBrush(cCyan);
                    FillRect(hdc, &barFr, hBarF);
                    DeleteObject(hBarF);
                }

                // 信息区
                SelectObject(hdc, hSmall);
                SetTextColor(hdc, cDim);
                wchar_t buf[64];
                swprintf(buf, 64, L"ITER  %d/%d", self?self->iteration_:0, self?self->maxIterations_:10);
                TextOutW(hdc, 10, 44, buf, wcslen(buf));

                if (self && !self->currentAction_.empty()) {
                    SetTextColor(hdc, cText);
                    int alen = MultiByteToWideChar(CP_UTF8,0,self->currentAction_.c_str(),-1,nullptr,0);
                    std::wstring wa(alen,0);
                    MultiByteToWideChar(CP_UTF8,0,self->currentAction_.c_str(),-1,&wa[0],alen);
                    if (wa.size() > 24) wa = wa.substr(0,24)+L"...";
                    TextOutW(hdc, 10, 64, wa.c_str(), wa.size());
                }

                if (self && !self->actionDetail_.empty()) {
                    SelectObject(hdc, hMini);
                    SetTextColor(hdc, cDim);
                    int dlen = MultiByteToWideChar(CP_UTF8,0,self->actionDetail_.c_str(),-1,nullptr,0);
                    std::wstring wd(dlen,0);
                    MultiByteToWideChar(CP_UTF8,0,self->actionDetail_.c_str(),-1,&wd[0],dlen);
                    if (wd.size()>32) wd = wd.substr(0,32)+L"...";
                    TextOutW(hdc, 10, 82, wd.c_str(), wd.size());
                }

                // 日志区
                int logStart = 100;
                SelectObject(hdc, hMini);
                SetTextColor(hdc, RGB(0,180,220));
                TextOutW(hdc, 10, logStart, L">> LOG", 5);

                if (self) {
                    std::lock_guard<std::mutex> lock(self->mutex_);
                    int total = (int)self->logs_.size();
                    for (int i=total-1,n=0; i>=0 && n<2; i--,n++) {
                        const auto& log = self->logs_[i];
                        int len = MultiByteToWideChar(CP_UTF8,0,log.c_str(),-1,nullptr,0);
                        std::wstring wl(len,0);
                        MultiByteToWideChar(CP_UTF8,0,log.c_str(),-1,&wl[0],len);
                        if (wl.size()>36) wl = wl.substr(0,36)+L"...";
                        SetTextColor(hdc, (i==total-1)?cGreen:cDim);
                        TextOutW(hdc, 14, logStart+16+n*17, wl.c_str(), wl.size());
                    }
                }

                // 右下角角标
                SelectObject(hdc, hMini);
                SetTextColor(hdc, RGB(0, 180, 220));
                TextOutW(hdc, W-48, H-17, L"v1.3.1", 6);

                // STOP 按钮
                int btnW = 52, btnH = 22;
                int btnX = 10, btnY = H - btnH - 8;
                RECT btnR = {btnX, btnY, btnX+btnW, btnY+btnH};
                HBRUSH hBtn = CreateSolidBrush(cRed);
                FillRect(hdc, &btnR, hBtn);
                DeleteObject(hBtn);

                SelectObject(hdc, hSmall);
                SetTextColor(hdc, RGB(255,255,255));
                SetTextAlign(hdc, TA_CENTER);
                TextOutW(hdc, btnX+btnW/2, btnY+3, L"STOP", 4);
                SetTextAlign(hdc, TA_LEFT);

                // 清理
                DeleteObject(hMono);
                DeleteObject(hSmall);
                DeleteObject(hMini);
                DeleteObject(hPen);
                DeleteObject(hPen2);

                EndPaint(hwnd, &ps);
                break;
            }
            
            case WM_LBUTTONDOWN: {
                if (self && self->stopCallback_) {
                    RECT rect;
                    GetClientRect(hwnd, &rect);
                    int btnW = 52, btnH = 22;
                    int btnX = 10;
                    int btnY = rect.bottom - btnH - 8;
                    POINT pt = {LOWORD(lParam), HIWORD(lParam)};
                    if (pt.x >= btnX && pt.x <= btnX + btnW &&
                        pt.y >= btnY && pt.y <= btnY + btnH) {
                        self->stopCallback_();
                    }
                }
                break;
            }
            
            case WM_USER:
                if (self && self->hwnd_) {
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                break;

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
            PostMessageW(hwnd_, WM_USER, 0, 0);
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
    int tokenPrompt_ = 0;
    int tokenCompletion_ = 0;
    int tokenTotal_ = 0;
};

}
