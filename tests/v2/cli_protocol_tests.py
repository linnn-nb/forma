#!/usr/bin/env python3
"""Real child-process CLI checks; stdout/stderr remain separate, never scraped."""
import argparse
import json
from pathlib import Path
import subprocess
import uuid


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    root = args.report.parent / ("CLI-Protocol-Test-" + uuid.uuid4().hex[:12])
    root.mkdir(parents=True)
    checks, cases = [], []

    def check(ok, why):
        if not ok:
            raise AssertionError(why)
        checks.append(why)

    args.report.write_text(json.dumps({"result": "running"}))
    try:
        for name, params, code in [("probe", ["probe"], 0),
                                   ("missing", ["inspect", str(root / "missing.tracktionedit")], 1),
                                   ("usage", ["unknown-command"], 2)]:
            p = subprocess.run([str(args.cli), *params], stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, timeout=20)
            (root / (name + ".stdout")).write_bytes(p.stdout)
            (root / (name + ".stderr")).write_bytes(p.stderr)
            check(p.returncode == code, name + ": actual process exit code")
            value = json.loads(p.stdout)
            check(isinstance(value, dict), name + ": stdout contains one strict JSON object")
            if code:
                check(value["status"] == "failed" and value["error"], name + ": actual failure receipt")
            cases.append({"name": name, "exit": p.returncode, "stdout_bytes": len(p.stdout),
                          "stderr_bytes": len(p.stderr)})
        result = {"result": "passed", "checks": len(checks), "cases": cases, "fixture": str(root)}
    except Exception as error:
        result = {"result": "failed", "checks": len(checks), "cases": cases,
                  "fixture": str(root), "error": str(error)}
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["result"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
