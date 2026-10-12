"""Export the implemented battle FSM as editable draw.io and Mermaid diagrams.
These are design documents; pet_battle.h is the runtime implementation.
"""
from pathlib import Path
from xml.etree import ElementTree as ET
import sys

root = Path(__file__).resolve().parents[1]
three = '--three' in sys.argv
destination = root / ('experimental/three/design' if three else 'design')
destination.mkdir(parents=True, exist_ok=True)
nodes = [
    ('cafe', '咖啡厅模式', 25, 30, 205, 70),
    ('any', '任意战斗状态\n拖拽 > 连击退场 > 关窗结算 > 技能', 780, 35, 320, 90),
    ('death', '退场：Vital_Death × 1', 1180, 35, 225, 80),
    ('retreat', 'Vital_Retreat × 1\n上升到屏幕外', 1470, 35, 255, 80),
    ('victory_start', '胜利：Victory_Start × 1', 270, 165, 240, 80),
    ('victory_end', 'Victory_End × 3', 560, 165, 220, 80),
    ('skill', '技能：Exs × 1\n结束恢复原状态和播放进度', 1180, 185, 330, 80),
    ('pickup', '被拖拽\nFormation_Pickup 循环', 45, 420, 250, 90),
    ('normal', '初始 / 松开拖拽 / 胜利后\nNormal_Idle（未选姿态）', 355, 410, 300, 110),
    ('move', '随机移动：Move_Ing\n有输入 0.6–1.2s；无输入 2–4s\n速度 ×2.5，随角色大小变化', 780, 410, 315, 110),
    ('jump', '提前起跳：Move_Jump × 1\n交界处为动作中点；移动不中断', 1180, 370, 280, 90),
    ('move_end', '结束移动\nMove_End_Normal / Stand / Kneel\n只选完整可用姿态；有输入时朝向鼠标', 1470, 480, 330, 110),
    ('idle', '姿态_Idle\nNormal / Stand / Kneel', 1030, 680, 280, 110),
    ('callsign', '指挥：朝向鼠标\n姿态_Callsign × 1\n缺少时使用 Normal_Callsign', 1470, 710, 330, 100),
    ('attack_start', '准备攻击\n姿态_Attack_Start × 1', 75, 825, 260, 90),
    ('attack_delay', '攻击空闲\n姿态_Attack_Delay 循环', 430, 825, 280, 90),
    ('attack', '攻击\n姿态_Attack_Ing × 1', 75, 1060, 260, 90),
    ('attack_end', '攻击结束\n姿态_Attack_End × 1', 810, 1060, 265, 90),
    ('reload', '换弹\n姿态_Reload / Attack_Reload × 1', 1250, 1060, 350, 90),
]
edges = [
    ('cafe', 'normal', '选择战斗模式；模型具备战斗动作'),
    ('any', 'pickup', '拖拽：强制打断（最高优先级）'),
    ('pickup', 'normal', '松开 / 取消拖拽'),
    ('any', 'death', '1.2s 内点击角色三次：强制'),
    ('death', 'retreat', '死亡播放结束'),
    ('retreat', 'cafe', '退场结束'),
    ('any', 'victory_start', '关闭其他应用的顶层窗口'),
    ('victory_start', 'victory_end', '播放结束'),
    ('victory_end', 'normal', '完整播放三次结束'),
    ('any', 'skill', '普通状态持续输入：每 30s 有 5% 概率'),
    ('skill', 'any', '恢复被打断的状态、动作时间和剩余行走'),
    ('normal', 'move', '持续输入 3s 或无输入等待 10–30s'),
    ('normal', 'normal', '无触发：循环 Idle'),
    ('move', 'jump', '预测可跨越屏幕 / 窗口边界；仅有 Jump 的模型'),
    ('jump', 'move', '落地且尚有路程：保持原方向、速度'),
    ('jump', 'move_end', '落地后：走完 / 关闭走动'),
    ('move', 'move_end', '走完 / 关闭走动 / 不可通行的桌面外边缘'),
    ('move_end', 'idle', '结束动作播放完成'),
    ('idle', 'idle', '无触发：循环 Idle'),
    ('idle', 'move', '持续输入 3s 或无输入等待 10–30s'),
    ('idle', 'callsign', '鼠标点击：10% 概率'),
    ('normal', 'callsign', '鼠标点击：10% 概率（若有动作）'),
    ('callsign', 'idle', '完成：已选择姿态'),
    ('callsign', 'normal', '完成：未选择姿态'),
    ('idle', 'attack_start', '键盘输入'),
    ('attack_start', 'attack_delay', '准备动作播放结束'),
    ('attack_delay', 'attack', '连续键盘输入 3s'),
    ('attack', 'attack_delay', '攻击动作播放结束'),
    ('attack_delay', 'attack_end', '无键盘输入 10s（鼠标不重置计时）'),
    ('attack_end', 'idle', '播放结束'),
    ('attack_delay', 'reload', '句号 / 回车 / 分号 / 问号键（若有换弹动作）'),
    ('reload', 'idle', '播放结束'),
]

if three:
    replacements={
        'cafe':'咖啡厅模式\n速度 = 战斗的 1/3；不跳跃',
        'normal':'初始 / 松开拖拽 / 胜利后\n默认可用姿态_Idle（Normal 优先）',
        'move_end':'结束移动：Move_End_姿态（若有）\n按停止动作脚部采样减速，站稳后无位移\n移动末段曲线转向，停止朝向不突变',
        'idle':'姿态_Idle',
        'move':'随机移动：Move_Ing\n有输入：先远离鼠标，距离 ×50%\n末段曲线转向鼠标，接脚部减速\n战斗速度 2.376 × 缩放',
        'victory_start':'胜利：Victory_Start × 1\n强制面向屏幕',
        'victory_end':'Victory_End × 3\n全过程面向屏幕',
        'attack_start':'准备攻击\n姿态_Attack_Start（缺少则跳过）',
        'attack_end':'攻击结束\n姿态_Attack_End（缺少则跳过）',
        'reload':'攻击换弹\n姿态_Attack_Reload × 1\n期间忽略键盘，不缓存攻击',
    }
    nodes = [(key, replacements.get(key,label.replace('上升到屏幕外', '当前朝向 +180°，上升到屏幕外')), x, y, w, h)
             for key, label, x, y, w, h in nodes]
    edges = [(source, 'attack' if source == 'attack_start' and target == 'attack_delay' else
              'attack_end' if source == 'attack_delay' and target == 'reload' else target,
              '准备结束，执行首次按键攻击' if source == 'attack_start' else
              '按下键盘，无需持续 3s' if source == 'attack_delay' and target == 'attack' else
              '句号 / 回车 / 分号 / 问号：先收尾，排队换弹' if source == 'attack_delay' and target == 'reload' else
              '播放结束，无换弹请求' if source == 'attack_end' and target == 'idle' else label)
             for source, target, label in edges]
    edges.append(('normal', 'attack_start', '按下键盘，准备并执行一次攻击'))
    edges.append(('attack_end', 'reload', '收尾完成或缺少 End；有换弹请求'))
    edges.append(('normal', 'attack', '按键且缺少 Start：直接攻击'))
    edges.append(('idle', 'attack', '按键且缺少 Start：直接攻击'))
    edges.append(('cafe','normal','三连击：模型有战斗动作时切换；只控制被点击角色'))
    edges=[(source,target,label.replace('屏幕 / 窗口边界','屏幕 / 实际可见窗口边界')) for source,target,label in edges]
    edges=[(source,target,
        '持续输入 ≥ continued_input 或无输入 ≥ input_idle；满足走动间隔' if target=='move' and source in ('normal','idle') else
        '每 continued_input 秒尝试 EX 触发概率' if target=='skill' else
        '鼠标点击：随机一个角色，点击指挥概率（默认 10%）' if target=='callsign' else
        '无键盘输入 ≥ keyboard_idle（默认 10s；鼠标不重置）' if source=='attack_delay' and target=='attack_end' else label)
        for source,target,label in edges]
    nodes += [
        ('follow','跟随鼠标（两模式）\nWalk：最近 / 最远距离可调\n身位按本体宽度；屏幕范围优先',25,230,310,120),
        ('dock','停靠单屏\n使用行走动作；固定 Idle 范围\n停稳后恢复 Idle',380,230,310,120),
    ]
    edges += [('cafe','follow','开启跟随且允许移动：立即启动，不等待散步条件'),
              ('normal','follow','开启跟随：立即按距离规划'),
              ('idle','follow','开启跟随：立即按距离规划；Idle 不持续转向鼠标'),
              ('follow','normal','到达距离；战斗恢复原空闲姿态'),
              ('follow','cafe','到达距离；咖啡厅恢复 Idle'),
              ('normal','dock','Idle 范围跨屏；一次固定目标停靠'),
              ('idle','dock','Idle 范围跨屏；一次固定目标停靠'),
              ('cafe','dock','Idle 范围跨屏；一次固定目标停靠'),
              ('dock','idle','战斗恢复原空闲姿态；无 Walk 时一次放置'),
              ('dock','cafe','咖啡厅恢复 Idle'),
              ('follow','attack_start','按键打断（战斗）；Jump 时排队到落地'),
              ('follow','jump','仅战斗：预测屏幕 / 可见窗口交界；有 Jump 且落点可达'),
              ('jump','follow','跟随 Jump 落地：只播放一次，保持移动；重新规划鼠标目标'),
              ('follow','pickup','拖拽即时打断'),('dock','pickup','拖拽即时打断')]

diagram = ET.Element('mxfile', {'host': 'app.diagrams.net', 'version': '24.7.17'})
page = ET.SubElement(diagram, 'diagram', {'id': 'battle-mode', 'name': '战斗模式'})
model = ET.SubElement(page, 'mxGraphModel', {'dx': '1800', 'dy': '1250', 'grid': '1', 'gridSize': '10',
    'guides': '1', 'tooltips': '1', 'connect': '1', 'arrows': '1', 'fold': '1', 'page': '1',
    'pageScale': '1', 'pageWidth': '1840', 'pageHeight': '1260', 'math': '0', 'shadow': '0'})
cells = ET.SubElement(model, 'root')
ET.SubElement(cells, 'mxCell', {'id': '0'})
ET.SubElement(cells, 'mxCell', {'id': '1', 'parent': '0'})
for key, label, x, y, w, h in nodes:
    color = '#fff1f0' if key in ('death', 'retreat') else '#e9f7ff'
    cell = ET.SubElement(cells, 'mxCell', {'id': key, 'value': label.replace('\n', '<br>'),
        'style': f'rounded=1;whiteSpace=wrap;html=1;fillColor={color};strokeColor=#1eaee6;fontColor=#253951;fontSize=16;',
        'vertex': '1', 'parent': '1'})
    ET.SubElement(cell, 'mxGeometry', {'x': str(x), 'y': str(y), 'width': str(w), 'height': str(h), 'as': 'geometry'})
for i, (source, target, label) in enumerate(edges):
    cell = ET.SubElement(cells, 'mxCell', {'id': f'edge{i}', 'value': label,
        'style': 'edgeStyle=orthogonalEdgeStyle;rounded=1;html=1;endArrow=block;endFill=1;fontSize=13;labelBackgroundColor=#ffffff;strokeColor=#627d94;',
        'edge': '1', 'parent': '1', 'source': source, 'target': target})
    ET.SubElement(cell, 'mxGeometry', {'relative': '1', 'as': 'geometry'})
note = ET.SubElement(cells, 'mxCell', {'id': 'note', 'value':
    '两版共用 src/pet_battle.h。图是设计文档，编辑图不自动修改程序。<br>Jump 期间保持移动，设置/关窗/技能反应等待落地；拖拽/三连击仍强制打断。<br>无 Jump 时不检测交界，正常移动；无完整 Normal 战斗姿态时禁用战斗模式（Hibiki）。',
    'style': 'text;html=1;whiteSpace=wrap;align=left;fontSize=14;fontColor=#627d94;', 'vertex': '1', 'parent': '1'})
ET.SubElement(note, 'mxGeometry', {'x': '25', 'y': '1175', 'width': '1740', 'height': '70', 'as': 'geometry'})
ET.indent(diagram)
if three:
    note.set('value', note.get('value').replace('两版共用 src/pet_battle.h。',
             'Three.js：默认从当前骨骼与表情混合 0.18s；停止动作根据导入时脚部运动采样减速，脚站稳后不再额外位移。窗口交界仅检测露出的原始边缘。')
             .replace('无完整 Normal 战斗姿态', '无可用 Idle + Attack 姿态'))
    note.set('value',note.get('value')+'<br>最多 5 个角色，共享网页参数；各自模式保存在角色历史。输入暂停容忍默认 2s；动画中最多缓存一个按键攻击请求。咖啡厅不 Jump。起步前检查完整路径与减速落点；跨屏连续完成，无安全方向就不走。')
(destination / 'battle-mode.drawio').write_bytes(ET.tostring(diagram, encoding='utf-8', xml_declaration=True))

mermaid = ['%% Design document; runtime: src/pet_battle.h', 'flowchart TB']
for key, label, *_ in nodes:
    mermaid.append(f'  {key}["{label.replace(chr(10), "<br/>")}"]')
for source, target, label in edges:
    mermaid.append(f'  {source} -->|"{label}"| {target}')
mermaid.extend(['  classDef base fill:#e9f7ff,stroke:#1eaee6,color:#253951;',
    '  classDef exit fill:#fff1f0,stroke:#e67e75,color:#253951;',
    '  class ' + ','.join(n[0] for n in nodes if n[0] not in ('death', 'retreat')) + ' base;',
    '  class death,retreat exit;'])
(destination / 'battle-mode.mmd').write_text('\n'.join(mermaid)+'\n', encoding='utf-8')

assert len({node[0] for node in nodes}) == len(nodes)
ids = {node[0] for node in nodes}
assert all(source in ids and target in ids for source, target, _ in edges)
print(f'Exported battle-mode.drawio and battle-mode.mmd: {len(nodes)} states, {len(edges)} transitions')
