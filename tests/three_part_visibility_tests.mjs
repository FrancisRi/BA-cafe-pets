import assert from 'node:assert/strict';
import fs from 'node:fs';
import * as T from '../artifacts/three-reference/package/build/three.module.js';
import {GLTFLoader} from '../artifacts/three-reference/package/examples/jsm/loaders/GLTFLoader.js';
import {createPartVisibility,visibleInTree,visibleModelBounds} from '../experimental/three/app/part_visibility.js';
import {discoverAnimations} from '../experimental/three/app/animation_catalog.js';
import {analyzeSpatialBounds} from '../experimental/three/app/spatial_bounds.js';
import {PoseAnimator} from '../experimental/three/app/animation_blend.js';
globalThis.ProgressEvent??=class{constructor(type,options){Object.assign(this,{type},options);}};
for(const file of process.argv.slice(2)){
 const bytes=fs.readFileSync(file),n=bytes.readUInt32LE(12),doc=JSON.parse(bytes.subarray(20,20+n));doc.buffers[0].uri='data:application/octet-stream;base64,'+bytes.subarray(28+n).toString('base64');delete doc.images;delete doc.textures;delete doc.samplers;doc.materials=doc.materials.map(m=>({name:m.name}));
 const {scene:root,animations:clips}=await new GLTFLoader().parseAsync(JSON.stringify(doc),'');const parts=createPartVisibility(root,clips),catalog=discoverAnimations(clips),cafe=clips[catalog.cafe.idle];
 const nodes=[];root.traverse(node=>nodes.push(node));parts.apply(cafe);
 const optional=nodes.filter(n=>n.userData.optional);for(const n of optional)assert.equal(visibleInTree(n),false,'Cafe must hide optional props');
 for(const tag of cafe.userData?.hide||[])for(const n of nodes.filter(n=>n.userData[tag]))assert.equal(visibleInTree(n),false,'Clip-declared alternate geometry must stay hidden');
 for(const clip of clips)if(clip.userData?.show?.includes('prop0')){parts.apply(clip);for(const n of nodes.filter(n=>n.userData.prop0))assert.equal(visibleInTree(n),true,'Combat show rule must restore the shield');break;}
 parts.apply(cafe);const rest=nodes.map(node=>({node,p:node.position.clone(),q:node.quaternion.clone(),s:node.scale.clone(),weights:node.morphTargetInfluences?.slice()}));
 const reset=()=>{for(const r of rest){r.node.position.copy(r.p);r.node.quaternion.copy(r.q);r.node.scale.copy(r.s);if(r.weights)r.node.morphTargetInfluences.splice(0,r.weights.length,...r.weights);}};
 const mix=new T.AnimationMixer(root);mix.clipAction(cafe).play();mix.update(0);const initial=visibleModelBounds(T,root),fit=2.3/initial.getSize(new T.Vector3()).y,origin=new T.Vector3(-(initial.min.x+initial.max.x)/2,-initial.min.y,-(initial.min.z+initial.max.z)/2);mix.stopAllAction();reset();
 const spatial=analyzeSpatialBounds(T,root,clips,catalog,fit,origin);assert.ok(spatial.envelope.every(n=>Number.isFinite(n)&&Math.abs(n)<20),'Offstage parked weapons must not create a hundred-unit render window');
 const hair=rest.filter(r=>/hair/i.test(r.node.name));for(const index of [catalog.cafe.idle,...catalog.battle.poses.map(p=>p.idle)])for(const time of [0.25,0.8,1.5]){
  reset();const raw=new T.AnimationMixer(root);raw.clipAction(clips[index]).play();raw.setTime(time);const expected=hair.map(r=>r.node.quaternion.clone());raw.stopAllAction();raw.uncacheRoot(root);reset();
  const animator=new PoseAnimator(T,root,rest,clips);animator.start(index,false);animator.sample(time,1);for(let i=0;i<hair.length;i++)assert.ok(1-Math.abs(hair[i].node.quaternion.dot(expected[i]))<1e-6,'Hair sampling must match original Three.js cubic interpolation');animator.dispose();
 }
 console.log('PASS:',file,'optional/switch rules, finite viewport, raw hair pose parity');
}
