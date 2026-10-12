import fs from 'node:fs';
import {discoverAnimations} from '../experimental/three/app/animation_catalog.js';
const source=JSON.parse(fs.readFileSync('artifacts/three-wiki-names.json','utf8'));
const models=source.models.map(m=>{
 if(m.error)return m;
 const catalog=discoverAnimations(m.names.map(name=>({name,duration:1}))),used=new Set();
 const scan=object=>{for(const v of Object.values(object)){if(typeof v==='number'&&v>=0)used.add(v);else if(v&&typeof v==='object')scan(v);}};
 scan(catalog.cafe);scan(catalog.battle);
 return {...m,catalog,battle:catalog.battle.poses.length>0,unmapped:m.names.filter((_,i)=>!used.has(i)),cafeIdle:m.names[catalog.cafe.idle],walk:m.names[catalog.cafe.walk]};
});
const report={source:source.source,revision:source.revision,method:source.method,scope:'Names only; duration=1 for matching. Not runtime or visual compatibility verification.',
 summary:{total:models.length,readable:models.filter(m=>!m.error).length,failed:models.filter(m=>m.error).length,battle:models.filter(m=>m.battle).length,walking:models.filter(m=>m.walk).length,warnings:models.filter(m=>m.catalog?.warnings.length).length},models};
fs.writeFileSync('artifacts/three-wiki-name-compatibility.json',JSON.stringify(report,null,2));
console.log(report.summary);
for(const m of models.filter(m=>m.catalog?.warnings.some(w=>w.startsWith('Ambiguous'))).slice(0,8))console.log(m.model,JSON.stringify(m.catalog.warnings));
const unrecognized=new Map();for(const m of models)for(const name of m.unmapped||[])unrecognized.set(name,(unrecognized.get(name)||0)+1);
console.log('COMMON UNMAPPED',JSON.stringify([...unrecognized].sort((a,b)=>b[1]-a[1]).slice(0,12)));
