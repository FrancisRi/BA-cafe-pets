import assert from 'node:assert/strict';
import * as T from '../artifacts/three-reference/package/build/three.module.js';
import {PoseAnimator} from '../experimental/three/app/animation_blend.js';
const root=new T.Group(),bone=new T.Bone(),face=new T.Object3D();root.add(bone,face);
bone.name='bone';face.name='face';face.morphTargetInfluences=[0,0];
const rest=[root,bone,face].map(node=>({node,p:node.position.clone(),q:node.quaternion.clone(),s:node.scale.clone(),weights:node.morphTargetInfluences?.slice()}));
const clips=[new T.AnimationClip('A',1,[new T.VectorKeyframeTrack('bone.position',[0,1],[0,0,0,0,0,0])]),
 new T.AnimationClip('B',1,[new T.VectorKeyframeTrack('bone.position',[0,1],[10,0,0,10,0,0]),new T.NumberKeyframeTrack('face.morphTargetInfluences',[0,1],[1,0.5,1,0.5]),new T.QuaternionKeyframeTrack('bone.quaternion',[0,1],[0,1,0,0,0,1,0,0])])];
const blend=new PoseAnimator(T,root,rest,clips);blend.start(0);blend.sample(0,1);
blend.start(1);blend.sample(0,0);assert.equal(bone.position.x,0);assert.equal(face.morphTargetInfluences[0],0);
blend.sample(0.09,0.09);assert.ok(Math.abs(bone.position.x-5)<1e-6);assert.equal(face.morphTargetInfluences[0],0.5);
assert.ok(Math.abs(new T.Vector3(0,0,1).applyQuaternion(bone.quaternion).z)<1e-6);
blend.start(0);blend.sample(0,0);assert.ok(Math.abs(bone.position.x-5)<1e-6);assert.equal(face.morphTargetInfluences[0],0.5);
blend.sample(0.2,0.2);assert.equal(bone.position.x,0);assert.equal(face.morphTargetInfluences[0],0);
blend.start(1,false);blend.sample(0.6,1);assert.equal(bone.position.x,10);assert.equal(face.morphTargetInfluences[0],1);
blend.dispose();console.log('PASS: Three.js pose continuity, interrupted blends, facial weights, missing-track defaults, unblended review');
