#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Six bounded, real-device diagnostic conditions; not a release acceptance test."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session', required=True, type=Path)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path, help='new or empty evidence directory')
    parser.add_argument('--headphones', required=True, help='exact native output device name')
    parser.add_argument('--speakers', required=True, help='exact native output device name')
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('this CoreAudio diagnostic requires macOS')
    if not args.session.is_file() or not args.executable.is_file():
        parser.error('actual session and built diagnostic executable are required')
    if args.output.exists() and any(args.output.iterdir()):
        parser.error('output directory must be empty; earlier results are preserved')
    args.output.mkdir(parents=True, exist_ok=True)
    variants = [
        ('hp-edit', 'edit', args.headphones, 0, 0),
        ('hp-hidden', 'hidden', args.headphones, 0, 0),
        ('sp-hidden', 'hidden', args.speakers, 0, 0),
        ('sp-edit', 'edit', args.speakers, 0, 0),
        ('hp-load', 'edit', args.headphones, 4, 0),
        ('hp-load-wg', 'edit', args.headphones, 4, 1),
    ]
    results = []
    for name, mode, device, count, workgroup in variants:
        print('START', name, flush=True)
        workers, samples, proc = [], [], None
        report = args.output / (name + '.json')
        started = time.monotonic()
        try:
            for _ in range(count):
                workers.append(subprocess.Popen([
                    sys.executable, '-c',
                    'import time\nend=time.monotonic()+240\nx=1\n'
                    'while time.monotonic()<end:\n x=(x*1664525+1013904223)&0xffffffff\n',
                ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
            command = [str(args.executable.resolve()), str(report.resolve()), str(args.session.resolve()),
                       '90', mode, device, '256', str(workgroup)]
            with (args.output / (name + '.log')).open('w') as log:
                proc = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
                next_sample = 0
                while proc.poll() is None:
                    if time.monotonic() - started > 180:
                        raise RuntimeError('condition exceeded its 180 second process limit')
                    if time.monotonic() >= next_sample:
                        output = subprocess.run(['ps', '-axo', 'pid=,%cpu=,comm='], capture_output=True,
                                                text=True, timeout=3).stdout
                        rows = []
                        owned = [w.pid for w in workers] + [proc.pid]
                        for line in output.splitlines():
                            fields = line.strip().split(None, 2)
                            if len(fields) != 3:
                                continue
                            pid, cpu, command_name = int(fields[0]), float(fields[1]), fields[2]
                            if cpu < 1 and pid not in owned:
                                continue
                            if pid in owned or any(s in command_name for s in [
                                    'StorageManagement', 'ApplicationsStorageExtension', 'mds', 'mdworker', 'backupd']):
                                rows.append({'pid': pid, 'cpu_pct': cpu, 'command': command_name})
                        samples.append({'process_elapsed_s': time.monotonic() - started, 'rows': rows})
                        next_sample = time.monotonic() + 10
                    time.sleep(.25)
            item = {'name': name, 'exit': proc.returncode, 'load_workers': count,
                    'workgroup_requested': bool(workgroup), 'elapsed_s': time.monotonic() - started,
                    'load_samples': samples}
            if report.exists():
                data = json.loads(report.read_text())
                item['report'] = data
                print('DONE', name, 'exit', proc.returncode, 'xruns', data['xruns_after'] - data['xruns_before'],
                      'main_cpu', round(data['main_cpu_pct'], 2), 'host_gaps',
                      data['callback_spacing']['host_gap_gt_1_5_period'], 'passive',
                      data.get('passive_playback_condition', 'not recorded'), flush=True)
            else:
                print('DONE', name, 'exit', proc.returncode, 'NO REPORT', flush=True)
            results.append(item)
        except Exception as error:
            results.append({'name': name, 'error': str(error), 'load_samples': samples})
            print('ERROR', name, str(error), flush=True)
        finally:
            # Only processes created by this condition are touched. No system service is stopped.
            for child in ([proc] if proc is not None else []) + workers:
                if child.poll() is None:
                    child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()
            (args.output / 'summary.json').write_text(json.dumps(results, ensure_ascii=False, indent=2))
    # Exit status covers execution/condition integrity, not the zero-XRUN/FPS release budget.
    if not all(x.get('exit') == 0 and 'report' in x for x in results):
        return 1
    if not all(x['report'].get('passive_playback_condition') is True and
               x['report']['device_unchanged'] and x['report']['callback_spacing']['events_dropped'] == 0
               for x in results):
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
