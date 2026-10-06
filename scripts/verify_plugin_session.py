#!/usr/bin/env python3
"""Genuine Apple AU project -> export and actual CoreAudio playback; no model/listening substitution."""
import argparse,hashlib,json,os,subprocess,uuid
from pathlib import Path
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('music',type=Path);parser.add_argument('--cli',type=Path,default=root/'build/ndaw_artefacts/Release/ndaw')
parser.add_argument('--prefix',default='plugin-project-device');parser.add_argument('--offline-only',action='store_true');parser.add_argument('--require-audit',action='store_true')
args=parser.parse_args();assert args.prefix and all(c.isalnum() or c in '-_' for c in args.prefix)
cli=args.cli.resolve();music=args.music.resolve();music_hash=hashlib.sha256(music.read_bytes()).hexdigest()
run=root/'evidence/runs'/('plugin-session-'+str(uuid.uuid4()));run.mkdir(parents=True);catalog=run/'catalog'
env=dict(os.environ,NATIVEDAW_PLUGIN_CATALOG=str(catalog))
summary={'status':'running','run_directory':str(run),'binary':str(cli),'binary_sha256':hashlib.sha256(cli.read_bytes()).hexdigest(),
 'actual_plugin':True,'actual_model':False,'listening':False,'scope':'actual stereo Apple AU session/export; physical CoreAudio gates separately named; no Pro Tools/Windows/dense capacity/low-latency monitor claim',
 'checks':[],'device_receipts':[]}
def call(label,*arguments,expected=0):
 p=subprocess.run([str(cli),*map(str,arguments)],env=env,capture_output=True,text=True,timeout=25)
 (run/(label+'.stdout.json')).write_text(p.stdout);(run/(label+'.stderr.txt')).write_text(p.stderr)
 text=(p.stdout or p.stderr).lstrip();start=text.find('{');assert start>=0,(label,p.returncode,text[:200]);r,_=json.JSONDecoder().raw_decode(text[start:])
 assert p.returncode==expected,(label,p.returncode,r.get('code'),r.get('failure'),r.get('status'));return r
try:
 scanned=call('scan','plugins-scan','AudioUnit','AudioUnit:Effects/aufx,lpas,appl',catalog);assert scanned['status']=='verified'
 plugin=scanned['entry']['plugins'][0];summary['plugin']={k:plugin[k] for k in ('id','name','version','format','reported_latency_frames')}
 for buffer in ((64,128,256,512) if not args.offline_only else (256,)):
  d=run/str(buffer);d.mkdir();project=d/'session.ndaw';call(str(buffer)+'-new','new',project)
  ops=[{'command':'add_audio_track','id':'audio','name':'Actual music'},{'command':'import_audio','track_id':'audio','path':str(music),'position':0},
       {'command':'add_aux_track','id':'aux','bus_id':'aux-input','name':'Actual AU Aux'},
       {'command':'set_track_output','track_id':'audio','target_bus_id':'aux-input'},
       {'command':'insert_plugin','track_id':'aux','plugin_id':plugin['id'],'id':'au-instance'},
       {'command':'set_plugin_parameter','track_id':'aux','processor_id':'au-instance','parameter_id':'0','normalized':.05},
       {'command':'set_track_gain','track_id':'audio','gain_db':-33},
       {'command':'add_master_track','id':'master','name':'Main'}, {'command':'set_track_gain','track_id':'master','gain_db':-12}]
  commands=d/'commands.json';commands.write_text(json.dumps(ops));preview=call(str(buffer)+'-preview','preview',project,commands);assert preview['status']=='preview' and preview['changes']
  commit=call(str(buffer)+'-commit','edit',project,commands);assert commit['status']=='committed'
  session=call(str(buffer)+'-query','query',project);assert session['schema_version']==7
  inventory=call(str(buffer)+'-inventory','plugin-inventory',project);assert plugin['id'] in [p['id'] for p in inventory['plugins']] and 'path' not in json.dumps(inventory['plugins'])
  exported=call(str(buffer)+'-render','render',project,d/'actual-processed.wav',0,240000)
  assert exported['status']=='exported_and_verified' and exported['format']['frames']==240000 and exported['routing']['algorithmic_latency_frames']==1024
  actual=exported['plugins'][0];assert actual['fault']==0 and actual['processed_quanta']>=937 and actual['process_exit']['status']=='exited' and actual['process_exit']['reaped']
  assert abs(next(p['value'] for p in actual['prepared']['parameters'] if p['id']=='0')-.05)<=1e-6
  assert exported['measurement']['peak']>1e-8
  summary['checks'].append({'id':'PLUGIN-MUSIC-'+str(buffer),'passed':True,'project':str(project),'real_export':str(d/'actual-processed.wav'),
   'frames':240000,'actual_processed_quanta':actual['processed_quanta'],'process_exit':actual['process_exit'],'source_unchanged':hashlib.sha256(music.read_bytes()).hexdigest()==music_hash})
  if args.offline_only:continue
  result=call(str(buffer)+'-device','device-edit-play',project,8000,buffer)
  assert result['status']=='device_playback_with_domain_edits_verified' and result['executed_wall_ms']>=8000
  native=result['native_device'];counts=native['full_ioproc_histogram'];total=sum(counts);accumulated=0;p99=None
  for count,bound in zip(counts,(10,25,50,100,250,500,1000,None)):
   accumulated+=count
   if accumulated>=total*.99:p99=bound;break
  live=result['routing']['plugins']['instances'];assert len(live)==1 and result['routing']['plugins']['workers']==1
  instance=live[0];assert instance['fault']==0 and instance['deadline_misses']==0 and instance['processed_quanta']*256>=240000
  assert instance['processed_playing_frames']-1024>=240000 and instance['nonzero_output_frames']>=240000
  period=buffer/48000*1e6;maximum=native['full_ioproc_max_us']
  assert p99 is not None and p99<=period*.5 and maximum<=period*.9,(buffer,p99,maximum,period)
  assert result['deadline_miss']==0 and result['playback_queue_underruns']==0 and result['maximum_output_peak']>1e-8
  assert native['full_ioproc_deadline_miss']==0 and native['fault']==0 and native['driver_overloads']==0
  if args.require_audit:
   audit=native['rt_audit'];assert audit['scopes']>0
   for k in ('allocation','free','blocking_lock_wait','file_network','device_property_control'):assert audit[k]==0,(k,audit)
  summary['device_receipts'].append({'id':'PLUGIN-COREAUDIO-'+str(buffer),'passed':True,'buffer_frames':buffer,'sample_rate':48000,
   'wall_ms':result['executed_wall_ms'],'plugin_processed_frames':instance['processed_quanta']*256,'plugin_process_max_us':instance['process_max_us'],
   'plugin_processed_playing_frames':instance['processed_playing_frames'],'plugin_nonzero_output_frames':instance['nonzero_output_frames'],'nonzero_threshold':1e-12,
   'callback_p99_upper_bin_us':p99,'full_ioproc_max_us':maximum,'period_us':period,'predeclared_p99_percent':50,'predeclared_max_percent':90,
   'plugin_deadline_misses':0,'source_underruns':0,'callback_deadline_misses':0,'native_fault':0,'observed_driver_overload':0,
   'graph_processing_latency_frames':result['processing_latency_frames'],'live_gain_edits':len(result['edits']),'plugin_workers_after_edits':1,
   'audit':native.get('rt_audit'),'raw_receipt':str(run/(str(buffer)+'-device.stdout.json'))})
 summary['status']='passed';summary['original_music_sha256']=music_hash;summary['original_unchanged']=hashlib.sha256(music.read_bytes()).hexdigest()==music_hash
except Exception as e:
 summary['status']='failed';summary['error']=str(e);raise
finally:
 (run/'summary.json').write_text(json.dumps(summary,indent=2));(root/'evidence'/(args.prefix+'.json')).write_text(json.dumps(summary,indent=2))
 print(json.dumps({'status':summary['status'],'checks':len(summary['checks']),'device_gates':len(summary['device_receipts']),'run':str(run),'error':summary.get('error')}),flush=True)
