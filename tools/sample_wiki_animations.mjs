// Preserve Wiki's interpolation before Blender import (which drops glTF cubic tangents).
// Uses Three.js' official GLTFLoader track interpolants; never changes skins/bones.
import fs from 'node:fs';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
import {GLTFLoader} from '../artifacts/three-reference/package/examples/jsm/loaders/GLTFLoader.js';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
globalThis.ProgressEvent ??= class {constructor(type,options){Object.assign(this,{type},options);}};
const inputPath=process.argv[2] || path.join(root,'assets/Hibiki.glb');
const outputPath=process.argv[3] || path.join(root,'artifacts/Hibiki.sampled.glb');
const original=fs.readFileSync(inputPath);
const jsonSize=original.readUInt32LE(12),doc=JSON.parse(original.subarray(20,20+jsonSize));
let binary=original.subarray(28+jsonSize);
const readDoc=structuredClone(doc);
readDoc.buffers[0].uri='data:application/octet-stream;base64,'+binary.toString('base64');
delete readDoc.images;delete readDoc.textures;delete readDoc.samplers;
readDoc.materials=(readDoc.materials||[]).map(m=>({name:m.name}));
const gltf=await new GLTFLoader().parseAsync(JSON.stringify(readDoc),'');
function append(array,width){
 const data=Buffer.from(array.buffer,array.byteOffset,array.byteLength),offset=binary.length;
 binary=Buffer.concat([binary,data]);const view=doc.bufferViews.length;
 doc.bufferViews.push({buffer:0,byteOffset:offset,byteLength:data.length});
 const index=doc.accessors.length,accessor={bufferView:view,componentType:5126,count:array.length/width,type:width===1?'SCALAR':`VEC${width}`};
 if(width===1){accessor.min=[array[0]];accessor.max=[array[array.length-1]];}
 doc.accessors.push(accessor);return index;
}
let sampled=0;
for(let a=0;a<(doc.animations||[]).length;a++){
 const animation=doc.animations[a],clip=gltf.animations[a];
 for(const channel of animation.channels){
  const sampler=animation.samplers[channel.sampler],property={translation:'position',rotation:'quaternion',scale:'scale'}[channel.target.path];
  if(!property || sampler.interpolation==='STEP')continue;
  const node=await gltf.parser.getDependency('node',channel.target.node);
  const matches=clip.tracks.filter(t=>t.name===`${node.name}.${property}`);
  if(matches.length!==1)throw Error(`Ambiguous track ${animation.name}/${node.name}/${property}`);
  const track=matches[0],width=property==='quaternion'?4:3;
  const end=track.times[track.times.length-1],times=[];
  for(let frame=0;frame/60<end-1e-7;frame++)times.push(frame/60);
  times.push(end);
  const values=new Float32Array(times.length*width),interpolant=track.createInterpolant(new Float32Array(width));
  times.forEach((time,i)=>values.set(interpolant.evaluate(time),i*width));
  // A fresh sampler per channel also handles legal shared samplers safely.
  channel.sampler=animation.samplers.length;
  animation.samplers.push({input:append(new Float32Array(times),1),output:append(values,width),interpolation:'LINEAR'});
  sampled++;
 }
 // Blender resets unkeyed pose-bone properties between Actions. glTF instead
 // preserves a node's authored default, including the goggles' non-unit scale.
 // Make those defaults explicit without changing any skeleton or bind matrix.
 const jointNodes=new Set((doc.skins||[]).flatMap(skin=>skin.joints));
 for(const index of jointNodes){
  const node=await gltf.parser.getDependency('node',index);
  for(const [target,property] of Object.entries({translation:'position',rotation:'quaternion',scale:'scale'})){
   if(animation.channels.some(c=>c.target.node===index&&c.target.path===target))continue;
   const values=node[property].toArray(),sampler=animation.samplers.length;
   animation.samplers.push({input:append(new Float32Array([0,clip.duration]),1),output:append(new Float32Array([...values,...values]),values.length),interpolation:'LINEAR'});
   animation.channels.push({sampler,target:{node:index,path:target}});
  }
 }
}
doc.buffers[0].byteLength=binary.length;
let json=Buffer.from(JSON.stringify(doc));json=Buffer.concat([json,Buffer.alloc((4-json.length%4)%4,32)]);
const header=Buffer.alloc(20);header.write('glTF');header.writeUInt32LE(2,4);header.writeUInt32LE(28+json.length+binary.length,8);header.writeUInt32LE(json.length,12);header.writeUInt32LE(0x4e4f534a,16);
const binHeader=Buffer.alloc(8);binHeader.writeUInt32LE(binary.length);binHeader.writeUInt32LE(0x004e4942,4);
fs.writeFileSync(outputPath,Buffer.concat([header,json,binHeader,binary]));
console.log(`Sampled ${sampled} TRS tracks with Wiki Three.js 0.185.1 at 60 Hz; original skins and morph tracks unchanged.`);
