// Import pipeline: Node standard library + pinned Three.js + official Blender.
import fs from 'node:fs';
import {normalizeAnimationNames} from './normalize_animation_names.mjs';
import {normalizeSkinNodes} from './normalize_skin_nodes.mjs';
import {animationRoles,selectAnimations} from './select_animations.mjs';
import path from 'node:path';
import crypto from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const options={};
for(let i=2;i<process.argv.length;i+=2) options[process.argv[i]]=process.argv[i+1];
const required=name=>{if(!options[name])throw Error('Missing '+name);return path.resolve(options[name]);};
const sha=data=>crypto.createHash('sha256').update(data).digest('hex');
let folder, sourcePath;
function progress(percent,stage,detail='') {
 const file=options['--progress'];if(!file)return;
 const clean=value=>String(value).replace(/[\r\n]/g,' ');
 try {fs.writeFileSync(file,`${percent}\n${clean(stage)}\n${clean(detail)}\n`);}catch(error){console.error('Progress update unavailable: '+error.message);}
}
function run(program,args,log,timeout=600000,env=process.env){
 const result=spawnSync(program,args,{cwd:root,windowsHide:true,encoding:'utf8',timeout,maxBuffer:64*1024*1024,env});
 fs.writeFileSync(path.join(folder,log),(result.stdout||'')+(result.stderr||''));
 if(result.error || result.status!==0)throw Error(`${log}: ${result.error?.message||'process exit '+result.status}`);
 return result.stdout;
}
function glb(file,allowDuplicateNames=false){
 const bytes=fs.readFileSync(file);
 if(bytes.length<28 || bytes.toString('ascii',0,4)!=='glTF' || bytes.readUInt32LE(4)!==2 || bytes.readUInt32LE(8)!==bytes.length)throw Error('Invalid GLB 2.0 header');
 let doc,bin;
 for(let at=12;at<bytes.length;){const size=bytes.readUInt32LE(at),type=bytes.readUInt32LE(at+4);if(at+8+size>bytes.length)throw Error('Truncated GLB chunk');const chunk=bytes.subarray(at+8,at+8+size);if(type===0x4e4f534a)doc=JSON.parse(chunk);if(type===0x004e4942)bin=chunk;at+=size+8;}
 if(!doc || !bin)throw Error('Self-contained JSON + BIN GLB required');
 for(const item of [...(doc.buffers||[]),...(doc.images||[])])if(item.uri && !item.uri.startsWith('data:'))throw Error('External resources are not supported; embed them in GLB');
 if(doc.extensionsRequired?.length)throw Error('Required glTF extensions need explicit support: '+doc.extensionsRequired.join(', '));
 function floats(index){const a=doc.accessors[index],v=doc.bufferViews[a.bufferView];if(a.componentType!==5126 || a.sparse)throw Error('Unsupported animation accessor');const width={SCALAR:1,VEC2:2,VEC3:3,VEC4:4}[a.type];if(!width)throw Error('Unexpected animation accessor type');const result=[];for(let i=0;i<a.count;i++)for(let c=0;c<width;c++){const n=bin.readFloatLE((v.byteOffset||0)+(a.byteOffset||0)+i*(v.byteStride||width*4)+c*4);if(!Number.isFinite(n))throw Error('Non-finite animation data');result.push(n);}return result;}
 const clips=(doc.animations||[]).map((a,index)=>({name:a.name||'',index,duration:Math.max(...a.samplers.map(s=>Math.max(...floats(s.input))))}));
 const problems=[],seen=new Set();
 if(!clips.length)problems.push('No animations found in this GLB');
 for(const clip of clips){
  const label=`Animation #${clip.index} ${JSON.stringify(clip.name)}`;
  if(!clip.name)problems.push(`${label}: missing name`);
  if(!allowDuplicateNames && seen.has(clip.name))problems.push(`${label}: duplicate name`);
  seen.add(clip.name);
  const bytes=Buffer.byteLength(clip.name);
  if(bytes>31)problems.push(`${label}: name is ${bytes} UTF-8 bytes; raylib limit is 31`);
  if(!Number.isFinite(clip.duration)||clip.duration<=0)problems.push(`${label}: invalid duration ${clip.duration}`);
 }
 if(problems.length)throw Error('Animation precheck failed:\n'+problems.join('\n'));
 return {doc,clips,floats};
}
function validate(source,target){
 if(target.doc.skins?.length!==1)throw Error('Conversion did not produce one compatible skin');
 if(target.doc.skins[0].joints.length>256)throw Error('raylib supports at most 256 joint indices');
 const names=g=>g.clips.map(c=>c.name).sort().join('\n');if(names(source)!==names(target))throw Error('Animation names/count changed in conversion');
 for(const clip of source.clips){const other=target.clips.find(c=>c.name===clip.name);if(Math.abs(clip.duration-other.duration)>1/60+0.001)throw Error('Animation duration changed: '+clip.name);}
 for(const m of source.doc.materials||[])if(m.alphaMode==='MASK'){
  const out=target.doc.materials?.find(x=>x.name===m.name);
  if(!out || out.alphaMode!=='MASK' || Math.abs((out.alphaCutoff??.5)-(m.alphaCutoff??.5))>1e-6)throw Error('Alpha mask changed: '+m.name);
 }
 function morphNodes(g){return (g.doc.nodes||[]).filter(n=>n.mesh!==undefined && g.doc.meshes[n.mesh].primitives.some(p=>p.targets?.length));}
 function weights(g,node,clip,time){
  const mesh=g.doc.meshes[node.mesh],width=mesh.primitives.find(p=>p.targets?.length).targets.length;
  const channel=g.doc.animations[clip.index].channels.find(c=>c.target.node===g.doc.nodes.indexOf(node) && c.target.path==='weights');
  if(!channel)return node.weights||mesh.weights||Array(width).fill(0);
  const sampler=g.doc.animations[clip.index].samplers[channel.sampler],times=g.floats(sampler.input),values=g.floats(sampler.output);
  if(!['STEP','LINEAR'].includes(sampler.interpolation||'LINEAR'))throw Error('Cubic morph weights need additional support');
  let i=0;while(i+1<times.length && times[i+1]<=time)i++;const j=Math.min(i+1,times.length-1);
  const t=sampler.interpolation==='STEP'||i===j?0:Math.max(0,Math.min(1,(time-times[i])/(times[j]-times[i])));
  return Array.from({length:width},(_,k)=>values[i*width+k]*(1-t)+values[j*width+k]*t);
 }
 const nodes=morphNodes(source),outNodes=morphNodes(target);let checks=0;
 for(const node of nodes){const out=outNodes.find(n=>n.name===node.name);if(!out)throw Error('Morph mesh missing: '+node.name);
  for(const clip of source.clips){const other=target.clips.find(c=>c.name===clip.name);
   const channels=source.doc.animations[clip.index].channels.filter(c=>c.target.node===source.doc.nodes.indexOf(node)&&c.target.path==='weights');
   const boundaries=channels.flatMap(c=>source.floats(source.doc.animations[clip.index].samplers[c.sampler].input));
   for(let frame=0;frame<clip.duration*60;frame++){const time=frame/60;if(boundaries.some(t=>Math.abs(time-t)<=1/60+1e-5))continue;
    const a=weights(source,node,clip,time),b=weights(target,out,other,time);if(a.length!==b.length||a.some((v,k)=>Math.abs(v-b[k])>1e-4))throw Error(`Morph mismatch: ${clip.name}/${node.name}/${time}`);checks++;
   }
  }
 }
 return {skins:1,bones:target.doc.skins[0].joints.length,clips:target.clips,morphMeshes:outNodes.length,morphChecks:checks};
}
function behavior(g){
 const {idle,walk,click,drag}=animationRoles(g.clips.map(c=>c.name));
 let xml=fs.readFileSync(path.join(root,'design/hibiki-cafe-attention.bt.xml'),'utf8');
 const escape=s=>s.replaceAll('&','&amp;').replaceAll('"','&quot;').replaceAll('<','&lt;');
 xml=xml.replaceAll('clip="Cafe_Idle"',`clip="${escape(idle)}"`).replaceAll('clip="Formation_Pickup"',`clip="${escape(drag)}"`);
 if(click)xml=xml.replaceAll('clip="Cafe_Reaction"',`clip="${escape(click)}"`);
 else xml=xml.replace(/      <Sequence name="Click reaction">[\s\S]*?<\/Sequence>\s*/, '');
 if(walk)xml=xml.replaceAll('clip="Cafe_Walk"',`clip="${escape(walk)}"`);
 else xml=xml.replace(/      <ReturnHome[^>]*\/>\s*/,'').replace(/      <ReactiveSequence name="Only wander when user is inactive">[\s\S]*?<\/ReactiveSequence>\s*/,'');
 return {xml,roles:{idle,walk:walk||null,click:click||null,drag}};
}
try {
 progress(0,'prepare');
 sourcePath=required('--source');
 console.log('SOURCE '+sourcePath);
 const exe=required('--exe'),blender=required('--blender');
 const bytes=fs.readFileSync(sourcePath),sourceHash=sha(bytes);
 let source=glb(sourcePath,true);
 const version=spawnSync(blender,['--version'],{encoding:'utf8',windowsHide:true}).stdout?.split('\n')[0];if(!version)throw Error('Blender unavailable');
 const fingerprint=sha(Buffer.concat([bytes,...['import_model.mjs','select_animations.mjs','normalize_skin_nodes.mjs','normalize_animation_names.mjs','sample_wiki_animations.mjs','blender_roundtrip.py','compare_wiki_pose.mjs'].map(f=>fs.readFileSync(path.join(root,'tools',f))),fs.readFileSync(exe),fs.readFileSync(path.join(root,'design/hibiki-cafe-attention.bt.xml')),Buffer.from(version+process.version)]));
 const name=path.basename(sourcePath,path.extname(sourcePath)).replace(/[^\p{L}\p{N}_. -]/gu,'_');
 folder=path.join(root,'artifacts','imports',`${name}-${fingerprint.slice(0,12)}`);fs.mkdirSync(folder,{recursive:true});
 const resultPath=path.join(folder,'complete.json');
 if(fs.existsSync(resultPath)){
  const cached=JSON.parse(fs.readFileSync(resultPath));
  if(sha(fs.readFileSync(cached.model))!==cached.outputSha256 || sha(fs.readFileSync(cached.behavior))!==cached.behaviorSha256)throw Error('Cached output changed; remove complete.json to reconvert');
  progress(98,'ready','Cache');
  console.log('CACHE '+resultPath);if(options['--result'])fs.writeFileSync(options['--result'],JSON.stringify(cached,null,2));process.exit(0);
 }
 progress(5,'prepare','Source validated');
 console.log('1/5 Inspect and preserve source');
 fs.writeFileSync(path.join(folder,'original.glb'),bytes);
 const namedSource=path.join(folder,'source-named.glb'),normalizedSource=path.join(folder,'source-normalized.glb');
 const animationNames=normalizeAnimationNames(sourcePath,namedSource);
 fs.writeFileSync(path.join(folder,'animation-names.json'),JSON.stringify(animationNames,null,2));
 source=glb(namedSource);
 const selectedProfile=behavior(source);
 const animationSelection=selectAnimations(namedSource,normalizedSource,selectedProfile.xml,true);
 fs.writeFileSync(path.join(folder,'animation-selection.json'),JSON.stringify(animationSelection,null,2));
 source=glb(normalizedSource);
 console.log(`SELECTED ${source.clips.length}/${animationSelection.total}: ${source.clips.map(c=>c.name).join(', ')}`);
 progress(5,'prepare',`${source.clips.length}/${animationSelection.total} animations selected`);
 const stageKey=sha(Buffer.concat([fs.readFileSync(normalizedSource),fs.readFileSync(path.join(root,'tools/sample_wiki_animations.mjs')),fs.readFileSync(path.join(root,'tools/blender_roundtrip.py')),Buffer.from(version+'three-0.185.1')]));
 const stages=path.join(root,'artifacts/import-stages',stageKey);fs.mkdirSync(stages,{recursive:true});
 const sampled=path.join(stages,'sampled.glb'),converted=path.join(stages,'converted.glb'),stageReport=path.join(stages,'stage.json'),output=path.join(folder,name+'.glb');
 let reusable=false;
 if(fs.existsSync(stageReport)&&fs.existsSync(converted)){const record=JSON.parse(fs.readFileSync(stageReport));reusable=record.sha256===sha(fs.readFileSync(converted));}

 progress(16,'sampling',reusable?'Cache':`${source.clips.length} animations, 60 Hz`);
 console.log('2/5 Sample animation with Wiki Three.js');
 if(!reusable)run(process.execPath,[path.join(root,'tools/sample_wiki_animations.mjs'),normalizedSource,sampled],'sampling.log');
 progress(33,'converting',reusable?'Cache':`${source.clips.length} animations, Blender`);
 console.log('3/5 Convert with official Blender glTF importer/exporter');
 if(!reusable){run(blender,['--background','--factory-startup','--disable-autoexec','--python-exit-code','1','--python',path.join(root,'tools/blender_roundtrip.py'),'--','--input',sampled,'--output',converted],'blender.log');glb(converted);fs.writeFileSync(stageReport,JSON.stringify({sha256:sha(fs.readFileSync(converted))}));}
 normalizeSkinNodes(converted,output);
 progress(50,'validating');
 console.log('4/5 Validate skeleton, clips, face weights and materials');
 const target=glb(output),report=validate(source,target),profile=behavior(target);
 const tree=path.join(folder,'behavior.bt.xml');fs.writeFileSync(tree,profile.xml);
 console.log('5/5 Render every clip and compare representative poses');
 for(const clip of target.clips){const stem='clip-'+clip.index;
  progress(66+Math.floor(17*clip.index/target.clips.length),'rendering',`${clip.index+1}/${target.clips.length}  ${clip.name}`);
  run(exe,['--model',output,'--behavior',tree,'--clip',clip.name,'--sample',String(Math.floor(clip.duration*.4*60)/60),'--frames','1','--snapshot',path.join(folder,stem+'.png'),'--pose-dump',path.join(folder,stem+'.bin')],stem+'.log',60000);
 }
 const comparisons=[];
 for(const name of [profile.roles.idle,profile.roles.click,profile.roles.drag].filter((n,i,a)=>n&&a.indexOf(n)===i)){
  progress(83+Math.floor(14*comparisons.length/3),'comparing',name);
  const clip=target.clips.find(c=>c.name===name);
  const text=run(process.execPath,[path.join(root,'tools/compare_wiki_pose.mjs'),name,String(Math.floor(clip.duration*.4*60)/60),path.join(folder,'clip-'+clip.index+'.bin')],'compare-'+clip.index+'.json',120000,{...process.env,PET_COMPARE_SOURCE:normalizedSource,PET_COMPARE_MODEL:output});
  comparisons.push(JSON.parse(text));
 }
 fs.writeFileSync(path.join(folder,'comparisons.json'),JSON.stringify(comparisons,null,2));
 for(const c of comparisons)for(const row of c.results)for(const kind of ['conversion','raylib']) {
  const d=row[kind];if(!Number.isFinite(d.max)||!Number.isFinite(d.mean)||d.mean>c.referenceExtent*.05||d.max>c.referenceExtent*.25)throw Error(`Severe pose mismatch: ${c.clip}/${row.material}/${kind}; see comparisons.json`);
 }
 const warnings=['Visual acceptance required. raylib TRS/shear and unskinned attachment limitations remain.'];
 for(const comparison of comparisons)for(const row of comparison.results){
  if(row.conversion.max>0.0005)warnings.push(`${comparison.clip}/${row.material}: conversion distance ${row.conversion.max}`);
  if(row.raylib.max>0.0005)warnings.push(`${comparison.clip}/${row.material}: renderer distance ${row.raylib.max}`);
 }
 const result={status:'ready_for_visual_review',name,source:sourcePath,sourceSha256:sourceHash,animationNames,animationSelection,model:output,outputSha256:sha(fs.readFileSync(output)),behavior:tree,behaviorSha256:sha(fs.readFileSync(tree)),tools:{node:process.version,blender:version,three:'0.185.1'},...report,roles:profile.roles,comparisons,warnings};
 fs.writeFileSync(resultPath,JSON.stringify(result,null,2));
 if(options['--result'])fs.writeFileSync(options['--result'],JSON.stringify(result,null,2));
 progress(98,'ready');
 console.log('READY '+resultPath);
} catch(error){
 progress(0,'failed',error.message);
 const report={status:'failed',source:sourcePath,error:error.message,folder};
 if(folder)fs.writeFileSync(path.join(folder,'failure.json'),JSON.stringify(report,null,2));
 if(options['--result'])fs.writeFileSync(options['--result'],JSON.stringify(report,null,2));
 console.error(error.stack);process.exitCode=1;
}
