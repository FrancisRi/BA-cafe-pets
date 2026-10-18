ba_cafe_lite · Windows x64

双击下载的 ba_cafe_lite EXE 即可启动，默认角色 Shiroko。
不需要 Blender、浏览器、WebView2、Python 或开发环境。
首次启动会释放模型及程序资源，后续启动复用缓存。
首次使用在角色下方提示“右键打开菜单”，打开菜单后不再显示。

操作
  右键点击角色：打开设置。
  左键拖动：移动角色。右键拖动：旋转角色。
  鼠标滚轮：缩放。设置中“导入原始 GLB”：添加角色。
  设置右上角红色“退出”：关闭程序。

咖啡厅：待机、点击反馈、随机散步和鼠标追踪。
战斗：有对应动作的角色可启用；键盘触发攻击，标点/回车请求换弹。
三个连续点击切换至战斗，战斗时三个连续点击触发退场。
全局设置可选择渲染精度：省电／均衡／清晰，默认清晰。
放大或多人时可选均衡降低显卡负担，边缘可能更粗糙。
所有角色共享设置，最多同时显示 5 个，历史列表最多 15 条。
导入后保留原 GLB 文件，不要删除或移动它。

数据与日志
  单 EXE 版：%LOCALAPPDATA%\ba_cafe_lite_native\data\
  便携 ZIP 版：解压目录的 data\
  settings.ini 为设置；characters.json 为角色历史。
  默认不写日志。排错时通过 ba_cafe_lite.exe --diagnostics 开启日志、性能统计与挂起诊断，保存在 logs\；旧日志保留。
  关闭程序后可以备份或迁移整个 data 文件夹。
  单 EXE 版缓存：%LOCALAPPDATA%\ba_cafe_lite_native\app\
  新版复用同一 data，旧缓存可在关闭程序后手动删除。

要求与限制
  Windows 10/11 x64，支持 OpenGL 3.3 的显卡驱动。
  支持内嵌 PNG/JPEG 贴图的 GLB 2.0；不支持 Draco/Meshopt/KTX2 压缩。
  部分特殊模型或不完整的动作组可能需要后续适配。
  目标 60 FPS，实际性能随模型数量和显示大小变化。

源码：https://github.com/maxiumlee/ba-cafe-lite
第三方许可见 licenses\；可在便携 ZIP 或单 EXE 版缓存中查看。
