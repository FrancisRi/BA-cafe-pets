# 开发、验证与发行

## 工具与构建

目标平台为 Windows 10/11 x64。运行程序需要支持 OpenGL 3.3 的显卡驱动；开发需要 C++20 工具链、CMake 3.24+ 和 Ninja。

`build.ps1` 使用 MinGW / g++，优先查找 PATH 中的工具，也支持本地 `.tools/w64devkit`、`.tools/ninja` 和 `.tools/cmake-*`。工具链不随 Git 上传。MSVC 可直接通过 CMake 配置构建。

```powershell
./build.ps1
./build/bin/BA-Cafe-Pets.exe --settings
```

或使用已在终端配置好的 C/C++ 编译器：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ba_cafe_lite_native -j 4
./build/bin/BA-Cafe-Pets.exe --settings
```

CMake 会下载并校验固定版本的 raylib 6.0 与 BehaviorTree.CPP 4.8.0。可将相应源码放入 `vendor/raylib-6.0`、`vendor/BehaviorTree.CPP-4.8.0` 进行本地构建；首次下载需要网络。cgltf 位于 `shared/`。

本分支从 **1.0.0** 开始独立编号，不沿用原作者版本号。`0.2.15` 仅表示源码来源基线。默认源码构建、便携版与单文件版内部程序名统一为 `BA-Cafe-Pets.exe`；CMake 目标标识和持久数据目录保留，以维持构建与数据兼容。对外发行文件使用 `BA-Cafe-Pets-版本号` 命名。

## 代码入口

| 文件 | 职责 |
| --- | --- |
| `main.cpp` | 单个角色、原生窗口、模型加载和动作更新 |
| `scene.h` | 多角色、共享设置、模型库同步及窗口层级 |
| `native_settings.h` / `native_fluent.h` | 快捷菜单、完整界面、模型右键菜单及主题 |
| `native_model_import.h` | Win32 多选文件结果解析及批次处理 |
| `native_model_library.h` | 模型复制、校验、去重、文件发现与回收站删除 |
| `native_model_storage.h` | EXE 目录解析、保存位置设置、旧模型记录恢复与备份 |
| `native_tray.h` | 通知区域图标、Explorer 重建恢复、窗口入口与模型快捷菜单 |
| `native_antialias.h` | 离屏 MSAA、颜色／深度缓冲、采样解析和显存预算回退 |
| `native_renderer.cpp` / `gltf_model.cpp` | 渲染、GLB 解析、动画与蒙皮 |
| `shared/` / `design/` | 行为实现、行为树与状态机 |
| `release_launcher.cpp` / `package.ps1` | 单文件启动器和发行打包 |

## 回归检查

静止咖啡厅视线回归用实际 GLB、模拟鼠标坐标和系统空闲时间，覆盖平滑与 30° 两种追踪：3 秒输入间隔后重新指向另一侧、超过 30 秒空闲后的眼部悬停、120 秒空闲后恢复输入、长期静止不位移以及关闭视线开关。`ba_cafe_lite_follow_tests` 另检查静止策略无等待触发、暂停优先级和移动启用时保留旧输入策略。

`shared/model_view.h` 为渲染、投影放置边界和眼部缓冲区共享显示变换。上仰 5° 在水平朝向之后施加，保持侧面竖直和脚下原点，不调整地面相机与移动坐标。单元回归覆盖所有主朝向和支持的缩放，像素边缘回归检查列队靠底部的实际可见范围。

Exs 将触发时的逻辑朝向保存到 `skillYaw`，显示时相对叠加 90° 的动画补偿。手动双击的第一次点击保留朝向，自动战斗技能在进入 Skill 状态时捕获方向；技能期间鼠标和面向请求不覆盖此值。回归覆盖多个正负角度、角度环绕、完成、提起中断、三连击和固定朝前的列队。

GLB 缩放显隐按组合后的蒙皮矩阵相对绑定姿态的缩放判断，仅当一个绘制部件全部实际使用的骨骼均缩到约千分之一时隐藏该部件。相互补偿的祖先缩放及正常小道具保留；渲染、归一化和放置使用同一判定，边界只统计实际索引引用的顶点。提放回归会检测骨骼显隐变化，并验证这些骨骼直接使用目标 TRS，防止平滑过渡把隐藏道具从远处飞回手中。

交互模式回归使用独立资料、隐藏真实角色窗口和模拟鼠标，覆盖全模式三连击循环（跳过缺失模式）、列队双击技能及同一套待机恢复、滚轮缩放与范围限制、双击及自动 Exs 的 +90° 释放朝向补偿、技能结束／提起／第三次点击中断、鼠标朝向锁定、列队长时间静止／提放／旋转限制、旧参数保留与模式重启恢复。可额外传入只读 GLB；再追加一个输出目录可导出 Exs 分段截图，检查动画本身的转身及道具朝向。以 Hifumi 模型验证父子补偿缩放在提放过渡中不放大。

```powershell
cmake --build build --target ba_cafe_lite_interaction_tests -j 4
./build/ba_cafe_lite_interaction_tests.exe (Join-Path $PWD ('artifacts/interactions-' + [guid]::NewGuid().ToString('N')))
```

附加 `GLB 输出目录 最大底边留白像素 [缩放]` 可验证屏幕底部放置：通过实际 Win32 提放消息，在各可用模式拖到底部，采样待机五个时间点并按渲染像素检查角色底部。像素测量忽略孤立的亚像素描边点。睡衣小雪列队回归使用 80 像素上限，分别检查 0.45、1、1.21 和 1.65 缩放；原故障会留下约 350 像素空白。放置边界仅排除当前姿态已退化的三角形，模型归一化、原始动画与显示范围保持独立。

开机自启与描边数值框的回归：

```powershell
cmake --build build --target ba_cafe_lite_autostart_tests ba_cafe_lite_ui_tests
./build/ba_cafe_lite_autostart_tests.exe (Join-Path $PWD ('artifacts/autostart-' + [guid]::NewGuid().ToString('N')))
./build/ba_cafe_lite_ui_tests.exe
```

自启测试将注册表操作限制在随机生成的 `HKCU/Software/BA-Cafe-Pets-Tests/` 测试键，结束后清除该键，不修改真实 `Run` 启动项。界面回归检查浅深主题与多 DPI、数值框的键盘／失焦行为、范围校验、滑块同步、绘制一致性及自启开关消息。主场景测试也使用注入的独立注册表服务。

先完成应用构建，再构建测试目标：

```powershell
cmake --build build --target ba_cafe_lite_ui_tests ba_cafe_lite_model_library_tests ba_cafe_lite_model_import_tests ba_cafe_lite_model_storage_tests ba_cafe_lite_topmost_tests ba_cafe_lite_follow_tests ba_cafe_lite_drag_follow_tests ba_cafe_lite_antialias_tests -j 4
./build/ba_cafe_lite_ui_tests.exe
./build/ba_cafe_lite_model_import_tests.exe
./build/ba_cafe_lite_topmost_tests.exe
./build/ba_cafe_lite_follow_tests.exe (Join-Path $PWD ('artifacts/follow-' + [guid]::NewGuid().ToString('N') + '.ini'))
./build/ba_cafe_lite_drag_follow_tests.exe (Join-Path $PWD ('artifacts/drag-follow-' + [guid]::NewGuid().ToString('N')))
```

抗锯齿测试使用隐藏 OpenGL 窗口，不读取用户模型；比较真实 GPU 斜边覆盖与 32×32 子采样参考，验证透明预乘颜色、画质切换、缓冲绑定恢复和显存预算回退：

```powershell
./build/ba_cafe_lite_antialias_tests.exe
```

Morph 描边回归验证收缩隐藏、共享顶点访问器、普通表情、混合目标和动作过渡的区分：

```powershell
cmake --build build --target ba_cafe_lite_morph_outline_tests
./build/ba_cafe_lite_morph_outline_tests.exe
```

可追加一个或多个只读 GLB 路径，使用真实加载与 CPU 几何缓存路径检查隐藏、过渡和恢复；带模型时仅创建隐藏 OpenGL 窗口，不读取或保存用户设置。

使用 Visual Studio 等多配置生成器时，构建命令加 `--config Release`，测试程序位于 `build/Release/`，应用位于 `build/bin/Release/`。实际模型画面应额外检查 Ibuki、Reisa 的 `Formation_Pickup` 表情切换前后；可用 `--clip Formation_Pickup --sample 4 --snapshot <输出.png>` 固定动画帧，并使用独立的 `--data-dir`。

拖动回归使用真实角色、Win32 拖动消息及模拟鼠标位置，检查逻辑位移、实际渲染窗口位移和视线恢复。窗口保持隐藏，不移动用户鼠标。测试资料路径必须不存在；可依次附加只读模型路径、`cafe`／`battle`、缩放值，例如模型路径后使用 `cafe 1.24`。额外模型不会复制进发行包。

咖啡厅拖动回归还覆盖快速拖放、旋转中断后继续散步、再次拖放更新返回点，以及遇阻停止后恢复输入返回最后拖放位置。`ba_cafe_lite_presentation_tests` 另外检查临时路线清空及完成返回后仍保留该位置。

模型库测试会创建文件并测试回收站删除，参数必须是尚不存在的独立测试目录：

```powershell
$scratch = Join-Path $PWD ('artifacts/model-library-' + [guid]::NewGuid().ToString('N'))
./build/ba_cafe_lite_model_library_tests.exe $scratch
./build/ba_cafe_lite_model_storage_tests.exe (Join-Path $PWD ('artifacts/model-storage-' + [guid]::NewGuid().ToString('N')))
cmake --build build --target ba_cafe_lite_model_unification_tests
./build/ba_cafe_lite_model_unification_tests.exe (Join-Path $PWD ('artifacts/model-unification-' + [guid]::NewGuid().ToString('N')))
```

统一模型目录测试覆盖自带白子、旧目录发现、重名冲突、隐藏角色、选中状态／模式／大小、重启、防止旧副本重新出现和保存失败回退。原生场景测试使用真实 GLB、独立资料及隐藏窗口，检查迁移发生在模型加载前后时演员路径与记录同步；原始模型和旧副本保持不变。

打包后可用 Windows 桌面验证单文件启动器的旧记录恢复。脚本将 EXE、模型与数据复制到独立测试目录，不改动实际用户资料：

```powershell
python tests/launcher_storage_compatibility.py ./dist/BA-Cafe-Pets-1.1.11-x64.exe
python tests/tray_integration.py ./dist/BA-Cafe-Pets-1.1.11-portable-x64.zip
python tests/tray_background_regressions.py ./dist/BA-Cafe-Pets-1.1.11-portable-x64.zip
python tests/tray_background_regressions.py ./dist/BA-Cafe-Pets-1.1.11-x64.exe
```

界面回归检查使用隔离 Windows 桌面，不把模拟模型写入实际用户数据。测试还比较滚动后的实际窗口像素与完整重绘结果，并核对滑块普通绘制和合成绘制一致；覆盖浅色／深色主题及 100%／150%／200% DPI，同时验证滑块拖动和键盘操作。

`tray_background_regressions.py` 在独立资料中预置三个隐藏模型，验证托盘重复右键通知后菜单仍可操作、从全隐藏状态选择其他模型及连续启用多个模型。单文件测试还隔离启动器缓存，避免共享用户资料；开发时可对 ZIP 加 `--core build/bin/ba_cafe_lite_native_release.exe` 检查重建的内核，正式发布必须直接测试两个完整发行包。

可选截图输出：

```powershell
./build/ba_cafe_lite_ui_tests.exe artifacts/ui-preview
```

未提供历史文件时，截图使用空模型列表。可额外传入一个 `characters.json` 进行只读预览。

手工验证请使用独立数据目录。模型默认写入程序旁的 model 文件夹，导入测试前应在模型管理中选择专用测试目录，或将程序及资源复制到隔离目录后运行：

```powershell
./build/bin/BA-Cafe-Pets.exe --data-dir ./artifacts/manual-data --settings --diagnostics
```

重点检查多选导入、重复模型复用、删除取消、文件被占用、删除正在显示的角色、模型库刷新、五角色显示上限，以及完整界面打开时动画持续。隐藏或删除全部模型后继续在托盘运行；验证托盘恢复、重启保持隐藏、空模型库导入与显式退出，确认记录和本地文件均已同步。

更多行为及性能测试目标位于 `tests/`；上游算法说明保留在 [原版 README](UPSTREAM_README.zh-CN.md) 与 `design/`。

## 打包与发布

打包使用 PowerShell 7 与 Windows `makecab.exe`：

```powershell
pwsh -File ./package.ps1
```

脚本生成 `BA-Cafe-Pets-版本号-x64.exe`、便携 ZIP、SHA-256 校验文件及发行清单，输出到 `dist/`，同时生成根目录的 `BA-Cafe-Pets.exe`。只打包默认模型、程序资源和许可文件，不打包本地 `data/` 或额外模型。

使用 Visual Studio / MSVC 已构建的 Release 配置时，可直接指定二进制目录与内核名称，无需安装 MinGW：

```powershell
cmake --build build --config Release --target ba_cafe_lite_native ba_cafe_lite_release_launcher
pwsh -File ./package.ps1 -SkipBuild -BinaryDirectory ./build/bin/Release -CoreExecutableName BA-Cafe-Pets
```

两个发行包均附 `LICENSE.md`、`docs/LICENSE-SCOPE.md` 和 `licenses/GPL-3.0-only.txt`。GPL-3.0-only 仅为 FrancisRi 自有原创新增及修改的授权；不得因此将上游整份代码标记为 GPL。分发者仍须满足 GPL 对相应作品的适用要求，并取得其他必要授权。发布时应提供实际发行对应的完整提交或源码归档，包含构建脚本、接口和必要来源；不要只指向持续变化的默认分支。

正式发布前需更新 CMake 项目版本号和变更记录，确认二进制版本、默认配置、许可文件及空用户数据的启动结果，再将产物发布至 [本分支 Releases](https://github.com/FrancisRi/BA-cafe-pets/releases)。Git 标签使用本分支版本号，例如 `v1.0.0`，与程序版本及发行文件名对应。

单文件启动器仍使用原数据目录 `%LOCALAPPDATA%/ba_cafe_lite_native/data/`，与原版共用该位置。需要完全独立配置时，可使用便携版或通过原生程序的 `--data-dir` 指定目录。升级前退出旧进程，并在关闭程序后备份整个 `data/`。

应用图标源文件为 `assets/app-icon.svg`。修改后运行 `python tools/generate_app_icon.py` 生成多尺寸 ICO；该开发脚本依赖 Pillow，发行程序不需要 Python。
