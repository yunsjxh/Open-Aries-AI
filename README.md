# Open Aries AI Desktop

Windows 桌面 AI 助手 — C++17 + WebView2 原生应用，单文件 4.8MB，零外部依赖。支持多模型对话、Agent 自主工具调用、MCP 协议扩展。

## 功能

- **原生桌面** — 38MB 单文件 exe，双击即用，WebView2 DLL 内嵌，不依赖外部 Runtime
- **多模型兼容** — OpenAI / DeepSeek / SiliconFlow 等兼容 API
- **Agent 模式** — AI 自主调用 16 个系统工具（文件读写、PowerShell、进程管理、窗口截图等）
- **MCP 协议** — 支持 Model Context Protocol stdio 传输，可接入外部工具服务器
- **流式输出** — 实时 SSE 流式显示，支持思考过程展示
- **视觉识别** — 截图分析，多模态请求（需视觉模型支持）
- **多会话** — 新建/切换对话，历史持久化到本地 JSON
- **文件附件** — 拖拽文件添加到对话上下文（最大 500KB/文件）
- **暗色主题** — 护眼暗色配色
- **权限系统** — 规则引擎 allow/deny/ask + 通配符匹配，操作确认与记忆
- **路径安全** — 文件操作白名单+黑名单校验，防路径遍历
- **系统通知** — AI 回复完成或需确认时弹出托盘气泡，点击直接聚焦窗口
- **自适应窗口** — 四边四角自由拖动调整大小，最小宽度 480px

## 编译

### 依赖

- **MinGW-w64** (GCC 15+，MSYS2 UCRT64 环境)
- **WebView2Loader.dll** (从 [NuGet](https://www.nuget.org/packages/Microsoft.Web.WebView2) 下载)

### 构建

```bash
# 1. 编译资源文件（图标 + 版本信息）
windres resource.rc resource.o

# 2. 嵌入 WebView2Loader.dll
objcopy --input-format binary --output-format pe-x86-64 \
    --binary-architecture i386:x86-64 \
    WebView2Loader.dll webview2_dll.o

# 3. 生成内嵌 HTML（仅当 界面.html 有修改时）
python gen_ui_html.py

# 4. 静态编译链接
g++ -O2 -std=c++17 -static-libgcc -static-libstdc++ \
    -Iinclude main.cpp resource.o webview2_dll.o \
    -o "Open Aries AI.exe" \
    -lgdiplus -lcomctl32 -ldwmapi -lole32 -luuid \
    -lshlwapi -lshell32 -lwininet -mwindows
```

## 使用

1. 双击 `Open Aries AI.exe` 启动
2. 首次启动自动弹出设置页，填入 API 地址和 Key
3. 输入框输入问题发送，Enter 发送，Shift+Enter 换行
4. 点击输入框下方「Agent 模式」按钮切换自主工具调用模式

### 配置文件

首次启动后自动生成 `config.txt`（参考 `config.example.txt`）：

```
api_host=https://api.siliconflow.cn/v1
api_key=你的API密钥
model=Pro/moonshotai/Kimi-K2.6
```

### MCP 服务器

`mcp_servers.json`（设置页图形化管理，或手动编辑）：

```json
{
  "servers": [
    {
      "label": "desktop-control",
      "command": "demo\\mcp_server.exe"
    }
  ]
}
```

### Agent 工具 (16个)

| 工具 | 功能 | 需确认 |
|------|------|--------|
| `READ_FILE` | 读取文件内容 | — |
| `WRITE_FILE` | 写入文件 | — |
| `COPY_FILE` | 复制文件 | — |
| `DELETE_FILE` | 删除文件/目录 | — |
| `MOVE_FILE` | 移动/重命名文件 | — |
| `LIST_DIR` | 列出目录内容 | — |
| `LIST_APPS` | 列出已安装应用 | — |
| `OPEN_APP` | 启动应用 | — |
| `UNINSTALL_APP` | 卸载应用 | — |
| `OPEN_APP_LOCATION` | 打开应用所在文件夹 | — |
| `RUN_PS` | 执行 PowerShell | 是 |
| `LIST_PROCESSES` | 列出运行中进程 | — |
| `KILL_PROCESS` | 终止进程 | 是 |
| `GET_FOREGROUND_WINDOW` | 获取前台窗口信息 | — |
| `LIST_WINDOWS` | 列出所有可见窗口 | — |
| `CAPTURE_WINDOW` | 截取窗口画面 | — |

## 项目结构

```
├── main.cpp                    # 主程序 — Win32窗口 + WebView2 + Agent循环 + 工具
├── index.html                  # 旧版全项目介绍页（CLI + Web + Desktop）
├── index-new.html              # Desktop 版专属介绍页（零外部依赖）
├── 界面.html                   # WebView2 内嵌 UI（聊天界面）
├── ui_html.h                   # 由 界面.html 生成的 C 头文件
├── resource.rc                 # 图标 + 版本信息资源
├── app_icon.ico / .png         # 应用图标
├── gen_ui_html.py              # 生成 ui_html.h 的脚本
├── WebView2Loader.dll          # WebView2 运行时（构建时嵌入 exe）
├── config.example.txt          # 配置文件模板
├── mcp_servers.json            # MCP 服务器配置
│
├── include/
│   ├── ai_provider.hpp                # AI Provider 抽象接口
│   ├── openai_compatible_provider.hpp # OpenAI 兼容实现（SSE流式 + 多模态）
│   ├── security_config.hpp           # 路径安全校验（黑白名单 + 遍历检测）
│   ├── tool_system.hpp               # 工具注册/执行/截断框架（OpenCode 风格）
│   ├── permission_system.hpp         # 权限规则引擎（allow/deny/ask + 通配符）
│   └── mcp_client.hpp                # MCP 协议客户端（stdio 传输）
│
└── demo/
    ├── mcp_server.cpp           # MCP 桌面控制服务器（DXGI 桌面复制）
    ├── mcp_server.exe           # 编译好的 MCP 服务器
    ├── preview.cpp              # 屏幕预览工具源码
    └── build.bat                # demo 构建脚本
```

## 技术栈

- **语言**: C++17
- **UI**: Win32 + WebView2 (Edge Chromium)
- **AI**: OpenAI 兼容 API (SSE 流式 + Function Calling)
- **协议**: JSON-RPC 2.0 (MCP stdio 传输)
- **图像**: GDI+ (截图 / PNG 编码)
- **HTTP**: WinINet

## License

MIT
