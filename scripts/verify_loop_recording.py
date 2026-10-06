#!/usr/bin/env python3
"""Actual default microphone loop capture; no synthetic input or model claims.
Two logical tracks share one physical microphone, not two physical inputs.
All takes are verified against the continuous source with independent RIFF PCM.
"""
from pathlib import Path
import json,subprocess,sys,uuid,hashlib,time
from verify_playlist_comp_pcm import wav,verify

repo=Path(__file__).resolve().parent.parent
cli=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else repo/'build/ndaw_artefacts/Release/ndaw'
root=repo/'evidence/loop-recording'/('physical-'+str(uuid.uuid4()));root.mkdir(parents=True)
project=root/'session.ndaw';operations=[]
def call(name,*args):
 start=time.monotonic();p=subprocess.run([str(cli),*map(str,args)],capture_output=True,text=True,timeout=35)
 (root/(name+'.stdout.json')).write_text(p.stdout);(root/(name+'.stderr.txt')).write_text(p.stderr)
 result=json.loads(p.stdout or p.stderr);operations.append({'name':name,'exit_code':p.returncode,'seconds':time.monotonic()-start})
 if p.returncode:raise RuntimeError(result)
 return result
def edit(name,ops):
 p=root/(name+'.operations.json');p.write_text(json.dumps(ops));return call(name,'edit',project,p)
def query(name):return call(name,'query',project)
report={'status':'running','actual_device':True,'physical_microphone':True,'physical_input_count':1,'logical_track_count':2,
        'actual_model':False,'listening':False,'scope':'4.5 second real input loop recording at 48k/256; fixed 1e-7 PCM tolerance; no endurance/RTT/performance parity','work':str(root)}
try:
 call('new','new',project,48000)
 edit('setup',[{'command':'add_audio_track','id':track,'name':'Microphone '+track} for track in ('capture-a','capture-b')]+
      [{'command':'set_track_arm','track_id':track,'record_armed':True} for track in ('capture-a','capture-b')]+
      [{'command':'set_record_mode','mode':'loop','begin':37,'end':48074}])
 before=query('before');result=call('capture','record-armed',project,4500,0);captured=result['capture'];after=query('after')
 total=captured['frames'];length=48037;full,partial=divmod(total,length);passes=full+bool(partial)
 assert total>=length*3 and len(after['takes'])==passes*2 and len(after['sources'])==2
 assert result['engine']['recording_gaps']==0 and result['engine']['deadline_miss']==0 and result['engine']['playback_queue_underruns']==0
 assert result['engine']['native_device']['fault']==0 and result['engine']['driver_xruns']==0
 assert result['engine']['native_device']['full_ioproc_deadline_miss']==0
 audit=result['engine']['native_device'].get('rt_audit')
 if audit:
  assert audit['scopes']>0 and all(audit[k]==0 for k in ('allocation','free','blocking_lock_wait','file_network','device_property_control'))
 assert result['device_close'].get('aggregate_cleanup_os_status',0)==0
 hashes={s['id']:s['sha256'] for s in after['sources']};pcm_checks=[]
 for prior,t in zip(before['tracks'],after['tracks']):
  assert t['playlists'][0]==prior['playlists'][0] and t['target_playlist_id']==prior['target_playlist_id'] and len(t['playlists'])==passes+1
  active=(passes if partial>length//2 else full) if partial else full
  assert t['active_playlist_id']==t['playlists'][active]['id']
  for index,p in enumerate(t['playlists'][1:]):
   c=p['clips'][0];assert c['start']==37 and c['source_start']==index*length and c['length']==min(length,total-index*length)
 for index in range(passes):
  edit(f'select-{index}',[{'command':'select_playlist','track_id':t['id'],'playlist_id':t['playlists'][index+1]['id']} for t in after['tracks']])
  selected=query(f'selected-{index}');end=37+min(length,total-index*length);output=root/f'take-{index+1}.wav'
  call(f'render-{index}','render',project,output,37,end);pcm_checks.append(verify(selected,root,output,37,end))
 # Original attachment Undo remains separately verified after returning to the
 # saved capture state, keeping every render/selection journal for inspection.
 for index in range(passes):call(f'undo-selection-{index}','undo',project)
 restored=query('selection-undone');copy=dict(restored);copy['revision']=after['revision'];assert copy==after
 call('undo-recording','undo',project);undone=query('recording-undone');copy=dict(undone);copy['revision']=before['revision'];assert copy==before
 manifest=Path(captured['manifest']);retry=call('retry-after-undo','attach-capture',project,manifest);assert retry==result['transaction'] and query('retry-no-change')==undone
 call('redo-recording','redo',project);reopened=query('reopened');copy=dict(reopened);copy['revision']=after['revision'];assert copy==after
 for s in after['sources']:assert hashlib.sha256((root/s['path']).read_bytes()).hexdigest()==hashes[s['id']]
 for f in captured['files']:
  assert hashlib.sha256(Path(f['path']).read_bytes()).hexdigest()==f['format']['sha256'];assert wav(f['path'])[1]==total
 report.update(status='passed',capture=result,frames=total,full_passes=full,partial_frames=partial,pcm_checks=pcm_checks,
               source_hashes=hashes,attachment_undo_exact=True,reopen_exact=True,retry_after_undo_preserves_state=True)
except Exception as e:
 report.update(status='failed',error=str(e));raise
finally:
 report['operations']=operations;(root/'result.json').write_text(json.dumps(report,indent=2))
 (repo/'evidence/loop-recording/physical-workflow.json').write_text(json.dumps(report,indent=2))
 print(json.dumps({'status':report['status'],'work':str(root),'error':report.get('error')},indent=2))
