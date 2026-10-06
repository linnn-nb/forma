#!/usr/bin/env python3
"""Actual CoreAudio output + domain edits, using isolated copies of the vertical project.

No model, plugin, microphone or acoustic loopback is substituted by this test.
The budgets are declared in docs/VERIFICATION.md before its first execution.
"""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import time
import uuid
import argparse

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--prefix', default='realtime-controls', help='distinct evidence filename prefix; preserves older iteration receipts')
parser.add_argument('--cli', type=Path, help='explicit build binary; supports the separate test-only RT audit build')
parser.add_argument('--require-native', action='store_true')
parser.add_argument('--require-audit', action='store_true')
parser.add_argument('--routing', action='store_true', help='insert actual Aux/send/master/limiter via domain commands before each physical run')
args = parser.parse_args()
prefix = args.prefix
if not prefix or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_' for c in prefix):
    raise SystemExit('Invalid evidence prefix')
cli = args.cli.resolve() if args.cli else root / 'build/ndaw_artefacts/Release/ndaw'
original = Path((root / 'evidence/current-test-project.txt').read_text().strip())
run = root / 'evidence/runs' / (prefix + '-' + str(uuid.uuid4()))
run.mkdir(parents=True)
media_hashes = {str(p.relative_to(original.parent)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in (original.parent / 'media').iterdir() if p.is_file()}
receipts = []
for buffer in (64, 128, 256, 512):
    directory = run / str(buffer)
    directory.mkdir()
    project = directory / 'session.ndaw'
    shutil.copy2(original, project)
    shutil.copytree(original.parent / 'media', directory / 'media')
    routing_receipt = None
    if args.routing:
        queried = json.loads(subprocess.run([str(cli), 'query', str(project)], capture_output=True, text=True, check=True).stdout)
        track = next(t for t in queried['tracks'] if t['kind'] == 'audio')
        aux, bus, master = (uuid.uuid4().hex for _ in range(3))
        operations = [
            {'command': 'add_aux_track', 'name': 'Parallel limiter Aux', 'id': aux, 'bus_id': bus},
            {'command': 'add_master_track', 'name': 'Main master', 'id': master},
            {'command': 'add_send', 'track_id': track['id'], 'target_bus_id': bus, 'gain_db': -18},
            {'command': 'insert_limiter', 'track_id': aux, 'lookahead_frames': 64, 'ceiling_db': -1, 'release_ms': 100},
            {'command': 'set_track_gain', 'track_id': master, 'gain_db': -6}]
        op_path = directory / 'routing-operations.json'
        op_path.write_text(json.dumps(operations, indent=2))
        committed = json.loads(subprocess.run([str(cli), 'edit', str(project), str(op_path)], capture_output=True, text=True, check=True).stdout)
        reopened = json.loads(subprocess.run([str(cli), 'query', str(project)], capture_output=True, text=True, check=True).stdout)
        assert reopened['tracks'][0]['output'] == track['output']
        facts = json.loads(subprocess.run([str(cli), 'routing', str(project)], capture_output=True, text=True, check=True).stdout)
        assert facts['algorithmic_latency_frames'] == 64
        export = json.loads(subprocess.run([str(cli), 'render', str(project), str(directory / 'routing-verified.wav'), '0', '240000'],
                                          capture_output=True, text=True, check=True, timeout=30).stdout)
        assert export['status'] == 'exported_and_verified' and export['format']['frames'] == 240000
        routing_receipt = {'transaction': committed, 'saved_reopened_revision': reopened['revision'],
                           'original_output_retained': True, 'facts': facts, 'real_file_export': export}
    start = time.monotonic()
    process = subprocess.run([str(cli), 'device-edit-play', str(project), '5000', str(buffer)],
                             capture_output=True, text=True, timeout=30)
    try:
        result = json.loads(process.stdout or process.stderr)
    except ValueError:
        result = {'status': 'failed', 'stdout': process.stdout, 'stderr': process.stderr}
    result['process_exit_code'] = process.returncode
    result['process_wall_ms'] = (time.monotonic() - start) * 1000
    result['requested_device_buffer'] = buffer
    result['test_project'] = str(project)
    if routing_receipt is not None:
        result['routing_preparation'] = routing_receipt
        result['actual_processing_latency_frames'] = 64
    native = result.get('native_device', {})
    counts = native.get('full_ioproc_histogram', result.get('callback_histogram', []))
    total = sum(counts)
    index, accumulated = 0, 0
    for index, count in enumerate(counts):
        accumulated += count
        if accumulated >= total * 0.99:
            break
    bound = [10, 25, 50, 100, 250, 500, 1000, None][index] if counts else None
    result['callback_p99_upper_bin_us'] = bound
    result['timing_boundary'] = native.get('scope', 'NativeDAW engine body only; excludes device wrapper')
    result['binary'] = str(cli)
    result['predeclared_budgets'] = {'wall_ms': 5000, 'callback_p99_upper_bin_us': 500,
                                    'deadline_miss': 0, 'playback_queue_underruns': 0,
                                    'graph_prepare_max_ms': 2000, 'commit_to_pcm_ms': 50,
                                    'publication_to_pcm_ms': 20, 'gain_ramp_frames': 240}
    result['control_budgets_met'] = len(result.get('edits', [])) == 2 and all(
        0 <= e.get('commit_to_pcm_ms', float('inf')) <= 50 and
        0 <= e.get('publication_to_pcm_ms', float('inf')) <= 20 and
        e.get('gain_ramp_settled_at_sample', -1) - e.get('pcm_applied_at_sample', 0) == 240
        for e in result.get('edits', []))
    result['passed'] = (result['control_budgets_met'] and process.returncode == 0 and
                        result.get('status') == 'device_playback_with_domain_edits_verified' and
                        result.get('executed_wall_ms', 0) >= 5000 and
                        result.get('sample_rate') == 48000 and result.get('buffer_size') == buffer and
                        all(result.get(k, -1) == 0 for k in ('deadline_miss','playback_queue_underruns','recording_gaps')) and
                        bound is not None and bound <= 500 and
                        result.get('graph_prepare_max_ms', float('inf')) <= 2000)
    if args.require_native:
        closed = result.get('device_close', {})
        result['native_budgets_met'] = (result.get('backend') == 'CoreAudio-Native' and
            native.get('full_ioproc_callbacks', 0) > 0 and
            all(native.get(k, -1) == 0 for k in ('fault', 'full_ioproc_deadline_miss', 'timestamp_discontinuities', 'missing_sample_timestamps', 'driver_overloads')) and
            all(closed.get(k, -1) == 0 for k in ('fault', 'active_callbacks', 'stop_os_status', 'aggregate_cleanup_os_status', 'global_retained_bytes')) and
            closed.get('callback_owner_wait_timed_out') is False and
            closed.get('quarantined_callback_storage') is False)
        result['passed'] &= result['native_budgets_met']
    if args.require_audit:
        audit = native.get('rt_audit', {})
        result['audit_budgets_met'] = (audit.get('scopes', 0) > 0 and all(audit.get(k, -1) == 0 for k in
            ('allocation', 'free', 'blocking_lock_wait', 'file_network', 'device_property_control')))
        result['passed'] &= result['audit_budgets_met']
    result['original_media_unchanged'] = all(hashlib.sha256((original.parent / p).read_bytes()).hexdigest() == h
                                             for p, h in media_hashes.items())
    result['passed'] = result['passed'] and result['original_media_unchanged']
    if args.routing:
        result['passed'] &= (result.get('processing_latency_frames') == 64 and
                             result.get('graph_failures') == 0 and
                             any(m.get('id') == aux and m.get('post_fader_sample_peak', 0) > 0
                                 for m in result.get('routing', {}).get('meters', [])))
    receipts.append(result)
    (root / 'evidence' / f'{prefix}-{buffer}.json').write_text(json.dumps(result, indent=2))
report = {'status': 'passed' if all(r['passed'] for r in receipts) else 'failed',
          'actual_coreaudio': True, 'real_model': False, 'acoustic_listening': False,
          'native_required': args.require_native, 'process_rt_audit_required': args.require_audit,
          'actual_aux_send_limiter_routing': args.routing,
          'scope': 'short output/graph-update tests only; no endurance, monitoring RTT or Pro Tools comparison',
          'receipts': receipts}
(root / 'evidence' / f'{prefix}-summary.json').write_text(json.dumps(report, indent=2))
print(json.dumps({'status': report['status'], 'buffers': [r['requested_device_buffer'] for r in receipts],
                  'callbacks': [r.get('callbacks') for r in receipts],
                  'max_callback_us': [r.get('callback_max_us') for r in receipts],
                  'native_max_us': [r.get('native_device', {}).get('full_ioproc_max_us') for r in receipts],
                  'p99_upper_bin_us': [r.get('callback_p99_upper_bin_us') for r in receipts],
                  'edit_commit_to_pcm_ms': [[e.get('commit_to_pcm_ms') for e in r.get('edits', [])] for r in receipts]}, indent=2))
raise SystemExit(0 if report['status'] == 'passed' else 1)
