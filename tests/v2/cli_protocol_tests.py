#!/usr/bin/env python3
"""Actual CLI children and independent PCM checks, never scraped combined output.

The channel probe is a test executable, not an audio or plugin substitute.
Optional existing projects are opened read-only; no audio or project is uploaded.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import time
import uuid
import wave


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cli', type=Path, required=True)
    parser.add_argument('--output-probe', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--project', type=Path, action='append', default=[])
    args = parser.parse_args()
    root = args.report.parent / ('CLI-Protocol-Test-' + uuid.uuid4().hex[:12])
    root.mkdir(parents=True)
    checks, cases, projects = [], [], []

    def check(ok, why):
        if not ok:
            raise AssertionError(why)
        checks.append(why)

    def call(name, params, code=0, tool=None, controlled_failure=False):
        began = time.monotonic()
        p = subprocess.run([str(tool or args.cli), *map(str, params)], stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, timeout=20)
        (root / (name + '.stdout')).write_bytes(p.stdout)
        (root / (name + '.stderr')).write_bytes(p.stderr)
        cases.append({'name': name, 'exit': p.returncode, 'stdout_bytes': len(p.stdout),
                      'stderr_bytes': len(p.stderr), 'wall_ms': (time.monotonic() - began) * 1000})
        check(p.returncode == code, name + ': actual process exit code')
        check(len(p.stdout) <= 16 * 1024 * 1024, name + ': fixture response within 16MiB')
        value = json.loads(p.stdout.decode('utf-8'),
                           parse_constant=lambda text: (_ for _ in ()).throw(ValueError(text)))
        check(isinstance(value, (dict, list)), name + ': stdout is exactly one strict UTF-8 JSON value')
        if controlled_failure:
            check(value['status'] == 'failed' and value['error'], name + ': actual failure receipt')
        return value, p.stderr

    def pcm(file):
        with wave.open(str(file), 'rb') as reader:
            check((reader.getnchannels(), reader.getsampwidth(), reader.getframerate(), reader.getnframes())
                  == (2, 3, 48000, 96000), file.name + ': independent actual PCM24 stereo format and duration')
            raw = reader.readframes(96000)
        check(len(raw) == 576000, file.name + ': complete actual WAV payload')
        return [int.from_bytes(raw[i:i + 3], 'little', signed=True) / 8388608.0
                for i in range(0, len(raw), 3)]

    args.report.write_text(json.dumps({'result': 'running'}))
    try:
        source = root / "Source 原音 'one'.wav"
        with wave.open(str(source), 'wb') as writer:
            writer.setparams((2, 2, 48000, 96000, 'NONE', 'not compressed'))
            writer.writeframes(b''.join(struct.pack('<hh', int(3000 * math.sin(i * 2 * math.pi * 440 / 48000)),
                                                   int(2000 * math.sin(i * 2 * math.pi * 997 / 48000)))
                                       for i in range(96000)))
        source_hash = digest(source)
        probe, diagnostics = call('probe', ['probe'])
        check(probe['tracks'] == [] and not probe['can_undo'], 'probe queries a real empty native Edit')
        check(b'Settings file:' in diagnostics and b'Cleaning up temp files' in diagnostics,
              'actual SDK startup and final Engine teardown logs are on stderr')
        default, _ = call('default', [])
        check(default['tracks'] == [], 'no-argument probe remains compatible')
        commands, _ = call('commands', ['commands'])
        check(any(c['id'] == 'track.create' for c in commands), 'real registry keeps its array schema')
        for name, params, code in [
            ('missing', ['inspect', root / 'missing.tracktionedit'], 1),
            ('usage', ['unknown-command'], 2),
            ('wrong-arity', ['inspect'], 2),
            ('extra-argument', ['commands', 'ignored-before'], 2),
            ('invalid-scan-flag', ['plugin-scan', 'VST3', 'missing', '--ignored-before'], 2),
            ('analysis-missing', ['analyse', root / 'missing.wav'], 1),
            ('invalid-scan-format', ['plugin-scan', 'INVALID', root / 'missing.vst3'], 1),
        ]:
            call(name, params, code, controlled_failure=True)
        corrupt = root / 'Broken.tracktionedit'
        corrupt.write_text('<EDIT><broken')
        call('broken-xml', ['inspect', corrupt], 1, controlled_failure=True)
        for text in ['nan', 'inf', '-inf', '0trailing', 'not-a-number']:
            output = root / ('bad-gain-' + str(len(cases)) + '.wav')
            call('gain-' + str(len(cases)), ['render', source, output, text], 1, controlled_failure=True)
            check(not output.exists(), 'invalid gain does not publish an audio file')
        analysis, _ = call('analysis', ['analyse', source])
        check(analysis['frames'] == 96000 and analysis['channels'] == 2 and analysis['peak'] > 0,
              'real analyser reports independently known frames/channels and nonzero signal')
        outputs = []
        for gain in [0, -6]:
            file = root / ('Render ' + str(gain) + '.wav')
            result, _ = call('render-' + str(gain), ['render', source, file, str(gain)])
            check(result['receipt']['state'] == 'committed' and result['render']['audio_verified']
                  and Path(result['render']['path']) == file, 'actual L1 and verified file receipts returned')
            outputs.append(pcm(file))
        error = max(abs(a * 10 ** (-6 / 20) - b) for a, b in zip(*outputs))
        check(error <= 2e-6 and max(abs(v) for v in outputs[0]) > .001,
              'all actual stereo samples follow -6dB within fixed 2e-6 tolerance')
        existing_wav = root / 'Render 0.wav'
        wav_hash = digest(existing_wav)
        call('output-exists', ['render', source, existing_wav, '0'], 1, controlled_failure=True)
        check(digest(existing_wav) == wav_hash, 'refused render preserves existing file bytes')
        # Actual v1 import through L1; integers avoid canonical float-print ambiguity.
        session = {'schema_version': 1, 'id': 'cli-legacy', 'revision': 0, 'name': 'CLI 原生导入',
                   'sample_rate': 48000,
                   'sources': [{'id': 'source', 'path': source.name, 'sha256': source_hash,
                                'sample_rate': 48000, 'frames': 96000, 'channels': 2, 'file_bits': 16,
                                'floating_pcm': False, 'original_name': source.name}],
                   'tracks': [{'id': 'vocal', 'kind': 'audio', 'name': 'Voice', 'locked': False,
                               'muted': False, 'gain_db': -6, 'pan': 0,
                               'clips': [{'id': 'clip', 'name': 'Source', 'source_id': 'source', 'start': 0,
                                          'source_start': 0, 'length': 96000, 'gain_db': 0, 'fade_in': 0,
                                          'fade_out': 0, 'locked': False}]}],
                   'markers': [], 'analysis': [], 'provenance': []}
        data = json.dumps(session, ensure_ascii=False, separators=(',', ':'), sort_keys=True)
        legacy = root / 'Legacy.ndaw'
        legacy.write_text(json.dumps({'format': 'NativeDAW', 'checksum': hashlib.sha256(data.encode()).hexdigest(),
                                      'session': session}, ensure_ascii=False))
        legacy_hash = digest(legacy)
        preview, _ = call('legacy-preview', ['legacy-preview', legacy])
        check(isinstance(preview, dict), 'legacy preview returns actual structured Plan evidence')
        project = root / 'Imported.tracktionedit'
        imported, _ = call('legacy-import', ['legacy-import', legacy, project])
        check(imported['receipt']['state'] == 'committed' and imported['save']['sha256'] == digest(project),
              'actual import commits and hashes a real saved project')
        inspected, _ = call('inspect', ['inspect', project])
        check(inspected['tracks'] and inspected['can_undo'], 'real saved schema3 project cold-opens with original Undo')
        call('legacy-existing', ['legacy-import', legacy, project], 1, controlled_failure=True)
        check(digest(legacy) == legacy_hash and digest(source) == source_hash,
              'all CLI operations preserve original legacy document and source audio')
        for i, project in enumerate(args.project):
            original = digest(project)
            value, _ = call('external-project-' + str(i), ['inspect', project])
            loaded = [p for t in value['tracks'] for p in t['plugins'] if p.get('external', {}).get('loaded')]
            check(loaded, 'optional real project instantiates actual external plugins')
            check(digest(project) == original, 'existing external-plugin project is not overwritten')
            projects.append({'sha256': original, 'tracks': len(value['tracks']), 'loaded_plugins': len(loaded)})
        if args.output_probe:
            for mode, code in [('normal', 0), ('missing', 1), ('mismatch', 3)]:
                _, stderr = call('channel-' + mode, [mode], code, tool=args.output_probe, controlled_failure=bool(code))
                for text in [b'test C++ ostream log', b'test C stdio log', b'test teardown printf log', b'test atexit printf log']:
                    check(text in stderr, 'channel redirects actual log: ' + text.decode())
                if os.name != 'nt':
                    check(b'test raw fd log' in stderr and b'test inherited stdout child log' in stderr,
                          'raw fd and exec-child stdout are diagnostic; private result fd not inherited')
            if os.name != 'nt':
                p = subprocess.Popen([str(args.output_probe), 'normal'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                p.stdout.close()
                p.stdout = None
                _, stderr = p.communicate(timeout=5)
                check(p.returncode == 74 and b'result write failed' in stderr,
                      'closed consumer produces actual nonzero output failure instead of success')
        result = {'result': 'passed', 'checks': len(checks), 'cases': cases, 'fixture': str(root),
                  'pcm_max_error': error, 'pcm_tolerance': 2e-6, 'projects': projects,
                  'scope': 'actual CLI processes, native renderer and file hashes; channel probe is an explicit test '
                           'executable; no OS desktop, physical input or listening qualification'}
    except Exception as error:
        result = {'result': 'failed', 'checks': len(checks), 'cases': cases,
                  'fixture': str(root), 'error': str(error)}
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result['result'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
