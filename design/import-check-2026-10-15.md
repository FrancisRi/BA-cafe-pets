# GLB 批量导入检查 · 2026-10-15

检查对象：用户提供的 BlueArchiveModels-main 目录，共 287 个 GLB。

- 修复前：284 个通过，3 个导入失败。
- 修复后：287 个全部通过；7,135 段动作，21,405 个采样姿势。
- 使用原生 GltfModel 加载器，实际建立 raylib/OpenGL 上下文，解码贴图并上传网格。
- 每段动作检查开始、中点和接近末尾处的几何更新及有限坐标。没有核对视觉效果，也没有验证所有帧和桌面交互。
- 原始 GLB 文件未改写。

## 修复的导入失败

| 模型 | 原错误 | 修复后 |
| --- | --- | --- |
| Akari (New Year).glb | Animation times must increase | 通过 |
| Ayane (Swimsuit).glb | Animation times must increase | 通过 |
| Mutsuki (New Year).glb | Animation times must increase | 通过 |

原因：导出文件的个别动画通道包含重复时间戳。加载时保留同一时间最后一组完整关键帧，包括 CUBICSPLINE 入/出切线，避免零时长插值区间。不删除整段动作，不修改帧时间或动作总时长。倒序时间、非有限值及错误维度继续拒绝。

回归单测覆盖线性/阶梯数据布局、连续重复帧、三次插值切线、全部同一时间、相邻但不同的浮点时间以及非法输入。

## 动作映射提示（不是导入失败）

以下角色仍可加载。缺少咖啡厅待机时使用已有动作；有歧义的动作沿用现有跳过策略，未在本次改动中猜测映射。

| 模型 | 提示 |
| --- | --- |
| Arisu (Battle) (Scenario).glb | Cafe Idle fallback: Normal_Idle |
| Eimi (Battle) (Scenario).glb | Cafe Idle fallback: Exs01 |
| Haruka.glb | Ambiguous Exs: skipped |
| Iroha.glb | Ambiguous Vital_Death: skipped |
| Kei (Scenario 1).glb | Cafe Idle fallback: Normal_Idle |
| Kei (Scenario 2).glb | Cafe Idle fallback: Normal_Idle |
| Material Development Club Member.glb | Cafe Idle fallback: Mesh |
| Mika (Cut-in).glb | Cafe Idle fallback: CH0069_Exs_Cutin |
| Nonomi.glb | Cafe Idle fallback: Normal_Idle |
| Rio (Battle) (Scenario).glb | Cafe Idle fallback: Exs01 |
| Toki (Battle) (Scenario).glb | Cafe Idle fallback: Normal_Idle |
| Yukari.glb | Ambiguous Normal_Idle: skipped |

逐文件详细记录：`artifacts/import-before.json`、`artifacts/import-after.json`（本机产物，不包含模型文件）。

重新检查的方法见 README 的批量导入检查命令。
