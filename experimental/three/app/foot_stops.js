// Import-time analysis only. Sample authored animation on the existing nodes,
// then restore every transform. No runtime bone edits or pose-driven placement.
export function profileFromFeet(frames,duration,entrySpeed=2.376){
 if(frames.length<3||!(duration>0))return null;
 const dt=duration/(frames.length-1),feet=frames[0].length;
 if(feet!==2||frames.some(f=>f.length!==2||f.some(p=>p.some(v=>!Number.isFinite(v)))))return null;
 const floor=Math.min(...frames.flat().map(p=>p[1])),window=Math.max(2,Math.ceil(0.15/dt));
 let plant=-1;
 for(let i=1;i+window<frames.length&&plant<0;i++)for(let foot=0;foot<2;foot++){
  const at=frames[i][foot],final=frames.at(-1)[foot];
  if(at[1]>floor+0.045||Math.hypot(at[0]-final[0],at[2]-final[2])>0.055)continue;
  let travel=0,raised=false;
  for(let j=i;j<i+window;j++){
   const a=frames[j][foot],b=frames[j+1][foot];
   travel+=Math.hypot(b[0]-a[0],b[2]-a[2]);raised||=b[1]>floor+0.045;
  }
  if(!raised&&travel<=0.018){plant=i;break;}
 }
 if(plant<0)return null;
 const stopTime=plant*dt;
 if(stopTime<0.12)return null; // Static/ambiguous clip: use the short fallback.
 // Only displacement of a low support foot counts, never the swinging leg or
 // a jump between left/right foot coordinates. Forward motion already in the
 // authored clip needs no extra forward translation from the desktop host.
 let supportTravel=0;
 for(let i=0;i<plant;i++){
  const a=frames[i],b=frames[i+1];
  const foot=(a[0][1]+b[0][1])<(a[1][1]+b[1][1])?0:1;
  if((a[foot][1]+b[foot][1])*0.5<=floor+0.07)
   supportTravel+=Math.max(0,a[foot][2]-b[foot][2]);
 }
 // Smooth speed begins at the incoming speed and ends by the planted-foot
 // time. Fit its integral to measured support travel, with a short blending
 // allowance and a conservative cap. All distances are normalized model units.
 const budget=Math.min(entrySpeed*stopTime*0.5,Math.max(entrySpeed*0.06,supportTravel));
 const count=65,h=stopTime/(count-1);
 const speeds=power=>Array.from({length:count},(_,i)=>{
  const t=i/(count-1);return entrySpeed*Math.pow(Math.max(0,1-t*t*(3-2*t)),power);
 });
 const integral=values=>values.slice(1).reduce((sum,v,i)=>sum+(v+values[i])*h*0.5,0);
 let low=1,high=64;
 for(let i=0;i<24;i++){const mid=(low+high)*0.5;if(integral(speeds(mid))>budget)low=mid;else high=mid;}
 const velocity=speeds((low+high)*0.5);
 return {stopTime,speeds:velocity,distance:integral(velocity),supportTravel,source:'feet'};
}

export function analyzeFootStops(T,root,clips,catalog,fit){
 const sides={left:[],right:[]};
 root.traverse(node=>{
  if(!node.isBone)return;
  const n=node.name.toLowerCase();
  const toe=/(?:toe0|toebase|toes)$/.test(n),foot=/foot$/.test(n);
  if(!toe&&!foot)return;
  const side=/left|(?:^|[_. :])l(?:[_. :]|$)/.test(n)?'left':/right|(?:^|[_. :])r(?:[_. :]|$)/.test(n)?'right':null;
  if(side)sides[side].push({node,score:toe?2:1});
 });
 const select=side=>{const a=sides[side].sort((a,b)=>b.score-a.score);return a.length&&(!a[1]||a[0].score>a[1].score)?a[0].node:null;};
 const feet=[select('left'),select('right')],profiles=[];
 if(feet.some(f=>!f))return profiles;
 const rest=[];root.traverse(node=>rest.push({node,p:node.position.clone(),q:node.quaternion.clone(),s:node.scale.clone(),weights:node.morphTargetInfluences?.slice()}));
 const restore=()=>{for(const r of rest){r.node.position.copy(r.p);r.node.quaternion.copy(r.q);r.node.scale.copy(r.s);if(r.weights)r.node.morphTargetInfluences.splice(0,r.weights.length,...r.weights);}};
 const mixer=new T.AnimationMixer(root),vector=new T.Vector3();
 try{
  for(const index of new Set(catalog.battle.poses.map(p=>p.moveEnd).filter(i=>i>=0))){
   mixer.stopAllAction();restore();const clip=clips[index],count=Math.ceil(clip.duration*60);
   const action=mixer.clipAction(clip).reset();action.setLoop(T.LoopOnce,1);action.clampWhenFinished=true;action.play();
   const frames=[];for(let i=0;i<=count;i++){
    mixer.setTime(clip.duration*i/count);root.updateMatrixWorld(true);
    frames.push(feet.map(foot=>foot.getWorldPosition(vector).multiplyScalar(fit).toArray()));
   }
   const profile=profileFromFeet(frames,clip.duration);
   if(profile)profiles.push({clip:index,name:clip.name,feet:feet.map(f=>f.name),...profile});
  }
 }finally{mixer.stopAllAction();mixer.uncacheRoot(root);restore();root.updateMatrixWorld(true);}
 return profiles;
}
