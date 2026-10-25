BA Cafe Pets 1.1.5 · Windows x64

基于 maxiumlee（Max）的 ba-cafe-lite 继续开发。
原作者：https://github.com/maxiumlee/ba-cafe-lite
本分支：https://github.com/FrancisRi/BA-cafe-pets
本分支采用独立版本号，首个正式版为 1.0.0；原版 0.2.15 仅表示源码来源基线。
程序文件名统一为 BA-Cafe-Pets.exe；持久数据目录保留，以兼容已有配置。
便携版请退出旧程序后解压到原目录，运行 BA-Cafe-Pets.exe；旧 ba_cafe_lite.exe 不自动删除，请勿再启动。

启动
  单文件版双击 BA-Cafe-Pets-1.1.5-x64.exe；便携版解压后运行 BA-Cafe-Pets.exe。
  默认角色 Shiroko。
  不需要 Blender、浏览器、WebView2、Python 或开发环境。
  便携 ZIP 请完整解压，保留 model、defaults.ini、behavior 和 licenses。

操作
  右下角托盘图标：单击打开主界面，右键打开模型快捷菜单。
  全部模型隐藏后继续后台运行，重启保留隐藏状态；“退出桌宠”才退出程序。
  右键点击桌宠：打开快捷菜单，勾选显示或隐藏模型。
  “显示完整界面”：打开 Fluent 风格的全局、咖啡厅、战斗和模型管理。
  桌宠保持置顶；完整主界面可被其他窗口覆盖，打开时桌宠继续活动。
  左键拖动：移动。右键／中键拖动：旋转。鼠标滚轮：缩放。
  “导入模型”：选择 GLB，可按 Ctrl／Shift 一次多选多个。
  模型管理中右键点击一行：显示／隐藏、设为当前、定位或删除。
  “退出桌宠”：保存设置并关闭全部角色。
  朝向鼠标：30° 模式转身时更新鼠标目标；头部中心附近忽略微小移动。

画面质量
  全局设置 → 画质与抗锯齿：均衡／清晰档增强抗锯齿，省电档保留轻量渲染。
  清晰档结合 2× 超采样与最高 4× MSAA，均衡档尝试 2× MSAA。
  两种描边模式均平滑边缘，显卡限制或采样缓冲预算不足时自动降低采样。
  选项下方显示当前角色实际生效的超采样、MSAA 和自动降级状态，切换后随实际绘制更新。
  线条粗细右侧数字可点击输入 10–300%；Enter 或失焦保存，Esc 取消，滑块与数字同步。

开机自启
  全局设置 → 应用 → 开机自启，可手动开启或关闭，初次运行默认不添加启动项。
  登录 Windows 后恢复上次角色状态；单文件版记录外层 EXE，显式 --data-dir 参数一并保留。

模型管理
  所有模型（含白子）统一复制、校验到 EXE 旁的 model 文件夹或自选目录，运行记录不指向解压缓存。
  模型管理 → “更改保存位置…”可选择目录，“恢复默认位置”返回 EXE 旁的 model 文件夹。
  更改目录时现有模型一并复制并校验，保留角色设置；失败保留原配置，原始文件和旧副本保留作备份。
  模型库不受旧的 15 条历史限制，最多同时显示 5 个角色。
  “刷新模型库”同步实际文件，新发现的模型不会自动显示。
  删除本地模型会将副本移入回收站，并同步移除记录、关闭相应桌宠。
  外部文件和自带模型仅移除记录，原始文件保留。
  取消或删除失败时保留文件与记录；删除最后一个角色后仍保留托盘，可重新导入模型。

数据与日志
  单 EXE 版：%LOCALAPPDATA%\ba_cafe_lite_native\data\
  便携 ZIP／原生程序：可执行文件旁的 data\
  settings.ini 保存设置；characters.json 保存模型记录；model-library.json 保存模型目录设置。
  模型文件单独保存在 EXE 旁的 model 文件夹或自选目录，旧 data/models 和 EXE 旁已有的 GLB 继续兼容。
  根目录 EXE 会恢复同目录 data 和 build/bin/data 中存在的旧模型记录。
  恢复前备份为 characters.before-storage-*.json，不修改旧文件和记录。
  当前单 EXE 数据目录与原版共用。需要独立数据，可用便携版或原生程序的 --data-dir。
  升级前退出旧版本，关闭程序后同时备份 data 文件夹和模型文件。
  默认不写日志；--diagnostics 开启日志、性能统计与挂起诊断，保存在 logs\，不自动上传。

要求与边界
  Windows 10/11 x64，显卡驱动支持 OpenGL 3.3。
  支持内嵌 PNG/JPEG 的 GLB 2.0；不含 Draco、Meshopt、KTX2 等解码器。
  动作完整性和特殊模型可能需要进一步适配。实际帧率取决于模型及设备。

作者与许可
  原作者：maxiumlee（Max）；本分支维护：FrancisRi。
  原始提交历史和第三方声明保留；上游基线没有提供项目整体 LICENSE。
  Copyright (C) 2026 FrancisRi；自有原创新增及修改采用 GPL-3.0-only，仅限第 3 版。
  授权见 LICENSE.md，范围见 docs\LICENSE-SCOPE.md，完整条款见 licenses\GPL-3.0-only.txt。
  本授权不替上游及第三方授权，也不构成 GPL 例外；分发合并作品仍需满足适用要求并取得其他必要授权。
  源码与构建脚本：https://github.com/FrancisRi/BA-cafe-pets （使用对应发行的完整提交或源码归档）。
  完整来源说明见 CREDITS.md、NOTICE.md 和 licenses\NOTICE.txt。
  角色、模型及贴图权利属于各自权利人。
