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
