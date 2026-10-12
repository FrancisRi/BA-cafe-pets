# Hoshino GLB 核对

用户 ZIP 内的文件与已解压文件相同，也与 Wiki Models 固定引用 `lihaohong6/BlueArchiveModels@525ae0faeb0a89f54ef4023f3be3692dcb529e51` 的 Git blob 一致：

- Hoshino.glb：`0baeab5a58131e3654766d79184dde4948fca5e0`
- Hoshino (Swimsuit).glb：`7a24c6ebd6e499f4d493916e4cdf6a12ef5d26bd`

Wiki 页面默认 `data-hide-parts="optional"`。普通 Hoshino 盾牌标记 optional/prop0；咖啡厅不声明 show，战斗相应动作声明 show prop0。泳装模型动作明确 hide switch0 或 switch1。宿主此前未读取这些规则，因此会显示不应显示的部件。已实现通用规则，不按角色手动移动骨骼。

导入最后一步报错 `Invalid model envelope`。胜利动作将普通武器移到约 -98 单位的远处，使加余量后的包围盒达到 -110.65；该停放武器不应该撑大桌宠窗口。现在测量可见部件，并忽略远离角色主体的武器对窗口范围的贡献，源动画和渲染变换保持原样。

对 Cafe_Idle 及所选战斗 Idle 在 0.25/0.8/1.5 秒采样，原始 Three.js 播放与平滑播放器完成过渡后的头发四元数误差低于 1e-6（泳装约 1e-16）。未发现立方插值被改写。部件隐藏修复后的头发穿模和过渡视觉仍由用户核对。

参考缓存：artifacts/references/models.html、gadget-impl.js、wiki-animation.ts；在线哈希只读取 GitHub tree 元数据，没有下载 GLB。回归：tests/three_part_visibility_tests.mjs。
