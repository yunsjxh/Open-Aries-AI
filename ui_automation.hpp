#pragma once

#include <windows.h>
#include <uiautomation.h>
#include <string>
#include <vector>
#include <sstream>
#include <comdef.h>

#pragma comment(lib, "uiautomationcore.lib")

namespace aries {

// 控件信息结构
struct ControlInfo {
    std::string name;
    std::string controlType;
    std::string className;
    std::string automationId;
    RECT boundingRect;
    bool isEnabled;
    bool isOffscreen;
    std::vector<ControlInfo> children;
};

// UI Automation 工具类
class UIAutomationTool {
public:
    UIAutomationTool() : pAutomation_(nullptr), initialized_(false), comInitialized_(false) {}
    
    ~UIAutomationTool() {
        cleanup();
    }
    
    // 初始化 UI Automation
    bool initialize() {
        if (initialized_) return true;
        
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        comInitialized_ = SUCCEEDED(hr);
        
        hr = CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                              IID_IUIAutomation, (void**)&pAutomation_);
        if (FAILED(hr) || !pAutomation_) {
            if (comInitialized_) {
                CoUninitialize();
                comInitialized_ = false;
            }
            return false;
        }
        
        initialized_ = true;
        return true;
    }
    
    // 清理资源
    void cleanup() {
        if (pAutomation_) {
            pAutomation_->Release();
            pAutomation_ = nullptr;
        }
        if (comInitialized_) {
            CoUninitialize();
            comInitialized_ = false;
        }
        initialized_ = false;
    }
    
    // 获取鼠标位置下的控件信息
    std::string getControlAtCursor() {
        if (!initialized_ && !initialize()) {
            return "Error: UI Automation 初始化失败";
        }
        
        POINT pt;
        if (!GetCursorPos(&pt)) {
            return "Error: 无法获取鼠标位置";
        }
        
        IUIAutomationElement* pElement = nullptr;
        HRESULT hr = pAutomation_->GetRootElement(&pElement);
        if (FAILED(hr) || !pElement) {
            return "Error: 无法获取桌面根元素";
        }
        
        IUIAutomationElement* pTarget = nullptr;
        hr = pAutomation_->ElementFromPoint(pt, &pTarget);
        pElement->Release();
        
        if (FAILED(hr) || !pTarget) {
            return "Error: 无法获取鼠标位置下的控件";
        }
        
        ControlInfo info;
        getControlInfo(pTarget, info, 0, 0); // 只获取当前控件
        
        std::string result = formatControlInfo(info);
        pTarget->Release();
        
        return result;
    }
    
    // 获取指定坐标的控件信息
    std::string getControlAtPoint(int x, int y) {
        if (!initialized_ && !initialize()) {
            return "Error: UI Automation 初始化失败";
        }
        
        POINT pt = {x, y};
        
        IUIAutomationElement* pElement = nullptr;
        HRESULT hr = pAutomation_->GetRootElement(&pElement);
        if (FAILED(hr) || !pElement) {
            return "Error: 无法获取桌面根元素";
        }
        
        IUIAutomationElement* pTarget = nullptr;
        hr = pAutomation_->ElementFromPoint(pt, &pTarget);
        pElement->Release();
        
        if (FAILED(hr) || !pTarget) {
            return "Error: 无法获取指定位置的控件";
        }
        
        ControlInfo info;
        getControlInfo(pTarget, info, 0, 0);
        
        std::string result = formatControlInfo(info);
        pTarget->Release();
        
        return result;
    }
    
    // 获取指定窗口的控件树
    std::string getWindowControlTree(const std::string& windowTitle, int maxDepth = 3) {
        if (!initialized_ && !initialize()) {
            return "Error: UI Automation 初始化失败";
        }
        
        IUIAutomationElement* pRoot = nullptr;
        HRESULT hr = pAutomation_->GetRootElement(&pRoot);
        if (FAILED(hr) || !pRoot) {
            return "Error: 无法获取桌面根元素";
        }
        
        // 创建条件：查找指定标题的窗口
        IUIAutomationCondition* pCondition = nullptr;
        std::wstring wTitle(windowTitle.begin(), windowTitle.end());
        BSTR bstrTitle = SysAllocString(wTitle.c_str());
        hr = pAutomation_->CreatePropertyCondition(UIA_NamePropertyId, _variant_t(bstrTitle), &pCondition);
        SysFreeString(bstrTitle);
        
        if (FAILED(hr) || !pCondition) {
            pRoot->Release();
            return "Error: 无法创建查找条件";
        }
        
        // 查找窗口
        IUIAutomationElement* pWindow = nullptr;
        hr = pRoot->FindFirst(TreeScope_Children, pCondition, &pWindow);
        pCondition->Release();
        pRoot->Release();
        
        if (FAILED(hr) || !pWindow) {
            return "Error: 未找到窗口: " + windowTitle;
        }
        
        ControlInfo info;
        getControlInfo(pWindow, info, 0, maxDepth);
        
        std::string result = formatControlTree(info);
        pWindow->Release();
        
        return result;
    }
    
    // 获取当前活动窗口的控件树
    std::string getActiveWindowControlTree(int maxDepth = 3) {
        if (!initialized_ && !initialize()) {
            return "Error: UI Automation 初始化失败";
        }
        
        HWND hwnd = GetForegroundWindow();
        if (!hwnd) {
            return "Error: 无法获取活动窗口";
        }
        
        IUIAutomationElement* pWindow = nullptr;
        HRESULT hr = pAutomation_->ElementFromHandle(hwnd, &pWindow);
        if (FAILED(hr) || !pWindow) {
            return "Error: 无法获取活动窗口的 UI 元素";
        }
        
        ControlInfo info;
        getControlInfo(pWindow, info, 0, maxDepth);
        
        std::string result = formatControlTree(info);
        pWindow->Release();
        
        return result;
    }
    
    // 列出所有顶层窗口
    std::string listTopLevelWindows() {
        if (!initialized_ && !initialize()) {
            return "Error: UI Automation 初始化失败";
        }
        
        IUIAutomationElement* pRoot = nullptr;
        HRESULT hr = pAutomation_->GetRootElement(&pRoot);
        if (FAILED(hr) || !pRoot) {
            return "Error: 无法获取桌面根元素";
        }
        
        // 创建条件：只查找窗口控件
        IUIAutomationCondition* pCondition = nullptr;
        hr = pAutomation_->CreatePropertyCondition(UIA_ControlTypePropertyId, 
            _variant_t((long)UIA_WindowControlTypeId), &pCondition);
        
        if (FAILED(hr) || !pCondition) {
            pRoot->Release();
            return "Error: 无法创建查找条件";
        }
        
        // 查找所有窗口
        IUIAutomationElementArray* pArray = nullptr;
        hr = pRoot->FindAll(TreeScope_Children, pCondition, &pArray);
        pCondition->Release();
        pRoot->Release();
        
        if (FAILED(hr) || !pArray) {
            return "Error: 无法获取窗口列表";
        }
        
        int count = 0;
        pArray->get_Length(&count);
        
        std::ostringstream oss;
        oss << "找到 " << count << " 个顶层窗口:\n";
        
        for (int i = 0; i < count; i++) {
            IUIAutomationElement* pElement = nullptr;
            hr = pArray->GetElement(i, &pElement);
            if (SUCCEEDED(hr) && pElement) {
                BSTR name = nullptr;
                hr = pElement->get_CurrentName(&name);
                if (SUCCEEDED(hr) && name) {
                    std::string windowName = bstrToString(name);
                    if (!windowName.empty()) {
                        oss << (i + 1) << ". " << windowName << "\n";
                    }
                    SysFreeString(name);
                }
                pElement->Release();
            }
        }
        
        pArray->Release();
        return oss.str();
    }
    
    // 查找并点击指定名称的控件
    bool clickControlByName(const std::string& windowTitle, const std::string& controlName) {
        if (!initialized_ && !initialize()) {
            return false;
        }
        
        // 获取窗口
        IUIAutomationElement* pWindow = findWindowByTitle(windowTitle);
        if (!pWindow) {
            return false;
        }
        
        // 在窗口中查找控件
        IUIAutomationCondition* pCondition = nullptr;
        std::wstring wName(controlName.begin(), controlName.end());
        BSTR bstrName = SysAllocString(wName.c_str());
        HRESULT hr = pAutomation_->CreatePropertyCondition(UIA_NamePropertyId, _variant_t(bstrName), &pCondition);
        SysFreeString(bstrName);
        
        if (FAILED(hr) || !pCondition) {
            pWindow->Release();
            return false;
        }
        
        IUIAutomationElement* pControl = nullptr;
        hr = pWindow->FindFirst(TreeScope_Descendants, pCondition, &pControl);
        pCondition->Release();
        pWindow->Release();
        
        if (FAILED(hr) || !pControl) {
            return false;
        }
        
        // 获取控件中心点并点击
        RECT rect;
        hr = pControl->get_CurrentBoundingRectangle(&rect);
        pControl->Release();
        
        if (FAILED(hr)) {
            return false;
        }
        
        int centerX = (rect.left + rect.right) / 2;
        int centerY = (rect.top + rect.bottom) / 2;
        
        // 执行点击
        SetCursorPos(centerX, centerY);
        Sleep(50);
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Sleep(50);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        
        return true;
    }
    
    // 获取控件的中心坐标
    std::pair<int, int> getControlCenter(const std::string& windowTitle, const std::string& controlName) {
        if (!initialized_ && !initialize()) {
            return {-1, -1};
        }
        
        IUIAutomationElement* pWindow = findWindowByTitle(windowTitle);
        if (!pWindow) {
            return {-1, -1};
        }
        
        IUIAutomationCondition* pCondition = nullptr;
        std::wstring wName(controlName.begin(), controlName.end());
        BSTR bstrName = SysAllocString(wName.c_str());
        HRESULT hr = pAutomation_->CreatePropertyCondition(UIA_NamePropertyId, _variant_t(bstrName), &pCondition);
        SysFreeString(bstrName);
        
        if (FAILED(hr) || !pCondition) {
            pWindow->Release();
            return {-1, -1};
        }
        
        IUIAutomationElement* pControl = nullptr;
        hr = pWindow->FindFirst(TreeScope_Descendants, pCondition, &pControl);
        pCondition->Release();
        pWindow->Release();
        
        if (FAILED(hr) || !pControl) {
            return {-1, -1};
        }
        
        RECT rect;
        hr = pControl->get_CurrentBoundingRectangle(&rect);
        pControl->Release();
        
        if (FAILED(hr)) {
            return {-1, -1};
        }
        
        return {(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
    }
    
    // ==================== 窗口操作 ====================
    
    // 查找窗口句柄（通过标题）
    HWND findWindowHandle(const std::string& windowTitle) {
        if (windowTitle.empty()) {
            return GetForegroundWindow();
        }
        
        // 尝试精确匹配
        std::wstring wTitle(windowTitle.begin(), windowTitle.end());
        HWND hwnd = FindWindowW(NULL, wTitle.c_str());
        if (hwnd) return hwnd;
        
        // 尝试部分匹配
        struct EnumParams {
            const std::wstring* title;
            HWND* foundHwnd;
        };
        
        HWND foundHwnd = NULL;
        EnumParams params = {&wTitle, &foundHwnd};
        
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* p = reinterpret_cast<EnumParams*>(lParam);
            wchar_t title[256];
            GetWindowTextW(hwnd, title, 256);
            std::wstring windowTitleStr(title);
            
            // 检查是否包含目标字符串
            if (windowTitleStr.find(*p->title) != std::wstring::npos) {
                *p->foundHwnd = hwnd;
                return FALSE; // 找到，停止枚举
            }
            return TRUE; // 继续枚举
        }, reinterpret_cast<LPARAM>(&params));
        
        return foundHwnd;
    }
    
    // 最小化窗口
    bool minimizeWindow(const std::string& windowTitle) {
        HWND hwnd = findWindowHandle(windowTitle);
        if (!hwnd) {
            return false;
        }
        
        return ShowWindow(hwnd, SW_MINIMIZE) != 0;
    }
    
    // 最大化窗口
    bool maximizeWindow(const std::string& windowTitle) {
        HWND hwnd = findWindowHandle(windowTitle);
        if (!hwnd) {
            return false;
        }
        
        return ShowWindow(hwnd, SW_MAXIMIZE) != 0;
    }
    
    // 还原窗口
    bool restoreWindow(const std::string& windowTitle) {
        HWND hwnd = findWindowHandle(windowTitle);
        if (!hwnd) {
            return false;
        }
        
        return ShowWindow(hwnd, SW_RESTORE) != 0;
    }
    
    // 关闭窗口
    bool closeWindow(const std::string& windowTitle) {
        HWND hwnd = findWindowHandle(windowTitle);
        if (!hwnd) {
            return false;
        }
        
        return PostMessage(hwnd, WM_CLOSE, 0, 0) != 0;
    }
    
    // 设置窗口置顶
    bool setWindowTopmost(const std::string& windowTitle, bool topmost) {
        HWND hwnd = findWindowHandle(windowTitle);
        if (!hwnd) {
            return false;
        }
        
        return SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 
                           0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) != 0;
    }
    
    // 移动窗口
    bool moveWindow(const std::string& windowTitle, int x, int y, int width, int height) {
        HWND hwnd = findWindowHandle(windowTitle);
        if (!hwnd) {
            return false;
        }
        
        return MoveWindow(hwnd, x, y, width, height, TRUE) != 0;
    }
    
    // 获取窗口位置和大小
    std::string getWindowRect(const std::string& windowTitle) {
        HWND hwnd = findWindowHandle(windowTitle);
        if (!hwnd) {
            return "Error: 未找到窗口";
        }
        
        RECT rect;
        if (!GetWindowRect(hwnd, &rect)) {
            return "Error: 无法获取窗口位置";
        }
        
        std::ostringstream oss;
        oss << "窗口位置: (" << rect.left << ", " << rect.top << ")\n";
        oss << "窗口大小: " << (rect.right - rect.left) << " x " << (rect.bottom - rect.top);
        return oss.str();
    }
    
    // 获取窗口状态
    std::string getWindowState(const std::string& windowTitle) {
        HWND hwnd = findWindowHandle(windowTitle);
        if (!hwnd) {
            return "Error: 未找到窗口";
        }
        
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if (!GetWindowPlacement(hwnd, &wp)) {
            return "Error: 无法获取窗口状态";
        }
        
        std::string state;
        switch (wp.showCmd) {
            case SW_SHOWNORMAL: state = "正常"; break;
            case SW_SHOWMINIMIZED: state = "最小化"; break;
            case SW_SHOWMAXIMIZED: state = "最大化"; break;
            default: state = "其他"; break;
        }
        
        std::ostringstream oss;
        oss << "窗口状态: " << state << "\n";
        oss << "是否可见: " << (IsWindowVisible(hwnd) ? "是" : "否") << "\n";
        oss << "是否启用: " << (IsWindowEnabled(hwnd) ? "是" : "否") << "\n";
        oss << "是否置顶: " << (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST ? "是" : "否");
        return oss.str();
    }
    
    // 激活窗口（设置为前台窗口）
    bool activateWindow(const std::string& windowTitle) {
        HWND hwnd = findWindowHandle(windowTitle);
        if (!hwnd) {
            return false;
        }
        
        // 如果窗口最小化，先还原
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        }
        
        return SetForegroundWindow(hwnd) != 0;
    }

private:
    IUIAutomation* pAutomation_;
    bool initialized_;
    bool comInitialized_;
    
    // 将 BSTR 转换为 std::string
    static std::string bstrToString(BSTR bstr) {
        if (!bstr) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, bstr, -1, NULL, 0, NULL, NULL);
        if (len <= 0) return "";
        std::string result(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, bstr, -1, &result[0], len, NULL, NULL);
        return result;
    }
    
    // 获取控件类型名称
    static std::string getControlTypeName(CONTROLTYPEID controlTypeId) {
        switch (controlTypeId) {
            case UIA_ButtonControlTypeId: return "Button";
            case UIA_CalendarControlTypeId: return "Calendar";
            case UIA_CheckBoxControlTypeId: return "CheckBox";
            case UIA_ComboBoxControlTypeId: return "ComboBox";
            case UIA_EditControlTypeId: return "Edit";
            case UIA_HyperlinkControlTypeId: return "Hyperlink";
            case UIA_ImageControlTypeId: return "Image";
            case UIA_ListItemControlTypeId: return "ListItem";
            case UIA_ListControlTypeId: return "List";
            case UIA_MenuControlTypeId: return "Menu";
            case UIA_MenuBarControlTypeId: return "MenuBar";
            case UIA_MenuItemControlTypeId: return "MenuItem";
            case UIA_ProgressBarControlTypeId: return "ProgressBar";
            case UIA_RadioButtonControlTypeId: return "RadioButton";
            case UIA_ScrollBarControlTypeId: return "ScrollBar";
            case UIA_SliderControlTypeId: return "Slider";
            case UIA_SpinnerControlTypeId: return "Spinner";
            case UIA_StatusBarControlTypeId: return "StatusBar";
            case UIA_TabControlTypeId: return "Tab";
            case UIA_TabItemControlTypeId: return "TabItem";
            case UIA_TextControlTypeId: return "Text";
            case UIA_ToolBarControlTypeId: return "ToolBar";
            case UIA_ToolTipControlTypeId: return "ToolTip";
            case UIA_TreeControlTypeId: return "Tree";
            case UIA_TreeItemControlTypeId: return "TreeItem";
            case UIA_CustomControlTypeId: return "Custom";
            case UIA_GroupControlTypeId: return "Group";
            case UIA_ThumbControlTypeId: return "Thumb";
            case UIA_DataGridControlTypeId: return "DataGrid";
            case UIA_DataItemControlTypeId: return "DataItem";
            case UIA_DocumentControlTypeId: return "Document";
            case UIA_SplitButtonControlTypeId: return "SplitButton";
            case UIA_WindowControlTypeId: return "Window";
            case UIA_PaneControlTypeId: return "Pane";
            case UIA_HeaderControlTypeId: return "Header";
            case UIA_HeaderItemControlTypeId: return "HeaderItem";
            case UIA_TableControlTypeId: return "Table";
            case UIA_TitleBarControlTypeId: return "TitleBar";
            case UIA_SeparatorControlTypeId: return "Separator";
            default: return "Unknown";
        }
    }
    
    // 递归获取控件信息
    void getControlInfo(IUIAutomationElement* pElement, ControlInfo& info, int depth, int maxDepth) {
        if (!pElement || depth > maxDepth) return;
        
        HRESULT hr;
        
        // 获取控件名称
        BSTR name = nullptr;
        hr = pElement->get_CurrentName(&name);
        if (SUCCEEDED(hr) && name) {
            info.name = bstrToString(name);
            SysFreeString(name);
        }
        
        // 获取控件类型
        CONTROLTYPEID controlTypeId;
        hr = pElement->get_CurrentControlType(&controlTypeId);
        if (SUCCEEDED(hr)) {
            info.controlType = getControlTypeName(controlTypeId);
        }
        
        // 获取类名
        BSTR className = nullptr;
        hr = pElement->get_CurrentClassName(&className);
        if (SUCCEEDED(hr) && className) {
            info.className = bstrToString(className);
            SysFreeString(className);
        }
        
        // 获取 Automation ID
        BSTR automationId = nullptr;
        hr = pElement->get_CurrentAutomationId(&automationId);
        if (SUCCEEDED(hr) && automationId) {
            info.automationId = bstrToString(automationId);
            SysFreeString(automationId);
        }
        
        // 获取边界矩形
        RECT rect;
        hr = pElement->get_CurrentBoundingRectangle(&rect);
        if (SUCCEEDED(hr)) {
            info.boundingRect = rect;
        }
        
        // 获取启用状态
        BOOL isEnabled = FALSE;
        hr = pElement->get_CurrentIsEnabled(&isEnabled);
        if (SUCCEEDED(hr)) {
            info.isEnabled = (isEnabled == TRUE);
        }
        
        // 获取离屏状态
        BOOL isOffscreen = FALSE;
        hr = pElement->get_CurrentIsOffscreen(&isOffscreen);
        if (SUCCEEDED(hr)) {
            info.isOffscreen = (isOffscreen == TRUE);
        }
        
        // 递归获取子控件
        if (depth < maxDepth) {
            IUIAutomationTreeWalker* pWalker = nullptr;
            HRESULT hr = pAutomation_->get_RawViewWalker(&pWalker);
            if (SUCCEEDED(hr) && pWalker) {
                IUIAutomationElement* pChild = nullptr;
                hr = pWalker->GetFirstChildElement(pElement, &pChild);
                
                while (SUCCEEDED(hr) && pChild) {
                    ControlInfo childInfo;
                    getControlInfo(pChild, childInfo, depth + 1, maxDepth);
                    info.children.push_back(childInfo);
                    
                    IUIAutomationElement* pNext = nullptr;
                    hr = pWalker->GetNextSiblingElement(pChild, &pNext);
                    pChild->Release();
                    pChild = pNext;
                }
                
                pWalker->Release();
            }
        }
    }
    
    // 格式化单个控件信息
    std::string formatControlInfo(const ControlInfo& info) {
        std::ostringstream oss;
        oss << "控件信息:\n";
        oss << "  名称: " << (info.name.empty() ? "(无)" : info.name) << "\n";
        oss << "  类型: " << info.controlType << "\n";
        oss << "  类名: " << (info.className.empty() ? "(无)" : info.className) << "\n";
        oss << "  Automation ID: " << (info.automationId.empty() ? "(无)" : info.automationId) << "\n";
        oss << "  位置: (" << info.boundingRect.left << ", " << info.boundingRect.top
            << ", " << info.boundingRect.right - info.boundingRect.left
            << "x" << info.boundingRect.bottom - info.boundingRect.top << ")\n";
        oss << "  启用状态: " << (info.isEnabled ? "启用" : "禁用") << "\n";
        oss << "  可见性: " << (info.isOffscreen ? "离屏" : "可见") << "\n";
        return oss.str();
    }
    
    // 格式化控件树
    std::string formatControlTree(const ControlInfo& info, int depth = 0) {
        std::ostringstream oss;
        std::string indent(depth * 2, ' ');
        
        oss << indent << "[" << info.controlType << "]";
        
        if (!info.name.empty()) {
            oss << " \"" << info.name << "\"";
        }
        
        oss << " (" << info.boundingRect.left << "," << info.boundingRect.top
            << "-" << info.boundingRect.right << "," << info.boundingRect.bottom << ")";
        
        if (!info.automationId.empty()) {
            oss << " ID:" << info.automationId;
        }
        
        if (!info.isEnabled) {
            oss << " [Disabled]";
        }
        
        if (info.isOffscreen) {
            oss << " [Offscreen]";
        }
        
        oss << "\n";
        
        for (const auto& child : info.children) {
            oss << formatControlTree(child, depth + 1);
        }
        
        return oss.str();
    }
    
    // 根据标题查找窗口
    IUIAutomationElement* findWindowByTitle(const std::string& windowTitle) {
        IUIAutomationElement* pRoot = nullptr;
        HRESULT hr = pAutomation_->GetRootElement(&pRoot);
        if (FAILED(hr) || !pRoot) {
            return nullptr;
        }
        
        IUIAutomationCondition* pCondition = nullptr;
        std::wstring wTitle(windowTitle.begin(), windowTitle.end());
        BSTR bstrTitle = SysAllocString(wTitle.c_str());
        hr = pAutomation_->CreatePropertyCondition(UIA_NamePropertyId, _variant_t(bstrTitle), &pCondition);
        SysFreeString(bstrTitle);
        
        if (FAILED(hr) || !pCondition) {
            pRoot->Release();
            return nullptr;
        }
        
        IUIAutomationElement* pWindow = nullptr;
        hr = pRoot->FindFirst(TreeScope_Children, pCondition, &pWindow);
        pCondition->Release();
        pRoot->Release();
        
        return pWindow;
    }
};

} // namespace aries
