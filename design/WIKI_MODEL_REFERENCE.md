# Wiki 模型处理参考

核对版本：MirahezeDevScripts 提交 `5e4d3e449f8915af840e79aebb6a11819876e59c`，Three.js 0.185.1。源码参考副本位于 `artifacts/references/wiki-*.ts`，只用于研究，不作为桌宠运行依赖。

## 加载与动画

[formats.ts](https://github.com/lihaohong6/MirahezeDevScripts/blob/5e4d3e449f8915af840e79aebb6a11819876e59c/gadgets/ModelViewer/formats.ts) 直接调用 GLTFLoader.parse，使用返回的 scene 与 animations。没有经 Blender 合并蒙皮或逐帧重导出。

[animation.ts](https://github.com/lihaohong6/MirahezeDevScripts/blob/5e4d3e449f8915af840e79aebb6a11819876e59c/gadgets/ModelViewer/animation.ts) 使用以完整场景为根的 AnimationMixer。开始另一个动作前停止旧动作并恢复保存的默认位置、旋转、缩放；将多 primitive 网格组的 morph 通道分发到真正具有 morphTargetInfluences 的子网格。

[viewer.ts](https://github.com/lihaohong6/MirahezeDevScripts/blob/5e4d3e449f8915af840e79aebb6a11819876e59c/gadgets/ModelViewer/viewer.ts) 创建开启 antialias 的 WebGLRenderer，像素比例最高为 2；替换为不受光照影响的材质时保留贴图、透明度、双面与 alphaTest。

## 原 raylib 版本对应处理

| Wiki / glTF 行为 | 当前桌宠 |
| --- | --- |
| 播放完整节点层级，包括骨架外父节点 | 已修正 Iori 的带动画父节点，每帧使用其祖先链动画；有动画的非关节中间挂件仍需另行支持 |
| 多个 mesh / skin 原样加载 | raylib 单皮肤限制仍通过 Blender 官方工具统一 |
| 原始关键帧按时间插值 | 使用同版 Three.js 插值器采样后交给 raylib，仍有转换成本 |
| 切换动作前恢复默认属性 | 采样显式保留未动画化关节默认值；每帧先重置 morph 默认权重 |
| 按 primitive 播放 morph | model_faces.h 为各 primitive 读取嘴型与权重通道 |
| 保留 alpha 裁切与贴图采样 | 小型 alpha-cutout shader 和 glTF 贴图采样设置 |

## 后续适配原则

以原始 GLB 在 Wiki 同版 Three.js 的结果为数值参照，保留节点层级、蒙皮、绑定矩阵与默认值，不逐角色手调。raylib 版本继续只转换必要动作。

要接近网页的直接加载速度，需要运行时支持原始 glTF 的多皮肤和完整动画层级，从而减少或取消 Blender 步骤；仅移植网页材质设置不能消除转换耗时。

## 独立 Three.js 原型

现在已新增 `experimental/three`，构建为 `build/three/desktop_pet_three.exe`。直接加载未转换的 GLB，采用上述 Wiki 加载、动画和材质方法，旧版仍单独保留。运行和限制见 `experimental/three/README.md`。

[outline.ts](https://github.com/lihaohong6/MirahezeDevScripts/blob/5e4d3e449f8915af840e79aebb6a11819876e59c/gadgets/ModelViewer/outline.ts) 使用各 mesh 的反向外壳、面积加权平滑法线和骨骼/morph 着色器；透过部件遮挡形成局部线条，贴图内部线条原样保留。原型采用同一算法，按桌面相机 FOV 和完整投影高度补偿线宽，以保持 Wiki 参考视窗的像素粗细。
