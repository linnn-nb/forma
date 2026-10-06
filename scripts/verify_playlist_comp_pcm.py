#!/usr/bin/env python3
"""Independent RIFF float32 decode + sample/source/fade-map verification."""
from pathlib import Path
import json,struct,math,hashlib,sys
repo=Path(__file__).resolve().parent.parent

def wav(path):
 data=Path(path).read_bytes();assert data[:4]==b'RIFF' and data[8:12]==b'WAVE';chunks={};pos=12
 while pos+8<=len(data):
  key,size=struct.unpack_from('<4sI',data,pos);chunks[key]=data[pos+8:pos+8+size];pos+=8+size+(size&1)
 fmt=chunks[b'fmt '];tag,channels,rate,_,align,bits=struct.unpack_from('<HHIIHH',fmt);tag=struct.unpack_from('<H',fmt,24)[0] if tag==65534 else tag
 assert tag==3 and bits==32 and align==channels*4 and rate==48000
 pcm=struct.unpack('<'+'f'*(len(chunks[b'data'])//4),chunks[b'data']);assert all(math.isfinite(v) for v in pcm)
 return channels,len(pcm)//channels,pcm

def verify(session,root,file,begin,end):
 sources={s['id']:s for s in session['sources']};decoded={}
 for t in session['tracks']:
  if t['kind']!='audio':continue
  assert not t['processors'] and not t['sends'] and t['pan']==0 and t['gain_db']==0 and not t['muted']
  for c in next(p['clips'] for p in t['playlists'] if p['id']==t['active_playlist_id']):
   if c['source_id'] not in decoded:
    s=sources[c['source_id']];path=root/s['path'];assert hashlib.sha256(path.read_bytes()).hexdigest()==s['sha256'];decoded[c['source_id']]=wav(path)
 out_channels,frames,actual=wav(file);assert out_channels==2 and frames==end-begin
 expected=[0.]*(frames*2)
 for t in session['tracks']:
  if t['kind']!='audio':continue
  for c in next(p['clips'] for p in t['playlists'] if p['id']==t['active_playlist_id']):
   channels,_,pcm=decoded[c['source_id']]
   for frame in range(max(begin,c['start']),min(end,c['start']+c['length'])):
    inside=frame-c['start'];scale=10**(c['gain_db']/20)
    if c['fade_in'] and inside<c['fade_in']:scale*=inside/c['fade_in']
    if c['fade_out'] and inside>=c['length']-c['fade_out']:scale*=(c['length']-1-inside)/c['fade_out']
    sample=c['source_start']+inside
    for channel in range(2):expected[(frame-begin)*2+channel]+=pcm[sample*channels+(channel if channels==2 else 0)]*scale/(math.sqrt(2) if channels==1 else 1)
 error=max(abs(a-b) for a,b in zip(actual,expected));assert error<=1e-7,error
 return {'passed':True,'independent_float32_decoder':True,'frames':frames,'channels':2,'sample_rate':48000,'pcm_max_error':error,'tolerance':1e-7,'export_sha256':hashlib.sha256(Path(file).read_bytes()).hexdigest(),'scope':'actual captured PCM, explicit sample/offset/gain/linear fades; no listening/model/performance ranking'}

if __name__=='__main__':
 manifest=json.loads((repo/'evidence/playlist-comp/physical-workflow.json').read_text());root=Path(manifest['work']);session=json.loads((root/'selected.stdout.json').read_text())
 receipt=verify(session,root,root/'candidate.wav',0,96000);receipt['project']=str(root/'session.ndaw');(repo/'evidence/playlist-comp/independent-capture-pcm.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt,indent=2))
