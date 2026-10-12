// Numerical reference only: same Three.js release as Wiki ModelViewer (0.185.1).
// Does not implement skinning or modify model/bone data.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {AnimationMixer, Vector3, LoopOnce, AnimationClip} from '../artifacts/three-reference/package/build/three.module.js';
import {GLTFLoader} from '../artifacts/three-reference/package/examples/jsm/loaders/GLTFLoader.js';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
globalThis.ProgressEvent ??= class { constructor(type,options){Object.assign(this,{type},options);} };
function jsonGlb(file){const b=fs.readFileSync(file),size=b.readUInt32LE(12);return {json:JSON.parse(b.subarray(20,20+size)),bin:b.subarray(28+size)};}
async function reference(file,clipName,time){
 const {json,bin}=jsonGlb(file);
 // Textures do not affect positions. Keep material names/primitive splits and
 // skip image decoding so the official loader also works in Node without DOM.
 json.buffers[0].uri='data:application/octet-stream;base64,'+bin.toString('base64');
 delete json.images;delete json.textures;delete json.samplers;
 json.materials=json.materials.map(m=>({name:m.name}));
 const gltf=await new GLTFLoader().parseAsync(JSON.stringify(json),'');
 const authored=gltf.animations.find(a=>a.name===clipName);if(!authored)throw Error('Missing clip '+clipName);
 // Wiki's retargetMorphTracks propagates a grouped primitive's weight track.
 const tracks=[];
 for(const track of authored.tracks){if(!track.name.endsWith('.morphTargetInfluences')){tracks.push(track);continue;}
  const object=gltf.scene.getObjectByName(track.name.slice(0,track.name.lastIndexOf('.')));
  if(!object){tracks.push(track);continue;}object.traverse(child=>{if(child.morphTargetInfluences){const copy=track.clone();copy.name=child.name+'.morphTargetInfluences';tracks.push(copy);}});
 }
 const mixer=new AnimationMixer(gltf.scene),action=mixer.clipAction(new AnimationClip(authored.name,authored.duration,tracks));
 action.setLoop(LoopOnce,1);action.clampWhenFinished=true;action.play();mixer.setTime(time);
 gltf.scene.updateMatrixWorld(true);
 const result={},vector=new Vector3();
 gltf.scene.traverse(mesh=>{if(!mesh.isMesh)return;if(mesh.isSkinnedMesh)mesh.skeleton.update();const list=result[mesh.material.name]??=[];
  for(let i=0;i<mesh.geometry.attributes.position.count;i++){mesh.getVertexPosition(i,vector).applyMatrix4(mesh.matrixWorld);list.push(vector.toArray());}
 });
 return result;
}
function raylibDump(file,model){const b=fs.readFileSync(file),j=jsonGlb(model).json,out={};let offset=4;for(let m=0;m<b.readInt32LE(0);m++){
 const count=b.readInt32LE(offset),material=b.readInt32LE(offset+4);offset+=8;const rows=out[j.materials[material-1].name]??=[];
 for(let v=0;v<count;v++){rows.push([b.readFloatLE(offset),b.readFloatLE(offset+4),b.readFloatLE(offset+8)]);offset+=12;}
}return out;}
function difference(a,b,sameOrder){let max=0,sum=0,worstIndex=0;for(let i=0;i<a.length;i++){let squared=Infinity;const target=sameOrder?[b[i]]:b;for(const v of target){let d=0;for(let c=0;c<3;c++)d+=(a[i][c]-v[c])**2;squared=Math.min(squared,d);}const d=Math.sqrt(squared);if(d>max){max=d;worstIndex=i;}sum+=d;}return {max,mean:sum/a.length,worstIndex};}
const clip=process.argv[2]||'Cafe_Idle',seconds=Number(process.argv[3]||1),dump=process.argv[4];
const original=await reference(process.env.PET_COMPARE_SOURCE || path.join(root,'assets/Hibiki.glb'),clip,seconds);
const convertedPath=process.env.PET_COMPARE_MODEL || path.join(root,'assets/Hibiki.blender.glb');
const converted=await reference(convertedPath,clip,seconds),runtime=dump?raylibDump(dump,convertedPath):null;
const results=[];
for(const [material,vertices] of Object.entries(converted))results.push({material,vertices:vertices.length,conversion:difference(vertices,original[material],false),...(runtime?{raylib:difference(vertices,runtime[material],true)}:{})});
const body=Object.values(original).sort((a,b)=>b.length-a.length)[0];
const referenceExtent=Math.max(...[0,1,2].map(c=>Math.max(...body.map(v=>v[c]))-Math.min(...body.map(v=>v[c]))));
console.log(JSON.stringify({clip,seconds,referenceExtent,results},null,2));
