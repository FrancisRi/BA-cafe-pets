import * as T from 'three';
import {MotionClock,samplePosition} from './motion.js';
import {discoverAnimations} from './animation_catalog.js';
import {createPartVisibility,visibleModelBounds} from './part_visibility.js';
import {PoseAnimator} from './animation_blend.js';
import {analyzeFootStops} from './foot_stops.js';
import {analyzeSpatialBounds} from './spatial_bounds.js';
import {alphaSpans} from './alpha_mask.js';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
import {makeOutlines} from './outline.js';
import {makeSilhouette} from './silhouette.js';
const status=document.querySelector('#status');
const menuHint=document.querySelector('#menu-hint');
const send=value=>{window.chrome?.webview?.postMessage(value);};
const report=error=>{console.error(error);send({type:'error',message:String(error.message||error)});};
window.addEventListener('error',e=>report(e.error||e.message));
window.addEventListener('unhandledrejection',e=>report(e.reason));
const renderer=new T.WebGLRenderer({canvas:document.querySelector('#scene'),alpha:true,antialias:true});
renderer.setPixelRatio(2);renderer.setSize(480,480,false); // Fixed 2x SSAA for alpha-cutout edges.
renderer.setClearColor(0,0);renderer.outputColorSpace=T.SRGBColorSpace;
renderer.toneMapping=T.NoToneMapping;
const silhouette=makeSilhouette(renderer);let suspended=false;
const scene=new T.Scene(),camera=new T.PerspectiveCamera(3.957349,1,5,500);
let viewportWidth=480,viewportHeight=480,maskWidth=480,maskHeight=480;
const hitTarget=new T.WebGLRenderTarget(maskWidth,maskHeight,{depthBuffer:true});
let rgba=new Uint8Array(maskWidth*maskHeight*4);
let character=null,root=null,clips=[],outlines=null,mixer=null,action=null,rest=[];
let partsVisibility=null;let animator=null,blendElapsed=0,blendFrameAt=0;
let generation=-1,lastMask=0,loadToken=0,ready=false,state=null,stateAt=0,maskCount=0,maskPending=false;
const motionClock=new MotionClock();
let frames=0,nextFrame=0,renderAt=0,stateMessages=0,cpuTime=0,maskTime=0,maskSamples=0,frameGaps=[],statsAt=performance.now(),shaderError='';
renderer.debug.onShaderError=(gl,program,vs,fs)=>{
 shaderError=[gl.getProgramInfoLog(program),gl.getShaderInfoLog(vs),gl.getShaderInfoLog(fs)].filter(Boolean).join('\n');
 report(new Error('Shader: '+shaderError));
};
function restore(){
 animator?.dispose();animator=null;
 if(mixer){mixer.stopAllAction();mixer.uncacheRoot(root);}mixer=action=null;
 for(const r of rest){r.node.position.copy(r.p);r.node.quaternion.copy(r.q);r.node.scale.copy(r.s);
  if(r.weights)r.node.morphTargetInfluences.splice(0,r.weights.length,...r.weights);
 }
}
function retargetMorphTracks(clip,model){
 const tracks=[];
 for(const track of clip.tracks){
  if(!track.name.endsWith('.morphTargetInfluences')){tracks.push(track);continue;}
  const node=model.getObjectByName(track.name.slice(0,track.name.lastIndexOf('.')));
  if(!node){tracks.push(track);continue;}
  node.traverse(child=>{if(child.morphTargetInfluences&&!child.userData.isOutline){
   const copy=track.clone();copy.name=child.name+'.morphTargetInfluences';tracks.push(copy);
  }});
 }
 return new T.AnimationClip(clip.name,clip.duration,tracks);
}
function beginClip(index){
 partsVisibility?.apply(clips[index]);
 if(state?.smooth&&!state.review){
  if(!animator){restore();animator=new PoseAnimator(T,root,rest,clips.map(c=>retargetMorphTracks(c,root)));}
  animator.start(index);blendElapsed=0;blendFrameAt=performance.now();return;
 }
 restore();const clip=clips[index];if(!clip)return;
 mixer=new T.AnimationMixer(root);action=mixer.clipAction(retargetMorphTracks(clip,root));
 action.setLoop(T.LoopOnce,1);action.clampWhenFinished=true;action.play();mixer.update(0);
}
function dispose(model){
 if(!model)return;
 const geometries=new Set(),materials=new Set(),textures=new Set();
 model.traverse(o=>{if(o.isMesh){geometries.add(o.geometry);for(const m of [].concat(o.material))materials.add(m);}});
 for(const m of materials){for(const value of Object.values(m))if(value?.isTexture)textures.add(value);m.dispose();}
 for(const g of geometries)g.dispose();for(const t of textures)t.dispose();
}
export async function loadModel(url,name='Model'){
 const token=++loadToken,started=performance.now();let candidate;
 status.textContent='正在读取 '+name+'…';send({type:'progress',percent:5,detail:'读取原始 GLB'});
 try {
  const response=await fetch(url);if(!response.ok)throw Error('GLB HTTP '+response.status);
  const total=Number(response.headers.get('content-length'))||0,reader=response.body.getReader(),parts=[];let length=0;
  for(;;){const r=await reader.read();if(r.done)break;parts.push(r.value);length+=r.value.length;
   if(token!==loadToken){await reader.cancel();return;}
   send({type:'progress',percent:total?Math.min(40,5+Math.floor(length/total*35)):15,detail:`读取 GLB ${(length/1048576).toFixed(1)} MB`});
  }
  const bytes=new Uint8Array(length);let offset=0;for(const part of parts){bytes.set(part,offset);offset+=part.length;}
  const header=new DataView(bytes.buffer);
  if(length<28||header.getUint32(0,true)!==0x46546c67||header.getUint32(4,true)!==2)throw Error('需要有效的 GLB 2.0 文件');
  const source=JSON.parse(new TextDecoder().decode(bytes.subarray(20,20+header.getUint32(12,true))));
  if([...(source.buffers||[]),...(source.images||[])].some(x=>x.uri&&!x.uri.startsWith('data:')))throw Error('请使用内嵌贴图和缓冲区的 GLB');
  send({type:'progress',percent:45,detail:'Three.js 解析骨架、材质和动画'});
  const manager=new T.LoadingManager();
  manager.setURLModifier(value=>{
   const resolved=new URL(value,url);
   if(['blob:','data:'].includes(resolved.protocol)||resolved.origin===new URL(url).origin)return value;
   throw Error('模型引用了外部网络资源，请导出内嵌资源的 GLB');
  });
  const parsed=await new GLTFLoader(manager).parseAsync(bytes.buffer,url.slice(0,url.lastIndexOf('/')+1));
  candidate=parsed.scene;if(token!==loadToken){dispose(candidate);return;}
  const staticDisplay=!parsed.animations.some(c=>Number.isFinite(c.duration)&&c.duration>0);
  if(staticDisplay)parsed.animations=[new T.AnimationClip('Imported_Idle',1,[])];
  const saved=[];candidate.traverse(node=>{
   saved.push({node,p:node.position.clone(),q:node.quaternion.clone(),s:node.scale.clone(),weights:node.morphTargetInfluences?.slice()});
   if(node.isMesh){
    node.frustumCulled=false;
    node.material=[].concat(node.material).map(source=>{
     // Wiki unlit material keeps the authored textures and alpha-cutout edges.
     const material=new T.MeshBasicMaterial({map:source.map||null,color:source.color?.clone()||0xffffff,
      transparent:source.transparent,opacity:source.opacity,side:source.side,alphaTest:source.alphaTest});
     material.name=source.name;material.toneMapped=false;source.dispose();return material;
    });if(node.material.length===1)node.material=node.material[0];
   }
  });
  const catalog=discoverAnimations(parsed.animations);
  if(staticDisplay)catalog.warnings.push('Model has no usable animation: static display only');
  const nextParts=createPartVisibility(candidate,parsed.animations);
  const idleIndex=Math.max(0,catalog.cafe.idle);nextParts.apply(parsed.animations[idleIndex]);
  const initialMixer=new T.AnimationMixer(candidate),initialAction=initialMixer.clipAction(retargetMorphTracks(parsed.animations[idleIndex],candidate));
  initialAction.play();initialMixer.update(0);candidate.updateMatrixWorld(true);
  const bounds=visibleModelBounds(T,candidate),size=bounds.getSize(new T.Vector3());
  if(!Number.isFinite(size.y)||size.y<1e-6)throw Error('模型高度无效');
  const fit=2.3/size.y,origin=new T.Vector3(-(bounds.min.x+bounds.max.x)/2,-bounds.min.y,-(bounds.min.z+bounds.max.z)/2);
  let eyeSum=0,eyeCount=0;candidate.traverse(o=>{if(['Bip001 eye_L','Bip001 eye_R'].includes(o.name)){eyeSum+=o.getWorldPosition(new T.Vector3()).y;eyeCount++;}});
  const eyeHeight=eyeCount?(eyeSum/eyeCount+origin.y)*fit:1.7;
  initialMixer.stopAllAction();initialMixer.uncacheRoot(candidate);
  const stopProfiles=analyzeFootStops(T,candidate,parsed.animations,catalog,fit);
  send({type:'progress',percent:72,detail:'计算角色显示范围与屏幕边界'});
  await new Promise(resolve=>setTimeout(resolve,0));
  const spatial=analyzeSpatialBounds(T,candidate,parsed.animations,catalog,fit,origin);
  const wrapper=new T.Group(),normalizer=new T.Group(),offsetGroup=new T.Group();
  normalizer.scale.setScalar(fit);offsetGroup.position.copy(origin);offsetGroup.add(candidate);normalizer.add(offsetGroup);wrapper.add(normalizer);
  const nextOutlines=makeOutlines(candidate);nextOutlines.apply(true,3.957349);
  send({type:'progress',percent:85,detail:'准备 Wiki 材质和逐网格线条'});
  const temporary=new T.Scene();temporary.add(wrapper);shaderError='';await renderer.compileAsync(temporary,camera);
  if(shaderError)throw Error(shaderError);
  if(token!==loadToken){nextOutlines.dispose();dispose(wrapper);return;}
  restore();outlines?.dispose();if(character){scene.remove(character);dispose(character);}
  character=wrapper;root=candidate;partsVisibility=nextParts;clips=parsed.animations;rest=saved;outlines=nextOutlines;
  scene.add(character);generation=-1;ready=true;maskCount=0;status.textContent='';
  send({type:'loaded',name,eyeHeight,elapsedMs:Math.round(performance.now()-started),meshes:saved.filter(r=>r.node.isMesh).length,
   skins:source.skins?.length||0,catalog,stopProfiles,spatial,
   clips:clips.map((c,i)=>({name:c.name||'Clip_'+i,duration:c.duration,index:i})),outlineMeshes:outlines.hulls.length});
 }catch(error){if(candidate&&candidate!==root)dispose(candidate);status.textContent=ready?'':'模型加载失败：'+error.message;send({type:'loadFailed',message:error.message});}
}
function configureCamera(s){
 camera.position.fromArray(s.camera);camera.up.set(0,1,0);camera.lookAt(0,0,0);camera.fov=s.fov;camera.far=s.far;
 camera.setViewOffset(s.desktop[0],s.desktop[1],s.crop[0],s.crop[1],viewportWidth,viewportHeight);
 camera.updateProjectionMatrix();
}
function configureViewport(s){
 const [width,height]=s.viewport||[480,480];if(width===viewportWidth&&height===viewportHeight)return true;
 if(maskPending)return false;
 viewportWidth=width;viewportHeight=height;
 renderer.setPixelRatio(Math.max(1,Math.min(2,1200/Math.max(width,height))));renderer.setSize(width,height,false);
 const scale=Math.min(1,480/Math.max(width,height));maskWidth=Math.max(1,Math.round(width*scale));maskHeight=Math.max(1,Math.round(height*scale));
 hitTarget.setSize(maskWidth,maskHeight);rgba=new Uint8Array(maskWidth*maskHeight*4);lastMask=0;return true;
}
async function mask(){
 maskPending=true;const token=loadToken,started=performance.now(),sampleGeneration=state.generation,sampleRevision=state.maskRevision??0;
 const width=viewportWidth,height=viewportHeight;let generationAt=0;
 try{
  renderer.setRenderTarget(hitTarget);renderer.clear();renderer.render(scene,camera);
  const read=renderer.readRenderTargetPixelsAsync(hitTarget,0,0,maskWidth,maskHeight,rgba);renderer.setRenderTarget(null);
  await read;if(token!==loadToken||state.interacting||sampleGeneration!==state.generation||sampleRevision!==(state.maskRevision??0))return;
  generationAt=performance.now();
  const padding=state.outlineMode===1?Math.max(4,Math.ceil(1.5*(state.outlineWidth??100)/100+1)):4;
  send({type:'mask',width,height,generation:sampleGeneration,maskRevision:sampleRevision,spans:alphaSpans(rgba,maskWidth,maskHeight,width,height,padding)});maskCount++;
 }finally{
  if(generationAt)cpuTime+=performance.now()-generationAt;
  maskTime+=performance.now()-started;maskSamples++;maskPending=false;renderer.setRenderTarget(null);
 }
}
function frame(now){
 requestAnimationFrame(frame);if(!ready||!state||suspended)return;
 const interval=1000/60;if(now+0.1<nextFrame)return;
 nextFrame+=interval;if(nextFrame<now)nextFrame=now+interval;
 const cpuStart=performance.now();if(renderAt)frameGaps.push(now-renderAt);renderAt=now;
 if(!configureViewport(state))return;
 const age=motionClock.age(state.sampleTime,now,stateAt);
 configureCamera(state);character.position.fromArray(samplePosition(state,age));character.rotation.y=T.MathUtils.degToRad(state.yaw);character.scale.setScalar(state.zoom);
 if(generation!==state.generation){generation=state.generation;beginClip(state.clip);}
 if(animator){const clip=clips[state.clip],elapsed=state.paused?0:Math.min(age,0.1);
  if(!state.paused)blendElapsed+=Math.max(0,Math.min(0.1,(now-blendFrameAt)/1000));blendFrameAt=now;
  const time=state.loop?(state.time+elapsed)%clip.duration:Math.min(state.time+elapsed,clip.duration);
  animator.sample(time,blendElapsed);
 }
 if(mixer&&action){action.paused=false;const elapsed=state.review||state.paused?0:Math.min(age,0.1);
  const clip=clips[state.clip];mixer.setTime(state.loop?(state.time+elapsed)%clip.duration:Math.min(state.time+elapsed,clip.duration));}
 const mode=state.outlineMode??(state.outline?2:0),width=state.outlineWidth??100;
 outlines.apply(mode===2,camera.fov,state.desktop[1],width);scene.updateMatrixWorld(true);
 if(mode===1)silhouette.render(scene,camera,width);
 else {renderer.setRenderTarget(null);renderer.clear();renderer.render(scene,camera);}
 // HTML hint is deliberately excluded from the WebGL alpha input mask.
 menuHint.hidden=!state.menuHint;
 if(state.menuHint){const foot=character.position.clone().project(camera);
  menuHint.style.left=((foot.x+1)*viewportWidth/2)+'px';
  menuHint.style.top=((1-foot.y)*viewportHeight/2+12)+'px';}
 if(!state.interacting&&!maskPending&&now-lastMask>32){mask().catch(report);lastMask=now;}
 if(maskCount===1){send({type:'frameReady'});maskCount++;}
 cpuTime+=performance.now()-cpuStart;frames++;if(now-statsAt>3000){
  const seconds=(now-statsAt)/1000;frameGaps.sort((a,b)=>a-b);
  send({type:'stats',fps:Math.round(frames/seconds),stateHz:Math.round(stateMessages/seconds),
   frameP95:frameGaps[Math.floor(frameGaps.length*0.95)]||0,cpuMs:cpuTime/frames,maskMs:maskTime/Math.max(1,maskSamples),
   maskHz:Math.round(maskSamples/seconds),drawCalls:renderer.info.render.calls});
  frames=stateMessages=cpuTime=maskTime=maskSamples=0;frameGaps=[];statsAt=now;
 }
}
window.chrome?.webview?.addEventListener('message',event=>{
 const message=event.data;if(message.type==='load')loadModel(message.url,message.name);
 else if(message.type==='suspend')suspended=true;
 else if(message.type==='state'){
  if(state?.interacting&&!message.interacting)lastMask=0;
  suspended=false;state=message;stateAt=performance.now();motionClock.observe(state.sampleTime,stateAt);stateMessages++;
 }
});
// Browser-based automated checks can exercise the same renderer without a desktop host.
window.petPreview={loadModel,setState:s=>{state=s;stateAt=performance.now();},get root(){return root;},get clips(){return clips;},get outlines(){return outlines;},renderer};
requestAnimationFrame(frame);send({type:'boot',three:T.REVISION});
