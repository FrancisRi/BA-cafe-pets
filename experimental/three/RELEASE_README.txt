ba_cafe_lite · Three.js 离线版 · Windows 10/11 x64

双击发布 EXE 即可运行。首次启动会自动释放离线资源；无需管理员权限、
Blender、Node、Python、浏览器安装或网络连接。默认角色为 Shiroko。
运行文件缓存在 %LOCALAPPDATA%\BA-DesktopPet\app；设置、角色历史、
浏览器缓存和日志保存在 %LOCALAPPDATA%\BA-DesktopPet\data。
以后更新 EXE 会保留已修改的设置和历史，不会重新覆盖默认参数。

左键拖拽 · 右键旋转 · 滚轮缩放 · 中键打开设置。
首次显示中键提示，打开菜单后永久隐藏。菜单右上角“退出”关闭全部角色。
设置中的“导入 GLB”可直接选择本地模型，无需转换环境。
支持贴图、缓冲区内嵌的 GLB 2.0；外部资源或需要额外解码器的模型不支持。
导入后请保留原 GLB 文件，角色历史记录它的位置。
历史最多 15 条，同时显示最多 5 个；所有角色共用设置。
首次默认参数采用制作者当前设置，包括战斗模式、线条模式与 50% 粗细。
关闭最后一个角色，确认后退出。重新双击已运行的发布 EXE 不会重复启动。

发生问题时查看 data\logs\pet.log 与 pet.err。
授权说明见 licenses 文件夹及 app\three\LICENSE。
随包保留微软运行时的原始文件；其第三方许可可通过
runtime\webview2\show_third_party_software_licenses.bat 查看。
运行时为随包固定版本，更新运行时需要重新发布 EXE。
本包尚未进行代码签名。
