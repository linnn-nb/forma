#!/usr/bin/env python3
"""Actual microphone -> separate Playlists/Takes -> explicit range Comp -> real export.
Records the selected default physical input locally; never uploads or infers
performance quality. Run only under the user's recording-test authorization.
"""
from pathlib import Path
import json,subprocess,sys,hashlib,time,uuid
repo=Path(__file__).resolve().parent.parent
root=repo/'evidence/playlist-comp'/('physical-'+str(uuid.uuid4()))
root.mkdir(parents=True);project=root/'session.ndaw';cli=repo/'build/ndaw_artefacts/Release/ndaw'
receipts=[]
def call(name,*args):
 started=time.monotonic();p=subprocess.run([str(cli),*map(str,args)],capture_output=True,text=True,timeout=35)
 (root/(name+'.stdout.json')).write_text(p.stdout);(root/(name+'.stderr.txt')).write_text(p.stderr)
 receipt=json.loads(p.stdout or p.stderr);receipts.append({'name':name,'exit_code':p.returncode,'wall_seconds':time.monotonic()-started,'receipt_path':str(root/(name+'.stdout.json'))})
 if p.returncode:raise RuntimeError(receipt)
 return receipt
def edit(name,operations,preview=False):
 file=root/(name+'.operations.json');file.write_text(json.dumps(operations));return call(name,'preview' if preview else 'edit',project,file)
def query(name):return call(name,'query',project)
def track(s):return next(t for t in s['tracks'] if t['id']=='microphone')
def playlist(t,pid):return next(p for p in t['playlists'] if p['id']==pid)
report={'status':'running','actual_microphone':True,'actual_model':False,'listening':False,'scope':'two sequential five-second physical input captures; ambient input, no performance-quality claim; explicit user-independent candidate selections','work':str(root)}
try:
 call('new','new',project,48000)
 edit('arm',[{'command':'add_audio_track','id':'microphone','name':'Physical microphone takes'},{'command':'set_track_arm','track_id':'microphone','record_armed':True}])
 before=query('before');first=track(before)['active_playlist_id']
 edit('name-first',[{'command':'rename_playlist','track_id':'microphone','playlist_id':first,'name':'Mic capture 01'}])
 captures=[]
 for n in (1,2):
  if n==2:edit('new-second',[{'command':'create_playlist','track_id':'microphone','id':'capture-02','name':'Mic capture 02'},{'command':'select_playlist','track_id':'microphone','playlist_id':'capture-02'}])
  captured=call(f'capture-{n}','record-armed',project,5000,0);captures.append(captured)
  assert captured['capture']['status']=='recorded_and_verified' and captured['capture']['frames']>=48000*4
  assert captured['engine']['recording_gaps']==0 and captured['engine']['native_device']['fault']==0
  assert captured['device_close'].get('aggregate_cleanup_os_status',0)==0
  for f in captured['capture']['files']:
   assert hashlib.sha256(Path(f['path']).read_bytes()).hexdigest()==f['format']['sha256']
 after=query('after-recording');t=track(after);assert len(after['takes'])==2 and len(t['playlists'])==2
 files={s['id']:root/s['path'] for s in after['sources']};original_hashes={sid:hashlib.sha256(p.read_bytes()).hexdigest() for sid,p in files.items()}
 first_clip=playlist(t,first)['clips'][0];second_clip=playlist(t,'capture-02')['clips'][0]
 ops=[{'command':'create_comp_playlist','track_id':'microphone','id':'candidate-comp','name':'Candidate Comp','segments':[{'source_playlist_id':first,'clip_id':first_clip['id'],'begin':0,'end':48000,'fade_in':240,'fade_out':240,'new_clip_id':'candidate-first'},{'source_playlist_id':'capture-02','clip_id':second_clip['id'],'begin':48000,'end':96000,'fade_in':240,'fade_out':240,'new_clip_id':'candidate-second'}]}]
 edit('comp-preview',ops,True);assert query('preview-did-not-edit')==after
 edit('comp-commit',ops);inactive=query('comp-inactive');assert track(inactive)['active_playlist_id']=='capture-02' and inactive['takes']==after['takes']
 edit('activate',[{'command':'set_track_arm','track_id':'microphone','record_armed':False},{'command':'set_target_playlist','track_id':'microphone','playlist_id':'candidate-comp'},{'command':'select_playlist','track_id':'microphone','playlist_id':'candidate-comp'}])
 selected=query('selected');render=call('render','render',project,root/'candidate.wav',0,96000);assert render['format']['frames']==96000 and render['format']['channels']==2 and render['format']['floating_pcm']
 assert query('reopened')==selected
 report.update(status='passed',project=str(project),captures=captures,candidate_render=render,original_media_hashes=original_hashes,source_playlists_retained=playlist(track(selected),first)==playlist(t,first) and playlist(track(selected),'capture-02')==playlist(t,'capture-02'),stable_take_objects=selected['takes']==after['takes'])
 assert report['source_playlists_retained'] and report['stable_take_objects'] and all(hashlib.sha256(files[sid].read_bytes()).hexdigest()==h for sid,h in original_hashes.items())
except Exception as e:
 report.update(status='failed',error=str(e));raise
finally:
 report['operations']=receipts;(root/'result.json').write_text(json.dumps(report,indent=2));(repo/'evidence/playlist-comp/physical-workflow.json').write_text(json.dumps(report,indent=2));print(json.dumps({'status':report['status'],'work':str(root),'error':report.get('error')},indent=2))
