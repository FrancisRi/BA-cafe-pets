# 开发、验证与发行

## 工具与构建

目标平台为 Windows 10/11 x64。运行程序需要支持 OpenGL 3.3 的显卡驱动；开发需要 C++20 工具链、CMake 3.24+ 和 Ninja。

`build.ps1` 使用 MinGW / g++，优先查找 PATH 中的工具，也支持本地 `.tools/w64devkit`、`.tools/ninja` 和 `.tools/cmake-*`。工具链不随 Git 上传。MSVC 可直接通过 CMake 配置构建。

```powershell
./build.ps1
./build/bin/ba_cafe_lite_native.exe --settings
```

或使用已在终端配置好的 C/C++ 编译器：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ba_cafe_lite_native -j 4
./build/bin/ba_cafe_lite_native.exe --settings
```

CMake 会下载并校验固定版本的 raylib 6.0 与 BehaviorTree.CPP 4.8.0。可将相应源码放入 `vendor/raylib-6.0`、`vendor/BehaviorTree.CPP-4.8.0` 进行本地构建；首次下载需要网络。cgltf 位于 `shared/`。

本分支从 **1.0.0** 开始独立编号，不沿用原作者版本号。`0.2.15` 仅表示源码来源基线。原生程序内部文件名及持久数据目录继续保留，以维持数据兼容；对外发行文件使用 `BA-Cafe-Pets-版本号` 命名。

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

先完成应用构建，再构建测试目标：

```powershell
cmake --build build --target ba_cafe_lite_ui_tests ba_cafe_lite_model_library_tests ba_cafe_lite_model_import_tests ba_cafe_lite_model_storage_tests ba_cafe_lite_topmost_tests ba_cafe_lite_follow_tests ba_cafe_lite_antialias_tests -j 4
./build/ba_cafe_lite_ui_tests.exe
./build/ba_cafe_lite_model_import_tests.exe
./build/ba_cafe_lite_topmost_tests.exe
./build/ba_cafe_lite_follow_tests.exe (Join-Path $PWD ('artifacts/follow-' + [guid]::NewGuid().ToString('N') + '.ini'))
```

抗锯齿测试使用隐藏 OpenGL 窗口，不读取用户模型；比较真实 GPU 斜边覆盖与 32×32 子采样参考，验证透明预乘颜色、画质切换、缓冲绑定恢复和显存预算回退：

```powershell
./build/ba_cafe_lite_antialias_tests.exe
```

模型库测试会创建文件并测试回收站删除，参数必须是尚不存在的独立测试目录：

```powershell
$scratch = Join-Path $PWD ('artifacts/model-library-' + [guid]::NewGuid().ToString('N'))
./build/ba_cafe_lite_model_library_tests.exe $scratch
./build/ba_cafe_lite_model_storage_tests.exe (Join-Path $PWD ('artifacts/model-storage-' + [guid]::NewGuid().ToString('N')))
```

打包后可用 Windows 桌面验证单文件启动器的旧记录恢复。脚本将 EXE、模型与数据复制到独立测试目录，不改动实际用户资料：

```powershell
python tests/launcher_storage_compatibility.py ./dist/BA-Cafe-Pets-1.1.2-x64.exe
python tests/tray_integration.py ./dist/BA-Cafe-Pets-1.1.2-portable-x64.zip
python tests/tray_background_regressions.py ./dist/BA-Cafe-Pets-1.1.2-portable-x64.zip
python tests/tray_background_regressions.py ./dist/BA-Cafe-Pets-1.1.2-x64.exe
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
./build/bin/ba_cafe_lite_native.exe --data-dir ./artifacts/manual-data --settings --diagnostics
```

重点检查多选导入、重复模型复用、删除取消、文件被占用、删除正在显示的角色、模型库刷新、五角色显示上限，以及完整界面打开时动画持续。隐藏或删除全部模型后继续在托盘运行；验证托盘恢复、重启保持隐藏、空模型库导入与显式退出，确认记录和本地文件均已同步。

更多行为及性能测试目标位于 `tests/`；上游算法说明保留在 [原版 README](UPSTREAM_README.zh-CN.md) 与 `design/`。

## 打包与发布

打包使用 PowerShell 7 与 Windows `makecab.exe`：

```powershell
pwsh -File ./package.ps1
```

脚本生成 `BA-Cafe-Pets-版本号-x64.exe`、便携 ZIP、SHA-256 校验文件及发行清单，输出到 `dist/`，同时生成根目录的 `BA-Cafe-Pets.exe`。只打包默认模型、程序资源和许可文件，不打包本地 `data/` 或额外模型。

正式发布前需更新 CMake 项目版本号和变更记录，确认二进制版本、默认配置、许可文件及空用户数据的启动结果，再将产物发布至 [本分支 Releases](https://github.com/FrancisRi/BA-cafe-pets/releases)。Git 标签使用本分支版本号，例如 `v1.0.0`，与程序版本及发行文件名对应。

单文件启动器仍使用原数据目录 `%LOCALAPPDATA%/ba_cafe_lite_native/data/`，与原版共用该位置。需要完全独立配置时，可使用便携版或通过原生程序的 `--data-dir` 指定目录。升级前退出旧进程，并在关闭程序后备份整个 `data/`。

应用图标源文件为 `assets/app-icon.svg`。修改后运行 `python tools/generate_app_icon.py` 生成多尺寸 ICO；该开发脚本依赖 Pillow，发行程序不需要 Python。
