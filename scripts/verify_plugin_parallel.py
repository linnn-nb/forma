#!/usr/bin/env python3
"""Eight real parallel AU effects on music, real CoreAudio and verified NRT retirement; no model/listening claim."""
import argparse,hashlib,json,os,subprocess,uuid
from pathlib import Path
root=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('music',type=Path);parser.add_argument('--cli',type=Path,default=root/'build/ndaw_artefacts/Release/ndaw');parser.add_argument('--prefix',default='plugin-parallel-device');parser.add_argument('--require-audit',action='store_true');args=parser.parse_args()
assert args.prefix and all(c.isalnum() or c in '-_' for c in args.prefix);cli=args.cli.resolve();music=args.music.resolve();original=hashlib.sha256(music.read_bytes()).hexdigest()
run=root/'evidence/runs'/('parallel-au-'+uuid.uuid4().hex);run.mkdir(parents=True);catalog=run/'catalog';folder=run/'project';folder.mkdir();project=folder/'session.ndaw';env=dict(os.environ,NATIVEDAW_PLUGIN_CATALOG=str(catalog))
report={'status':'running','actual_sdk':True,'actual_model':False,'listening':False,'binary':str(cli),'binary_sha256':hashlib.sha256(cli.read_bytes()).hexdigest(),'run_directory':str(run),'scope':'8 instances of one actual stereo Apple AU, 48k/256, real music/device and NRT teardown; not all-vendor/capacity/endurance/Pro Tools/low-latency monitor qualification'}
def call(label,*args):
 r=subprocess.run([str(cli),*map(str,args)],capture_output=True,text=True,env=env,timeout=30);(run/(label+'.stdout.json')).write_text(r.stdout);(run/(label+'.stderr.log')).write_text(r.stderr)
 text=(r.stdout or r.stderr).lstrip();j,_=json.JSONDecoder().raw_decode(text[text.find('{'):]);assert r.returncode==0,(label,r.returncode,j.get('code'),j.get('status'));return j
try:
 scan=call('scan','plugins-scan','AudioUnit','AudioUnit:Effects/aufx,lpas,appl',catalog);assert scan['status']=='verified';plugin=scan['entry']['plugins'][0]
 call('new','new',project);ops=[{'command':'add_audio_track','id':'audio','name':'Actual music'},{'command':'import_audio','track_id':'audio','path':str(music),'position':0},{'command':'set_track_gain','track_id':'audio','gain_db':-35},{'command':'add_master_track','id':'master','name':'Main'},{'command':'set_track_gain','track_id':'master','gain_db':-24}]
 for i in range(8):
  aux,fx,bus=f'aux-{i}',f'au-{i}',f'bus-{i}'
  ops.extend([{'command':'add_aux_track','id':aux,'bus_id':bus,'name':f'Actual AU {i+1}'},{'command':'insert_plugin','track_id':aux,'plugin_id':plugin['id'],'id':fx},{'command':'set_plugin_parameter','track_id':aux,'processor_id':fx,'parameter_id':'0','normalized':.05+i*.002},{'command':'add_send','track_id':'audio','target_bus_id':bus,'gain_db':-12}])
 commands=folder/'commands.json';commands.write_text(json.dumps(ops));preview=call('preview','preview',project,commands);assert preview['status']=='preview' and preview['changes'];assert call('commit','edit',project,commands)['status']=='committed'
 reopened=call('reopen','query',project);assert reopened['schema_version']==7 and sum(len(t['processors']) for t in reopened['tracks'])==8
 exported=call('render','render',project,folder/'actual-eight-au.wav',0,240000);assert exported['status']=='exported_and_verified' and exported['format']['frames']==240000 and exported['measurement']['peak']>1e-8 and len(exported['plugins'])==8
 for p in exported['plugins']:assert p['fault']==0 and p['process_exit']['reaped'] and p['process_exit']['status']=='exited' and p['processed_quanta']>=937
 # 6.5 s wall allowance retains the declared >=5 s of actual playing/nonzero frames per instance after priming.
 result=call('device','device-play',project,6500,256);assert result['status']=='device_callback_run' and result['executed_wall_ms']>=6500
 live=result['routing']['plugins']['instances'];assert len(live)==8 and result['routing']['plugins']['workers']==8
 for p in live:assert p['fault']==0 and p['deadline_misses']==0 and p['processed_playing_frames']-1024>=240000 and p['nonzero_output_frames']>=240000
 native=result['native_device'];period=256/48000*1e6;hist=native['full_ioproc_histogram'];acc=0;p99=None
 for count,bound in zip(hist,(10,25,50,100,250,500,1000,None)):
  acc+=count
  if acc>=sum(hist)*.99:p99=bound;break
 assert p99 is not None and p99<=period*.5 and native['full_ioproc_max_us']<=period*.9
 assert result['deadline_miss']==0 and result['playback_queue_underruns']==0 and result['maximum_output_peak']>1e-8 and result['processing_latency_frames']==1024
 assert native['full_ioproc_deadline_miss']==0 and native['fault']==0 and native['driver_overloads']==0
 if args.require_audit:
  audit=native['rt_audit'];assert audit['scopes']>0
  for k in ('allocation','free','blocking_lock_wait','file_network','device_property_control'):assert audit[k]==0
 retirements=[]
 for p in live:
  directory=Path(p['job_directory']);assert directory.resolve().is_relative_to(run.resolve());r=json.loads((directory/'retirement.json').read_text());assert r['retirement_status']=='verified_normal_teardown' and r['reservation_released'] and r['process_exit']['reaped'] and r['process_exit']['exit_code']==0
  elapsed=(r['finished_ns']-r['requested_ns'])/1e6;assert elapsed<=1500;retirements.append({'instance_id':r['instance_id'],'normal_exit_reaped':True,'retire_ms':elapsed})
 assert hashlib.sha256(music.read_bytes()).hexdigest()==original
 report.update(status='passed',plugin={k:plugin[k] for k in ['id','name','version','format']},actual_export=str(folder/'actual-eight-au.wav'),export_frames=exported['format']['frames'],sdk_instances=8,wall_ms=result['executed_wall_ms'],sample_rate=48000,buffer_frames=256,callback_p99_upper_bin_us=p99,full_ioproc_max_us=native['full_ioproc_max_us'],period_us=period,predeclared_p99_percent=50,predeclared_max_percent=90,observed_faults=0,algorithmic_latency_frames=1024,instances=[{k:p[k] for k in ['instance_id','owned_pid','processed_playing_frames','nonzero_output_frames','process_max_us','deadline_misses']} for p in live],audit=native.get('rt_audit'),normal_retirements=retirements,original_music_sha256=original,original_unchanged=True)
except Exception as e:report.update(status='failed',error=str(e));raise
finally:
 (run/'summary.json').write_text(json.dumps(report,indent=2));(root/'evidence'/(args.prefix+'.json')).write_text(json.dumps(report,indent=2));print(json.dumps({k:report.get(k) for k in ['status','sdk_instances','full_ioproc_max_us','run_directory','error']}))
