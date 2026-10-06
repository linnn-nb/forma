#!/usr/bin/env python3
"""Real previously captured microphone media -> explicit linked candidate -> export.
Uses one physical input's two logical capture files; no multi-mic/listening/model
acceptance. Independent RIFF decode, exact envelope weights and immutable hashes.
Fresh owned project per run, preserved on failure and success.
"""
from pathlib import Path
import json,math,hashlib,subprocess,sys,time,uuid
from verify_playlist_comp_pcm import wav
repo=Path(__file__).resolve().parent.parent
cli=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else repo/'build/ndaw_artefacts/Release/ndaw'
root=repo/'evidence/linked-comp'/('physical-'+str(uuid.uuid4()));root.mkdir(parents=True)
project=root/'session.ndaw';receipts=[]
def call(name,*args):
 started=time.monotonic();p=subprocess.run([str(cli),*map(str,args)],capture_output=True,text=True,timeout=35)
 (root/(name+'.stdout.json')).write_text(p.stdout);(root/(name+'.stderr.txt')).write_text(p.stderr)
 result=json.loads(p.stdout or p.stderr);receipts.append({'name':name,'exit_code':p.returncode,'seconds':time.monotonic()-started})
 if p.returncode:raise RuntimeError(result)
 return result
def edit(name,ops):
 file=root/(name+'.operations.json');file.write_text(json.dumps(ops));return call(name,'edit',project,file)
def query(name):return call(name,'query',project)
def state_equal(a,b):
 c=dict(a);c['revision']=b['revision'];assert c==b

def verify(session,path):
 channels,frames,actual=wav(path);assert channels==2 and frames==144119
 sources={s['id']:s for s in session['sources']};decoded={};expected=[0.]*(frames*2)
 for t in session['tracks']:
  assert not t['processors'] and not t['sends'] and not t['muted'] and t['pan']==0 and t['gain_db']==0
  p=next(p for p in t['playlists'] if p['id']==t['active_playlist_id'])
  for c in p['clips']:
   sid=c['source_id'];s=sources[sid];file=root/s['path'];assert hashlib.sha256(file.read_bytes()).hexdigest()==s['sha256']
   if sid not in decoded:decoded[sid]=wav(file)
   count,_,pcm=decoded[sid];assert count==1
   for frame in range(max(0,c['start']),min(frames,c['start']+c['length'])):
    local=frame-c['start'];gain=10**(c['gain_db']/20)
    if c['fade_in'] and local<c['fade_in']:gain*=local/c['fade_in']
    if c['fade_out'] and local>=c['length']-c['fade_out']:gain*=(c['length']-1-local)/c['fade_out']
    for ramp in c.get('gain_envelope',[]):
     x=min(1.,max(0.,(local-ramp['begin'])/(ramp['end']-ramp['begin'])))
     gain*=((math.sin(x*math.pi/2) if ramp['direction']=='in' else math.cos(x*math.pi/2)) if ramp['curve']=='equal_power' else (x if ramp['direction']=='in' else 1-x))
    value=pcm[c['source_start']+local]*gain/math.sqrt(2)
    expected[frame*2]+=value;expected[frame*2+1]+=value
 error=max(abs(a-b) for a,b in zip(expected,actual));assert error<=1e-7,error
 return {'passed':True,'independent_riff_float32_decode':True,'frames':frames,'channels':2,'sample_rate':48000,'pcm_max_error':error,'tolerance':1e-7,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
report={'status':'running','work':str(root),'cli':str(cli),'cli_sha256':hashlib.sha256(cli.read_bytes()).hexdigest(),
 'physical_input_count':1,'logical_track_count':2,'new_input_capture_this_run':False,'actual_captured_source':True,
 'actual_model':False,'listening':False,'desktop_mouse_acceptance':False,'pro_tools_comparison':False,
 'scope':'explicit linked source pairing, centered481-sample crossfades, independent actual microphone PCM, full Undo/reopen and short48k/256 real CoreAudio output'}
try:
 prior=json.loads((repo/'evidence/punch-recording/physical-release.json').read_text())
 files=prior['capture']['capture']['files'];assert len(files)==2
 hashes={f['path']:hashlib.sha256(Path(f['path']).read_bytes()).hexdigest() for f in files}
 call('new','new',project,48000);ops=[]
 for i,f in enumerate(files):ops.extend([{'command':'add_audio_track','id':f'linked-{i}','name':f'Captured linked member {i+1}'},{'command':'import_audio','track_id':f'linked-{i}','path':f['path'],'position':0}])
 edit('import-real-capture',ops);before=query('before');targets={t['id']:t['target_playlist_id'] for t in before['tracks']}
 setup=[]
 for t in before['tracks']:
  pid='alternate-'+t['id'];cid='alternate-clip-'+t['id'];setup.extend([
   {'command':'create_playlist','track_id':t['id'],'id':pid,'name':'Alternate37-sample source map','source_playlist_id':t['active_playlist_id'],'clip_ids':[cid]},
   {'command':'select_playlist','track_id':t['id'],'playlist_id':pid},
   {'command':'trim_clip','clip_id':cid,'source_start':37,'length':192093-37},
   {'command':'select_playlist','track_id':t['id'],'playlist_id':t['active_playlist_id']}])
 edit('prepare-explicit-source-maps',setup);prepared=query('prepared');members=[]
 for t in prepared['tracks']:
  p=next(p for p in t['playlists'] if p['id']==t['active_playlist_id']);alt=next(p for p in t['playlists'] if p['id']=='alternate-'+t['id'])
  members.append({'track_id':t['id'],'segments':[{'source_playlist_id':p['id'],'clip_id':p['clips'][0]['id']},{'source_playlist_id':alt['id'],'clip_id':alt['clips'][0]['id']}]})
 create={'command':'create_linked_comp','source_revision':prepared['revision'],'id':'linked-real-candidate','name':'Real capture linked Comp','members':members,
         'ranges':[{'begin':128,'end':48037},{'begin':48037,'end':144119}],'crossfade_frames':481,'curve':'linear'}
 preview_file=root/'preview.operations.json';preview_file.write_text(json.dumps([create]));call('preview','preview',project,preview_file);assert query('preview-unchanged')==prepared
 edit('create-linear',[create]);inactive=query('inactive');assert all(t['active_playlist_id']==prepared['tracks'][i]['active_playlist_id'] for i,t in enumerate(inactive['tracks']))
 edit('select-linear',[{'command':'select_comp_set','comp_set_id':create['id']}]);selected=query('selected-linear');render=call('render-linear','render',project,root/'linear.wav',0,144119);report['linear_pcm']=verify(selected,root/'linear.wav')
 # A second actual command uses equal-power for the same explicitly paired sources.
 edit('restore-linear',[{'command':'restore_comp_set','comp_set_id':create['id']}]);current=query('before-power');power=dict(create,id='linked-real-power',name='Real capture equal power',curve='equal_power',source_revision=current['revision']);edit('create-power',[power]);edit('select-power',[{'command':'select_comp_set','comp_set_id':power['id']}]);power_state=query('selected-power');call('render-power','render',project,root/'equal-power.wav',0,144119);report['power_pcm']=verify(power_state,root/'equal-power.wav')
 assert all(t['target_playlist_id']==targets[t['id']] for t in power_state['tracks'])
 for i,t in enumerate(power_state['tracks']):
  for original in prepared['tracks'][i]['playlists']:assert next(p for p in t['playlists'] if p['id']==original['id'])==original
 assert query('reopen')==power_state
 device=call('physical-device','device-play',project,3200,256);assert device['callbacks']>0 and device['maximum_output_peak']>0 and device['status']=='device_callback_run'
 for key in ('deadline_miss','playback_queue_underruns','driver_xruns','graph_failures','recording_gaps'):assert device[key]==0,(key,device[key])
 assert device['native_device']['fault']==0 and device['native_device']['full_ioproc_deadline_miss']==0
 audit=device['native_device'].get('rt_audit')
 if audit:assert audit['scopes']>0 and all(audit[k]==0 for k in ('allocation','free','blocking_lock_wait','file_network','device_property_control'))
 report['device']=device
 call('undo-power-selection','undo',project);restored=query('restored');assert all(t['active_playlist_id']==prepared['tracks'][i]['active_playlist_id'] for i,t in enumerate(restored['tracks']))
 assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
 report.update(status='passed',original_media_hashes=hashes,project=str(project),source_playlists_preserved=True,target_preserved=True,undo_all_members_verified=True,real_source_export_reopen_verified=True)
except Exception as e:report.update(status='failed',error=str(e));raise
finally:
 report['operations']=receipts;(root/'result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
