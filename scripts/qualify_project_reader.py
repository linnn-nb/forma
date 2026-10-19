#!/usr/bin/env python3
"""Build an unchanged historical format reader using this qualified Make build.

No checkout/reset or SDK writes. Objects, binary and receipts stay in ignored
build output. Baseline is a full commit ID, not a moving branch. This probes
production ProjectDocument::read, not old audio/Undo implementations.
"""
import argparse
import hashlib
import json
import pathlib
import re
import shlex
import subprocess
import time
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=pathlib.Path, required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--fixture", type=pathlib.Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-f]{40}", args.baseline):
        raise ValueError("baseline must be an exact commit ID")
    repo = pathlib.Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    work = build / ("reader-baseline-" + args.baseline[:12] + "-" + uuid.uuid4().hex[:8])
    work.mkdir()
    (build / "p2-compatibility-historical-reader.json").write_text(json.dumps(
        dict(result="running", baseline_commit=args.baseline, work=str(work)), indent=2))
    source = subprocess.run(["git", "show", args.baseline + ":src/v2/ProjectDocument.cpp"], cwd=repo,
                            check=True, capture_output=True).stdout
    old_source = work / "ProjectDocument.cpp"
    old_source.write_bytes(source)
    headers = {}
    for name in ("ProjectDocument.h", "NativeEditXml.h"):
        data = subprocess.run(["git", "show", args.baseline + ":src/v2/" + name], cwd=repo,
                              check=True, capture_output=True).stdout
        (work / name).write_bytes(data)
        headers[name] = hashlib.sha256(data).hexdigest()
    flags = {}
    for line in (build / "CMakeFiles/ndaw_engine.dir/flags.make").read_text().splitlines():
        for key in ("CXX_DEFINES", "CXX_INCLUDES", "CXX_FLAGS"):
            if line.startswith(key + " = "):
                flags[key] = shlex.split(line.split(" = ", 1)[1])
    if len(flags) != 3:
        raise ValueError("expected qualified CMake Make build flags")
    link = shlex.split((build / "CMakeFiles/ndaw_project_document_tests.dir/link.txt").read_text())
    objects = []
    with (work / "build.log").open("w") as log:
        for file in (old_source, repo / "tests/v2/ProjectReaderProbe.cpp"):
            obj = work / (file.stem + ".o")
            command = [link[0], "-I", str(work)] + sum((flags[k] for k in ("CXX_DEFINES", "CXX_INCLUDES", "CXX_FLAGS")), [])
            command += ["-I", str(repo / "src/v2"), "-c", str(file), "-o", str(obj)]
            subprocess.run(command, cwd=build, stdout=log, stderr=log, check=True, timeout=120)
            objects.append(str(obj))
        binary = work / "historical-project-reader"
        marker = "CMakeFiles/ndaw_project_document_tests.dir/tests/v2/ProjectDocumentTests.cpp.o"
        if link.count(marker) != 1 or link.count("-o") != 1:
            raise ValueError("unexpected qualified linker template")
        at = link.index(marker)
        link[at:at + 1] = objects
        link[link.index("-o") + 1] = str(binary)
        subprocess.run(link, cwd=build, stdout=log, stderr=log, check=True, timeout=120)
    cases = []
    fixture = args.fixture.resolve()
    expected = [("Native-Sidechain", 0, ""), ("Frozen", 1, "封装版本"),
                ("Freeze-Redo-Only", 1, "封装版本"),
                ("Bounce-Only", 1, "封装版本"), ("Inactive-Print", 1, "封装版本")]
    for name, code, reason in expected:
        path = fixture / (name + ".tracktionedit")
        began = time.monotonic()
        proc = subprocess.run([str(binary), str(path)], capture_output=True, text=True, timeout=8)
        passed = proc.returncode == code and (not reason or reason in proc.stderr)
        cases.append(dict(name=name, path=str(path), exit_code=proc.returncode,
                          seconds=time.monotonic() - began, stderr=proc.stderr, passed=passed))
    current = build / "ndaw_artefacts/Release/ndaw"
    migrations = []
    for name in ("Frozen", "Freeze-Redo-Only", "Bounce-Only", "Inactive-Print"):
        old = work / (name + "-written-by-baseline.tracktionedit")
        if old.exists():
            raise ValueError("baseline writer fixture already exists; use a fresh qualification directory")
        subprocess.run([str(binary), "--legacy-copy", str(fixture / (name + ".tracktionedit")), str(old)],
                       check=True, capture_output=True, text=True, timeout=8)
        before = hashlib.sha256(old.read_bytes()).hexdigest()
        proc = subprocess.run([str(current), "inspect", str(old)], capture_output=True, text=True, timeout=15)
        (work / (name + "-current-open.stdout")).write_text(proc.stdout)
        (work / (name + "-current-open.stderr")).write_text(proc.stderr)
        migrations.append(dict(name=name, path=str(old), exit_code=proc.returncode, original_sha256=before,
                               passed=proc.returncode == 0 and hashlib.sha256(old.read_bytes()).hexdigest() == before))
    report = dict(result="passed" if all(c["passed"] for c in cases + migrations) else "failed",
                  baseline_commit=args.baseline, baseline_source_sha256=hashlib.sha256(source).hexdigest(),
                  baseline_headers_sha256=headers,
                  binary=str(binary), binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  scope="unchanged historical production format reader; not a complete old application or audio qualification",
                  cases=cases, actual_old_writer_migrations=migrations)
    (build / "p2-compatibility-historical-reader.json").write_text(json.dumps(report, ensure_ascii=False, indent=2))
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["result"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
