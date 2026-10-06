#!/usr/bin/env python3
"""Group edits of retained physical microphone PCM, independently decoded export.
One prior physical input/two logical capture files; short output device run only.
No new input, model, listening, desktop, endurance or comparison acceptance.
"""
from pathlib import Path
import hashlib,json,math,subprocess,sys,time,uuid
from verify_playlist_comp_pcm import wav
repo=Path(__file__).resolve().parent.parent
cli=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else repo/'build/ndaw_artefacts/Release/ndaw'
label=sys.argv[2] if len(sys.argv)>2 else 'release'
base=repo/'evidence/edit-groups'
root=base/('physical-'+str(uuid.uuid4()));root.mkdir(parents=True)
project=root/'session.ndaw';calls=[]
def call(name,*args):
 start=time.monotonic();p=subprocess.run([str(cli),*map(str,args)],capture_output=True,text=True,timeout=40)
 (root/(name+'.stdout.json')).write_text(p.stdout);(root/(name+'.stderr.txt')).write_text(p.stderr)
 calls.append({'name':name,'seconds':time.monotonic()-start,'exit_code':p.returncode})
 data=json.loads(p.stdout or p.stderr)
 if p.returncode:raise RuntimeError(data)
 return data
def edit(name,ops):
 path=root/(name+'.operations.json');path.write_text(json.dumps(ops));return call(name,'edit',project,path)
def query(name):return call(name,'query',project)
def same(a,b):
 x=dict(a);x['revision']=b['revision'];assert x==b
report={'status':'running','cli':str(cli),'cli_sha256':hashlib.sha256(cli.read_bytes()).hexdigest(),'work':str(root),'actual_captured_source':True,'physical_input_count':1,'logical_capture_files':2,'new_input_capture':False,'actual_model':False,'listening':False,'desktop_mouse_acceptance':False,'endurance':False,'pro_tools_comparison':False}
try:
 prior=json.loads((repo/'evidence/punch-recording/physical-release.json').read_text());files=prior['capture']['capture']['files'];assert len(files)==2
 hashes={f['path']:hashlib.sha256(Path(f['path']).read_bytes()).hexdigest() for f in files}
 call('new','new',project,48000);ops=[]
 for i,f in enumerate(files):ops.extend([{'command':'add_audio_track','id':f'group-{i}','name':f'Captured group member {i+1}'},{'command':'import_audio','track_id':f'group-{i}','path':f['path'],'position':128},{'command':'set_track_gain','track_id':f'group-{i}','gain_db':-18+6*i}])
 edit('import',ops);s=query('before');ops=[]
 for t in s['tracks']:
  c=next(p for p in t['playlists'] if p['id']==t['active_playlist_id'])['clips'][0]
  ops.extend([{'command':'trim_clip','clip_id':c['id'],'source_start':37,'length':120001},{'command':'set_clip_fades','clip_id':c['id'],'fade_in':701,'fade_out':1207}])
 ops.extend([{'command':'insert_limiter','track_id':'group-0','lookahead_frames':16,'ceiling_db':0,'release_ms':10},
             {'command':'create_track_group','id':'actual-source-group','name':'Actual source linked edit/mix','type':'edit_mix','track_ids':['group-0','group-1']}])
 edit('setup',ops);prepared=query('prepared')
 # No device is opened during the extreme-value check.
 edit('upper-bound',[{'command':'set_track_gain','track_id':'group-0','gain_db':24}]);upper=query('upper-bound-state');assert upper['tracks'][1]['gain_db']==24 and upper['tracks'][1]['group_values']['gain_db']==30
 edit('safe-levels',[{'command':'set_track_gain','track_id':'group-0','gain_db':-18}]);safe=query('safe-levels-state');assert safe['tracks'][1]['gain_db']==-12
 ids=[c['id'] for t in safe['tracks'] for p in t['playlists'] if p['id']==t['active_playlist_id'] for c in p['clips']]
 operations=[{'command':'edit_clip_selection','clip_ids':ids,'action':'move','delta':71},
             {'command':'edit_clip_selection','clip_ids':ids,'action':'trim','edge':'left','delta':173},
             {'command':'edit_clip_selection','clip_ids':ids,'action':'split','position':48037}]
 path=root/'plan.operations.json';path.write_text(json.dumps(operations));preview=call('preview','preview',project,path);assert query('preview-unchanged')==safe
 edit('commit',operations);selected=query('selected');assert selected['revision']==safe['revision']+1 and all(len(next(p for p in t['playlists'] if p['id']==t['active_playlist_id'])['clips'])==2 for t in selected['tracks'])
 export_frames=max(c['start']+c['length'] for t in selected['tracks'] for p in t['playlists'] if p['id']==t['active_playlist_id'] for c in p['clips']);assert export_frames==120200
 export=call('render','render',project,root/'group-mix.wav',0,export_frames);channels,frames,pcm=wav(root/'group-mix.wav');assert channels==2 and frames==export_frames
 decoded={s['id']:wav(root/s['path']) for s in selected['sources']};expected=[0.]*(frames*2)
 for t in selected['tracks']:
  assert not t['muted'] and t['pan']==0 and not t['sends']
  for c in next(p for p in t['playlists'] if p['id']==t['active_playlist_id'])['clips']:
   nc,_,source=decoded[c['source_id']];assert nc==1
   for at in range(c['start'],min(frames,c['start']+c['length'])):
    local=at-c['start'];gain=10**((t['gain_db']+c['gain_db'])/20)/math.sqrt(2)
    if c['fade_in'] and local<c['fade_in']:gain*=local/c['fade_in']
    if c['fade_out'] and local>=c['length']-c['fade_out']:gain*=(c['length']-1-local)/c['fade_out']
    for ramp in c.get('gain_envelope',[]):
     x=min(1.,max(0.,(local-ramp['begin'])/(ramp['end']-ramp['begin'])))
     gain*=(math.sin(x*math.pi/2) if ramp['direction']=='in' else math.cos(x*math.pi/2)) if ramp['curve']=='equal_power' else (x if ramp['direction']=='in' else 1-x)
    value=source[c['source_start']+local]*gain;assert abs(value)<1 # limiter remains below ceiling
    expected[2*at]+=value;expected[2*at+1]+=value
 error=max(abs(a-b) for a,b in zip(expected,pcm));assert error<=1e-7,error
 report['pcm']={'passed':True,'independent_riff_decode':True,'frames':frames,'channels':channels,'sample_rate':48000,'pcm_max_error':error,'tolerance':1e-7,'sha256':hashlib.sha256((root/'group-mix.wav').read_bytes()).hexdigest(),'actual_16_sample_limiter_and_pdc':True}
 assert query('reopen')==selected
 device=call('physical-device','device-play',project,2600,256);assert device['status']=='device_callback_run' and device['callbacks']>0 and device['maximum_output_peak']>0
 for key in ('deadline_miss','driver_xruns','recording_gaps','playback_queue_underruns','graph_failures'):assert device[key]==0,(key,device[key])
 native=device['native_device'];assert native['full_ioproc_deadline_miss']==0
 audit=native.get('rt_audit')
 if audit:assert audit['scopes']>0 and all(audit[k]==0 for k in ('allocation','free','blocking_lock_wait','file_network','device_property_control'))
 report['actual_coreaudio_output']=device
 call('undo','undo',project);same(query('undone'),safe);call('redo','redo',project);same(query('redone'),selected)
 assert selected['sources']==prepared['sources'] and all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==sha for path,sha in hashes.items())
 report.update(status='verified_group_commands_actual_source_pcm_and_short_physical_output',original_hashes=hashes,preview=preview,calls=calls,actual_model_end_to_end='blocked: no configured authorized Provider/model/credentials')
except Exception as exc:
 report.update(status='failed',error=str(exc),calls=calls);(base/('physical-'+label+'.json')).write_text(json.dumps(report,indent=2));raise
(base/('physical-'+label+'.json')).write_text(json.dumps(report,indent=2));print(json.dumps({'status':report['status'],'pcm':report['pcm'],'work':str(root)},indent=2))
