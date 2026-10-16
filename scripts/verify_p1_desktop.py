#!/usr/bin/env python3
"""Read-only audit of the four-second P1 OS desktop/microphone acceptance run.

This consumes real query_session receipts, saved projects and captured/rendered
WAVs. It neither drives a UI nor generates recording or execution receipts.
It does not qualify physical latency, listening, multitrack or endurance.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys
import wave
import xml.etree.ElementTree as ET


class Audit:
    def __init__(self, root):
        self.root = root.resolve()
        self.checks = []
        self.files = {}

    def check(self, condition, description):
        if not condition:
            raise ValueError(description)
        self.checks.append(description)

    def query(self, name):
        envelope = json.loads((self.root / (name + ".json")).read_text())
        self.check("error" not in envelope and not envelope["result"].get("isError", False),
                   name + ": successful read-only execution receipt")
        return envelope["result"]["structuredContent"]["result"]

    def project(self, name):
        path = self.root / name
        self.files[name] = {"sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        # JUCE's binary attributes use an unbound base64: name. Only the parser's
        # in-memory attribute names change; source file bytes are never rewritten.
        tree = ET.fromstring(re.sub(r"(\s)base64:", r"\1base64_", path.read_text()))
        edit = tree if tree.tag == "EDIT" else tree.find("EDIT")
        self.check(edit is not None, name + ": actual Edit payload exists")
        return edit

    def wav(self, path, channels, frames):
        self.check(path.resolve().is_relative_to(self.root.resolve()),
                   path.name + ": media remains inside the owned test directory")
        with wave.open(str(path), "rb") as reader:
            self.check((reader.getnchannels(), reader.getsampwidth(), reader.getframerate(),
                        reader.getnframes(), reader.getcomptype()) == (channels, 3, 48000, frames, "NONE"),
                       path.name + ": actual PCM24/48k format, channel and frame count")
            self.check(0 < frames <= 2_000_000, path.name + ": bounded independent decoding")
            pcm = reader.readframes(frames)
        self.check(len(pcm) == frames * channels * 3, path.name + ": complete PCM data")
        samples = []
        for offset in range(0, len(pcm), 3):
            value = int.from_bytes(pcm[offset:offset + 3], "little", signed=True)
            samples.append(value / 8388608.0)
        peak = max(abs(x) for x in samples)
        rms = math.sqrt(math.fsum(x * x for x in samples) / len(samples))
        self.check(peak > 0 and rms > 0, path.name + ": independently decoded nonzero audio")
        self.files[str(path.relative_to(self.root))] = {
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "pcm_sha256": hashlib.sha256(pcm).hexdigest(), "frames": frames,
            "channels": channels, "sample_rate": 48000, "pcm_bits": 24,
            "peak": peak, "rms": rms,
        }
        return pcm, samples

    def history_cycle(self, before, applied, undone, redone):
        b, a, u, r = [self.query(n) for n in (before, applied, undone, redone)]
        self.check(u["tracks"] == b["tracks"], applied + ": original Undo restores complete track facts")
        self.check(r["tracks"] == a["tracks"], applied + ": original Redo restores complete track facts")
        self.check(a["history"]["undo"] == u["history"]["redo"] == r["history"]["undo"],
                   applied + ": same original Plan crosses Undo/Redo")
        self.check(b["history"]["retention"]["retained_steps"] + 1 ==
                   a["history"]["retention"]["retained_steps"],
                   applied + ": exactly one engineering transaction")
        return a

    def run(self):
        loop = self.history_cycle("loop-ready", "loop-recorded", "loop-undone", "loop-redone")
        punch = self.history_cycle("punch-ready", "punch-recorded", "punch-undone", "punch-redone")
        comp = self.history_cycle("comp-before", "comp-created", "comp-undone", "comp-redone")
        promote = self.history_cycle("promote-before", "promote-applied", "promote-undone", "promote-redone")
        first = self.history_cycle("fade-before", "fade1-applied", "fade1-undone", "fade1-redone")
        self.check(comp["tracks"][0]["playlists"]["active"] == punch["tracks"][0]["playlists"]["active"],
                   "creating a candidate Comp leaves original playback active")
        raw = []
        for query, mode in [(loop, "loop"), (punch, "punch")]:
            receipt = query["last_recording"]
            self.check(receipt["state"] == "committed" and receipt["outcome"] == "success"
                       and receipt["mode"] == mode and not receipt["error"], mode + ": real writer committed")
            self.check(len(receipt["files"]) == 1, mode + ": one continuous capture file")
            file = receipt["files"][0]
            path = Path(file["path"])
            pcm, samples = self.wav(path, 1, file["frames"])
            measured = self.files[str(path.relative_to(self.root))]
            self.check(measured["sha256"] == file["media_hash"], mode + ": original media hash remains unchanged")
            self.check(abs(measured["peak"] - file["peak"]) <= 1 / 8388608 and
                       abs(measured["rms"] - file["rms"]) <= 1 / 8388608,
                       mode + ": independent peak/RMS agree with recording receipt within one PCM24 LSB")
            raw.append(file)
        takes = [p for p in loop["tracks"][0]["playlists"]["items"] if p["role"] == "take"]
        self.check([p["source_offset_samples"] for p in takes] == [0, 192000, 384000, 576000]
                   and [p["length_samples"] for p in takes] == [192000] * 3 + [raw[0]["frames"] - 576000]
                   and [p["complete"] for p in takes] == [True, True, True, False],
                   "Loop retains three complete takes and one exact continuous-source partial take")
        self.check(punch["last_recording"]["automatic_stop"] and
                   punch["last_recording"]["punch_range"] == {
                       "capture_end_samples": 168000, "capture_start_samples": 24000,
                       "end_samples": 144000, "sample_rate": 48000, "start_samples": 48000,
                       "timebase": "session_samples"}, "Punch uses the declared 1–3s range and 0.5s pre/post roll")
        chosen = promote["tracks"][0]["clips"]
        self.check([(c["start_samples"], c["length_samples"], c["source_offset_samples"]) for c in chosen] ==
                   [(0, 48000, 0), (48000, 96000, 24000), (144000, 48000, 144000)],
                   "Comp promotes only the actual Punch selection, keeping Take 1 on both sides")
        self.check([c["path"] for c in chosen] == [raw[0]["path"], raw[1]["path"], raw[0]["path"]],
                   "Comp pieces reference captured originals instead of synthesized substitutes")
        final = self.query("final-before-restart")
        cold = self.query("final-cold-open")
        undo = self.query("final-cold-undone")
        redo = self.query("final-cold-redone")
        self.check(final["session_token"] != cold["session_token"], "cold open has a new actual session token")
        self.check(final["tracks"] == cold["tracks"] == redo["tracks"],
                   "cold file open and original Redo restore complete final track facts")
        self.check(first["tracks"] == undo["tracks"], "cold original Undo removes only the second crossfade")
        self.check(final["history"]["undo"] == cold["history"]["undo"] == undo["history"]["redo"] ==
                   redo["history"]["undo"], "cold Undo/Redo retains the original second-fade Plan")
        track = final["tracks"][0]
        fades = track["crossfades"]
        self.check([(f["start_samples"], f["join_samples"], f["end_samples"], f["curve"]) for f in fades] ==
                   [(46800, 48000, 49200, "equal_power"), (142800, 144000, 145200, "equal_power")],
                   "two actual equal-power 50ms crossfades have the declared geometry")
        self.check([(c["start_samples"], c["length_samples"], c["source_offset_samples"]) for c in track["clips"]] ==
                   [(0, 49200, 0), (46800, 98400, 22800), (142800, 49200, 142800)],
                   "crossfades extend real source handles without slipping source time")
        files = []
        for name in ("01-hard-cut.wav", "02-final.wav", "03-cold-final.wav"):
            files.append(self.wav(self.root / name, 2, 192000))
        self.check(files[1][0] == files[2][0], "cold re-export PCM is byte-for-byte equal to final export")
        self.check(files[0][0] != files[1][0], "crossfades actually change audio PCM")
        maximum = 0.0
        for frame in range(192000):
            if any(f["start_samples"] <= frame < f["end_samples"] for f in fades):
                continue
            for channel in range(2):
                i = frame * 2 + channel
                maximum = max(maximum, abs(files[0][1][i] - files[1][1][i]))
        self.check(maximum <= 2e-7, "outside declared crossfades PCM stays within the original 2e-7 budget")
        edit = self.project("P1-Microphone-Comp-Final.tracktionedit")
        history = edit.find("NATIVEDAW/PERSISTENT_HISTORY")
        self.check(history is not None and history.get("cursor") == "22" and len(history.findall("STEP")) == 22,
                   "actual final project stores all 22 original history steps")
        self.check(self.files["P1-Microphone-Comp-Final.tracktionedit"]["sha256"] ==
                   "f3a523e65c80571b3195e3c5bedbfbd491137110cf532ae14d35056715daa1fb",
                   "final saved project bytes remain unchanged through cold Undo/Redo and re-export")
        native_track = edit.find("TRACK")
        playlists = native_track.find("NDAW_PLAYLISTS")
        self.check(len(playlists.findall("PLAYLIST")) == 8 and
                   playlists.get("active") == track["playlists"]["active"],
                   "saved file retains all eight Main/Take/Punch/Comp containers")
        prior = self.project("Exit-Punch.tracktionedit").find("TRACK")
        for take in takes + [p for p in punch["tracks"][0]["playlists"]["items"] if p["name"] == "Punch Take 5"]:
            selector = "PLAYLIST[@id='" + take["id"] + "']"
            a = prior.find("NDAW_PLAYLISTS/" + selector)
            b = playlists.find(selector)
            self.check(a is not None and b is not None and ET.tostring(a) == ET.tostring(b),
                       take["name"] + ": complete inactive source tree preserved in actual file")
        original = self.query("original-running-query")
        restored = self.query("original-restored-verified")
        self.check(original["tracks"] == restored["tracks"] and original["history"] == restored["history"],
                   "user Warp Comp complete track facts and nine-step original history restored")
        self.check(restored["position_samples"] == original["position_samples"] == 77385 and
                   not restored["playing"] and not restored["recording"],
                   "user playhead restored; transport and recording stopped")
        device = restored["audio_configuration"]["actual"]
        self.check(device["input"] == "" and device["input_channels"] == [] and device["output"] == "外置耳机"
                   and device["sample_rate"] == 48000 and device["buffer_frames"] == 512,
                   "original physical audio configuration restored (native UI separately reports verified)")
        self.project("Original-Warp-Comp.tracktionedit")
        self.check(self.files["Original-Warp-Comp.tracktionedit"]["sha256"] ==
                   "825f2e6f4b2316b9433210e95d61760216a1cdce635b1c1cd644f2ad81fba03c",
                   "preserved original full project has its pre-test byte hash")
        return {"result": "passed", "checks": len(self.checks), "assertions": self.checks,
                "files": self.files, "outside_fades_max_error": maximum,
                "outside_fades_tolerance": 2e-7,
                "device_query_state": restored["audio_configuration"]["state"],
                "scope": "read-only independent audit of actual OS desktop/microphone files and receipts; "
                         "not a UI driver, listening, hardware latency, endurance or complete P1 qualification"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    args.report.write_text(json.dumps({"result": "running", "checks": 0}) + "\n")
    audit = Audit(args.root)
    try:
        result = audit.run()
    except Exception as error:
        args.report.write_text(json.dumps({"result": "failed", "checks": len(audit.checks),
                                          "error": str(error)}, ensure_ascii=False, indent=2) + "\n")
        raise
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"result": result["result"], "checks": result["checks"],
                      "outside_fades_max_error": result["outside_fades_max_error"]}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
