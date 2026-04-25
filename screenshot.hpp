#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <string>

inline void get_screen_size(int& width, int& height) {
    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);
}

inline bool save_bitmap_to_png(HBITMAP hBitmap, const std::string& filepath) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    bool result = false;
    {
        Gdiplus::Bitmap bitmap(hBitmap, NULL);
        CLSID pngClsid = {0};
        bool foundEncoder = false;
        
        UINT numEncoders = 0;
        UINT size = 0;
        Gdiplus::GetImageEncodersSize(&numEncoders, &size);
        if (size > 0) {
            Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)malloc(size);
            if (pImageCodecInfo) {
                Gdiplus::GetImageEncoders(numEncoders, size, pImageCodecInfo);
                
                for (UINT i = 0; i < numEncoders; i++) {
                    if (wcscmp(pImageCodecInfo[i].MimeType, L"image/png") == 0) {
                        pngClsid = pImageCodecInfo[i].Clsid;
                        foundEncoder = true;
                        break;
                    }
                }
                free(pImageCodecInfo);
            }
        }
        
        if (foundEncoder) {
            std::wstring wfilepath(filepath.begin(), filepath.end());
            result = (bitmap.Save(wfilepath.c_str(), &pngClsid, NULL) == Gdiplus::Ok);
        }
    }
    
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return result;
}

inline bool capture_screen(const std::string& filepath) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenWidth, screenHeight);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
    
    BitBlt(hMemoryDC, 0, 0, screenWidth, screenHeight, hScreenDC, 0, 0, SRCCOPY);
    
    bool result = save_bitmap_to_png(hBitmap, filepath);
    
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
    
    return result;
}

// 捕获压缩后的屏幕截图（用于视觉模型转述，减小文件大小）
inline bool capture_screen_compressed(const std::string& filepath, int maxWidth = 1280) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 计算缩放比例
    float scale = 1.0f;
    if (screenWidth > maxWidth) {
        scale = (float)maxWidth / screenWidth;
    }
    
    int newWidth = (int)(screenWidth * scale);
    int newHeight = (int)(screenHeight * scale);
    
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HDC hTempDC = CreateCompatibleDC(hScreenDC);
    
    // 创建原始大小的位图
    HBITMAP hOriginalBitmap = CreateCompatibleBitmap(hScreenDC, screenWidth, screenHeight);
    HBITMAP hOldOriginal = (HBITMAP)SelectObject(hMemoryDC, hOriginalBitmap);
    
    // 捕获屏幕
    BitBlt(hMemoryDC, 0, 0, screenWidth, screenHeight, hScreenDC, 0, 0, SRCCOPY);
    
    // 创建缩放后的位图
    HBITMAP hScaledBitmap = CreateCompatibleBitmap(hScreenDC, newWidth, newHeight);
    HBITMAP hOldScaled = (HBITMAP)SelectObject(hTempDC, hScaledBitmap);
    
    // 缩放图像
    SetStretchBltMode(hTempDC, HALFTONE);
    StretchBlt(hTempDC, 0, 0, newWidth, newHeight, hMemoryDC, 0, 0, screenWidth, screenHeight, SRCCOPY);
    
    // 保存为 PNG
    bool result = save_bitmap_to_png(hScaledBitmap, filepath);
    
    // 清理
    SelectObject(hTempDC, hOldScaled);
    SelectObject(hMemoryDC, hOldOriginal);
    DeleteObject(hScaledBitmap);
    DeleteObject(hOriginalBitmap);
    DeleteDC(hTempDC);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
    
    return result;
}
