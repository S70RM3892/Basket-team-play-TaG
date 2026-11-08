"""
synth_sfx.py - procedural sound effects for Basket Team-play TaG (16-bit mono 44.1 kHz WAV, peak ~ -3 dBFS).

  python C:/TaG/tools/audio/synth_sfx.py [--out C:/TaG/tools/build/wav]

Uses numpy (falls back with a clear error if it is missing).  Deterministic: fixed random seeds.
AmbientLoop is built from components that are exactly periodic over the loop length (FFT-filtered noise of the
loop length, LFOs and tones with an integer number of cycles), so it loops seamlessly.
"""
import json
import os
import sys
import wave

try:
    import numpy as np
except ImportError:
    sys.exit("numpy is required: python -m pip install numpy")

SR = 44100
PEAK = 10 ** (-3.0 / 20.0)


# ------------------------------------------------------------------------------------------------ building blocks
def t_axis(dur):
    return np.arange(int(round(dur * SR))) / SR


def rng(seed):
    return np.random.default_rng(seed)


def osc(freq, dur, kind="sine", phase=0.0):
    """freq: float or array (Hz) -> phase-accumulated oscillator."""
    n = int(round(dur * SR))
    f = np.full(n, float(freq)) if np.isscalar(freq) else np.asarray(freq, dtype=float)[:n]
    ph = 2 * np.pi * np.cumsum(f) / SR + phase
    if kind == "sine":
        return np.sin(ph)
    if kind == "saw":
        return 2.0 * ((ph / (2 * np.pi)) % 1.0) - 1.0
    if kind == "square":
        return np.sign(np.sin(ph))
    if kind == "tri":
        return 2.0 * np.abs(2.0 * ((ph / (2 * np.pi)) % 1.0) - 1.0) - 1.0
    raise ValueError(kind)


def expdec(dur, tau, delay=0.0):
    t = t_axis(dur)
    e = np.exp(-np.maximum(t - delay, 0.0) / tau)
    e[t < delay] = 0.0
    return e


def ar(dur, attack, release_tau=None, hold=0.0):
    t = t_axis(dur)
    a = np.clip(t / max(attack, 1e-4), 0, 1)
    if release_tau is None:
        return a
    r = np.exp(-np.maximum(t - attack - hold, 0) / release_tau)
    return a * r


def sweep(f0, f1, dur, curve="exp"):
    t = t_axis(dur) / dur
    if curve == "exp":
        return f0 * (f1 / f0) ** t
    return f0 + (f1 - f0) * t


def glide(f0, f1, tg, dur):
    """Exponential glide from f0 to f1 over tg seconds, then hold (exactly dur seconds of samples)."""
    u = np.clip(t_axis(dur) / tg, 0, 1)
    return f0 * (f1 / f0) ** u


def bandpass_fft(x, lo, hi, soft=0.15):
    """Zero-phase band-pass in the frequency domain with soft (log) edges."""
    n = len(x)
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(n, 1 / SR)
    lf = np.log2(np.maximum(f, 1.0))
    w = np.ones_like(f)
    if lo > 0:
        w *= 1 / (1 + np.exp(-(lf - np.log2(lo)) / soft))
    if hi < SR / 2:
        w *= 1 / (1 + np.exp((lf - np.log2(hi)) / soft))
    return np.fft.irfft(X * w, n)


def onepole_lp(x, fc):
    a = np.exp(-2 * np.pi * fc / SR)
    y = np.empty_like(x)
    acc = 0.0
    for i in range(len(x)):
        acc = (1 - a) * x[i] + a * acc
        y[i] = acc
    return y


def tv_bandpass(x, fc, q=2.0):
    """Time-varying state-variable band-pass (fc array in Hz)."""
    y = np.zeros_like(x)
    low = band = 0.0
    fc = np.asarray(fc, dtype=float)
    for i in range(len(x)):
        f = 2 * np.sin(np.pi * min(fc[i], SR * 0.45) / SR)
        high = x[i] - low - band / q
        band += f * high
        low += f * band
        y[i] = band
    return y


def modal(freqs, decays, amps, dur, seed=0, jitter=0.0):
    t = t_axis(dur)
    out = np.zeros_like(t)
    r = rng(seed)
    for f, d, a in zip(freqs, decays, amps):
        ph = r.uniform(0, 2 * np.pi)
        ff = f * (1 + jitter * r.uniform(-1, 1))
        out += a * np.sin(2 * np.pi * ff * t + ph) * np.exp(-t / d)
    return out


def noise(dur, seed):
    return rng(seed).standard_normal(int(round(dur * SR)))


def fade(x, fin=0.002, fout=0.01):
    n = len(x)
    a, b = int(fin * SR), int(fout * SR)
    if a > 0:
        x[:a] *= np.linspace(0, 1, a)
    if b > 0:
        x[n - b:] *= np.linspace(1, 0, b) ** 2
    return x


def soft_clip(x, drive=1.0):
    return np.tanh(x * drive) / np.tanh(drive)


def mix(dur, *parts):
    n = int(round(dur * SR))
    out = np.zeros(n)
    for p in parts:
        m = min(n, len(p))
        out[:m] += p[:m]
    return out


def normalize(x, peak=PEAK):
    m = np.max(np.abs(x))
    return x * (peak / m) if m > 0 else x


def write_wav(path, x):
    x = np.clip(x, -1.0, 1.0)
    data = (x * 32767.0).astype("<i2").tobytes()
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data)


# ------------------------------------------------------------------------------------------------ the sounds
def bounce():
    d = 0.38
    f = sweep(190, 88, d)
    thump = osc(f, d) * expdec(d, 0.075)
    ping = modal([310, 640, 1210], [0.12, 0.07, 0.04], [0.35, 0.18, 0.08], d, 1)
    slap = bandpass_fft(noise(d, 2), 700, 3500) * expdec(d, 0.012)
    return mix(d, thump, ping, slap * 0.5)


def swish():
    d = 0.6
    n = noise(d, 3)
    fc = sweep(2600, 6200, d)
    s = tv_bandpass(n, fc, q=1.4) * ar(d, 0.03, 0.16)
    rattle = bandpass_fft(noise(d, 4), 3000, 9000) * ar(d, 0.06, 0.08) * (0.5 + 0.5 * np.sin(2 * np.pi * 23 * t_axis(d)))
    return mix(d, s, rattle * 0.25)


def rim():
    d = 1.0
    ring = modal([432, 1187, 2215, 3490, 4988, 6620], [0.55, 0.38, 0.24, 0.15, 0.09, 0.06],
                 [1.0, 0.6, 0.45, 0.3, 0.2, 0.12], d, 5)
    click = bandpass_fft(noise(d, 6), 1500, 8000) * expdec(d, 0.004)
    thud = osc(sweep(160, 90, d), d) * expdec(d, 0.04)
    return mix(d, ring * 0.7, click * 0.6, thud * 0.5)


def board():
    d = 0.5
    body = modal([176, 412, 758, 1290], [0.16, 0.11, 0.07, 0.04], [1.0, 0.6, 0.35, 0.15], d, 7)
    knock = bandpass_fft(noise(d, 8), 200, 2500) * expdec(d, 0.018)
    frame_ring = modal([2310, 3120], [0.18, 0.12], [0.08, 0.05], d, 9)
    return mix(d, body, knock * 0.8, frame_ring)


def shot_release():
    d = 0.32
    wh = tv_bandpass(noise(d, 10), sweep(900, 3200, d), 1.2) * ar(d, 0.02, 0.07)
    chirp = osc(sweep(420, 1300, d), d) * ar(d, 0.005, 0.06) * 0.35
    flick = bandpass_fft(noise(d, 11), 1500, 6000) * expdec(d, 0.006) * 0.6
    return mix(d, wh, chirp, flick)


def pass_():
    d = 0.28
    wh = tv_bandpass(noise(d, 12), sweep(700, 2600, d), 1.0) * ar(d, 0.015, 0.06)
    tone = osc(sweep(600, 900, d), d) * ar(d, 0.005, 0.05) * 0.15
    return mix(d, wh, tone)


def catch():
    d = 0.22
    slap = bandpass_fft(noise(d, 13), 250, 2200) * expdec(d, 0.02)
    thud = osc(sweep(170, 110, d), d) * expdec(d, 0.05)
    return mix(d, slap, thud * 0.8)


def tackle():
    d = 0.55
    boom = osc(sweep(95, 48, d), d) * expdec(d, 0.12)
    crunch = soft_clip(bandpass_fft(noise(d, 14), 150, 2000) * 3.0, 2.0) * expdec(d, 0.05)
    zap = osc(sweep(1800, 300, d), d, "saw") * expdec(d, 0.07) * 0.18
    zap = bandpass_fft(zap, 200, 5000)
    return mix(d, boom, crunch * 0.6, zap)


def block():
    d = 0.42
    slap = bandpass_fft(noise(d, 15), 400, 4500) * expdec(d, 0.015)
    shield = osc(sweep(520, 180, d), d, "square") * expdec(d, 0.09) * 0.3
    shield = bandpass_fft(shield, 100, 3000)
    thump = osc(sweep(150, 80, d), d) * expdec(d, 0.05)
    return mix(d, slap, shield, thump * 0.7)


def steal():
    d = 0.36
    swipe = tv_bandpass(noise(d, 16), sweep(1200, 4500, d), 1.3) * ar(d, 0.01, 0.05)
    tick = osc(1560, d) * expdec(d, 0.03, delay=0.05) * 0.6 + osc(2340, d) * expdec(d, 0.02, delay=0.05) * 0.3
    return mix(d, swipe, tick)


def bell(f, d, tau, seed):
    return modal([f, f * 2.0, f * 2.76, f * 4.07], [tau, tau * 0.6, tau * 0.4, tau * 0.25], [1, 0.5, 0.3, 0.15], d, seed)


def read_win():
    d = 0.9
    out = np.zeros(int(d * SR))
    for i, f in enumerate((1046.5, 1318.5, 1568.0, 2093.0)):
        b = bell(f, d, 0.35, 20 + i)
        s = int(i * 0.06 * SR)
        out[s:] += b[: len(out) - s] * (0.7 if i < 3 else 0.5)
    shimmer = bandpass_fft(noise(d, 24), 5000, 12000) * ar(d, 0.05, 0.25) * 0.15
    return out + shimmer


def read_lose():
    d = 0.65
    f = np.where(t_axis(d) < 0.22, 311.0, 233.0) * sweep(1.0, 0.92, d)
    tone = bandpass_fft(osc(f, d, "saw"), 80, 1400) * ar(d, 0.01, 0.25)
    return tone


def whistle():
    d = 0.75
    t = t_axis(d)
    trill = 1 + 0.5 * (0.5 + 0.5 * np.sin(2 * np.pi * 34 * t))
    f = 3050 + 60 * np.sin(2 * np.pi * 34 * t)
    tone = osc(f, d) * trill * 0.6 + osc(f * 2, d) * 0.08
    breath = bandpass_fft(noise(d, 25), 2000, 7000) * 0.12
    env = np.clip(t / 0.02, 0, 1) * np.clip((d - t) / 0.08, 0, 1)
    return (tone + breath) * env


def buzzer():
    d = 1.25
    t = t_axis(d)
    tone = osc(220, d, "saw") + 0.8 * osc(221.5, d, "saw") + 0.5 * osc(330, d, "square")
    tone = bandpass_fft(tone, 90, 1800, 0.25)
    env = np.clip(t / 0.015, 0, 1) * np.clip((d - t) / 0.06, 0, 1) * (0.75 + 0.25 * np.exp(-t / 0.3))
    return soft_clip(bandpass_fft(tone, 90, 1400, 0.3) * env * 0.6, 1.2)


def score():
    d = 1.7
    t = t_axis(d)
    rise = osc(glide(180, 820, 0.55, d), d)
    rise2 = osc(glide(360, 1640, 0.55, d), d) * 0.35
    up_env = np.clip(t / 0.5, 0, 1) ** 2 * np.exp(-np.maximum(t - 0.55, 0) / 0.35)
    hum = (osc(110, d) + 0.5 * osc(220, d) + 0.25 * osc(330, d) + 0.15 * osc(440, d, "saw"))
    hum_env = np.clip((t - 0.45) / 0.1, 0, 1) * np.exp(-np.maximum(t - 0.6, 0) / 0.6)
    zap = bandpass_fft(noise(d, 26), 3000, 10000) * np.exp(-np.abs(t - 0.56) / 0.05) * 0.4
    sparkle = sum(bell(f, d, 0.4, 30 + i) * np.clip((t - 0.55) * 50, 0, 1) for i, f in enumerate((1318.5, 1975.5)))
    return (rise + rise2) * up_env * 0.6 + hum * hum_env * 0.35 + zap + sparkle * 0.25


def drive():
    d = 0.75
    wh = tv_bandpass(noise(d, 27), sweep(300, 3200, d), 1.2) * (t_axis(d) / d) ** 1.5
    env = np.clip((d - t_axis(d)) / 0.08, 0, 1)
    sub = osc(sweep(60, 140, d), d) * (t_axis(d) / d) * 0.4
    return (wh + sub) * env


def dodge():
    d = 0.55
    t = t_axis(d)
    r = rng(28)
    out = np.zeros_like(t)
    for i in range(9):
        f = r.uniform(2200, 6500)
        out += np.sin(2 * np.pi * f * (1 + 0.04 * t) * t + r.uniform(0, 6.28)) * r.uniform(0.3, 1.0)
    trem = 0.6 + 0.4 * np.sin(2 * np.pi * 18 * t)
    env = np.clip(t / 0.03, 0, 1) * np.exp(-t / 0.18)
    swish_ = tv_bandpass(noise(d, 29), sweep(4000, 1500, d), 1.0) * env * 0.5
    return out * trem * env * 0.25 + swish_


def contest_hum():
    d = 1.2
    t = t_axis(d)
    hum = osc(90, d) + 0.9 * osc(92.3, d) + 0.45 * osc(180, d) + 0.3 * osc(271, d)
    buzz = bandpass_fft(osc(90, d, "saw"), 200, 1600) * 0.25
    env = np.clip(t / 0.12, 0, 1) * np.clip((d - t) / 0.25, 0, 1)
    wob = 0.8 + 0.2 * np.sin(2 * np.pi * 6 * t)
    return (hum * 0.5 + buzz) * env * wob


def tackle_cue():
    d = 0.25
    t = t_axis(d)
    f = sweep(160, 135, d)
    tone = osc(f, d) + 0.35 * osc(f * 3, d) + 0.12 * osc(f * 5, d)
    env = np.clip(t / 0.008, 0, 1) * np.clip((d - t) / 0.06, 0, 1) * np.exp(-t / 0.12)
    return tone * env


def step():
    d = 0.16
    thud = osc(sweep(120, 70, d), d) * expdec(d, 0.03)
    scuff = bandpass_fft(noise(d, 31), 500, 2500) * ar(d, 0.004, 0.02)
    return mix(d, thud, scuff * 0.35)


def foot(seed, f0):
    """A footfall: rubber sole on a sport court. A soft low thud from the heel, a short sole tick, a faint scuff."""
    d = 0.14
    thud = osc(sweep(f0, f0 * 0.6, d), d) * expdec(d, 0.022)
    body = bandpass_fft(noise(d, seed), 250, 1400) * expdec(d, 0.02)
    tick = bandpass_fft(noise(d, seed + 1), 2500, 7000) * expdec(d, 0.005)
    scuff = bandpass_fft(noise(d, seed + 2), 900, 3000) * ar(d, 0.012, 0.03)
    # the body and the tick carry it on small (phone) speakers, which barely play the low thud
    return mix(d, thud * 0.7, body * 1.6, tick * 0.6, scuff * 0.35)


def squeak(seed, f0, f1, d):
    """A sneaker squeak on a hard court (a hard cut or stop): the sole sticks and slips, a squeal with harmonics."""
    t = t_axis(d)
    r = rng(seed)
    f = glide(f0, f1, d * 0.45, d) * (1 + 0.02 * np.sin(2 * np.pi * 23 * t + r.uniform(0, 6.28)))
    tone = osc(f, d) + 0.45 * osc(f * 2, d) + 0.18 * osc(f * 3, d) + 0.06 * osc(f * 4.02, d)
    chatter = 0.7 + 0.3 * np.sin(2 * np.pi * 62 * t)
    env = np.clip(t / 0.012, 0, 1) * np.clip((d - t) / 0.05, 0, 1) * (0.6 + 0.4 * np.exp(-t / 0.08))
    hiss = bandpass_fft(noise(d, seed), 3000, 9000) * env * 0.08
    return tone * chatter * env + hiss


def down():
    d = 1.0
    t = t_axis(d)
    boom = osc(sweep(70, 38, d), d) * expdec(d, 0.22)
    crunch = soft_clip(bandpass_fft(noise(d, 32), 100, 1500) * 3, 2.0) * expdec(d, 0.06)
    whine = osc(glide(900, 90, 0.8, d), d, "tri") * \
        np.clip((t - 0.05) / 0.05, 0, 1) * np.exp(-np.maximum(t - 0.1, 0) / 0.3) * 0.25
    return mix(d, boom, crunch * 0.5, whine)


def revive():
    d = 0.95
    t = t_axis(d)
    rise = osc(glide(300, 900, 0.5, d), d) * \
        np.clip(t / 0.4, 0, 1) * np.exp(-np.maximum(t - 0.5, 0) / 0.2) * 0.4
    chime = bell(1174.7, d, 0.35, 33) * np.clip((t - 0.45) * 80, 0, 1) * 0.6
    chime2 = bell(1760.0, d, 0.3, 34) * np.clip((t - 0.55) * 80, 0, 1) * 0.4
    shimmer = bandpass_fft(noise(d, 35), 5000, 12000) * np.clip(t / 0.5, 0, 1) * np.exp(-t / 0.5) * 0.12
    return rise + chime + chime2 + shimmer


def art_cue():
    d = 0.85
    t = t_axis(d)
    pulses = np.zeros_like(t)
    for k, at in enumerate((0.0, 0.18)):
        p = osc(sweep(140, 70, d), d) * expdec(d, 0.05, delay=at)
        pulses += p
    rise = osc(sweep(330, 990, d), d, "tri") * np.clip((t - 0.3) / 0.3, 0, 1) * np.exp(-np.maximum(t - 0.6, 0) / 0.12)
    sync = bell(1318.5, d, 0.25, 36) * np.clip((t - 0.36) * 80, 0, 1)
    return pulses * 0.8 + rise * 0.3 + sync * 0.35


def ui_move():
    d = 0.06
    return osc(1250, d) * expdec(d, 0.012)


def ui_confirm():
    d = 0.2
    t = t_axis(d)
    f = np.where(t < 0.07, 880.0, 1320.0)
    return osc(f, d) * np.where(t < 0.07, np.exp(-t / 0.03), np.exp(-(t - 0.07) / 0.05))


def city_power_up():
    d = 2.6
    t = t_axis(d)
    hum = osc(glide(38, 82, 1.6, d), d)
    hum = hum + 0.5 * osc(glide(76, 164, 1.6, d), d)
    hum_env = np.clip(t / 1.4, 0, 1) ** 1.5 * np.clip((d - t) / 0.8, 0, 1)
    clicks = np.zeros_like(t)
    for k, at in enumerate((0.55, 0.8, 1.0, 1.15, 1.3, 1.42)):
        clicks += bandpass_fft(noise(d, 40 + k), 800, 6000) * expdec(d, 0.006, delay=at) * 0.6
    surge = bandpass_fft(noise(d, 50), 2000, 9000) * np.exp(-np.abs(t - 1.55) / 0.12) * 0.25
    chord = sum(bell(f, d, 0.8, 60 + i) * np.clip((t - 1.55) * 40, 0, 1)
                for i, f in enumerate((523.3, 659.3, 784.0)))
    return hum * hum_env * 0.5 + clicks + surge + chord * 0.18


def ambient_loop(dur=24.0):
    n = int(dur * SR)
    t = np.arange(n) / SR

    def per_noise(lo, hi, seed, tilt=0.0):
        X = np.fft.rfft(rng(seed).standard_normal(n))
        f = np.fft.rfftfreq(n, 1 / SR)
        w = np.ones_like(f)
        lf = np.log2(np.maximum(f, 1.0))
        w *= 1 / (1 + np.exp(-(lf - np.log2(lo)) / 0.3))
        w *= 1 / (1 + np.exp((lf - np.log2(hi)) / 0.3))
        w *= np.maximum(f, 20.0) ** (-tilt)
        y = np.fft.irfft(X * w, n)
        return y / np.max(np.abs(y))

    def lfo(cycles, phase=0.0):
        return np.sin(2 * np.pi * cycles * t / dur + phase)

    def snap(f):  # frequency with an integer number of cycles over the loop
        return round(f * dur) / dur

    wind = per_noise(150, 2500, 70, tilt=0.5)
    gust = 0.55 + 0.25 * lfo(1, 0.3) + 0.15 * lfo(2, 1.7) + 0.08 * lfo(5, 0.9)
    wind_hi = per_noise(1500, 6000, 71) * (0.3 + 0.2 * lfo(3, 2.0))
    lap = per_noise(60, 400, 72, tilt=0.3) * (0.6 + 0.4 * np.maximum(lfo(4, 0.5), 0) ** 2)
    hum = sum(a * np.sin(2 * np.pi * snap(f) * t + p) for f, a, p in
              ((50.0, 1.0, 0.0), (50.375, 0.6, 1.0), (100.0, 0.5, 0.4), (150.0, 0.25, 2.1), (200.0, 0.12, 0.7)))
    hum *= 0.8 + 0.2 * lfo(2, 0.0)
    # distant harbour horn swell, centred in the loop (envelope ~0 at both ends)
    horn_env = np.exp(-0.5 * ((t - 13.0) / 1.6) ** 2)
    horn = (np.sin(2 * np.pi * snap(98.0) * t) + 0.5 * np.sin(2 * np.pi * snap(196.0) * t) +
            0.25 * np.sin(2 * np.pi * snap(294.0) * t)) * horn_env
    # far-off industrial clanks
    clank = np.zeros(n)
    for k, at in enumerate((4.2, 9.7, 18.1)):
        seg = modal([523, 1377, 2390], [0.5, 0.3, 0.2], [1, 0.5, 0.3], dur - at, 80 + k)
        s0 = int(at * SR)
        L = min(len(seg), n - s0)
        clank[s0:s0 + L] += seg[:L]
    clank = bandpass_fft(clank, 300, 3000)
    out = wind * gust * 0.55 + wind_hi * 0.1 + lap * 0.25 + hum * 0.05 + horn * 0.07 + clank * 0.04
    return out


SOUNDS = [
    ("Bounce", bounce), ("Swish", swish), ("Rim", rim), ("Board", board), ("ShotRelease", shot_release),
    ("Pass", pass_), ("Catch", catch), ("Tackle", tackle), ("Block", block), ("Steal", steal),
    ("ReadWin", read_win), ("ReadLose", read_lose), ("Whistle", whistle), ("Buzzer", buzzer), ("Score", score),
    ("Drive", drive), ("Dodge", dodge), ("ContestHum", contest_hum), ("TackleCue", tackle_cue), ("Step", step),
    ("Down", down), ("Revive", revive), ("ArtCue", art_cue), ("UIMove", ui_move), ("UIConfirm", ui_confirm),
    ("CityPowerUp", city_power_up), ("AmbientLoop", ambient_loop),
    # footfalls (three takes so steps do not repeat) and sneaker squeaks for hard cuts and stops
    ("Foot1", lambda: foot(51, 105)), ("Foot2", lambda: foot(53, 95)), ("Foot3", lambda: foot(55, 118)),
    ("Squeak1", lambda: squeak(41, 1500, 2300, 0.2)), ("Squeak2", lambda: squeak(43, 1900, 1550, 0.16)),
]


def main():
    out = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "wav"))
    if "--out" in sys.argv:
        out = sys.argv[sys.argv.index("--out") + 1]
    os.makedirs(out, exist_ok=True)
    info = {}
    for name, fn in SOUNDS:
        x = np.asarray(fn(), dtype=float)
        x = x - np.mean(x)
        if name == "AmbientLoop":
            x = normalize(x)          # periodic: no fades, the loop point is seamless by construction
        else:
            x = normalize(fade(x))
        write_wav(os.path.join(out, name + ".wav"), x)
        jump = abs(x[-1] - x[0]) if name == "AmbientLoop" else None
        info[name] = {"duration_s": round(len(x) / SR, 3), "peak_dbfs": round(20 * np.log10(np.max(np.abs(x))), 2),
                      "looping": name == "AmbientLoop"}
        if jump is not None:
            info[name]["loop_seam_jump"] = round(float(jump), 5)
        print("%-12s %6.3f s  peak %.2f dBFS" % (name, len(x) / SR, info[name]["peak_dbfs"]))
    with open(os.path.join(out, "sfx.json"), "w", encoding="utf-8") as f:
        json.dump(info, f, indent=1)


if __name__ == "__main__":
    main()
