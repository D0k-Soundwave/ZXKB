"""ZX VU - streams Windows audio levels to D0k's Keychron Q3 (custom QMK firmware).
Runs in the system tray. Right-click the tray icon: VU meter on/off, VU style,
Spectrum lighting effects, Quit.
"""
import threading, time, sys, json, os
import numpy as np
import hid
import pyaudiowpatch as pyaudio
import pystray
from PIL import Image, ImageDraw

VID, PID = 0x3434, 0x0123
CMD_LEVELS, CMD_MODE, CMD_EFFECT = 0xD0, 0xD1, 0xD2
EFFECTS = [(1, 'Spectrum stripes'), (2, 'Tape loading'), (3, 'Border flash'),
           (4, 'Attribute clash'), (5, 'Typing ripple'), (6, 'Logo sweep')]
BANDS = 16
FPS = 40
SETTINGS = os.path.join(os.environ.get('APPDATA', '.'), 'ZX_VU.json')
RANGE_DB = 30     # dynamic range shown across the keyboard; lower = longer bars
HEADROOM_DB = 9   # bars reach the far right only when this much louder than the recent average
CURVE = 1.7       # >1 = more up-and-down movement
FALL = 0.72       # how fast bars drop each frame (lower = faster)

def find_keyboard():
    for d in hid.enumerate(VID, PID):
        if d.get("usage_page") == 0xFF60 and d.get("usage") == 0x61:
            k = hid.device(); k.open_path(d["path"]); k.set_nonblocking(True)
            return k
    return None

def send(kb, payload):
    buf = bytes([0x00] + payload + [0] * (32 - len(payload)))   # report id 0 + 32 bytes
    if kb.write(buf) < 0:
        raise IOError("keyboard write failed")   # keyboard unplugged / reflashed: reconnect

class VU:
    def __init__(self):
        self.running = True
        self.lock = threading.Lock()
        self.kb = None
        self.levels = np.zeros(BANDS)
        self.peak = -40.0
        self.abc_peak = -40.0
        self.abc = np.zeros(3)
        try: cfg = json.load(open(SETTINGS))
        except Exception: cfg = {}
        self.style = cfg.get('style', 'bands')
        self.enabled = cfg.get('vu', True)
        self.effect = cfg.get('effect', 0)

    def save(self):
        try: json.dump({'style': self.style, 'vu': self.enabled, 'effect': self.effect}, open(SETTINGS, 'w'))
        except Exception: pass

    def keyboard(self):
        if self.kb is None:
            self.kb = find_keyboard()
            if self.kb is not None and self.enabled:
                try: send(self.kb, [CMD_MODE, 1])
                except Exception: self.kb = None
        return self.kb

    def push(self, payload):
        with self.lock:
            kb = self.keyboard()
            if kb is None: return
            try: send(kb, payload)
            except Exception:
                try: kb.close()
                except Exception: pass
                self.kb = None

    def set_enabled(self, on):
        self.enabled = on
        self.save()
        self.push([CMD_MODE, 1 if on else 0])

    def set_effect(self, n):
        self.effect = n
        self.enabled = False          # an effect replaces the VU meter (tick VU again to bring it back)
        self.save()
        self.push([CMD_EFFECT, n])

    def audio_loop(self):
        p = pyaudio.PyAudio()
        while self.running:
            try:
                wasapi = p.get_host_api_info_by_type(pyaudio.paWASAPI)
                spk = p.get_device_info_by_index(wasapi["defaultOutputDevice"])
                if not spk.get("isLoopbackDevice"):
                    for lb in p.get_loopback_device_info_generator():
                        if spk["name"] in lb["name"]:
                            spk = lb; break
                rate = int(spk["defaultSampleRate"]); ch = spk["maxInputChannels"]
                block = 1024
                stream = p.open(format=pyaudio.paFloat32, channels=ch, rate=rate, input=True,
                                frames_per_buffer=block, input_device_index=spk["index"])
                freqs = np.fft.rfftfreq(block, 1.0 / rate)
                edges = np.geomspace(40, 16000, BANDS + 1)
                idx = [np.where((freqs >= edges[i]) & (freqs < edges[i + 1]))[0] for i in range(BANDS)]
                win = np.hanning(block)
                last = 0
                while self.running:
                    data = np.frombuffer(stream.read(block, exception_on_overflow=False), dtype=np.float32)
                    frames = data.reshape(-1, ch) if ch > 1 else np.stack([data, data], axis=1)
                    mono = frames[:, :2].mean(axis=1)
                    if self.style == 'abc':
                        # ACB stereo: A left, C centre, B right. L = A + 0.7C, R = B + 0.7C  ->  C from what L and R share
                        L, R = frames[:, 0], frames[:, 1]
                        lr = max(float(np.mean(L * R)), 0.0)
                        pa = max(float(np.mean(L * L)) - lr, 0.0)
                        pc = max(float(np.mean(R * R)) - lr, 0.0)
                        pb = lr * 2.0
                        total = pa + pb + pc + 1e-12
                        if (pa + pc) / total < 0.15:
                            # little stereo separation: split by pitch so all three bars still move
                            sp = np.abs(np.fft.rfft(mono * win)) ** 2
                            f = freqs
                            pb = float(sp[(f >= 30) & (f < 250)].sum())      # centre slot -> C = bass
                            pc = float(sp[(f >= 250) & (f < 2000)].sum())    # right slot  -> B = mids
                            pa = float(sp[(f >= 2000) & (f < 16000)].sum()) * 4  # A = treble (boosted, it is quieter)
                            pa, pb, pc = (p / (block * block) for p in (pa, pb, pc))
                        dbs = 10 * np.log10(np.array([pa, pb, pc]) + 1e-12)
                        self.abc_peak = max(self.abc_peak + 0.03 * (dbs.mean() - self.abc_peak), -70.0)   # ~1 s average level
                        t = np.clip((dbs - (self.abc_peak + HEADROOM_DB - RANGE_DB)) / RANGE_DB, 0, 1) ** CURVE
                        self.abc = np.where(t > self.abc, t, self.abc * FALL)
                        now = time.time()
                        if self.enabled and now - last >= 1.0 / FPS:
                            a, c, b = (int(v * 255) for v in self.abc)   # [left, centre, right] = [A, C, B]
                            # rows: top two = A (bands 11-15), middle two = B (5-10), bottom two = C (0-4)
                            self.push([CMD_LEVELS] + [c] * 5 + [b] * 6 + [a] * 5)
                            last = now
                        continue
                    spec = np.abs(np.fft.rfft(mono * win)) / (block / 4)
                    raw = np.array([spec[i].max() if len(i) else 0 for i in idx])
                    db = 20 * np.log10(raw + 1e-9)
                    # auto-gain: follow the loudest band so bars use the full width at any volume
                    self.peak = max(self.peak + 0.03 * (db.max() - self.peak), -70.0)   # ~1 s average of the loudest band
                    target = np.clip((db - (self.peak + HEADROOM_DB - RANGE_DB)) / RANGE_DB, 0, 1) ** CURVE
                    self.levels = np.where(target > self.levels, target, self.levels * FALL)
                    now = time.time()
                    if self.enabled and now - last >= 1.0 / FPS:
                        self.push([CMD_LEVELS] + [int(v * 255) for v in self.levels])
                        last = now
                stream.close()
            except Exception:
                time.sleep(2)   # audio device changed or keyboard busy: retry
        p.terminate()

def icon_image(on=True):
    img = Image.new("RGBA", (64, 64), (0, 0, 0, 0)); d = ImageDraw.Draw(img)
    cols = [(255, 0, 0), (255, 200, 0), (0, 220, 0), (0, 200, 255)]
    for i, c in enumerate(cols):
        x = 14 + i * 10
        d.polygon([(x, 60), (x + 10, 60), (x + 30, 4), (x + 20, 4)], fill=c if on else (90, 90, 90))
    return img

def main():
    vu = VU()
    threading.Thread(target=vu.audio_loop, daemon=True).start()

    def toggle(icon, item):
        vu.set_enabled(not vu.enabled); icon.icon = icon_image(vu.enabled)
    def quit_(icon, item):
        vu.running = False; vu.push([CMD_MODE, 0]); icon.stop()

    def set_style(style):
        def f(icon, item):
            vu.style = style
            vu.save()
        return f

    def set_fx(n):
        def f(icon, item):
            vu.set_effect(n); icon.icon = icon_image(False)
        return f
    fx_menu = pystray.Menu(*[pystray.MenuItem(name, set_fx(n), checked=(lambda n: lambda i: not vu.enabled and vu.effect == n)(n), radio=True)
                             for n, name in EFFECTS])

    menu = pystray.Menu(pystray.MenuItem("VU meter on keyboard", toggle, checked=lambda i: vu.enabled, default=True),
                        pystray.Menu.SEPARATOR,
                        pystray.MenuItem("Frequency bands (bass bottom, treble top)", set_style('bands'),
                                         checked=lambda i: vu.style == 'bands', radio=True),
                        pystray.MenuItem("AY channels (ACB): A top, B middle, C bottom", set_style('abc'),
                                         checked=lambda i: vu.style == 'abc', radio=True),
                        pystray.Menu.SEPARATOR,
                        pystray.MenuItem("Spectrum lighting effect", fx_menu),
                        pystray.Menu.SEPARATOR,
                        pystray.MenuItem("Quit", quit_))
    pystray.Icon("ZX VU", icon_image(vu.enabled), "ZX VU - keyboard audio meter", menu).run()

if __name__ == "__main__":
    main()
