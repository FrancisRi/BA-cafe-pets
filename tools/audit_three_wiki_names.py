"""Read only HTTP range metadata; never fetch/save/import a GLB body.

The cached Wiki page must match the live page's model URL digest. The report
contains names only. Keyframe buffers, meshes and textures are not requested.
"""
import concurrent.futures
import json
import re
import struct
import time
import urllib.parse
import urllib.request
from html.parser import HTMLParser
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
groups=[]
class Page(HTMLParser):
    def handle_starttag(self,tag,attrs):
        attributes=dict(attrs)
        if 'data-models' in attributes:groups.extend(json.loads(attributes['data-models']))

Page().feed((ROOT/'artifacts/references/models.html').read_text(encoding='utf-8'))
urls=sorted(m['src'] for m in groups)
digest=2166136261
for c in '\n'.join(urls):digest=((digest^ord(c))*16777619)&0xffffffff
assert len(urls)==295 and digest==1491436931, 'Cached catalog does not match live Wiki observation'

def read_range(url,start,count):
    request=urllib.request.Request(url,headers={'Range':f'bytes={start}-{start+count-1}'})
    with urllib.request.urlopen(request,timeout=25) as response:
        value=response.headers.get('Content-Range','')
        match=re.fullmatch(r'bytes (\d+)-(\d+)/(\d+)',value)
        # Check headers before reading: a server ignoring Range must not cause
        # an accidental full model download.
        if response.status!=206 or not match or int(match[1])!=start or int(match[2])!=start+count-1:
            raise ValueError(f'Server did not honor the exact metadata range: {response.status}, {value}')
        data=response.read(count)
        if len(data)!=count:raise ValueError('Truncated metadata range')
        return data

def inspect(model):
    source=model['src'];revision,path=source.split('@',1)[1].split('/',1)
    file=urllib.parse.unquote(path)
    url=f'https://raw.githubusercontent.com/lihaohong6/BlueArchiveModels/{revision}/{urllib.parse.quote(file)}'
    result={'model':file,'source':source}
    try:
        magic,version,total,length,kind=struct.unpack('<5I',read_range(url,0,20))
        if magic!=0x46546c67 or version!=2 or kind!=0x4e4f534a:
            raise ValueError('Invalid GLB metadata header')
        if not 0<length<=16_000_000:raise ValueError(f'JSON metadata exceeds 16MB limit: {length}')
        document=json.loads(read_range(url,20,length))
        result.update(names=[a.get('name','') for a in document.get('animations',[])],metadataBytes=20+length,modelBytes=total)
    except Exception as error:result['error']=str(error)
    return result

output=ROOT/'artifacts/three-wiki-names.json'
previous=json.loads(output.read_text(encoding='utf-8')) if output.exists() else {}
results=[r for r in previous.get('models',[]) if 'error' not in r and r['source'] in urls]
done={r['source'] for r in results};started=time.monotonic()
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
    for task in concurrent.futures.as_completed([pool.submit(inspect,m) for m in groups if m['src'] not in done]):
        results.append(task.result())
        if len(results)%25==0:print(f'Names checked: {len(results)}/{len(groups)}; failures {sum("error" in r for r in results)}',flush=True)
report={'source':'https://bluearchive.wiki/wiki/Models','revision':'525ae0faeb0a89f54ef4023f3be3692dcb529e51',
        'method':'Exact HTTP 206 JSON metadata ranges only; no GLB body saved or imported',
        'catalogDigest':digest,'seconds':round(time.monotonic()-started,1),'models':sorted(results,key=lambda r:r['model'])}
output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(f'Saved names-only report: {len(results)} models, {sum("error" in r for r in results)} failures, {report["seconds"]}s',flush=True)
