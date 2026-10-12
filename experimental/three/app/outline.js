// Same inverted-hull method as Wiki ModelViewer (see design/WIKI_MODEL_REFERENCE.md).
// Per-mesh hulls retain intersections inside the character; texture linework is untouched.
import * as T from 'three';
const vertex=`
#include <common>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
attribute vec3 smoothNormal;
uniform float outlineWidth;
void main(){
 vec3 objectNormal=smoothNormal;
 #include <skinbase_vertex>
 #include <skinnormal_vertex>
 vec3 transformed=position;
 #include <morphtarget_vertex>
 #include <skinning_vertex>
 vec4 mvPosition=modelViewMatrix*vec4(transformed,1.0);
 mvPosition.xyz+=normalize(normalMatrix*objectNormal)*outlineWidth*max(-mvPosition.z,0.05);
 gl_Position=projectionMatrix*mvPosition;
}`;
const fragment=`uniform vec3 outlineColor;
void main(){gl_FragColor=vec4(outlineColor,1.0);
#include <colorspace_fragment>
}`;
function smoothNormals(g){
 if(g.hasAttribute('smoothNormal'))return;
 const p=g.getAttribute('position');if(!p)return;
 const keys=[],sums=new Map();
 for(let i=0;i<p.count;i++){
  const key=[p.getX(i),p.getY(i),p.getZ(i)].map(v=>Math.round(v*1e4)).join(',');
  keys.push(key);if(!sums.has(key))sums.set(key,new T.Vector3());
 }
 const index=g.getIndex(),count=index?index.count:p.count;
 const a=new T.Vector3(),b=new T.Vector3(),c=new T.Vector3();
 for(let i=0;i+2<count;i+=3){
  const ids=[i,i+1,i+2].map(v=>index?index.getX(v):v);
  a.fromBufferAttribute(p,ids[0]);b.fromBufferAttribute(p,ids[1]);c.fromBufferAttribute(p,ids[2]);
  b.sub(a).cross(c.sub(a));for(const id of ids)sums.get(keys[id]).add(b);
 }
 const values=new Float32Array(p.count*3);
 keys.forEach((key,i)=>sums.get(key).clone().normalize().toArray(values,i*3));
 g.setAttribute('smoothNormal',new T.BufferAttribute(values,3));
}
export function makeOutlines(root){
 const sources=[],hulls=[];root.traverse(o=>{if(o.isMesh&&!o.userData.isOutline)sources.push(o);});
 for(const source of sources){
  if([].concat(source.material).some(m=>m.transparent))continue;
  smoothNormals(source.geometry);if(!source.geometry.hasAttribute('smoothNormal'))continue;
  const material=new T.ShaderMaterial({vertexShader:vertex,fragmentShader:fragment,side:T.BackSide,
   uniforms:{outlineWidth:{value:0.002},outlineColor:{value:new T.Color('#000000')}}});
  const hull=source.isSkinnedMesh?new T.SkinnedMesh(source.geometry,material):new T.Mesh(source.geometry,material);
  if(source.isSkinnedMesh)hull.bind(source.skeleton,source.bindMatrix);
  hull.name=source.name+'__outline';hull.userData.isOutline=true;hull.frustumCulled=false;hull.renderOrder=-1;
  hull.morphTargetInfluences=source.morphTargetInfluences;hull.morphTargetDictionary=source.morphTargetDictionary;
  source.add(hull);hulls.push(hull);
 }
 return {hulls,apply(enabled,fov,viewHeight=480,width=100){
  // The camera crops a full desktop into this 480px window. Compensate both
  // its long lens and full projection height to match Wiki's 480px/35deg view.
  const scale=Math.tan(T.MathUtils.degToRad(fov/2))/Math.tan(T.MathUtils.degToRad(35/2))*480/viewHeight;
  for(const h of hulls){h.visible=enabled;h.material.uniforms.outlineWidth.value=0.002*scale*width/100;}
 },dispose(){for(const h of hulls){h.removeFromParent();h.material.dispose();}}};
}
