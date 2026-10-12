import assert from 'node:assert/strict';
import fs from 'node:fs';
import {animationRoles,battleAnimations,referencedAnimations,selectAnimations} from '../tools/select_animations.mjs';

assert.deepEqual(animationRoles(['Exs','Idle','Walk','Click']),{idle:'Idle',walk:'Walk',click:'Click',drag:'Idle'});
assert.deepEqual(animationRoles(['Only']),{idle:'Only',walk:null,click:null,drag:'Only'});
assert.deepEqual(battleAnimations(['Cafe_Idle','Formation_Pickup','Exs']),[]);
const normal=['Normal_Idle','Normal_Attack_Start','Normal_Attack_Delay','Normal_Attack_Ing','Normal_Attack_End','Move_End_Normal'];
const stand=normal.map(n=>n.replaceAll('Normal','Stand').replace('Attack_Start','Attack_STart'));
const combat=[...normal,...stand,'Move_Ing','Normal_Reload','Normal_Callsign','Exs','Exs_Cutin','Public01','Vital_Death','Vital_Retreat'];
assert.deepEqual(battleAnimations(combat),[...normal,'Normal_Reload','Normal_Callsign',...stand,'Move_Ing','Exs','Vital_Death','Vital_Retreat']);
assert.deepEqual(battleAnimations(combat.filter(n=>n!=='Normal_Attack_Ing')),[]);
const xml=`<root><BehaviorTree ID="Pet"><LoopAnimation clip="Idle"/>
 <PlayRandomAnimation clips="Reaction; Wave&amp;Smile"/></BehaviorTree>
 <!-- <BehaviorTree><PlayAnimation clip="Unused"/></BehaviorTree> -->
 <TreeNodesModel><Action ID="WalkRandom"><input_port default="Walk"/></Action></TreeNodesModel></root>`;
assert.deepEqual(referencedAnimations(xml),['Idle','Reaction','Wave&Smile']);
assert.throws(()=>referencedAnimations('<BehaviorTree><LoopAnimation clip="{idle}"/></BehaviorTree>'),/literal/);
const dir=fs.mkdtempSync('artifacts/selection-test-');
try {
 const doc={asset:{version:'2.0'},nodes:[{name:'Head',translation:[1,2,3]}],skins:[{joints:[0]}],
  animations:['Exs','Idle','Reaction','Wave&Smile',...combat.filter(n=>n!=='Exs')].map(name=>({name,samplers:[{input:0,output:1}],channels:[{sampler:0,target:{node:0,path:'rotation'}}]}))};
 let json=Buffer.from(JSON.stringify(doc));json=Buffer.concat([json,Buffer.alloc((4-json.length%4)%4,32)]);
 const tail=Buffer.from([4,0,0,0,66,73,78,0,1,2,3,4]),header=Buffer.alloc(20);
 header.write('glTF');header.writeUInt32LE(2,4);header.writeUInt32LE(20+json.length+tail.length,8);header.writeUInt32LE(json.length,12);header.writeUInt32LE(0x4e4f534a,16);
 const original=Buffer.concat([header,json,tail]);fs.writeFileSync(dir+'/source.glb',original);
 const result=selectAnimations(dir+'/source.glb',dir+'/selected.glb',xml);
 assert.deepEqual(result.kept.map(a=>a.name),['Idle','Reaction','Wave&Smile']);
 assert.equal(result.skipped[0].name,'Exs');
 const selected=fs.readFileSync(dir+'/selected.glb'),length=selected.readUInt32LE(12);
 const actual=JSON.parse(selected.subarray(20,20+length));
 assert.deepEqual(actual.animations,doc.animations.slice(1,4));
 assert.deepEqual(actual.nodes,doc.nodes);assert.deepEqual(actual.skins,doc.skins);
 assert.deepEqual(selected.subarray(20+length),tail);assert.equal(selected.readUInt32LE(8),selected.length);
 assert.deepEqual(fs.readFileSync(dir+'/source.glb'),original);
 assert.throws(()=>selectAnimations(dir+'/source.glb',dir+'/bad.glb','<BehaviorTree><PlayAnimation clip="Missing"/></BehaviorTree>'),/missing/);
 assert(!fs.existsSync(dir+'/bad.glb'));
 const both=selectAnimations(dir+'/source.glb',dir+'/both.glb',xml,true);
 assert(both.kept.some(a=>a.name==='Normal_Attack_Ing'));assert(both.kept.some(a=>a.name==='Stand_Attack_STart'));
 assert(both.skipped.some(a=>a.name==='Exs_Cutin'));assert(both.skipped.some(a=>a.name==='Public01'));
 const bothBytes=fs.readFileSync(dir+'/both.glb');assert.deepEqual(bothBytes.subarray(20+bothBytes.readUInt32LE(12)),tail);
} finally {for(const file of fs.readdirSync(dir))fs.unlinkSync(dir+'/'+file);fs.rmdirSync(dir);}
console.log('PASS: role fallback, random tree references, missing clip rejection, source/binary/skeleton preservation');
