#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdint>
#include <algorithm>

using Microsoft::WRL::ComPtr;

// link: -ld3d11 -ldxgi -ldxguid -lole32 -luser32 -lgdi32

HWND            g_wnd = nullptr;
HBITMAP         g_hbm = nullptr;
BYTE*           g_bits = nullptr;
LONG            g_bmW = 0, g_bmH = 0;
CRITICAL_SECTION g_cs = {};
volatile bool   g_run = true;

ComPtr<ID3D11Device>        g_dev;
ComPtr<ID3D11DeviceContext> g_ctx;
ComPtr<IDXGIOutputDuplication> g_dup;
UINT g_capW = 0, g_capH = 0;

// ===== capture =====
bool InitCapture() {
    ComPtr<ID3D11Device> dev;
    ComPtr<ID3D11DeviceContext> ctx;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIDevice> dxd;
    dev.As(&dxd);
    ComPtr<IDXGIAdapter> adp;
    dxd->GetAdapter(&adp);
    ComPtr<IDXGIFactory1> fac;
    adp->GetParent(__uuidof(IDXGIFactory1), &fac);

    // find 2nd attached display
    ComPtr<IDXGIAdapter1> ea;
    UINT ai = 0;
    int hit = 0;
    while (fac->EnumAdapters1(ai, &ea) != DXGI_ERROR_NOT_FOUND) {
        ComPtr<IDXGIOutput> out;
        UINT oi = 0;
        while (ea->EnumOutputs(oi, &out) != DXGI_ERROR_NOT_FOUND) {
            DXGI_OUTPUT_DESC d;
            out->GetDesc(&d);
            if (d.AttachedToDesktop) {
                if (hit == 1) {
                    ComPtr<IDXGIOutput1> o1;
                    hr = out.As(&o1);
                    if (FAILED(hr)) return false;
                    hr = o1->DuplicateOutput(dev.Get(), &g_dup);
                    if (FAILED(hr)) return false;
                    g_dev = dev; g_ctx = ctx;
                    DXGI_OUTDUPL_DESC dd;
                    g_dup->GetDesc(&dd);
                    g_capW = dd.ModeDesc.Width;
                    g_capH = dd.ModeDesc.Height;
                    // discard stale frame
                    ComPtr<IDXGIResource> r;
                    DXGI_OUTDUPL_FRAME_INFO fi;
                    hr = g_dup->AcquireNextFrame(50, &fi, &r);
                    if (SUCCEEDED(hr)) g_dup->ReleaseFrame();
                    return true;
                }
                hit++;
            }
            oi++;
        }
        ai++;
    }
    return false;
}

bool CaptureInto(BYTE* dst, LONG stride) {
    if (!g_dup) return false;
    ComPtr<IDXGIResource> res;
    DXGI_OUTDUPL_FRAME_INFO fi;
    HRESULT hr = g_dup->AcquireNextFrame(0, &fi, &res);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return false;
    if (FAILED(hr)) return false;

    ComPtr<ID3D11Texture2D> tex;
    res.As(&tex);
    D3D11_TEXTURE2D_DESC td;
    tex->GetDesc(&td);

    D3D11_TEXTURE2D_DESC sd = td;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> stg;
    hr = g_dev->CreateTexture2D(&sd, nullptr, &stg);
    if (FAILED(hr)) { g_dup->ReleaseFrame(); return false; }

    g_ctx->CopyResource(stg.Get(), tex.Get());
    D3D11_MAPPED_SUBRESOURCE sm;
    hr = g_ctx->Map(stg.Get(), 0, D3D11_MAP_READ, 0, &sm);
    if (SUCCEEDED(hr)) {
        UINT rows = std::min<UINT>(g_capH, td.Height);
        UINT bytes = std::min<UINT>(g_capW, td.Width) * 4;
        LONG dstStride = stride;
        if (dstStride < 0) dstStride = -dstStride;
        // DXGI BGRA -> GDI BGRA: copy rows top-to-bottom
        for (UINT y = 0; y < rows; ++y) {
            memcpy(dst + y * dstStride, (BYTE*)sm.pData + y * sm.RowPitch, bytes);
        }
        g_ctx->Unmap(stg.Get(), 0);
    }
    g_dup->ReleaseFrame();
    return true;
}

// ===== window proc =====
LRESULT CALLBACK WP(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_DESTROY:  g_run = false; PostQuitMessage(0); return 0;
    case WM_KEYDOWN:  if (wp == VK_ESCAPE) DestroyWindow(w); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(w, &ps);
        EnterCriticalSection(&g_cs);
        if (g_hbm) {
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP old = (HBITMAP)SelectObject(mem, g_hbm);
            RECT r; GetClientRect(w, &r);
            SetStretchBltMode(dc, HALFTONE);
            StretchBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, g_bmW, g_bmH, SRCCOPY);
            SelectObject(mem, old);
            DeleteDC(mem);
        }
        LeaveCriticalSection(&g_cs);
        EndPaint(w, &ps);
        return 0;
    }
    }
    return DefWindowProc(w, m, wp, lp);
}

// ===== capture thread =====
DWORD WINAPI CaptureThread(LPVOID) {
    while (g_run) {
        if (!g_dup) { Sleep(100); continue; }

        LONG w = g_bmW, h = g_bmH;
        if (w != (LONG)g_capW || h != (LONG)g_capH) {
            // resize bitmap
            EnterCriticalSection(&g_cs);
            if (g_hbm) DeleteObject(g_hbm);
            w = g_capW; h = g_capH;
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
            bi.bmiHeader.biWidth  = w;
            bi.bmiHeader.biHeight = -h; // top-down
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            HDC dc = GetDC(nullptr);
            g_hbm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&g_bits, nullptr, 0);
            ReleaseDC(nullptr, dc);
            g_bmW = w; g_bmH = h;
            LeaveCriticalSection(&g_cs);
            if (!g_hbm) { Sleep(100); continue; }
        }

        EnterCriticalSection(&g_cs);
        bool got = CaptureInto(g_bits, w * 4);
        LeaveCriticalSection(&g_cs);
        if (got)
            InvalidateRect(g_wnd, nullptr, FALSE);

        Sleep(8); // ~120 fps cap
    }
    return 0;
}

// ===== main =====
int main() {
    printf("=== Virtual Display Live Preview ===\n");

    if (!InitCapture()) {
        printf("Cannot init capture. Run as admin and make sure virtual display is active.\n");
        return 1;
    }
    printf("Virtual display: %ux%u\n", g_capW, g_capH);

    InitializeCriticalSection(&g_cs);

    HINSTANCE hi = GetModuleHandle(nullptr);
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WP;
    wc.hInstance   = hi;
    wc.hCursor     = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"VirtPrevWnd";
    RegisterClassExW(&wc);

    RECT r = { 0, 0, (LONG)g_capW, (LONG)g_capH };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    g_wnd = CreateWindowExW(
        WS_EX_TOPMOST, L"VirtPrevWnd",
        L"Virtual Display  [ESC=close | resize drag]",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, hi, nullptr);
    if (!g_wnd) return 1;

    // Pre-allocate bitmap
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth  = g_capW;
    bi.bmiHeader.biHeight = -(int)g_capH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HDC dc = GetDC(nullptr);
    g_hbm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&g_bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    g_bmW = g_capW; g_bmH = g_capH;

    HANDLE hThread = CreateThread(nullptr, 0, CaptureThread, nullptr, 0, nullptr);

    printf("Running (ESC to quit)...\n");
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_run = false;
    WaitForSingleObject(hThread, 2000);

    if (g_hbm) DeleteObject(g_hbm);
    DeleteCriticalSection(&g_cs);
    return 0;
}
