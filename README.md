<div align="center">

# Open Aries AI Desktop

**Windows 原生桌面 AI 助手 — C++17 + WebView2，单文件 ~5MB，零外部依赖**

[![License](https://img.shields.io/github/license/yunsjxh/Open-Aries-AI-Desktop?style=flat-square\&color=blue)](LICENSE)
[![Release](https://img.shields.io/github/v/release/yunsjxh/Open-Aries-AI-Desktop?style=flat-square\&color=green)](https://github.com/yunsjxh/Open-Aries-AI-Desktop/releases/latest)
[![Stars](https://img.shields.io/github/stars/yunsjxh/Open-Aries-AI-Desktop?style=flat-square\&color=yellow)](https://github.com/yunsjxh/Open-Aries-AI-Desktop/stargazers)

[下载最新版](https://github.com/yunsjxh/Open-Aries-AI-Desktop/releases/latest) · [功能介绍](#功能) · [快速开始](#快速开始) · [编译指南](#编译)

</div>

---

## 目录

- [简介](#简介)
- [功能](#功能)
- [快速开始](#快速开始)
- [截图](#截图)
- [编译](#编译)
- [使用指南](#使用指南)
- [项目结构](#项目结构)
- [技术栈](#技术栈)
- [路线图](#路线图)
- [贡献](#贡献)
- [安全](#安全)
- [许可证](#许可证)

---

## 简介

Open Aries AI Desktop 是一款为 Windows 设计的原生桌面 AI 助手。不同于需要浏览器或命令行的方案，它是一个**真正的原生窗口应用**：C++17 底层 + WebView2 渲染引擎，所有资源编译进单个 exe，双击即用，无需安装任何运行时。

> 目标：让 AI 以最低门槛、最高权限、最流畅的体验融入你的 Windows 桌面工作流。

### 核心亮点

| 维度 | 旧版 Open Aries AI | **Open Aries AI Desktop** |
|------|-------------------|---------------------------|
| 运行方式 | 命令行 / 浏览器 localhost | 双击 exe，原生窗口 |
| 文件体积 | 多文件 + 外部依赖 | **单文件 ~5MB** |
| 工具调用 | 文本解析（固定格式）| OpenAI 原生 Function Calling |
| 权限控制 | 无 | allow / deny / ask 规则引擎 |
| 离线可用 | 依赖 CDN | **完全离线** |

---

## 功能

- **原生桌面** — 单文件 exe，双击即用，WebView2 DLL 内嵌，不依赖外部 Runtime
- **多模型兼容** — OpenAI / DeepSeek / SiliconFlow / MiniMax 等全部 OpenAI 格式 API
- **Agent 自主调用** — AI 可自主读写文件、执行 PowerShell、管理进程、截取窗口等 16 个系统工具
- **MCP 协议扩展** — 支持 Model Context Protocol stdio 传输，接入外部工具服务器
- **流式 SSE 输出** — 实时显示 AI 推理过程和回复，支持 `<think>` 思考链展示
- **视觉识别** — 截图分析，多模态请求（需视觉模型支持）
- **多会话管理** — 新建 / 切换 / 重命名 / 删除对话，历史持久化到本地 JSON
- **文件附件** — 拖拽文件添加到对话上下文（最大 500KB / 文件）
- **系统托盘通知** — AI 回复完成或需确认时弹出托盘气泡，点击直接聚焦窗口
- **暗色主题** — 护眼暗色配色
- **权限系统** — 规则引擎 allow / deny / ask + 通配符匹配，操作确认与记忆
- **路径安全** — 文件操作白名单 + 黑名单校验，防路径遍历
- **自适应窗口** — 四边四角自由拖动调整大小，最小宽度 480px

### Agent 工具（16 个）

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

---

## 快速开始

### 1. 下载

从 [Releases](https://github.com/yunsjxh/Open-Aries-AI-Desktop/releases/latest) 下载 `Open Aries AI.exe`。

### 2. 配置

首次启动自动弹出设置页，填入你的 API 信息：

```
API 地址: https://api.siliconflow.cn/v1
API 密钥: 你的密钥
模型:     Pro/moonshotai/Kimi-K2.6
```

或手动创建同目录下的 `config.txt`：

```ini
api_host=https://api.siliconflow.cn/v1
api_key=sk-xxxxxxxxxxxxxxxxxxxxxxxx
model=Pro/moonshotai/Kimi-K2.6
```

### 3. 使用

- **发送消息** — 输入框输入内容，按 `Enter` 发送，`Shift+Enter` 换行
- **Agent 模式** — 点击输入框下方「Agent 模式」按钮，AI 将自主调用工具完成任务
- **新建会话** — 点击侧边栏「+ 新对话」
- **附件** — 拖拽文件到聊天区域，自动添加到上下文

---

## 截图

![主界面](https://open-aries-ai.xn--9kq396ceqaq4si9m.cn/main.png)

*暗色主题主界面，左侧会话列表，右侧聊天区域，底部 Agent 模式开关。*

---

## 编译

### 依赖

| 工具 | 版本 | 说明 |
|------|------|------|
| MinGW-w64 | GCC 15+ | MSYS2 UCRT64 环境 |
| WebView2Loader.dll | — | 从 [NuGet](https://www.nuget.org/packages/Microsoft.Web.WebView2) 下载 |
| Python 3 | — | 仅用于生成 `ui_html.h` |

### 构建步骤

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

编译完成后，当前目录会生成 `Open Aries AI.exe`，可直接运行。

---

## 使用指南

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

### 权限规则

首次触发敏感操作时，会弹出确认对话框。你可以选择：

- **允许一次** — 仅本次生效
- **总是允许** — 写入规则文件，永久生效
- **拒绝** — 阻止操作

规则存储在 `permissions.txt`，支持通配符匹配路径和命令。

### 配置文件

首次启动后自动生成 `config.txt`（参考 `config.example.txt`）。

---

## 项目结构

```
├── main.cpp                    # 主程序 — Win32窗口 + WebView2 + Agent循环 + 工具
├── index.html                  # 旧版全项目介绍页（CLI + Web + Desktop）
├── index-new.html              # Desktop 版专属介绍页（零外部依赖）
├── 界面.html                   # WebView2 内嵌 UI（聊天界面）
├── ui_html.h                   # 由 gen_ui_html.py 生成的 C 头文件
├── gen_ui_html.py              # HTML 转 C++ 字符串脚本
├── resource.rc                 # 图标 + 版本信息资源
├── app_icon.ico / .png         # 应用图标
├── WebView2Loader.dll          # WebView2 运行时（构建时嵌入 exe）
├── config.example.txt          # 配置文件模板
├── mcp_servers.json            # MCP 服务器配置
│
├── include/
│   ├── ai_provider.hpp                # AI Provider 抽象接口
│   ├── openai_compatible_provider.hpp # OpenAI 兼容实现（SSE流式 + 多模态）
│   ├── security_config.hpp           # 路径安全校验（黑白名单 + 遍历检测）
│   ├── tool_system.hpp               # 工具注册/执行/截断框架
│   ├── permission_system.hpp         # 权限规则引擎（allow/deny/ask + 通配符）
│   └── mcp_client.hpp                # MCP 协议客户端（stdio 传输）
│
└── demo/
    ├── mcp_server.cpp           # MCP 桌面控制服务器（DXGI 桌面复制）
    ├── mcp_server.exe           # 编译好的 MCP 服务器
    ├── preview.cpp              # 屏幕预览工具源码
    └── build.bat                # demo 构建脚本
```

---

## 技术栈

| 层级 | 技术 |
|------|------|
| 语言 | C++17 |
| UI 框架 | Win32 + WebView2 (Edge Chromium) |
| AI 接口 | OpenAI 兼容 API (SSE 流式 + Function Calling) |
| 协议 | JSON-RPC 2.0 (MCP stdio 传输) |
| 图像 | GDI+ (截图 / PNG 编码) |
| HTTP | WinINet |
| 构建 | MinGW-w64, windres, objcopy |

---

## 路线图

- [x] 原生 Win32 窗口 + WebView2
- [x] OpenAI 兼容 API + SSE 流式
- [x] 16 个系统工具（Agent 模式）
- [x] MCP stdio 协议支持
- [x] 权限规则引擎
- [x] 多会话持久化
- [x] 系统托盘通知
- [x] `<think>` 推理标签支持
- [ ] 浅色主题
- [ ] 全局快捷键（呼出/隐藏窗口）
- [ ] 插件系统（DLL 扩展）
- [ ] 本地模型支持（llama.cpp / ollama）
- [ ] 国际化（i18n）

---

## 贡献

欢迎提交 Issue 和 PR！

1. Fork 本仓库
2. 创建功能分支 (`git checkout -b feat/xxx`)
3. 提交更改 (`git commit -m 'feat: add xxx'`)
4. 推送到分支 (`git push origin feat/xxx`)
5. 创建 Pull Request

请确保代码符合项目现有风格，并在可能的情况下添加测试。

---

## 安全

本项目涉及文件系统访问和进程管理，已内置多层安全防护：

- **路径遍历检测** — 所有文件操作路径经过规范化与校验
- **白名单/黑名单** — 可配置允许或禁止访问的目录
- **权限确认** — 敏感操作（PowerShell、终止进程）默认需要用户确认
- **沙盒意识** — 即使 Agent 模式开启，用户仍保有最终控制权

发现安全漏洞请通过 [GitHub Security Advisories](https://github.com/yunsjxh/Open-Aries-AI-Desktop/security/advisories) 私下报告。

---

## 许可证

[MIT](LICENSE) © yunsjxh
