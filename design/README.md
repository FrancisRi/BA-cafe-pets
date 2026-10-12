# Groot2 行为编辑

桌宠使用官方 BehaviorTree.CPP 4.8.0 执行 XML 行为树。Groot2 是外部编辑器；桌宠不依赖 Qt，没有更换模型、骨架或渲染方案。

当前有效文件是 `hibiki-cafe-attention.bt.xml`。旧的 `hibiki.bt.xml`、`hibiki-drag.bt.xml` 、`hibiki-cafe.bt.xml` 和 `hibiki-cafe-idle.bt.xml` 保留；启动脚本不再加载旧树。Groot2 中请关闭旧标签并打开新文件。

Three.js 版实际运行树为 `build/three/behavior/角色名.bt.xml`（日志 `CAFE tree` 给出路径），
可直接在 Groot2 编辑并每 0.5 秒热加载；它与 raylib 的模型行为文件分别保存。
战斗模式的图在 `battle-mode.drawio`（diagrams.net）和 `battle-mode.mmd`（Mermaid），
运行逻辑由两版共享 `src/pet_battle.h`。状态图是设计文件，不作为 Groot2 XML 执行。
修改战斗图不会自动更新程序；完整状态、事件和初始参数见 `BATTLE_MODE.md`。

## 开始使用

1. 运行 `tools/downloads/Groot2-v1.9.0-windows-installer.exe`，由用户完成安装。
2. 在 Groot2 的 Editor 中打开本目录的 `hibiki-cafe-attention.bt.xml`。自定义节点模型已包含在文件中；若需要单独导入，使用 Import Models 导入 `tree_nodes.xml`。
3. 关闭已有桌宠（Ctrl+Alt+Q），然后在项目根目录运行 `powershell -ExecutionPolicy Bypass -File tools/start-behavior.ps1`。
4. 在 Groot2 中编辑节点、连线或参数，保存回同一个 XML。桌宠每 0.5 秒检查文件内容，验证成功后从新树开始运行，无需重新编译。

启动脚本使用项目中的 `design/hibiki-cafe-attention.bt.xml`。直接双击 `build/desktop_pet.exe` 则读取可执行文件旁的 `behavior/hibiki.bt.xml`；构建只在该副本不存在时初始化它，避免覆盖用户编辑。

默认 `UserInactive seconds="60"` 是自主行走的总开关：当前 Windows 会话连续 60 秒无键鼠输入才允许走动，任何新输入会立即中断行走。测试行走参数时可以临时设为 0；结束后恢复 60。IdleCooldownReady 控制两次行走之间的间隔。

错误的 XML、未知节点、未知动作名或无效间隔不会替换旧树。原因写入 XML 旁的 `.error.txt` 文件及运行日志；修正并保存后再次加载。首次启动时 XML 不可用则保持安全 Idle。若通过解析的内置脚本在运行时出错，则记录原因并切回安全 Idle。有效重载会打断当前动作、重置 Idle 间隔，保留最多一次尚未消费的点击。

## 默认树与节点

当前编辑树：拖拽中播放用户选择的 `Formation_Pickup` → 点击播放 `Cafe_Reaction` → 持续输入后概率朝向鼠标 → Idle 后随机行走 → Idle。拖拽分支使用 `ReactiveSequence(IsDragging, LoopAnimation)`，开始拖拽立即打断动作和位移，松开或丢失鼠标捕获后退出分支；清除排队点击，拖拽期间暂停自主动作计时。`IsDragging` 表示左键移动超过原有 4 像素拖拽阈值。

`IdleCooldownReady` 默认等待 5–10 秒；`WalkRandom` 从可行的八个朝向中抽取一个，循环 `Cafe_Walk` 并按朝向走 2–4 秒，然后回到 Idle。节点参数 `speed=0.792` 为世界单位/秒，`min_seconds` / `max_seconds` 控制行走时长，`clip` 指定走路动作。开始时排除会走出当前主屏工作区的方向；行走中旋转遇到边界则提前停止。右键旋转会作为用户输入停止自主行走；点击或拖拽（含松开/取消）会强制朝向正前方。

地面采用固定透视摄像头和水平 X/Z 平面。角色面对镜头（朝向 0°、+Z）走近，背对镜头（180°、-Z）走远，90° / 270° 横向走动保持深度。模型本身不随屏幕高度改缩放；近大远小来自透视。拖动位置时，把鼠标位移对应的脚底屏幕点投回同一地面，所以拖往屏幕上方也会自然变小。滚轮仍可整体调整模型尺寸。

步速从 0.72 再提高 10% 到 0.792 世界单位/秒。透视让远处较小的角色每秒移动更少屏幕像素，近处相应更快；滚轮调整大小时，地面步速也乘以相同缩放倍数，动画播放速度保持原样。边界按角色留白，不再按整个 480 像素窗口留白：正常大小脚底可到左右约 90 像素、底部约 18 像素；上侧保留角色高度以防头部被裁切。

随机选择使用权重：`up_weight=0.4` 减少向上（远处）的选择次数，`up_distance=2.5` 将这些步伐持续时间乘以 2.5（默认为 5–10 秒），以补偿垂直漂移。`cursor_bias=2` 让朝鼠标方向获得最多 3 倍的额外权重。边界与鼠标偏好仍会影响最终分布，这不是把角色锁在固定位置。三个参数都可在 Groot2 的 WalkRandom 节点修改。

摄像头俯角 12°，距离约 143.28，垂直 FOV 3.957349°。当前 1920×1008 工作区的远近投影高度差约 25%，保持角色的大致显示高度；未按屏幕坐标额外缩放模型。

为保持 480×480 的窗口和原有 2× 抗锯齿开销，使用 raylib 的 `rlFrustum` 只渲染固定全屏摄像头中角色周围的区域；窗口跟随这个区域，摄像头位置和朝向不跟随角色。活动范围为所有连接显示器工作区的并集，支持负坐标和错位排列。模型验收的 `--clip` 模式继续使用旧的静态相机，不启用地面位移。

下面是可重新加入随机活动的参考配置；完整随机行为留在 `tests/default.bt.xml` 中作为测试样例，自动测试不依赖用户正在编辑的树。

优先级：点击回应 → 随机活动 → 循环待机。顶层使用 `ReactiveFallback`，两个有条件的分支使用普通 `Sequence`：普通 Sequence 会继续执行正在运行的动作，不会因点击已被消费而中断它。

| 节点 | 可编辑参数 | 行为 |
| --- | --- | --- |
| `HasPendingClick` | 无 | 检测最多一次待处理点击，不消费 |
| `ActiveInputChance` | `seconds=30`、`probability=0.5` | 连续输入达到门槛后抽取一次概率；输入间隔超过 3 秒开启新一段，热重载不重复抽取 |
| `FaceCursor` | 无 | 启动持续跟随：以眼睛为屏幕方向原点，平滑转身，略偏侧面；不指定动画 |
| `UserInactive` | `seconds` | 当前会话键鼠无输入达到指定秒数才成功；放在 ReactiveSequence 中使新输入能打断行走 |
| `PlayAnimation` | `clip`、`consume_click`、`reset_cooldown` | 开始时可消费点击；播放一次，完成返回 SUCCESS |
| `IdleCooldownReady` | `min_seconds`、`max_seconds` | 每轮只抽取一次间隔，只累计循环待机时间 |
| `PlayRandomAnimation` | `clips`、`reset_cooldown` | 从分号分隔的列表中等概率选一次，播放到结束 |
| `LoopAnimation` | `clip` | 循环播放并持续返回 RUNNING，允许高优先级分支打断 |
| `WalkRandom` | `clip`、`speed`、`min_seconds`、`max_seconds` | 在地面上沿随机可行朝向移动，循环走路动作，到时回到 Idle |

WalkRandom 还提供 `up_weight`、`up_distance`、`cursor_bias` 三个方向偏好参数，详见上文。`UserInactive` 通过 Win32 `GetLastInputInfo` 获取当前会话最后一次键鼠输入时间，无需全局键盘钩子。

默认随机动作是 `Cafe_Walk;Formation_Pickup;Exs`。可用动作：`Cafe_Idle`、`Cafe_Reaction`、`Cafe_Walk`、`Formation_Idle`、`Formation_Pickup`、`Exs`、`Tactical_Start`。名称区分大小写。

点击可以打断随机动作；回应过程中再次点击只保留一次后续回应，不反复重置当前动画。结束或打断一次性动作后重新计时。中断的旧节点不会停止新节点已经开始的动作。

参数使用固定值（或启动时已能解析的 SubTree 参数），不提供运行时黑板参数设计界面。保持单文件 XML；多个节点不应并行争用角色的唯一动画通道。身体与整个面部使用原始模型动作，不额外覆盖表情。

## 运行记录与节点模型

启动脚本把文本日志和 Groot2 `.btlog` 保存到 `artifacts/behavior/`。退出桌宠后，在 Groot2 的日志查看功能中打开 `.btlog`，查看节点执行顺序。每次有效重载新建带序号的日志分段，避免把不同树的事件写在一起。此版接入编辑、热重载和离线记录，未启用 ZeroMQ 实时 Monitor 连接。

`tree_nodes.xml` 由真实 C++ 节点注册信息生成，`hibiki-cafe-attention.bt.xml` 的 TreeNodesModel 与其一致。增加 C++ 节点后重新生成并在 Groot2 导入：

```powershell
Start-Process -FilePath .\build\desktop_pet.exe -ArgumentList '--export-node-models design/tree_nodes.xml' -WindowStyle Hidden -Wait
```

其他入口：`--behavior path.xml` 指定树；`--bt-log path.btlog` 启用记录。`--clip name` 专用于模型验收，循环指定动作并绕过行为树。

## 验证

最新版本目标 60 FPS：真实桌面 600 帧平均 59.46 FPS，透明穿透与置顶通过，日志 `artifacts/cafe60-smoke.log`。性能测试副本临时将空闲门槛设为 0，正式树保持 60。自动测试覆盖 59.9/60 秒门槛、新输入打断、8000 次方向抽样、向上距离补偿、朝鼠标偏好、尺寸与速度关系及更靠近屏幕边缘的范围。当时 15° 相机的工作区大小差为 14.84%，其他测试尺寸约 14.64%–15.03%；动画步伐观感仍由用户确认。

固定摄像头地面版本：行为和投影测试通过，覆盖走路循环、点击/拖拽取消位移、近大远小、横向大小不变、屏幕点投回地面和裁剪投影一致性。桌面测试 360 帧平均 30.03 FPS，穿透与置顶检查通过，实际朝向/位移记录见 `artifacts/cafe-smoke.log`。视觉效果由用户确认。

`ctest --test-dir build --output-on-failure` 验证点击优先级、中断、点击排队、Idle 计时、动作完成、错误重载和修改参数生效。Groot2 客户端安装与实际编辑体验仍由用户确认。

本机上述自动测试通过。`powershell -ExecutionPolicy Bypass -File tools/smoke-behavior.ps1` 已在真实桌面验证：随机动作返回 Idle、错误保存保留旧树、有效保存切换动作、日志分段、透明穿透与置顶。240 帧平均 29.27 FPS，结果保存在 `artifacts/behavior-smoke/20261010-120329-505/`。测试只改动 artifacts 下的临时 XML，不修改本目录的编辑文件。

参考：[官方 XML 格式](https://www.behaviortree.dev/docs/learn-the-basics/xml_format/)、[官方节点模型与 Groot2 接入](https://www.behaviortree.dev/docs/tutorial-basics/tutorial_11_groot2/)、[Groot2 下载与版本功能](https://behaviortree.dev/groot/)。

## 输入与朝向测试

连续 60 秒无键鼠输入进入无输入状态；从该状态恢复输入，角色沿本轮散步路径返回起点，再面向屏幕（尚未散步则直接面向屏幕）。短暂停顿不会重置朝向。持续输入 30 秒后以 50% 概率开始持续跟随鼠标，每段持续输入只尝试一次；相邻事件间隔不超过 3 秒视为持续。点击和拖拽分支优先。朝向计算以模型左右眼骨骼在 Idle 首帧的平均高度为基准，并随模型缩放；使用眼睛与鼠标的屏幕方向，不再反投影到水平面；纵向权重 0.65，让侧面过渡更多。以每秒最多 240° 沿最短角度平滑转身。随机行走的鼠标偏好仍使用地面目标。

把鼠标放在目标位置，按 **Ctrl+Alt+L** 开启持续跟随测试，再按关闭，绕过等待与概率。进入 60 秒无输入状态、点击、拖拽或右键手动旋转时取消跟随。此次没有增加后续动作；视觉由用户确认，之后可在 FaceCursor 后串接用户选择的 PlayAnimation 节点。自动测试覆盖 30 秒门槛、概率 0/1、每段只抽取一次、输入恢复与热重载保留抽取状态。

快捷键通过 Win32 注册并辅以每帧组合键状态检测；非激活窗口也可触发，按住不连续触发。触发来源与转向角度写入日志。实际键盘链路测试通过（含长按与再次按下），结果见 `artifacts/hotkey-smoke.txt`；视觉仍由用户确认。

12° 相机与眼睛高度转向：行为和投影测试通过，覆盖眼睛左右、上下目标、目标重合时保留朝向和 0.45–1.65 倍缩放。当前大小差 15.29%，测试屏幕范围为 15.08%–15.48%。最终视觉由用户确认。

设置菜单通过中键打开。菜单中的走动/追踪总开关控制默认树对应分支；用户填写的时间覆盖 UserInactive / ActiveInputChance 的 seconds 参数，保存于可执行文件旁的 settings.ini。概率、动作和随机间隔仍由 XML 决定。关闭菜单不退出桌宠。

最新调整：保持 12° 俯角，调整相机距离与视野，将正常缩放下工作区底部/顶部角色高度之比设为约 1.25；中心附近的显示大小基本保持原样。地面步速 0.792 世界单位/秒，仍随角色滚轮缩放和透视变化。

`ReturnHome` 节点放在点击/拖拽之后、自动追踪之前，使用 Cafe_Walk 和 speed=0.792。它不受 UserInactive 限制，所以恢复输入后能返回；更高优先级的交互仍能打断。当前 XML 已包含此节点。

本轮验证：行为/地面测试全部通过，包含返回整轮散步起点、到达后回 Idle、拖拽中断返回、多屏边界与屏幕空白区；独立窗口实例通过实际跨屏拖动、纯名称标题、描边开关与设置保存。两块显示器的实际范围为 3840×1449，正常模型开启描边运行 240 帧平均 59.31 FPS，透明穿透与置顶检查通过，日志 `artifacts/journey-smoke/outline.log`。视觉效果由用户确认。
