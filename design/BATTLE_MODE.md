# 咖啡厅模式与战斗模式（raylib / Three.js）

中键打开设置，上方切换“咖啡厅模式 / 战斗模式”。两版共用同一个 C++ 状态机。
咖啡厅继续运行已有 Groot2 XML；战斗使用 `src/pet_battle.h` 中的状态机。
raylib 的 `build/settings.ini`、Three.js 的 `build/three/settings.ini` 分别保存 `Behavior/Mode`，互不覆盖。
角色缺少战斗动作时自动使用咖啡厅。Three.js 直接加载原 GLB，不需要转换。

文件：咖啡厅模板 `hibiki-cafe-attention.bt.xml` 可在 Groot2 编辑；Three.js 实际加载
`build/three/behavior/角色名.bt.xml`，raylib 使用 active-model.ini 指定的模型行为 XML。
战斗状态图提供 `battle-mode.drawio` 与 `battle-mode.mmd`，可分别用 diagrams.net / Mermaid 编辑。
状态图是设计文档，编辑图不自动改变程序；运行实现为共享 `src/pet_battle.h`。

## 动作识别与导入

按加载后的动作名称匹配，不按角色名字判断。必须具备 `Normal_Idle`、
`Normal_Attack_Start/Delay/Ing/End`、`Move_End_Normal` 和 `Move_Ing`。
Stand / Kneel 必须具备完整同组动作才参与随机选择。
匹配不区分大小写，兼容 Azusa 的 `Stand_Attack_STart`；换弹接受
`姿态_Attack_Reload` 和实际使用的 `姿态_Reload`。
缺少姿态 Callsign 时使用存在的 Normal_Callsign 并切换至 Normal。
Exs、Move_Jump、胜利、退场、换弹为可选动作，缺少时跳过对应环节。
Hibiki 单有 Exs，不具备战斗姿态，战斗按钮禁用。

raylib 新导入保留咖啡厅树引用的动作，以及可用战斗姿态和全局转场动作。
不保留 Exs_Cutin、Exs_Root、Vital_Dying、Public、额外交互等未使用动作。
`animation-selection.json` 的 `battle` 字段记录清单。旧四动作缓存需重新导入；
已有完整战斗动作的转换资源可直接使用。不会自动替用户导入或切换测试角色。

## 状态与触发

| 事件 | 执行 |
|---|---|
| 进入战斗 / 结束拖拽 | Normal_Idle |
| 拖拽（最高优先级） | 中断当前动作，循环 Formation_Pickup；释放回 Normal_Idle |
| 连续键鼠输入达到 3 秒 | 随机短走 0.6–1.2 秒；走完至少 Idle 3 秒再走 |
| 10–30 秒无输入 | 随机走 2–4 秒；每轮重新抽取等待时间 |
| 移动结束 | 随机可用姿态的 Move_End → 同组 Idle；有输入时面向鼠标 |
| 跨过可通行的屏幕 / 应用窗口边缘 | 提前播放 Move_Jump 一次，使交界处为动作中点；全程沿原方向移动，落地后继续剩余路程；无 Jump 时不检测交界，只正常移动 |
| 姿态 Idle 时点击鼠标 | 10% 概率面向鼠标 → Callsign → Idle |
| 姿态 Idle 时键盘输入 | Attack_Start → 循环 Attack_Delay |
| Attack_Delay 中连续键盘输入 3 秒 | Attack_Ing 一次 → Attack_Delay |
| Attack_Delay 中句号 / 回车 / 分号 / 问号键 | Reload 一次 → 姿态 Idle |
| Attack_Delay 中无键盘输入 10 秒 | Attack_End → 姿态 Idle；鼠标活动不重置计时 |
| 每持续键鼠输入 30 秒 | 5% 概率 Exs；结束恢复被打断的状态、动作时间和剩余行走时长 |
| 关闭其他应用顶层窗口 | Victory_Start 一次 + Victory_End 三次 → Normal_Idle |
| 1.2 秒内左键点击角色三次 | Vital_Death → Vital_Retreat 并上升到屏幕外 → 咖啡厅 |

连续键鼠输入允许不超过 3 秒的输入间隙；攻击的持续键盘输入允许不超过 1 秒的按键间隙。
问号按通常键盘的 Shift + `/` 识别，不读取输入法提交的文本。
图中未规定的短走时长、连续输入阈值、小概率和连击窗口采用以上初始值。
走动/追踪开关继续生效；两个时间输入框配置咖啡厅，战斗采用本页时间。
设置打开时暂停战斗决策和播放，正在 Jump 时先完成落地。Jump 期间关窗结算和技能等待落地，关闭走动也先落地；拖拽与三连击仍保持强制打断。
raylib 用 Ctrl+Alt+Q、Three.js 用 Ctrl+Alt+Shift+Q 直接退出，不触发胜利结算。
两版的咖啡厅散步、直线返回和战斗移动速度均为原来的 2.5 倍，并继续随缩放倍率变化。
可跨越的内部边界使用地面投影预测；桌面外边缘没有可用落点时正常结束移动，不向屏幕外起跳。
raylib 动作切换使用内置 `UpdateModelAnimationEx` 混合 0.16 秒，同步混合面部 morph；中断混合时从当前显示姿势继续。Three.js 本次不改动作混合方式。

## 验证与限制

`battle_tests` 覆盖状态转移、优先级、次数、概率边界与缺失动作。
`tests/import_selection_tests.mjs` 覆盖两模式筛选及源文件、二进制、骨架保留。
`tools/test_battle_native.ps1 -Renderer raylib|three` 用自建窗口与临时桌宠验证胜利次数、三连击、上升退场和回咖啡厅。
临时测试可用 `desktop_pet.exe --model 转换资源.glb --mode battle`，
`--mode cafe|battle` 参数本身不保存模式；`--clip` 继续绕过所有行为。

关闭检测使用 Win32 [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)，
仅跟踪可见、有标题栏、非工具窗口的顶层应用窗口。关闭标签页、隐藏窗口、
仅隐藏而不销毁窗口的应用不保证触发。矩形窗口边缘只触发跳跃，不实现实体障碍碰撞。
键鼠使用只读低级钩子，回调仅设置标记并调用
[CallNextHookEx](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc)。
钩子仅在战斗模式安装，不保存键盘文本；锁屏、安全桌面和更高权限应用的输入不保证收到。
战斗姿态、根位移、超过 480px 渲染区域的大幅动作由用户核对视觉。
