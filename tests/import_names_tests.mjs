import assert from 'node:assert/strict';
import fs from 'node:fs';
import {uniqueAnimationNames,normalizeAnimationNames} from '../tools/normalize_animation_names.mjs';
const clips=['Cafe_Idle','Exs_Root','Exs_Root','Exs_Root__2','Exs_Root','中'.repeat(10),'中'.repeat(10)].map(name=>({name}));
const mapping=uniqueAnimationNames(clips);
assert.deepEqual(clips.slice(0,5).map(c=>c.name),['Cafe_Idle','Exs_Root','Exs_Root__3','Exs_Root__2','Exs_Root__4']);
assert.equal(new Set(clips.map(c=>c.name)).size,clips.length);
assert(clips.every(c=>Buffer.byteLength(c.name)<=31));
assert.equal(mapping[2].original,'Exs_Root');
assert.equal(mapping[2].index,2);
const dir=fs.mkdtempSync('artifacts/name-test-');
try {
 const doc={asset:{version:'2.0'},animations:[{name:'Idle',samplers:[]},{name:'Idle',samplers:[]}]};
 let json=Buffer.from(JSON.stringify(doc));json=Buffer.concat([json,Buffer.alloc((4-json.length%4)%4,32)]);
 const tail=Buffer.from([4,0,0,0,66,73,78,0,1,2,3,4]),header=Buffer.alloc(20);
 header.write('glTF');header.writeUInt32LE(2,4);header.writeUInt32LE(20+json.length+tail.length,8);header.writeUInt32LE(json.length,12);header.writeUInt32LE(0x4e4f534a,16);
 const original=Buffer.concat([header,json,tail]);fs.writeFileSync(dir+'/source.glb',original);
 normalizeAnimationNames(dir+'/source.glb',dir+'/output.glb');
 const output=fs.readFileSync(dir+'/output.glb');
 assert.deepEqual(output.subarray(20+output.readUInt32LE(12)),tail);
 assert.deepEqual(fs.readFileSync(dir+'/source.glb'),original);
 normalizeAnimationNames(dir+'/output.glb',dir+'/unchanged.glb');
 assert.deepEqual(fs.readFileSync(dir+'/unchanged.glb'),output);
} finally {for(const file of fs.readdirSync(dir))fs.unlinkSync(dir+'/'+file);fs.rmdirSync(dir);}
console.log('PASS: duplicate/colliding/UTF-8 names, stable mapping, source and binary preservation, unchanged-file cache stability');
