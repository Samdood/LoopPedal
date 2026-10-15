#!/usr/bin/env python3
"""
Diagnose noise sources in a recording captured from the Daisy Seed.

Reads a WAV file (mono, 16-bit, expected 48 kHz from grab_recording.py),
computes the spectrum, and prints a human-readable report:

  - overall level (RMS, peak)
  - mains-hum content at 50/60/120/180/240 Hz
  - top tonal peaks (narrow spikes above the broadband noise floor)
  - flags for likely switching-supply whine in 5-20 kHz

Usage:
    python3 tools/analyze_noise.py [noise_test.wav]
"""

import math
import struct
import sys
import wave
from typing import List, Tuple

try:
    import numpy as np
except ImportError:
    sys.exit("numpy not installed. Run: pip3 install numpy")


def db(x: float) -> float:
    if x <= 0:
        return -200.0
    return 20.0 * math.log10(x)


def load_wav(path: str) -> Tuple[np.ndarray, int]:
    with wave.open(path, "rb") as w:
        ch     = w.getnchannels()
        sw     = w.getsampwidth()
        sr     = w.getframerate()
        frames = w.readframes(w.getnframes())
    if sw != 2:
        sys.exit(f"Expected 16-bit PCM, got {sw*8}-bit")
    data = np.frombuffer(frames, dtype="<i2").astype(np.float32) / 32768.0
    if ch == 2:
        data = data.reshape(-1, 2).mean(axis=1)
    return data, sr


def find_peaks(mag_db: np.ndarray, freqs: np.ndarray,
               min_prom_db: float = 12.0,
               min_freq: float = 30.0,
               max_freq: float = 22000.0) -> List[Tuple[float, float]]:
    """Find narrow spectral peaks that stand at least `min_prom_db` above
    a local median (rolling 200-bin window) — i.e. tonal components above
    the broadband noise floor."""
    n = len(mag_db)
    win = 100  # bins on each side
    floor = np.empty_like(mag_db)
    # rolling median is slow in pure python; use numpy convolution-ish trick
    cum = np.cumsum(np.sort(mag_db))
    # Approximation: use a moving median via partition
    for i in range(n):
        a = max(0, i - win)
        b = min(n, i + win + 1)
        floor[i] = np.median(mag_db[a:b])

    peaks: List[Tuple[float, float]] = []
    for i in range(1, n - 1):
        f = freqs[i]
        if f < min_freq or f > max_freq:
            continue
        if mag_db[i] <= mag_db[i - 1] or mag_db[i] <= mag_db[i + 1]:
            continue
        prom = mag_db[i] - floor[i]
        if prom >= min_prom_db:
            peaks.append((f, mag_db[i]))
    # Merge peaks within 1.5 % freq, keep loudest
    peaks.sort()
    merged: List[Tuple[float, float]] = []
    for f, lvl in peaks:
        if merged and abs(math.log2(f / merged[-1][0])) < 0.02:
            if lvl > merged[-1][1]:
                merged[-1] = (f, lvl)
        else:
            merged.append((f, lvl))
    merged.sort(key=lambda p: -p[1])
    return merged


def level_at(freqs: np.ndarray, mag_db: np.ndarray, target: float,
             halfwidth_hz: float = 2.0) -> float:
    mask = (freqs >= target - halfwidth_hz) & (freqs <= target + halfwidth_hz)
    if not np.any(mask):
        return -200.0
    return float(np.max(mag_db[mask]))


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else "noise_test.wav"
    data, sr = load_wav(path)

    n = len(data)
    duration = n / sr

    # ---- Time-domain stats ----
    rms  = float(np.sqrt(np.mean(data * data)))
    peak = float(np.max(np.abs(data)))
    dc   = float(np.mean(data))

    # ---- Spectrum (Hann-windowed FFT, single shot) ----
    # Use a power-of-two slice for clean FFT; up to all of `data`.
    N = 1 << int(math.log2(n))
    seg = data[:N] - dc
    win = np.hanning(N).astype(np.float32)
    seg *= win
    win_correction = 2.0 / np.sum(win)   # so peaks read true amplitude
    spec = np.fft.rfft(seg)
    mag  = np.abs(spec) * win_correction
    mag_db = 20.0 * np.log10(np.maximum(mag, 1e-12))
    freqs = np.fft.rfftfreq(N, 1.0 / sr)

    # ---- Broadband noise-floor estimate ----
    band = (freqs > 500) & (freqs < 5000)
    floor_db = float(np.median(mag_db[band]))

    # ---- Mains hum scan ----
    mains_freqs = [50, 60, 100, 120, 150, 180, 200, 240]
    mains_levels = {f: level_at(freqs, mag_db, f, halfwidth_hz=2.0)
                    for f in mains_freqs}

    # ---- Tonal peaks ----
    peaks = find_peaks(mag_db, freqs)

    # ---- Switching-supply whine band 5-20 kHz ----
    hf_band = (freqs > 5000) & (freqs < 20000)
    hf_floor = float(np.median(mag_db[hf_band]))
    hf_peak  = float(np.max(mag_db[hf_band]))
    hf_peak_freq = float(freqs[hf_band][np.argmax(mag_db[hf_band])])

    # ---- Report ----
    print(f"file:          {path}")
    print(f"sample rate:   {sr} Hz")
    print(f"duration:      {duration:.2f} s")
    print(f"DC offset:     {dc:+.5f}  ({db(abs(dc)):+.1f} dBFS)")
    print(f"RMS:           {rms:.5f}  ({db(rms):+.1f} dBFS)")
    print(f"Peak:          {peak:.5f}  ({db(peak):+.1f} dBFS)")
    print(f"Noise floor (500 Hz – 5 kHz, median): {floor_db:+.1f} dBFS")
    print()

    print("Mains-hum scan (peak within ±2 Hz of each candidate):")
    print("  freq    level    delta-vs-floor")
    for f in mains_freqs:
        lvl = mains_levels[f]
        delta = lvl - floor_db
        flag = "  <-- prominent" if delta > 18 else ("  (mild)" if delta > 10 else "")
        print(f"  {f:>4d} Hz  {lvl:+6.1f} dBFS   {delta:+6.1f} dB{flag}")
    print()

    if peaks:
        print(f"Top tonal peaks (≥12 dB above local floor), highest first:")
        print("  freq        level")
        for f, lvl in peaks[:15]:
            note_hint = ""
            if 55 <= f <= 65:    note_hint = " (60 Hz mains)"
            elif 45 <= f <= 55:  note_hint = " (50 Hz mains)"
            elif 115 <= f <= 125: note_hint = " (120 Hz, 2nd hum harmonic)"
            elif 175 <= f <= 185: note_hint = " (180 Hz, 3rd hum harmonic)"
            elif f > 5000:       note_hint = " (HF — possible SMPS whine)"
            print(f"  {f:>8.1f} Hz  {lvl:+6.1f} dBFS{note_hint}")
    else:
        print("No prominent tonal peaks found above broadband noise floor.")
    print()

    print("High-frequency band (5 – 20 kHz):")
    print(f"  median floor: {hf_floor:+.1f} dBFS")
    print(f"  peak:         {hf_peak:+.1f} dBFS at {hf_peak_freq:.0f} Hz")
    if hf_peak - hf_floor > 15:
        print("  -> sharp tone in HF band; likely switching-supply / clock leakage.")
    else:
        print("  -> no significant HF tones.")
    print()

    # ---- Verdict ----
    print("=== verdict ===")
    diagnoses = []
    if mains_levels[60] - floor_db > 18 or mains_levels[50] - floor_db > 18:
        diagnoses.append("strong mains hum (60 Hz or 50 Hz) — shielding / ground loop")
    if mains_levels[120] - floor_db > 15 or mains_levels[180] - floor_db > 15:
        diagnoses.append("mains harmonics — typical of unshielded single-coil pickups or ground loop")
    if hf_peak - hf_floor > 15:
        diagnoses.append("HF tone in 5-20 kHz — switching power supply leakage")
    if floor_db > -75 and rms < 0.05:
        diagnoses.append("broadband floor is high relative to signal — input gain too low / op-amp noise")
    if not diagnoses:
        print("No dominant tonal noise source detected. Noise may be broadband")
        print("(thermal / op-amp / codec). Try (a) shielded input cable, (b) battery")
        print("power instead of USB, (c) raise input gain so the signal is well above")
        print("the noise floor before going into the codec.")
    else:
        for d in diagnoses:
            print(" - " + d)

    return 0


if __name__ == "__main__":
    sys.exit(main())
