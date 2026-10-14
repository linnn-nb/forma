#!/usr/bin/env python3
"""Replay and reverse every declared SDK patch against the exact locked originals."""
import hashlib
import json
import pathlib
import re
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
sdk = root / "third_party/tracktion_engine"
pin = "0d4d77c8c9defa6ec2aec6454f634e77bbd13f98"
assert subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=sdk).decode().strip() == pin
cmake = (root / "CMakeLists.txt").read_text()
names = re.search(r"foreach\(TE_PATCH_NAME (.*?)\)", cmake).group(1).split()
patches = [root / "patches" / name for name in names]
paths = sorted({m for p in patches for m in re.findall(r"^\+\+\+ b/(.*)$", p.read_text(), re.M)})
originals = {p: subprocess.check_output(["git", "show", f"{pin}:{p}"], cwd=sdk) for p in paths}
with tempfile.TemporaryDirectory(prefix="forma-sdk-patch-replay-") as folder:
    temp = pathlib.Path(folder)
    subprocess.run(["git", "init", "-q"], cwd=temp, check=True)
    for path, data in originals.items():
        target = temp / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    for patch in patches:
        subprocess.run(["git", "apply", "--check", str(patch)], cwd=temp, check=True)
        subprocess.run(["git", "apply", str(patch)], cwd=temp, check=True)
    for path in paths:
        assert (temp / path).read_bytes() == (sdk / path).read_bytes(), f"unrecorded SDK source: {path}"
    for patch in reversed(patches):
        subprocess.run(["git", "apply", "--reverse", str(patch)], cwd=temp, check=True)
    for path, original in originals.items():
        assert (temp / path).read_bytes() == original, f"reverse did not restore pin: {path}"
result = {"result": "passed", "pin": pin, "patches": len(patches), "files": len(paths),
          "scope": "fresh apply, exact qualified source bytes, reverse to original; no duplicate SDK checkout retained",
          "patch_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in patches}}
print(json.dumps(result, indent=2))
