import * as T from 'three';
// Dilate the rendered character's alpha, so only its outer silhouette gains a stroke.
export function makeSilhouette(renderer){
 const target=new T.WebGLRenderTarget(1,1,{depthBuffer:true});
 const material=new T.ShaderMaterial({transparent:true,premultipliedAlpha:true,depthTest:false,depthWrite:false,
  uniforms:{image:{value:target.texture},texel:{value:new T.Vector2()},radius:{value:1}},
  vertexShader:'varying vec2 uvSample;void main(){uvSample=uv;gl_Position=vec4(position.xy,0.0,1.0);}',
  fragmentShader:`uniform sampler2D image;uniform vec2 texel;uniform float radius;varying vec2 uvSample;
  void main(){vec4 source=texture2D(image,uvSample);float alpha=source.a;
   for(int ring=1;ring<=3;ring++){for(int i=0;i<12;i++){
    float angle=float(i)*6.28318530718/12.0;
    vec2 offset=vec2(cos(angle),sin(angle))*texel*radius*float(ring)/3.0;
    alpha=max(alpha,texture2D(image,uvSample+offset).a);
   }}
   gl_FragColor=vec4(source.a>0.00001?source.rgb/source.a:vec3(0.0),1.0);
   #include <colorspace_fragment>
   gl_FragColor.rgb*=source.a;gl_FragColor.a=alpha;
  }`});
 const scene=new T.Scene(),camera=new T.Camera(),quad=new T.Mesh(new T.PlaneGeometry(2,2),material);quad.frustumCulled=false;scene.add(quad);
 const size=new T.Vector2();
 return {render(sceneSource,cameraSource,width){
  renderer.getDrawingBufferSize(size);if(target.width!==size.x||target.height!==size.y)target.setSize(size.x,size.y);
  material.uniforms.texel.value.set(1/size.x,1/size.y);material.uniforms.radius.value=1.5*width/100*renderer.getPixelRatio();
  renderer.setRenderTarget(target);renderer.clear();renderer.render(sceneSource,cameraSource);
  renderer.setRenderTarget(null);renderer.clear();renderer.render(scene,camera);
 },dispose(){target.dispose();quad.geometry.dispose();material.dispose();}};
}
