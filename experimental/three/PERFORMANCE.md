# raylib 与 Three.js / WebView2 原型的资源评估

2026-10-11，本机 16 个逻辑处理器。以下比较的是本项目的两个实现，不是两个库的普遍性能排名。

## 长期运行

两个版本都使用 Hibiki、目标 60 FPS，资源采样包含 Three.js 原生宿主及全部 WebView2 后代进程。两次短时采样中有普通动作、追踪或散步，状态不完全相同；两者同时运行，未作电池续航测试。第二次采样两者的描边设置均关闭。

| 项目 | raylib 版 | Three.js 版 |
| --- | --- | --- |
| 私有提交内存 | 129.6–129.7 MiB | 274.6–286.8 MiB |
| CPU，占本机总算力 | 2.15–3.41% | 4.87–5.29% |
| 进程数 | 1 | 7，包含 WebView2 |
| 可执行文件 | 7,673,955 字节 | 约 7.4 MB，另有 2.2 MiB 的 Three.js 文件及 WebView2Loader.dll |

数据位于 `artifacts/three-smoke/resources-before.json` 和 `resources-after.json`。私有提交内存不是物理常驻内存；各进程工作集相加可能重复计入共享页，所以没有用它作为主要内存比较。CPU 是区间平均值，GPU 计数器只是另一次瞬时采样，不能用来推断耗电或 GPU 性能排名。

当前 raylib 版在内存和 CPU 上更轻。Three.js 文件本身很小，嵌入 WebView2 后仍要运行浏览器、渲染及 GPU 进程。[微软 WebView2 性能文档](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/performance)说明了这类运行时开销。

## 导入阶段

raylib 流程先筛选动作，再采样、通过 Blender 合并兼容蒙皮、校验，最后加载。Iori 本次选择 4 个动作的日志中，单独 glTF 导出就用了 **46.663 秒**，不含其他步骤；再次使用已通过校验的缓存可以跳过转换。

Three.js 原型直接读取原始 GLB，不运行 Node、Python 或 Blender。本机渲染测试的读取、解析及材质/着色器准备耗时：Hibiki 228ms、Azusa 254ms、Iori 259ms，保留各模型的全部动作。新原型一次启动到 Hibiki READY 的墙钟时间约 **3.3 秒**，其中模型阶段 227ms；前面的 WebView2 初始化也必须计入首次使用体验。没有在无 Runtime 的新电脑上测量。

上述 0.2–0.3 秒与 46.7 秒不是同一范围的端到端测量；它们分别说明原始 GLB 加载与 Blender 导出的成本。导入峰值内存和峰值 CPU 尚未测量。

若按当前 `tools/package-portable.ps1` 将转换工具全部随旧版附带，Node 目录约 88.5 MiB，完整 Blender 目录约 900.9 MiB。新版运行和导入不需要这些工具，但要求 WebView2 Runtime；[Evergreen 分发使用系统共享 Runtime](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution)，不能把它当成不存在的依赖。当前新版支持普通自包含、有动画的 GLB，尚未加入 Draco/KTX2 解码器。

低资源长期挂桌面优先选现有 raylib 版；原始模型快速导入、Wiki 视觉与动画兼容性优先继续验证 Three.js 版。当前还不能用短时采样保证长期没有内存增长。

## 本次交互修正与验证

- 拖拽窗口从鼠标事件立即移动，位置/角度直接转发，不再只等待行为定时器。
- Win32 定时器改为 10ms；持续状态更新约 60–64 Hz，浏览器按 60 FPS 调度。
- 屏幕反投影与 Three.js 相机使用一致的裁剪面，减少拖拽位置误差。
- 透明 alpha 使用 Three.js 内置异步读回；鼠标捕获期间暂停读回并移除旧裁剪区域，松手后恢复。
- 隔离实例检查：立即拖动 24×12px，返回位置误差 0px，取消捕获正常；旋转改变实际渲染区域；透明角落穿透、角色命中、菜单和线条开关通过。日志为 `artifacts/three-smoke/native-isolated.log`。
- 本机持续渲染约 60 FPS，100Hz 屏幕上的帧间隔 p95 约 20ms。该值不是输入到显示的延迟，也不替代用户对跟手程度和视觉效果的核对。

raylib 版的外轮廓目前没有改成角色线条；新线条算法仅在独立 Three.js 原型中实现。
