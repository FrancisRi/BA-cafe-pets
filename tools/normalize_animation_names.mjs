import fs from 'node:fs';

export function uniqueAnimationNames(animations) {
 const reserved=new Set(animations.map(a=>a.name)),used=new Set();
 return animations.map((animation,index)=>{
  const original=animation.name;let name=original;
  for(let suffix=2;used.has(name);suffix++) {
   const ending=`__${suffix}`;let prefix=original;
   while(Buffer.byteLength(prefix+ending)>31)prefix=Array.from(prefix).slice(0,-1).join('');
   name=prefix+ending;
   if(reserved.has(name))name=original;
  }
  used.add(name);animation.name=name;
  return {index,original,name};
 });
}

export function normalizeAnimationNames(input,output) {
 const bytes=fs.readFileSync(input),length=bytes.readUInt32LE(12);
 const doc=JSON.parse(bytes.subarray(20,20+length));
 const names=uniqueAnimationNames(doc.animations||[]);
 if(names.every(row=>row.name===row.original)){fs.copyFileSync(input,output);return names;}
 let json=Buffer.from(JSON.stringify(doc));json=Buffer.concat([json,Buffer.alloc((4-json.length%4)%4,32)]);
 const tail=bytes.subarray(20+length),header=Buffer.from(bytes.subarray(0,20));
 header.writeUInt32LE(20+json.length+tail.length,8);header.writeUInt32LE(json.length,12);
 fs.writeFileSync(output,Buffer.concat([header,json,tail]));return names;
}
