# 作者与来源

## 原作者

**maxiumlee（Max）** 是本项目原始代码库 **ba-cafe-lite** 的作者。

- 原作者主页：<https://github.com/maxiumlee>
- 原始仓库：<https://github.com/maxiumlee/ba-cafe-lite>
- 本分支基线：`e6f5cba`，原版 `0.2.15`。
- 本仓库保留原始 Git 提交历史、原作者署名和第三方许可声明。

原项目提供了原生桌宠的渲染、模型及动画加载、咖啡厅与战斗行为、多角色协调、跟随、避让、窗口边界处理和发行构建等基础实现。

## 本分支维护

**FrancisRi** 维护 **BA Cafe Pets**，仓库为 <https://github.com/FrancisRi/BA-cafe-pets>。

当前分支在原版基础上增加了 Fluent Design 界面、角色快捷菜单、持久模型库、多选导入、模型右键管理与删除，并调整了主界面窗口层级和桌宠暂停行为。详细变更见 [版本记录](release/CHANGELOG.md)。

这是基于原项目继续开发的衍生分支。原项目与本分支的来源、作者及版本记录分别保留。

Copyright (C) 2026 FrancisRi。FrancisRi 自有原创新增及修改采用 **GPL-3.0-only**，具体授权范围见 [LICENSE.md](LICENSE.md) 和 [docs/LICENSE-SCOPE.md](docs/LICENSE-SCOPE.md)。这不替原作者及第三方重新授权。

## 第三方组件与资源

| 组件／资源 | 来源 | 许可或说明 |
| --- | --- | --- |
| raylib | <https://github.com/raysan5/raylib> | zlib；构建时附带许可文本 |
| cgltf | <https://github.com/jkuhlmann/cgltf> | MIT；见 `licenses/cgltf.txt` |
| GLFW | <https://www.glfw.org/> | zlib；构建时附带许可文本 |
| BehaviorTree.CPP | <https://github.com/BehaviorTree/BehaviorTree.CPP> | MIT；构建时附带许可文本 |
| tinyxml2、minitrace、minicoro、lexy | BehaviorTree.CPP 的第三方依赖 | 构建及打包时附带各自许可文本 |
| GCC libstdc++ / libgcc | GCC | GPLv3 与 GCC Runtime Library Exception；见 `licenses/` |
| 默认 Shiroko 模型 | <https://github.com/lihaohong6/BlueArchiveModels> | 保留原模型文件；角色与模型权利属于相应权利人 |
| 模型查看器参考 | <https://bluearchive.wiki/wiki/Models> | 原项目的模型与动画适配参考 |

第三方声明见 [licenses/NOTICE.txt](licenses/NOTICE.txt)。项目整体的许可状态见 [NOTICE.md](NOTICE.md)。
