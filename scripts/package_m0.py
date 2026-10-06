#!/usr/bin/env python3
"""Package a qualified, local-only M0 increment; archive previous development artifacts."""
import argparse, hashlib, json, shutil, subprocess, tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
if 'PRODUCT_NAME "NativeDAW M0"' not in (root / 'CMakeLists.txt').read_text():
    raise SystemExit('Historical M0 packager: current branch has advanced to M1; no M1 package before milestone acceptance')
parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', default='build-v2-tracktion')
args = parser.parse_args()
build = (root / args.build_dir).resolve()

def run(*argv):
    return subprocess.check_output(argv, cwd=root, text=True, stderr=subprocess.STDOUT).strip()

# Git is the source of truth. Never make an artifact in place of committing source.
if run('git', 'status', '--porcelain', '--ignore-submodules=dirty'):
    raise SystemExit('Commit repository changes before packaging')
commit = run('git', 'rev-parse', 'HEAD')
for name in ('test-summary.json', 'device-playback.json'):
    report = json.loads((root / 'evidence/M0' / name).read_text())
    if not (report.get('result') == 'passed' or report.get('passed') is True):
        raise SystemExit(f'Unqualified M0 report: {name}')
app = build / 'NativeDAW_artefacts/Release/NativeDAW M0.app'
cli = build / 'ndaw_artefacts/Release/ndaw'
output = root / 'dist/NativeDAW-0.2.0-M0-macos-arm64-dev.dmg'
if output.exists():
    raise SystemExit('M0 milestone package already exists; do not silently replace it')
with tempfile.TemporaryDirectory(prefix='package-m0-', dir=build) as work:
    work = Path(work)
    stage = work / 'content'
    stage.mkdir()
    run('ditto', str(app), str(stage / app.name))
    # The compiler's linker signature does not seal a bundle. Seal only our staging copy.
    # Ad-hoc signing is for this local development artifact, not Developer ID or notarisation.
    run('codesign', '--force', '--deep', '--sign', '-', str(stage / app.name))
    run('codesign', '--verify', '--deep', '--strict', str(stage / app.name))
    shutil.copy2(cli, stage / 'ndaw')
    for doc in ('LICENSE', 'NOTICE.md', 'docs/M0_REPORT.md'):
        shutil.copy2(root / doc, stage / Path(doc).name)
    licenses = stage / 'ThirdPartyNotices'
    for source in (root / 'third_party/tracktion_engine', root / 'third_party/libebur128', root / '.deps/JUCE-37c894f83d379179b2070d437ccd0f1cd9af9576'):
        for path in source.rglob('*'):
            if path.is_file() and path.name.upper().startswith(('LICENSE', 'LICENCE', 'COPYING', 'NOTICE', 'COPYRIGHT')) and '.git' not in path.parts:
                dest = licenses / source.name / path.relative_to(source)
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, dest)
    shutil.copy2(root / '.deps/json.hpp', licenses / 'json.hpp')
    (stage / '试用说明.md').write_text('''# NativeDAW M0 本地开发包\n\nM0 可行性关口已验证，完整 DAW 尚未完成。打开应用，导入本地音频，接受预览并播放；停止后 Undo/Redo；增益可单独撤销；保存和导出请选择新文件名。\n\n不包含测试音频、第三方插件或模型。没有正式签名、公证或 Windows 验收；这是本地开发产物，尚未对外发布。M1 开始接通 Solo、内置处理器、自动化、MIDI 和完整 Edit/Mix。\n''')
    (stage / 'Source.md').write_text(f'''# 构建源码\n\n本地 Git 仓库：{root}\n分支：v2-tracktion\n提交：{commit}\n\n依赖提交、已记录补丁、构建命令和边界见 M0_REPORT.md。源码由 Git 管理，本包没有替代源码版本管理的 tar 包。这个包仅用于当前工作区的本地试用，没有对外发布。正式分发前还需完成对应源码分发和全部第三方许可审计。\n''')
    image = work / output.name
    run('hdiutil', 'create', '-volname', 'NativeDAW M0', '-srcfolder', str(stage), '-format', 'UDZO', '-o', str(image))
    run('hdiutil', 'verify', str(image))
    archive = Path.home() / 'Archive/NativeDAW-legacy-artifacts'
    archive.mkdir(parents=True, exist_ok=True)
    output.parent.mkdir(exist_ok=True)
    moved = []
    for old in list(output.parent.iterdir()):
        if old.suffix == '.dmg' or '.tar' in old.name or (old.is_dir() and old.name.startswith('macos-')):
            dest = archive / old.name
            n = 1
            while dest.exists():
                dest = archive / f'{old.stem}-archived-{n}{old.suffix}'
                n += 1
                if n > 100:
                    raise SystemExit('Archive name conflict limit reached')
            shutil.move(str(old), str(dest))
            moved.append(str(dest))
    shutil.move(str(image), str(output))
    summary = {'path':str(output),'source_commit':commit,'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'archived':moved,'qualification':'M0 local development only'}
    (root / 'evidence/M0/package.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2))
    print(json.dumps(summary, ensure_ascii=False, indent=2))
