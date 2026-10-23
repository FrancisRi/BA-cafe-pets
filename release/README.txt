BA Cafe Pets · Windows x64

基于 maxiumlee（Max）的 ba-cafe-lite 继续开发。
原作者：https://github.com/maxiumlee/ba-cafe-lite
本分支：https://github.com/FrancisRi/BA-cafe-pets
当前保留原版的可执行文件名、0.2.15 版本基线与持久数据目录。

启动
  双击发行包中的 ba_cafe_lite.exe，默认角色 Shiroko。
  不需要 Blender、浏览器、WebView2、Python 或开发环境。
  便携 ZIP 请完整解压，保留 models、defaults.ini、behavior 和 licenses。

操作
  右键点击桌宠：打开快捷菜单，勾选显示或隐藏模型。
  “显示完整界面”：打开 Fluent 风格的全局、咖啡厅、战斗和模型管理。
  桌宠保持置顶；完整主界面可被其他窗口覆盖，打开时桌宠继续活动。
  左键拖动：移动。右键／中键拖动：旋转。鼠标滚轮：缩放。
  “导入模型”：选择 GLB，可按 Ctrl／Shift 一次多选多个。
  模型管理中右键点击一行：显示／隐藏、设为当前、定位或删除。
  “退出桌宠”：保存设置并关闭全部角色。

模型管理
  导入会将模型复制、校验并保存到本地 models 文件夹；原始下载文件可移动或删除。
  模型库不受旧的 15 条历史限制，最多同时显示 5 个角色。
  “刷新模型库”同步实际文件，新发现的模型不会自动显示。
  删除本地模型会将副本移入回收站，并同步移除记录、关闭相应桌宠。
  外部文件和自带模型仅移除记录，原始文件保留。
  取消或删除失败时保留文件与记录；删除最后一个显示中的角色时会确认退出。

数据与日志
  单 EXE 版：%LOCALAPPDATA%\ba_cafe_lite_native\data\
  便携 ZIP／原生程序：可执行文件旁的 data\
  settings.ini 保存设置；characters.json 保存模型记录；models\ 保存模型副本。
  当前单 EXE 数据目录与原版共用。需要独立数据，可用便携版或原生程序的 --data-dir。
  升级前退出旧版本，关闭程序后备份整个 data 文件夹。
  默认不写日志；--diagnostics 开启日志、性能统计与挂起诊断，保存在 logs\，不自动上传。

要求与边界
  Windows 10/11 x64，显卡驱动支持 OpenGL 3.3。
  支持内嵌 PNG/JPEG 的 GLB 2.0；不含 Draco、Meshopt、KTX2 等解码器。
  动作完整性和特殊模型可能需要进一步适配。实际帧率取决于模型及设备。

作者与许可
  原作者：maxiumlee（Max）；本分支维护：FrancisRi。
  原始提交历史和第三方声明保留；上游基线没有提供项目整体 LICENSE。
  完整说明见 CREDITS.md、NOTICE.md 和 licenses\NOTICE.txt。
  角色、模型及贴图权利属于各自权利人。
