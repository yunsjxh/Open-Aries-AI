# Open Aries AI

Windows 桌面 AI 助手，基于 C++17 + WebView2 原生开发。支持多模型对话、Agent 自主工具调用、MCP 协议扩展、主题切换。

## 功能

- **多模型兼容** — OpenAI / DeepSeek / SiliconFlow 等兼容 API
- **Agent 模式** — AI 自主调用系统工具（文件读写、PowerShell、进程管理、窗口截图等 15+ 工具）
- **MCP 协议** — 支持 Model Context Protocol，可接入外部工具服务器
- **流式输出** — 实时 SSE 流式显示，支持思考过程展示
- **视觉识别** — 截图分析（需视觉模型支持）
- **多会话** — 新建/切换对话，历史持久化到本地 JSON
- **文件附件** — 拖拽文件添加到对话上下文
- **暗色主题** — Aries AI Android 项目同款暗色配色，一键切换
- **权限系统** — 规则引擎 allow/deny/ask，支持记住选择
- **路径安全** — 文件操作白名单+黑名单校验，防路径遍历
- **自适应窗口** — 四边四角自由拖动调整大小

## 编译

### 依赖

- **MinGW-w64** (GCC 15+)
- **WebView2 Runtime** (Windows 10/11 自带)

### 构建

```bash
# 1. 生成内嵌 HTML
echo '#pragma once' > ui_html.h
echo '' >> ui_html.h
echo -n 'static const char g_htmlUI[] = R"HTML(' >> ui_html.h
cat 界面.html >> ui_html.h
echo ')HTML";' >> ui_html.h

# 2. 编译链接
g++ -std=c++17 -I./include main.cpp webview2_dll.o \
    -o "Open Aries AI.exe" \
    -mwindows -ldwmapi -lgdiplus -lole32 -lshlwapi \
    -lwininet -lcomctl32 -luuid -static
```

`webview2_dll.o` 由 `WebView2Loader.dll` 通过 objcopy 嵌入：

```bash
objcopy -I binary -O pe-x86-64 -B i386:x86-64 \
    WebView2Loader.dll webview2_dll.o
```

## 使用

1. 启动 `Open Aries AI.exe`
2. 首次启动自动弹出设置页，填入 API 地址和 Key
3. 输入框输入问题发送
4. 点击输入框下方「Agent 模式」按钮切换自主工具调用

### 配置文件

`config.txt`（自动生成）：

```
api_host=https://api.siliconflow.cn/v1
api_key=sk-your-key-here
model=Pro/moonshotai/Kimi-K2.6
```

### MCP 服务器

`mcp_servers.json`（设置 → MCP 标签页图形化管理）：

```json
{
  "servers": [
    {
      "label": "desktop",
      "command": "demo\\mcp_server.exe"
    }
  ]
}
```

### Agent 工具

| 工具 | 功能 | 权限 |
|------|------|------|
| `READ_FILE` | 读取文件内容 | read |
| `WRITE_FILE` | 写入文件 | write |
| `COPY_FILE` | 复制文件 | write |
| `DELETE_FILE` | 删除文件/目录 | delete |
| `MOVE_FILE` | 移动/重命名文件 | write |
| `LIST_DIR` | 列出目录 | read |
| `LIST_APPS` | 列出已安装应用 | read |
| `OPEN_APP` | 启动应用 | execute |
| `UNINSTALL_APP` | 卸载应用 | execute |
| `OPEN_APP_LOCATION` | 打开应用所在文件夹 | read |
| `RUN_PS` | 执行 PowerShell（需确认） | execute |
| `LIST_PROCESSES` | 列出进程 | read |
| `KILL_PROCESS` | 终止进程（需确认） | execute |
| `GET_FOREGROUND_WINDOW` | 获取前台窗口信息 | read |
| `LIST_WINDOWS` | 列出所有可见窗口 | read |
| `CAPTURE_WINDOW` | 截取窗口画面 | read |

## 项目结构

```
├── main.cpp                    # 主程序 — Win32 窗口 + WebView2 + Agent 循环
├── 界面.html                   # WebView2 UI（可直接浏览器调试）
├── ui_html.h                   # 内嵌 HTML（由 界面.html 生成）
├── WebView2Loader.dll          # WebView2 运行时加载器
├── webview2_dll.o              # 嵌入的 DLL 对象文件
├── config.txt                  # 用户 API 配置
├── mcp_servers.json            # MCP 服务器配置
│
├── include/
│   ├── ai_provider.hpp                # AI Provider 接口
│   ├── openai_compatible_provider.hpp # OpenAI 兼容实现 + SSE 流式
│   ├── security_config.hpp           # 路径安全校验
│   ├── tool_system.hpp               # 工具注册/执行/截断框架
│   ├── permission_system.hpp         # 权限规则引擎
│   └── mcp_client.hpp                # MCP 协议客户端
│
└── demo/
    ├── mcp_server.cpp           # MCP 桌面控制服务器源码
    ├── mcp_server.exe           # 编译好的 MCP 服务器
    ├── preview.cpp              # 屏幕预览工具源码
    └── build.bat                # demo 构建脚本
```

## 技术栈

- C++17 + Win32 API
- WebView2 (Edge Chromium)
- OpenAI 兼容 API (SSE 流式)
- JSON-RPC 2.0 (MCP 协议)
- GDI+ (截图/图像)
- WinINet (HTTP 客户端)

## License

MIT
