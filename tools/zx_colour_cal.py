"""ZX colour calibration for D0k's Keychron Q3 (custom QMK firmware v10+).
Every colour has its own Red / Green / Blue sliders plus Brightness. Match the keyboard to the on-screen panel.
Hardware balance sets the LED driver current per channel (affects everything, saved on the keyboard).
Left/Right arrows nudge the highlighted slider by 1 (Shift = 10). Up/Down picks the slider.
Reset buttons put one colour (or everything, including the LED hardware) back to factory/Spectrum values.
Saves to ZX_colours.json next to this script, then puts the lighting back.
"""
import json, os, subprocess, time, tkinter as tk
import hid

VID, PID = 0x3434, 0x0123
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "ZX_colours.json")
VU_EXE = os.path.join(HERE, "ZX_VU.exe")
SET, GET = 0x07, 0x08
CH = 3                       # VIA RGB matrix channel
BRIGHT, EFFECT, COLOUR = 1, 2, 4
SOLID = 1                    # QMK solid colour effect
HW_SET, HW_SAVE, HW_GET = 0xD3, 0xD4, 0xD5

def hsv_to_rgb(h, s, v):     # same maths as QMK's hsv_to_rgb
    if s == 0: return v, v, v
    region = h * 6 // 255
    rem = (h * 2 - region * 85) * 3
    p = (v * (255 - s)) >> 8
    q = (v * (255 - ((s * rem) >> 8))) >> 8
    t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8
    return [(v, t, p), (q, v, p), (p, v, t), (p, q, v), (t, p, v), (v, p, q), (v, t, p)][region]

def rgb_to_hsv(r, g, b):
    mx, mn = max(r, g, b), min(r, g, b)
    if mx == 0: return 0, 0, 0
    d = mx - mn
    s = round(255 * d / mx)
    if d == 0: h = 0
    elif mx == r: h = (43 * (g - b) / d) % 256
    elif mx == g: h = 85 + 43 * (b - r) / d
    else: h = 171 + 43 * (r - g) / d
    return int(round(h)) % 256, s, mx

class KB:
    def __init__(self):
        self.d = None
        for i in hid.enumerate(VID, PID):
            if i.get("usage_page") == 0xFF60 and i.get("usage") == 0x61:
                self.d = hid.device(); self.d.open_path(i["path"]); break
        if self.d is None: raise SystemExit("Keyboard not found")
    def cmd(self, *b, reply=True):
        self.d.write(bytes([0] + list(b) + [0] * (32 - len(b))))
        return self.d.read(32, 500) if reply else []
    def get(self, vid): return self.cmd(GET, CH, vid)[3:5]
    def set(self, vid, *vals): self.cmd(SET, CH, vid, *vals)

kb = KB()
subprocess.run(["taskkill", "/IM", "ZX_VU.exe", "/F"], capture_output=True)   # stop VU taking over the lights
kb.cmd(0xD1, 0, reply=False); time.sleep(0.3)                                  # and drop out of VU mode
hw0 = list(kb.cmd(HW_GET)[1:4])                                                # LED current limits: red, green, blue
orig = {"eff": kb.get(EFFECT)[0], "bri": kb.get(BRIGHT)[0], "col": kb.get(COLOUR)}

# The real ZX Spectrum BRIGHT palette: on-screen target, and the starting point for each colour
TARGET = {"Red": (255, 0, 0), "Yellow": (255, 255, 0), "Green": (0, 255, 0), "Cyan": (0, 255, 255),
          "Blue": (0, 0, 255), "Magenta": (255, 0, 255), "White": (255, 255, 255)}
HW = "Hardware balance"
names = [HW] + list(TARGET)
cal = {k: [*v, 255] for k, v in TARGET.items()}          # [red, green, blue, brightness]
if os.path.exists(OUT):
    try:
        for k, v in json.load(open(OUT)).items():
            if k in cal:
                if "mix" in v: cal[k] = list(v["mix"]) + [v.get("bright", 255)]
                else: cal[k] = list(v.get("rgb_set", v["rgb"])) + [255]
    except Exception: pass

kb.set(EFFECT, SOLID)
root = tk.Tk(); root.title("ZX colour calibration"); root.resizable(False, False)
cur = tk.StringVar(value="Red")
rv, gv, bv, brv = (tk.IntVar() for _ in range(4))
hr, hg, hb = (tk.IntVar(value=v) for v in hw0)
info = tk.StringVar()
loading = False

def out_rgb(c):
    r, g, b, br = cal[c]
    return tuple(x * br // 255 for x in (r, g, b))

def push(*_):
    if loading: return
    c = cur.get()
    if c == HW:
        got = kb.cmd(HW_SET, hr.get(), hg.get(), hb.get())[1:4]
        kb.set(COLOUR, 0, 0); kb.set(BRIGHT, 255)                  # plain full white
        info.set(f"LED current limits: red {got[0]}  green {got[1]}  blue {got[2]}   (Keychron default 68 / 157 / 157)")
        return
    cal[c] = [rv.get(), gv.get(), bv.get(), brv.get()]
    h, s, v = rgb_to_hsv(*out_rgb(c))
    kb.set(COLOUR, h, s); kb.set(BRIGHT, max(v, 1))
    r, g, b = hsv_to_rgb(h, s, v)
    info.set(f"Sending to keyboard: R {r}  G {g}  B {b}")

def show(*_):
    global loading
    c = cur.get(); loading = True
    for w in mix_rows + hw_rows: w.grid_remove()
    if c == HW:
        for w in hw_rows: w.grid()
        select(sliders_hw[0])
        target_lbl.config(text="Hardware: balance the LED current until the keyboard is a clean white")
        swatch.config(bg="#FFFFFF")
    else:
        rv.set(cal[c][0]); gv.set(cal[c][1]); bv.set(cal[c][2]); brv.set(cal[c][3])
        for w in mix_rows: w.grid()
        select(sliders_mix[0])
        target_lbl.config(text=f"Spectrum BRIGHT {c.lower()}: make the keyboard look like this")
        swatch.config(bg="#%02X%02X%02X" % TARGET[c])
    loading = False
    push()

def finish(save):
    if save:
        kb.cmd(HW_SAVE)                                           # store LED limits on the keyboard
        data = {"Hardware": {"red": hr.get(), "green": hg.get(), "blue": hb.get()}}
        for k in TARGET:
            r, g, b, br = cal[k]
            data[k] = {"mix": [r, g, b], "bright": br, "rgb": list(out_rgb(k))}
        json.dump(data, open(OUT, "w"), indent=1)
    else:
        kb.cmd(HW_SET, *hw0)                                      # cancel: put the old limits back
    kb.set(COLOUR, *orig["col"]); kb.set(BRIGHT, orig["bri"]); kb.set(EFFECT, orig["eff"])
    if os.path.exists(VU_EXE): subprocess.Popen([VU_EXE])
    root.destroy()

HW_DEFAULT = (68, 157, 157)                                       # Keychron factory LED current (red, green, blue)
def reset_this():
    c = cur.get()
    if c == HW: hr.set(HW_DEFAULT[0]); hg.set(HW_DEFAULT[1]); hb.set(HW_DEFAULT[2])
    else: cal[c] = [*TARGET[c], 255]
    show()
def reset_all():
    for k, v in TARGET.items(): cal[k] = [*v, 255]
    hr.set(HW_DEFAULT[0]); hg.set(HW_DEFAULT[1]); hb.set(HW_DEFAULT[2])
    kb.cmd(HW_SET, *HW_DEFAULT)
    show()

# ---------- arrow-key fine tuning ----------
active = [None]
def select(sc):
    for s in sliders_mix + sliders_hw: s.config(troughcolor="#d0d0d0")
    sc.config(troughcolor="#ffd24a"); active[0] = sc
def visible_sliders(): return sliders_hw if cur.get() == HW else sliders_mix
def nudge(d):
    sc = active[0]
    if sc is None: return "break"
    lo, hi = int(sc.cget("from")), int(sc.cget("to"))
    sc.var.set(max(lo, min(hi, sc.var.get() + d))); push()
    return "break"
def move_sel(d):
    vs = visible_sliders()
    i = vs.index(active[0]) if active[0] in vs else 0
    select(vs[(i + d) % len(vs)]); return "break"
for k, d in (("<Left>", -1), ("<Right>", 1), ("<Shift-Left>", -10), ("<Shift-Right>", 10)):
    root.bind_all(k, lambda e, d=d: nudge(d))
root.bind_all("<Up>", lambda e: move_sel(-1)); root.bind_all("<Down>", lambda e: move_sel(1))

def slider(row, text, var, lo, hi, colour):
    lbl = tk.Label(f, text=text, fg=colour, font=("Segoe UI", 10, "bold"))
    lbl.grid(row=row, column=0, columnspan=4, sticky="w", pady=(6, 0))
    sc = tk.Scale(f, from_=lo, to=hi, orient="horizontal", length=560, variable=var, command=push,
                  takefocus=0, troughcolor="#d0d0d0")
    sc.grid(row=row + 1, column=0, columnspan=4); sc.var = var
    sc.bind("<Button-1>", lambda e: select(sc), add="+")
    for k, d in (("<Left>", -1), ("<Right>", 1), ("<Shift-Left>", -10), ("<Shift-Right>", 10)):
        sc.bind(k, lambda e, d=d: nudge(d))
    sc.bind("<Up>", lambda e: move_sel(-1)); sc.bind("<Down>", lambda e: move_sel(1))
    return lbl, sc

f = tk.Frame(root, padx=14, pady=10); f.pack()
tk.Label(f, text="Pick a colour, then adjust until the keyboard matches the colour panel.\n"
                 "Left/Right arrows nudge the highlighted slider by 1 (Shift = 10). Up/Down picks the slider.",
         font=("Segoe UI", 10)).grid(row=0, column=0, columnspan=4, pady=(0, 8))
bf = tk.Frame(f); bf.grid(row=1, column=0, columnspan=4)
for i, n in enumerate(names):
    tk.Radiobutton(bf, text=n, variable=cur, value=n, command=show, indicatoron=0, width=15 if n == HW else 8,
                   takefocus=0, font=("Segoe UI", 11)).grid(row=0, column=i, padx=2)
target_lbl = tk.Label(f, font=("Segoe UI", 11, "bold")); target_lbl.grid(row=2, column=0, columnspan=4, pady=(8, 2))
swatch = tk.Label(f, width=70, height=6, relief="sunken", bd=2); swatch.grid(row=3, column=0, columnspan=4, pady=(0, 4))

l1, s1 = slider(4, "Red", rv, 0, 255, "#c00000")
l2, s2 = slider(6, "Green", gv, 0, 255, "#008000")
l3, s3 = slider(8, "Blue", bv, 0, 255, "#0000c0")
l4, s4 = slider(10, "Brightness", brv, 10, 255, "#000000")
l5, s5 = slider(12, "LED hardware: red current", hr, 16, 192, "#c00000")
l6, s6 = slider(14, "LED hardware: green current", hg, 16, 192, "#008000")
l7, s7 = slider(16, "LED hardware: blue current", hb, 16, 192, "#0000c0")
sliders_mix, sliders_hw = [s1, s2, s3, s4], [s5, s6, s7]
mix_rows, hw_rows = [l1, s1, l2, s2, l3, s3, l4, s4], [l5, s5, l6, s6, l7, s7]

tk.Label(f, textvariable=info, font=("Consolas", 9), fg="#666").grid(row=20, column=0, columnspan=4, pady=(6, 0))
tk.Button(f, text="Reset this to default", width=20, takefocus=0, command=reset_this).grid(row=21, column=0, columnspan=2, pady=(6, 0))
tk.Button(f, text="Reset everything to default", width=24, takefocus=0, command=reset_all).grid(row=21, column=2, columnspan=2, pady=(6, 0))
tk.Button(f, text="Save and finish", width=16, takefocus=0, command=lambda: finish(True)).grid(row=22, column=0, columnspan=2, pady=6)
tk.Button(f, text="Cancel", width=16, takefocus=0, command=lambda: finish(False)).grid(row=22, column=2, columnspan=2, pady=6)
root.protocol("WM_DELETE_WINDOW", lambda: finish(False))
show()
root.mainloop()
