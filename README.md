# Aries AI - Windows 智能自动化助手

[![API Support](https://img.shields.io/badge/API-OpenAI%20Compatible-green)](https://openai.com/blog/openai-api)
[![Platform](https://img.shields.io/badge/Platform-Windows-blue)](https://www.microsoft.com/windows)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-orange)](https://isocpp.org/)

Aries AI 是一个基于大语言模型的 Windows 桌面自动化助手。通过截图分析 + AI 决策 + 自动执行，实现自然语言控制电脑操作。

**核心优势**: 支持几乎所有主流大模型厂商（OpenAI、DeepSeek、阿里云、智谱等），基于标准 OpenAI API 格式，一键切换不同厂商模型。

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
│  ├─ silicon_flow_client_simple.hpp  (OpenAI 兼容 API 客户端)        │
│  │   ├─ 支持 Silicon Flow / DeepSeek / 阿里云 / 智谱 / OpenAI...    │
│  │   └─ 一键切换厂商，无需修改代码                                  │
│  ├─ action_parser.hpp               (动作解析)                      │
│  ├─ action_executor.hpp             (动作执行)                      │
│  ├─ app_manager.hpp                 (应用管理)                      │
│  ├─ prompt_templates.hpp            (提示词模板)                    │
│  └─ secure_storage.hpp              (安全存储)                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 多厂商 API 支持

基于 **OpenAI 兼容 API 标准**，支持以下厂商：

```
┌─────────────────────────────────────────────────────────────┐
│                    OpenAI Compatible API                    │
│                      (统一接口格式)                          │
├─────────────┬─────────────┬─────────────┬───────────────────┤
│  Silicon    │  阿里云     │   DeepSeek  │     OpenAI        │
│   Flow      │  百炼       │             │                   │
├─────────────┼─────────────┼─────────────┼───────────────────┤
│   智谱 AI   │  百度千帆   │   MiniMax   │   Azure OpenAI    │
├─────────────┼─────────────┼─────────────┼───────────────────┤
│  月之暗面   │  零一万物   │    Groq     │   Google Gemini   │
└─────────────┴─────────────┴─────────────┴───────────────────┘
```

## 🚀 快速开始

### 环境要求

- Windows 10/11
- C++17 编译器 (MSVC/MinGW)
- [libcurl](https://curl.se/libcurl/)
- [nlohmann/json](https://github.com/nlohmann/json)

### 构建项目

#### 方式一：使用 g++ (MinGW/MSYS2)

```bash
# 1. 安装 MSYS2 和依赖
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-curl

# 2. 编译
g++ -std=c++17 -O2 -o aries_agent.exe aries_agent.cpp \
    -lws2_32 -lgdi32 -lgdiplus -lcrypt32
```

#### 方式二：使用 MSVC

```bash
# 使用 vcpkg 安装依赖
vcpkg install curl:x64-windows nlohmann-json:x64-windows

# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake

# 构建
cmake --build . --config Release
```

#### 方式三：简单 Makefile (g++)

```makefile
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall
LDFLAGS = -lcurl -lws2_32 -lgdiplus -lcrypt32

aries_agent: aries_agent.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f aries_agent.exe
```

### 运行

```bash
# 直接运行
aries_agent.exe

# 运行后：
# 1. 选择 API 提供商（支持 14+ 厂商）
# 2. 输入 API Key（可选择保存）
# 3. 输入任务目标
```

**多 API Key 管理**:
- 支持同时保存多个厂商的 API Key
- 每次运行可选择使用不同的提供商
- 使用 `clear` 命令可清除保存的 API Key
- 支持的环境变量：`SILICON_FLOW_API_KEY`, `DEEPSEEK_API_KEY`, `OPENAI_API_KEY`, `ALIYUN_API_KEY` 等

## 📝 使用示例

### 示例 1: 打开计算器并计算

```
用户: 打开计算器计算 123 + 456

AI 执行流程:
1. 获取已安装应用列表
2. 启动计算器 (calc)
3. 点击计算器界面
4. 输入 "123+456"
5. 任务完成
```

### 示例 2: 打开任务管理器

```
用户: 打开任务管理器

AI 执行流程:
1. 通过 Launch 动作启动 taskmgr
2. 等待界面加载
3. 任务完成
```

### 示例 3: 执行 PowerShell 命令

```
用户: 查看当前目录下的所有文件

AI 执行流程:
1. 执行 Execute 动作: dir
2. 获取命令输出
3. 返回结果给用户
```

## 🎮 支持的动作

| 动作 | 参数 | 说明 |
|------|------|------|
| `Tap` / `Click` | `element=[x,y]` | 点击指定坐标位置 |
| `Type` | `text="内容"` | 模拟键盘输入文本 |
| `Swipe` | `start=[x,y], end=[x,y]` | 模拟鼠标拖动/滑动 |
| `Back` | - | 模拟返回键 (Alt+Left) |
| `Home` | - | 模拟主页键 (Win 键) |
| `Wait` | `duration=秒数` | 等待指定时间 |
| `Launch` | `app="应用名"` | 启动应用程序 |
| `Execute` | `command="命令"` | 执行 PowerShell 命令 |
| `Installed` | - | 获取已安装应用列表 |
| `finish` | `message="消息"` | 完成任务 |

## 🔐 安全特性

### API Key 安全存储

`secure_storage.hpp` 提供硬件绑定的加密存储：

- **硬件绑定**: 基于 CPU、硬盘、主板等硬件信息生成唯一密钥
- **加密存储**: 使用多层加密算法保护 API Key
- **设备唯一**: 存储文件 `.api_key_<provider>_secure` 只能在当前设备上解密使用
- **防复制**: 即使文件被复制到其他设备也无法解密
- **多 Key 支持**: 每个厂商的 API Key 独立存储，互不影响

> 注意：硬件变更（如更换主板、CPU）可能导致无法解密，需要重新保存 API Key。

## 🛠️ 模块详解

### 1. action_parser.hpp - 动作解析器

解析 AI 返回的自然语言指令，提取可执行的动作：

```cpp
aries::ActionParser parser;
auto action = parser.parse("do(action=\"Tap\", element=[500,300])");
// action.action = "Tap"
// action.fields["element"] = "[500,300]"
```

支持多种格式解析：
- `<answer>` 标签
- `<|begin_of_box|>` 标签
- `<tool_call>` 标签
- Unicode 转义序列

### 2. action_executor.hpp - 动作执行器

执行解析后的动作：

```cpp
aries::ActionExecutor executor;
auto result = executor.execute(action);
```

支持的操作：
- 模拟鼠标点击、拖动
- 模拟键盘输入
- 执行 PowerShell 命令
- 启动应用程序

### 3. app_manager.hpp - 应用管理器

管理 Windows 已安装应用：

```cpp
// 获取已安装应用列表
auto apps = aries::AppManager::getInstalledApps();

// 查找应用
auto* app = aries::AppManager::findApp(apps, "Chrome");

// 启动应用
aries::AppManager::launchApp(*app);
```

### 4. prompt_templates.hpp - 提示词模板

构建系统提示词，指导 AI 输出格式：

```cpp
std::string systemPrompt = aries::PromptTemplates::buildSystemPrompt(1920, 1080);
std::string repairPrompt = aries::PromptTemplates::buildActionRepairPrompt(failedAction);
```

### 5. silicon_flow_client_simple.hpp - API 客户端

Silicon Flow API 的 C++ 客户端：

```cpp
silicon_flow::ClientConfig config;
config.api_key = api_key;

silicon_flow::SiliconFlowClient client(config);

// 简单对话
auto [success, response] = client.simple_chat("你好", "你是一个助手");

// 带图片的对话（用于截图分析）
auto [success, response] = client.chat_with_images(
    "描述这张截图",
    {"screenshot.png"},
    "你是一个图像分析助手",
    "zai-org/GLM-4.5V"
);
```

## 🧩 支持的模型厂商

本项目基于 **OpenAI 兼容 API** 设计，支持几乎所有主流大模型厂商：

### 国内厂商

| 厂商 | 基础 URL | 推荐模型 |
|------|----------|----------|
| **Silicon Flow** | `https://api.siliconflow.cn/v1` | `deepseek-ai/DeepSeek-V3`, `zai-org/GLM-4.5V` |
| **阿里云百炼** | `https://dashscope.aliyuncs.com/compatible-mode/v1` | `qwen-vl-plus` (视觉), `qwen-max` |
| **百度千帆** | `https://qianfan.baidubce.com/v2` | `ernie-4.0` |
| **智谱 AI** | `https://open.bigmodel.cn/api/paas/v4` | `glm-4v` (视觉), `glm-4` |
| **DeepSeek** | `https://api.deepseek.com/v1` | `deepseek-chat`, `deepseek-coder` |
| **MiniMax** | `https://api.minimax.chat/v1` | `abab6.5-chat` |
| **月之暗面** | `https://api.moonshot.cn/v1` | `moonshot-v1-8k` |
| **零一万物** | `https://api.lingyiwanwu.com/v1` | `yi-large` |

### 国际厂商

| 厂商 | 基础 URL | 推荐模型 |
|------|----------|----------|
| **OpenAI** | `https://api.openai.com/v1` | `gpt-4o` (视觉), `gpt-4-turbo` |
| **Azure OpenAI** | `https://{resource}.openai.azure.com/openai/deployments/{deployment}` | `gpt-4o` |
| **Anthropic** | `https://api.anthropic.com/v1` | `claude-3-opus` |
| **Google Gemini** | `https://generativelanguage.googleapis.com/v1beta` | `gemini-pro-vision` |
| **Groq** | `https://api.groq.com/openai/v1` | `llama3-70b-8192` |
| **Together AI** | `https://api.together.xyz/v1` | `meta-llama/Llama-3-70b` |

### 配置示例

```cpp
// Silicon Flow (默认)
silicon_flow::ClientConfig config;
config.api_key = "your-siliconflow-api-key";
config.base_url = "https://api.siliconflow.cn/v1";

// 阿里云百炼
silicon_flow::ClientConfig config;
config.api_key = "your-dashscope-api-key";
config.base_url = "https://dashscope.aliyuncs.com/compatible-mode/v1";

// OpenAI
silicon_flow::ClientConfig config;
config.api_key = "your-openai-api-key";
config.base_url = "https://api.openai.com/v1";

// DeepSeek
silicon_flow::ClientConfig config;
config.api_key = "your-deepseek-api-key";
config.base_url = "https://api.deepseek.com/v1";
```

### 视觉模型推荐

用于截图分析时，推荐使用以下视觉模型：

| 厂商 | 模型名称 | 特点 |
|------|----------|------|
| Silicon Flow | `zai-org/GLM-4.5V` | 性价比高，中文理解好 |
| 阿里云 | `qwen-vl-plus` | 强大的视觉理解能力 |
| 智谱 | `glm-4v` | 优秀的图文理解 |
| OpenAI | `gpt-4o` | 最强视觉能力 |
| Google | `gemini-pro-vision` | 多模态能力强 |

### 文本模型推荐

| 厂商 | 模型名称 | 特点 |
|------|----------|------|
| Silicon Flow | `deepseek-ai/DeepSeek-V3` | 推理能力强，价格低 |
| DeepSeek | `deepseek-chat` | 优秀的代码和推理 |
| 阿里云 | `qwen-max` | 中文理解优秀 |
| OpenAI | `gpt-4-turbo` | 综合能力最强 |

## 💾 多 API Key 管理

### 保存多个 API Key

程序支持同时保存多个厂商的 API Key，每个 Key 独立加密存储：

```bash
# 第一次运行，保存 Silicon Flow Key
aries_agent.exe
# 选择 1 (Silicon Flow)
# 输入 API Key
# 选择 y 保存

# 再次运行，保存 DeepSeek Key
aries_agent.exe
# 选择 0 (输入新的 API Key)
# 选择 2 (DeepSeek)
# 输入 API Key
# 选择 y 保存
```

### 使用已保存的 API Key

```bash
aries_agent.exe
# 显示已保存的提供商列表
# 输入对应编号选择
# 直接开始任务
```

### 清除 API Key

```bash
aries_agent.exe
# 输入 clear
# 选择要清除的提供商 (或 0 清除所有)
```

### 支持的 API Key 文件

每个厂商的 API Key 存储为独立文件：
- `.api_key_siliconflow_secure`
- `.api_key_deepseek_secure`
- `.api_key_openai_secure`
- `.api_key_aliyun_secure`
- `.api_key_zhipu_secure`
- ... (其他厂商)

## 📊 执行流程

```
用户输入指令
      │
      ▼
┌─────────────┐
│  截取屏幕    │
└─────────────┘
      │
      ▼
┌─────────────┐     ┌─────────────┐
│  发送给 AI   │────▶│  获取响应    │
│  (带截图)    │     │  (动作指令)  │
└─────────────┘     └─────────────┘
                            │
                            ▼
                    ┌─────────────┐
                    │  解析动作    │
                    │(ActionParser)│
                    └─────────────┘
                            │
                            ▼
                    ┌─────────────┐
                    │  执行动作    │
                    │(ActionExecutor)│
                    └─────────────┘
                            │
                    ┌───────┴───────┐
                    ▼               ▼
              任务完成?         需要继续?
                 是               否
                  │               │
                  ▼               ▼
            返回结果          继续循环
```

## ⚙️ 配置选项

### ActionExecutor 配置

```cpp
aries::ActionExecutor::Config config;
config.screenWidth = 1920;           // 屏幕宽度
config.screenHeight = 1080;          // 屏幕高度
config.tapAwaitWindowTimeoutMs = 500; // 点击后等待时间

aries::ActionExecutor executor(config);
```

## 🐛 故障排除

### 问题 1: API Key 无效

**解决**: 检查 API Key 是否正确，或重新运行程序输入 Key

### 问题 2: 截图失败

**解决**: 确保程序有屏幕捕获权限，尝试以管理员身份运行

### 问题 3: 动作执行失败

**解决**: 
- 检查屏幕分辨率设置
- 确认目标应用窗口可见
- 查看 `aries_agent.log` 日志文件

### 问题 4: 应用启动失败

**解决**: 使用 `Installed` 动作先获取应用列表，确认应用名称正确

## 📄 日志文件

程序运行日志保存在 `aries_agent.log`，可用于调试：

```
[2024-01-15 10:30:45] 系统提示词构建完成
[2024-01-15 10:30:46] 截图成功: screenshot.png
[2024-01-15 10:30:47] 发送请求到 AI...
[2024-01-15 10:30:48] 收到响应: do(action="Launch", app="calc")
[2024-01-15 10:30:48] 执行动作: Launch calc
```

## 🤝 依赖项目

- [libcurl](https://curl.se/libcurl/) - HTTP 客户端
- [nlohmann/json](https://github.com/nlohmann/json) - JSON 处理

## 🌐 支持的 API 服务商

- [Silicon Flow](https://siliconflow.cn/) - 国内聚合 API 平台
- [阿里云百炼](https://www.aliyun.com/product/bailian) - 通义千问系列
- [百度千帆](https://qianfan.cloud.baidu.com/) - 文心一言系列
- [智谱 AI](https://open.bigmodel.cn/) - ChatGLM 系列
- [DeepSeek](https://platform.deepseek.com/) - DeepSeek 系列
- [MiniMax](https://www.minimaxi.com/) - abab 系列
- [月之暗面](https://www.moonshot.cn/) - Kimi 系列
- [零一万物](https://www.lingyiwanwu.com/) - Yi 系列
- [OpenAI](https://openai.com/) - GPT 系列
- [Azure OpenAI](https://azure.microsoft.com/en-us/services/cognitive-services/openai-service/) - 企业级 GPT
- [Groq](https://groq.com/) - 极速推理平台
- [Together AI](https://www.together.ai/) - 开源模型平台

> 任何支持 OpenAI 兼容 API 格式的服务商均可使用，只需修改 `base_url` 和 `api_key`。

## 📜 许可证

MIT License

## 🙏 致谢

感谢 Silicon Flow 提供的大模型 API 服务。
