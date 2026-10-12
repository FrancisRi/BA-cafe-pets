// Whole displayed-pose snapshots include facial weights. Missing target tracks
// get authored defaults, preventing an old expression from leaking into a clip.
export class PoseAnimator {
 constructor(T,root,rest,clips){
  this.T=T;this.root=root;this.rest=rest;this.sources=clips;this.clips=[];
  this.mixer=new T.AnimationMixer(root);this.displayed=false;
  this.snapshot=new T.AnimationClip('displayed-pose',1,this.tracks(false));
 }
 tracks(snapshot){
  const T=this.T,tracks=[];
  for(const r of this.rest){
   const node=r.node;
   tracks.push(new T.VectorKeyframeTrack(node.uuid+'.position',[0],(snapshot?node.position:r.p).toArray()));
   tracks.push(new T.QuaternionKeyframeTrack(node.uuid+'.quaternion',[0],(snapshot?node.quaternion:r.q).toArray()));
   tracks.push(new T.VectorKeyframeTrack(node.uuid+'.scale',[0],(snapshot?node.scale:r.s).toArray()));
   const weights=snapshot?node.morphTargetInfluences:r.weights;
   if(weights?.length)tracks.push(new T.NumberKeyframeTrack(node.uuid+'.morphTargetInfluences',[0],weights.slice()));
  }
  return tracks;
 }
 complete(clip){
  const T=this.T,covered=new Set();
  const tracks=clip.tracks.map(track=>{
   const parsed=T.PropertyBinding.parseTrackName(track.name),node=T.PropertyBinding.findNode(this.root,parsed.nodeName);
   const copy=track.clone();
   if(node&&!parsed.objectName){
    copy.name=node.uuid+'.'+parsed.propertyName+(parsed.propertyIndex!==undefined?'['+parsed.propertyIndex+']':'');
    covered.add(node.uuid+'.'+parsed.propertyName);
   }
   return copy;
  });
  tracks.push(...this.tracks(false).filter(t=>!covered.has(t.name)));
  return new T.AnimationClip(clip.name,clip.duration,tracks);
 }
 start(index,blend=true){
  const T=this.T;if(!this.sources[index])return;
  const target=this.clips[index]??=this.complete(this.sources[index]);
  const blendFromDisplayed=this.displayed&&blend;
  if(blendFromDisplayed){
   let i=0;const tracks=this.snapshot.tracks;
   for(const r of this.rest){
    tracks[i++].values.set(r.node.position.toArray());tracks[i++].values.set(r.node.quaternion.toArray());tracks[i++].values.set(r.node.scale.toArray());
    if(r.weights?.length)tracks[i++].values.set(r.node.morphTargetInfluences);
   }
  }
  // Reuse the mixer, actions and canonical property bindings across transitions.
  // Stop restores authored defaults after the displayed pose has been captured.
  this.mixer.stopAllAction();this.action=this.mixer.clipAction(target).reset();
  this.action.setLoop(T.LoopOnce,1);this.action.clampWhenFinished=true;this.action.play();
  this.duration=Math.min(0.18,target.duration*0.35);
  this.frozen=blendFromDisplayed?this.mixer.clipAction(this.snapshot).reset():null;
  if(this.frozen)this.frozen.play();this.displayed=true;
  this.sample(0,0);
 }
 sample(time,transitionSeconds){
  if(!this.mixer)return;
  const t=this.frozen?Math.min(1,Math.max(0,transitionSeconds/this.duration)):1;
  const weight=t*t*(3-2*t);
  this.action.paused=false;this.action.setEffectiveWeight(weight);
  if(this.frozen)this.frozen.setEffectiveWeight(1-weight);
  this.mixer.setTime(time);
  if(this.frozen&&t===1){this.frozen.stop();this.frozen=null;}
 }
 dispose(){if(this.mixer){this.mixer.stopAllAction();this.mixer.uncacheRoot(this.root);}this.mixer=null;}
}
