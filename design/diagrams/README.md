# ba_cafe_lite 原生版本代码与行为图

基线：当前 `native-raylib` 的 0.2.5 源码，包括上一轮未提交的待机稳定性修改。不是旧 Three.js 设计图，也不是仅依照历史需求推演的目标设计。这里仅生成文档，不修改程序、用户设置或每角色 XML。

直接打开 [index.html](index.html) 查看，支持缩放、平移、切换图表与 SVG 导出；不需要联网或安装依赖。用 diagrams.net / draw.io 打开 [ba-cafe-lite.drawio](ba-cafe-lite.drawio) 可编辑全部八页。各图同时提供 SVG、Mermaid `.mmd` 和 Graphviz `.dot` 源文件。

| 图 | 内容 |
|---|---|
| [01 代码框架](01-architecture.svg) | 启动器、场景、输入、设置、每角色行为、模型、CPU 工作线程、GPU 和 Win32、诊断 |
| [02 行为总览](02-behavior-overview.svg) | 模式切换、基础动作、移动/视线覆盖、菜单与桌面暂停、退出 |
| [03 咖啡厅行为树](03-cafe-behavior-tree.svg) | ReactiveFallback 的全部六个模板分支、实际禁用分支、能力修剪和 XML 安全回退 |
| [04 战斗状态机](04-battle-state-machine.svg) | 与 `Battle::Phase` 对应的全部 16 个状态，常规流转及缺失动作降级 |
| [05 战斗优先级](05-battle-priority.svg) | 请求缓存、拖拽、三连击、关窗结算、菜单、EX、常规攻击决策顺序 |
| [06 公共移动](06-follow-and-placement.svg) | 跟随区间、中点停止、单屏停靠、边界 Jump、碰撞停止、无输入散步 |
| [07 咖啡厅视线](07-cafe-look-tracking.svg) | 持续输入、资格条件、平滑和 30° 阈值追踪，与位置跟随独立 |
| [08 导入流水线](08-import-pipeline.svg) | 原始 GLB、多 Skin/Morph、命名映射、附件与表情保底、测量缓存与能力降级 |

## 读图时需要区分的实现细节

- **模式、Phase、覆盖不是同一层。** 模式是 cafe / battle；战斗 Phase 有 16 个；DirectedWalk 跟随/停靠与 CafeAttention 视线追踪是宿主覆盖，不能误画为新增战斗 Phase。停止跟随后保留原 Idle 姿态，不强制切回 Normal。
- **咖啡厅树不是整套运行逻辑。** 模板还保留 `ActiveInputChance → FaceCursor`，但 `App::tick` 调用咖啡厅树时临时关闭 `trackingEnabled`，故原生版该分支无效。实际视线开关与持续计时来自 `CafeAttention`，没有 50% 概率。模板 XML 中 60s/30s 等数值可由共享参数覆盖。
- **行为树顺序是打断优先级。** 上面的分支可抢占下面的分支。反应/走动完成后重置冷却。取消拖拽恢复重新评估；更换动作时通过 generation 避免旧分支停止新动作。
- **咖啡厅输入恢复返程有条件。** 之前进入无输入状态、允许散步、有本轮起点、位置跟随关闭且无 DirectedWalk 时才请求直线返程；到达后面向屏幕。开启位置跟随时不会强制返程。
- **战斗动作条件有优先顺序。** Idle 中攻击先于指挥再先于走动；AttackDelay 中换弹先于无键盘收枪再先于新攻击。持续输入是否活跃按 3s 宽限处理；缓存攻击另有 0.12s 去重，二者用途不同。
- **换弹并非在每个状态都记入队列。** `AttackStart / Attack / AttackDelay / AttackEnd` 中记录；在 AttackDelay 消费或 AttackEnd 结束时消费。有 End 时先 End 后 Reload，无 End 时直接 Reload。Reload 期间屏蔽键盘/换弹请求并清除攻击缓存，但仍允许拖拽、三连击、关窗结算打断。攻击动作播放时的按键可合并缓存为一次后续攻击，不为每个字符排队。
- **随机走动和跟随的按键响应不同。** 普通 Moving/MoveEnd 中只缓存攻击，到返回 Idle 才处理；跟随基础状态是 Idle，因此非 Jump 的按键可以进入攻击并取消跟随。跟随 Jump 时普通输入由宿主缓存到落地，普通战斗 Jump 的关窗请求也延后。用户拖拽/旋转仍可强制取消跳跃覆盖。
- **Jump 是提前规划，不是碰边才起跳。** 仅战斗、有 Jump、落点可达且没有角色阻挡时检查；边界位于动作中点；动作播放一次，移动不中断。咖啡厅不检查 Jump，缺少 Jump 正常移动。桌面外缘不能跳出；窗口仅计算未被遮挡的原始边。
- **允许散步和允许走动独立。** 无输入随机散步在两个模式都由全局 wandering 控制；有输入战斗短走由 battleWalking 控制；位置跟随不受这两个开关限制。跟随输入消失后仍可进入随机散步。
- **跟随距离按本体宽度，不按含武器的包围盒。** 静止时区间内不启动；区间外向中点移动，进入区间后仍继续至中点；最近值为零不主动退离鼠标。屏幕边界和避让优先，目标不可达时不保证精确到达中点。被阻挡后立即放弃，鼠标移动超过 16px 才解除重试锁存。
- **方向与位置分离。** CafeAttention 转身 Walk 不移动地面锚点；战斗短走先远离鼠标，允许安全曲线时末段转向鼠标。有脚部制动采样则按采样积分位移；缺少可靠采样用 `StopFallbackSeconds=0.7s`，不是旧图里的 0.28s。
- **Idle 单屏检查基于稳定边界。** 使用当前待机动作每网格采样包围盒和实际朝向，保持一个固定停靠目标。可行时行走停靠，否则一次放置。任意动画的临时摆动不意味着都能完整留在屏内，渲染包络与停靠边界分别处理。
- **EX 是保存/恢复，不是回 Idle。** EX 保存普通 Phase、当前动作时间、循环/移动标志和剩余路程；结束恢复。拖拽、三连击、关窗结算可清除这份保存状态。Reload、Jump、已在 EX/退场/胜利时不尝试 EX。
- **关窗结算指其他应用的顶层窗口销毁。** 不是关闭桌宠菜单，也不是仅当前前台应用；候选由窗口事件观察。胜利全过程面向屏幕，End 播放三次；关闭最后一个桌宠角色是确认后退出程序。
- **多角色独立状态，共享设置。** 场景每次鼠标抬键从可指挥的候选中随机一人，再由该角色按概率决定是否执行；已有指挥播放时不会再选择。模式只改变当前角色；启动和新导入大小参考当前角色缩放众数，碰撞被阻挡直接停止。
- **0.2.5 暂停逻辑是源码状态。** 睡眠/锁屏/熄屏停止行为及渲染，恢复时重置计时/缓存；菜单暂停一般行为，Jump 继续。挂起诊断并不等于已确认长期卡死完全修复。
- **菜单暂停不是所有计时器都冻结。** 战斗普通 Phase 在菜单打开时提前返回；咖啡厅树仍会 tick，纯 Idle 的动画时钟由宿主冻结，随机走动/追踪被资格条件抑制，但已运行的咖啡厅单次反应仍可能推进时间。图上的“菜单暂停”是普通活动的概括，不表示整棵咖啡厅树完全停止执行。

## 发行默认参数（不是用户当前配置）

来自 `release/defaults.ini`。已有配置优先，现有用户不会因升级被重置。数值描述图上的变量默认值，而非把所有模型的动画名或动作时长硬编码。

| 参数 | 默认 | 含义 |
|---|---:|---|
| Mode | Battle | 角色具备能力时的初始模式；历史可覆盖 |
| Wandering | 开 | 两模式无输入随机散步 |
| BattleWalking | 开 | 战斗持续输入短走 |
| InputIdleSeconds | 30s | 无输入散步门槛 |
| ContinuedInputSeconds | 5s | 咖啡厅视线/战斗短走/EX 尝试门槛 |
| InputGrace | 3s | 连续输入宽限，固定且不在菜单显示 |
| MoveInterval | 10s | 散步冷却/战斗走动间隔基础值 |
| MoveBodies | 5 身位 | 随机走动距离基础值；有输入战斗短走乘 0.5 |
| WalkTimeJitter / WalkDistanceJitter | ±50% / ±50% | 时间/距离的随机幅度 |
| CafeTracking / CafeLookMode | 开 / threshold | 咖啡厅视线；30° 转身模式 |
| Tracking | 开 | 战斗动作按条件面向鼠标 |
| Follow | 关 | 两模式位置跟随开关 |
| FollowNear / FollowFar | 1 / 5 身位 | 静止不启动区间；目标中点为 3 身位 |
| ClickProbability | 50% | 当选候选的指挥概率，不是旧图的 10% |
| SkillProbability | 1% | 每个持续输入计时周期的 EX 尝试概率 |
| KeyboardIdle | 10s | 只由键盘重置的收枪计时 |
| OutlineMode / OutlineWidth | 线条 / 60% | 无、轮廓、线条三种模式及粗细 |

速度：咖啡厅 `0.792 × zoom`；战斗 `0.792 × zoom × 3`。屏幕透视造成实际像素速度随位置变化。仅咖啡厅纯 Idle 动画以 75% 播放，其余动作正常速度。

## 动作缺失和导入限制

导入根据关键词、别名和一致动作组映射，不依赖角色文件名。战斗至少有一个 Idle + Attack 姿态配对；少姿态跳过。Start/End 可跳过；缺少 Delay 时回 Idle；缺少 MoveEnd 可用 Walk 制动；没有 Jump/Callsign/Exs/Reload 时不触发对应能力。死亡与退场均缺少则直接返回咖啡厅；VictoryStart/End 分别可降级。

咖啡厅 Idle 可退回战斗 Idle 或首个可用动作；Walk 可用 battle Move；Pickup 可用 Idle。无 Reaction 移除点击反应分支。没有任何可用动画或必要 GLB 数据不支持时导入报错。动画名以运行产生的 `.animations.json` 为准；本图使用规范语义名称。

每角色 XML 首次生成后保留，用户可在 Groot2 编辑。现有 XML 改动可能使咖啡厅分支不同，本文件绘制的是随程序提供的模板及宿主约束。战斗状态机和宿主覆盖目前不能通过该 XML 修改。

## 图源与重新生成

`graphs.json` 保存语义节点/连线，`generate.py` 是文档定义源，`render.cjs` 将 Graphviz 布局输出为 SVG、离线页面和八页 draw.io。生成工具只用于文档，不打入 EXE、不增加桌宠依赖。重新生成需 Python 3、Node 与 `@viz-js/viz`（本次使用工作区提供的 3.25.0）：

```powershell
python design/diagrams/generate.py
# NODE_PATH 可指向本机的 node_modules；也可在独立工具目录安装 @viz-js/viz。
node design/diagrams/render.cjs
```

代码依据见每张图底部的文件/函数标注。旧 `design/battle-mode.*` 是历史设计图；本目录的八页图作为当前实现的完整文档快照。
