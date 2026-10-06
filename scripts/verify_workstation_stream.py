#!/usr/bin/env python3
"""Bounded real CoreAudio/music/AU verification of the Edit/Mix source workload."""
import argparse, datetime, hashlib, json, shutil, subprocess, uuid
from pathlib import Path

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('session', type=Path)
p.add_argument('--cli', type=Path, default=root/'build/ndaw_artefacts/Release/ndaw')
p.add_argument('--prefix', default='workstation-stream-release')
p.add_argument('--buffers', type=int, nargs='+', default=[64,128,256,512])
p.add_argument('--require-audit', action='store_true')
a = p.parse_args()
assert a.prefix and all(c.isalnum() or c in '-_' for c in a.prefix)
assert all(n in (64,128,256,512) for n in a.buffers)
cli = a.cli.resolve(); original = a.session.resolve()
run = root/'evidence/runs'/('workstation-stream-'+uuid.uuid4().hex)
run.mkdir(parents=True)
project = run/'session.ndaw'
shutil.copy2(original, project)
for folder in ('media', '.plugin-states'):
    if (original.parent/folder).exists(): shutil.copytree(original.parent/folder, run/folder)
def hashes(folder):
    return {str(f.relative_to(folder)):hashlib.sha256(f.read_bytes()).hexdigest()
            for f in sorted(folder.rglob('*')) if f.is_file()}
media_before = hashes(run/'media')
summary = {'status':'running','verified_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
           'binary_sha256':hashlib.sha256(cli.read_bytes()).hexdigest(),'checks':[],
           'actual_model':False,'actual_music':True,'actual_plugin':True,'listening':False,
           'budget':'evidence/ui-refactor/budget-declared.json',
           'scope':'six arrangements of one real source, one genuine Apple AU, stereo main; short warm-disk runs, not endurance/Pro Tools parity'}
try:
    for buffer in a.buffers:
        result = subprocess.run([str(cli),'device-play',str(project),'20000',str(buffer)],capture_output=True,text=True,timeout=35)
        receipt = run/f'{buffer}.json'; receipt.write_text(result.stdout)
        (run/f'{buffer}.stderr.txt').write_text(result.stderr)
        j = json.loads(result.stdout)
        assert result.returncode == 0 and j['status']=='device_callback_run', (buffer,j.get('error'))
        assert j['playback_state']==0 and j['content_presentation_position_samples']==864000
        assert j['executed_wall_ms']>=20000 and j['maximum_output_peak']>0 and j['error']==''
        assert all(j[k]==0 for k in ('playback_queue_underruns','deadline_miss','graph_failures','driver_xruns'))
        native = j['native_device']; period = buffer/48000*1e6
        assert all(native[k]==0 for k in ('fault','driver_overloads','full_ioproc_deadline_miss','timestamp_discontinuities'))
        counts = native['full_ioproc_histogram']; total = sum(counts); cumulative=0; p99=None
        for count,bound in zip(counts,(10,25,50,100,250,500,1000,None)):
            cumulative+=count
            if cumulative>=total*.99: p99=bound; break
        assert p99 is not None and p99<=period*.5 and native['full_ioproc_max_us']<=period*.9
        instances = j['routing']['plugins']['instances']; assert len(instances)==1
        fx = instances[0]; assert fx['fault']==0 and fx['deadline_misses']==0 and fx['nonzero_output_frames']>=12000
        retired = json.loads((Path(fx['job_directory'])/'retirement.json').read_text())
        assert retired['retirement_status']=='verified_normal_teardown' and retired['reservation_released']
        assert retired['process_exit']['reaped'] and retired['process_exit']['exit_code']==0
        if a.require_audit:
            audit = native['rt_audit']; assert audit['scopes']>0
            assert all(audit[k]==0 for k in ('allocation','free','blocking_lock_wait','file_network','device_property_control'))
        summary['checks'].append({'buffer':buffer,'passed':True,'wall_ms':j['executed_wall_ms'],
            'callbacks':native['full_ioproc_callbacks'],'native_max_us':native['full_ioproc_max_us'],
            'native_p99_bound_us':p99,'underruns':0,'deadline_miss':0,'source_cache_bytes':j['source_cache_bytes'],
            'content_frames':j['content_presentation_position_samples'],'actual_sdk_nonzero_frames':fx['nonzero_output_frames'],
            'normal_teardown':True,'rt_audit':native.get('rt_audit'),'receipt':str(receipt)})
    assert hashes(run/'media')==media_before
    assert all(hashlib.sha256((original.parent/'media'/f).read_bytes()).hexdigest()==h for f,h in media_before.items())
    summary.update(status='verified',original_media_unchanged=True,media_sha256=media_before)
except Exception as e:
    summary.update(status='failed',error=str(e)); raise
finally:
    (root/'evidence/ui-refactor'/f'{a.prefix}.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))
