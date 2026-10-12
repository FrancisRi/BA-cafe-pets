# ba_cafe_lite 0.2.0 · raylib + cgltf 原生版

独立 Git / CMake 工程，只包含 raylib + cgltf 原生版。模型、行为头文件和状态机均位于本仓库内，无需上级目录或旧版本源码。
本版直接读取用户的原始 GLB，不运行 Blender，不需要 Node、Python 或 WebView2。
默认 Shiroko；便携版设置和历史在程序旁的 `data/`，单 EXE 版在 `%LOCALAPPDATA%/ba_cafe_lite_native/data/`。首次使用空历史，不包含开发机用户数据。

## 构建与启动

在仓库根目录执行（C++20、CMake 3.24+、Windows SDK 或 MinGW）：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ba_cafe_lite_native -j 4
./build/bin/ba_cafe_lite_native.exe
```

或者运行 `./build.ps1`。发行打包运行 `./package.ps1`，输出单 EXE、便携 ZIP 和 SHA-256 校验文件到本仓库的 `dist/`。打包脚本使用 PowerShell 7；用户运行发行版不需要开发工具。
打包后也会在项目根目录生成 `ba_cafe_lite.exe`，可以直接双击；此发行入口不提交到 Git。
请一起保留 `models/Shiroko.glb`、`defaults.ini` 和 `behavior/` 文件夹。

右键点击角色打开设置；右键拖动旋转，松开后不打开菜单。左键拖拽、滚轮缩放；中键拖动也可旋转。
咖啡厅待机动画按 75% 速度播放；走路、点击动作及行为等待时间保持原速。菜单字体和布局间距扩大 50%。
退场上升位移采用先慢后快的加速曲线，动作时长和总位移保持一致。
菜单分为全局、咖啡厅、战斗、角色；红色“退出”关闭全部角色。
首次显示右键提示，打开菜单后保存已读状态。
“导入原始 GLB”直接读取模型；请保留原文件，历史记录其位置。
历史最多 15 条，同时显示最多 5 个。

指定模型或独立数据目录：

```powershell
./build/bin/ba_cafe_lite_native.exe --model "C:/Models/Iori.glb"
./build/bin/ba_cafe_lite_native.exe --data-dir "C:/MyPetData"
```

日志：`data/logs/pet.log`、`pet.err`。错误记录包括解析、资源加载、动画映射和着色器信息。
0.1.1 改用异步 Raw Input 观察按键／点击，不安装全局低级键鼠钩子，避免渲染阻塞系统鼠标。
参考旧 raylib 版的小画面读回方式，每帧只处理当前变形后的角色范围，包含描边余量和首次提示；世界坐标、镜头和大小计算不受显示裁剪影响。
日志每三秒输出 `PERF native`：帧率、实际画面尺寸、蒙皮／GPU 读回／窗口提交平均耗时，便于继续定位性能问题。
0.1.2 使用蓝白侧栏菜单、卡片内容区、开关和角色列表；底部常驻导入状态与进度。
缓存骨架遍历顺序、子节点索引、动画通道计算缓冲和各网格实际使用的骨骼版本。只有依赖的骨骼／Morph／变换改变时，才重新计算和上传该网格。
复用未变化的 alpha 命中区域；画面完全不变时复用上一帧，包含打开菜单后的静止画面。动态动画仍按 60 FPS 绘制，不预烘焙动画，也不减少蒙皮或表情精度。
显示裁剪位置在余量足够时保持固定；每帧绘制使用最新的同一份窗口坐标，减少边界重居中与旧状态造成的细微抖动。
`PERF native` 增加每帧实际更新网格数量、画面与命中区域缓存次数；实际 CPU 降幅需在同一模型、大小和动作下由用户核对。
多角色帧率优化：相同骨骼权重组合共享每帧蒙皮矩阵；待机稳定后收缩动作留下的过大画布；精确合并命中区域的连续矩形；只有实际绘制时才提交隐藏的 OpenGL 窗口。仍保留至多 2x SSAA 和 60 FPS 目标，不减少动画采样精度。
日志额外区分命中区域提交（mask）与分层窗口提交（layer）耗时。多角色是否稳 60 FPS 需要在用户设备上实测。
开发命令仍支持 `--clip`、`--sample`、`--snapshot`、`--duration`，本次未运行自动视觉测试。

## 模型适配层

- `gltf_model.cpp` 使用 cgltf 解析和读取 accessor；完全绕过 raylib 的 `LoadModel/LoadModelAnimations`。
- 保留每套 skin 的 joints 和 inverseBindMatrices，以及非骨骼父节点、原始局部 TRS／matrix。
- 在完整父节点层级上计算世界变换。蒙皮使用各自的 `jointWorld * inverseBindMatrix`，不手动合并骨架或修改角色骨骼。
- 保留 Morph 目标与默认权重；支持节点／子网格表情权重通道。
- 拖动动作增加缺失替代面部的保底：只有模型唯一的 `switch` 面部、单目标塌缩成点且 STEP 权重为 0/1 时才保留基础面部（如 Ibuki）；保留眼部骨骼与嘴型动画，其他动作和普通表情 Morph 不受影响。日志输出 `FACE pickup fallback`，原始 GLB 不修改。
- 原始 STEP、LINEAR、CUBICSPLINE 插值，四元数 LINEAR 使用 slerp；不将全部动画烘焙为 60 FPS。
- 动作切换从当前显示姿态混合到新动作；缺少通道恢复原始默认值。默认过渡时间最多 0.18 秒。
- 按当前 JS 版的关键词、动作组和部分命名别名映射咖啡厅／战斗动作；歧义动作跳过并记录日志。
- 参考 Wiki 和当前 JS 版的 `optional` 与动画 `show/hide`，处理盾牌及其他可选部件。
- 可选部件增加通用动画依赖检查，预先按动作缓存：检查其实际使用的骨骼、附件父节点和 Morph 通道。只移动共用的角色根节点不算独立道具的姿态动画；缺少驱动时隐藏，即使动作声明 `show`。正常依附手／头骨骼的道具仍能显示。主体和非可选静态部件不应用此规则。
- 日志的 `PART animation guard` 记录每个可选部件缺少驱动的动作数量。Hoshino 普通战斗动作的盾牌因此隐藏，包含盾牌动画的 EX 动作仍按模型声明显示；不修改原始 GLB 或手动偏移骨骼。
- 按必要动作低频采样固定显示边界；仅对停止移动动作采样脚部轨迹。大小、窗口边界不随 Idle 的骨骼抖动重新计算。

规范基线：[glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)、[cgltf](https://github.com/jkuhlmann/cgltf)。
视觉算法基线：Blue Archive Wiki ModelViewer 的材质、表情、描边、动画处理。

## 已迁移的行为

复用当前 C++ 行为实现：咖啡厅／战斗模式、三连击切换／退场、攻击与缓冲换弹、技能、指挥、胜利、随机走动、曲线停止和脚部制动、输入 debounce、视线／位置跟随、跨屏、战斗边界 Jump、避让、深度顺序、多角色历史、共享参数和统一大小。
行为树模板和战斗状态图随程序提供。咖啡厅没有 Jump。

## 本版的边界

这是供用户核对效果的原生迁移原型，尚未确认视觉等价或导入／常驻性能。

- 第一版使用 CPU 计算 Morph 和蒙皮，再更新 raylib 的动态顶点缓冲；GPU 负责绘制。
- 透明窗口采用独立的 Win32 分层显示窗口和 alpha 输入窗口。每帧只从 GPU 读回当前角色范围的 RGBA 并提交 `UpdateLayeredWindow`；大幅缩放或多角色时仍需用户核对性能。
- 不透明材质、MASK 裁切、BLEND、双面、顶点颜色、基础色贴图与 UV 变换按 glTF 读取；采用 Wiki 的无光照显示风格和 sRGB 输出。
- 支持无／轮廓／逐网格线条。线条使用平滑法线的反向外壳；轮廓使用 alpha 后处理；采用至多 2x SSAA。
- 首版原生设置使用 Win32 控件，功能迁移，外观不与网页菜单完全相同。
- 只支持三角网格、内嵌 PNG/JPEG 纹理与缓冲区的 GLB 2.0。不含 Draco、Meshopt、KTX2 等解码器；遇到未支持的必需扩展会明确报错。
- 当前没有发布安装器或自动更新。首次打开会按本版随包参数建立独立数据目录。

模型及动画保持原文件，不写回 GLB。共用行为和移动头文件位于 `shared/`，默认模型在 `assets/`，行为树和状态机在 `design/`。CMake 优先使用本仓库的 `vendor/`；缺少时自动获取固定版本并校验 SHA-256，不依赖上级目录。构建目录、发行包和用户数据均不进入 Git。
