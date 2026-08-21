#!/usr/bin/env python3
"""Automated A/B comparator: port renders vs real-DOS-capture reference WAVs.

Ears are not a reliable/scalable comparator during sound porting. This does
chroma-DTW alignment (pitch-class content over time, synth-timbre invariant)
between each port render and its reference capture, and reports:

  - tempo_ratio: port tempo / reference tempo (from independent beat
    tracking on each side). This is the number that catches "notes right,
    speed wrong" bugs like the SOUND_TICK_HZ regression this tool was
    built to catch.
  - dtw_cost: normalized chroma-DTW path cost. Low + monotonic path =
    same melodic/harmonic content in the same order. High cost or a path
    that doesn't track the diagonal = wrong notes / wrong order / desync.
  - drift_rms_sec: RMS deviation of the DTW path from the best-fit affine
    line (port_time = a*ref_time + b). Near 0 = constant tempo relationship
    (even if a != 1, i.e. uniformly off-speed). Large = tempo *drifts*
    over the piece (inconsistent decoding, stuck/skip bugs, etc).

DTW runs banded (Sakoe-Chiba, band_rad=0.06) on purpose: these are march/
fiddle tunes with strong ~4s self-repeating phrases (AABB structure).
Unconstrained DTW can "teleport" onto a different, near-identical repeat of
the phrase instead of tracking true chronological time — a comparator false
positive, not a port bug. Confirmed on Jine the Cavalry: tightening the band
from unconstrained down to band_rad=0.01 dropped apparent drift 4.43s→0.83s
at nearly unchanged path cost (0.203→0.257), i.e. most of that "drift" was
the phrase-repeat trap, not real desync. band_rad=0.06 is the compromise —
tight enough to block repeat-jumps, loose enough not to mask a real global
tempo bug by force-fitting the diagonal.

Requires the venv at .venv-sound (numpy/scipy/librosa) — see
docs/ai_transcription.md or run:
  python3 -m venv --without-pip .venv-sound
  .venv-sound/bin/python3 <(curl -sS https://bootstrap.pypa.io/get-pip.py)
  .venv-sound/bin/pip install numpy scipy librosa soundfile matplotlib

Usage:
  tools/dump_gsound_wav --ab   # renders build/music-ab/*_port.wav + symlinks *_ref.wav
  .venv-sound/bin/python3 tools/compare_music_ab.py [--plot]
"""
from __future__ import annotations

import argparse
import os
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AB = os.path.join(ROOT, "build", "music-ab")
SR = 22050

SONGS = [
    ("0x21", "Bird Song"),
    ("0x26", "Jine the Cavalry"),
    ("0x2b", "Hole In The Wall"),
    ("0x37", "Indian Victory"),
]


def load(path: str):
    import librosa

    y, sr = librosa.load(path, sr=SR, mono=True)
    return y


def tempo_of(y: np.ndarray) -> float:
    import librosa

    onset_env = librosa.onset.onset_strength(y=y, sr=SR)
    tempo, _ = librosa.beat.beat_track(onset_envelope=onset_env, sr=SR)
    return float(np.atleast_1d(tempo)[0])


def chroma_of(y: np.ndarray, hop: int) -> np.ndarray:
    import librosa

    return librosa.feature.chroma_cqt(y=y, sr=SR, hop_length=hop)


def dtw_align(ref: np.ndarray, port: np.ndarray, hop: int = 512):
    import librosa

    c_ref = chroma_of(ref, hop)
    c_port = chroma_of(port, hop)
    cost_matrix = 1 - (c_ref.T @ c_port) / (
        np.linalg.norm(c_ref, axis=0)[:, None] * np.linalg.norm(c_port, axis=0)[None, :] + 1e-9
    )
    # Repetitive tunes (march/fiddle AABB phrases) give unconstrained DTW a
    # near-identical *other* repeat to lock onto instead of the true
    # chronological match — a comparator false positive, not a port bug.
    # A Sakoe-Chiba band forces the path to stay near the diagonal, i.e. near
    # the true 1:1-ish timing, so it can't "teleport" to a different repeat.
    D, wp = librosa.sequence.dtw(C=cost_matrix, global_constraints=True, band_rad=0.06)
    cost = D[-1, -1] / len(wp)
    wp = np.array(wp[::-1])  # dtw returns reversed (end->start)
    t_ref = wp[:, 0] * hop / SR
    t_port = wp[:, 1] * hop / SR
    # best-fit affine: t_port = a*t_ref + b
    a, b = np.polyfit(t_ref, t_port, 1)
    pred = a * t_ref + b
    drift_rms = float(np.sqrt(np.mean((t_port - pred) ** 2)))
    return cost, a, drift_rms, t_ref, t_port


def verdict(dtw_slope: float, cost: float, drift: float) -> str:
    # dtw_slope (best-fit port_time = a*ref_time + b over the DTW path) is the
    # trustworthy global-tempo readout — librosa.beat.beat_track is shown for
    # reference only, it's prone to octave/subdivision confusion on solo
    # melodic lines and shouldn't gate the verdict.
    bad = []
    if abs(dtw_slope - 1.0) > 0.08:
        bad.append(f"TEMPO OFF ({dtw_slope:.2f}x)")
    if cost > 0.35:
        bad.append(f"PITCH/CONTENT MISMATCH (cost={cost:.2f})")
    if drift > 1.5:
        bad.append(f"TIMING DRIFT ({drift:.2f}s, non-constant — see *_align.png stair-steps)")
    return "; ".join(bad) if bad else "OK"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--plot", action="store_true", help="write per-song alignment PNGs to build/music-ab/")
    args = ap.parse_args()

    if not os.path.isdir(AB):
        print(f"missing {AB} — run: build/debug/dump_gsound_wav --ab", file=sys.stderr)
        return 1

    print(f"Comparing under {AB}")
    hdr = (
        f"{'id':5} {'title':18} {'bpmR':>6} {'bpmP':>6} {'dtwA':>6} {'dtw':>6} {'drift':>7}  verdict"
    )
    print(hdr)
    exit_code = 0
    for sid, title in SONGS:
        ref_path = os.path.join(AB, f"{sid}_ref.wav")
        port_path = os.path.join(AB, f"{sid}_port.wav")
        if not os.path.exists(ref_path) or not os.path.exists(port_path):
            print(f"{sid} MISSING ref/port — run tools/dump_gsound_wav --ab first")
            exit_code = 1
            continue

        y_ref = load(ref_path)
        y_port = load(port_path)
        t_ref = tempo_of(y_ref)
        t_port = tempo_of(y_port)
        cost, a, drift, xr, xp = dtw_align(y_ref, y_port)
        v = verdict(a, cost, drift)
        if v != "OK":
            exit_code = 1
        print(
            f"{sid:5} {title:18} {t_ref:6.1f} {t_port:6.1f} {a:6.3f} {cost:6.2f} {drift:7.2f}  {v}"
        )

        if args.plot:
            import matplotlib

            matplotlib.use("Agg")
            import matplotlib.pyplot as plt

            plt.figure(figsize=(5, 5))
            plt.plot(xr, xp, ".", ms=1)
            plt.plot([0, max(xr)], [0, max(xr)], "r--", lw=0.5, label="1:1")
            plt.xlabel("reference time (s)")
            plt.ylabel("port time (s)")
            plt.title(f"{sid} {title}  a={a:.3f} drift={drift:.2f}s")
            plt.legend()
            out = os.path.join(AB, f"{sid}_align.png")
            plt.savefig(out, dpi=110)
            plt.close()
            print(f"  wrote {out}")

    print(
        "\nbpmR/bpmP: librosa beat-tracker tempo, reference vs port — informational only, "
        "prone to octave/subdivision confusion on solo melodic lines.\n"
        "dtwA: DTW-path best-fit slope, port_time = dtwA*ref_time — the trustworthy global "
        "tempo ratio (1.00 = matched speed).\n"
        "dtw: normalized chroma path cost (0 = identical pitch content/order, >~0.35 = suspect).\n"
        "drift: RMS deviation from the best-fit line, seconds. Non-zero with dtwA~1.0 means "
        "*local* stalls/desyncs (see --plot: look for stair-step flats), not a global tempo bug."
    )
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
