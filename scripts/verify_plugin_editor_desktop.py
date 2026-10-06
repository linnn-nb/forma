#!/usr/bin/env python3
"""Audit retained real SDK gestures, GUI journal, saved states and fresh PCM.

This script does not operate the UI, generate test substitutes or infer private
plugin semantics. Its inputs must come from the separately executed desktop run.
"""
import argparse
import hashlib
import json
from pathlib import Path

from verify_workstation_gui_receipts import wav


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def envelope(path, key):
    doc = json.loads(path.read_text())
    value = doc[key]
    encoded = json.dumps(value, sort_keys=True, separators=(',', ':'),
                         ensure_ascii=False).encode()
    assert hashlib.sha256(encoded).hexdigest() == doc['checksum'], path
    return value


def objects(session):
    return {k: v for k, v in session.items() if k != 'revision'}


def plugin(session):
    track = next(t for t in session['tracks'] if t['id'] == 'aux')
    return next(p for p in track['processors'] if p['id'] == 'au-instance')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence', type=Path)
    args = parser.parse_args()
    e = args.evidence.resolve()
    project = e / 'desktop-session'
    baseline = json.loads((e / 'desktop-baseline.json').read_text())
    candidate = envelope(e / 'desktop-saved-candidate.ndaw', 'session')
    restored = envelope(e / 'desktop-saved-restored.ndaw', 'session')
    assert [s['revision'] for s in (baseline, candidate, restored)] == [19, 20, 21]
    assert objects(restored) == objects(baseline)
    journals = [envelope(p, 'body') for p in (project / '.history').glob('*.json')]
    edits = [j for j in journals if j.get('operations', [{}])[0].get('command')
             == 'adopt_plugin_state']
    assert len(edits) == 1
    edit = edits[0]
    undo = next(j for j in journals if j.get('history_target') == edit['id'])
    assert edit['actor'] == undo['actor'] == 'gui'
    assert len(edit['operations']) == 1 and undo['history_action'] == 'undo'
    assert edit['before'] == baseline and edit['after'] == candidate
    assert undo['after'] == restored
    capture_id = edit['operations'][0]['capture_id']
    captures = []
    for path in sorted((project / '.plugin-state-captures').glob('*/receipt.json')):
        body = envelope(path, 'body')
        editor = body['actual_receipt']['editor']
        assert body['capture_method'] == 'native_editor_preview'
        assert editor['native_sdk_editor'] and editor['prior_state_restored']
        assert editor['status'] == 'succeeded' and editor['event'] == 'captured_and_restored'
        assert editor['before_parameters'] == editor['restored_parameters']
        assert editor['worker_pid'] > 0 and editor['editor_lease'] > 0
        assert editor['request_wall_ms'] < 2000
        for key in ('before_state', 'candidate_state'):
            state = editor[key]
            state_path = Path(state['path'])
            assert state_path.is_relative_to(project / '.plugin-runtime')
            assert state_path.stat().st_size == state['bytes']
            assert digest(state_path) == state['sha256']
        assert editor['before_state']['sha256'] != editor['candidate_state']['sha256']
        actual = body['actual_parameters']
        assert actual == editor['candidate_parameters']
        captures.append({'capture_id': body['capture_id'], 'adopted': body['capture_id'] == capture_id,
                         'worker_pid': editor['worker_pid'], 'editor_lease': editor['editor_lease'],
                         'native_sdk_editor': True, 'original_sdk_state_restored': True,
                         'candidate_state_sha256': editor['candidate_state']['sha256'],
                         'actual_parameters': actual})
        if body['capture_id'] == capture_id:
            fx = plugin(candidate)
            assert fx['state']['sha256'] == body['state']['sha256']
            assert digest(project / fx['state']['path']) == fx['state']['sha256']
            for parameter in fx['parameters']:
                actual_parameter = next(p for p in actual if p['id'] == parameter['sdk_id'])
                assert parameter['value'] == actual_parameter['value']
    assert len(captures) == 2 and sum(c['adopted'] for c in captures) == 1
    assert len({c['worker_pid'] for c in captures}) == 1
    source_hashes = []
    for source in baseline['sources']:
        assert digest(project / source['path']) == source['sha256']
        source_hashes.append({'id': source['id'], 'sha256': source['sha256']})
    original_state = plugin(baseline)['state']
    assert digest(project / original_state['path']) == original_state['sha256']
    exports = []
    for name, expected in (('candidate', candidate), ('restored', restored)):
        receipt = json.loads((e / f'{name}-render-receipt.json').read_text())
        assert receipt['status'] == 'exported_and_verified'
        assert receipt['revision'] == expected['revision']
        fx = receipt['plugins'][0]
        assert fx['fault'] == fx['deadline_misses'] == 0
        assert fx['process_exit']['exit_code'] == 0 and fx['process_exit']['reaped']
        if name == 'candidate':
            # Test opaque restoration before desired parameter overlays, not just
            # matching a requested normalized value after processing.
            before_overlay = fx['prepared']['parameters_after_state_restore']
            for parameter in plugin(candidate)['parameters']:
                actual = next(p for p in before_overlay if p['id'] == parameter['sdk_id'])
                assert abs(actual['value'] - parameter['value']) < 1e-6
        data, samples = wav(e / f'{name}-reopened.wav')
        exports.append({'name': name, 'sha256': hashlib.sha256(data).hexdigest(),
                        'bytes': len(data), 'frames': len(samples) // 2,
                        'finite_nonzero_pcm': True, 'normal_sdk_exit_reaped': True})
    _, candidate_pcm = wav(e / 'candidate-reopened.wav')
    _, restored_pcm = wav(e / 'restored-reopened.wav')
    difference = max(abs(a - b) for a, b in zip(candidate_pcm, restored_pcm))
    assert difference > 0
    prior = json.loads((e.parent / 'ui-refactor/gui-workflow.json').read_text())
    assert exports[1]['sha256'] == prior['export']['sha256']
    saved_ui = (e / '08-undo-saved.ax.txt').read_text()
    assert f'Saved | {project / "session.ndaw"}' in saved_ui and 'revision 21' in saved_ui
    result = {'status': 'actual_native_editor_GUI_state_and_reopened_audio_verified',
              'baseline_revision': 19, 'candidate_revision': 20, 'undo_revision': 21,
              'one_actual_gui_transaction': edit['id'], 'undo_transaction': undo['id'],
              'captures': captures, 'restored_all_project_fields': True,
              'original_media_unchanged': source_hashes,
              'original_plugin_state_unchanged': original_state['sha256'],
              'fresh_sdk_exports': exports, 'candidate_pcm_max_abs_difference': difference,
              'undo_whole_file_matches_prior_GUI_export': True, 'actual_model': False,
              'scope': 'Actual Apple AU mouse gestures, Reject/Accept/Undo/save, fresh CLI SDK '
                       'state restoration and 18 s real music PCM; not listening, arbitrary '
                       'presets, all vendors, GUI export in this iteration, endurance or full product.'}
    (e / 'desktop-workflow.json').write_text(json.dumps(result, indent=2) + '\n')
    package_project = e / 'package-session'
    if (e / '10-packaged-review.ax.txt').exists():
        package_session = envelope(package_project / 'session.ndaw', 'session')
        assert package_session == restored
        preview_ui = (e / '10-packaged-review.ax.txt').read_text()
        assert 'Cutoff Frequency: 13101.7 Hz -> 1714.5 Hz' in preview_ui
        saved_ui = (e / '11-packaged-keyboard-save.ax.txt').read_text()
        assert f'Saved | {package_project / "session.ndaw"}' in saved_ui and 'revision 21' in saved_ui
        live_ui = (e / '13-packaged-mix-playing.ax.txt').read_text()
        assert 'playing' in live_ui and 'Position 7.293 s' in live_ui
        assert live_ui.count('Post-fader sample peak -') == 8
        receipts = list((package_project / '.plugin-state-captures').glob('*/receipt.json'))
        assert len(receipts) == 1
        capture = envelope(receipts[0], 'body')
        editor = capture['actual_receipt']['editor']
        assert capture['capture_method'] == 'native_editor_preview'
        assert editor['native_sdk_editor'] and editor['prior_state_restored']
        assert editor['before_parameters'] == editor['restored_parameters']
        assert editor['candidate_parameters'][0]['display'] == '1714.5'
        assert digest(Path(editor['candidate_state']['path'])) == editor['candidate_state']['sha256']
        retirement = json.loads((Path(editor['job_directory']) / 'host-retirement.json').read_text())
        assert retirement['retirement_status'] == 'verified_normal_teardown'
        assert retirement['process_exit']['status'] == 'exited'
        assert retirement['process_exit']['exit_code'] == 0 and retirement['process_exit']['reaped']
        assert not list((package_project / '.history').glob('*.json'))
        for source in package_session['sources']:
            assert digest(package_project / source['path']) == source['sha256']
        package_result = {
            'status': 'actual_staged_native_editor_reject_keyboard_save_and_playback_verified',
            'stage': str(e.parent.parent / 'dist/macos-dev-iteration-12'),
            'project': str(package_project), 'saved_revision': 21,
            'all_session_fields_unchanged': True, 'actual_sdk_before_text': '13101.7 Hz',
            'actual_sdk_candidate_text': '1714.5 Hz', 'actual_sdk_editor': True,
            'actual_capture_id': capture['capture_id'], 'actual_worker_pid': editor['worker_pid'],
            'normal_native_worker_retirement': retirement, 'actual_keyboard_save': True,
            'actual_playback_position_seconds': 7.293, 'actual_processed_meter_count': 8,
            'clean_install': False, 'actual_model': False,
            'scope': 'Separate staged app/helper, real AU gesture/readable preview/Reject/Cmd-S/music '
                     'playback; Accept/Undo/export not repeated here. Source-app/fresh CLI proof is separate.'}
        (e / 'package-desktop-workflow.json').write_text(json.dumps(package_result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
