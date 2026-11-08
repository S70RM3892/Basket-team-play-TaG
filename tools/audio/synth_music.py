"""Background music for TaG, synthesised with numpy (no samples, no third-party audio).

  MusicMatch : Tetsuba industrial street beat, 100 BPM, 16 bars (38.4 s), seamless loop
  MusicMenu  : calm dusk loop over the bay, 84 BPM, 8 bars (~22.9 s), seamless loop

Writes 16-bit mono 44.1 kHz WAVs into tools/build/wav (picked up by tools/import/import_all.py like the SFX).
WORLD.md section 7: each district has its own genre; Tetsuba is industrial.
"""
import os
import wave

import numpy as np

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "wav")
RNG = np.random.default_rng(7)


def note_hz(n):
    """MIDI note number -> Hz."""
    return 440.0 * 2.0 ** ((n - 69) / 12.0)


def env_exp(n, tau):
    t = np.arange(n) / SR
    return np.exp(-t / tau)


def onepole_lp(x, fc):
    a = np.exp(-2.0 * np.pi * fc / SR)
    y = np.empty_like(x)
    acc = 0.0
    for i in range(len(x)):
        acc = (1 - a) * x[i] + a * acc
        y[i] = acc
    return y


def lp_fast(x, fc):
    """One-pole low-pass via an IIR filter implemented with cumulative tricks (vectorized approximation)."""
    # Use FFT-domain gentle low-pass for speed on long buffers.
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1.0 / SR)
    H = 1.0 / np.sqrt(1.0 + (f / fc) ** 2)
    return np.fft.irfft(X * H, n=len(x))


def hp_fast(x, fc):
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1.0 / SR)
    H = (f / fc) / np.sqrt(1.0 + (f / fc) ** 2)
    return np.fft.irfft(X * H, n=len(x))


def saw(freq, n, detune=0.0):
    t = np.arange(n) / SR
    ph = (freq * (1.0 + detune)) * t
    return 2.0 * (ph - np.floor(ph + 0.5))


def tri(freq, n):
    t = np.arange(n) / SR
    ph = freq * t
    return 2.0 * np.abs(2.0 * (ph - np.floor(ph + 0.5))) - 1.0


def kick(n=int(0.35 * SR)):
    t = np.arange(n) / SR
    f = 45.0 + 80.0 * np.exp(-t / 0.045)
    ph = 2 * np.pi * np.cumsum(f) / SR
    body = np.sin(ph) * np.exp(-t / 0.16)
    click = RNG.standard_normal(n) * np.exp(-t / 0.004) * 0.25
    return np.tanh((body + click) * 1.6) * 0.9


def snare(n=int(0.25 * SR)):
    t = np.arange(n) / SR
    noise = hp_fast(RNG.standard_normal(n), 1800.0) * np.exp(-t / 0.07)
    tone = np.sin(2 * np.pi * 185.0 * t) * np.exp(-t / 0.05) * 0.6
    return (noise * 0.7 + tone) * 0.6


def hat(n=int(0.05 * SR), open_=False):
    if open_:
        n = int(0.22 * SR)
    t = np.arange(n) / SR
    x = hp_fast(RNG.standard_normal(n), 7000.0)
    return x * np.exp(-t / (0.06 if open_ else 0.012)) * (0.22 if open_ else 0.18)


def clank(n=int(0.9 * SR)):
    """Industrial metal hit: inharmonic partials."""
    t = np.arange(n) / SR
    parts = [(523.0, 0.5), (1342.0, 0.35), (2210.0, 0.25), (3170.0, 0.2), (4400.0, 0.12)]
    y = np.zeros(n)
    for f, a in parts:
        y += np.sin(2 * np.pi * f * t + RNG.uniform(0, 6.28)) * a * np.exp(-t / (0.25 * (600.0 / f) ** 0.3))
    return y * 0.25


def place(buf, sig, start):
    """Add sig into buf at sample start, wrapping the tail to the beginning (seamless loop)."""
    n = len(buf)
    start %= n
    end = start + len(sig)
    if end <= n:
        buf[start:end] += sig
    else:
        first = n - start
        buf[start:] += sig[:first]
        buf[: end - n] += sig[first:]


def write(name, x, peak_db=-4.0):
    x = x - np.mean(x)
    peak = np.max(np.abs(x)) + 1e-9
    x = x / peak * (10 ** (peak_db / 20.0))
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype(np.int16).tobytes())
    print("wrote", path, "%.1f s" % (len(x) / SR))


def match_loop():
    bpm = 100.0
    beat = 60.0 / bpm
    bars = 16
    n = int(round(bars * 4 * beat * SR))
    drums = np.zeros(n)
    bass = np.zeros(n)
    pad = np.zeros(n)
    lead = np.zeros(n)
    s8 = beat / 2.0
    s16 = beat / 4.0
    # Am - F - C - G (two bars each), repeated
    roots = [57, 53, 48, 55]          # A3 F3 C3 G3
    chords = [[57, 60, 64, 67], [53, 57, 60, 64], [48, 52, 55, 59], [55, 59, 62, 65]]
    K, S = kick(), snare()
    for bar in range(bars):
        t0 = bar * 4 * beat
        # kick on 1 and 3, a syncopated push before 3 on odd bars
        for b in (0, 2):
            place(drums, K, int((t0 + b * beat) * SR))
        if bar % 2 == 1:
            place(drums, K * 0.7, int((t0 + 2 * beat - s16) * SR))
        # snare on 2 and 4 (ghost note late in every 4th bar)
        for b in (1, 3):
            place(drums, S, int((t0 + b * beat) * SR))
        if bar % 4 == 3:
            place(drums, S * 0.35, int((t0 + 3 * beat + 3 * s16) * SR))
        # hats on 8ths, open hat on the last 8th
        for i in range(8):
            h = hat(open_=(i == 7))
            place(drums, h * (1.0 if i % 2 else 0.6), int((t0 + i * s8) * SR))
        # industrial clank every two bars
        if bar % 2 == 0:
            place(drums, clank(), int((t0 + 1.5 * beat) * SR))
        # bass: 8th-note pattern on the chord root, octave jumps
        chord_i = (bar // 2) % 4
        root = roots[chord_i] - 12
        pattern = [0, None, 0, 12, None, 0, 7, None]
        for i, step in enumerate(pattern):
            if step is None:
                continue
            ln = int(s8 * 0.9 * SR)
            f = note_hz(root + step)
            tone = (saw(f, ln) * 0.6 + np.sin(2 * np.pi * f * np.arange(ln) / SR) * 0.7) * env_exp(ln, 0.18)
            place(bass, tone, int((t0 + i * s8) * SR))
        # pad: one chord per two bars (render once per chord)
        if bar % 2 == 0:
            ln = int(8 * beat * SR)
            env = np.minimum(1.0, np.arange(ln) / (0.8 * SR)) * np.minimum(1.0, (ln - np.arange(ln)) / (0.6 * SR))
            ch = np.zeros(ln)
            for m in chords[chord_i]:
                f = note_hz(m)
                ch += saw(f, ln, 0.003) + saw(f, ln, -0.004)
            place(pad, ch * env * 0.12, int(t0 * SR))
        # lead: pentatonic pluck motif in the second half
        if bar >= 8:
            scale = [69, 72, 74, 76, 79, 81]
            motif = [0, 2, 3, 2, 4, 3, 1, 0] if bar % 2 == 0 else [3, 4, 5, 4, 2, 3, 2, None]
            for i, m in enumerate(motif):
                if m is None:
                    continue
                ln = int(s8 * 1.6 * SR)
                f = note_hz(scale[m])
                tone = (tri(f, ln) * 0.6 + np.sin(2 * np.pi * 2 * f * np.arange(ln) / SR) * 0.15) * env_exp(ln, 0.12)
                place(lead, tone * 0.5, int((t0 + i * s8) * SR))
    # Sidechain duck the pad and bass on every beat (pumping feel)
    duck = np.ones(n)
    for b in range(bars * 4):
        s = int(b * beat * SR)
        ln = int(0.25 * SR)
        d = 1.0 - 0.55 * np.exp(-np.arange(ln) / (0.08 * SR))
        idx = (np.arange(ln) + s) % n
        duck[idx] = np.minimum(duck[idx], d)
    bass = lp_fast(bass, 380.0) * duck
    pad = lp_fast(pad, 1600.0) * duck
    lead = lp_fast(lead, 5000.0)
    mixdown = drums * 0.9 + bass * 0.9 + pad * 0.55 + lead * 0.5
    return np.tanh(mixdown * 1.1)


def menu_loop():
    bpm = 84.0
    beat = 60.0 / bpm
    bars = 8
    n = int(round(bars * 4 * beat * SR))
    drums = np.zeros(n)
    pad = np.zeros(n)
    arp = np.zeros(n)
    chords = [[57, 60, 64, 71], [53, 57, 60, 67], [50, 53, 57, 64], [52, 55, 59, 64]]   # Am9, Fmaj7, Dm7, Em7
    K = kick() * 0.6
    for bar in range(bars):
        t0 = bar * 4 * beat
        place(drums, K, int(t0 * SR))
        place(drums, K * 0.5, int((t0 + 2.5 * beat) * SR))
        rim = hp_fast(RNG.standard_normal(int(0.08 * SR)), 2500.0) * env_exp(int(0.08 * SR), 0.015) * 0.25
        place(drums, rim, int((t0 + 2 * beat) * SR))
        for i in range(8):
            place(drums, hat() * 0.5 * (1.0 if i % 2 else 0.5), int((t0 + i * beat / 2) * SR))
        ci = bar % 4
        ln = int(4 * beat * SR)
        env = np.minimum(1.0, np.arange(ln) / (1.2 * SR)) * np.minimum(1.0, (ln - np.arange(ln)) / (1.0 * SR))
        ch = np.zeros(ln)
        for m in chords[ci]:
            f = note_hz(m)
            ch += saw(f, ln, 0.004) + saw(f, ln, -0.005) + np.sin(2 * np.pi * f / 2 * np.arange(ln) / SR) * 0.5
        place(pad, ch * env * 0.1, int(t0 * SR))
        # gentle arpeggio, like lights flickering on across the water
        notes = chords[ci] + [chords[ci][1] + 12]
        for i in range(8):
            m = notes[(i * 2) % len(notes)] + 12
            l2 = int(beat * 0.9 * SR)
            f = note_hz(m)
            tone = np.sin(2 * np.pi * f * np.arange(l2) / SR) * env_exp(l2, 0.35)
            place(arp, tone * 0.18, int((t0 + i * beat / 2) * SR))
    pad = lp_fast(pad, 1200.0)
    mixdown = drums * 0.6 + pad * 0.8 + arp * 0.7
    # soft echo on the arpeggio side
    delay = int(beat * 0.75 * SR)
    mixdown = mixdown + np.roll(arp, delay) * 0.25 + np.roll(arp, 2 * delay) * 0.12
    return np.tanh(mixdown * 1.05)


if __name__ == "__main__":
    write("MusicMatch", match_loop(), -5.0)
    write("MusicMenu", menu_loop(), -6.0)
