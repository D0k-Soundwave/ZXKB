"""Run the real ZX VU audio loop (app/legacy/zx_vu.py, unchanged) on a test recording and print the
levels it would send to the keyboard, one block per line. Hardware modules are replaced with fakes."""
import sys, types, os
import numpy as np

path, style, legacy = sys.argv[1], sys.argv[2], sys.argv[3]
BLOCK = 1024
audio = np.fromfile(path, dtype=np.float32).reshape(-1, 2)

for name in ("hid", "pystray", "PIL", "PIL.Image", "PIL.ImageDraw"):
    sys.modules[name] = types.ModuleType(name)
sys.modules["PIL"].Image = sys.modules["PIL.Image"]; sys.modules["PIL"].ImageDraw = sys.modules["PIL.ImageDraw"]

state = {"pos": 0, "clock": 0.0}
class FakeStream:
    def read(self, n, exception_on_overflow=False):
        if state["pos"] + n > len(audio):
            vu.running = False
            raise EOFError
        blk = audio[state["pos"]:state["pos"] + n]; state["pos"] += n
        state["clock"] += 1.0          # one second per block, so the 40 FPS limit never drops a block
        return blk.tobytes()
    def close(self): pass
class FakePyAudio:
    def get_host_api_info_by_type(self, t): return {"defaultOutputDevice": 0}
    def get_device_info_by_index(self, i):
        return {"isLoopbackDevice": True, "defaultSampleRate": 48000.0, "maxInputChannels": 2, "index": 0, "name": "test"}
    def open(self, **kw): return FakeStream()
    def terminate(self): pass
pa = types.ModuleType("pyaudiowpatch"); pa.PyAudio = FakePyAudio; pa.paWASAPI = 13; pa.paFloat32 = 1
sys.modules["pyaudiowpatch"] = pa

sys.path.insert(0, os.path.dirname(legacy))
import zx_vu
zx_vu.time = types.SimpleNamespace(time=lambda: state["clock"], sleep=lambda s: None)
vu = zx_vu.VU()
vu.style, vu.enabled = style, True
out = []
vu.push = lambda payload: out.append(payload[1:17])
vu.audio_loop()
for row in out: print(" ".join(str(v) for v in row))
