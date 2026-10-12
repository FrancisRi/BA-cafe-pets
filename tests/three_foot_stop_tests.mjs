import assert from 'node:assert/strict';
import fs from 'node:fs';
import * as T from '../artifacts/three-reference/package/build/three.module.js';
import {GLTFLoader} from '../artifacts/three-reference/package/examples/jsm/loaders/GLTFLoader.js';
import {discoverAnimations} from '../experimental/three/app/animation_catalog.js';
import {analyzeFootStops,profileFromFeet} from '../experimental/three/app/foot_stops.js';
import {analyzeSpatialBounds,measureBodyWidth} from '../experimental/three/app/spatial_bounds.js';
globalThis.ProgressEvent??=class{constructor(type,options){Object.assign(this,{type},options);}};
assert.equal(profileFromFeet([],1),null);
const staticFeet=Array.from({length:91},()=>[[0,0,0],[0.1,0,0]]);
assert.equal(profileFromFeet(staticFeet,1.5),null,'A static pose is not evidence of an authored stopping step');
{
 const geometry=new T.BufferGeometry(),positions=[],joints=[],weights=[];
 for(let i=0;i<200;i++){
  const body=i<100;positions.push(body?(i%2?0.35:-0.35):10,1.3,0);joints.push(body?0:1,0,0,0);weights.push(1,0,0,0);
 }
 geometry.setAttribute('position',new T.Float32BufferAttribute(positions,3));geometry.setAttribute('skinIndex',new T.Uint16BufferAttribute(joints,4));geometry.setAttribute('skinWeight',new T.Float32BufferAttribute(weights,4));
 const mesh=new T.SkinnedMesh(geometry,new T.MeshBasicMaterial()),spine=new T.Bone(),hand=new T.Bone();spine.name='Spine';hand.name='Hand';mesh.add(spine,hand);mesh.bind(new T.Skeleton([spine,hand]));
 const body=measureBodyWidth(T,mesh,1,new T.Vector3());assert.ok(Math.abs(body.width-0.7)<1e-6,'Weapons extending ten units from a hand must not enlarge the body-distance unit');
 assert.equal(body.source,'body-skin');geometry.dispose();mesh.material.dispose();
}
const reports=[];
for(const file of ['assets/Azusa.glb',...process.argv.slice(2)]){
 const bytes=fs.readFileSync(file),size=bytes.readUInt32LE(12),doc=JSON.parse(bytes.subarray(20,20+size));
 doc.buffers[0].uri='data:application/octet-stream;base64,'+bytes.subarray(28+size).toString('base64');
 delete doc.images;delete doc.textures;delete doc.samplers;doc.materials=(doc.materials||[]).map(m=>({name:m.name}));
 const {scene:root,animations:clips}=await new GLTFLoader().parseAsync(JSON.stringify(doc),'');
 const catalog=discoverAnimations(clips),mixer=new T.AnimationMixer(root);
 const action=mixer.clipAction(clips[catalog.cafe.idle]);action.play();mixer.setTime(0);root.updateMatrixWorld(true);
 const idleBox=new T.Box3().setFromObject(root,true),fit=2.3/idleBox.getSize(new T.Vector3()).y;
 const origin=new T.Vector3(-(idleBox.min.x+idleBox.max.x)/2,-idleBox.min.y,-(idleBox.min.z+idleBox.max.z)/2);
 mixer.stopAllAction();mixer.uncacheRoot(root);
 const before=[];root.traverse(node=>before.push({node,p:node.position.toArray(),q:node.quaternion.toArray(),s:node.scale.toArray(),weights:node.morphTargetInfluences?.slice()}));
 const started=performance.now(),profiles=analyzeFootStops(T,root,clips,catalog,fit);
 const spatial=analyzeSpatialBounds(T,root,clips,catalog,fit,origin);
 console.log('SPATIAL',file,JSON.stringify(spatial));
 assert.ok(spatial.bodyWidth<(spatial.idle[3]-spatial.idle[0])*0.7,'Real body-distance unit must exclude the weapon/hair envelope');
 for(const r of before){assert.deepEqual(r.node.position.toArray(),r.p);assert.deepEqual(r.node.quaternion.toArray(),r.q);assert.deepEqual(r.node.scale.toArray(),r.s);assert.deepEqual(r.node.morphTargetInfluences,r.weights);}
 const envelope=new T.Box3(new T.Vector3(...spatial.envelope.slice(0,3)),new T.Vector3(...spatial.envelope.slice(3)));
 const probe=new T.AnimationMixer(root),vertex=new T.Vector3();
 const picked=[catalog.cafe.idle,catalog.cafe.walk,catalog.battle.jump,...catalog.battle.poses.flatMap(p=>[p.idle,p.moveEnd,p.attack,p.end])];
 // Probe times differ from the envelope's sampling grid, including posture
 // changes and the stopping tail; use actual skinned vertices as evidence.
 for(const index of new Set(picked.filter(i=>i>=0)))for(const fraction of [0.137,0.413,0.827]){
  probe.stopAllAction();for(const r of before){r.node.position.fromArray(r.p);r.node.quaternion.fromArray(r.q);r.node.scale.fromArray(r.s);if(r.weights)r.node.morphTargetInfluences.splice(0,r.weights.length,...r.weights);}
  const animation=probe.clipAction(clips[index]).reset();animation.setLoop(T.LoopOnce,1);animation.clampWhenFinished=true;animation.play();probe.setTime(clips[index].duration*fraction);root.updateMatrixWorld(true);
  root.traverse(mesh=>{if(!mesh.isMesh)return;const positions=mesh.geometry.getAttribute('position');for(let i=0;i<positions.count;i+=7){mesh.getVertexPosition(i,vertex).applyMatrix4(mesh.matrixWorld).add(origin).multiplyScalar(fit);assert.ok(envelope.containsPoint(vertex),`${file}: ${clips[index].name} must fit the reserved animation envelope`);}});
 }
 probe.stopAllAction();probe.uncacheRoot(root);
 for(const r of before){r.node.position.fromArray(r.p);r.node.quaternion.fromArray(r.q);r.node.scale.fromArray(r.s);if(r.weights)r.node.morphTargetInfluences.splice(0,r.weights.length,...r.weights);}
 assert.equal(profiles.length,catalog.battle.poses.filter(p=>p.moveEnd>=0).length,'Real Move_End clips should have usable support-foot samples');
 for(const p of profiles){
  const old=1.98*clips[p.clip].duration*0.75;
  assert.ok(p.stopTime<clips[p.clip].duration*0.8,'External motion must end before the long settling tail');
  assert.ok(p.distance<old*0.6,'Foot-derived stopping must remove most of the old sliding distance');
  assert.ok(Math.abs(p.speeds[0]-2.376)<1e-8,'Start at the incoming walking speed');
  assert.equal(p.speeds.at(-1),0,'Stop at the planted-foot deadline');
  for(let i=1;i<p.speeds.length;i++)assert.ok(p.speeds[i]<=p.speeds[i-1]);
  reports.push({model:file,clip:p.name,plant:p.stopTime,distance:p.distance,oldDistance:old,supportTravel:p.supportTravel});
 }
 console.log(file,`${(performance.now()-started).toFixed(1)}ms import analysis`,JSON.stringify(reports.filter(r=>r.model===file)));
}
fs.writeFileSync('artifacts/three-foot-stops.json',JSON.stringify(reports,null,2));
console.log('PASS: authored foot contacts, short stopping distance, monotonic speed, exact TRS/morph restoration');
