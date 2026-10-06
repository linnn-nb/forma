#!/usr/bin/env python3
"""Run the real file/CLI vertical workflow on a user-selected local audio file."""
from pathlib import Path
import sys,json,subprocess,uuid,hashlib,time

def active_clips(track):
 return next(p["clips"] for p in track["playlists"] if p["id"] == track["active_playlist_id"]) if track["kind"] == "audio" else []

repo=Path(__file__).resolve().parent.parent
if len(sys.argv)!=2:raise SystemExit('Usage: verify_vertical.py /absolute/path/to/real-48k-mono-or-stereo.wav')
source=Path(sys.argv[1]).resolve()
cli=repo/'build/ndaw_artefacts/Release/ndaw'
work=repo/'evidence/runs'/('vertical-'+str(uuid.uuid4()))
work.mkdir(parents=True)
project=work/'session.ndaw'
reports=[]
def run(*args,allow_failure=False,env=None):
 start=time.monotonic();p=subprocess.run([str(cli),*map(str,args)],capture_output=True,text=True,env=env,timeout=65)
 out=p.stdout or p.stderr
 try:j=json.loads(out)
 except ValueError:j={'raw_output':out}
 reports.append({'command':str(args[0]),'exit_code':p.returncode,'wall_seconds':time.monotonic()-start,'receipt':j})
 if p.returncode and not allow_failure:raise RuntimeError(out)
 return j
original=hashlib.sha256(source.read_bytes()).hexdigest()
run('new',project,48000)
track=str(uuid.uuid4());clip=str(uuid.uuid4());sid=str(uuid.uuid4())
ops=[{'command':'add_audio_track','name':'Local production audio','id':track},
 {'command':'import_audio','track_id':track,'path':str(source),'position':0,'source_id':sid,'clip_id':clip},
 {'command':'set_track_gain','track_id':track,'gain_db':-18}]
ops_file=work/'import-commands.json';ops_file.write_text(json.dumps(ops))
run('preview',project,ops_file);run('edit',project,ops_file)
base=run('query',project)
move_file=work/'move-commands.json';move_file.write_text(json.dumps([{'command':'move_clip','clip_id':clip,'position':480}]))
run('edit',project,move_file);run('undo',project)
restored=run('query',project)
assert restored['tracks']==base['tracks'] and restored['sources']==base['sources']
run('redo',project)
final=run('query',project)
assert active_clips(final['tracks'][0])[0]['start']==480
run('render',project,work/'verified-mix.wav',0,96000)
assert hashlib.sha256(source.read_bytes()).hexdigest()==original
report={'status':'verified_file_cli_vertical_slice','original_media_hash_unchanged':original,'source_local_only':str(source),
 'project':str(project),'export':str(work/'verified-mix.wav'),'workflow':reports,
 'gui_acceptance':'separate native UI execution required','live_model_e2e':'blocked until a real Provider/model is configured',
 'production_ready':False}
(repo/'evidence/vertical-workflow.json').write_text(json.dumps(report,ensure_ascii=False,indent=2))
(repo/'evidence/current-test-project.txt').write_text(str(project)+'\n')
print(json.dumps({'status':report['status'],'project':str(project),'export':report['export']},ensure_ascii=False,indent=2))
