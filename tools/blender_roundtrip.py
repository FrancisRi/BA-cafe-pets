"""Normalize the original GLB using Blender's official glTF importer/exporter.
Run: blender --background --factory-startup --python-exit-code 1 --python tools/blender_roundtrip.py
No bone remapping, matrix rewriting, or skinning implementation here.
"""
from pathlib import Path
import runpy
import argparse
import sys
import bpy
import json
import struct

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--input', type=Path, default=root / 'artifacts/Hibiki.sampled.glb')
parser.add_argument('--blend', type=Path)
parser.add_argument('--output', type=Path, default=root / 'assets/Hibiki.blender.glb')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
(root / "artifacts").mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.render.fps = 60
# sample_wiki_animations.mjs must run first: Blender currently discards cubic
# tangents on glTF import. Use the same interpolants as Wiki before adapting bones.
sampled = args.input
if not sampled.exists():
    raise RuntimeError("Run tools/sample_wiki_animations.mjs before Blender conversion")
# Declare the existing common skeleton root. Blender otherwise imports an isolated
# bullet skin as a separate armature (Azusa). No joint indices, weights, inverse
# bind matrices, hierarchy or transforms are modified; Blender handles the merge.
blob = sampled.read_bytes()
length = struct.unpack_from('<I', blob, 12)[0]
doc = json.loads(blob[20:20 + length])
if len(doc.get('skins', [])) > 1:
    parents = {child: i for i, node in enumerate(doc['nodes']) for child in node.get('children', [])}
    def ancestors(index):
        result = [index]
        while index in parents:
            index = parents[index]
            result.append(index)
        return result
    joints = [j for skin in doc['skins'] for j in skin['joints']]
    common = set(ancestors(joints[0]))
    for joint in joints[1:]:
        common.intersection_update(ancestors(joint))
    if not common:
        raise RuntimeError('Disconnected skeleton roots require explicit support')
    skeleton = next(i for i in ancestors(joints[0]) if i in common)
    for skin in doc['skins']:
        skin['skeleton'] = skeleton
    data = json.dumps(doc, separators=(',', ':')).encode('utf-8')
    data += b' ' * (-len(data) % 4)
    tail = blob[20 + length:]
    rooted = sampled.with_name('rooted.glb')
    rooted.write_bytes(struct.pack('<4sIIII', b'glTF', 2, 20 + len(data) + len(tail), len(data), 0x4E4F534A) + data + tail)
    sampled = rooted
    print('COMMON SKELETON ROOT', skeleton, doc['nodes'][skeleton].get('name'))
bpy.ops.import_scene.gltf(filepath=str(sampled),
                          bone_heuristic="BLENDER", guess_original_bind_pose=False)
for obj in bpy.context.scene.objects:
    print("IMPORTED", obj.name, obj.type,
          "bones=" + str(len(obj.data.bones)) if obj.type == "ARMATURE" else "")
for action in bpy.data.actions:
    print("ACTION", action.name, list(action.frame_range))
if args.blend:
    bpy.ops.wm.save_as_mainfile(filepath=str(args.blend))
bpy.ops.export_scene.gltf(filepath=str(args.output),
                          export_format="GLB", export_animations=True,
                          export_animation_mode="ACTIONS", export_merge_animation="ACTION",
                          export_force_sampling=True,
                          export_frame_step=1, export_def_bones=False,
                          export_morph=True, export_morph_animation=True)
# Check against original authored weights: NLA export can silently bake the
# active shape-key action into every clip. ACTIONS preserves each action slot.
if args.output.resolve() == (root / 'assets/Hibiki.blender.glb').resolve():
    runpy.run_path(str(root / "tools/verify_faces.py"))["verify"]()
