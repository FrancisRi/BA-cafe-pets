"""Compare exported/runtime facial weights against the untouched source GLB.

python tools/verify_faces.py [--exe build/desktop_pet.exe]
No third-party Python packages required. Visual acceptance remains manual.
"""
import argparse
import bisect
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


class Glb:
    def __init__(self, path):
        blob = Path(path).read_bytes()
        magic, version, size = struct.unpack_from('<4sII', blob)
        assert magic == b'glTF' and version == 2 and size == len(blob)
        offset = 12
        while offset < size:
            length, kind = struct.unpack_from('<II', blob, offset)
            chunk = blob[offset + 8:offset + 8 + length]
            if kind == 0x4E4F534A:
                self.doc = json.loads(chunk)
            elif kind == 0x004E4942:
                self.binary = chunk
            offset += 8 + length

    def scalar(self, index):
        a = self.doc['accessors'][index]
        assert a['componentType'] == 5126 and a['type'] == 'SCALAR' and 'sparse' not in a
        view = self.doc['bufferViews'][a['bufferView']]
        offset = view.get('byteOffset', 0) + a.get('byteOffset', 0)
        return [struct.unpack_from('<f', self.binary, offset + i*view.get('byteStride', 4))[0]
                for i in range(a['count'])]

    def tracks(self):
        result = {}
        for clip in self.doc['animations']:
            tracks = {}
            for channel in clip['channels']:
                if channel['target']['path'] != 'weights':
                    continue
                node = self.doc['nodes'][channel['target']['node']]
                sampler = clip['samplers'][channel['sampler']]
                times, values = self.scalar(sampler['input']), self.scalar(sampler['output'])
                assert len(times) == len(values), 'Hibiki has one target per mouth mesh'
                tracks[node['name']] = (times, values, sampler.get('interpolation', 'LINEAR'))
            result[clip['name']] = tracks
        return result


def sample(track, seconds):
    times, values, mode = track
    left = max(0, bisect.bisect_right(times, seconds) - 1)
    right = min(left + 1, len(times) - 1)
    if mode == 'STEP' or right == left:
        return values[left]
    assert mode == 'LINEAR'
    blend = max(0, min(1, (seconds - times[left])/(times[right] - times[left])))
    return values[left] + (values[right] - values[left])*blend


def verify(executable=None):
    original = Glb(ROOT/'assets/Hibiki.glb')
    converted = Glb(ROOT/'assets/Hibiki.blender.glb')
    assert len(converted.doc['skins']) == 1
    assert len(converted.doc['skins'][0]['joints']) == 128
    source, target = original.tracks(), converted.tracks()
    assert source.keys() == target.keys() and len(source) == 7
    for material in original.doc['materials']:
        if material.get('alphaMode') != 'MASK':
            continue
        match = next(m for m in converted.doc['materials'] if m['name'] == material['name'])
        assert match.get('alphaMode') == 'MASK'
        assert match.get('alphaCutoff', 0.5) == material.get('alphaCutoff', 0.5)
    checks = 0
    for clip, tracks in source.items():
        assert tracks.keys() == target[clip].keys() and len(tracks) == 3
        for name, track in tracks.items():
            duration = target[clip][name][0][-1]
            for tick in range(int(duration*60)):
                seconds = tick/60
                # Blender bakes to 60 Hz with LINEAR interpolation. Ignore only
                # one frame around authored STEP transitions (not entire poses).
                if any(abs(seconds - t) <= 1/60 + 1e-5 for t in track[0][1:]):
                    continue
                assert abs(sample(track, seconds) - sample(target[clip][name], seconds)) < 1e-5, (clip, name, seconds)
                checks += 1
    print(f'PASS: one 128-bone skin, 7 clips, MASK materials, {checks} source/export weight comparisons')
    if not executable:
        return
    executable = Path(executable).resolve()
    assert (executable.parent/'assets/Hibiki.blender.glb').read_bytes() == (ROOT/'assets/Hibiki.blender.glb').read_bytes()
    output = ROOT/'artifacts/faces'
    output.mkdir(parents=True, exist_ok=True)
    for clip, tracks in source.items():
        times = [0.1, 1, 3, 4.8, 7, 10] if clip == 'Formation_Pickup' else [0.1]
        for seconds in times:
            stem = output/f'{clip}-{seconds}'
            snapshot = stem.with_name(stem.name + '.png')
            run = subprocess.run([str(executable), '--clip', clip, '--sample', str(seconds),
                                  '--frames', '1', '--snapshot', str(snapshot)],
                                 cwd=ROOT, capture_output=True, text=True, timeout=30,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
            stem.with_name(stem.name + '.log').write_text(run.stdout + run.stderr, encoding='utf-8')
            assert run.returncode == 0 and 'FACE: loaded 3 morph meshes' in run.stdout, stem
            values = dict(re.findall(r'FACE SAMPLE: (\S+) target=0 weight=([\d.-]+)', run.stdout))
            assert values.keys() == tracks.keys(), stem
            for name, track in tracks.items():
                assert abs(float(values[name]) - sample(track, seconds)) < 1e-5, (clip, seconds, name)
        print(f'PASS: runtime {clip} ({len(times)} samples)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path)
    verify(parser.parse_args().exe)
