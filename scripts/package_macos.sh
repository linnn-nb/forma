#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if [ "$(uname -s)" != Darwin ]; then echo 'macOS packaging only' >&2; exit 2; fi
app="$PWD/build/NativeDAW_artefacts/Release/NativeDAW.app"
cli="$PWD/build/ndaw_artefacts/Release/ndaw"
worker_bundle="$PWD/build/ndaw_plugin_worker_artefacts/Release/ndaw_plugin_worker.app"
worker="$worker_bundle/Contents/MacOS/ndaw_plugin_worker"
[ -x "$app/Contents/MacOS/NativeDAW" ]
[ -x "$cli" ]
[ -x "$worker" ]
[ -x "$app/Contents/Helpers/ndaw_plugin_worker.app/Contents/MacOS/ndaw_plugin_worker" ]
if grep -q '^NATIVEDAW_RT_AUDIT:BOOL=ON$' build/CMakeCache.txt; then
  echo 'Refusing to package the test-only RT interposer build' >&2; exit 2
fi
stage="${NATIVEDAW_PACKAGE_STAGE:-$PWD/dist/macos-dev}"
export NATIVEDAW_PACKAGE_STAGE="$stage"
output="${NATIVEDAW_PACKAGE_DMG:-$PWD/dist/NativeDAW-0.1.0-macos-arm64-dev.dmg}"
export NATIVEDAW_PACKAGE_DMG="$output"
if [ -e "$output" ]; then echo "Refusing to overwrite existing package: $output" >&2; exit 2; fi
if [ -e "$stage/NativeDAW.app" ]; then echo "Refusing to overwrite existing app stage: $stage" >&2; exit 2; fi
mkdir -p "$stage/Documentation" "$stage/Command Line" "$stage/Third Party Notices" "$stage/Source"
ditto "$app" "$stage/NativeDAW.app"
cp "$cli" "$stage/Command Line/ndaw"
ditto "$worker_bundle" "$stage/Command Line/ndaw_plugin_worker.app"
cp README.md docs/PRODUCT_SPEC.md docs/PRO_TOOLS_PARITY.md docs/ARCHITECTURE.md docs/AI_COMMAND_CONTRACT.md docs/VERIFICATION.md docs/DEPENDENCIES_AND_BLOCKERS.md docs/NEXT_STEPS.md "$stage/Documentation/"
cp LICENSE NOTICE.md "$stage/"
cp third_party/nlohmann-json-LICENSE.MIT "$stage/Third Party Notices/"
python3 - <<'PY'
from pathlib import Path
import shutil,tarfile,gzip,hashlib,json,os
stage=Path(os.environ["NATIVEDAW_PACKAGE_STAGE"]).resolve()
assert stage.is_relative_to(Path("dist").resolve()), "Package stage must stay in this workspace dist directory"
cache=Path('build/CMakeCache.txt').read_text().splitlines()
juce=Path(next(line.split('=',1)[1] for line in cache if line.startswith('JUCE_SOURCE_DIR:')))
assert (juce/'LICENSE.md').is_file()
shutil.copyfile(juce/'LICENSE.md',stage/'Third Party Notices/JUCE-LICENSE.md')
source=juce/'modules';target=(stage/'Third Party Notices/JUCE-modules')
for p in source.rglob('*'):
 if p.is_file() and any(t in p.name.lower() for t in ('license','licence','copying','copyright')):
  out=target/p.relative_to(source);out.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,out)
# Include actual authored code/build scripts and dependency sources, never user
# audio/evidence, tools/credentials, Git history, build output or model weights.
items=[]
for name in ('.gitignore','AGENTS.md','CMakeLists.txt','CMakePresets.json','README.md','LICENSE','NOTICE.md'):
 items.append((Path(name),name))
for folder in ('include','src','tests','scripts','docs','third_party'):
 for p in sorted(Path(folder).rglob('*')):
  if p.is_file() and '__pycache__' not in p.parts:items.append((p,p.as_posix()))
items.append((Path('.deps/json.hpp'),'.deps/json.hpp'))
for p in sorted(juce.rglob('*')):
 if p.is_file() and '.git' not in p.parts:items.append((p,'.deps/JUCE-8.0.12/'+p.relative_to(juce).as_posix()))
manifest={name:hashlib.sha256(p.read_bytes()).hexdigest() for p,name in items}
metadata=json.dumps({'files_sha256':manifest,'contains_user_media':False,'licence':'AGPL-3.0-only; dependencies retain their licences'},sort_keys=True,indent=2).encode()
out=(stage/'Source/NativeDAW-source.tar.gz')
with out.open('wb') as raw,gzip.GzipFile(filename='',fileobj=raw,mode='wb',mtime=0) as zipped,tarfile.open(fileobj=zipped,mode='w') as archive:
 for p,name in sorted(items,key=lambda x:x[1]):
  info=archive.gettarinfo(str(p),'NativeDAW/'+name);info.mtime=0;info.uid=info.gid=0;info.uname=info.gname='';info.pax_headers={}
  with p.open('rb') as data:archive.addfile(info,data)
 import io
 info=tarfile.TarInfo('NativeDAW/SOURCE_MANIFEST.json');info.size=len(metadata);archive.addfile(info,io.BytesIO(metadata))
(stage/'Source/source-manifest.json').write_bytes(metadata)
with tarfile.open(out) as archive:
 for member in archive:
  if member.isfile() and member.name!='NativeDAW/SOURCE_MANIFEST.json':
   assert hashlib.sha256(archive.extractfile(member).read()).hexdigest()==manifest[member.name.removeprefix('NativeDAW/')]
Path('evidence/source-package.json').write_text(json.dumps({'status':'archive_readback_hashes_verified','files':len(items),
 'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'contains_user_media':False,'contains_models':False,
 'scope':'actual source + JUCE/json + build scripts; full release notice/SBOM audit remains pending'},indent=2))
PY
cat > "$stage/DEVELOPMENT BUILD.txt" <<'TXT'
NativeDAW 0.1.0 macOS arm64 open-source development increment, AGPL-3.0-only.
This local image is not a production-ready or notarized release.
It contains no user's audio, third-party plugin assets or models.
See LICENSE/NOTICE.md. JUCE's AGPLv3 option is selected; full release notice/SBOM and platform qualification remain unfinished.
Source/NativeDAW-source.tar.gz includes the actual project source, JUCE/json sources and build scripts. Source/source-manifest.json records all SHA256 values and archive readback is verified. No external publication occurred.
After extraction, build using README.md; to use the included JUCE sources configure with -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/.deps/JUCE-8.0.12". CMake/json version/hash pins remain in CMakeLists.txt.
To try the native build, copy NativeDAW.app to a user-owned application folder and open it. Audio playback/edit/save/export work without an AI model. Microphone capture needs the native permission prompt.
Close an already-running NativeDAW instance before opening a new build.
Uninstall: remove this development app. Local sessions in ~/Library/Application Support/NativeDAW are retained; inspect/save them before any deletion.
Updates are manual development bundle replacements. No production updater exists yet.
See Documentation/VERIFICATION.md and PRO_TOOLS_PARITY.md for all unfinished requirements and tested limits.
TXT
# Local integrity signing, not Developer ID or notarization.
codesign --force --deep --sign - "$stage/NativeDAW.app/Contents/Helpers/ndaw_plugin_worker.app" > evidence/package-helper-codesign.log 2>&1
codesign --force --deep --sign - "$stage/Command Line/ndaw_plugin_worker.app" >> evidence/package-helper-codesign.log 2>&1
codesign --verify --deep --strict "$stage/Command Line/ndaw_plugin_worker.app" >> evidence/package-helper-codesign.log 2>&1
codesign --force --deep --sign - "$stage/NativeDAW.app" > evidence/package-codesign.log 2>&1
codesign --verify --deep --strict "$stage/NativeDAW.app" >> evidence/package-codesign.log 2>&1
ln -sfn /Applications "$stage/Applications"
hdiutil create -volname 'NativeDAW Development' -srcfolder "$stage" -ov -format UDZO "$output" > evidence/package-dmg.log 2>&1
hdiutil verify "$output" >> evidence/package-dmg.log 2>&1
python3 - <<'PY'
from pathlib import Path
import hashlib,json,subprocess,os,plistlib
stage=Path(os.environ["NATIVEDAW_PACKAGE_STAGE"]).resolve()
app=(stage/'NativeDAW.app/Contents/MacOS/NativeDAW');dmg=Path(os.environ['NATIVEDAW_PACKAGE_DMG'])
command=(stage/'Command Line/ndaw')
r=subprocess.run([str(command.resolve()),'commands'],capture_output=True,text=True)
assert r.returncode==0 and 'move_clip' in json.loads(r.stdout)
libs=subprocess.run(['otool','-L',str(app)],capture_output=True,text=True,check=True).stdout
cli_libs=subprocess.run(['otool','-L',str(command)],capture_output=True,text=True,check=True).stdout
worker=(stage/'Command Line/ndaw_plugin_worker.app/Contents/MacOS/ndaw_plugin_worker')
helper=(stage/'NativeDAW.app/Contents/Helpers/ndaw_plugin_worker.app/Contents/MacOS/ndaw_plugin_worker')
worker_libs=subprocess.run(['otool','-L',str(worker)],capture_output=True,text=True,check=True).stdout
assert hashlib.sha256(worker.read_bytes()).hexdigest()==hashlib.sha256(helper.read_bytes()).hexdigest()
for executable in (worker,helper):
 info=plistlib.loads((executable.parent.parent/'Info.plist').read_bytes())
 assert info['CFBundleIdentifier']=='org.nativedaw.plugin-host' and info['CFBundleExecutable']==executable.name
assert not (stage/'NativeDAW.app/Contents/Helpers/ndaw_plugin_worker').exists(), 'Stale bare helper must not be packaged'
assert not any('fault_worker' in p.name or 'plugin_tests' in p.name or p.suffix=='.bin' for p in stage.rglob('*'))
for linkage in (libs,cli_libs,worker_libs):
 # otool's first line is the inspected executable, not a dependency.
 dependencies=[line.strip().split(' (',1)[0] for line in linkage.splitlines()[1:]]
 assert dependencies and all(path.startswith(('/System/Library/','/usr/lib/')) for path in dependencies)
 assert not any('ndaw_rt_audit' in path for path in dependencies)
# The first otool line is a relative app path, with dependencies restricted to system libraries/frameworks.
report={'status':'local_development_package_created_and_integrity_checked','production_release':False,
 'notarized':False,'developer_id_signed':False,'signature':'ad hoc integrity only','dmg':str(dmg.resolve()),
 'sha256':hashlib.sha256(dmg.read_bytes()).hexdigest(),'size_bytes':dmg.stat().st_size,
 'relocated_cli_commands_executed':r.returncode==0,'standalone_system_linkage':libs,'cli_system_linkage':cli_libs,
 'plugin_worker_system_linkage':worker_libs,'plugin_worker_in_cli_and_app_helpers':True,'plugin_worker_native_app_identity':'org.nativedaw.plugin-host',
 'plugin_worker_sha256':hashlib.sha256(worker.read_bytes()).hexdigest(),'test_worker_binary_packaged':False,'private_plugin_state_packaged':False,
 'rt_audit_packaged':False,
 'gui_install_launch':'installation unexecuted; actual earlier SDK-editor and Playlist/Comp native evidence retained; iteration14 native recording configuration submitted/saved/reopened. Iterations15-17 Punch, linked Comp, groups, multi-selection and inline Source Playlist compiled-widget evidence is separate from actual mouse/capture. Current Mac locked, actual17 desktop gestures unexecuted. Standalone microphone prompt pending user action because computer-use cannot access system popup. Separately signed GUI test folder and previous user app windows preserved',
 'source_archive':'Source/NativeDAW-source.tar.gz',
 'licence':'AGPL-3.0-only; dependency terms retained',
 'third_party_notice_collection':'pinned JUCE/module and json texts copied; full release audit incomplete'}
Path('evidence/package.json').write_text(json.dumps(report,indent=2))
print(json.dumps({k:report[k] for k in ['status','production_release','dmg','sha256']},indent=2))
PY
