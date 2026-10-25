# FrancisRi 自有贡献的许可范围

版权人：**FrancisRi**。年份：**2026**。

许可证：**GPL-3.0-only**，完整条款见
[GPL-3.0-only.txt](../licenses/GPL-3.0-only.txt)，授权声明见
[LICENSE.md](../LICENSE.md)。本清单用于识别授权对象，不修改 GPL 条款。

## 上游基线与识别方法

上游：[maxiumlee/ba-cafe-lite](https://github.com/maxiumlee/ba-cafe-lite)。

基线提交：`e6f5cba6c7cc35f6ca6bebaa988c1a0f23e6acec`（原版 0.2.15）。

授权对象是在该基线之上，**由 FrancisRi 拥有版权的原创新增及修改**。
现有上游文件中的授权只及于这些贡献，不及于原有表达。移入新文件、
移动行、格式调整、重命名或复制材料，不会因此把原有材料变为 FrancisRi
所有；第三方贡献也不会仅因被纳入本分支而自动获得本授权。

可用以下命令定位变更，再结合来源与版权声明判断具体内容：

```powershell
git diff --name-status e6f5cba6c7cc35f6ca6bebaa988c1a0f23e6acec HEAD
git diff --find-renames e6f5cba6c7cc35f6ca6bebaa988c1a0f23e6acec HEAD -- path/to/file
git log --follow -- path/to/file
```

Git 的新增行和提交作者记录用于追溯，不单独证明某行的版权归属。
未来贡献只有在权利人明确按相同条件授权时才纳入，不能由此替其他人授权。

## 当前代码清单

下列新增文件中，FrancisRi 拥有版权的原创内容适用 GPL-3.0-only；文件
引用或调用的上游／第三方实现，以及任何沿用材料，均不由本声明重新授权。

- `native_antialias.h`
- `morph_outline.h`
- `native_fluent.h`
- `native_model_import.h`
- `native_model_library.h`
- `native_model_storage.h`
- `native_tray.h`
- `tests/antialias_tests.cpp`
- `tests/launcher_storage_compatibility.py`
- `tests/model_import_tests.cpp`
- `tests/model_library_tests.cpp`
- `tests/model_storage_tests.cpp`
- `tests/morph_outline_tests.cpp`
- `tests/native_model_unification_tests.cpp`
- `tests/native_drag_follow_tests.cpp`
- `tests/native_ui_tests.cpp`
- `tests/tray_background_regressions.py`
- `tests/tray_integration.py`
- `tools/generate_app_icon.py`
- `native.manifest`

下列上游文件中的 FrancisRi 自有新增和修改同样按 GPL-3.0-only 授权；
**不表示整个文件已经改用 GPL**：

- `main.cpp`、`native_input.h`、`native_renderer.cpp`、`native_renderer.h`
- `native_settings.h`、`scene.h`
- `shared/cafe_attention.h`、`shared/crowd_steering.h`、`shared/desktop_path.h`
- `shared/pet_ground.h`、`shared/scene_state.h`
- `.gitignore`、`CMakeLists.txt`、`build.ps1`、`package.ps1`
- `release_launcher.cpp`、`version.rc.in`
- `tests/crowd_steering_tests.cpp`、`tests/follow_settings_tests.cpp`

与自有代码一起编写的原创构建说明、操作说明和许可范围说明，也在自有
版权范围内按相同条款提供。`assets/app-icon.svg` 的本分支原创图形及由其
生成的 `assets/app.ico` 也采用相同条款；SVG 和图标生成脚本提供可编辑来源。

## 明确排除

- 上游基线中的代码、文档、行为树、设计图和其他材料，及其被复制或移动的内容。
- `docs/UPSTREAM_README.zh-CN.md`：保留的上游 README，不因是新增路径而纳入本授权。
- `shared/cgltf.h` 等第三方代码、依赖库及各自的许可证原文。
- `assets/Shiroko.glb`、其他角色模型、贴图、动画，以及用户本地导入的文件。
- 属于其他权利人的名称、商标和其他材料。

第三方声明见 [CREDITS.md](../CREDITS.md) 与
[licenses/NOTICE.txt](../licenses/NOTICE.txt)。上游缺少项目整体许可证的
状态见 [NOTICE.md](../NOTICE.md)。这份范围说明不填补该授权缺口，也不
豁免 GPL 对合并或衍生作品的适用要求。

## 获取源码与分发

源码、修改记录和构建脚本位于
[BA-cafe-pets 仓库](https://github.com/FrancisRi/BA-cafe-pets)。
构建方法与固定依赖版本见 [DEVELOPMENT.md](DEVELOPMENT.md)。
获取发行对应版本时应使用该发行指向的完整提交或源码归档；重新分发者
必须针对实际分发的版本履行 GPL 中适用的完整对应源码义务，保留版权与
许可声明，并说明修改及日期，不能仅用一个指向最新分支的链接代替。

本授权不为整个应用或其依赖声称已经满足所有再分发条件；分发合并作品
时，仍需取得其他必要权利，并满足 GPL 对整体许可的适用要求。
