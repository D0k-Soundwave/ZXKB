"""Make a 30 s, 48 kHz stereo float32 test recording for the VU comparison (fixed seed, repeatable).
Sections: silence, log sweep, noise bursts, AY-style ACB stereo, chords with a beat, loud noise, fade."""
import sys
import numpy as np

RATE = 48000
rng = np.random.default_rng(1982)
def t(sec): return np.arange(int(sec * RATE)) / RATE

parts = []
parts.append(np.zeros((3 * RATE, 2)))
tt = t(5); f = 30 * (18000 / 30) ** (tt / 5); ph = 2 * np.pi * np.cumsum(f) / RATE
s = 0.5 * np.sin(ph) * (0.3 + 0.7 * np.abs(np.sin(2 * np.pi * 0.4 * tt)))
parts.append(np.stack([s, s], 1))
tt = t(4); n = rng.standard_normal((len(tt), 2)) * 0.2; env = ((tt * 4).astype(int) % 2 == 0)
parts.append(n * env[:, None])
tt = t(6)
A = 0.3 * np.sign(np.sin(2 * np.pi * 1000 * tt)) * (np.sin(2 * np.pi * 0.7 * tt) > 0)
B = 0.3 * np.sign(np.sin(2 * np.pi * 440 * tt)) * (np.sin(2 * np.pi * 1.1 * tt + 1) > 0)
C = 0.3 * np.sign(np.sin(2 * np.pi * 110 * tt)) * (0.5 + 0.5 * np.sin(2 * np.pi * 0.5 * tt))
parts.append(np.stack([A + 0.7 * C, B + 0.7 * C], 1))
tt = t(6); chord = sum(np.sin(2 * np.pi * fr * tt) / (i + 1) for i, fr in enumerate([110, 220, 277, 330, 440, 880, 1760, 3520]))
beat = np.exp(-((tt % 0.5) * 8)); m = 0.25 * chord * (0.3 + beat)
parts.append(np.stack([m, m], 1))
parts.append(np.clip(rng.standard_normal((3 * RATE, 2)), -1, 1))
tt = t(3); n = rng.standard_normal((len(tt), 2)) * 0.3 * np.exp(-tt * 3)[:, None]
parts.append(n)
audio = np.concatenate(parts).astype(np.float32)
audio.tofile(sys.argv[1])
print(f"{len(audio)} frames, {len(audio) / RATE:.1f} s")
