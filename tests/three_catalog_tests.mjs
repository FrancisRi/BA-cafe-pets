import assert from 'node:assert/strict';
import fs from 'node:fs';
import {discoverAnimations} from '../experimental/three/app/animation_catalog.js';
const catalog=names=>discoverAnimations(names.map(name=>({name,duration:1})));
const renamed=catalog(['Azusa|NORMAL.attack-START','Azusa|normal_attack_ing','AZUSA::Normal Idle','Move.End.Normal','Move_Ing','Normal-Attack-Delay','Normal_Attack_End','Normal_Reload']);
assert.equal(renamed.battle.poses.length,1);assert.equal(renamed.battle.poses[0].start,0);
assert.equal(renamed.battle.poses[0].reload,7);assert.equal(renamed.battle.move,4);
const partial=catalog(['Stand Idle','Stand Attack Loop','Kneel Attack_Ing','Walking','Exs_Root','Exs_Cutin','Cafe Idle']);
assert.equal(partial.battle.poses.length,1);assert.equal(partial.battle.poses[0].name,'Stand');
assert.equal(partial.battle.poses[0].start,-1);assert.equal(partial.battle.poses[0].end,-1);
assert.equal(partial.battle.poses[0].delay,0);assert.equal(partial.battle.skill,-1);
assert.ok(partial.warnings.some(w=>w.includes('Kneel')));
assert.equal(catalog(['Cafe Idle','Cafe Walk','Exs']).battle.poses.length,0);
assert.ok(catalog(['hero_A_Skill','hero_B_Skill']).warnings.some(w=>w.includes('Ambiguous Exs')));
assert.equal(catalog(['Root|Normal_Idle','Root|Normal_Attack_Ing']).battle.poses.length,1);
assert.equal(catalog(['Cafe_Idle','Move_Callsign','Move_Ing']).cafe.walk,2,'Callsign must not be mistaken for walking');
assert.equal(catalog(['Exs_Soldier_center','Exs_Soldier_L']).battle.death,-1,'Soldier contains die but is not a death action');
const multiSkill=catalog(['Cafe_Idle','Exs01','Exs02','Exs01_Idle','Exs01_Vital_Panic']);
assert.equal(multiSkill.battle.skill,1,'Use a complete EX representative, not stages or props');
const wiki=JSON.parse(fs.readFileSync('artifacts/three-wiki-names.json','utf8'));
for(const name of ['Eimi (Battle).glb','Hoshino (Battle).glb','Hoshino (Swimsuit).glb','Kurumi.glb','Toki.glb','Shiroko＊Terror.glb']){
 const entry=wiki.models.find(m=>m.model===name),mapped=catalog(entry.names);
 assert.equal(mapped.selection.group,'01',`${name}: choose one coherent numbered group`);
 assert.ok(mapped.battle.poses.length);assert.ok(mapped.battle.move>=0);
 for(const p of mapped.battle.poses){for(const key of ['idle','start','delay','attack','end','reload','moveEnd'])if(p[key]>=0){
  const n=entry.names[p[key]];assert.ok(!/^(02_)|Normal02_|_Idle02$|_Attack02_|_Attack_\w+02$|_Reload02$|_Normal02$/.test(n),`${name}: must not borrow group 02 ${key}: ${n}`);
 }}
 console.log(name,mapped.selection.group,mapped.battle.poses.map(p=>entry.names[p.attack]));
}
for(const [name,family] of [['Cherino (Hot Spring).glb','Tank'],['Iroha.glb','Tank'],['Yuzu (Maid).glb','ST']]){
 const mapped=catalog(wiki.models.find(m=>m.model===name).names);assert.equal(mapped.selection.family,family);assert.ok(mapped.battle.poses.length);
}
const track=value=>({name:'Bone.position',times:new Float32Array([0,1]),values:new Float32Array([0,value]),getInterpolation:()=>1});
const duplicates=[{name:'Normal_Idle',duration:1,tracks:[track(1)]},{name:'Normal_Idle',duration:1,tracks:[track(1)]},{name:'Normal_Attack_Ing',duration:1,tracks:[track(2)]}];
assert.equal(discoverAnimations(duplicates).battle.poses.length,1,'Exactly identical duplicate data may safely use either copy');
duplicates[1].tracks=[track(3)];assert.equal(discoverAnimations(duplicates).battle.poses.length,0,'Distinct same-name clips must remain ambiguous');
for(const file of ['assets/Hibiki.glb','assets/Azusa.glb']){
 const bytes=fs.readFileSync(file),json=JSON.parse(bytes.subarray(20,20+bytes.readUInt32LE(12)).toString());
 const roles=catalog(json.animations.map(a=>a.name));
 assert.ok(roles.cafe.idle>=0&&roles.cafe.walk>=0);
 assert.equal(roles.battle.poses.length>0,file.includes('Azusa'));
 console.log(file,roles.battle.poses.map(p=>p.name),roles.warnings);
}
console.log('PASS: keywords, separators/prefixes, incomplete poses, ambiguous names, optional actions, raw model catalogs');
