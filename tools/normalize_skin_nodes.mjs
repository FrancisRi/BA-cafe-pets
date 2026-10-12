// glTF 2.0 skins: mesh-node transforms are ignored for skinned geometry.
// raylib bakes those transforms into vertices, so detach leaf skinned nodes and
// give them identity transforms. Joints, weights and inverse bind matrices stay intact.
import fs from 'node:fs';
export function normalizeSkinNodes(input, output) {
 const bytes=fs.readFileSync(input),length=bytes.readUInt32LE(12);
 const doc=JSON.parse(bytes.subarray(20,20+length));let count=0;
 for(let index=0;index<doc.nodes.length;index++) {
  const node=doc.nodes[index];if(node.skin===undefined||node.mesh===undefined)continue;
  if(node.children?.length)throw Error('Skinned mesh nodes with children require additional transform support');
  if(doc.skins.some(s=>s.joints.includes(index)))throw Error('A mesh node is also a joint; explicit support required');
  if(doc.animations?.some(a=>a.channels.some(c=>c.target.node===index&&c.target.path!=='weights')))
   throw Error('Animated skinned mesh-node transforms require additional support');
  // Preserve scene membership while removing the transform-only ancestry.
  for(const scene of doc.scenes||[]) {
   const contains=i=>i===index||(doc.nodes[i].children||[]).some(contains);
   if(scene.nodes.some(contains)&&!scene.nodes.includes(index))scene.nodes.push(index);
  }
  for(const parent of doc.nodes)if(parent.children)parent.children=parent.children.filter(i=>i!==index);
  for(const key of ['matrix','translation','rotation','scale'])delete node[key];
  count++;
 }
 let json=Buffer.from(JSON.stringify(doc));json=Buffer.concat([json,Buffer.alloc((4-json.length%4)%4,32)]);
 const tail=bytes.subarray(20+length),header=Buffer.alloc(20);
 header.write('glTF');header.writeUInt32LE(2,4);header.writeUInt32LE(20+json.length+tail.length,8);
 header.writeUInt32LE(json.length,12);header.writeUInt32LE(0x4e4f534a,16);
 fs.writeFileSync(output,Buffer.concat([header,json,tail]));return count;
}
