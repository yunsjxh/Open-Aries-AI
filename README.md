# Open-Aries-AI - Windows 智能自动化助手

[![API Support](https://img.shields.io/badge/API-OpenAI%20Compatible-green)](https://openai.com/blog/openai-api)
[![Platform](https://img.shields.io/badge/Platform-Windows-blue)](https://www.microsoft.com/windows)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-orange)](https://isocpp.org/)

Open-Aries-AI 是一个基于大语言模型的 Windows 桌面自动化助手。通过截图分析 + AI 决策 + 自动执行，实现自然语言控制电脑操作。

**核心优势**: 支持智谱 AI 和自定义 OpenAI 兼容 API，一键切换不同厂商模型；支持中文输入和文件管理；完整的动作历史反馈机制。

## 🌟 核心功能

| 功能 | 描述 |
|------|------|
| **🖼️ 视觉感知** | 自动截取屏幕，通过视觉模型分析当前界面状态 |
| **🧠 智能决策** | AI 根据截图和用户指令，规划下一步操作 |
| **🤖 自动执行** | 模拟鼠标点击、键盘输入、滑动等操作 |
| **📱 应用管理** | 获取已安装应用列表，智能启动应用程序 |
| **📁 文件管理** | 完整的文件操作：读取、写入、搜索、执行文件（支持Unicode路径） |
| **💻 命令执行** | 通过 PowerShell 执行系统命令 |
| **🔒 安全存储** | 硬件绑定的 API Key 加密存储 |
| **📝 中文支持** | 完美支持中文输入和文件路径 |
| **📊 动作历史** | 完整的动作历史记录和反馈，避免重复操作 |
| **🔄 智能总结** | 第5次迭代自动提示AI进行阶段性总结 |

## 📋 系统架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          Open-Aries-AI                              │
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
│  ├─ file_manager.hpp                (文件管理器)                    │
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
   用哔哩哔哩打开影视飓风的最新视频
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
用哔哩哔哩搜索影视飓风
在D盘创建一个test文件夹
总结C:\Users\Username\Documents\project
```

### 特殊命令

| 命令 | 说明 |
|------|------|
| `quit` / `exit` | 退出程序 |
| `clear` | 清除所有保存的 API Key |
| `provider` | 切换 API 提供商（支持多提供商配置） |
| `key` | 打开浏览器获取 API Key |

### 支持的动作类型

#### 基础操作

| 动作 | 说明 | 示例 |
|------|------|------|
| `Tap` / `Click` | 点击屏幕指定位置 | `do(action="Tap", element=[100,200])` |
| `RightClick` | 右键点击 | `do(action="RightClick", element=[500,300])` |
| `Type` | 输入文本（支持中文） | `do(action="Type", text="影视飓风")` |
| `Swipe` | 滑动屏幕 | `do(action="Swipe", start=[500,800], end=[500,200])` |
| `Back` | 返回上一级 | `do(action="Back")` |
| `Home` | 回到主页 | `do(action="Home")` |
| `Wait` | 等待页面加载 | `do(action="Wait", duration=3)` |
| `Take_over` | 请求用户接管 | `do(action="Take_over", message="请手动完成")` |

#### 应用管理

| 动作 | 说明 | 示例 |
|------|------|------|
| `Launch` | 启动应用 | `do(action="Launch", app="记事本")` |
| `Installed` | 获取已安装应用列表 | `do(action="Installed")` |
| `Execute` | 执行 PowerShell 命令 | `do(action="Execute", command="calc")` |
| `finish` | 任务完成 | `finish(message="任务完成")` |

#### 文件管理

| 动作 | 说明 | 示例 |
|------|------|------|
| `FileList` | 列出目录内容 | `do(action="FileList", path="C:\\Users")` |
| `FileRead` | 读取文件内容 | `do(action="FileRead", path="test.txt")` |
| `FileHead` | 读取文件前N行 | `do(action="FileHead", path="log.txt", lines=50)` |
| `FileTail` | 读取文件后N行 | `do(action="FileTail", path="log.txt", lines=50)` |
| `FileRange` | 读取文件指定行 | `do(action="FileRange", path="code.cpp", start=10, end=20)` |
| `FileWrite` | 写入文件 | `do(action="FileWrite", path="test.txt", content="Hello")` |
| `FileAppend` | 追加到文件 | `do(action="FileAppend", path="log.txt", content="New line")` |
| `FileMkdir` | 创建目录 | `do(action="FileMkdir", path="NewFolder")` |
| `FileDelete` | 删除文件 | `do(action="FileDelete", path="old.txt")` |
| `FileMove` | 移动文件 | `do(action="FileMove", source="a.txt", destination="b.txt")` |
| `FileCopy` | 复制文件 | `do(action="FileCopy", source="a.txt", destination="b.txt")` |
| `FileInfo` | 获取文件信息 | `do(action="FileInfo", path="file.exe")` |
| `FileSearch` | 搜索文件 | `do(action="FileSearch", path=".", pattern="*.txt")` |
| `FileTree` | 获取目录树 | `do(action="FileTree", path=".", depth=3)` |
| `FileRun` | 执行可执行文件 | `do(action="FileRun", path="app.exe")` |

## ⚙️ 配置说明

### 智谱 AI (默认)

- **Base URL**: `https://open.bigmodel.cn/api/paas/v4`
- **默认模型**: `glm-4.6v-flash`
- **支持视觉**: ✅ 是
- **特点**: 支持推理过程显示

### 自定义 API

支持任何 OpenAI 兼容的 API：

1. 在输入 API Key 时输入 `provider`
2. 选择或添加自定义提供商
3. 输入 API Base URL（如 `https://api.siliconflow.cn/v1`）
4. 输入模型名称（如 `zai-org/GLM-4.6V`）
5. 输入 API Key

**支持多提供商配置**: 可以保存多个自定义 API 配置，随时切换使用。

## 📁 文件结构

```
Open-Aries-AI/
├── aries_agent.exe          # 主程序
├── aries_agent.cpp          # 主程序源码
├── README.md                # 本文件
│
├── ai_provider.hpp          # AI Provider 接口定义
├── openai_compatible_provider.hpp  # OpenAI 兼容 Provider 实现
├── provider_manager.hpp     # 提供商管理器（单例模式）
├── action_parser.hpp        # 动作解析器
├── action_executor.hpp      # 动作执行器
├── app_manager.hpp          # 应用管理器
├── file_manager.hpp         # 文件管理器
├── prompt_templates.hpp     # AI 提示词模板
├── secure_storage.hpp       # API Key 安全存储
│
└── .api_key_*_secure        # 加密的 API Key 存储文件
```

## 🔧 编译指南

### 使用 MinGW-w64

```bash
g++ -std=c++17 aries_agent.cpp -o aries_agent.exe ^
    -lgdiplus -lgdi32 -lws2_32 -lcrypt32 -lwininet
```

### 使用 Visual Studio

```bash
cl /std:c++17 /EHsc aries_agent.cpp ^
    gdiplus.lib gdi32.lib ws2_32.lib crypt32.lib wininet.lib
```

### 依赖库

- `gdiplus` - GDI+ (截图)
- `gdi32` - Windows GDI
- `ws2_32` - Winsock (网络)
- `crypt32` - 加密 API
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
5. **动作历史反馈** - 将所有动作和思考过程反馈给 AI，避免重复操作
6. **智能总结** - 第5次迭代时自动提示AI进行阶段性总结
7. **循环迭代** - 根据执行结果继续下一步操作

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
A: 程序会提示是否重试，选择 `y` 等待 1 秒后自动重试。

**Q: 中文输入不成功？**
A: 程序已支持中文输入，请确保使用的是最新版本。

**Q: 文件读取失败？**
A: 程序会显示具体错误原因（文件不存在/无权限/被占用等），AI会根据错误信息调整策略。

## 📝 更新日志

### v1.2 (当前版本)
- ✅ 完整的动作历史记录（保存所有历史，显示最近5次）
- ✅ 第5次迭代自动提示AI进行阶段性总结
- ✅ 文件操作详细的错误原因判断（不存在/无权限/被占用等）
- ✅ 执行失败时反馈给AI而不是直接退出
- ✅ 统一动作协议（所有操作通过action_executor处理）
- ✅ 修复迭代计数器负数问题
- ✅ 修复UTF-8编码错误

### v1.1
- ✅ 新增文件管理功能（14个文件操作命令）
- ✅ 新增右键点击功能
- ✅ 支持中文输入（修复Unicode输入问题）
- ✅ 支持多提供商配置和切换
- ✅ 动作历史反馈（避免重复操作）
- ✅ 智能应用启动（自动检测多可执行文件）
- ✅ 改进的错误处理和重试机制
- ✅ 迭代间隔优化为1秒

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
- [Aries AI](https://github.com/ZG0704666/Aries-AI) - 本软件参考了该项目的设计思路和实现方案
