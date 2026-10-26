#!/usr/bin/env python3
"""Ur sound effects and music, synthesised (no samples, no licences).

Struck sounds are a short noise burst feeding a few decaying resonances
(the Gammon-X recipe). The lyre is Karplus-Strong. Writes raw 16-bit mono
PCM at 22050 Hz to romfs/sfx/*.pcm (what the 3DS plays) and WAV previews
to design/sfx/.
"""
import os
import wave
import numpy as np

RATE = 22050
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "romfs", "sfx")
PREVIEW = os.path.join(ROOT, "design", "sfx")


def t_axis(dur):
    return np.arange(int(RATE * dur)) / RATE


def strike(modes, dur, noise_ms=2.0, noise_gain=0.6, rng=None):
    rng = rng or np.random.default_rng(0)
    t = t_axis(dur)
    out = np.zeros_like(t)
    for f, d, g in modes:
        out += g * np.sin(2 * np.pi * f * t + rng.uniform(0, 2 * np.pi)) * np.exp(-t / d)
    n = int(RATE * noise_ms / 1000)
    out[:n] += noise_gain * rng.normal(0, 1, n) * np.linspace(1, 0, n) ** 2
    a = int(RATE * 0.0006)
    out[:a] *= np.linspace(0, 1, a)
    return out


def lowpass(x, cutoff):
    k = np.exp(-2 * np.pi * cutoff / RATE)
    y = np.zeros_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1 - k) * v + k * acc
        y[i] = acc
    return y


def normalise(x, peak):
    m = np.max(np.abs(x))
    return x * (peak / m) if m > 0 else x


def mix_at(total, x, t0, gain=1.0):
    s = max(0, int(RATE * t0))
    e = min(len(total), s + len(x))
    if e > s:
        total[s:e] += gain * x[:e - s]


def pluck(freq, dur, bright=0.5, seed=0):
    """Karplus-Strong string: a lyre/harp note."""
    rng = np.random.default_rng(seed)
    n = int(RATE / freq)
    buf = rng.uniform(-1, 1, n)
    buf = np.convolve(buf, [bright, 1 - bright], mode="same")
    out = np.zeros(int(RATE * dur))
    decay = 0.996
    idx = 0
    for i in range(len(out)):
        cur = buf[idx]
        nxt = buf[(idx + 1) % n]
        buf[idx] = decay * 0.5 * (cur + nxt)
        out[i] = cur
        idx = (idx + 1) % n
    a = int(RATE * 0.002)
    out[:a] *= np.linspace(0, 1, a)
    return out


def clack(seed):
    """Shell counter landing on the inlaid board."""
    rng = np.random.default_rng(seed)
    j = rng.uniform(0.94, 1.06)
    x = strike([(1450 * j, 0.020, 1.0), (2900 * j, 0.010, 0.5), (4300 * j, 0.005, 0.3),
                (760 * j, 0.026, 0.35)], 0.09, noise_ms=1.4, noise_gain=0.9, rng=rng)
    return normalise(lowpass(x, 5600), 0.55)


def tap(seed):
    """A light touch for each square a piece hops over."""
    rng = np.random.default_rng(seed)
    j = rng.uniform(0.96, 1.04)
    x = strike([(1700 * j, 0.010, 1.0), (3300 * j, 0.005, 0.4)], 0.04, noise_ms=0.8, noise_gain=0.5, rng=rng)
    return normalise(lowpass(x, 5000), 0.22)


def rattle(seed):
    """Four bone tetrahedra dropped on wood."""
    rng = np.random.default_rng(seed)
    total = np.zeros(int(RATE * 0.55))
    for d in range(4):
        t0, gap, amp = rng.uniform(0, 0.03), 0.06, 1.0
        for k in range(4):
            j = rng.uniform(0.9, 1.12)
            hit = strike([(2100 * j, 0.012, 1.0), (3700 * j, 0.007, 0.55), (5200 * j, 0.004, 0.3),
                          (1050 * j, 0.015, 0.35)], 0.05, noise_ms=1.0, noise_gain=0.7, rng=rng)
            mix_at(total, hit, t0, amp)
            t0 += gap * rng.uniform(0.7, 1.2)
            gap *= 0.7
            amp *= rng.uniform(0.5, 0.7)
    return normalise(lowpass(total, 6500), 0.5)


def thunk():
    """Capture: a heavier knock as the piece is struck off."""
    x = strike([(430, 0.06, 1.0), (1080, 0.03, 0.5), (1900, 0.014, 0.25), (215, 0.07, 0.45)],
               0.18, noise_ms=3.0, noise_gain=0.9, rng=np.random.default_rng(9))
    return normalise(lowpass(x, 3600), 0.65)


def tick():
    x = strike([(1900, 0.006, 1.0), (3400, 0.003, 0.4)], 0.03, noise_ms=0.6, noise_gain=0.4,
               rng=np.random.default_rng(3))
    return normalise(lowpass(x, 6000), 0.28)


# D Phrygian-ish, an old-world colour without pretending to be authentic.
D = 146.83
SCALE = [1, 16 / 15, 6 / 5, 4 / 3, 3 / 2, 8 / 5, 9 / 5, 2]


def note(deg, octave=1):
    return D * octave * SCALE[deg % 7] * (2 ** (deg // 7))


def rosette():
    """Landing on a rosette: a bright lyre dyad."""
    total = np.zeros(int(RATE * 1.2))
    mix_at(total, pluck(note(4, 2), 1.1, 0.4, 1), 0.0)
    mix_at(total, pluck(note(7, 2), 1.0, 0.4, 2), 0.07, 0.8)
    return normalise(total, 0.38)


def win():
    total = np.zeros(int(RATE * 2.0))
    for k, deg in enumerate((0, 2, 4, 7, 9, 11)):
        mix_at(total, pluck(note(deg, 2), 1.6, 0.45, 10 + k), 0.11 * k, 0.9)
    return normalise(total, 0.42)


def lose():
    total = np.zeros(int(RATE * 1.8))
    for k, deg in enumerate((4, 3, 1, 0)):
        mix_at(total, pluck(note(deg, 1), 1.5, 0.6, 20 + k), 0.18 * k, 0.9)
    return normalise(total, 0.38)


def music():
    """A slow lyre loop over a drone and a soft frame drum. 48 s, seamless."""
    bars, beat = 16, 0.75          # 4 beats per bar, 80 bpm
    length = bars * 4 * beat
    total = np.zeros(int(RATE * (length + 3)))
    rng = np.random.default_rng(42)
    # drone: root + fifth, slow swell
    t = t_axis(length + 3)
    swell = 0.6 + 0.4 * np.sin(2 * np.pi * t / length)
    drone = (np.sin(2 * np.pi * D / 2 * t) + 0.5 * np.sin(2 * np.pi * D * 3 / 4 * t)
             + 0.2 * np.sin(2 * np.pi * D * t)) * swell
    total += 0.10 * lowpass(drone, 600)
    # lyre phrases: a figure per bar, varied by bar
    figures = [(0, 2, 4, 2), (0, 3, 4, 7), (6, 4, 3, 1), (0, 2, 1, 0)]
    for bar in range(bars):
        fig = figures[bar % 4]
        for k, deg in enumerate(fig):
            if rng.uniform() < 0.15 and k > 0:
                continue
            t0 = (bar * 4 + k) * beat + rng.uniform(-0.01, 0.01)
            mix_at(total, pluck(note(deg + 7, 1), 2.4, 0.5, bar * 7 + k), t0, 0.45 * rng.uniform(0.8, 1.0))
        if bar % 2 == 1:
            mix_at(total, pluck(note(fig[0] + 14, 1), 2.0, 0.35, 900 + bar), (bar * 4 + 2.5) * beat, 0.2)
    # frame drum: low thump on 1 and 3, soft
    for b in range(bars * 4):
        if b % 4 in (0, 2):
            hit = strike([(85, 0.12, 1.0), (170, 0.05, 0.3)], 0.3, noise_ms=4, noise_gain=0.3,
                         rng=np.random.default_rng(b))
            mix_at(total, lowpass(hit, 900), b * beat, 0.30 if b % 4 == 0 else 0.18)
    # fold the tail over the start so the loop is seamless
    n = int(RATE * length)
    out = total[:n].copy()
    out[:len(total) - n] += total[n:]
    return normalise(out, 0.30)


def write(name, x):
    pcm = np.clip(x * 32767, -32768, 32767).astype("<i2")
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(PREVIEW, exist_ok=True)
    pcm.tofile(os.path.join(OUT, name + ".pcm"))
    with wave.open(os.path.join(PREVIEW, name + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())


if __name__ == "__main__":
    for k in range(3):
        write("clack%d" % k, clack(100 + k))
    for k in range(2):
        write("tap%d" % k, tap(150 + k))
    for k in range(2):
        write("rattle%d" % k, rattle(200 + k))
    write("thunk", thunk())
    write("tick", tick())
    write("rosette", rosette())
    write("win", win())
    write("lose", lose())
    write("music", music())
    print("ok")
