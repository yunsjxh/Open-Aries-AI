# 安全开发文档（4.1 迭代落地）

本文档对应 `PROJECT_REVIEW.md` 中“4.1 两周内（安全与稳定先行）”的落地实现。

## 已完成项

### 1) 能力开关（Capability Flags）

已在 `ActionExecutor::Config` 中增加以下开关，默认关闭高危能力：

- `allowExecute`（默认 `false`）
- `allowFileWrite`（默认 `false`，同时控制 `FileWrite` / `FileAppend`）
- `allowFileDelete`（默认 `false`）
- `allowFileRun`（默认 `false`）
- `requireHighRiskConfirmation`（默认 `true`）

应用启动后，会在控制台引导用户逐项确认是否开启。

### 2) 高危动作二次确认

新增统一高危动作确认逻辑：

- 高危动作集合：`Execute`、`FileWrite`、`FileAppend`、`FileDelete`、`FileRun`
- 执行前会弹出确认提示，用户输入 `y/Y` 才会继续。
- 若拒绝，动作会被拦截并反馈“用户拒绝执行高危动作”。

### 3) SecureStorage 切换为 DPAPI

API Key 与自定义 Provider 配置已改为优先使用 Windows DPAPI：

- 加密：`CryptProtectData`
- 解密：`CryptUnprotectData`
- 存储格式版本头：`DPAPI_V1`

并保留了旧格式（硬件指纹 + XOR + 混淆）的读取兼容，避免升级后历史数据无法读取。

### 4) Action 严格 Schema 校验

在 `ActionParser` 新增 schema 校验机制：

- 仅接受已注册动作类型。
- 对关键动作要求必填字段（例如 `Execute` 要有 `command`，`FileWrite` 要有 `path` + `content`）。
- 校验失败时返回无效动作，主流程不会继续执行该动作。

---

## 兼容性说明

1. **密钥数据兼容**：
   - 新写入数据：`DPAPI_V1`
   - 旧数据：仍可读取

2. **行为兼容**：
   - 高危能力默认关闭，可能导致以前可直接执行的动作被拒绝。
   - 建议按任务场景显式开启必要能力。

---

## 推荐下一步

1. 将能力开关做成配置文件（如 `aries_config.json`）并支持环境变量覆盖。
2. 为 `ActionParser::validateAgainstSchema` 增加覆盖测试。
3. 为高危动作确认增加“本轮记住选择”机制，减少频繁重复输入。
4. 将 `Execute` 从黑名单策略逐步迁移为“命令模板白名单 + 参数化”。
