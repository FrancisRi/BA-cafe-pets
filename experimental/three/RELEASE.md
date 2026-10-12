# ba_cafe_lite 单 EXE 发布

发布文件：`dist/ba_cafe_lite-0.1.2-x64.exe`。Windows 10/11 x64，未签名。
默认 Shiroko 原始 GLB，默认参数来自 `artifacts/release-defaults.ini`。
不包含开发机的角色历史、浏览器用户数据、模型转换工具或其他个人文件。

启动器使用 Windows Cabinet API 解码随 EXE 附带的 CAB。
微软原始 WebView2 CAB 不重复压缩；释放后删除临时 CAB。
首次启动显示校验和释放进度；后续按包内容哈希复用缓存。
两个原生程序静态链接编译器运行库，WebView2Loader 仅依赖 Kernel32。
不需要安装 Node、Python、Blender、解压软件或浏览器，也不需要联网。

运行资源：`%LOCALAPPDATA%\BA-DesktopPet\app\<内容哈希>`。
共享用户数据：`%LOCALAPPDATA%\BA-DesktopPet\data`。
重命名后的版本沿用该数据位置及单实例锁，以保留先前设置和角色历史。
设置首次从包内 defaults.ini 复制；之后不覆盖用户改动。
角色历史、生成的行为 XML、日志、WebView2 profile 都保存在 data 中。
导入 GLB 保持原文件不变，历史记录源文件路径。

| EXE 构成 | MiB |
| --- | ---: |
| 微软 WebView2 155.0.4283.45 x64 原始 CAB | 296.16 |
| 原生离线启动器 | 3.08 |
| 共同压缩的桌宠资源及外层 CAB 结构 | 4.01 |
| 合计 | 303.26 |

主程序原始大小 7.46 MiB；Shiroko 4.66 MiB；Three.js 2.15 MiB。
运行时解包后 672.84 MiB；完整缓存约 687.47 MiB。
首次释放的临时峰值约 984 MiB，另外需要用户数据空间。
压缩时多个应用文件共享 LZX 字典，单个文件的压缩体积不能准确拆分。

## 构建与测试

```powershell
Copy-Item build/three/settings.ini artifacts/release-defaults.ini
cmake --build build --target desktop_pet_three three_release_launcher three_scene_native
./tools/package-three-release.ps1 -Version 0.1.2
./tools/test-three-release.ps1 -Version 0.1.2
```

已有发布文件不会被脚本覆盖；正式更新应使用新的版本号。
打包版本须与 CMake 的 project VERSION 及编译后 Windows 文件属性一致。
`-Resume` 仅重用本次版本的暂存目录，仍不覆盖已经生成的 EXE。
原始微软 CAB 使用固定 SHA256 校验，解压来源的 EXE 必须具有有效微软签名。
运行时必须与 `RuntimeCab` 匹配；换版本时更新参数及固定校验值。

0.1.2 已构建并打包；按用户要求未运行完整发布验证。

以下为 0.1.1 的发布验证记录：

- 从全新中文路径释放，全部 296 个随包应用／运行库文件的 SHA256 匹配源文件。
- 首次仅显示 Shiroko，战斗模式与当前默认参数一致。
- 二次启动复用缓存，手动修改的设置保留。
- 原始 Iori GLB 直接加载，源文件未修改。
- 使用随包固定运行时验证真实跨进程背景点击、角色右键捕获及可见画面。
- 战斗、桌面几何回归通过；raylib EXE 哈希未改变。

开发测试可通过进程环境变量 `BA_PET_HOME` 将释放目录限制在项目内。
`--prepare-only` 只释放资源；`--exit-after N` 启动正常模式后自动退出，供测试使用。
