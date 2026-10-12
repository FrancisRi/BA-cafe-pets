import fs from 'node:fs';

export function animationRoles(names) {
 const find=(...choices)=>choices.find(n=>names.includes(n));
 const idle=find('Cafe_Idle','Idle')||names[0];
 return {idle,walk:find('Cafe_Walk','Walk','Move_Ing')||null,
  click:find('Cafe_Reaction','Click','Reaction')||null,drag:find('Formation_Pickup')||idle};
}

// Mirror the runtime capability rules. Only clips used by complete battle poses
// and global transitions are retained; cutins, dying loops and interactions are omitted.
export function battleAnimations(names) {
 const find=n=>names.find(actual=>actual.toLowerCase()===n.toLowerCase());
 const poses=['Normal','Stand','Kneel'].map(pose=>{
  const core=['_Idle','_Attack_Start','_Attack_Delay','_Attack_Ing','_Attack_End'].map(suffix=>find(pose+suffix));
  core.push(find('Move_End_'+pose));
  return {pose,core,extra:[find(pose+'_Attack_Reload')||find(pose+'_Reload'),find(pose+'_Callsign')].filter(Boolean)};
 }).filter(p=>p.core.every(Boolean));
 if(!poses.some(p=>p.pose==='Normal') || !find('Move_Ing'))return [];
 return [...new Set([...poses.flatMap(p=>[...p.core,...p.extra]),...['Move_Ing','Move_Jump','Formation_Pickup',
  'Exs','Vital_Death','Vital_Retreat','Victory_Start','Victory_End'].map(find).filter(Boolean)])];
}

// Only executable trees: TreeNodesModel contains port documentation, not actions.
export function referencedAnimations(xml) {
 const names=new Set();
 const decode=s=>s.replace(/&(amp|quot|lt|gt|apos);/g,(_,key)=>({amp:'&',quot:'"',lt:'<',gt:'>',apos:"'"}[key]));
 xml=xml.replace(/<!--[\s\S]*?-->/g,'');
 for(const tree of xml.matchAll(/<BehaviorTree\b[^>]*>([\s\S]*?)<\/BehaviorTree>/g))
  for(const tag of tree[1].matchAll(/<[\w:]+\b[^>]*>/g))
   for(const attr of tag[0].matchAll(/\b(clip|clips)\s*=\s*(["'])(.*?)\2/g))
    for(const name of (attr[1]==='clips'?decode(attr[3]).split(';'):[decode(attr[3])])) {
     const trimmed=name.trim();
     if(!trimmed||trimmed.includes('{'))throw Error('Import requires literal animation names in behavior tree: '+name);
     names.add(trimmed);
    }
 return [...names];
}

export function selectAnimations(input,output,xml,includeBattle=false) {
 const bytes=fs.readFileSync(input),length=bytes.readUInt32LE(12);
 const doc=JSON.parse(bytes.subarray(20,20+length));
 const available=(doc.animations||[]).map(a=>a.name);
 const battle=includeBattle?battleAnimations(available):[];
 const requested=[...new Set([...referencedAnimations(xml),...battle])];
 if(!requested.length)throw Error('Behavior tree does not reference any animations');
 for(const name of requested)if(!available.includes(name))throw Error('Behavior animation missing from model: '+name);
 const wanted=new Set(requested),kept=[],skipped=[];
 doc.animations=doc.animations.filter((a,index)=>{
  (wanted.has(a.name)?kept:skipped).push({index,name:a.name});return wanted.has(a.name);
 });
 // Keep every node, mesh, morph, texture and binary accessor intact. Only remove
 // animation definitions, so Blender never creates Actions for unused clips.
 let json=Buffer.from(JSON.stringify(doc));json=Buffer.concat([json,Buffer.alloc((4-json.length%4)%4,32)]);
 const tail=bytes.subarray(20+length),header=Buffer.from(bytes.subarray(0,20));
 header.writeUInt32LE(20+json.length+tail.length,8);header.writeUInt32LE(json.length,12);
 fs.writeFileSync(output,Buffer.concat([header,json,tail]));
 return {total:available.length,kept,skipped,battle};
}
