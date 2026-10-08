#!/usr/bin/env python3
"""Sign local developer bundles without changing keychain trust or security settings."""
import argparse
import pathlib
import plistlib
import re
import subprocess


def sign(path, identity, identifier):
    requirement = '=designated => identifier "' + identifier + '" and certificate leaf = H"' + identity + '"'
    subprocess.run([
        '/usr/bin/codesign', '--force', '--sign', identity, '--identifier', identifier,
        '--timestamp=none', '--requirements', requirement, str(path)
    ], check=True)
    subprocess.run(['/usr/bin/codesign', '--verify', '--strict', '--verbose=2', str(path)], check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', type=pathlib.Path, required=True)
    parser.add_argument('--identity', required=True, help='SHA1 of the fixed code-signing certificate')
    args = parser.parse_args()
    if not re.fullmatch(r'[0-9a-fA-F]{40}', args.identity):
        parser.error('identity must be a fixed certificate SHA1')
    with (args.app / 'Contents/Info.plist').open('rb') as file:
        info = plistlib.load(file)
    if info['CFBundleIdentifier'] != 'org.forma.daw' or info['CFBundleName'] != 'Forma':
        raise RuntimeError('unexpected application identity')
    helpers = args.app / 'Contents/Helpers'
    bridge = helpers / 'forma-mcp'
    if bridge.is_file():
        sign(bridge, args.identity, 'org.forma.daw.mcp')
    scanner = helpers / 'ndaw_plugin_scan_worker.app'
    if scanner.is_dir():
        with (scanner / 'Contents/Info.plist').open('rb') as file:
            scanner_info = plistlib.load(file)
        sign(scanner, args.identity, scanner_info['CFBundleIdentifier'])
    sign(args.app, args.identity, info['CFBundleIdentifier'])
    subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', '--verbose=2', str(args.app)], check=True)
    subprocess.run(['/usr/bin/codesign', '-d', '-r-', str(args.app)], check=True)


if __name__ == '__main__':
    main()
