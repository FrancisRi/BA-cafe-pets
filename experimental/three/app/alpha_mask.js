// Low-resolution GPU readback keeps enlarged viewports inexpensive. The Win32
// region uses native pixel coordinates; transparent pixels remain click-through.
export function alphaSpans(rgba,width,height,nativeWidth,nativeHeight,padPixels=4) {
 const coverage=new Uint8Array(width*height),pad=Math.max(1,Math.ceil(padPixels*width/nativeWidth));
 for(let y=0;y<height;y++){
  let start=-1;
  for(let x=0;x<=width;x++){
   const hit=x<width&&rgba[((height-1-y)*width+x)*4+3]>2;
   if(hit&&start<0)start=x;
   if(!hit&&start>=0){
    for(let row=Math.max(0,y-pad);row<=Math.min(height-1,y+pad);row++)
     coverage.fill(1,row*width+Math.max(0,start-pad),row*width+Math.min(width,x+pad));
    start=-1;
   }
  }
 }
 const spans=[];
 for(let y=0;y<height;y++){
  let start=-1;
  for(let x=0;x<=width;x++){
   const hit=x<width&&coverage[y*width+x];if(hit&&start<0)start=x;
   if(!hit&&start>=0){
    const left=Math.floor(start*nativeWidth/width),right=Math.min(nativeWidth,Math.ceil(x*nativeWidth/width));
    for(let row=Math.floor(y*nativeHeight/height);row<Math.floor((y+1)*nativeHeight/height);row++)spans.push(row,left,right);
    start=-1;
   }
  }
 }
 return spans;
}
