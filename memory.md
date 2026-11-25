## 2026-11-26 · beta.12 田野通过（当前最新；已实车验证的基线，非必要勿动）

**Jordan 实车报告：「大获成功」——右滚轮长按免打扰成功稳定；左滚轮 DND 成功稳定。** 证据=车机截图 `~/Downloads/IMG_4757.PNG`（板子在车机 AP 下 192.168.8.129，会话 1h15m）+ 板子 `/status` 实拉（192.168.1.56，uiBuildId 与发布版一致）。
- 截图计数：右滚轮 按下帧 126 / 发送失败 0 / 提醒轮次 21 / 自愈 0 / 动作帧跳过 34；左滚轮 正向 10 / 补偿 8 / 配对中和 5 / 计数器冲突 0 / 未 disarm。★左滚轮正向比反向多 2 次（10−8）=本程音量净升约 2 格，就是 Jordan 说过的"音量回不到原位"老问题（11-23 稳定期录制里同样存在），也是走右滚轮路线的理由；本次未改左轮引擎。**两个引擎都在 FSD 注入关的情况下运行（fsd_3ee_inject=false、bootCan=false）→ TX 分路目标实车达成**；「总开关默认关时两引擎都不动」的问题随之消失。
- **左轮沉默根因（排除法定案）**：beta.11→beta.12 对左轮 tick 的唯一改动就是去掉车辆 OTA 闸，去掉后恢复 → 是它。这个标志在这条总线上会被置位（录制器没录 0x318，所以之前看不到），置位后只能靠 0x318 清除。
- ★**126 = 21 × 6：21 轮全部顶到 6 帧上限**，没有一轮在上限之前收到 hands<3。原生帧约 100ms 一拍、0x399 约 0.5s 一帧 → 自愈型短提醒只会产生约 5 帧；全部 ≥6 说明这 21 轮不是自愈，是按住清掉的（与 Jordan 的报告一致）。含义：**每轮的按住时长其实由上限决定（固定 6 拍≈0.6s），不是由"提醒已清除"决定**——引擎从没在上限之前收到过清除。21/21 够用，但余量多少不知道（原生拍快时 6 帧只有约 0.42s；beta.07 有两轮到第 2 个 0x399 才报清除）。**暂不建议动**：清不掉的通知要等下一个 0x399（0.5s 一帧）才知道，调高上限=每一轮都按得更久，而不只是给少数轮次留余量；若以后出现清不掉的提醒，第一个要调的旋钮才是上限 6→8（仍远低于 beta.08 的 853 帧，且有 1.5s 稳定时间限速）。
- 板上现状（台架、无总线，重启后约 10 分钟）：右滚轮开 / 左滚轮关 / FSD 注入关，均已持久化；vehicleOta=false。
- 小瑕疵（P3）：右滚轮开着但还没收到过任何 0x399 时，状态栏显示「被闸门拦截 disabled」——那个代码是引擎的初始占位值，真实原因是总线离线。只在台架上出现，会让人困惑，下版顺手修。

**★Jordan 的四个决定（2026-11-26）**：
1. **菜单会弹，约 3–4 秒消失，基本无感**——不再是"待优化"的问题，宣发里如实写一句即可。
2. **暂不出 beta.13**。右滚轮状态栏写明具体闸门、上面那个小瑕疵，都搁置。
3. **冻结免打扰功能的改动（"目前基本完美"）**：范围=左滚轮 DND 引擎、右滚轮按住引擎、它们在 `src/main.cpp` 的 tick 闸门、让它们在 FSD 注入关闭时照常运行的 TX 分路。要改需 Jordan 明确解冻；发现 bug 也先报告不擅自改。已落到代码：两个引擎源文件顶部加了 FROZEN 说明（纯注释，commit 5bc0a2f，117 项测试全过），beta.12 资产 README 也记了。
4. **宣发口径改为两个卖点**：新功能「右滚轮免打扰」（现在是主卖点之一，不再是"如实提及不当作卖点"）+ TX 分路（FSD 注入关闭后独立运行）。"实车验证中"可以改。

**推广素材第三版（2026-11-26，Desktop 唯一的一套 `share-t2can-6.1.0-beta.12-*`）**：copy.md（公众号/朋友圈/小红书）+ 三尺寸海报，逐张目检通过。海报=标题两行各一个卖点（新增右滚轮免打扰，不动音量 / 关掉 FSD 注入，免打扰照常运行）；卡 01 右滚轮（按住 6 拍时序图 + 实车三个数：75 分钟 / 21 轮 / 0 次发送失败）；卡 02 TX 分路（FSD 注入开关关 → 三条分路：注入已关闭、左滚轮照常、右滚轮照常）；底部中性说明条写菜单 3–4 秒消失。两卡都带「实车验证」标。海报上的事实全部有出处：75 分钟=截图会话 1:15:44，21/126/0=截图计数，菜单 3–4 秒=Jordan 实测。源文件 `/tmp/beta12-social/social-cards-v3.html`。
旧素材已清：beta.09 / beta17 / beta21 / 6.0.0-beta14 / beta03 五套 + beta.12 第一版海报，共 23 个文件，移入废纸篓 `~/.Trash/t2can-old-promo-20261126`（可找回）。其他项目的素材（p4can、单 CAN）没动。
文案里仍然不写的：21 次全部清除（没量过）、两个免打扰同时开启（没验证过并行）、速度偏移在注入关闭时照常、右滚轮状态栏写明闸门、长期可靠、任何绝对话术。

**待办**：Jordan 自己决定何时发布。若以后要解冻改免打扰，先读本节"Jordan 的四个决定"和两个引擎顶部的 FROZEN 说明。

---

## 2026-11-26 · beta.12 = 去掉滚轮的 OTA 闸 + 右滚轮回到按住 + 界面显示闸门（已实车验证，见上）

**main=fb203d1 已 push，uiBuildId abfb1d5f14ac**。beta.11 实车（9 段录制 2026-11-25T203829→203847，最后一段左轮开、其余右轮开）：

**左滚轮回归（最严重）**：车上 0 帧、提醒持续 6 秒；**同样的帧回放进生产引擎（带生产绑定）发出 67 帧**→引擎逻辑与协调都对，是车上某道运行时闸关着。回查全部历史录制：**左轮的帧最后一次在实车发出是 11-23 22:48，11-24 起再没发过**（包括 beta.05 那次"实车通过"——那几次是短型提醒自己消失）。这很可能就是 Jordan 之前报的「总开关默认关两引擎都不动」同一根因。
- 闸门排查：abort guard 排除（全程 AP 无 8/9）；右轮卡左轮排除（回放 siblingYields=0）；**vehicleOtaActive 最可疑**——它只能靠 0x318 帧清除，而这车总线没有 0x318，一旦置位整段行程都关。**Jordan 定夺：这道闸从两个滚轮引擎去掉**（FSD 激活注入与防抖程序保留）。合约钉住两个 tick 不引用它、防抖仍引用。

**右滚轮真相——我 beta.11 把形态改错了**：提醒分两种，短型（~0.5s 自己消失）与持续型（无人管 5–9s，中位 6.5s）。beta.11「点击」对持续型 **0/3**（5.3/7.6/7.6s = 自然时长）；"偶尔有效"的是短型。**beta.07 有效不是点击而是「按住」**：提醒期间每个原生帧后跟一帧按压直到 hands 降（4–8 帧/0.4–0.7s），6/6 清除。beta.07 是无上限电平型。**beta.12 = 有界电平按压**：每帧按压、**每轮上限 6 帧**（beta.07 最慢 8 帧清除）、不发松开帧、无点击间隔；保留稳定时间 1.5s/hands=2 不触发/闪回不重装。让路闸改为「只在自己发按压帧的那一拍占有，下一个原生帧释放」。菜单仍会弹（已知代价未解决）。
- 排除的假设（都撤回了）：0x399 b3 bit5（A0/80）决定有效性——beta.07 成功回合反而是 A0。

**界面显示闸门**（Jordan 选"盲修三道闸 + 界面显示"）：两个滚轮诊断表上方各加状态横条（运行中/开关未开/被闸门拦截+具体闸名/连续发送失败已停用），`wheelDndDiag` 增 `runtimeAllowed`+`runtimeBlock`，`rightWheelPressDiag` 增 `pressInFlight`。24 个原因代码翻成中英文。

**★教训**：
1. **"N/N 成功"要用对照验**：短型提醒不干预也会消失，必须量「持续型提醒」+ 对照「无设备干预时的自然时长」，否则把自愈算成功。beta.07「6/6」就是这样被高估的——实际清除的是 3 个持续型，但形态被我误读成点击。
2. **改形态前先量原形态**：我从抓包统计里读出 beta.07 是"单帧 P"，就把 beta.11 做成点击；实际序列是 `PPPPP`（电平）。写代码前应把成功样本的 TX 序列逐帧打印。
3. **禁词冲突**：`settling` 是已删功能 AP-settle 的墓碑词，两条合约 `assertNotIn`——新原因名用 `warn_flicker`。改 UI 后合约变红先看是不是撞禁词，不要去动守护合约。
4. **i18n 字典行替换只做精确子串、替换后比对键集合**（这次又差点踩：数出 3 次而非 2 次，先定位再动）。
5. **脚本化改测试连续失败两次就停手换 Read+Edit**——这次又绕了一圈。

**（历史，已被上面的「推广素材第三版」取代）推广素材第一、二版（2026-11-26 上午）**：当时 Jordan 定的是"双卖点并列（右滚轮按住 + 闸门状态横条）；右滚轮如实提及不当作卖点；先不发"，口径是 beta.12 零实车数据→不写"已验证"。**下面这两版的口径、文件名和待办都已作废，只留方法和教训。**
**⚠️第一版海报有一处与固件不符（11-26 重做时发现，已更正）**：状态栏画成「右滚轮按压免打扰：被闸门拦截 · CAN 发送器未就绪」——固件的状态栏实际是「文字 + `<code>代码</code>`」（`paintEngBar`），中文闸门名在诊断表「阻断原因」那一行；而且**右滚轮引擎的 `setRuntimeAllowed(bool)` 只收一个布尔，状态栏代码固定为 `runtime`，不说是哪道闸**——只有左滚轮经 `setRuntimeAllowed(allowed, gateBlock)` 能写明。文案里"被闸门拦截——并写明是哪一道"对右滚轮同样是过度承诺，已改成"左滚轮会写明是哪一道；右滚轮目前只显示被闸门拦截，下个版本补上"。**教训：画"界面实物"前要读渲染函数，逐个状态核对它实际拼出什么字符串，不能拿源码里出现过的两句话自行拼成一句。**

**海报第二版（2026-11-26，Desktop 标准文件名；第一版改名 `-v1-` 保留，勿发）**：设计=标题两行各一个卖点（右滚轮：按住，而不是点一下 / 界面写明被什么挡住）；两个卖点做成「外壳+内芯」双层玻璃卡；状态栏按固件实际显示来画（左滚轮 `twai_unavailable` 那条）；统计格换成车主看得懂且有出处的三个数（6 帧上限 / 1.5 秒 / 0.8 秒自动恢复），去掉「1 条」「172 个测试」；`.body{flex:1}` + 卡片 `flex:1 1 0` 吃掉全部剩余高度 → 三张都没有底部空带。源文件 `/tmp/beta12-social/social-cards-v2.html`。
★新方法：**布局先自检再目检**——页面里加一段脚本把每张卡的「底部空隙 / 横向溢出 / 内容溢出 / 文字超框」写进 DOM，`--dump-dom` 导出后解析（三张均：底部空隙=卡片内边距、溢出 0）；出图后仍逐张目检（抓到横版说明的孤字「绪」→ 对照文字设 `white-space:nowrap`）。
★出图坑：**Playwright MCP 连接超时时，用本机 Chrome 无头模式替代**（`"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=1 --window-size=W,H --virtual-time-budget=1500 --screenshot=out.png "url?only=<card>"`）；三张卡在同一页面时无头 Chrome 只截视口，需加 `?only=` 参数隐藏其余卡。square 用两节并排会下半空旷（内容仅到 60%）→改上下堆叠+状态栏竖排才填到约 82%。

**（这段待办已全部了结，现行待办见本文件最上面一节）** 原待办：车上 OTA 验证 → 已通过；左轮不动 → 已解决；菜单 → 3–4 秒消失，如实写；旧宣发 → 已移入废纸篓；右滚轮状态栏不写具体闸门 → 固件缺口仍在，Jordan 定暂不出 beta.13，搁置（改法=右滚轮 tick 把具体闸门传给引擎，仿左滚轮 `setRuntimeAllowed(allowed, gateBlock)`），且免打扰已冻结。

---

## 2026-11-25 · beta.11 = beta.10 独立审查修复（已被 beta.12 取代）

**main=f7fcb6d 已 push，uiBuildId cc5eea0b6fe0**。独立审查 beta.10 抓 **1C+5I**，全修：
- **C1 没有全局发包上限（最严重）**：每轮 2 次的预算在每次"提醒进入窗口"时重装——hands 2↔3 闪回、或闸门落下再抬起，都会以节奏速率持续点击（审查复现约 10 帧/秒 > beta.08 的 8.8）。**修法（Jordan 定）= 提醒须连续消失满 1.5s 才开新一轮**（`kWarningSettleMs`），一条规则对 hands/闸门/开关一视同仁；引擎无条件"观测"总线，只 gate "动作"。
- **松开帧改骑下一帧原生（Jordan 定）**：beta.10 的按下+立即松开用 c+2，造成总线计数器倒退 c→c+1→c+2→c+1（本设备从未发过的形态，审查 M6）。代价=按键按住时长从 0.25ms 回到约 100ms，**菜单是否还弹仍是待验证的核心问题**。
- I1 `/status` 没发 `apState`→状态表永远 `AP--`；I2 阻断原因被下一拍覆盖→0x399 侧管原因、0x045 侧只在回合内细化，`episode_limit` 粘滞；I3 松开帧发送结果不回传→`released` 只在总线收下后才加，失败重发并计入 disarm；I4 web 任务用弱条件重授运行许可→tick 成为唯一写入者；I5 闸门落下后右轮永久占有让路拍→左轮被卡死到重启→失权即结束动作状态，观测/发帧分离（CH 视角只观测）。
- M2 左轮协调测试改用**生产绑定**（isPressFrame+siblingOwnsBeat，双引擎真实联动）；M9 `/defense_config` 参数表漏 `right_wheel_press`；新增合约钉「界面读的键 = /status 发的键」（已做变异验证）。
- 门禁 native 175/175 + ESP 绿 + 镜像自检 15/15。

**★排查教训（这次绕了很久）**：Unity 失败只报「期望/实际」不报是哪个变量——我把 `pressed` 的失败一直当成 `warningEpisodes` 的失败，所有探针又都只打印 `warningEpisodes`，于是"探针全过、测试总挂"，一度怀疑构建缓存、未定义行为、状态泄漏，全错。**正确做法：第一步就把真测试文件 + 一个 Unity shim 原样编译，让断言打印出「表达式文本」**（`#e,#a` 字符串化），一次就定位。另外"手抄探针"不可信——要机械抽取，不要凭记忆重写。

**★工程坑**：`const char*` 与字符串字面量做 `!=` 比的是指针地址（结果不可预测，clang 有 `-Wstring-compare` 警告）——比较原因字符串必须用 `strcmp`。三元运算符里多写一个右括号会让整页 JS 静默失效，`node --check` 是唯一能抓住它的检查。

**待办**：①Jordan 车上 OTA beta.11——先不开右轮开关确认左轮正常；再开右轮看「提醒是否清除 + **菜单是否还弹**（核心问题）+ 无红轮」；②若菜单仍弹→中央键路线需换思路（按住时长已回到 100ms 量级，与 beta.07 弹菜单时接近）；③「总开关默认关闭两引擎不动」我和独立审查都没在代码里找到路径——若重启后从未碰总开关仍能复现，请在提醒出现时抓 `/status`；④Desktop 的 beta.09 宣发四件仍须作废/重出。

---

## 2026-11-25 · 右滚轮回中央键点击 · 6.1.0-beta.10（已被 beta.11 取代）

**beta.09 田野判定**：右滚轮侧压「完全没有效果」→ 放大视野重读 Downloads 全量 23 份录制（beta.05–09），结论：

| 按压信号 | 版本 | 警告回合 | 结果 |
|---|---|---|---|
| **中央键 0x0010** | beta.07 | 6 | **6/6 清除**，2 次单帧 9–26ms 即清 |
| 侧键 0x0020 | beta.09 | 8 | 6 次未清（持续 6–15s），2 次警告自行消失 |
| 侧键+hands=2 触发 | beta.08 | — | 96.9s/853 帧狂发 → strike-out |

**两个引擎发帧骨架相同，只有按键位不同 → 车认「按中央」不认「按侧键」。beta.08 把按压位从中央改到侧键是这轮失败的源头。**

**★我犯的视野错误（永久教训）**：第一遍分析只统计「警告完全消失」且只看 beta.09 三段 → 判「右滚轮整条路线无效」并建议放弃。Jordan 纠正：「右滚轮中央按压和侧压确实可以实现无感但有部分屏幕信息残留，可以继续优化」。重读全量后才看到 beta.07 中央键 6/6。**教训：判一条路线死刑前必须读该路线全部版本的数据，不能只读最新失败版；用户说「部分有效」时去找「部分」在哪。**

**残留的本质**：按中央键会弹车机菜单（「手机未连接」卡片），每次按压都弹——车机收到同一个按键信号，清警告和弹菜单在 0x045 上分不开。同构问题=左滚轮滚动改音量。

**beta.10（main=e0e858a 已 push，uiBuildId 47bc051b65c4）**：Jordan 拍板方案一+二合并——
- 按压位回中央（data[4] 0x10）
- **点击形态**：按下帧 + 立即松开帧（原引擎发完按下帧要等车自己的下一原生帧≈100ms 才松开=车机看到「按住」；真实按键是按下+紧跟松开）
- 每轮上限 3→**2 次点击**；第二次需松开已发+其后新原生空闲帧+距松开≥150ms；发送失败的按下帧不补松开
- 左轮让路延到松开帧之后；触发语义不变（hands[3,6]∪[9,10] 沿型，hands=2 不触发）
- **UI 状态表**（Jordan 要求对齐左滚轮）：右轮开关下 7 行（告警状态/原生0x045/状态·本轮点击·上限/按下·松开·发送失败/回声·动作帧/提醒轮次·自愈/阻断原因），`/status rightWheelPressDiag` 同步扩字段
- native 167/167（右轮 20/20）+ESP 绿+镜像自检 13/13

**★两个工程坑（本次踩到）**：
1. **按行截断改 i18n 字典会吞掉同行后面的所有词条**——上一轮改 0x3EE 英文文案时用 `s.index("\n")` 截断，把 `'0x3EE FSD 激活注入'/'TX 分路'/'Release 在线更新 '/'双拨爆闪 '` 整批删掉；JS 仍语法合法（node --check 抓不到），英文界面显示裸 key。**是 `test_ui_i18n_data_attributes_covered_by_dict` 合约测试抓到的**。改 i18n 只替换精确的旧键值对，绝不按行截断；每次改 UI 后必跑该合约测试。
2. Python 合约套件有基线旧账（现 3F+2E：jitter fail-closed / release metadata / uptime-fsd persistence / 2 个 single-can ERROR）——非本次引入，待清理。本次顺手修了 1 个过时钉（wheel-DND gate 仍钉 canActive，beta.08 已改 canOnline）。

**待办**：①Jordan 车上 OTA beta.10 → 验证：警告清除（状态表「按下/松开」=1/1）+ **菜单是否还弹（本版核心问题）** + hands=2 不动作 + 无红轮；②若菜单仍弹→中央键路线需换思路（如试更短/不同键值，或回到「左滚轮音量」问题本身）；③Desktop 的 beta.09 宣发四件仍把右滚轮写成卖点，**须重出或作废**（写的是侧压，已判无效）。

---

## 2026-11-24 · 右滚轮按压回退沿型+限次 · 6.1.0-beta.09（已被 beta.10 取代）

**推广素材（2026-11-24 备好，Jordan 定"先不发"）**：四件在 Desktop `share-t2can-6.1.0-beta.09-*`（copy.md 三版+portrait/wide/square 三图，目检通过）。Jordan 拍板：**双卖点并列**（TX 分路+右滚轮按压）+ 右滚轮按压**照常宣传**。口径处理：右滚轮按压效果只写到代码/测试证明的程度（警告沿触发/每轮≤3次/消除即停），**不写"实车验证有效"**——beta.09 该功能零实车数据，README 田野清单未完成。
★两处发布前须核：①**速度偏移"实车正常生效"超出 Jordan 原话**（beta.05 实车反馈只说"DND 正常无红轮无卡顿"，未单提速度偏移）②记忆里的"native 189/189"是 beta.08 旧数且含已删的 nag_kill 12 例——beta.09 实测五套件 **162/162**（海报已用 162）。发布时点等右滚轮按压实车结果：通过→文案不动；未通过→删第二节。

**★关键教训：hands=2 不是 NAG 提示，是"手未上盘"的持续基线态！**

beta.08 把警告窗改成 `[2,7]`（hands=2 触发）是**方向性错误**。实车判定：引擎在 hands=2 期间疯狂持续发按压帧（抓包 221001 里 96.9 秒、853 帧不间断），车辆判定为"反复滥用侧键"→ strike-out 警告升级 → ap 状态 6→8→9→1→14（禁止辅助转向）。

**正确语义（对齐左滚轮 DND 六周稳定原理）**：
- 警告窗 `[3,6]∪[9,10]`：hands=3（VISUAL 视觉提示）才触发，hands=2 基线态不动作
- 沿型：警告沿才开 episode，持续警告不重复触发
- 限次配对：每 episode 最多 3 次按压（每个 fresh idle 帧一次）
- 消除立即停（500ms 去抖删除——它只为掩盖错误的 [2,7] 电平窗）

**DAS_autopilotHandsOnState 权威阶梯**：0=NOT_REQD, 1=DETECTED(手回盘正常), 2=NOT_DETECTED(手未上盘基线，**非提示**), 3=VISUAL(真正提示起点), 4/5=CHIME, 7=STRUCK_OUT, 8=SUSPENDED。

## 2026-11-24 · 右滚轮按压修正 + DND 解耦 · 6.1.0-beta.08（已废弃）

**实车 beta.07 反馈两个问题，全修复**：

### 问题1：右滚轮按压"部分有效/偶发失效/出现提示"
**★重大发现：0x399 hands 警告窗解码错了**。权威 opendbc DBC `DAS_autopilotHandsOnState`：0=NOT_REQD, 1=DETECTED, **2=NOT_DETECTED(NAG初始态)**, 3=VISUAL, 4/5=CHIME, 7=STRUCK_OUT, 8=SUSPENDED。现有 `[3,6]∪[9,10]` 漏掉 hands=2（最常见 NAG 态）。实车 hands 大部分在 2，短暂升 3。
- **修1 警告窗** → `[2,7]` 全阶梯
- **修2 按压位** → bit5 0x0020（右滚轮左侧键，停车抓包99次），原 bit4 0x0010 是中键
- **修3 释放去抖** → 500ms（hands 1↔2 每21-26ms抖动，清除即停导致 NAG 反弹）

### 问题2：免打扰机制被 FSD 注入总开关错误耦合
免打扰引擎（左右滚轮 DND）gate 链错误依赖 `canActive`（FSD 注入总开关），总开关关→免打扰失效，需"先开后关"假动作。
- **修4** → gate `canActive`→`canOnline`（总线连通），三处：main.cpp 右滚轮 tick(544)/左滚轮 tick(568)、mcp2515_dashboard.h setup(1716)。符合原设计注释"wheel DND unaffected — independent tick"。

**门禁**：native 189/189 + ESP 构建 SUCCESS（工具链 15.2.0 已修）。

**资产**：`firmware-assets/6.1.0-beta.08_20261124/`，uiBuildId `…-228a624831b2`。commit：`13d20c6`(警告窗/按压位/去抖) + `228a624`(DND解耦) + `c4320aa`(资产)。

**待办**：①Jordan 车上 OTA 验证（右滚轮按压单独验证 + FSD总开关关时免打扰照常）②push origin。

---

## 2026-11-24 · 右滚轮按压免打扰 · 6.1.0-beta.07（已被 beta.08 取代）

**实车判定**：beta.06 bit27 状态机制免打扰开启后车辆大量报警无法使用，路线整体否决。新方向：在**实车稳定的左滚轮免打扰**基础上开发并行机制——**NAG 提醒时合成右滚轮按压帧消除提示**。

**抓包标定（三份实车 CSV 交叉验证）**：右滚轮按压 = 0x045 开关字（b4|b5<<8）**bit4 = data[4]|=0x10**；右滚轮侧键B=bit5(0x20)；左滚轮按压=bit1(0x02)。与 `docs/LEGACY-0X045-WHEEL-VEHICLE-ANALYSIS` 权威位表一致。

**v3 实现（TDD RED→GREEN）**：
- 新引擎 `dash_legacy_wheel_045_press.h`：**电平型**——警告激活（hands[3,6]|[9,10]）期间，每个原生 idle 0x045 克隆置按压位 bit4、counter+1、CRC，骑原生帧 1:1 跟发；警告清除即停。按压无音量累积副作用，**无需反向平衡对**（与左滚轮 tick 不同）。
- **bit27 状态机制彻底删除**：引擎/测试/NVS nk27//status nagKillDiag//defense_config nag_kill/UI 开关全移除。
- 与左滚轮 DND **并行互让路**（beat-generational yield，独立开关独立诊断）。
- UI 新增「右滚轮按压免打扰」开关（实车路径徽标）；NVS `rwp`；/status `rightWheelPressDiag`；/defense_config `right_wheel_press`。
- 测试 9/9：按压编码、电平语义、警告窗、echo 不链、动作帧拒绝、disarm 自愈、beat-yield。

**门禁**：native 全量 **186/186 绿** + ESP 构建 SUCCESS。

**★工具链修复**：ESP 工具链漂移（裸目录 `toolchain-xtensa-esp-elf` 被覆盖成 14.2.0，framework 需 15.2.0）→ 已修复：旧 14.2.0 目录改名 `@14.2.0-backup`，`@15.2.0` 目录 cp -a 为裸目录。

**资产**：`firmware-assets/6.1.0-beta.07_20261124/`，uiBuildId `…-c5e476526276`，latest 三指针已更新。commit：`c5e4765`(代码) + `41fe447`(资产)。

**待办**：①Jordan 车上 OTA 田野验证（右滚轮按压单独验证无红轮为关键闸）②两 commit push 到 origin。

---

## 2026-11-24 · bit27 nagKill 电平型 1:1 跟随 · 6.1.0-beta.06（已被 beta.07 取代）

**断点续接（模型额度中断后恢复）**：田野判定 beta.05 沿型单发已死——单帧 bit27 淹没在 10Hz 原生流里无意义，「方向盘状态有效」声明必须持续覆盖（每个原生帧都跟一帧 bit27 版），与 DND 配对清除、0x3EE offsetOnly 速度写入同构（一切成功路线都是骑原生流、时序与原生同步）。

**v3 改造（TDD 全程 RED→GREEN）**：
- `dash_legacy_nag_kill_045.h`：沿型→电平型。`onDasStatus` 改维护 `inWindow_` 电平（hands∈[3,5] 持续），`onNative045` 只要 inWindow 且 idle 原生帧就克隆（不再消费 trigger）。hands 出窗或 ≥6 立即清电平停发。
- 删 `kTriggerExpireMs`（500ms 过期栅——沿型残留，电平型由 0x399 流驱动）。
- 保留全部安全栅：idle-only、own-echo、DND 互认让路（beat-generational yield）、disarm 自愈（800ms/60s 3次）。
- diag：`triggerArmed`→`inWindow`（/status nagKillDiag 同步改）。
- 测试重写 12/12：持续警告逐帧克隆、出窗即停、hands6 硬关、重入恢复、echo 不链、disarm 自愈。

**门禁**：native 全量 **189/189 绿**（含 nag_kill 12 例）+ ESP `lilygo_t2can_dual` 构建 SUCCESS。

**待办**：①产出 beta.06 资产（merge + latest 指针 + uiBuildId）②Jordan 车上 OTA 田野验证（bit27 单独验证无红轮为关键闸）③python 合约 3F+2E 旧账待清理。

---

## 2026-11-24 · TX 分路 v2 发版闭环 · 6.1.0-beta.05 终态（已被 beta.06 取代）

**全链 14 commit 已 ff 合 main 并 push（f2cc7a5..66451f9）**。终审（429 中断后恢复）抓出 **2C+3I**，全部修复 RED→GREEN：
- **C1 认罪**：我 Ruling 声称"沿型单发"但实现是电平型（每 0x399 重武装——reviewer 模拟 6s 警告=13 克隆而非 1）。修复=onDasStatus 边沿检测（窗外→窗内才武装）。**教训：叙述与代码分叉是最危险的失败形态——声称的语义必须有测试钉住（持续警告不重武装钉）**
- **C2**：hands≥6 硬关真实现（武装中升 6 立即清 trigger）
- **I1**：DND 让路改代际（clone 后直到下一个 idle 原生帧才还拍，20ms 窗有重复 counter 路径）
- **I2**：只骑 idle 帧（b4|b5<<8 & 0x3FFF==0——**字节序坑：switchWord=b4|b5<<8 非 b1/b2**，DND tick 回显/jitter 杆帧不克隆）
- **I3**：t2canNagKillTick 复刻 DND 全 gate 链（canActive/jitter/vehicleOTA/abortGuard/TWAI）
- **Declined-to-judge 闭环**：九段录制 10024 帧原生 0x45 bit27=0 命中（sibling 饿死风险封零）
门禁 158/158+ESP 绿+镜像自检 6/6。**OTA=firmware-assets/6.1.0-beta.05_20261123/（uiBuildId …-5535204287ad）**。latest 三指针已指向 review-fixed 版。
**待办**：①Jordan 车上 OTA（README 四步清单：默认开无回归→关开关看 offsetOnlyTxOk→bit27 单独验证无红轮→并行验证）②M1-M5 minor 延后清单在 sdd ledger ③python 合约 3F+2E 旧账待清理。

---

## 2026-11-23 深夜 · TX 分路 v2 实现（已由 11-24 终态取代）

**全链 10 commit 在分支 `tx-split-v2`（f2cc7a5..aa57d06），TDD 全程 RED→GREEN，门禁 154/154+ESP 构建绿，镜像自检 10/10，资产 6.1.0-beta.05 已产**：
1. **0x2F8 骑载整体删除**（X197 无宿主帧实证）+ 旧钉迁移
2. **master-off→offsetOnly 路由**：开关关时 mux0 走 beta02 既有纯速度通道（田野写入器 b3 bits1-6/b5|=0x03/b7 pct），gate 链全套，字节奇偶钉扎
3. **DND 事件内自愈**：800ms 复活+60s 滑窗 3 次限速（计划语义修正：per-episode 计数在 abort-复活机制下不可达——TX 两连败必跨警告清除沿）
4. **ci 纯源化**：/status ci=f sd3eeInject（canActive 耦合根除，python 合约钉）
5. **bit27 nagKill 引擎（默认关）**：警告沿单帧骑载（比参考伪代码更保守=沿型非电平型）、hands[3,5]、6+ 硬关、与 DND 互认（bit27 标记=兄弟帧）+让路（cloneInFlight 20ms）、NVS nk27
6. UI：bit27 开关在滚轮 DND 上方（实验徽标）+ 0x3EE 描述 offsetOnly 化
**待终审回来→修复→合 main→push→宣发**。OTA=firmware-assets/6.1.0-beta.05_20261123/（uiBuildId …-6492135e3b1e）。田野清单在资产 README（bit27 单独验证无红轮为关键闸）。

---

## 2026-11-23 晚 · beta.04 田野判定 TX 分路 v1 失败 → v2 设计（已被上面实现取代）

**Jordan 实车报告**：①关两开关后 DND+速度偏移失效②两开关显示不同步③开注入后车机卡顿+偶发警告④"未实现 TX 分路"。

**9 段录制数据分析四根因判决**（22:48 七段=连续下载时刻非录制时刻，实为七次先后测试循环；000600/000557=beta02 基线）：
1. **速度偏移失效=我的设计错误实锤**：A3 0x2F8 骑载移植自 Atlas P4 **三总线**（0x2F8@PT 线），T2CAN 单接 X197——**1080s 录制 0x2F8 宿主帧 0 次**，fallback 物理不可能。教训：**跨项目移植必须先验证宿主帧存在性**。
2. **DND 间歇死与 0x3EE 开关无因果**（段②两开关全关 DND 照发 10 帧配对）：真凶=TG 两连败 disarm 无自愈（段③死 14s 直到手动 toggle 复活）+爆闪占用让路。
3. **开关不同步=ci 语义污染**：首页状态源 `ci=canActive&&fsd3eeInject` vs defense 页 `fsd_3ee_inject` 两源可不一致。
4. **卡顿=速度请求 burst**（2s 5 帧 T 叠加原生流）——beta02 本有行为，关过再开被对比放大。

**★物理标定黄金数据**：X197 线上仅 7 id（45/118/186/370/399/3EE/488）；0x3EE 原生流 1.9Hz **完全静态无 counter**（97 帧逐字节同）；**beta02 引擎本来就是"原生帧后 1ms 跟发改写"**；改写差异=b3 BC→D4（bits3,5,6 激活位）+b5 C0→C3（**bits0-5=速度偏移字段**）+b7 CRC8；mux1 T=R 完全一致（回显双记）。

**v2 方案（Jordan 拍板：全骑载）**：速度偏移=0x3EE 纯速度骑载（仅改 b5 不碰 b3，1:1 跟帧零 burst）；DND=0x45 骑载不变+**disarm 新沿自愈**；开关只管激活引擎；ci 单一真源化。设计文档 `docs/TX-SPLIT-RIDE-0X3EE-DESIGN-20261123.md`，builder check 进行中。

---

## 2026-11-23 上午 · beta.04（UI 布局修复版，已被田野判定待替代）

**Jordan 台架检查抓到真 UI 缺陷并拍板双入口设计**：
1. **防护页行序 bug（我引入）**：新开关「0x3EE FSD 激活注入」插在滚轮开关与滚轮诊断表（`wheel-*` 全家：告警状态/原生0x045/配对计数/回声镜像）中间——滚轮体检表视觉上挂到 0x3EE 名下（Jordan："完全对不上"）。修复=行序 滚轮开关→滚轮诊断表→0x3EE 开关→速度免打扰，镜像内 gzip blob 解压断言钉扎（31c3766）。
2. **双入口定案（Jordan 拍板保留）**：首页大按钮与防护页开关=同一状态两入口（NVS f3ee 同源）；0x3EE 开关描述加注「与首页『模块总开关』为同一开关的两个入口，状态互通」（中英）。
3. **版本号改点号式 beta.04**：修复 parseVersion 同脉 quirk——beta.04 对所有已装版本 OTA 正确提示更新。
4. **★镜像自检坑**：固件 bin 里 UI blob 是 **gzip 流**（@~104KB），明文 token 直接搜必 FAIL——须 `zlib` 解压后断言（def-fsd-3ee-tgl/行序/中文注记全在解压层）。

**OTA 文件=firmware-assets/6.1.0-beta.04_20261123/ ota_app.bin**（uiBuildId `6.1.0-beta.04-lilygo_t2can_dual-31c3766880a6`；ad0e4b6 已 push f2cc7a5；latest 三指针已更新）。门禁 141/141+构建绿。

**待办**：Jordan 车上 OTA beta.04（点号式=从任何旧版都正确提示更新）→ 验证清单见资产 README（默认开无回归→防护页视觉→关开关+原生激活 DND/骑载/无红轮→抓 /status）。

---

## 2026-11-23 终态 · 大回退 + 6.1.0 新系列

**当天全链**（完整事实见 auto-memory `t2can-61-series-clean-slate-20261123.md`）：

1. **凌晨到白天**：beta02 两段实车录制分析（输入全健康/总线零注入=纯开关耦合）→ beta20 TX 分路发版 → 一审抓 3 缺陷 → beta21 修复发版 + 宣发四件。
2. **深夜转折**：**beta21 实车复现红色方向盘警告 → Jordan 拍板大回退**：beta03..beta21 全系作废，main 硬回退 beta02（aa4c16d）+ force push，历史存档 `archive/pre-rollback-beta21`。Desktop 的 beta17/beta21 宣发四件全部作废。
3. **6.1.0 新系列**（beta02 干净基线 + 唯一新增 TX 分路）：
   - beta01（6c652df）：0x3EE 主开关（f3ee 默认 ON=与 beta02 逐字节一致；关→0x3EE 全停+速度偏移自动 fallback A3 0x2F8 骑载+DND 天然独立）
   - beta02（e29aa9d）：**首页「模块总开关」真凶定案**（原绑 /config can:0 清 canActive=一刀切总闸，即"关一开关死三功能"元凶）→ 改绑 fsd_3ee_inject
   - **beta03 终态（444ff2a 已 push）**：二审抓 2 P1 修复——①jitter Bit46Shot 漏 gate **重演**（beta20/21 教训，main.cpp ESP-only 无 native 桩抓不住）②首页恢复死角（`!S.can` 用 canOnline 而非 canActive→旧总开关关掉的板无法恢复）
4. **★当天教训汇总**：①"写入点穷举 grep driver.send 按帧 id"教训一天内重演（设计文档写"仅 2 处"即穷举不全——**main.cpp 的 ESP-only 注入路径是盲区，设计文档必须穷举到文件级**）②UI 状态源语义迁移（ci 字段）要枚举全部前端消费者③sed 生成 README 是事故源（beta03 起 README 手写）④**版本号 quirk：同脉 beta02→beta03 OTA 判"不更新"（preNum 只解析带点 beta.N）——下次 bump 用 `6.1.0-beta.04` 或修解析器**。

**OTA 文件=firmware-assets/6.1.0-beta.04_20261123/ ota_app.bin**（beta03 资产保留但 latest 已指向 beta.04）。

---

## （已作废）2026-11-18~23 早段 · beta17-21 系列

> ⚠️ 本段及以下 11-19 段描述的 beta17/18/20/21 已随大回退**全部作废**（保留仅作历史档案；代码可 `git show archive/pre-rollback-beta21:...` 找回）。

---

## 2026-11-19 P0 热修收口

**田野事故与修复（全闭环）**：Jordan 上车报「CAN1 不工作+UI 出现 3 路 CAN+配置保存失效」，初判 P4 代码混入。**取证真相：无任何 P4 混入**（二进制探针+串口启动日志+端点实测三重证据；TWAI/canActive 全程正常、NVS 保存实际成功）——真根因=beta17 的 `appendClone27DiagJson` 多发一个闭合引号，**/status JSON 在 clone27Diag 处断裂**，断点后所有 UI 数据段为空 →「CAN1 死」「3 路 CAN 卡」「保存失效」全是同一 JSON 断裂的假象。

**修复链**：2c9ff57（删多余引号+bump beta18+CHANGELOG）→ clean 重建烧录 → 板上验收全过（/status 7764B 全解析、can3Available=false、clone27Diag/wheelDndDiag 完整、/defense_config 保存往返 ✓）→ beta18 资产 f88ba16 已 push，latest 三指针已更新。

**教训（永久）**：新增 JSON 拼接段后必须 `curl /status` 全解析验收——native_dashboard 套件不覆盖 /status 生成器，这类手误测试网抓不住；UI 数据异常先验 JSON 合法性再猜功能层。

**待办**：①实车两阶段验证照旧（短途看 DTC/告警+cloneTx 增长 → 1h+ 长途压 strike-out）②宣发四件在 Desktop（beta17 版），田野验证过后可放；若要换 beta18 版号需重出 ③抓 /status（clone27Diag+wheelDndDiag）回传分析。

---

## 2026-11-18 收工 · 明日继续

**今晚两大变更（均未 commit、未烧录——板子未连接）：**

1. **滚轮 DND 回退 beta02 基线（修"运行一段后失效"回归）**
   - Jordan 实车判定：beta02 完全没问题 → beta16 约 1h 后失效（nag 不清，最终升红轮）
   - `dash_legacy_wheel_045_dnd.h` 逐行回退 beta02 原子配对引擎：+1→confirm→−1→confirm，音量天然平衡（balancedPairs）；撤销 beta07 单帧交替（faf10fa）+ beta14 回合内自愈（2d498da）
   - **beta02 语义要点（测试已 pin，18/18）**：预算是软的——警告持续时 Idle 路径重新武装继续清（这就是 beta02 好用的本质）；瞬态 abort 回合内静默等下一个警告沿（既知特性，接受）
   - 保留增项：struckOutEpisodes 只计数不注入（state 7 = Tesla strikeout，5 次锁 Autosteer 一周，注入对抗 = 被记 strikeout）

2. **档杆手势 DND 作废 → 0x045 bit27 状态克隆 DND（新方案已实现）**
   - 作废原因：手势=动作模拟，累积"只交互不出力"必然 strike-out（红轮数学上不可避免）
   - 新路线 `dash_legacy_045_bit27_clone.h`：**状态声明**——克隆最新原生 0x45（CAN1=X197 13/14 DAS/CH 线）置 StW_Cond_Flt bit27、counter+1、CRC，**预防 nag 产生而非清除**
   - 参数照抄参考固件逆向报告（Jordan 确认准确）：**handsOn 窗{2..5} + autopilotState>3 + 1500ms 间隔**；窗到 6 硬关（永不对抗 strike-out 域）
   - 安全栅全套复刻滚轮纪律（nativeFresh 150ms/own-echo/物理滚轮不克隆/TX 两连败 disarm）；测试 16/16
   - 命名映射：NVS `def_c27`（旧 def_st 不迁移）、/defense_config `clone27_dnd`、/status `clone27Diag`、UI「Legacy 状态克隆免打扰」、abort guard 路径 `Legacy045Bit27Clone`
   - 旧档杆全套已删（header/测试/env/接线/UI）；`analyze_legacy_stalk_045_capture.py` 保留（sess_137 标定工具）
   - 设计文档：`docs/LEGACY-0X045-BIT27-CLONE-DESIGN-20261118.md`

**门禁状态**：wheel 18/18 + clone27 16/16 + dashboard 全绿；`lilygo_t2can_dual` 与 `p4_3can` 构建均 SUCCESS。

**★当晚已发版 beta17（全链闭环）**：三段 commit（7f24209→7ded1e7→7f0811d）已 push origin；资产 `6.0.0-beta17_20261118/` 四镜像自检绿 + latest 三指针；正式 beta17 已重烧上板（uiBuildId=…-7ded1e7ce8a4-…，四段 hash 全验）；宣发四件在 Desktop（主打=bit27 状态克隆双卖点，三卡目检过）。构建坑：upload 必须走 wrapper（IDF_MAINTAINER=1）；改 VERSION 必须 clean 重建。

**明日优先级**：
1. 实车两阶段：①AP 激活短途——看仪表无新告警/DTC、handsOnState 被压住不进窗 ②1h+ 长途——nag 不出现、无 strike-out、clone27Diag.cloneTx 持续增长
2. 滚轮回退版同步实车验证（正常清除、音量不漂、1h+ 不失效）
3. 若 bit27 克隆触发车端 fault（仪表告警）→ 立即关开关，抓 /status 回来分析
4. 跑完抓 /status（clone27Diag + wheelDndDiag）回传分析；田野顺利则宣发可以放出

**今晚坑归档**：测试时间跳跃必须同步喂 0x399（contextFresh 1s）和 0x045（nativeFresh 150ms）两路新鲜度；beta02 状态机 abort 后只能靠警告回落+再触发沿复活（toggle 开关不够）；longjmp 的旧 CHANGELOG [Unreleased] 与代码终态对不上时要先核对再改。
