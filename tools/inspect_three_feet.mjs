import fs from 'node:fs';
import * as T from '../artifacts/three-reference/package/build/three.module.js';
import {GLTFLoader} from '../artifacts/three-reference/package/examples/jsm/loaders/GLTFLoader.js';
import {discoverAnimations} from '../experimental/three/app/animation_catalog.js';
globalThis.ProgressEvent??=class{constructor(type,options){Object.assign(this,{type},options);}};
for(const file of process.argv.slice(2)){
 const bytes=fs.readFileSync(file),size=bytes.readUInt32LE(12),doc=JSON.parse(bytes.subarray(20,20+size));
 doc.buffers[0].uri='data:application/octet-stream;base64,'+bytes.subarray(28+size).toString('base64');
 delete doc.images;delete doc.textures;delete doc.samplers;doc.materials=(doc.materials||[]).map(m=>({name:m.name}));
 const gltf=await new GLTFLoader().parseAsync(JSON.stringify(doc),''),root=gltf.scene;
 const catalog=discoverAnimations(gltf.animations),rest=[];root.traverse(node=>rest.push({node,p:node.position.clone(),q:node.quaternion.clone(),s:node.scale.clone()}));
 const bones=[];root.traverse(node=>{if(/(?:foot|toe0)$/i.test(node.name))bones.push(node);});
 const mixer=new T.AnimationMixer(root);
 const idle=mixer.clipAction(gltf.animations[catalog.cafe.idle]);idle.play();mixer.setTime(0);root.updateMatrixWorld(true);
 const box=new T.Box3().setFromObject(root,true),fit=2.3/box.getSize(new T.Vector3()).y;
 console.log(file,'fit',fit,'bones',bones.map(n=>n.name));
 for(const p of catalog.battle.poses){
  const clip=gltf.animations[p.moveEnd];if(!clip)continue;
  mixer.stopAllAction();for(const r of rest){r.node.position.copy(r.p);r.node.quaternion.copy(r.q);r.node.scale.copy(r.s);}
  const action=mixer.clipAction(clip).reset();action.setLoop(T.LoopOnce,1);action.clampWhenFinished=true;action.play();
  const rows=[];for(let time=0;time<clip.duration+0.001;time+=0.1){mixer.setTime(Math.min(time,clip.duration));root.updateMatrixWorld(true);
   rows.push({t:Number(time.toFixed(2)),feet:bones.map(node=>node.getWorldPosition(new T.Vector3()).multiplyScalar(fit).toArray().map(v=>Number(v.toFixed(3))))});}
  console.log(clip.name,clip.duration,JSON.stringify(rows));
 }
}
