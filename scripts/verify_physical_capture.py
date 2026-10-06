#!/usr/bin/env python3
"""Actual default microphone, two logical mono tracks, no monitoring/test PCM.

Requires existing OS authorization. Never requests/grants/bypasses permission.
Recorded media stays in ignored evidence/runs and is never packaged/uploaded.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse, json, subprocess, time, uuid

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cli', type=Path)
parser.add_argument('--prefix', default='native-physical-input')
parser.add_argument('--routing', action='store_true', help='add actual dry/parallel Aux+lookahead/master routes; monitor remains off')
args = parser.parse_args()
assert args.prefix and all(c in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_' for c in args.prefix)
cli = args.cli.resolve() if args.cli else root / 'build/ndaw_artefacts/Release/ndaw'
work = root / 'evidence/runs' / (args.prefix + '-' + str(uuid.uuid4()))
work.mkdir(parents=True)
session = work / 'session.ndaw'
def call(*arguments):
    p = subprocess.run([str(cli), *map(str, arguments)], capture_output=True, text=True, timeout=20)
    return {'exit_code': p.returncode, 'receipt': json.loads(p.stdout or p.stderr)}
assert call('new', session, 48000)['exit_code'] == 0
ids = [str(uuid.uuid4()), str(uuid.uuid4())]
ops = [{'command': 'add_audio_track', 'id': t, 'name': f'Physical mic track {i+1}'} for i, t in enumerate(ids)]
for t in ids:
    ops += [{'command': 'set_track_input', 'track_id': t, 'input_channels': [0]},
            {'command': 'set_track_arm', 'track_id': t, 'record_armed': True},
            {'command': 'set_track_monitor', 'track_id': t, 'monitor_mode': 'off'}]
if args.routing:
    aux, bus, master = (uuid.uuid4().hex for _ in range(3))
    ops += [{'command': 'add_aux_track', 'id': aux, 'bus_id': bus, 'name': 'Recorded mic parallel Aux'},
            {'command': 'insert_limiter', 'track_id': aux, 'lookahead_frames': 64, 'ceiling_db': -1},
            {'command': 'add_master_track', 'id': master, 'name': 'Recorded mic main master'},
            {'command': 'set_track_gain', 'track_id': master, 'gain_db': -6}]
    ops += [{'command': 'add_send', 'track_id': t, 'target_bus_id': bus, 'gain_db': -18} for t in ids]
commands = work / 'commands.json'
commands.write_text(json.dumps(ops))
assert call('edit', session, commands)['exit_code'] == 0
began = time.monotonic()
process = subprocess.Popen([str(cli), 'record-armed', str(session), '5500'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
sizes = {}
while process.poll() is None and time.monotonic()-began < 20:
    for p in (work / 'recordings').glob('**/*.partial.wav'):
        try: size = p.stat().st_size
        except FileNotFoundError: continue
        values = sizes.setdefault(str(p), [])
        if not values or values[-1] != size: values.append(size)
    time.sleep(.025)
if process.poll() is None: process.kill()  # this new disposable test process only
stdout, stderr = process.communicate(timeout=5)
try: recording = json.loads(stdout or stderr)
except ValueError: recording = {'status': 'failed', 'stdout': stdout, 'stderr': stderr}
report = {'checked_utc': datetime.now(timezone.utc).isoformat(), 'binary': str(cli), 'project': str(session),
          'status': 'blocked_or_failed', 'actual_input_accepted': False, 'record_exit_code': process.returncode,
          'scope': 'actual default input, two logical tracks sharing exposed input ordinal 0; not two physical inputs; monitor off; no injected PCM',
          'permission_prompt_bypassed': False, 'recording': recording, 'progressive_partial_file_sizes': sizes,
          'record_wall_ms': (time.monotonic()-began)*1000, 'acoustic_listening': False, 'physical_rtt_calibrated': False}
report['actual_aux_send_limiter_routing'] = args.routing
if process.returncode == 0:
    try:
        capture, engine = recording['capture'], recording['engine']
        native, closed = engine['native_device'], recording['device_close']
        assert capture['frames'] >= 240000 and len(capture['files']) == 2
        assert all(f['format']['frames'] == capture['frames'] and f['physical_inputs'] == [0] for f in capture['files'])
        assert native['private_aggregate'] and any(s['enabled'] for s in native['input_stream_usage'])
        assert engine['recording_gaps'] == engine['deadline_miss'] == native['full_ioproc_deadline_miss'] == native['fault'] == native['timestamp_discontinuities'] == native['driver_overloads'] == 0
        assert all(closed[k] == 0 for k in ('fault','active_callbacks','stop_os_status','aggregate_cleanup_os_status','global_retained_bytes'))
        assert not closed['callback_owner_wait_timed_out']
        assert not closed['private_aggregate'] and not closed['quarantined_callback_storage']
        assert len(sizes) == 2 and all(len(v) >= 2 and max(v) > min(v) for v in sizes.values())
        audit = native.get('rt_audit', {})
        if audit: assert audit['scopes'] > 0 and all(audit[k] == 0 for k in ('allocation','free','blocking_lock_wait','file_network','device_property_control'))
        report['reopen'] = call('query', session)
        report['analysis'] = call('analyze', session)
        assert report['reopen']['exit_code'] == report['analysis']['exit_code'] == 0
        if args.routing:
            report['routing'] = call('routing', session)
            assert report['routing']['exit_code'] == 0 and report['routing']['receipt']['algorithmic_latency_frames'] == 64
            assert len(report['reopen']['receipt']['tracks']) == 4
        # Actual nonzero signal is a test gate, not a production rule rejecting
        # legitimate silence. Enabling flags alone cannot qualify microphone IO.
        assert all(a['peak'] > 1e-12 and a['rms'] > 1e-12 for a in report['analysis']['receipt']), 'Input is all zero; real microphone signal gate failed'
        report['export'] = call('render', session, work / 'verified-input-mix.wav', 0, capture['frames'])
        assert report['export']['exit_code'] == 0
        if args.routing:
            assert report['export']['receipt']['routing']['algorithmic_latency_frames'] == 64
        report['playback'] = call('device-play', session, 1500, 256)
        playback = report['playback']['receipt']
        assert report['playback']['exit_code'] == 0 and playback['status'] == 'device_callback_run'
        assert playback['maximum_output_peak'] > 0 and playback['playback_queue_underruns'] == playback['deadline_miss'] == 0
        if args.routing: assert playback['processing_latency_frames'] == 64
        assert playback['native_device']['fault'] == playback['device_close']['fault'] == 0 and not playback['device_close']['callback_owner_wait_timed_out']
        report.update(status='actual_input_capture_growth_reopen_export_verified', actual_input_accepted=True)
    except (AssertionError, KeyError) as e: report['failure'] = str(e) or 'Physical capture gate failed'
(root / 'evidence' / (args.prefix + '.json')).write_text(json.dumps(report, indent=2))
print(json.dumps({k: report[k] for k in ('status','actual_input_accepted','record_exit_code')}, indent=2))
raise SystemExit(0 if report['actual_input_accepted'] else 1)
