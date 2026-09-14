# LILYGO T-2CAN 固件资产 v4.5.0-beta08（2026-09-12）

**相对 v4.5.0-beta07 的变化**：修复手机（iOS Safari）上底部标签栏
滚动后消失的问题。纯 UI 层修复，8.3.6 防甩机制语义零改动。

## 症状（Jordan 报告 + 三张截图）

手机上打开控制页，**底部标签栏（▣状态/◉模式/↗速度/◎网络/···更多
那一条）会消失**：

- IMG_4376 / IMG_4377：页面滚动到底部时标签栏消失，底部只剩
  Safari 自己的白色工具栏（网页内容直接结束）；
- IMG_4378（红圈标注）：同一页面浅位置标签栏正常显示——证明是
  **间歇性**、与滚动状态相关，不是永远不渲染。

## 根因

两条底部标签栏（`#mob-tabs` / `#mob-tabs-dual`）和两个「更多」面板
（`#mob-more-single` / `#mob-more`）是 `position:fixed` 元素，但它们
在 HTML 里嵌套在 `.content`（`overflow-y:auto` 滚动容器）内部。

**iOS Safari 在长页面惯性滚动（momentum scroll）期间不重绘滚动容器
内嵌套的 fixed 元素**——滚动后标签栏渲染空白，看起来就是「消失」。
这与单 CAN 固件 v1.0.7（2026-07-05）修过的是同一个 bug，双 CAN 的
UI 是独立分支，从未移植该修复。

## 修复（移植单 CAN v1.0.7 的 hoistMobileChrome）

- 新增 `hoistMobileChrome()`：DOM 加载时把 4 个 fixed chrome 元素重新
  挂为 `<body>` 直接子元素，`position:fixed` 稳定锚定视口；
- `#mob-tabs` 改为自带 `data-dual-hide="1"`（原包装 div 删除）——
  hoist 脱离包装后，产品模式显隐（单/双 CAN）由元素自身属性控制，
  行为不变；
- 布局逐像素零变化（430×932 探针：栏位置 430×77@底部、层级
  z-index 200、`.main` 74px 预留均与修复前一致）。

## 验证

- pytest 234 通过 + 721 subtests（新增 hoist 双面合约：
  源 + minified 产物钉死函数/调用/4 元素/属性；旧包装反断言防回退）
- `pio test -e native_ap_rerequest` 54/54
- ESP 双构建 SUCCESS（lilygo_t2can_dual + waveshare_single_can_standalone），
  产物 grep：beta08×7 / beta07×0
- Playwright（压缩 HTML = 真机服务内容，430×932 手机视口）：
  4 元素挂 body、双/单 CAN 模式显隐正确、切页/高亮/更多面板/
  内联 onclick 全部正常、滚动到底标签栏保持钉底

## 烧录（Jordan 自行，二选一）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`
- **esptool 全量**（清配置）：`esptool write_flash 0x0 *_full_merged.bin`
- ⚠️ **禁 `pio run -t upload`**（app@0x10000 ≠ 分区表 app0@0x20000，损坏板子）

## 手机验证方法（烧录后）

1. 手机连板子热点打开控制页；
2. 进入一个长页面（如「◇ 硬件模式」或「◈ FSD 防护」）；
3. **快速滚动到底部再滚回顶部，反复几次**——标签栏应始终保持显示，
   不再出现空白/消失。

## 边界

- 未烧录、未接车、未做实车验证；8.3.6 防甩机制语义零改动
  （修复仅在 UI HTML/JS 层）
- 未 commit / 未 push / 未发布
