// Documentation-only renderer: NODE_PATH may point to bundled @viz-js/viz.
const fs=require('node:fs'),path=require('node:path');
const {instance}=require('@viz-js/viz');
const root=__dirname,graphs=JSON.parse(fs.readFileSync(path.join(root,'graphs.json'),'utf8'));
const esc=s=>String(s).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
(async()=>{
 const viz=await instance(),pages=[];
 for(const graph of graphs){
  const dot=fs.readFileSync(path.join(root,graph.key+'.dot'),'utf8');
  const svg=viz.renderString(dot,{format:'svg'}).replace(/<\?xml[^>]*\?>\s*/,'').replace(/<!DOCTYPE[\s\S]*?\]>\s*/,'').replace(/<!DOCTYPE[^>]*>\s*/,'');
  fs.writeFileSync(path.join(root,graph.key+'.svg'),svg);
  const layout=JSON.parse(viz.renderString(dot,{format:'json'})),bb=layout.bb.split(',').map(Number),H=bb[3];
  const cells=['<mxCell id="0"/><mxCell id="1" parent="0"/>'],objects=new Map();
  for(const node of layout.objects||[]){if(!node.pos)continue;objects.set(node._gvid,node.name);const [x,y]=node.pos.split(',').map(Number),w=Number(node.width)*72,h=Number(node.height)*72;
   const kind=graph.nodes.find(n=>n.id===node.name)?.kind,shape=kind==='decision'?'rhombus':kind==='note'?'note':'rectangle';
   const fill=kind==='decision'?'#fff5d9':kind==='exit'?'#fff0ef':kind==='inactive'?'#f2f3f5':'#eef8fc';
   cells.push(`<mxCell id="${esc(node.name)}" value="${esc(node.label)}" style="shape=${shape};rounded=1;whiteSpace=wrap;html=0;fontFamily=Microsoft YaHei;fontSize=14;fillColor=${fill};strokeColor=#7cafc3;fontColor=#183848;${kind==='inactive'?'dashed=1;':''}" vertex="1" parent="1"><mxGeometry x="${(x-w/2+32).toFixed(2)}" y="${(H-y-h/2+56).toFixed(2)}" width="${w.toFixed(2)}" height="${h.toFixed(2)}" as="geometry"/></mxCell>`);
  }
  graph.edges.forEach((edge,i)=>{cells.push(`<mxCell id="edge-${i}" value="${esc(edge.label)}" style="edgeStyle=orthogonalEdgeStyle;rounded=0;html=0;fontFamily=Microsoft YaHei;fontSize=12;strokeColor=#7093a3;fontColor=#47697b;endArrow=block;endFill=1;${edge.kind==='dashed'?'dashed=1;':''}" edge="1" parent="1" source="${edge.source}" target="${edge.target}"><mxGeometry relative="1" as="geometry"/></mxCell>`);});
  cells.push(`<mxCell id="page-title" value="${esc(graph.title)}" style="text;html=0;fontFamily=Microsoft YaHei;fontSize=22;fontStyle=1;align=left;" vertex="1" parent="1"><mxGeometry x="32" y="0" width="900" height="36" as="geometry"/></mxCell>`);
  pages.push(`<diagram id="${graph.key}" name="${esc(graph.title)}"><mxGraphModel dx="1280" dy="800" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="0" pageScale="1" pageWidth="${bb[2]+64}" pageHeight="${H+88}"><root>${cells.join('')}</root></mxGraphModel></diagram>`);
  graph.svg=svg;
 }
 fs.writeFileSync(path.join(root,'ba-cafe-lite.drawio'),`<mxfile host="app.diagrams.net" version="24.7.17" type="device">${pages.join('')}</mxfile>`);
 const payload=JSON.stringify(graphs).replaceAll('<','\\u003c');
 const html=`<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ba_cafe_lite · 代码与角色行为图</title>
<style>
*{box-sizing:border-box}body{margin:0;font:15px/1.6 "Microsoft YaHei UI",system-ui,sans-serif;color:#17374a;background:#f5f9fc}header{padding:22px 28px 14px;background:#fff;border-top:4px solid #2aaedb}h1{margin:0;font-size:22px;font-weight:600}header p{margin:5px 0 14px;color:#537284}nav{display:flex;flex-wrap:wrap;gap:8px}button,a.control{font:inherit;padding:7px 12px;border:1px solid #d4e3ec;border-radius:7px;background:#fff;color:#22516b;cursor:pointer;text-decoration:none}button:hover,a.control:hover{background:#eaf7fd}button[aria-selected=true]{background:#138db8;border-color:#138db8;color:#fff}button:focus-visible,a:focus-visible{outline:3px solid #82d9f4;outline-offset:2px}main{padding:14px 28px 24px}.bar{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:10px}.bar h2{margin:0 auto 0 0;font-size:18px}#description{margin:8px 0 12px;max-width:1200px;color:#47697b}#canvas{height:calc(100vh - 310px);min-height:430px;background:#fff;border:1px solid #d4e3ec;border-radius:9px;overflow:auto;cursor:grab;touch-action:none}#canvas.dragging{cursor:grabbing}#canvas svg{display:block;max-width:none;flex-shrink:0}#zoom{min-width:54px;text-align:center;font-variant-numeric:tabular-nums}footer{display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap;margin-top:10px;font-size:13px;color:#547386}footer a{color:#147ea8}#sources{overflow-wrap:anywhere}details{margin-top:16px;padding:12px 0}summary{cursor:pointer}details ul{padding-left:22px} @media(max-width:650px){header{padding:15px}main{padding:12px 15px}#canvas{height:65vh;min-height:350px}h1{font-size:19px}}
</style></head><body>
<header><h1>ba_cafe_lite · 代码框架与完整角色行为</h1><p>原生 raylib + cgltf · 0.2.5 代码快照 · 文档图，不改变运行逻辑</p><nav id="tabs" role="tablist" aria-label="图表分类"></nav></header>
<main><div class="bar"><h2 id="title"></h2><button id="fit">适合窗口</button><button id="minus" aria-label="缩小">−</button><output id="zoom" aria-live="polite"></output><button id="plus" aria-label="放大">＋</button><button id="actual">100%</button><a id="svg-link" class="control" download>导出 SVG</a><a id="mmd-link" class="control" download>Mermaid 源文件</a></div><p id="description"></p><div id="canvas" role="tabpanel" aria-label="图表，滚轮缩放、拖拽平移"></div><footer><span>滚轮缩放 · 拖拽平移 · <a href="ba-cafe-lite.drawio" download>全部图的 draw.io 可编辑文件</a> · <a href="README.md">条件、参数与降级说明</a></span><span id="sources"></span></footer>
<details><summary>读图约定</summary><ul><li>黄色菱形：条件判断；蓝色方框：状态、动作或代码模块；虚线：覆盖、说明或不启用的旧分支。</li><li>cafе / battle 是角色模式；战斗有 16 个 Phase。位置跟随、停靠和咖啡厅视线追踪属于宿主层覆盖。</li><li>图中动作名是语义角色；实际 GLB 动作由关键词、动作组和别名映射。缺少可选动作时按说明降级。</li><li>运行采用共享设置与每角色独立状态。发行默认参数见 README；现有用户设置和每角色 XML 可覆盖默认。</li></ul></details></main>
<script id="graph-data" type="application/json">${payload}</script>
<script>
const graphs=JSON.parse(document.getElementById('graph-data').textContent),tabs=document.getElementById('tabs'),canvas=document.getElementById('canvas');let selected=0,scale=1,svg=null,w=1,h=1;
const buttons=graphs.map((g,i)=>{const b=document.createElement('button');b.type='button';b.id='tab-'+i;b.role='tab';b.setAttribute('aria-controls','canvas');b.textContent=g.title;b.onclick=()=>show(i);tabs.append(b);return b;});
function resize(next,anchor){const old=scale;scale=Math.max(.08,Math.min(3,next));svg.style.width=w*scale+'px';svg.style.height=h*scale+'px';document.getElementById('zoom').value=Math.round(scale*100)+'%';if(anchor){canvas.scrollLeft=(canvas.scrollLeft+anchor.x)*scale/old-anchor.x;canvas.scrollTop=(canvas.scrollTop+anchor.y)*scale/old-anchor.y;}}
function fit(){resize(Math.min((canvas.clientWidth-24)/w,(canvas.clientHeight-24)/h,1));canvas.scrollLeft=canvas.scrollTop=0;}
function show(i){selected=i;const g=graphs[i];buttons.forEach((b,j)=>{b.setAttribute('aria-selected',String(i===j));});document.getElementById('title').textContent=g.title;document.getElementById('description').textContent=g.description;document.getElementById('sources').textContent='代码：'+g.sources.join(' · ');canvas.setAttribute('aria-labelledby','tab-'+i);canvas.innerHTML=g.svg;svg=canvas.querySelector('svg');svg.setAttribute('role','img');svg.setAttribute('aria-label',g.title+'。'+g.description);const box=svg.viewBox.baseVal;w=box.width;h=box.height;document.getElementById('svg-link').href=g.key+'.svg';document.getElementById('mmd-link').href=g.key+'.mmd';scale=1;resize(Math.max(.92,Math.min((canvas.clientWidth-24)/w,1)));canvas.scrollLeft=canvas.scrollTop=0;}
document.getElementById('fit').onclick=fit;document.getElementById('actual').onclick=()=>resize(1);document.getElementById('plus').onclick=()=>resize(scale*1.25);document.getElementById('minus').onclick=()=>resize(scale/1.25);
canvas.addEventListener('wheel',e=>{e.preventDefault();const rect=canvas.getBoundingClientRect();resize(scale*Math.exp(-e.deltaY*.0015),{x:e.clientX-rect.left,y:e.clientY-rect.top});},{passive:false});let dragging=null;
canvas.addEventListener('pointerdown',e=>{if(e.button!==0)return;dragging={x:e.clientX,y:e.clientY,left:canvas.scrollLeft,top:canvas.scrollTop};canvas.classList.add('dragging');canvas.setPointerCapture(e.pointerId);});
canvas.addEventListener('pointermove',e=>{if(!dragging)return;canvas.scrollLeft=dragging.left-e.clientX+dragging.x;canvas.scrollTop=dragging.top-e.clientY+dragging.y;});function end(){dragging=null;canvas.classList.remove('dragging');}canvas.addEventListener('pointerup',end);canvas.addEventListener('pointercancel',end);canvas.addEventListener('lostpointercapture',end);
tabs.addEventListener('keydown',e=>{let i=buttons.indexOf(document.activeElement);if(i<0)return;if(e.key==='ArrowRight')i=(i+1)%buttons.length;else if(e.key==='ArrowLeft')i=(i+buttons.length-1)%buttons.length;else return;e.preventDefault();buttons[i].focus();show(i);});show(0);
</script></body></html>`;
 fs.writeFileSync(path.join(root,'index.html'),html);
 console.log('Rendered '+graphs.length+' SVGs, offline index.html, and editable multi-page draw.io.');
})().catch(error=>{console.error(error);process.exitCode=1;});
