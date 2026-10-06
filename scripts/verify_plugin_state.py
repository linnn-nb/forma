#!/usr/bin/env python3
"""Actual resident DSP-state capture, adoption, recovery and Undo on CoreAudio."""
import argparse,array,hashlib,json,math,os,struct,subprocess,sys,uuid
from pathlib import Path

def active_clips(track):
 return next(p["clips"] for p in track["playlists"] if p["id"] == track["active_playlist_id"]) if track["kind"] == "audio" else []

root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('music',type=Path);p.add_argument('--cli',type=Path,default=root/'build/ndaw_artefacts/Release/ndaw');p.add_argument('--prefix',default='plugin-state-device-release');p.add_argument('--require-audit',action='store_true');a=p.parse_args()
assert a.prefix and all(c.isalnum() or c in '-_' for c in a.prefix)
cli=a.cli.resolve();music=a.music.resolve();original_hash=hashlib.sha256(music.read_bytes()).hexdigest();run=root/'evidence/runs'/('plugin-state-'+uuid.uuid4().hex);run.mkdir(parents=True);catalog=run/'catalog';env=dict(os.environ,NATIVEDAW_PLUGIN_CATALOG=str(catalog))
summary={'status':'running','binary':str(cli),'binary_sha256':hashlib.sha256(cli.read_bytes()).hexdigest(),'run_directory':str(run),'actual_model':False,'actual_plugin':True,'listening':False,'checks':[],'scope':'four short built-in CoreAudio buffer runs, real music and genuine Apple AU; no broad vendor/editor/long-session/Pro Tools performance parity'}
def call(label,*args):
 r=subprocess.run([str(cli),*map(str,args)],env=env,capture_output=True,text=True,timeout=45);(run/(label+'.stdout.json')).write_text(r.stdout);(run/(label+'.stderr.txt')).write_text(r.stderr)
 text=(r.stdout or r.stderr).lstrip();j,_=json.JSONDecoder().raw_decode(text[text.find('{'):]);assert r.returncode==0,(label,r.returncode,j);return j
def pcm(path):
 data=path.read_bytes();assert data[:4]==b'RIFF' and data[8:12]==b'WAVE';at=12;fmt=raw=None
 while at+8<=len(data):
  name,size=struct.unpack_from('<4sI',data,at);chunk=data[at+8:at+8+size]
  if name==b'fmt ':fmt=chunk
  if name==b'data':raw=chunk
  at+=8+size+(size&1)
 code,ch,rate,_,align,bits=struct.unpack_from('<HHIIHH',fmt)
 if code==65534:code=struct.unpack_from('<I',fmt,24)[0]
 assert (code,ch,rate,align,bits)==(3,2,48000,8,32);result=array.array('f');result.frombytes(raw)
 if sys.byteorder!='little':result.byteswap()
 assert len(result)==192000 and all(math.isfinite(x) for x in result);return result
try:
 scan=call('scan','plugins-scan','AudioUnit','AudioUnit:Effects/aufx,lpas,appl',catalog);plugin=scan['entry']['plugins'][0];summary['plugin']={k:plugin[k] for k in ('id','name','version','format','reported_latency_frames')}
 for buffer in (64,128,256,512):
  folder=run/str(buffer);folder.mkdir();project=folder/'session.ndaw';call(f'{buffer}-new','new',project)
  ops=[{'command':'add_audio_track','id':'audio','name':'Actual music'},{'command':'import_audio','track_id':'audio','path':str(music),'position':0},{'command':'add_aux_track','id':'aux','bus_id':'aux-input','name':'Actual resident AU'},{'command':'set_track_output','track_id':'audio','target_bus_id':'aux-input'},{'command':'insert_plugin','track_id':'aux','plugin_id':plugin['id'],'id':'au-instance'},{'command':'set_plugin_parameter','track_id':'aux','processor_id':'au-instance','parameter_id':'0','normalized':.25},{'command':'set_track_gain','track_id':'audio','gain_db':-33},{'command':'add_master_track','id':'master','name':'Main'},{'command':'set_track_gain','track_id':'master','gain_db':-12}]
  edit=folder/'edit.json';edit.write_text(json.dumps(ops));call(f'{buffer}-setup','edit',project,edit);before=call(f'{buffer}-before','query',project);fx=next(t for t in before['tracks'] if t['id']=='aux')['processors'][0];state=folder/fx['state']['path'];state_hash=hashlib.sha256(state.read_bytes()).hexdigest()
  req=folder/'request.json';req.write_text(json.dumps({'track_id':'aux','processor_id':'au-instance','parameter_id':'0','normalized':.6}));r=call(f'{buffer}-device','device-plugin-state',project,buffer,req)
  assert r['status']=='actual_resident_sdk_capture_adoption_recovery_undo_export_executed' and r['sdk_opaque_restore_before_overlay'];capture=r['capture_receipt'];assert capture['owned_pid']==r['resident_pid'] and capture['fault']==0 and capture['process_exit']['status']=='exited' and capture['process_exit']['exit_code']==0 and capture['process_exit']['reaped']
  timing=capture['capture_timing'];assert timing['callback_quiescence_ms']<=250 and timing['preparer_ack_ms']<=1500 and timing['sdk_capture_exit_reap_ms']<=1500 and timing['complete_suspend_capture_ms']<=4000
  assert r['capture']['state_sha256']!=state_hash and r['restored_instance']['owned_pid']!=r['resident_pid'];restored=r['restored_playing_instance'];assert restored['fault']==0 and restored['nonzero_output_frames']>=12000
  files=[folder/name for name in ('state-before.wav','state-after.wav','state-undo.wav')];decoded=[pcm(f) for f in files];error=max(abs(x-y) for x,y in zip(decoded[0],decoded[1]));undo_error=max(abs(x-y) for x,y in zip(decoded[0],decoded[2]));assert error<=2e-6 and undo_error<=2e-6
  after=call(f'{buffer}-after','query',project);af=next(t for t in after['tracks'] if t['id']=='aux')['processors'][0];assert af['state']==fx['state'] and abs(af['parameters'][0]['value']-.6)<=1e-6 and hashlib.sha256(state.read_bytes()).hexdigest()==state_hash
  assert after['sources']==before['sources'] and [active_clips(t) for t in after['tracks']]==[active_clips(t) for t in before['tracks']]
  native=r['device']['native_device'];assert native['fault']==0 and native['driver_overloads']==0 and native['full_ioproc_deadline_miss']==0;period=buffer/48000*1e6;counts=native['full_ioproc_histogram'];total=sum(counts);cum=0;p99=None
  for count,bound in zip(counts,(10,25,50,100,250,500,1000,None)):
   cum+=count
   if cum>=total*.99:p99=bound;break
  assert p99 is not None and p99<=period*.5 and native['full_ioproc_max_us']<=period*.9
  retirement=json.loads((Path(restored['job_directory'])/'retirement.json').read_text());assert retirement['retirement_status']=='verified_normal_teardown' and retirement['reservation_released'] and retirement['process_exit']['reaped']
  if a.require_audit:
   audit=native['rt_audit'];assert audit['scopes']>0 and all(audit[k]==0 for k in ('allocation','free','blocking_lock_wait','file_network','device_property_control'))
  summary['checks'].append({'buffer':buffer,'passed':True,'capture_timing':timing,'sdk_opaque_restore_before_overlay':True,'pcm_max_error':error,'undo_pcm_max_error':undo_error,'pcm_frames':96000,'native_callback_max_us':native['full_ioproc_max_us'],'native_callback_p99_bound_us':p99,'original_state_retained':True,'actual_normal_retirement':retirement['retirement_status'],'receipt':str(run/f'{buffer}-device.stdout.json'),'rt_audit':native.get('rt_audit')})
 assert hashlib.sha256(music.read_bytes()).hexdigest()==original_hash;summary.update(status='verified',original_media_sha256=original_hash,all_four_buffer_gates=True)
except Exception as e:
 summary.update(status='failed',error=str(e));raise
finally:
 (root/'evidence'/f'{a.prefix}.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
