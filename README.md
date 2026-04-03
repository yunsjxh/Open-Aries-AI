# Aries AI - Windows 智能自动化助手

[![API Support](https://img.shields.io/badge/API-OpenAI%20Compatible-green)](https://openai.com/blog/openai-api)
[![Platform](https://img.shields.io/badge/Platform-Windows-blue)](https://www.microsoft.com/windows)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-orange)](https://isocpp.org/)

Aries AI 是一个基于大语言模型的 Windows 桌面自动化助手。通过截图分析 + AI 决策 + 自动执行，实现自然语言控制电脑操作。

**核心优势**: 支持智谱 AI 和自定义 OpenAI 兼容 API，一键切换不同厂商模型。

## 🌟 核心功能

| 功能 | 描述 |
|------|------|
| **🖼️ 视觉感知** | 自动截取屏幕，通过视觉模型分析当前界面状态 |
| **🧠 智能决策** | AI 根据截图和用户指令，规划下一步操作 |
| **🤖 自动执行** | 模拟鼠标点击、键盘输入、滑动等操作 |
| **📱 应用管理** | 获取已安装应用列表，智能启动应用程序 |
| **💻 命令执行** | 通过 PowerShell 执行系统命令 |
| **🔒 安全存储** | 硬件绑定的 API Key 加密存储 |

## 📋 系统架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                            Aries AI                                 │
├─────────────┬─────────────┬─────────────┬───────────────────────────┤
│   视觉感知   │   AI 决策    │   动作执行   │        应用管理            │
│ (Screenshot)│ (LLM API)   │  (Action    │      (AppManager)         │
│             │             │   Executor) │                           │
└─────────────┴─────────────┴─────────────┴───────────────────────────┘
       │              │              │              │
       ▼              ▼              ▼              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         核心模块                                     │
│  ├─ ai_provider.hpp                 (AI Provider 接口)              │
│  ├─ openai_compatible_provider.hpp  (OpenAI 兼容 Provider)          │
│  ├─ provider_manager.hpp            (提供商管理器)                  │
│  ├─ action_parser.hpp               (动作解析)                      │
│  ├─ action_executor.hpp             (动作执行)                      │
│  ├─ app_manager.hpp                 (应用管理)                      │
│  ├─ prompt_templates.hpp            (提示词模板)                    │
│  └─ secure_storage.hpp              (安全存储)                      │
└─────────────────────────────────────────────────────────────────────┘
```

## 🚀 快速开始

### 环境要求

- Windows 10/11
- 支持 GDI+ 的显卡
- 网络连接

### 首次运行

1. **下载并运行程序**
   ```bash
   aries_agent.exe
   ```

2. **配置 API Key**
   - 程序会显示 ASCII Logo
   - 输入 `key` 打开浏览器获取智谱 AI API Key
   - 或输入 `provider` 配置自定义 API 提供商
   - 或直接输入 API Key

3. **输入任务目标**
   ```
   打开哔哩哔哩
   ```

## 📝 使用指南

### 基本操作

```bash
# 启动程序
aries_agent.exe

# 输入任务目标（示例）
打开记事本
打开计算器计算 123+456
打开浏览器访问 github.com
```

### 特殊命令

| 命令 | 说明 |
|------|------|
| `quit` / `exit` | 退出程序 |
| `clear` | 清除所有保存的 API Key |
| `provider` | 切换 API 提供商 |
| `key` | 打开浏览器获取 API Key |

### 支持的动作类型

| 动作 | 说明 | 示例 |
|------|------|------|
| `Tap` / `Click` | 点击屏幕指定位置 | `do(action="Tap", element="[100,200]")` |
| `Type` | 输入文本 | `do(action="Type", text="Hello")` |
| `Launch` | 启动应用 | `do(action="Launch", app="记事本")` |
| `Execute` | 执行命令 | `do(action="Execute", command="calc")` |
| `Installed` | 获取已安装应用列表 | `do(action="Installed")` |
| `finish` | 任务完成 | `do(action="finish")` |

## ⚙️ 配置说明

### 智谱 AI (默认)

- **Base URL**: `https://open.bigmodel.cn/api/paas/v4`
- **默认模型**: `glm-4.6v-flash`
- **支持视觉**: ✅ 是
- **特点**: 支持推理过程显示

### 自定义 API

支持任何 OpenAI 兼容的 API：

1. 在输入 API Key 时输入 `provider`
2. 输入 API Base URL（如 `https://api.openai.com/v1`）
3. 输入模型名称（如 `gpt-4o`）
4. 输入 API Key

## 📁 文件结构

```
Aries AI/
├── aries_agent.exe          # 主程序
├── aries_agent.cpp          # 主程序源码
├── aries_agent.log          # 运行日志
├── README.md                # 本文件
│
├── ai_provider.hpp          # AI Provider 接口定义
├── openai_compatible_provider.hpp  # OpenAI 兼容 Provider 实现
├── provider_manager.hpp     # 提供商管理器（单例模式）
├── action_parser.hpp        # 动作解析器
├── action_executor.hpp      # 动作执行器
├── app_manager.hpp          # 应用管理器
├── prompt_templates.hpp     # AI 提示词模板
├── secure_storage.hpp       # API Key 安全存储
│
└── .api_key_zhipu_secure    # 加密的 API Key 存储文件
```

## 🔧 编译指南

### 使用 MinGW-w64

```bash
g++ -std=c++17 -o aries_agent.exe aries_agent.cpp ^
    -lgdi32 -luser32 -lgdiplus -lshell32 -lwininet
```

### 使用 Visual Studio

```bash
cl /std:c++17 /EHsc aries_agent.cpp ^
    gdi32.lib user32.lib gdiplus.lib shell32.lib wininet.lib
```

### 依赖库

- `gdi32` - Windows GDI
- `user32` - Windows User API
- `gdiplus` - GDI+ (截图)
- `shell32` - Shell API (启动应用)
- `wininet` - WinINet (HTTP 请求)

## 🎯 工作流程

```
用户输入 → 屏幕截图 → AI 分析 → 动作解析 → 动作执行 → 结果反馈
                ↑___________________________________________↓
```

1. **屏幕捕获** - 使用 Windows GDI+ 截取全屏
2. **AI 分析** - 将截图和用户指令发送给 AI
3. **动作解析** - 解析 AI 返回的动作指令
4. **动作执行** - 执行点击、输入、启动应用等操作
5. **循环迭代** - 根据执行结果继续下一步操作

## 🔐 安全特性

- **硬件绑定加密**: API Key 使用 CPU + 硬盘 + 主板序列号加密
- **安全输入**: 输入 API Key 时显示星号
- **本地存储**: 密钥本地加密存储，不上传云端

## ❓ 常见问题

**Q: 程序启动后闪退？**
A: 检查是否安装了必要的运行库，或查看 `aries_agent.log` 日志文件。

**Q: API Key 验证失败？**
A: 检查网络连接，或尝试使用 `provider` 命令切换其他 API 提供商。

**Q: 无法启动应用？**
A: 某些 UWP 应用可能需要特殊处理，程序会自动尝试多种启动方式。

**Q: 点击坐标不准确？**
A: 程序使用相对坐标 (0-1000) 自动适配屏幕分辨率。

**Q: 遇到"访问量过大"错误？**
A: 程序会提示是否重试，选择 `y` 等待 3 秒后自动重试。

## 📝 更新日志

### v1.0
- ✅ 初始版本发布
- ✅ 支持智谱 AI 和自定义 OpenAI 兼容 API
- ✅ 支持屏幕分析和自动化操作
- ✅ 支持应用管理和启动
- ✅ 支持 API Key 验证和重试机制
- ✅ 硬件绑定加密存储

## 📄 许可证

MIT License

## 🙏 致谢

- [智谱 AI](https://open.bigmodel.cn/) - 提供大模型服务
- [OpenAI](https://openai.com/) - OpenAI 兼容 API 标准
