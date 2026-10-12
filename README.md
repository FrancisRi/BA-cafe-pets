# ba_cafe_lite

Windows 3D 桌宠。当前发行版使用 **C++20 + raylib + 独立 cgltf 动画适配层 + Win32**，直接导入原始 GLB，不需要 Blender、浏览器或 WebView2。

## 使用

下载发行版的 `ba_cafe_lite-0.2.0-x64.exe`，双击启动，首次使用默认 Shiroko。单个 EXE 包含模型、默认设置和依赖许可；首次启动释放资源，之后复用缓存。

- **右键点击**角色打开菜单；**右键拖动**旋转。
- **左键拖动**移动角色，**滚轮**缩放。
- 菜单中导入本地 GLB、管理角色、切换模式和调整共享参数。
- 右上角红色“退出”关闭程序。

咖啡厅模式支持待机、点击反馈、随机散步、鼠标朝向与位置跟随。战斗模式仅在存在对应动作时可用，支持攻击、换弹、技能、指挥、退场与胜利动作。历史最多 15 条，同时显示最多 5 个角色。

单 EXE 版设置与日志在 `%LOCALAPPDATA%\ba_cafe_lite_native\data\`；便携 ZIP 版在解压目录的 `data/`。导入历史记录原 GLB 路径，请保留模型原文件。迁移设置可在关闭程序后复制整个 `data/` 文件夹。

要求 Windows 10/11 x64 和 OpenGL 3.3。支持内嵌 PNG/JPEG 贴图的 GLB 2.0；暂不支持 Draco、Meshopt、KTX2 压缩资源。目标 60 FPS，实际帧率取决于模型数量、复杂度和显示大小；特殊模型仍可能需要适配。

## 构建

需要 CMake 3.24+、C++20 编译器和 Windows SDK 或 MinGW。CMake 优先使用本地 `vendor/`，否则获取固定版本并校验 SHA-256；运行发行版不需要这些开发工具。

```powershell
cmake -S native-raylib -B native-raylib/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build native-raylib/build --target ba_cafe_lite_native -j 4
.\native-raylib\build\bin\ba_cafe_lite_native.exe
```

本机自动寻找 MinGW/Ninja 的构建入口：

```powershell
.\native-raylib\build.ps1
.\native-raylib\package.ps1
```

打包脚本生成 `dist/ba_cafe_lite-0.2.0-x64.exe`、便携 ZIP 与 SHA-256 文件。单 EXE 使用 Windows Cabinet API 解压，不依赖安装器或脚本运行时。发行包只包含明确列出的文件，不包含开发机设置、角色历史和日志。

## 源码布局

| 路径 | 内容 |
| --- | --- |
| `native-raylib/` | 当前原生程序、cgltf 适配层、Win32 UI 与发行脚本 |
| `src/` | 共用咖啡厅行为树、战斗状态机与地面投影；旧 raylib 入口 |
| `experimental/three/*.h` | 原生版本复用的移动、边界、避让和角色列表逻辑 |
| `design/`、`experimental/three/design/` | Groot2 XML、Mermaid、draw.io 状态机 |
| `assets/Shiroko.glb` | 默认模型原始文件 |
| `tests/`、`tools/` | 历史验证、资源制作与旧版本工具 |

原生实现与模型支持细节见 [native-raylib/README.md](native-raylib/README.md)，更新内容见 [CHANGELOG](native-raylib/release/CHANGELOG.md)。旧 raylib 文档保留在 [README.legacy.md](README.legacy.md)，Three.js 版保留在 [experimental/three/README.md](experimental/three/README.md)。

第三方源码采用各自许可，发行包附带许可文本。默认角色来自用户提供的 BlueArchiveModels 文件；角色和模型权利归原权利方。
