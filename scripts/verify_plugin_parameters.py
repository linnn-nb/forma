#!/usr/bin/env python3
"""Actual music/CoreAudio live SDK controls, Undo and file verification; no model substitute."""
import argparse,array,hashlib,json,math,os,struct,subprocess,sys,uuid
from pathlib import Path

def active_clips(track):
 return next(p["clips"] for p in track["playlists"] if p["id"] == track["active_playlist_id"]) if track["kind"] == "audio" else []

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('music',type=Path);parser.add_argument('--cli',type=Path,default=root/'build/ndaw_artefacts/Release/ndaw')
parser.add_argument('--prefix',default='plugin-parameters-device-release');parser.add_argument('--require-audit',action='store_true')
args=parser.parse_args();assert args.prefix and all(c.isalnum() or c in '-_' for c in args.prefix)
cli=args.cli.resolve();music=args.music.resolve();music_hash=hashlib.sha256(music.read_bytes()).hexdigest()
run=root/'evidence/runs'/('plugin-parameters-'+uuid.uuid4().hex);run.mkdir(parents=True);catalog=run/'catalog'
env=dict(os.environ,NATIVEDAW_PLUGIN_CATALOG=str(catalog))
summary={'status':'running','run_directory':str(run),'binary':str(cli),'binary_sha256':hashlib.sha256(cli.read_bytes()).hexdigest(),
 'actual_plugin':True,'actual_model':False,'listening':False,'checks':[],
 'scope':'genuine Apple stereo AU, real music, four short CoreAudio runs and verified files; no general VST3/automation/endurance/Pro Tools qualification'}
def call(label,*arguments):
 p=subprocess.run([str(cli),*map(str,arguments)],env=env,capture_output=True,text=True,timeout=25)
 (run/(label+'.stdout.json')).write_text(p.stdout);(run/(label+'.stderr.txt')).write_text(p.stderr)
 text=(p.stdout or p.stderr).lstrip();start=text.find('{');assert start>=0,(label,p.returncode,text[:200]);r,_=json.JSONDecoder().raw_decode(text[start:])
 assert p.returncode==0,(label,p.returncode,r.get('code'),r.get('status'));return r
def pcm(path):
 raw=path.read_bytes();assert raw[:4]==b'RIFF' and raw[8:12]==b'WAVE';at=12;fmt=data=None
 while at+8<=len(raw):
  kind,size=struct.unpack_from('<4sI',raw,at);chunk=raw[at+8:at+8+size]
  if kind==b'fmt ':fmt=chunk
  if kind==b'data':data=chunk
  at+=8+size+(size&1)
 code,channels,rate,_,align,bits=struct.unpack_from('<HHIIHH',fmt)
 if code==0xfffe:code=struct.unpack_from('<I',fmt,24)[0]
 assert (code,channels,rate,align,bits)==(3,2,48000,8,32)
 samples=array.array('f');samples.frombytes(data)
 if sys.byteorder!='little':samples.byteswap()
 assert len(samples)==480000 and all(math.isfinite(x) for x in samples);return samples
try:
 scan=call('scan','plugins-scan','AudioUnit','AudioUnit:Effects/aufx,lpas,appl',catalog);assert scan['status']=='verified';plugin=scan['entry']['plugins'][0]
 summary['plugin']={k:plugin[k] for k in ('id','name','version','format','reported_latency_frames')}
 for buffer in (64,128,256,512):
  d=run/str(buffer);d.mkdir();project=d/'session.ndaw';call(f'{buffer}-new','new',project)
  ops=[{'command':'add_audio_track','id':'audio','name':'Actual music'},{'command':'import_audio','track_id':'audio','path':str(music),'position':0},
       {'command':'add_aux_track','id':'aux','bus_id':'aux-input','name':'Actual AU Aux'},{'command':'set_track_output','track_id':'audio','target_bus_id':'aux-input'},
       {'command':'insert_plugin','track_id':'aux','plugin_id':plugin['id'],'id':'au-instance'},
       {'command':'set_plugin_parameter','track_id':'aux','processor_id':'au-instance','parameter_id':'0','normalized':.25},
       {'command':'set_track_gain','track_id':'audio','gain_db':-33},{'command':'add_master_track','id':'master','name':'Main'},{'command':'set_track_gain','track_id':'master','gain_db':-12}]
  commands=d/'commands.json';commands.write_text(json.dumps(ops));call(f'{buffer}-setup','edit',project,commands)
  parameter={'command':'set_plugin_parameter','track_id':'aux','processor_id':'au-instance','parameter_id':'0'}
  def edit(label,value):
   commands.write_text(json.dumps([dict(parameter,normalized=value)]));return call(f'{buffer}-{label}','edit',project,commands)
  edit('low',.05);low=d/'low.wav';reference=call(f'{buffer}-low-render','render',project,low,0,240000);low_pcm=pcm(low)
  edit('high',.6);high=d/'high.wav';call(f'{buffer}-high-render','render',project,high,0,240000);assert max(abs(a-b) for a,b in zip(low_pcm,pcm(high)))>2e-6
  edit('initial',.25);before=call(f'{buffer}-before','query',project);fx=next(t for t in before['tracks'] if t['id']=='aux')['processors'][0]
  state=project.parent/fx['state']['path'];state_hash=hashlib.sha256(state.read_bytes()).hexdigest()
  request=d/'parameter-workload.json';request.write_text(json.dumps({'track_id':'aux','processor_id':'au-instance','parameter_id':'0','values':[.05,.6]}))
  result=call(f'{buffer}-device','device-plugin-parameters',project,8000,buffer,request)
  assert result['status']=='device_plugin_parameters_verified' and result['executed_wall_ms']>=8000 and len(result['edits'])==3
  assert result['same_plugin_pid'] and result['monotonic_transport'] and result['playing_when_committed'] and result['saved_reopen_equal']
  for e in result['edits']:
   assert e['owned_pid']==result['owned_pid'] and e['commit_publish_ms']<=50 and 0<=e['publication_to_instance_output_ms']<=50 and e['commit_to_instance_output_ms']<=100
   v=e['verified_output'];assert v['acknowledged_token']==e['desired_token']==v['returned_output_token']
   assert abs(next(x['actual'] for x in v['actual_values'] if x['sdk_id']=='0')-e['desired_normalized'])<=1e-6
  live=result['routing']['plugins']['instances'];assert len(live)==1;instance=live[0];assert instance['fault']==0 and instance['deadline_misses']==0
  assert instance['processed_playing_frames']-1024>=240000 and instance['nonzero_output_frames']>=240000
  native=result['native_device'];counts=native['full_ioproc_histogram'];total=sum(counts);cumulative=0;p99=None
  for count,bound in zip(counts,(10,25,50,100,250,500,1000,None)):
   cumulative+=count
   if cumulative>=total*.99:p99=bound;break
  period=buffer/48000*1e6;assert p99 is not None and p99<=period*.5 and native['full_ioproc_max_us']<=period*.9
  assert native['full_ioproc_deadline_miss']==0 and native['fault']==0 and native['driver_overloads']==0
  if args.require_audit:
   audit=native['rt_audit'];assert audit['scopes']>0
   assert all(audit[k]==0 for k in ('allocation','free','blocking_lock_wait','file_network','device_property_control'))
  after=call(f'{buffer}-after','query',project);assert before['sources']==after['sources'] and [(t['id'],active_clips(t)) for t in before['tracks']]==[(t['id'],active_clips(t)) for t in after['tracks']]
  final=d/'after-live-undo.wav';export=call(f'{buffer}-final-render','render',project,final,0,240000);error=max(abs(a-b) for a,b in zip(low_pcm,pcm(final)));assert error<=2e-6
  assert export['status']=='exported_and_verified' and export['plugins'][0]['process_exit']['reaped'] and export['plugins'][0]['fault']==0
  retirements=[json.loads(p.read_text()) for p in (d/'.plugin-runtime').glob('*/retirement.json')]
  matching=[r for r in retirements if r.get('process_exit',{}).get('pid')==result['owned_pid']]
  assert len(matching)==1 and matching[0]['retirement_status']=='verified_normal_teardown' and matching[0]['reservation_released']
  assert hashlib.sha256(state.read_bytes()).hexdigest()==state_hash
  summary['checks'].append({'id':f'PLUGIN-PARAM-COREAUDIO-{buffer}','passed':True,'buffer_frames':buffer,'sample_rate':48000,'wall_ms':result['executed_wall_ms'],
   'same_pid':result['owned_pid'],'edits':result['edits'],'callback_p99_upper_bin_us':p99,'full_ioproc_max_us':native['full_ioproc_max_us'],
   'predeclared_p99_percent':50,'predeclared_max_percent':90,'actual_processed_playing_frames':instance['processed_playing_frames'],'actual_nonzero_output_frames':instance['nonzero_output_frames'],
   'plugin_process_max_us':instance['process_max_us'],'sdk_and_pipeline_latency_frames':result['processing_latency_frames'],'final_export_pcm_max_error':error,
   'source_and_opaque_state_unchanged':True,'actual_final_exit':matching[0]['process_exit'],'audit':native.get('rt_audit'),'project':str(project),'raw_receipt':str(run/(f'{buffer}-device.stdout.json'))})
 summary['status']='passed';summary['original_music_sha256']=music_hash;summary['original_unchanged']=hashlib.sha256(music.read_bytes()).hexdigest()==music_hash
except Exception as e:
 summary['status']='failed';summary['error']=str(e);raise
finally:
 (run/'summary.json').write_text(json.dumps(summary,indent=2));(root/'evidence'/(args.prefix+'.json')).write_text(json.dumps(summary,indent=2))
 print(json.dumps({'status':summary['status'],'checks':len(summary['checks']),'run':str(run),'error':summary.get('error')}),flush=True)
