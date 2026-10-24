# BA Cafe Pets

面向 Windows 10/11 x64 的 Blue Archive 原生桌宠，使用 C++20、Win32、raylib 与 cgltf，支持咖啡厅／战斗动作、多角色显示和本地模型管理。

**原作者：[maxiumlee（Max）](https://github.com/maxiumlee)**

**原始项目：[maxiumlee/ba-cafe-lite](https://github.com/maxiumlee/ba-cafe-lite)**

**本分支维护：FrancisRi · [BA-cafe-pets](https://github.com/FrancisRi/BA-cafe-pets)**

本项目基于原版 `0.2.15`（提交 `e6f5cba`）继续开发，采用独立版本号，首个正式版为 **v1.0.0**，保留原始 Git 历史及第三方声明。完整致谢见 [CREDITS.md](CREDITS.md)，许可状态见 [NOTICE.md](NOTICE.md)。

## 本分支功能

- Windows 11 Fluent Design 风格的快捷菜单、完整设置界面和模型操作菜单，适配深浅主题及 DPI 缩放。
- 右键桌宠，快速勾选模型显示／隐藏，切换咖啡厅／战斗模式、跟随、散步及视线追踪。
- 任务栏通知区域常驻图标：单击打开主界面，右键快速切换模型；隐藏最后一个模型后保持后台运行，重启也保留隐藏状态。
- 桌宠保持置顶；完整主界面使用普通窗口层级，打开、最小化或恢复主界面时桌宠继续活动。桌宠右键快捷菜单保留临时暂停行为。
- 按住 Ctrl／Shift 一次多选导入多个 GLB，逐个保存，单个文件失败不打断整个批次。
- 导入模型默认复制到程序旁的 `model/` 文件夹，可在模型管理中更改保存位置或恢复默认；相同内容复用，同名不同内容分别保存。
- 模型管理列表与本地模型文件统一，支持刷新发现直接放入模型库的 GLB，不再受旧的 15 条历史限制；最多同时显示 5 个角色。
- 在模型列表右键选择模型，可显示／隐藏、设为当前、在文件夹定位或删除。删除本地副本会同步移除记录，并将副本移入回收站；外部原文件及自带模型保留。
- 均衡／清晰档结合离屏 MSAA 与超采样平滑模型和线条边缘；轮廓描边增加多点采样，省电档保留轻量渲染。
- 保留原项目的动作、跟随、避让、渲染设置、共享参数、统一大小和自动保存等功能。

详细记录见 [release/CHANGELOG.md](release/CHANGELOG.md)。

## 获取与运行

从[本仓库 Releases](https://github.com/FrancisRi/BA-cafe-pets/releases/latest)下载 Windows x64 发行包：

- `BA-Cafe-Pets-1.1.3-x64.exe`：单文件版，双击运行，首次启动自动释放离线资源。
- `BA-Cafe-Pets-1.1.3-portable-x64.zip`：便携版，完整解压后运行 `BA-Cafe-Pets.exe`。
- 对应的 `.sha256` 文件：用于核对下载文件的 SHA-256。

也可以按下列步骤从源码构建。

运行要求：Windows 10/11 x64，显卡驱动支持 OpenGL 3.3。运行编译后的程序不需要 Blender、Node.js、Python、WebView2 或开发工具链。

开发环境需要 C++20 编译器、CMake 3.24+ 和 Ninja。`build.ps1` 使用 MinGW / g++，也支持项目内已有的便携工具；首次构建会下载固定版本的依赖。

```powershell
git clone https://github.com/FrancisRi/BA-cafe-pets.git
cd BA-cafe-pets
./build.ps1
./build/bin/BA-Cafe-Pets.exe --settings
```

已配置好编译器与 Ninja 的终端也可直接运行：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ba_cafe_lite_native -j 4
./build/bin/BA-Cafe-Pets.exe --settings
```

运行构建目录中的程序时，保留同目录的 `models/`、`defaults.ini` 和 `behavior/`。构建、测试及打包细节见 [开发说明](docs/DEVELOPMENT.md)。

## 日常操作

| 操作 | 效果 |
| --- | --- |
| 右键点击桌宠 | 打开快捷菜单 |
| 左键拖拽 | 移动桌宠 |
| 右键拖拽／中键拖拽 | 旋转角色 |
| 鼠标滚轮 | 缩放角色 |
| 快捷菜单勾选模型 | 显示／隐藏对应角色 |
| “显示完整界面” | 打开全局、咖啡厅、战斗和模型管理设置 |
| “导入模型” | 选择 GLB，可用 Ctrl／Shift 多选 |
| 模型管理中右键一行 | 选择该模型并打开管理菜单 |
| 模型列表中的 Delete | 确认删除所选本地模型或移除外部记录 |
| Esc | 关闭当前菜单或界面 |
| 单击任务栏托盘图标 | 打开主界面，保持模型当前显示状态 |
| 右键托盘图标 | 快速显示／隐藏模型或退出程序 |
| “退出桌宠” | 保存设置并退出程序 |

咖啡厅支持待机、点击反馈、散步和鼠标追踪；具备对应动画的模型可切换战斗模式。战斗模式支持键盘攻击、标点／回车请求换弹和点击指挥等行为。相关动画缺失时，操作会禁用或使用已有回退逻辑。

咖啡厅视线支持平滑追踪和 30° 转身模式：平滑追踪按经过时间计算，30° 模式在转身期间持续更新鼠标目标，头部中心附近有随模型大小变化的保护区，减少微小鼠标移动导致的转身。

## 画面质量

完整界面 → 全局设置 → **画质与抗锯齿**：省电保留 1× 渲染；均衡使用 1.5× 超采样并尝试 2× MSAA；清晰使用 2× 超采样并尝试 4× MSAA。选项下方显示当前角色实际生效的超采样与 MSAA，自动降低采样时也会说明；切换后首帧绘制完成即更新。两种描边模式均增强了边缘平滑。保持模型大小、线条粗细和透明窗口合成方式，原有画质选择继续生效。

MSAA 用在模型实际渲染的离屏缓冲，而非隐藏的主窗口；显卡不支持相应采样或超过每角色 64 MiB 的额外采样缓冲预算时自动回退，帧率取决于模型、角色数量和设备。

## 模型库与删除

通过程序导入后，GLB 原样复制并校验到 **EXE 所在目录下的 `model/` 文件夹**，启动时自动创建该目录。单文件版以用户双击的 EXE 所在目录为准，不使用内部解压缓存。加载使用本地副本，不修改模型内容。默认自带 Shiroko；用户额外导入的模型不会上传到本仓库。

完整界面 → 模型管理 → **更改保存位置…** 可选择文件夹，**恢复默认位置** 返回 EXE 旁的 `model/` 文件夹。设置重启后保留，更改后新导入的模型保存到新位置，旧模型保留原文件并继续显示和管理；不会自动搬动或删除旧文件。所选目录需要可写。

1.0.1 中直接保存在 EXE 旁边的 GLB 和更早的 `data/models/` 仍会识别和管理；已自选的保存目录保持设置。

打开模型管理或点击“刷新模型库”，会同步模型库内直接存放的 GLB，并清理已从磁盘移除且未显示的本地条目。新发现的模型不会自动显示。

- 本地模型：确认删除后，将副本移入 Windows 回收站，同时移除列表记录并关闭对应桌宠。
- 外部文件／自带模型：菜单显示“移除模型记录”，仅移除记录，保留文件。
- 取消删除或文件被占用：保留文件和记录，界面显示失败原因。
- 隐藏或删除最后一个显示中的角色：程序继续在托盘运行，可以重新显示模型或导入新模型。隐藏全部角色后停止动画渲染与桌面输入监听。

也可在菜单中定位模型文件或打开模型文件夹。旧版本的外部路径记录仍可用，重新导入时会迁移为本地副本并保留模式、大小及显示状态。

## 数据、升级与诊断

| 运行方式 | 持久数据目录 |
| --- | --- |
| 源码构建后的原生程序 | 可执行文件旁的 `data/` |
| 便携 ZIP | 解压目录的 `data/` |
| 单文件启动器 | `%LOCALAPPDATA%/ba_cafe_lite_native/data/` |

`settings.ini` 保存设置，`characters.json` 保存模型记录与显示状态，`model-library.json` 保存模型目录设置。模型文件位于程序旁的 `model/` 文件夹或自选目录；备份时需要同时保存数据目录及模型文件。

升级到 1.0.1 后，仍读取旧的 `data/models/`。单文件版还会恢复自身目录下 `data/characters.json` 和 `build/bin/data/characters.json` 中存在的模型，解决从开发版切换到根目录 EXE 后模型不见的问题。当前没有用户模型时，会恢复旧模型的显示状态、模式与大小；已有用户模型时只合并缺失记录。首次恢复前备份当前记录为 `characters.before-storage-*.json`，原文件和旧记录不修改，同一旧资料只自动恢复一次。显式传入 `--data-dir` 时不自动合并其他资料。

程序文件名统一为 `BA-Cafe-Pets.exe`；为兼容已有配置，仍沿用原持久数据目录。便携版升级时退出旧程序，将新包解压到原目录并运行新名称的程序即可；旧的 `ba_cafe_lite.exe` 不会自动删除，请勿再启动旧文件。单文件启动器会与原版共用上述 AppData 目录；需要独立数据时，可使用便携版，或为原生程序指定 `--data-dir`：

```powershell
./build/bin/BA-Cafe-Pets.exe --data-dir ./artifacts/my-data --settings
./build/bin/BA-Cafe-Pets.exe --diagnostics
```

默认不写运行日志。启用 `--diagnostics` 后，日志、性能信息和挂起诊断保存在数据目录的 `logs/`，不自动上传。升级前退出旧进程，并在关闭程序后备份数据。

## 验证与已知边界

当前修改已通过原生界面、模型库、多选导入和置顶维护回归检查，并使用独立数据目录验证真实文件选择、删除、取消、文件占用及模型库同步流程。验证命令见 [开发说明](docs/DEVELOPMENT.md#回归检查)。

- 支持内嵌 PNG/JPEG 贴图与缓冲区的 GLB 2.0，不含 Draco、Meshopt、KTX2 等解码器。
- 模型的动作完整性、命名及特殊部件会影响适配效果；导入成功不等于所有动作均已逐帧核对。
- 目标为 60 FPS，实际性能随模型数量、大小、渲染档位及设备而变化。
- 本分支从 `1.0.0` 开始独立编号，不沿用原项目版本号；尚未提供自动更新。

原版算法和历史说明保留于 [docs/UPSTREAM_README.zh-CN.md](docs/UPSTREAM_README.zh-CN.md)。

## 作者、许可与反馈

原始代码由 **maxiumlee（Max）** 开发，来源为 [maxiumlee/ba-cafe-lite](https://github.com/maxiumlee/ba-cafe-lite)；本分支由 **FrancisRi** 维护。

截至 2026-10-24，上游仍未提供项目整体的 `LICENSE`。为整份衍生代码添加 MIT／GPL 等许可证，需要先取得相关权利人的相应授权；维护者拥有版权的新增代码可以明确范围后单独授权。目前未为整个项目或新增部分指定开源许可证，注明原作者不能代替授权。第三方组件分别沿用其许可；角色、模型和贴图权利属于相应权利人。详见 [NOTICE.md](NOTICE.md)、[CREDITS.md](CREDITS.md) 和 [licenses/NOTICE.txt](licenses/NOTICE.txt)。

问题反馈请使用[本仓库 Issues](https://github.com/FrancisRi/BA-cafe-pets/issues)。参与开发请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。
