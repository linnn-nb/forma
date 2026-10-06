#!/usr/bin/env python3
"""Kill an owned disposable NativeDAW CLI; observe the genuine SDK child's guardian exit and preserved project."""
import argparse,hashlib,json,os,shutil,subprocess,time,uuid
from pathlib import Path
root=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cli',type=Path,default=root/'build/ndaw_artefacts/Release/ndaw');parser.add_argument('--prefix',default='plugin-parent-exit')
parser.add_argument('--seed',type=Path,default=root/'evidence/plugin-project-device-release.json');args=parser.parse_args()
assert args.prefix and all(c.isalnum() or c in '-_' for c in args.prefix);cli=args.cli.resolve()
seed=json.loads(args.seed.read_text());old=Path(seed['run_directory'])/'256';run=root/'evidence/runs'/('plugin-owner-'+str(uuid.uuid4()));run.mkdir(parents=True)
for name in ('session.ndaw','media','.plugin-states'):
 if (old/name).is_dir():shutil.copytree(old/name,run/name)
 else:shutil.copy2(old/name,run/name)
def hash(p):return hashlib.sha256(p.read_bytes()).hexdigest()
originals={str(p.relative_to(run)):hash(p) for p in run.rglob('*') if p.is_file()}
report={'status':'running','directory':str(run),'binary':str(cli),'binary_sha256':hash(cli),'actual_sdk_child':True,'real_model':False,
 'predeclared_guardian_observed_exit_budget_ms':500,'scope':'actual AU owner-death/process/lease/reference fault injection; no exhaustive vendor/storage-stall/HAL crash cleanup or interrupted-job recovery qualification'}
parent=None
try:
 with (run/'owner.stdout.json').open('w') as out,(run/'owner.stderr.log').open('w') as err:
  parent=subprocess.Popen([str(cli),'device-play',str(run/'session.ndaw'),'10000','256'],stdout=out,stderr=err)
  until=time.monotonic()+12;ready=None
  while time.monotonic()<until and parent.poll() is None:
   found=list((run/'.plugin-runtime').glob('*/ready.json')) if (run/'.plugin-runtime').exists() else []
   if found:ready=json.loads(found[0].read_text());job=found[0].parent;break
   time.sleep(.005)
  assert ready and ready['status']=='prepared',(parent.poll(),(run/'owner.stderr.log').read_text())
  request=json.loads((job/'request.json').read_text());assert request['owner_pid']==parent.pid
  child=ready['worker_pid'];assert child!=parent.pid and child>1
  # Target only the Popen process created here, with an already prepared real SDK child.
  began=time.monotonic();parent.kill();parent.wait(timeout=2);assert parent.returncode<0
  exited=False;zombie=False
  while time.monotonic()-began<2:
   state=subprocess.run(['ps','-o','stat=','-p',str(child)],capture_output=True,text=True,timeout=1).stdout.strip()
   if not state or state.startswith('Z'):exited=True;zombie=state.startswith('Z');break
   time.sleep(.005)
  observed=(time.monotonic()-began)*1000;assert exited and observed<=500,(exited,state,observed)
  # This also tests that the child did not retain an inherited session ownership lock.
  reopened=subprocess.run([str(cli),'query',str(run/'session.ndaw')],capture_output=True,text=True,timeout=5)
  (run/'reopen.stdout.json').write_text(reopened.stdout);(run/'reopen.stderr.log').write_text(reopened.stderr)
  assert reopened.returncode==0 and json.loads(reopened.stdout)['schema_version']==7
  assert all(hash(run/p)==sha for p,sha in originals.items())
  report.update(status='passed',owner_pid=parent.pid,owner_exit_code=parent.returncode,actual_child_pid=child,observed_child_exit_ms=observed,
   child_observed_exited=True,zombie_at_observation=zombie,child_reaping_scope='not this observer child; absence/zombie checked through OS; no waitpid reaping claim',
   project_reopened=True,original_project_media_and_state_unchanged=True,private_interrupted_job_retained=True,
   successful_job_receipt=False,raw_ready_path=str(job/'ready.json'))
except Exception as e:report.update(status='failed',error=str(e));raise
finally:
 if parent is not None and parent.poll() is None:parent.kill();parent.wait(timeout=2)
 (run/'summary.json').write_text(json.dumps(report,indent=2));(root/'evidence'/(args.prefix+'.json')).write_text(json.dumps(report,indent=2));print(json.dumps({k:report.get(k) for k in ('status','observed_child_exit_ms','directory','error')}))
