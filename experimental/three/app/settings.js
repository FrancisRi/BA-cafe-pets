const form=document.querySelector('form'),saved=document.querySelector('#saved');
const defaults={walking:true,look:true,outlineMode:2,outlineWidth:100,inputGrace:3,cafeTracking:true,cafeLookMode:'smooth',follow:false,followNear:3,followFar:5,idleSeconds:60,activeSeconds:30,moveInterval:8,moveBodies:3,skillProbability:5,clickProbability:10,keyboardIdle:10,walkTimeJitter:10,walkDistanceJitter:15};
const send=value=>window.chrome?.webview?.postMessage(value);
function collect(){const value={inputGrace:3};for(const input of form.elements){if(!input.name)continue;value[input.name]=input.type==='checkbox'?input.checked:['number','range'].includes(input.type)?input.valueAsNumber:input.name==='outlineMode'?Number(input.value):input.value;}return value;}
function apply(){const near=form.elements.followNear,far=form.elements.followFar;far.setCustomValidity(near.valueAsNumber>far.valueAsNumber?'最远距离不能小于最近距离':'');if(!form.reportValidity())return;saved.classList.remove('error');saved.textContent='正在保存…';send({type:'save',value:collect()});if(!window.chrome?.webview)saved.textContent='预览：修改尚未写入桌宠';}
function render(message){
 document.querySelector('#name').textContent=message.name||'角色设置';
 for(const [name,value] of Object.entries(message.value||{})){const input=form.elements.namedItem(name);if(!input||document.activeElement===input)continue;if(input.type==='checkbox')input.checked=value;else input.value=value;}
 document.querySelector('#outline-value').textContent=form.elements.outlineWidth.value+'%';
 document.querySelector('#battle-option').disabled=!message.battleAvailable;
 form.elements.mode.disabled=message.busy;
 document.querySelector('#capability').textContent=message.battleAvailable?'':'此模型没有可用战斗动作，使用咖啡厅模式。';
 form.elements.follow.disabled=!message.canFollow;
 document.querySelector('#follow-capability').textContent=message.canFollow?'':'此模型没有行走动作，鼠标跟随不可用。';
 document.querySelector('#import').disabled=message.busy||(message.characters?.length>=message.historyMaximum);
 document.querySelector('#import').title=message.characters?.length>=message.historyMaximum?'历史列表已满，请先删除角色':'';
 document.querySelector('#uniform-size').disabled=!message.activeCount;
 form.elements.cafeLookMode.disabled=!form.elements.cafeTracking.checked;
 form.elements.outlineWidth.disabled=Number(form.elements.outlineMode.value)===0;
 for(const name of ['followNear','followFar'])form.elements[name].disabled=!message.canFollow||!form.elements.follow.checked;
 for(const control of form.querySelectorAll('.row input,.row select'))control.closest('.row').classList.toggle('unavailable',control.disabled);
 document.querySelector('#progress').hidden=!(message.busy||message.detail);
 document.querySelector('#progress-detail').textContent=message.detail||'';document.querySelector('#percent').textContent=(message.percent||0)+'%';document.querySelector('progress').value=message.percent||0;
 saved.classList.toggle('error',!!message.error);saved.textContent=message.error||'已保存 · 打开菜单时暂停行为';
 if(message.characters){const container=document.querySelector('#characters');container.replaceChildren();
  document.querySelector('#character-count').textContent=`${message.characters.length} / ${message.historyMaximum||15} · 显示 ${message.activeCount} / ${message.maximum}`;
  for(const actor of message.characters){const row=document.createElement('div');row.className='character';row.classList.toggle('current',actor.id===message.selectedId);row.title=actor.path;
   row.classList.toggle('unavailable',actor.available===false||!actor.active);
   const toggle=document.createElement('input');toggle.type='checkbox';toggle.checked=actor.active;toggle.setAttribute('aria-label','显示 '+actor.name);toggle.disabled=!actor.active&&(actor.available===false||message.activeCount>=message.maximum);
   toggle.onchange=()=>send({type:'roster_toggle',id:actor.id,active:toggle.checked});
   const choose=document.createElement('input');choose.type='radio';choose.checked=actor.id===message.selectedId;choose.disabled=!actor.active||actor.loading||actor.failed||actor.available===false;choose.setAttribute('aria-label','选择 '+actor.name);choose.onchange=()=>send({type:'roster_select',id:actor.id});
   const label=document.createElement('span');label.textContent=actor.name;const detail=document.createElement('small');detail.textContent=actor.available===false?'文件不存在':actor.failed?'加载失败':actor.loading?'加载中…':!actor.active?'未显示':actor.mode==='battle'?'战斗模式':'咖啡厅模式';
   const remove=document.createElement('button');remove.type='button';remove.className='remove-character';remove.textContent='删除';remove.setAttribute('aria-label','删除 '+actor.name);
   remove.onclick=()=>{if(actor.active&&message.activeCount===1){send({type:'roster_remove',id:actor.id});return;}removeId=actor.id;document.querySelector('#remove-description').textContent='从历史列表移除 '+actor.name+'。'+(actor.active?'该角色会关闭。':'')+'GLB 文件保留。';removeDialog.showModal();};
   label.append(detail);label.onclick=()=>{if(!choose.disabled)send({type:'roster_select',id:actor.id});};row.append(toggle,label,choose);
   if(actor.failed&&actor.active&&actor.available!==false){const retry=document.createElement('button');retry.type='button';retry.textContent='重试';retry.onclick=()=>send({type:'roster_retry',id:actor.id});row.append(retry);}
   row.append(remove);container.append(row);
  }
 }
}
form.addEventListener('submit',e=>{e.preventDefault();apply();});form.addEventListener('change',e=>{if(e.target.name)apply();});
form.elements.outlineWidth.oninput=()=>{document.querySelector('#outline-value').textContent=form.elements.outlineWidth.value+'%';};
let removeId=null;const removeDialog=document.querySelector('#remove-dialog');document.querySelector('#remove-cancel').onclick=()=>removeDialog.close();document.querySelector('#remove-confirm').onclick=()=>{send({type:'roster_remove',id:removeId});removeDialog.close();};
document.querySelector('#uniform-size').onclick=()=>send({type:'uniform_size'});
const help=document.querySelector('#help-dialog');document.querySelector('#help').onclick=()=>help.showModal();document.querySelector('#help-close').onclick=()=>help.close();
document.querySelector('#close-character').onclick=()=>send({type:'close_character'});
document.querySelector('#close').onclick=()=>send({type:'close'});
document.querySelector('#exit').onclick=()=>send({type:'exit'});
document.querySelector('header').addEventListener('pointerdown',e=>{if(e.button===0&&!e.target.closest('button')){send({type:'drag'});e.preventDefault();}});
document.querySelector('#import').onclick=()=>send({type:'import'});
document.querySelector('#reset').onclick=()=>{render({name:document.querySelector('#name').textContent,value:{...defaults,mode:form.elements.mode.value},battleAvailable:!document.querySelector('#battle-option').disabled,canFollow:!form.elements.follow.disabled});apply();};
document.addEventListener('keydown',e=>{if(e.key==='Escape'){if(removeDialog.open){removeDialog.close();e.preventDefault();return;}if(help.open){help.close();e.preventDefault();return;}send({type:'close'});e.preventDefault();}});
for(const button of document.querySelectorAll('nav button'))button.onclick=()=>{for(const b of document.querySelectorAll('nav button'))b.classList.toggle('selected',b===button);for(const section of document.querySelectorAll('[data-section]'))section.hidden=section.dataset.section!==button.dataset.page;document.querySelector('main').scrollTop=0;};
window.chrome?.webview?.addEventListener('message',event=>{if(event.data.type==='settings')render(event.data);});
if(!window.chrome?.webview)render({name:'Azusa',value:{...defaults,mode:'battle'},battleAvailable:true,canFollow:true});
send({type:'ready'});
