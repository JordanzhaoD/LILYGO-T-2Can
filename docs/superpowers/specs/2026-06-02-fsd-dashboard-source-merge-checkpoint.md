# FSD Dashboard Source Merge Checkpoint

日期：2026-06-02  
项目：LILYGO T-2CAN Tesla Model Y FSD CAN Dashboard  
当前阶段：Superpowers brainstorming 已启动，尚未进入实现。

## 用户当前意图

用户希望后续继续执行：

- 全面检查本地 dashboard 界面功能和 UI 连接逻辑。
- 对照 `/Users/ziwind/Desktop/FSD截图`。
- 对照公开源代码：
  - `/Users/ziwind/Codex/DouyinFSD /源代码与抓包数据/公开源代码.txt`
  - `/Users/ziwind/Codex/DouyinFSD /源代码与抓包数据/抓包方向盘数据代码.txt`
- 精准合并本地尚未实现的截图/源代码功能。
- 不破坏此前已经实测成功的 v4.01/v4.x 成果。
- 给出开发计划文档、执行文档、优化对接合并审查建议。

## Superpowers 状态

已查找到正式注册/缓存的 Codex Superpowers skill：

- `/Users/ziwind/.codex/plugins/cache/openai-curated/superpowers/bebc3d6a/skills/using-superpowers/SKILL.md`
- `/Users/ziwind/.codex/plugins/cache/openai-curated/superpowers/bebc3d6a/skills/brainstorming/SKILL.md`
- `/Users/ziwind/.codex/plugins/cache/openai-curated/superpowers/bebc3d6a/skills/brainstorming/visual-companion.md`

注意：

- 当前会话顶部 active skills 列表没有直接列出 `superpowers:*`。
- 但插件缓存中存在正式 `using-superpowers` 与 `brainstorming` skill。
- 项目本地 `.superpowers/brainstorm` 只是 companion 持久化目录，不等同于正式 skill。

## 已完成审查

已完成第一轮上下文扫描：

- 仓库路径：`/Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware`
- Git 最近提交包含：
  - `bde9150 chore: publish batch a firmware assets`
  - `4ee6198 feat: add bus2 diagnostics and service pulse`
  - `14bb70a feat: wire legacy speed offset release`
  - `bb6f737 fix: decouple FSD boot toggle from session toggle, fix bootCan display`
  - `d7bb797 fix: persist legacy FSD module controls`
- 当前第一轮 `git status --short` 输出为空，未见未提交变更。

已读取/对照：

- `docs/FSD_screenshot_source_merge_plan_v4.01_zh.md`
- `docs/FSD_screenshot_source_merge_execution_v4.01_zh.md`
- `docs/superpowers/specs/2026-05-31-fsd-merge-design.md`
- `include/dash_bionic_steer.h`
- `include/dash_wheel_dnd.h`
- `include/dash_fog_light.h`
- `include/web/mcp2515_dashboard_ui.src.h`
- `include/web/mcp2515_dashboard_ui.h`
- `src/main.cpp`
- 外部公开源代码和抓包方向盘代码的关键片段。

## 当前重要判断

旧版合并计划文档中有些判断已经过时。当前本地代码已经落地了不少 Phase 3 / Phase 4 能力，包括：

- `dash_bionic_steer.h`：0x370 仿生方向盘扭矩模块。
- `dash_wheel_dnd.h`：0x3C2 音量/速度滚轮 DND 四步序列模块。
- `dash_fog_light.h`：0x273 后雾灯 strobe / F1 pilot / continuous 控制模块。
- UI 中已有 `pg-strobe` 灯光特技页面。
- UI/后端已有 `/fog_light` 相关调用。
- UI 已调用 `/speed_custom`、`/power_mgmt`、`/vehicle_ota_status`。
- 网络/DNS 黑白名单已合并在网络模块。
- 手机端已有底部导航和“更多”入口。

因此后续不能直接沿用旧文档中的“尚未实现”结论，需要重新按以下三类审查：

1. 已落地并需回归验证。
2. 半实现但 UI/API/运行态仍需校准。
3. 仍缺失或应保持实验/只读的高风险功能。

## 可视化 companion 状态

已启动过 Superpowers visual companion：

- URL：`http://localhost:51012`
- screen_dir：`/Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware/.superpowers/brainstorm/15009-1780376640/content`
- state_dir：`/Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware/.superpowers/brainstorm/15009-1780376640/state`
- 已生成页面：
  - `.superpowers/brainstorm/15009-1780376640/content/fsd-merge-audit-board.html`

页面给出了三种后续路线：

- A：稳健补齐契约。先审计 UI 按钮/API/返回字段，再修补缺口。CAN 注入类只做状态、配置、日志，不扩大行为。
- B：功能优先移植。快速把公开源代码的 0x370、0x3C2、0x273 行为接入运行路径，但实车风险高。
- C：分层实验通道。新增“实验运行态”层：配置可保存，执行需显式 armed；先支持手动短脉冲，再逐步开放自动触发。

建议后续优先选择 A 或 C；更推荐 C，因为它更适合安全接入 0x370 / 0x3C2 / 0x273 等实验能力。

## 后续继续时的下一步

按正式 Superpowers brainstorming 流程继续：

1. 如 companion server 已停止，重新启动：

   ```bash
   /Users/ziwind/.codex/plugins/cache/openai-curated/superpowers/bebc3d6a/skills/brainstorming/scripts/start-server.sh --project-dir /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware
   ```

2. 读取 browser events：

   ```bash
   cat /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware/.superpowers/brainstorm/15009-1780376640/state/events
   ```

3. 询问用户选择 A / B / C 哪条合并路线。
4. 按用户选择提出 2-3 个方案和推荐方案。
5. 写正式设计文档：

   ```text
   docs/superpowers/specs/2026-06-02-fsd-dashboard-source-merge-design.md
   ```

6. 自审设计文档，检查：

   - 是否有 TODO / TBD。
   - 是否与当前代码真实状态矛盾。
   - 是否破坏已实测成功功能。
   - 是否把高风险 CAN 注入默认关闭并标记实验。

7. 等用户确认设计后，再进入 writing-plans / implementation。

## 关键保护原则

- 不直接复制公开源代码的大段 `/api/cmd` 单入口逻辑。
- 保留本地结构化 API。
- 不破坏已验证的激活模式、网络/DNS、OTA、CAN工具、CAN2控制、手机版导航。
- 高风险功能默认关闭。
- 自动换挡保持只读/储备，除非有明确实车验证数据。
- 连续爆闪、FSD增强、AP/EAP 免打扰、MAX/V14 增强必须二次确认并标记实验。
- 每个功能继续坚持三层状态：
  - UI 显示状态。
  - NVS 持久化状态。
  - 实际 CAN / 网络运行状态。
