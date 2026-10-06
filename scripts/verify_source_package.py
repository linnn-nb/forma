#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Read every packaged source byte and compare it to the current checkout.

Local readback only. This does not certify third-party asset rights, installer UX,
signing, notarization, reproducibility on another host or production readiness.
"""
from pathlib import Path, PurePosixPath
from datetime import datetime, timezone
import argparse
import hashlib
import json
import tarfile


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def verify(stage, source_copy, receipt):
    repo = Path(__file__).resolve().parent.parent
    stage = stage.resolve()
    source_copy = source_copy.resolve()
    receipt = receipt.resolve()
    if not stage.is_relative_to(repo / 'dist') or not source_copy.is_relative_to(repo / 'dist'):
        raise ValueError('Development artifacts must stay in this checkout dist directory')
    if not receipt.is_relative_to(repo / 'evidence'):
        raise ValueError('Readback receipts must stay in this checkout evidence directory')
    if source_copy.exists():
        raise FileExistsError(f'Refusing to overwrite source artifact: {source_copy}')
    archive_path = stage / 'Source/NativeDAW-source.tar.gz'
    manifest_bytes = (stage / 'Source/source-manifest.json').read_bytes()
    manifest = json.loads(manifest_bytes)['files_sha256']
    seen = set()
    audio_assets = []
    excluded = {'.git', '.tools', 'evidence', 'dist', 'build', 'build-rtaudit',
                'build-sanitize', '__pycache__', '.plugin-states', '.plugin-runtime', 'models'}
    audio_suffixes = {'.wav', '.mp3', '.aif', '.aiff', '.flac', '.ogg', '.m4a', '.mid', '.midi'}
    with tarfile.open(archive_path) as archive:
        for member in archive:
            if not member.isfile() or not member.name.startswith('NativeDAW/'):
                raise ValueError(f'Unexpected source archive member: {member.name}')
            relative = member.name.removeprefix('NativeDAW/')
            path = PurePosixPath(relative)
            if path.is_absolute() or '..' in path.parts or excluded.intersection(path.parts):
                raise ValueError(f'Excluded source archive path: {relative}')
            if relative in seen:
                raise ValueError(f'Duplicate source archive member: {relative}')
            seen.add(relative)
            data = archive.extractfile(member).read()
            if relative == 'SOURCE_MANIFEST.json':
                if data != manifest_bytes:
                    raise ValueError('Embedded and adjacent source manifests differ')
                continue
            if relative not in manifest or sha256(data) != manifest[relative]:
                raise ValueError(f'Archive byte checksum mismatch: {relative}')
            current = repo.joinpath(*path.parts)
            if not current.is_file() or sha256(current.read_bytes()) != manifest[relative]:
                raise ValueError(f'Current source differs from archive: {relative}')
            if path.suffix.lower() in audio_suffixes:
                if not relative.startswith('.deps/JUCE-8.0.12/'):
                    raise ValueError(f'Unexpected media in corresponding source: {relative}')
                audio_assets.append({'path': member.name, 'sha256': manifest[relative]})
    if seen != set(manifest) | {'SOURCE_MANIFEST.json'}:
        raise ValueError('Source archive members do not match the complete manifest')
    source_copy.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation also closes the check/create race; existing artifacts
    # cannot be replaced by a retry. A failed write remains an explicit failure.
    with source_copy.open('xb') as output, archive_path.open('rb') as source:
        while data := source.read(1024 * 1024):
            output.write(data)
    original_hash = sha256(archive_path.read_bytes())
    if sha256(source_copy.read_bytes()) != original_hash:
        raise ValueError('Standalone corresponding-source copy failed checksum readback')
    result = {
        'status': 'verified_archive_manifest_current_checkout_and_exclusive_source_copy',
        'verified_utc': datetime.now(timezone.utc).isoformat(),
        'all_current_source_hashes_match_archive': True,
        'files': len(manifest),
        'source': str(source_copy), 'source_sha256': original_hash,
        'contains_user_media': False, 'contains_models': False,
        'contains_private_plugin_state': False,
        'upstream_juce_sample_assets': audio_assets,
        'complete_per_asset_release_rights_audit': False,
        'scope': 'All archive/source hashes checked. Included audio assets are pinned framework examples; full per-asset release notices/SBOM/rights audit remains pending.',
    }
    receipt.parent.mkdir(parents=True, exist_ok=True)
    receipt.write_text(json.dumps(result, indent=2))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', type=Path)
    parser.add_argument('source_copy', type=Path)
    parser.add_argument('receipt', type=Path)
    args = parser.parse_args()
    result = verify(args.stage, args.source_copy, args.receipt)
    print(json.dumps({key: result[key] for key in ('status', 'files', 'source', 'source_sha256')}, indent=2))
