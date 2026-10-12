const compact=name=>String(name).toLowerCase().replace(/[^a-z0-9\u4e00-\u9fff]/g,'');
const has=(n,words)=>words.some(w=>n.includes(w));
const poses={Normal:['normal','default','basic','battle','combat','普通'],Stand:['stand','standing','站立'],Kneel:['kneel','crouch','squat','蹲','跪']};
const stages={Start:['start','begin','enter','准备'],End:['end','stop','exit','结束'],Delay:['delay','idle','ready','wait','等待'],Reload:['reload','reloading','换弹']};
const globalRoles={move:'Move_Ing',jump:'Move_Jump',pickup:'Formation_Pickup',skill:'Exs',death:'Vital_Death',retreat:'Vital_Retreat',victoryStart:'Victory_Start',victoryEnd:'Victory_End'};
// Numbers attached to role words identify coherent modes; never strip IDs.
export function animationIdentity(name){
 let n=compact(name).replace(/ilde/g,'idle').replace(/callsigh/g,'callsign').replace(/strat/g,'start').replace(/destory/g,'destroy');
 const family=/tank(?=normal|idle|move|reload|vital|appearance)/.test(n)?'Tank':/riding(?=attack|idle|ing|end|reload|retreat|destroy|ex)/.test(n)?'Riding':/^bst(?=attack|idle|move|death|formation)/.test(n)?'B_ST':/^st(?=attack|idle|move|death|formation)/.test(n)?'ST':'Humanoid',numbers=[];
 n=n.replace(/^(\d{1,2})(?=normal|stand|kneel|cafe|move|formation|vital|victory|exs)/,(_,id)=>{numbers.push(id.padStart(2,'0'));return '';});
 n=n.replace(/(normal|stand|kneel|attack|shoot|fire|move|exs)(\d{1,2})(?=idle|attack|ready|reload|public|callsign|move|vital|victory|end|start|delay|ing|loop|jump|$)/g,(_,key,id)=>{numbers.push(id.padStart(2,'0'));return key;});
 n=n.replace(/(idle|reload|callsign|moveing|movejump|moveendnormal|moveendstand|moveendkneel|attackstart|attackend|attackdelay|attacking|victorystart|victoryend|vitaldeath|vitalretreat)(\d{1,2})(?=random|$)/g,(_,key,id)=>{numbers.push(id.padStart(2,'0'));return key;});
 if(family==='Tank')n=n.replace(/tank/g,'').replace(/(?<!normal)reload/,'normalreload').replace(/vitaldestroy/,'vitaldeath');
 if(family==='Riding')n=n.replace(/ridingattack/g,'normalattack').replace(/ridingidle/g,'normalidle').replace(/ridingreload/g,'normalreload').replace(/ridingendnormal/g,'moveendnormal').replace(/ridinging/g,'moveing').replace(/ridingretreat/g,'vitalretreat').replace(/ridingdestroy/g,'vitaldeath').replace(/ridingex\d*/g,'exs');
 if(family==='ST'||family==='B_ST'){n=n.replace(/^(?:b)?st/,'').replace(/^idle/,'normalidle').replace(/^attack/,'normalattack').replace(/^death/,'vitaldeath');}
 return {stem:n,family,group:numbers[0]||'',variant:numbers.length>1,excluded:has(n,['cutin','dying','rootmotion','summoned'])||n.endsWith('root')||/^fx/.test(n)};
}
export function discoverAnimations(clips){
 const warnings=[],notes=[],ids=clips.map(c=>animationIdentity(c.name));
 function candidates(role,family){
  const parts=role.split('_'),pose=parts.find(p=>poses[p]),wanted=compact(role),result=[];
  clips.forEach((clip,index)=>{
   const id=ids[index],n=id.stem;if(!(clip.duration>0)||id.excluded||id.family!==family)return;
   const cafe=has(n,['cafe','cafeteria','coffee','咖啡']),attack=has(n,['attack','shoot','fire','攻击']),move=has(n,['move','walk','run','行走','移动']);
   const posture=Object.keys(poses).find(p=>has(n,poses[p]));let match=false;
   if(pose){
    if(cafe||(posture&&posture!==pose)||(!posture&&pose!=='Normal'))return;
    if(parts.includes('Attack')){
     const tail=n.slice(Math.max(n.lastIndexOf('attack'),n.lastIndexOf('shoot'),n.lastIndexOf('fire'),n.lastIndexOf('攻击'))),last=parts.at(-1);
     if(has(tail,stages.Start)&&has(tail,stages.End))return;
     match=attack&&(last==='Ing'?!has(tail,Object.values(stages).flat()):has(tail,stages[last]||[]));
    }else if(parts.includes('Reload'))match=has(n,stages.Reload);
    else if(parts.includes('Callsign'))match=has(n,['callsign','command','指挥']);
    else if(parts.includes('Move'))match=move&&has(n,stages.End);
    else match=has(n,['idle','rest','空闲'])&&!attack&&!move&&!has(n,['exs','skill','interaction','subidle','idlestart']);
   }else switch(role){
    case 'Cafe_Idle':match=has(n,['idle','rest','空闲'])&&!attack&&!move&&!posture&&!has(n,['exs','skill','interaction','subidle','idlestart']);break;
    case 'Cafe_Walk':case 'Move_Ing':match=move&&!has(n,['end','stop','start','jump','callsign','public','interaction','结束','跳'])&&!attack&&(!cafe||role==='Cafe_Walk');break;
    case 'Cafe_Reaction':match=has(n,['reaction','click','touch','反应']);break;
    case 'Formation_Pickup':match=has(n,['pickup','pick','grab','拖拽']);break;
    case 'Move_Jump':match=has(n,['jump','hop','跳'])&&!has(n,['random']);break;
    case 'Exs':match=/(?:exs|skill|技能)$/.test(n);break;
    case 'Vital_Death':match=/(?:death|die|dead|死亡)$/.test(n);break;
    case 'Vital_Retreat':match=has(n,['retreat','withdraw','退场']);break;
    case 'Victory_Start':match=has(n,['victory','win','胜利'])&&has(n,stages.Start);break;
    case 'Victory_End':match=has(n,['victory','win','胜利'])&&has(n,stages.End);break;
   }
   if(match)result.push({index,...id,score:(n===wanted?1000:n.endsWith(wanted)?800:300+(pose&&posture===pose?100:0)+(role.startsWith('Cafe')&&cafe?100:0))+(n==='formationidle'?20:0)-(n.includes('random')?40:0)-(id.variant?20:0)-(/ilde|callsigh|strat|destory/.test(compact(clip.name))?5:0)});
  });return result;
 }
 function find(role,family,group,quiet=false){
  const all=candidates(role,family);let list=all.filter(c=>c.group===group||(!c.group&&(!group||group==='01'||!all.some(a=>a.group))));
  // Choose complete standalone EX variants, not EX Idle/Spawn/Destroy stages.
  if(role==='Exs'&&!list.length&&!group){const numbered=all.filter(c=>c.group);if(numbered.length){const first=numbered.map(c=>c.group).sort()[0];list=numbered.filter(c=>c.group===first);}}
  list.sort((a,b)=>b.score-a.score||a.index-b.index);
  if(list.length>1&&list[0].score===list[1].score){
   const tied=list.filter(c=>c.score===list[0].score),reference=clips[tied[0].index];
   const equal=other=>reference.duration===other.duration&&reference.tracks?.length>0&&reference.tracks.length===other.tracks?.length&&reference.tracks.every((t,i)=>{const v=other.tracks[i];return t.name===v.name&&t.getInterpolation()===v.getInterpolation()&&['times','values'].every(key=>t[key].length===v[key].length&&t[key].every((x,j)=>x===v[key][j]));});
   if(tied.every(c=>equal(clips[c.index]))){if(!quiet)notes.push(`Identical duplicate clips for ${role}: ${reference.name}; using first identical clip`);return tied[0].index;}
   if(!quiet)warnings.push(`Ambiguous ${role}: ${tied.map(c=>clips[c.index].name).join(', ')}`);return -1;
  }
  return list[0]?.index??-1;
 }
 function posture(name,family,group,quiet){
  const p={name,idle:find(name+'_Idle',family,group,quiet),start:find(name+'_Attack_Start',family,group,quiet),delay:find(name+'_Attack_Delay',family,group,quiet),attack:find(name+'_Attack_Ing',family,group,quiet),end:find(name+'_Attack_End',family,group,quiet),reload:find(name+'_Attack_Reload',family,group,quiet),callsign:find(name+'_Callsign',family,group,quiet),moveEnd:find('Move_End_'+name,family,group,quiet)};
  if(p.reload<0)p.reload=find(name+'_Reload',family,group,quiet);return p;
 }
 const alternatives=[];
 for(const family of ['Humanoid','Tank','Riding','ST','B_ST']){
  const groups=new Set(['']);for(const id of ids)if(id.family===family&&!id.excluded&&id.group&&/(?:normal|stand|kneel|move)/.test(id.stem))groups.add(id.group);
  for(const group of [...groups].sort()){
   const available=['Normal','Stand','Kneel'].map(n=>posture(n,family,group,true)).filter(p=>p.idle>=0&&p.attack>=0);
   const explicit=available.some(p=>/normal|stand|kneel/.test(ids[p.idle].stem));
   const numberedIdle=ids.some(id=>id.family===family&&id.group&&/(normal|stand|kneel).*idle/.test(id.stem));
   if(available.length&&(group||(!numberedIdle&&(explicit||!ids.some(id=>id.family===family&&id.group&&/idle/.test(id.stem))))))alternatives.push({family,group,postures:available.map(p=>p.name)});
  }
 }
 const selected=alternatives[0]||{family:'Humanoid',group:'',postures:[]},resultPoses=[];
 for(const name of ['Normal','Stand','Kneel']){
  const p=posture(name,selected.family,selected.group,false);
  if(p.idle>=0&&p.attack>=0){if(p.delay<0)p.delay=p.idle;p.family=selected.family;p.group=selected.group;resultPoses.push(p);}
  else if(p.idle>=0||p.attack>=0)warnings.push(`Skip ${name} posture: missing ${p.idle<0?'Idle':'Attack_Ing'}`);
 }
 const battle={poses:resultPoses};for(const [key,role] of Object.entries(globalRoles))battle[key]=find(role,selected.family,selected.group);
 const cafe={idle:find('Cafe_Idle','Humanoid',selected.group),walk:find('Cafe_Walk','Humanoid',selected.group),reaction:find('Cafe_Reaction','Humanoid',selected.group),pickup:find('Formation_Pickup','Humanoid',selected.group)};
 if(cafe.idle<0){cafe.idle=resultPoses[0]?.idle??clips.findIndex(c=>c.duration>0);if(cafe.idle>=0)warnings.push(`No unambiguous cafe Idle: display fallback ${clips[cafe.idle].name}`);}
 if(cafe.walk<0)cafe.walk=battle.move;if(cafe.pickup<0)cafe.pickup=cafe.idle;
 if(alternatives.length>1||selected.group||selected.family!=='Humanoid')notes.push(`Selected coherent ${selected.family} group ${selected.group||'default'}; alternatives: ${alternatives.map(a=>a.family+':'+(a.group||'default')).join(', ')}`);
 if(battle.skill>=0&&ids[battle.skill].group)notes.push(`EX representative: ${clips[battle.skill].name}; multi-stage EX/props are not chained automatically`);
 return {cafe,battle,warnings,notes,selection:{...selected,alternatives}};
}
