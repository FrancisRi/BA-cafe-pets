import {createPartVisibility,visibleInTree} from './part_visibility.js';
// A single import-time envelope. Animation never feeds back into window size,
// drag limits or ground position while the pet runs.
export function measureBodyWidth(T,root,fit,origin,height=2.3){
 const xs=[],point=new T.Vector3();root.updateMatrixWorld(true);
 root.traverse(mesh=>{
  if(!visibleInTree(mesh)||!mesh.isSkinnedMesh||/weapon|bullet|halo|backpack|prop|glasses/i.test(mesh.name))return;
  const joints=mesh.geometry.getAttribute('skinIndex'),weights=mesh.geometry.getAttribute('skinWeight');
  if(!joints||!weights)return;
  const core=mesh.skeleton.bones.map(b=>/spine|pelvis|hips|chest|(?:^|[_. :])head(?:$|[_. :])/i.test(b.name));
  for(let i=0;i<joints.count;i++){
   let influence=0;for(let j=0;j<4;j++)if(core[joints.getComponent(i,j)])influence+=weights.getComponent(i,j);
   if(influence<0.5)continue;
   mesh.getVertexPosition(i,point).applyMatrix4(mesh.matrixWorld).add(origin).multiplyScalar(fit);
   if(point.y>=height*0.35&&point.y<=height*0.9)xs.push(point.x);
  }
 });
 if(xs.length>=20){xs.sort((a,b)=>a-b);const width=xs[Math.floor((xs.length-1)*0.98)]-xs[Math.floor((xs.length-1)*0.02)];if(width>0.1)return {width,source:'body-skin'};}
 // Humanoid shoulder anchors exclude hands, hair, weapons and backpacks.
 const shoulders=[];root.traverse(b=>{if(b.isBone&&/upperarm/i.test(b.name))shoulders.push(b.getWorldPosition(new T.Vector3()).multiplyScalar(fit));});
 if(shoulders.length===2){const width=shoulders[0].distanceTo(shoulders[1]);if(width>0.1)return {width,source:'shoulder-bones'};}
 return {width:height*0.28,source:'height-fallback'};
}
export function analyzeSpatialBounds(T,root,clips,catalog,fit,origin){
 const rest=[];root.traverse(node=>rest.push({node,p:node.position.clone(),q:node.quaternion.clone(),s:node.scale.clone(),weights:node.morphTargetInfluences?.slice()}));
 const restore=()=>{for(const r of rest){r.node.position.copy(r.p);r.node.quaternion.copy(r.q);r.node.scale.copy(r.s);if(r.weights)r.node.morphTargetInfluences.splice(0,r.weights.length,...r.weights);}};
 const mixer=new T.AnimationMixer(root),point=new T.Vector3(),parts=createPartVisibility(root,clips);
 const measure=()=>{
  root.updateMatrixWorld(true);const box=new T.Box3(),seen=[],props=[];
  root.traverse(mesh=>{
   if(!mesh.isMesh||!visibleInTree(mesh))return;const positions=mesh.geometry.getAttribute('position');if(!positions)return;
   if(seen.some(o=>o.positions===positions&&o.skeleton===mesh.skeleton&&o.matrix.equals(mesh.matrixWorld)))return;
   seen.push({positions,skeleton:mesh.skeleton,matrix:mesh.matrixWorld.clone()});
   const bounds=new T.Box3();for(let i=0;i<positions.count;i++)bounds.expandByPoint(mesh.getVertexPosition(i,point).applyMatrix4(mesh.matrixWorld));
   bounds.min.add(origin).multiplyScalar(fit);bounds.max.add(origin).multiplyScalar(fit);
   if(/weapon|shield|bullet|prop|effect/i.test(mesh.name))props.push(bounds);else box.union(bounds);
  });
  // Some clips deliberately park unused weapons far outside the stage. Keep
  // their authored transform, but do not allocate an enormous desktop window.
  const core=box.clone(),reach=core.isEmpty()?Infinity:Math.max(3,core.getSize(new T.Vector3()).length()*2);
  for(const prop of props){const separation=Math.hypot(...['x','y','z'].map(axis=>Math.max(0,core.min[axis]-prop.max[axis],prop.min[axis]-core.max[axis])));
   if(core.isEmpty()||separation<=reach)box.union(prop);}
  return box;
 };
 const sample=(index,time)=>{
  mixer.stopAllAction();restore();parts.apply(clips[index]);if(!clips[index])return measure();const action=mixer.clipAction(clips[index]).reset();
  action.setLoop(T.LoopOnce,1);action.clampWhenFinished=true;action.play();mixer.setTime(time);return measure();
 };
 try{
  const idle=sample(catalog.cafe.idle,0),body=measureBodyWidth(T,root,fit,origin),bodyWidth=body.width;
  // One fixed placement footprint covers every available resting posture.
  // It never changes when an Idle clip advances or the character turns.
  const resting=new Set([catalog.cafe.idle,...(catalog.battle?.poses||[]).map(p=>p.idle)]);
  for(const index of resting)if(index>=0)for(const fraction of [0,0.25,0.5,0.75,1])idle.union(sample(index,clips[index].duration*fraction));
  const envelope=idle.clone();
  const indices=new Set();const collect=o=>{for(const value of Object.values(o)){if(typeof value==='number'&&value>=0)indices.add(value);else if(value&&typeof value==='object')collect(value);}};
  collect(catalog.cafe);collect(catalog.battle);
  for(const index of indices){const clip=clips[index],count=Math.max(2,Math.min(16,Math.ceil(clip.duration*4)));
   for(let i=0;i<=count;i++)envelope.union(sample(index,clip.duration*i/count));
  }
  // Reserve intermediate motion and outline space without changing the camera.
  envelope.expandByVector(envelope.getSize(new T.Vector3()).multiplyScalar(0.12).addScalar(0.12));
  return {idle:[...idle.min.toArray(),...idle.max.toArray()],envelope:[...envelope.min.toArray(),...envelope.max.toArray()],bodyWidth,bodyWidthSource:body.source};
 }finally{mixer.stopAllAction();mixer.uncacheRoot(root);restore();parts.restore();root.updateMatrixWorld(true);}
}
