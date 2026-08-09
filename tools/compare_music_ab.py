#!/usr/bin/env python3
"""Compare port dumps under build/music-ab against reference_music WAVs."""
from __future__ import annotations

import array
import math
import os
import sys
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AB = os.path.join(ROOT, "build", "music-ab")

SONGS = [
    ("0x21", "Bird Song", 46),
    ("0x26", "Jine the Cavalry", 71),
    ("0x2b", "Hole In The Wall", 72),
    ("0x37", "Indian Victory", 48),
]


def load_mono(path: str, max_sec: float | None = None):
    with wave.open(path, "rb") as w:
        rate = w.getframerate()
        ch = w.getnchannels()
        n = w.getnframes()
        if max_sec is not None:
            n = min(n, int(rate * max_sec))
        raw = w.readframes(n)
    samples = array.array("h")
    samples.frombytes(raw)
    if ch == 2:
        mix = [(samples[i] + samples[i + 1]) * 0.5 for i in range(0, len(samples) - 1, 2)]
    else:
        mix = list(samples)
    return rate, mix


def rms(mix):
    if not mix:
        return 0.0
    return math.sqrt(sum(s * s for s in mix) / len(mix))


def peak(mix):
    return max((abs(s) for s in mix), default=0.0)


def lr_corr(path: str, max_sec: float = 15.0):
    with wave.open(path, "rb") as w:
        rate = w.getframerate()
        ch = w.getnchannels()
        n = min(w.getnframes(), int(rate * max_sec))
        samples = array.array("h")
        samples.frombytes(w.readframes(n))
    if ch != 2:
        return 1.0
    L = samples[0::2]
    R = samples[1::2]
    mL = sum(L) / len(L)
    mR = sum(R) / len(R)
    num = sum((L[i] - mL) * (R[i] - mR) for i in range(len(L)))
    den = math.sqrt(
        sum((x - mL) ** 2 for x in L) * sum((x - mR) ** 2 for x in R)
    ) or 1.0
    return num / den


def zcr(mix, rate):
    if len(mix) < 2:
        return 0.0
    z = sum(1 for i in range(1, len(mix)) if (mix[i - 1] >= 0) != (mix[i] >= 0))
    return z / (len(mix) / rate)


def onset_period(mix, rate, hop=512):
    env = []
    for i in range(0, len(mix) - hop, hop):
        chunk = mix[i : i + hop]
        env.append(sum(abs(x) for x in chunk) / hop)
    d = [max(0.0, env[i] - env[i - 1]) for i in range(1, len(env))]
    if len(d) < 8:
        return None
    mean = sum(d) / len(d)
    x = [v - mean for v in d]
    lo = int(0.4 * rate / hop)
    hi = int(3.0 * rate / hop)
    best_lag, best_v = None, -1.0
    for lag in range(lo, min(hi, len(x) // 2)):
        v = sum(x[i] * x[i + lag] for i in range(len(x) - lag))
        if v > best_v:
            best_v, best_lag = v, lag
    return (best_lag * hop / rate) if best_lag else None


def main():
    print(f"Comparing under {AB}")
    print(
        f"{'id':4} {'title':22} {'sec':>4} {'rmsR':>7} {'rmsP':>7} "
        f"{'zcrR':>7} {'zcrP':>7} {'lrR':>5} {'lrP':>5} {'perR':>6} {'perP':>6}"
    )
    for sid, title, sec in SONGS:
        ref = os.path.join(AB, f"{sid}_ref.wav")
        port = os.path.join(AB, f"{sid}_port.wav")
        if not os.path.exists(ref) or not os.path.exists(port):
            print(f"{sid} MISSING ref/port — run dump_gsound_wav --ab first")
            continue
        _, rmix = load_mono(ref, sec)
        _, pmix = load_mono(port, sec)
        rate = 44100
        pr = onset_period(rmix, rate)
        pp = onset_period(pmix, rate)
        print(
            f"{sid:4} {title:22} {sec:4d} "
            f"{rms(rmix):7.1f} {rms(pmix):7.1f} "
            f"{zcr(rmix, rate):7.1f} {zcr(pmix, rate):7.1f} "
            f"{lr_corr(ref):5.2f} {lr_corr(port):5.2f} "
            f"{(pr or 0):6.2f} {(pp or 0):6.2f}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
