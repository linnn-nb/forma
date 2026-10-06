#!/usr/bin/env python3
"""Audit retained real mouse/key transactions and two real exports; never drives/fakes UI."""
import argparse
import array
import hashlib
import json
import math
import struct
from pathlib import Path


def wav(path):
    raw = path.read_bytes()
    assert raw[:4] == b'RIFF' and raw[8:12] == b'WAVE'
    chunks, pos = {}, 12
    while pos + 8 <= len(raw):
        tag, size = struct.unpack_from('<4sI', raw, pos)
        assert pos + 8 + size <= len(raw)
        chunks[tag] = raw[pos + 8:pos + 8 + size]
        pos += 8 + size + (size & 1)
    fmt, channels, rate, _, align, bits = struct.unpack_from('<HHIIHH', chunks[b'fmt '])
    assert (fmt, channels, rate, align, bits) == (3, 2, 48000, 8, 32)
    samples = array.array('f', chunks[b'data'])
    assert all(math.isfinite(x) for x in samples)
    assert len(samples) // 2 == 864000 and max(map(abs, samples)) > 0
    return raw, samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', type=Path)
    parser.add_argument('evidence', type=Path)
    args = parser.parse_args()
    p, e = args.project, args.evidence
    baseline = json.loads((p / 'ui-baseline.json').read_text())
    session = json.loads((p / 'session.ndaw').read_text())['session']
    history = {b['after']['revision']: b for f in (p / '.history').glob('*.json')
               for b in [json.loads(f.read_text())['body']]}
    assert baseline['revision'] == 7 and session['revision'] == 19
    for key in ('tracks', 'sources', 'buses', 'analysis', 'markers', 'main_bus_id'):
        assert session[key] == baseline[key], key
    checks = []
    expected = {8: ['split_clip'], 10: ['move_clip'], 12: ['move_clip', 'trim_clip'],
                14: ['duplicate_clip'], 16: ['set_track_gain'], 18: ['rename_track']}
    for rev, operations in expected.items():
        edit, undo = history[rev], history[rev + 1]
        assert edit['actor'] == undo['actor'] == 'gui'
        assert [op['command'] for op in edit['operations']] == operations
        assert undo['history_action'] == 'undo' and undo['history_target'] == edit['id']
        for key in ('tracks', 'sources', 'buses'):
            assert undo['after'][key] == edit['before'][key], (rev, key)
        checks.append({'revision': rev, 'operations': edit['operations'],
                       'one_edit_transaction': True, 'undo_revision': rev + 1,
                       'undo_restores_project_objects': True})
    trim = history[12]['after']['tracks'][0]['clips'][0]
    assert trim['start'] == trim['source_start'] == 60714
    assert trim['length'] == 803286 and trim['start'] + trim['length'] == 864000
    copied = history[14]['after']['tracks'][0]['clips']
    assert len(copied) == 2 and len({c['id'] for c in copied}) == 2
    assert copied[0]['source_id'] == copied[1]['source_id']
    assert history[18]['after']['tracks'][0]['name'] == 'Music source'
    sources = []
    for source in session['sources']:
        sha = hashlib.sha256((p / source['path']).read_bytes()).hexdigest()
        assert sha == source['sha256']
        sources.append({'source_id': source['id'], 'sha256': sha})
    gui, gui_pcm = wav(p / 'gui-workstation-mix.wav')
    cli, cli_pcm = wav(p / 'cli-workstation-mix.wav')
    assert gui == cli and gui_pcm == cli_pcm
    reference = json.loads((e / 'gui-export-cli-reference.json').read_text())
    assert reference['status'] == 'exported_and_verified' and reference['revision'] == 19
    assert reference['plugins'][0]['process_exit']['reaped']
    assert reference['plugins'][0]['process_exit']['exit_code'] == 0
    assert 'Export verified' in (e / 'gui-export.ax.txt').read_text()
    assert 'Position 18.000 s' in (e / 'gui-export.ax.txt').read_text()
    assert 'playing' in (e / 'gui-mix-final.ax.txt').read_text()
    result = {'status': 'retained_actual_GUI_transactions_and_audio_verified',
              'project': str(p.resolve()), 'baseline_revision': 7, 'saved_revision': 19,
              'actual_mouse_keyboard_edits': checks, 'non_destructive_media': sources,
              'all_undone_project_fields_equal_baseline': True,
              'export': {'channels': 2, 'sample_rate': 48000, 'frames': 864000,
                         'duration_seconds': 18, 'file_pcm': 'IEEE float32',
                         'finite_nonzero_pcm': True, 'whole_file_matches_reopened_cli': True,
                         'sha256': hashlib.sha256(gui).hexdigest(), 'bytes': len(gui),
                         'actual_isolated_plugin': 'Apple AULowpass', 'normal_sdk_exit': True,
                         'range': 'whole session; GUI selection-range export remains absent'},
              'device_gui_content_end_seconds': 18, 'actual_model': False,
              'scope': 'Real disposable six arrangements of one music source, Audio/Aux/Master; '
                       'not stems, recording, listening, endurance, full GUI acceptance or Pro Tools parity.'}
    (e / 'gui-workflow.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
