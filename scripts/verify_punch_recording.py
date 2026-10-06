#!/usr/bin/env python3
"""Actual microphone selection Punch; all work stays in a fresh owned project.
Two logical tracks share one actual input. No physical multi-input, listening,
GUI permission, real-model or Pro Tools performance acceptance is implied.
"""
from pathlib import Path
import hashlib, json, subprocess, sys, time, uuid
from verify_playlist_comp_pcm import wav, verify

repo=Path(__file__).resolve().parent.parent
cli=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else repo/'build/ndaw_artefacts/Release/ndaw'
root=repo/'evidence/punch-recording'/('physical-'+str(uuid.uuid4()))
root.mkdir(parents=True)
project=root/'session.ndaw'; operations=[]
begin,end,pre,post=48037,144119,48000,48011
start,stop,total=begin-pre,end+post,end+post-(begin-pre)

def call(name,*args):
    launched=time.monotonic()
    p=subprocess.run([str(cli),*map(str,args)],capture_output=True,text=True,timeout=35)
    (root/(name+'.stdout.json')).write_text(p.stdout)
    (root/(name+'.stderr.txt')).write_text(p.stderr)
    result=json.loads(p.stdout or p.stderr)
    operations.append({'name':name,'exit_code':p.returncode,'seconds':time.monotonic()-launched})
    if p.returncode: raise RuntimeError(result)
    return result

def edit(name,ops):
    file=root/(name+'.operations.json'); file.write_text(json.dumps(ops))
    return call(name,'edit',project,file)

def query(name): return call(name,'query',project)

def same_state(a,b):
    copy=dict(a); copy['revision']=b['revision']; assert copy==b

report={'status':'running','actual_device':True,'physical_microphone':True,
        'physical_input_count':1,'logical_track_count':2,'actual_model':False,'listening':False,
        'scope':'48k/256 exact selection Punch with pre/post handles, independent 1e-7 PCM; no endurance/RTT/GUI/performance parity',
        'work':str(root),'cli':str(cli),'cli_sha256':hashlib.sha256(cli.read_bytes()).hexdigest(),
        'capture_range':[start,stop],'punch_range':[begin,end]}
try:
    prior=json.loads((repo/'evidence/loop-recording/physical-release.json').read_text())
    original=Path(prior['capture']['capture']['files'][0]['path'])
    original_hash=hashlib.sha256(original.read_bytes()).hexdigest()
    assert wav(original)[1]==216576
    call('new','new',project,48000)
    setup=[]
    for track in ('punch-a','punch-b'):
        setup.extend([{'command':'add_audio_track','id':track,'name':'Microphone '+track},
                      {'command':'import_audio','track_id':track,'path':str(original),'position':0},
                      {'command':'set_track_arm','track_id':track,'record_armed':True}])
    setup.append({'command':'set_record_mode','mode':'punch','begin':begin,'end':end,'pre_roll':pre,'post_roll':post})
    edit('setup',setup); before=query('before')
    result=call('capture','record-armed',project,5500,0)
    captured=result['capture']; after=query('after')
    assert captured['frames']==total and captured['timestamp']==start and len(captured['files'])==2
    assert len(after['takes'])==2 and len(after['sources'])==4
    metrics=result['engine']
    assert metrics['recording_gaps']==0 and metrics['deadline_miss']==0 and metrics['playback_queue_underruns']==0 and metrics['driver_xruns']==0
    assert metrics['native_device']['fault']==0 and metrics['native_device']['full_ioproc_deadline_miss']==0
    audit=metrics['native_device'].get('rt_audit')
    if audit:
        assert audit['scopes']>0 and all(audit[k]==0 for k in ('allocation','free','blocking_lock_wait','file_network','device_property_control'))
    assert result['device_close'].get('aggregate_cleanup_os_status',0)==0
    for old,track in zip(before['tracks'],after['tracks']):
        assert len(track['playlists'])==3 and track['playlists'][0]==old['playlists'][0]
        assert track['target_playlist_id']==old['target_playlist_id']
        raw=track['playlists'][1]['clips'][0]
        assert raw['start']==start and raw['source_start']==0 and raw['length']==total
        assert track['active_playlist_id']==track['playlists'][2]['id']
        clips=track['playlists'][2]['clips']
        assert len(clips)==3 and clips[0]['start']==0 and clips[0]['length']==begin
        assert clips[1]['start']==begin and clips[1]['length']==end-begin and clips[1]['source_start']==pre
        assert clips[2]['start']==end and clips[2]['length']==216576-end
    hashes={s['id']:s['sha256'] for s in after['sources']}
    snapshot=root/'punch-recorded.ndaw'
    snapshot.write_bytes(project.read_bytes())
    assert call('recorded-snapshot','query',snapshot)==after
    output=root/'punch-result.wav'
    call('render','render',project,output,0,216576)
    pcm=verify(after,root,output,0,216576)
    call('undo-recording','undo',project); undone=query('recording-undone'); same_state(undone,before)
    manifest=Path(captured['manifest'])
    assert call('retry-after-undo','attach-capture',project,manifest)==result['transaction']
    assert query('retry-no-change')==undone
    call('redo-recording','redo',project); same_state(query('reopened'),after)
    call('undo-for-human-edit','undo',project)
    old_clip=before['tracks'][0]['playlists'][0]['clips'][0]['id']
    edit('later-human-edit',[{'command':'rename_clip','clip_id':old_clip,'name':'Human edit after capture Undo'}])
    human=query('human-before-retry')
    assert call('retry-preserves-human','attach-capture',project,manifest)==result['transaction']
    assert query('human-after-retry')==human
    call('undo-human-edit','undo',project)
    same_state(query('final-original-playlists'),before)
    for source in after['sources']:
        assert hashlib.sha256((root/source['path']).read_bytes()).hexdigest()==hashes[source['id']]
    for file in captured['files']:
        assert hashlib.sha256(Path(file['path']).read_bytes()).hexdigest()==file['format']['sha256']
        assert wav(file['path'])[1]==total
    assert hashlib.sha256(original.read_bytes()).hexdigest()==original_hash
    report.update(status='passed',capture=result,frames=total,punch_frames=end-begin,
                  pcm_check=pcm,source_hashes=hashes,original_media_sha256=original_hash,
                  attachment_undo_exact=True,reopen_exact=True,
                  retry_after_undo_preserves_state=True,restart_retry_preserves_human_edit=True,
                  recorded_snapshot=str(snapshot),
                  final_state='Original Playlists restored; Punch media/Take history remains in transaction journal')
except Exception as error:
    report.update(status='failed',error=str(error)); raise
finally:
    report['operations']=operations
    (root/'result.json').write_text(json.dumps(report,indent=2))
    (repo/'evidence/punch-recording/physical-workflow.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({'status':report['status'],'work':str(root),'error':report.get('error')},indent=2))
