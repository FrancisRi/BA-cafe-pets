// GLB-authored part rules, matching Wiki's optional baseline and clip show/hide.
const key=value=>String(value).replace(/\s/g,'_').replace(/[\[\].:/]/g,'').toLowerCase();
const names=value=>Array.isArray(value)?value.filter(v=>typeof v==='string'):typeof value==='string'?value.split(',').map(v=>v.trim()).filter(Boolean):[];
export function createPartVisibility(root,clips){
 const tags=new Set(['optional']);for(const clip of clips)for(const tag of [...names(clip.userData?.show),...names(clip.userData?.hide)])tags.add(tag.toLowerCase());
 const targets=new Map(),original=new Map();
 for(const tag of tags){const byName=[],byTag=[];root.traverse(node=>{
  if(node.userData.isOutline)return;if(key(node.name)===key(tag))byName.push(node);
  else if(Object.entries(node.userData).some(([name,value])=>value&&name.toLowerCase()===tag))byTag.push(node);
 });const nodes=new Set();for(const node of byName.length?byName:byTag){nodes.add(node);node.traverse(child=>{if(child.isMesh&&!child.userData.isOutline)nodes.add(child);});}
 targets.set(tag,[...nodes]);for(const node of nodes)if(!original.has(node))original.set(node,node.visible);
 }
 const set=(list,visible)=>{for(const tag of list)for(const node of targets.get(tag.toLowerCase())||[])node.visible=visible;};
 return {apply(clip){for(const [node,visible] of original)node.visible=visible;set(['optional'],false);set(names(clip?.userData?.hide),false);set(names(clip?.userData?.show),true);},restore(){for(const [node,visible] of original)node.visible=visible;}};
}
export function visibleInTree(node){for(let current=node;current;current=current.parent)if(!current.visible)return false;return true;}

export function visibleModelBounds(T,root){
 root.updateMatrixWorld(true);const box=new T.Box3(),point=new T.Vector3();
 root.traverse(mesh=>{if(!mesh.isMesh||!visibleInTree(mesh))return;const p=mesh.geometry.getAttribute('position');if(!p)return;
  for(let i=0;i<p.count;i++)box.expandByPoint(mesh.getVertexPosition(i,point).applyMatrix4(mesh.matrixWorld));});return box;
}
