import assert from 'node:assert/strict';
import {alphaSpans} from '../experimental/three/app/alpha_mask.js';
const w=40,h=30,rgba=new Uint8Array(w*h*4);
rgba[((h-1-15)*w+20)*4+3]=255;
for(const [nw,nh] of [[40,30],[80,60],[113,87]]){
 const spans=alphaSpans(rgba,w,h,nw,nh);
 assert.ok(spans.length>0);
 let covered=false;
 for(let i=0;i<spans.length;i+=3){
  const [y,left,right]=spans.slice(i,i+3);
  assert.ok(y>=0&&y<nh&&left>=0&&right<=nw&&right>left);
  if(y===Math.floor(15*nh/h)&&left<=20*nw/w&&right>20*nw/w)covered=true;
 }
 assert.ok(covered,'Native-size alpha region must cover the model pixel');
}
assert.deepEqual(alphaSpans(new Uint8Array(w*h*4),w,h,80,60),[]);
console.log('PASS: alpha region scaling, row coverage and transparency');
