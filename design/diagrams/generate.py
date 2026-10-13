"""Generate documentation graph sources. Does not change runtime behavior."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parent
GRAPHS = []
def graph(key, title, description, nodes, edges, sources, direction='TB'):
    GRAPHS.append(dict(key=key, title=title, description=description,
                       nodes=[dict(id=n[0],label=n[1],kind=n[2] if len(n)>2 else 'state') for n in nodes],
                       edges=[dict(source=e[0],target=e[1],label=e[2] if len(e)>2 else '',kind=e[3] if len(e)>3 else 'normal') for e in edges],sources=sources,direction=direction))

graph('01-architecture','代码框架与线程边界','每个角色独立保存动作与位置；所有角色共享输入、设置和历史。只有 CPU 动画/蒙皮并行，OpenGL 与 Win32 提交始终在主线程。',[
 ('launch','单 EXE 启动器\nrelease_launcher.cpp\n解包缓存 → 启动原生核心'),
 ('entry','wWinMain / GraphicsContext\nmain.cpp\n隐藏 GLFW OpenGL 上下文'),
 ('scene','Scene · 场景协调\nscene.h\n最多 5 个角色；深度排序与避让'),
 ('input','NativeDesktopInput\nnative_input.h / input_activity.h\n异步 Raw Input；连续输入宽限 3s'),
 ('settings','NativeSettings + NativePreferences\nnative_settings.h / native_preferences.h\nWin32 菜单；共享设置'),
 ('roster','SceneRoster\nshared/scene_state.h\n历史 15 条；当前角色选择'),
 ('app','App · 每角色宿主\nmain.cpp\n模式、交互、行为、位置、渲染状态'),
 ('cafe','BehaviorTree.CPP 咖啡厅树\npet_behavior.h + template.bt.xml\n每角色 XML；热重载与安全 Idle'),
 ('battle','pet::Battle 战斗状态机\nshared/pet_battle.h\n16 个 Phase；按能力降级'),
 ('motion','公共移动与朝向\nPetGround / DirectedWalk\nCafeAttention / StopTurn / FootStop'),
 ('geometry','DesktopGeometry / DesktopPath\n每网格待机边界；单屏停靠\nVisibleWindowEdges：可见窗口边'),
 ('playback','pet::Playback\nclip / time / loop / generation\n动作请求与宿主位置汇入帧状态'),
 ('model','GltfModel · 独立 cgltf 适配\ngltf_model.cpp / gltf_model.h\n原始 GLB；多 Skin 与 Morph'),
 ('jobs','FrameJobs · 持久工作线程\nframe_jobs.h\n逐角色 CPU 动画 / 蒙皮'),
 ('barrier','主线程等待全部 CPU 任务\n单角色内联计算\n完成后才上传/销毁模型'),
 ('renderer','NativeRenderer\nnative_renderer.cpp\n动态顶点上传 → 材质/线条绘制'),
 ('fbo','紧凑 FBO + 至多 2× SSAA\nGPU 回读 RGBA\n缓存姿态范围、帧和命中区域'),
 ('win','Win32 透明显示 + 输入窗\nUpdateLayeredWindow + alpha Region\n置顶；透明区域鼠标穿透'),
 ('session','NativeSession + NativeLoop\n125 Hz 动作计时；60 FPS 绘制\n睡眠/锁屏/熄屏暂停；消息预算'),
 ('health','NativeHealth · 诊断观察线程\n主线程无进展 ≥ 8s\n保存 hang.txt / hang.dmp'),
 ('disk','本地数据\nsettings.ini / characters.json\nbehavior/*.bt.xml / logs/*','data'),
], [('launch','entry'),('entry','scene'),('input','scene','输入事件'),('settings','scene','参数 / 导入 / 列表操作'),('roster','scene','显示角色'),('scene','app','协调各角色'),('session','scene','消息与计时'),('app','cafe','咖啡厅'),('app','battle','战斗'),('app','motion'),('motion','geometry','路径与落点检查'),('cafe','playback','动作请求'),('battle','playback','动作请求'),('motion','playback','位置 / 朝向 / 覆盖动作'),('model','jobs','各模型独占一个任务'),('playback','jobs','采样帧状态'),('jobs','barrier','全部 future 完成'),('barrier','renderer'),('renderer','fbo'),('fbo','win'),('win','input','本角色鼠标交互','dashed'),('settings','disk','保存 / 读取','dashed'),('roster','disk','保存 / 读取','dashed'),('cafe','disk','轮询 XML','dashed'),('health','disk','诊断文件','dashed'),('health','scene','观察阶段；不操作渲染','dashed')],['main.cpp','scene.h','gltf_model.cpp','native_renderer.cpp','frame_jobs.h','native_session.h','native_loop.h','native_health.h'])

graph('02-behavior-overview','角色行为总览','行为并非一棵大树：咖啡厅由行为树决定基础动作；战斗由状态机决定；跟随、停靠、视线追踪在宿主中覆盖显示或移动。',[
 ('start','启动 / 导入 GLB'),('cap','分析动作能力\n咖啡厅映射；战斗姿态集合'),('mode','选择角色模式','decision'),
 ('cafe','咖啡厅行为树\n拖拽 → 点击 → 返回起点\n散步 → Cafe_Idle'),('battle','战斗状态机\nIdle / Move / Attack / Reload\nCallsign / EX / Victory / Retreat'),
 ('input','输入观测\n3s 宽限 → inputActive\n持续计时 / 无输入计时'),
 ('follow','位置跟随覆盖\n仅基础 Idle + 输入活跃\n区间外启动，停在中点'),
 ('dock','待机单屏停靠\n固定目标；避免 Idle 跨屏'),
 ('look','咖啡厅视线覆盖\n持续输入达到阈值\n平滑转身 / 30° 阈值转身'),
 ('integrate','宿主整合当前帧\n曲线朝向、脚部减速\n碰撞停止、透视大小、深度排序'),
 ('render','原生渲染与透明窗口\nCPU 蒙皮 → GPU → Win32'),
 ('pause','暂停门\n睡眠 / 锁屏 / 熄屏：全部暂停\n菜单：行为暂停；Jump 继续'),
 ('resume','恢复桌面\n重置帧计时与缓存\n继续当前动作'),
 ('interact','用户交互\n左拖移动；右拖旋转；滚轮缩放\n右键点击菜单'),
 ('exit','退出\n红色退出 / 快捷键\n关闭最后角色须确认','exit')
], [('start','cap'),('cap','mode','无战斗姿态只保留咖啡厅'),('mode','cafe','cafe'),('mode','battle','battle；有战斗能力'),('input','cafe'),('input','battle'),('input','follow'),('input','look'),('cafe','battle','1.2s 内角色三连击；有战斗能力'),('battle','cafe','死亡/退场结束；或菜单切模式'),('cafe','integrate'),('battle','integrate'),('follow','integrate'),('dock','integrate'),('look','integrate'),('integrate','render'),('interact','integrate','强制交互 / 缩放'),('interact','pause','打开菜单'),('pause','resume','桌面可用 / 关闭菜单'),('resume','mode','按现有模式继续'),('interact','exit','明确退出或关闭最后角色')],['main.cpp:App::tick / presentState','scene.h:Scene::tick / message','shared/pet_behavior.h','shared/pet_battle.h'])

graph('03-cafe-behavior-tree','咖啡厅行为树 · 实际运行分支','ReactiveFallback 按编号顺序选择，前面的有效分支可打断后面的运行分支。模板中旧概率视线分支仍存在，但原生宿主调用树时关闭 trackingEnabled，实际视线由 CafeAttention 处理。',[
 ('root','ReactiveFallback\n每次 tick 按 1 → 6 检查','decision'),
 ('drag','1 · ReactiveSequence\nIsDragging → LoopAnimation\nFormation_Pickup'),
 ('click','2 · Sequence\nHasPendingClick → PlayAnimation\nCafe_Reaction；消费一次点击'),
 ('home','3 · ReturnHome\n散步后恢复输入且未开启位置跟随\nCafe_Walk；直线返回本轮起点'),
 ('legacy','4 · 旧视线 Sequence（未启用）\nActiveInputChance → FaceCursor\n原生宿主 trackingEnabled=false','inactive'),
 ('wander','5 · ReactiveSequence\nUserInactive 达标 + 允许散步','decision'),
 ('cooldown','Sequence\nIdleCooldownReady\n间隔 × 随机时间幅度'),
 ('walk','WalkRandom → Cafe_Walk\n8 方向加权选择；距离带随机\n上行权重 0.4、路程倍率 2.5'),
 ('idle','6 · LoopAnimation\nCafe_Idle\n纯待机动画速度 75%'),
 ('finish','单次动作 / 返回 / 散步完成\n重置冷却，重新从根节点评估'),
 ('cap','导入时按能力修剪\n无 Reaction：移除点击分支\n无 Walk：移除返程和散步分支','note'),
 ('override','宿主覆盖层（见移动/视线图）\n输入活跃 + 位置跟随会抑制随机散步\n跟随仍以基础 Idle 为宿主状态','note'),
 ('fallback','XML 每 0.5s 检查更新\n无效更新保留旧树\n首次无效 / 执行异常 → SafeIdle','note')
], [('root','drag','1'),('root','click','2'),('root','home','3'),('root','legacy','4','dashed'),('root','wander','5'),('root','idle','6'),('wander','cooldown','无输入 ≥ input_idle'),('cooldown','walk','待机冷却达到间隔'),('drag','root','松开/取消拖拽'),('click','finish','动作结束'),('home','finish','到达起点；面向屏幕'),('walk','finish','路程完成 / 输入恢复中断'),('finish','root'),('cap','root','动作映射与修剪','dashed'),('override','root','宿主约束','dashed'),('fallback','idle','安全待机','dashed')],['design/template.bt.xml','shared/pet_behavior.h','main.cpp:initializeBehavior / App::tick'])

graph('04-battle-state-machine','战斗完整状态机 · 16 个 Phase','这张图覆盖常规流转及各枚举状态；跨状态打断优先级见下一张图。姿态只从可用 Idle + Attack 配对中选择，Normal 优先，缺少其他姿态会跳过。',[
 ('entry','进入战斗 / 拖拽松开 / 胜利完成\n选首个可用姿态；ready=false','note'),
 ('Idle','Idle · 姿态_Idle\n循环；键盘、点击指挥、走动入口'),
 ('Moving','Moving · Move_Ing 循环\n战斗速度 = 0.792 × zoom × 3\n输入走动距离 = 正常的 50%'),
 ('Jump','Jump · Move_Jump 单次\n提前起跳；边界位于动作中点\n保持方向、速度与连续位移'),
 ('MoveEnd','MoveEnd · Move_End_姿态\n随机选择可用姿态；末段曲线转向\n按脚部采样减速至站稳'),
 ('AttackStart','AttackStart · 姿态_Attack_Start\n准备攻击；保留首次攻击请求'),
 ('Attack','Attack · 姿态_Attack_Ing\n播放一次；重复按键可缓存'),
 ('AttackDelay','AttackDelay · 姿态_Attack_Delay\n循环等待；优先处理换弹请求'),
 ('AttackEnd','AttackEnd · 姿态_Attack_End\n收枪；可携带 reloadPending'),
 ('Reload','Reload · 姿态_Reload\n或姿态_Attack_Reload；单次\n忽略键盘及换弹；清理攻击缓存'),
 ('Callsign','Callsign · 姿态_Callsign 单次\n按追踪开关面向鼠标\n全场随机一名候选再掷概率'),
 ('Pickup','Pickup · Formation_Pickup 循环\n缺少时用首姿态 Idle\n拖拽覆盖任何战斗状态'),
 ('Skill','Skill · Exs 单次\n保存阶段、动作时间、剩余行走\n结束恢复保存状态'),
 ('VictoryStart','VictoryStart · Victory_Start 单次\n强制面向屏幕'),
 ('VictoryEnd','VictoryEnd · Victory_End × 3\n全过程强制面向屏幕'),
 ('Death','Death · Vital_Death 单次'),
 ('Retreat','Retreat · Vital_Retreat 单次\n显示方向 +180°；上升屏幕外\n位移按 t² 先慢后快'),
 ('cafe','退出战斗 → 咖啡厅','exit'),
 ('interrupt','跨状态入口\n见优先级图','note'),
 ('saved','被 EX 打断的普通状态\n及其播放进度','note')
], [('entry','Idle'),('Idle','AttackStart','按键或已缓存按键；存在 Start'),('Idle','Attack','按键；缺少 Start'),('AttackStart','Attack','准备结束；执行首次攻击'),('Attack','AttackDelay','攻击完成；若缺 Delay 则 Idle'),('AttackDelay','Attack','新按键 / 缓存攻击'),('AttackDelay','AttackEnd','无键盘 ≥ keyboardIdle'),('AttackDelay','AttackEnd','有换弹请求且存在 End'),('AttackDelay','Reload','换弹请求；缺少 End'),('AttackEnd','Reload','结束后有请求且存在 Reload'),('AttackEnd','Idle','结束后无换弹请求'),('Reload','Idle','单次结束'),('Idle','Callsign','鼠标抬键事件；当选且概率成功'),('Callsign','Idle','结束；保持选中姿态'),('Idle','Moving','允许走动；间隔到期 + 输入/无输入条件'),('Moving','Jump','可跨越屏幕/可见窗口；存在 Jump'),('Jump','Moving','落地后仍有路程且允许走动'),('Jump','MoveEnd','落地后路程完成或走动关闭'),('Moving','MoveEnd','走完 / 禁用自动走动'),('MoveEnd','Idle','站稳且结束动作完成；缺 End 用 Walk 制动'),('interrupt','Pickup','拖拽'),('Pickup','entry','松开 / 取消拖拽'),('interrupt','Skill','持续输入；EX 概率成功'),('saved','Skill','保存','dashed'),('Skill','saved','播放完成：恢复','dashed'),('interrupt','VictoryStart','其他应用窗口关闭；有 Start'),('interrupt','VictoryEnd','窗口关闭；缺 Start、有 End'),('VictoryStart','VictoryEnd','Start 完成；有 End'),('VictoryStart','entry','缺少 End'),('VictoryEnd','VictoryEnd','已播放次数 < 3'),('VictoryEnd','entry','第 3 次结束'),('interrupt','Death','角色三连击；有 Death'),('interrupt','Retreat','三连击；缺 Death、有 Retreat'),('interrupt','cafe','三连击；两者都无'),('Death','Retreat','死亡结束；有 Retreat'),('Death','cafe','缺 Retreat'),('Retreat','cafe','退场结束')],['shared/pet_battle.h:Battle::Phase / tick','main.cpp:initializeBehavior / App::tick / presentState'])

graph('05-battle-priority','战斗打断与请求优先级','按实际 Battle::tick / App::tick 的顺序绘制。拖拽可打断 Jump；正常输入和关窗不会让跳跃途中停下。随机走动期间的按键先缓存，DirectedWalk 跟随则在非 Jump 时可被攻击打断。',[
 ('event','本帧输入与当前阶段'),('buffer','先缓存请求\n攻击请求去重 ≥ 0.12s\n四个攻击阶段记录换弹请求'),
 ('triple','角色 1.2s 内三连击？','decision'),('retire','设置 Death / Retreat / 返回咖啡厅'),
 ('drag','正在拖拽？','decision'),('pickup','最高优先级 → Pickup\n清理技能/换弹；拖动立即返回'),
 ('drop','刚结束拖拽？','decision'),('reset','回首个姿态 Idle\n清理被打断技能状态'),
 ('close','存在其他应用关窗请求？\n非 Jump、非退场、非胜利','decision'),('victory','VictoryStart / VictoryEnd\nJump 中请求延后到落地'),
 ('menu','菜单打开且不在 Jump？','decision'),('hold','暂停行为\nJump 保持动画与移动'),
 ('special','当前是 EX / 退场 / 胜利？','decision'),('specialRun','推进对应动画并处理结束\n不尝试新的 EX / 常规状态条件'),
 ('ex','输入活跃；EX 计时达阈值？\n非 Jump、非 Reload','decision'),('try','存在 Exs 且概率成功？','decision'),('skill','保存普通状态 → Skill'),
 ('normal','常规 Phase 分支\nIdle：攻击 > 指挥 > 走动\nAttackDelay：换弹 > 收枪 > 攻击'),
 ('reload','Reload 特例\n屏蔽键盘/换弹，不缓存攻击\n拖拽、三连击和关窗仍可打断','note'),
 ('jump','Jump 输入延后\n跟随 Jump：宿主缓存按键/换弹/关窗\n普通 Jump：攻击缓存、关窗延后','note')
], [('event','buffer'),('buffer','triple'),('triple','retire','是'),('triple','drag','否'),('retire','drag','拖拽仍可覆盖'),('drag','pickup','是'),('drag','drop','否'),('drop','reset','是'),('drop','close','否'),('reset','close'),('close','victory','是'),('close','menu','否'),('victory','menu'),('menu','hold','是'),('menu','special','否'),('special','specialRun','是'),('special','ex','否'),('ex','try','是；计时归零'),('ex','normal','否'),('try','skill','是'),('try','normal','否'),('reload','buffer','屏蔽键盘请求','dashed'),('jump','close','延后普通请求','dashed')],['shared/pet_battle.h:Battle::tick','main.cpp:App::tick','scene.h:Scene::tick'])

graph('06-follow-and-placement','两模式公共移动 · 跟随与单屏停靠','DirectedWalk 是宿主移动覆盖，不增加 Battle::Phase。跟随身位按角色本体宽度投影，屏幕可达性与其他角色碰撞优先于鼠标目标。',[
 ('idle','基础动作处于 Idle\n非交互、非返程、非菜单暂停'),
 ('dock','待机范围跨屏且需要检查？','decision'),('target','固定单屏停靠目标\n允许散步且有 Walk：行走到目标\n否则一次放置到可停靠点'),
 ('follow','开启位置跟随 + inputActive？\n有 Walk；无阻挡锁存/交互冷却','decision'),
 ('distance','距离在 [最近, 最远] 内？','decision'),('stay','Idle 保持位置与朝向\n无需维持鼠标附近'),
 ('plan','规划距离中点目标\n最近=0：只靠近、不远离\n每 0.15s 可重新规划'),
 ('valid','路径可通行且落点完整在单屏？','decision'),
 ('walk','DirectedWalk 行走覆盖\n咖啡厅 Cafe_Walk / 战斗 Move_Ing\n末端约 0.35s 距离制动'),
 ('boundary','仅战斗且存在 Jump？\n边界将到动作中点；落点可达\n朝向误差 ≤ 3°；无其他角色阻挡','decision'),
 ('jump','Move_Jump 播放一次\n方向和速度锁定、位移连续\n不重规划，不因到达距离而中断'),
 ('finish','到达中点 / 固定目标？\n跨屏时先完整进入一个屏幕','decision'),
 ('arrive','停止 DirectedWalk → 基础 Idle\n停靠可恢复进入前朝向\n跟随不强制面向鼠标'),
 ('blocked','碰撞 / 路径阻挡\n立即停止并放弃目的地\n鼠标移动 >16px 才解除重试锁存'),
 ('inactive','输入不活跃\n结束普通跟随；跨屏/跳跃先完成\n允许散步后由各模式决定散步'),
 ('override','强制交互或基础状态改变\n拖拽/旋转：即时取消\n非 Jump 攻击切换：取消覆盖','note')
], [('idle','dock'),('dock','target','是'),('dock','follow','否'),('target','valid','行走可用'),('target','arrive','直接放置'),('follow','distance','是'),('follow','stay','否'),('distance','stay','是且尚未移动'),('distance','plan','区间外；或继续向中点移动'),('plan','valid'),('valid','walk','是'),('valid','stay','否；不启动不可达路径'),('walk','boundary'),('boundary','jump','是'),('boundary','finish','否；咖啡厅始终否'),('jump','walk','落地；继续剩余行走'),('finish','arrive','是'),('finish','walk','否'),('walk','blocked','步进碰撞'),('blocked','idle','静止；等待解锁'),('stay','inactive','无输入达阈值'),('inactive','idle','散步结束'),('override','arrive','取消覆盖','dashed')],['shared/directed_walk.h','shared/desktop_path.h','shared/desktop_geometry.h','shared/window_boundaries.h','main.cpp:App::tick / stopBlocked','scene.h:steer'])

graph('07-cafe-look-tracking','咖啡厅视线追踪 · 与位置跟随独立','这是朝向覆盖而非移动状态。满足持续输入后每个角色都可追踪，不再抽概率。眼睛高度用于计算屏幕方向，阈值模式转身时只播放 Walk，不改变地面锚点。',[
 ('input','inputActive 且 cafeTracking 开启？','decision'),('reset','重置追踪\n停止转身动画；保留当前朝向'),
 ('eligible','基础 Cafe_Idle 且无跟随/停靠/返程\n非拖拽/旋转/点击/菜单/交互冷却？','decision'),
 ('wait','暂停本次转身\n等待基础动作重新允许追踪'),
 ('time','持续输入 ≥ activeSeconds\n且鼠标方向有效？','decision'),
 ('mode','视线模式？','decision'),('smooth','平滑追踪\n每帧 turnToward 当前鼠标方向\n仍显示 Cafe_Idle；锚点不动'),
 ('angle','当前未转身且角差 ≥ 30°？','decision'),('still','保持 Idle 与当前朝向'),
 ('turn','锁定本次 targetYaw\n转身覆盖 Cafe_Walk\n朝向最多变化 240°/s；位置不变'),
 ('done','与锁定方向角差 < 0.5°？','decision'),('idle','结束转身 → Cafe_Idle\n后续角差再次达到 30° 才重启')
], [('input','reset','否'),('input','eligible','是'),('eligible','wait','否'),('eligible','time','是'),('time','wait','否'),('time','mode','是'),('mode','smooth','smooth'),('mode','angle','threshold'),('angle','still','否'),('angle','turn','是'),('turn','done'),('done','turn','否'),('done','idle','是'),('idle','input','下一帧'),('smooth','input','下一帧')],['shared/cafe_attention.h','shared/input_activity.h','main.cpp:App::tick / presentState','shared/pet_ground.h:faceCursor'])

graph('08-import-pipeline','原始 GLB 导入与能力降级','运行时直接解析，不转换、不写回 GLB。导入在主线程分阶段完成，Scene 按角色顺序加载；CPU 帧并行不代表导入也并行。',[
 ('file','用户选择 GLB\n历史最多 15 条；显示最多 5 个'),
 ('validate','cgltf_parse / load_buffers / validate\nGLB 2.0；内嵌数据和 PNG/JPEG\n检查必需扩展与 accessor'),
 ('error','不支持 / 数据损坏\n显示导入错误；保留已显示模型','exit'),
 ('nodes','保留完整父节点层级\n多套 Skin、inverseBindMatrices\n节点 TRS / matrix、原始 Morph'),
 ('tracks','读取原始关键帧\nSTEP / LINEAR / CUBICSPLINE\n不用 Blender 烘焙帧'),
 ('catalog','关键词 / 别名 / 动作组映射\n选择一致 family + group\n歧义记录警告、跳过少数动作'),
 ('parts','可选部件驱动检查\noptional + show/hide + 动画通道\n唯一塌缩 switch 面部保底'),
 ('spatial','测量与缓存\n尺寸归一、眼高、本体宽度\n待机每网格边界、动作包络、脚部制动'),
 ('caps','能力降级','decision'),
 ('cafe','咖啡厅缺省映射\nIdle → 战斗 Idle / 首动作\nWalk → 战斗 Move；Pickup → Idle'),
 ('battle','战斗姿态必须 Idle + Attack\n优先 Normal；少姿态不参与随机\n无姿态：不提供战斗模式'),
 ('missing','可选动作不存在\nStart/End 跳过；Jump 不检查\n无 Delay 返回 Idle；无技能/换弹不触发'),
 ('runtime','生成/复用每角色 .bt.xml\n创建 Behavior 与 Battle\n原生模型就绪，进度 100%'),
 ('clip','运行帧\n采样 → 0.18s 内姿态混合\nMorph + 世界矩阵 + 蒙皮 → GPU')
], [('file','validate'),('validate','error','校验失败'),('validate','nodes','合法'),('nodes','tracks'),('tracks','catalog'),('catalog','parts'),('parts','spatial'),('spatial','caps'),('caps','cafe'),('caps','battle'),('caps','missing'),('cafe','runtime'),('battle','runtime'),('missing','runtime'),('runtime','clip')],['gltf_model.cpp:constructor / discoverAnimations / analyze','main.cpp:requestModel / initializeBehavior','scene.h:activate / tick'])

for g in GRAPHS:
    ids={n['id'] for n in g['nodes']}
    assert len(ids)==len(g['nodes'])
    assert all(e['source'] in ids and e['target'] in ids for e in g['edges'])
    def q(s):return json.dumps(s,ensure_ascii=False)
    dot=['digraph G {',f'graph [rankdir={g["direction"]}, bgcolor="transparent", pad=0.35, nodesep=0.45, ranksep=0.65, fontname="Microsoft YaHei", fontsize=18, splines=polyline];',
         'node [shape=box, style="rounded,filled", fillcolor="#eef8fc", color="#7cafc3", fontcolor="#183848", fontname="Microsoft YaHei", fontsize=14, margin="0.18,0.15", width=3.8];',
         'edge [color="#7093a3", fontcolor="#47697b", fontname="Microsoft YaHei", fontsize=12, arrowsize=0.65];']
    mmd=['%% ba_cafe_lite native 0.2.5 · actual source snapshot',f'flowchart {g["direction"]}']
    for n in g['nodes']:
        attrs=[]
        if n['kind']=='decision':attrs+=['shape=diamond','fillcolor="#fff5d9"']
        if n['kind']=='inactive':attrs+=['style="rounded,dashed,filled"','fillcolor="#f2f3f5"','fontcolor="#78848b"']
        if n['kind']=='note':attrs+=['shape=note','fillcolor="#f4f6f8"']
        if n['kind']=='exit':attrs+=['fillcolor="#fff0ef"','color="#cc8c89"']
        dot.append(f'{n["id"]} [label={q(n["label"])}{","+",".join(attrs) if attrs else ""}];')
        text=n['label'].replace('\n','<br/>').replace('"','&quot;')
        label='{'+q(text)+'}' if n['kind']=='decision' else '['+q(text)+']'
        mmd.append(f'  {n["id"]}{label}')
    for e in g['edges']:
        dot.append(f'{e["source"]} -> {e["target"]} [label={q(e["label"])}'+(',style=dashed,constraint=false' if e['kind']=='dashed' else '')+'];')
        op='-.->' if e['kind']=='dashed' else '-->'
        label='|'+q(e['label'])+'|' if e['label'] else ''
        mmd.append(f'  {e["source"]} {op}{label} {e["target"]}')
    dot.append('}')
    (ROOT/(g['key']+'.dot')).write_text('\n'.join(dot)+'\n',encoding='utf-8')
    (ROOT/(g['key']+'.mmd')).write_text('\n'.join(mmd)+'\n',encoding='utf-8')
(ROOT/'graphs.json').write_text(json.dumps(GRAPHS,ensure_ascii=False,indent=2),encoding='utf-8')
print(f'Generated {len(GRAPHS)} graph sources: {sum(len(g["nodes"]) for g in GRAPHS)} nodes, {sum(len(g["edges"]) for g in GRAPHS)} edges')
