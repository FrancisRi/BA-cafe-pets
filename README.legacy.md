# ba_cafe_lite

C++20 + raylib 6.0 + Win32，窗口在 `src/main.cpp`，眼口材质和嘴型补充在 `src/model_faces.h`，咖啡厅行为树在 `src/pet_behavior.h`，战斗状态机在 `src/pet_battle.h`。目标 60 FPS，无已保存导入记录时默认使用 Hibiki。

独立的 Three.js + WebView2 试验版位于 `build/three/desktop_pet_three.exe`，直接加载原始 GLB，采用 Wiki 材质及逐网格黑色线条，详见 [experimental/three/README.md](experimental/three/README.md)。旧版入口保持不变。

新增独立原生迁移工程 [native-raylib/README.md](native-raylib/README.md)：raylib 绘制 + cgltf 动画适配层，直接读取原始 GLB，不需要 Blender 或 WebView2。入口为 `native-raylib/build/bin/ba_cafe_lite_native.exe`；使用独立数据目录，视觉效果待用户核对。

## 运行

双击 `build/desktop_pet.exe`。程序没有控制台，中键可打开设置界面，角色默认出现在屏幕右下方。

| 操作 | 功能 |
| --- | --- |
| 左键按住角色拖动 | 移动悬浮窗口；超过 4 像素判定为拖动 |
| 左键单击角色 | 咖啡厅：播放 `Cafe_Reaction`；战斗：概率指挥，三连击强制退场 |
| 右键按住并水平拖动 | 旋转角色 |
| 角色上滚动鼠标滚轮 | 缩放，范围 0.45–1.65 倍 |
| 中键单击角色 | 打开/关闭设置菜单 |
| Ctrl+Alt+L | 切换持续跟随鼠标，供视觉测试，不触发新动作 |
| Ctrl+Alt+Q | 退出桌宠 |

Idle 循环播放；当前树在用户连续 60 秒无键鼠输入后，按 5–10 秒 Idle 间隔随机行走，完成后回到 Idle。拖拽与点击优先于自主行走，回应过程中再次点击最多排队一次。行为由 Groot2 可编辑 XML 驱动，详见 `design/README.md`。透明像素不接收鼠标，角色像素接收鼠标；窗口保持置顶，不主动抢占键盘焦点。

当前咖啡厅试验树 `design/hibiki-cafe-attention.bt.xml` 将随机活动改为行走：Idle 5–10 秒后随机朝向走 2–4 秒。采用固定透视摄像头，角色在水平地面移动；屏幕上侧更远、更小，向下走近变大，横向深度与大小不变。拖拽和点击可打断行走，右键旋转可转向。地面与投影代码位于 `src/pet_ground.h`，本版支持所有连接显示器的工作区及显示器布局变化。完整参数见 `design/README.md`。

两版现有行为命名为“咖啡厅模式”，中键设置可切换“战斗模式”，共用同一个 C++ 状态机。
仅具有完整战斗动作的模型可启用，Hibiki 会禁用该按钮。
raylib 新导入自动保留两种模式需要的动作，旧的咖啡厅精简资源需重新导入；Three.js 直接使用原始 GLB。
状态、触发、初始参数及设计文件见 [design/BATTLE_MODE.md](design/BATTLE_MODE.md)。

## 构建

需要 Windows、CMake 3.24+ 和支持 C++20 的编译器。已用本机 MinGW GCC 16.1 + Ninja 构建运行。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 6
.\build\desktop_pet.exe
```

MSVC 可使用独立构建目录：

```powershell
cmake -S . -B build-msvc -A x64
cmake --build build-msvc --config Release
.\build-msvc\Release\desktop_pet.exe
```

CMake 优先使用 `vendor/raylib-6.0` 和 `vendor/BehaviorTree.CPP-4.8.0`；没有本地副本时下载固定版本并校验 SHA-256。两者静态链接，不需要额外运行 DLL。未启用 ROS、Qt、SQLite 或 ZeroMQ。构建后自动复制 `assets` 到可执行文件旁；行为 XML 仅在首次构建时初始化到 `behavior/hibiki.bt.xml`，保留后续用户编辑。默认模型与行为路径相对于可执行文件，不依赖启动目录。

## 模型与骨架

- `assets/Hibiki.glb`：从用户指定页面取得的原始文件，未修改。
- `assets/Hibiki.blender.glb`：先由 Wiki 同版本 Three.js 0.185.1 采样动画，再由 Blender 5.2 官方 glTF 工具链生成的运行资源。
- `tools/sample_wiki_animations.mjs`：使用 Three.js 官方插值器按 60 Hz 采样动画，保留原始三次样条运动，并显式记录未被动画修改的默认变换。
- `tools/blender_roundtrip.py`：官方导入/导出脚本，关闭猜测绑定姿态。没有自编骨骼编号映射或蒙皮算法。

原始 Hibiki GLB 包含身体、背包、武器三套 skin。Blender 的导入器自动构造统一的 128 骨骼 Armature，导出为单一 skin，满足 raylib 单骨架加载要求。新模型导入只转换行为树引用的动作。运行时使用 raylib 6.0 的 `LoadModelAnimations` / `UpdateModelAnimation`；随项目的 raylib 加载器已补足多根骨骼、带动画的非关节祖先节点与静态挂件跟随，详见 `tools/IMPORT_PIPELINE.md`。Wiki 处理方式与当前差异见 `design/WIKI_MODEL_REFERENCE.md`。

重新转换（资源制作需要 Node.js、Blender；运行和编译 Demo 不需要）：

```powershell
.\tools\setup_wiki_reference.ps1
node tools/sample_wiki_animations.mjs
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' --background --factory-startup --python-exit-code 1 --python tools/blender_roundtrip.py
cmake --build build
```

`setup_wiki_reference.ps1` 下载固定版本的 Three.js 官方包并校验 SHA-256，只用于资源工具和参考测试，不进入 C++ 运行依赖。原 GLB 始终保留。这里是 Hibiki 资源制作流程；用户 GLB 导入见 `tools/IMPORT_PIPELINE.md`。

动作包括 `Cafe_Idle`、`Cafe_Reaction`、`Cafe_Walk`、`Formation_Idle`、`Formation_Pickup`、`Exs`、`Tactical_Start`。动作选择在行为 XML 中修改；未知动作名会拒绝加载该树，保留旧树，首次失败则回到可用 Idle。没有模型时显示几何体角色，仍可拖动、旋转、缩放和触发反馈。

## 眼皮与嘴型

验收基准：整个面部表情（眼皮、眼球、眉毛、嘴型及其遮挡关系）均参考 Wiki 播放器，使用同一原始动作和时间点对照。用户反馈的 Idle 大小眼和眼镜、武器偏移促成了本次转换修正，尚待用户重新视觉验收。行为设计以播放完整原始动作为单位，不用额外随机眨眼或嘴型覆盖掩盖模型适配问题。

参考 Wiki 的 Three.js ModelViewer：它保留眼口材质的 alphaTest，并由动画轨道更新 morphTargetInfluences。raylib 6.0 的 glTF 加载器没有实现这两项，因此做两处小范围补充：

- `assets/alpha_cutout.fs`：经用户允许的小型片元 Shader，按 GLB 的 `alphaMode` / `alphaCutoff` 裁掉眼口贴图的透明像素，避免透明片元遮挡后面的面部。顶点 Shader 仍使用 raylib 默认实现。
- `src/model_faces.h`：使用 raylib 自带的 cgltf 读取 POSITION morph target 和 weights 动画，在 raylib 骨骼蒙皮前应用顶点位移。支持 STEP / LINEAR 权重；不修改骨架、骨骼索引或绑定矩阵。

原始 Hibiki 用三个嘴部网格的 morph target 切换嘴型，权重 1 将对应网格收缩隐藏，权重 0 显示。只有 `Formation_Pickup` 包含随时间变化的嘴型；其余六个动作（包括 Idle、点击动作）保持原始固定嘴型，不另造口型动画。眼皮仍由原始骨骼动画驱动。

同时读取 glTF 纹理 sampler，生成 mipmap 并恢复原模型的过滤方式。Hibiki 使用线性放大和三线性缩小过滤，避免 raylib 默认点采样导致贴图锯齿与闪烁。

Blender 按 Action 导出并保留 morph 动画，避免 NLA 导出将当前 shape-key Action 烘焙为所有动作的固定嘴型。转换后自动与原始 GLB 比较权重。导出采用 60 Hz 采样和线性插值，原始 STEP 切换附近可能有不超过一帧的过渡。

本次定位到的转换问题：Blender 导入器丢弃 CUBICSPLINE 切线并改用自动曲线；猜测绑定姿态使部分眼皮偏移；切换 Action 时会重置未设置关键帧的默认缩放（影响眼镜）。因此先用 Three.js 的插值器采样原始 TRS，显式保留各动作未修改的关节默认值，再关闭猜测绑定姿态交给 Blender 转换。没有按角色部件手工修正位置。

## 实现范围

raylib 在隐藏的 OpenGL 窗口中使用 RenderTexture 渲染；每帧读取 RGBA 图像，交给 Win32 `UpdateLayeredWindow` 显示。分层窗口提供逐像素透明和 alpha=0 区域的鼠标穿透，避免仅用 `HTTRANSPARENT` 时跨进程穿透不可靠的问题。RGB 使用预乘 alpha；仅增加上述透明裁切 Shader，没有自定义渲染引擎。

窗口固定 480×480；内部以 960×960 渲染后在 GPU 缩小，提供 2× SSAA，覆盖几何轮廓及 alpha 裁切边缘。缩小时直接复制预乘 RGBA，每帧仍只读取 480×480 图像。适用于小型 60 FPS Demo，不适合大量角色或大尺寸高帧率显示。大幅动作或放大后可能超出窗口边界。材质不复现原游戏的光照效果；morph 支持限于 POSITION，未实现 NORMAL / TANGENT 或 CUBICSPLINE 权重（Hibiki 不需要）。通用半透明材质未实现排序。

尚存限制：raylib 将层级骨骼变换合成为 TRS，无法完整保留非均匀缩放与旋转组合产生的 shear；未蒙皮网格的父节点动画也不会自动更新。对照已检出背包/部分身体顶点的残差，以及光环挂件偏差。当前版本不能声称已完整达到 Wiki 的 glTF 播放效果；详见 `artifacts/wiki-poses/summary.csv`。

## 验证与调试

构建及程序性验证由开发端完成；视觉效果由用户确认。自动运行并导出所有七个动作的四个时间点：

```powershell
.\tools\smoke.ps1
python tools/verify_faces.py --exe build/desktop_pet.exe
.\tools\verify_wiki_poses.ps1
```

输出位于 `artifacts/poses`，汇总图位于 `artifacts/animation-review.png`。这验证资源可加载、动画可更新和渲染、进程正常退出，不代表视觉验收通过。

`verify_faces.py` 检查单一 skin、七个动作、MASK 材质，并将导出权重及程序实际采样权重与原始 GLB 对照。运行日志与采样图保存在 `artifacts/faces`。只验证转换数据可省略 `--exe`。

本机记录：当前资源的 6,177 次源文件/导出文件权重对照通过；运行时嘴型共 12 次采样通过。七个动作共 28 个姿态采样点正常渲染退出，数值对齐结果见下一段。加入抗锯齿后的正式构建连续运行 420 帧，平均 29.91 FPS；透明区域穿透、实体命中和置顶检查通过，日志见 `artifacts/quality-window.log`。此前几何体窗口测试及随机动作结束返回 Idle 检查也已通过。鼠标拖动、旋转、缩放及视觉效果交由用户验收。

`verify_wiki_poses.ps1` 使用 Three.js 官方 GLTFLoader、AnimationMixer、SkinnedMesh 作为 Wiki 数值参考，对七个动作各取四个采样点，同时比较“原始→转换资源”和“转换资源→raylib”。转换前后使用逐材质最近顶点距离（不是完整拓扑/图像等价证明），raylib 对照使用同序顶点。当前最大转换距离约 0.000539 模型单位，略超过严格阈值 0.0005，测试会如实返回失败；同时列出更明显的运行时误差。最终视觉效果仍由用户确认。

其他调试入口：

```powershell
.\build\desktop_pet.exe --geometry
.\build\desktop_pet.exe --model C:/models/character.glb
.\build\desktop_pet.exe --clip Formation_Pickup
.\build\desktop_pet.exe --clip Cafe_Idle --sample 1 --frames 1 --snapshot artifacts/idle.png
.\build\desktop_pet.exe --geometry --check-window --frames 120
```

`--check-window` 在第 4 帧通过 Windows 命中查询检查透明区域穿透、实体区域命中和置顶样式，至少运行 4 帧。启动参数错误、窗口检查失败或初始化失败返回非零退出码。用 PowerShell `Start-Process` 的输出重定向可以保存日志；程序不弹出控制台。

视觉验收重点：眼镜与背包位置、Idle 的脚部是否稳定、各动作中身体是否保持完整，以及点击后能否返回 Idle。交互验收：拖动、右键旋转、滚轮缩放和在透明区域点击后面的其他应用。

眼口验收可运行 `--clip Formation_Pickup`：约 0.53、2.27、4.20、5.57、9.50 秒切换嘴型。`--clip` 验收模式循环指定动作并绕过行为树，便于反复观察。重点检查眼皮是否正确遮挡眼睛、嘴部是否残留重叠贴片。

## 行为设计与后续计划

咖啡厅行为编辑使用 Groot2；`design/hibiki-cafe-attention.bt.xml` 已接入官方 C++ 执行器，支持验证后热重载及 Groot2 离线执行日志。运行 `powershell -ExecutionPolicy Bypass -File tools/start-behavior.ps1` 后，在 Groot2 修改并保存这份 XML 即可生效。操作步骤、节点语义和限制见 `design/README.md`；安装包已准备在 `tools/downloads/`，由用户完成安装，客户端编辑体验尚待确认。未启用实时 Monitor。之前的网页仅保留为交互原型。用户 GLB 导入已接入原生菜单，限制与依赖见 `tools/IMPORT_PIPELINE.md`。

用户已确认本版动作稳定。保持现有模型、动画与渲染方案；本次新增的 BehaviorTree.CPP 仅负责行为决策，不更换骨骼动画库。

## 来源

- [Hibiki 模型页面](https://bluearchive.wiki/wiki/Models#Hibiki)
- [Wiki ModelViewer 实现](https://cdn.jsdelivr.net/gh/lihaohong6/MirahezeDevScripts@dist/dist/ModelViewer/gadget-impl.js)：morph 轨道及 alphaTest 的参考。
- [glTF 2.0 规范](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)：morph 位移在蒙皮与节点变换之前应用。
- [原始 GLB 固定版本](https://cdn.jsdelivr.net/gh/lihaohong6/BlueArchiveModels@525ae0faeb0a89f54ef4023f3be3692dcb529e51/Hibiki.glb)
- [raylib 6.0 官方发布](https://github.com/raysan5/raylib/releases/tag/6.0)，raylib 使用 zlib 许可证（本地 `vendor/raylib-6.0/LICENSE`）。
- [Windows 分层窗口说明](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#layered-windows)

角色模型为独立的第三方素材，未因本 Demo 获得新的授权；不把模型的许可等同于 raylib 的代码许可证。

进入 60 秒无输入状态后，恢复输入时先走回本轮散步起点，再面向屏幕。持续输入 30 秒后，以 50% 概率开始持续跟随鼠标；相邻输入间隔不超过 3 秒视为持续，同一段只抽取一次。当前仅测试转向，后续动作由用户选择。可按 Ctrl+Alt+L 切换持续跟随测试。

## 设置菜单

中键单击角色打开设置；标题为当前角色名称（去掉文件扩展名和生成资源的 .blender 后缀），右上角叉仅关闭菜单。白底、浅蓝分组和青蓝开关参考《蔚蓝档案》的配色方向。

- 走动：总开关；Input idle 时间为允许自主行走前的键鼠空闲秒数，也用于无输入状态恢复判断。初始默认 60 秒，仍保留树中的走动间隔。
- 追踪鼠标：总开关；Continued input 时间为抽取跟随概率前的持续输入秒数，初始默认 30 秒，概率仍为行为树设置的 50%。关闭也会停止已开启的跟随，并禁用测试快捷键的开启功能。
- 输入范围 1–3600 整数秒，有效输入立即应用；无效输入显示提示并保留上次有效值。菜单显示期间暂停自主走动和朝向跟随。
- 设置保存到可执行文件旁的 `settings.ini`，下次启动读取。设置过的时间优先于 Groot2 对应节点的 seconds 参数；动作和概率仍由行为树控制。

行为测试及独立原生窗口测试通过：开关、时间覆盖、输入校验、保存、关闭与重新打开。

本次更新：两版散步、返回和战斗移动的速度均乘以 2.5，默认实际速度为 1.98 世界单位/秒再乘角色缩放倍率（树内基础 speed 仍为 0.792）。一次无输入期间的散步只记录本轮起点；恢复输入后沿直线走回起点，再面向屏幕。点击、拖拽、右键操作或关闭走动会取消返回；设置菜单打开时暂停返回。随机走动使用显示器工作区并集，排除显示器之间的空白区域；直线返程可能短暂经过屏幕间隙；改变显示器布局时重新定位并取消旧返回目标。

设置菜单新增“模型描边”，即时开关并保存：以角色透明度生成约 2 像素深蓝外轮廓，不改变 GLB 或内部材质。采用轻量原生圆角卡片布局，无新增 UI 框架。

本轮验证：行为/地面测试全部通过，包含返回整轮散步起点、到达后回 Idle、拖拽中断返回、多屏边界与屏幕空白区；独立窗口实例通过实际跨屏拖动、纯名称标题、描边开关与设置保存。两块显示器的实际范围为 3840×1449，正常模型开启描边运行 240 帧平均 59.31 FPS，透明穿透与置顶检查通过，日志 `artifacts/journey-smoke/outline.log`。视觉效果由用户确认。

## 自动导入 GLB

中键设置菜单新增“导入 GLB 模型…”。完整流程、依赖、缓存与已知限制见 [导入说明](tools/IMPORT_PIPELINE.md)。原始 GLB 保留不动；成功后保存当前模型，下次启动自动读取。Azusa 使用与 Hibiki 相同的官方转换路线，并补足蒙皮节点变换与多个根骨骼的通用兼容处理。

导入期间，设置菜单显示进度条、当前步骤、动作检查计数与已用时间。步骤百分比不等同于耗时比例；失败会显示提示并保留当前角色。
