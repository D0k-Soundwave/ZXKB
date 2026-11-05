"""Generate ZXKB keyboard reference docs for the Keychron Q3 ISO knob with KAM Wraith ZX Spectrum caps.

Outputs (in docs/keyboard/):
  keycaps.json / keycaps.md   which Wraith cap sits on each physical key (legends, cap colour, position)
  layout.json  / layout.md    physical layout, LED index, QMK keycode on all 4 layers, function per mode
  *.png                       keyboard pictures (plain caps, Windows, Fn, Spectrum)

Usage: python tools/gen_keyboard_docs.py firmware/keymaps/d0k/keymap.c docs/keyboard
(writes HTML pictures; turn them into PNGs with any headless browser, e.g. Playwright)
"""
import json, os, re, sys
from wraith_caps import K, render, GREY, BLACK, F58   # keycap data + SVG renderer (from the manual)

KEYMAP, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)

# Physical order of LAYOUT_tkl_f13_iso (89 entries: 88 keys + knob press)
ORDER = (["esc"] + [f"f{i}" for i in range(1, 13)] + ["knob", "prt", "scr", "pau"] +
         ["grv"] + [f"n{d}" for d in "1234567890"] + ["min", "eq", "bks", "ins", "hom", "pgu"] +
         ["tab"] + list("qwertyuiop") + ["lbr", "rbr", "del", "end", "pgd"] +
         ["cap"] + list("asdfghjkl") + ["sem", "quo", "hsh", "ent"] +
         ["lsh", "iso"] + list("zxcvbnm") + ["com", "dot", "sl", "rsh", "up"] +
         ["lct", "lwn", "lal", "spc", "ral", "rwn", "fn", "rct", "lft", "dn", "rt"])
assert len(ORDER) == 89

# ---------- parse the four layers out of keymap.c ----------
src = open(KEYMAP, encoding="utf-8").read()
blk = src[src.index("keymaps[]"):src.index("#if defined(ENCODER_MAP_ENABLE)")]
layers = {}
for m in re.finditer(r"\[(\w+)\]\s*=\s*LAYOUT_tkl_f13_iso\(", blk):
    i, depth, cur, toks = m.end(), 1, "", []
    while True:
        ch = blk[i]; i += 1
        if ch == "(": depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0: break
        if ch == "," and depth == 1: toks.append(cur.strip()); cur = ""; continue
        cur += ch
    toks.append(cur.strip())
    assert len(toks) == 89, (m.group(1), len(toks))
    layers[m.group(1)] = dict(zip(ORDER, toks))
enc = dict(re.findall(r"\[(\w+)\]\s*=\s*\{\s*ENCODER_CCW_CW\(([^)]*)\)", src))

# ---------- what each keycode does, in plain words ----------
NAMES = {
    "KC_ESC": "Esc", "KC_TAB": "Tab", "KC_CAPS": "Caps Lock", "KC_LSFT": "Left Shift", "KC_RSFT": "Right Shift",
    "KC_LCTL": "Left Ctrl", "KC_RCTL": "Right Ctrl", "KC_LALT": "Alt", "KC_RALT": "AltGr", "KC_LWIN": "Windows",
    "KC_RWIN": "Windows", "KC_SPC": "Space", "KC_ENT": "Enter", "KC_BSPC": "Backspace", "KC_INS": "Insert",
    "KC_HOME": "Home", "KC_PGUP": "Page Up", "KC_DEL": "Delete", "KC_END": "End", "KC_PGDN": "Page Down",
    "KC_UP": "Up", "KC_DOWN": "Down", "KC_LEFT": "Left", "KC_RGHT": "Right", "KC_PSCR": "Print Screen",
    "KC_SCRL": "Scroll Lock", "KC_PAUS": "Pause", "KC_MUTE": "Mute", "KC_VOLU": "Volume up", "KC_VOLD": "Volume down",
    "KC_MPRV": "Previous track", "KC_MPLY": "Play/Pause", "KC_MNXT": "Next track", "KC_BRID": "Screen brightness down",
    "KC_BRIU": "Screen brightness up", "KC_GRV": "` ~", "KC_MINS": "- _", "KC_EQL": "= +", "KC_LBRC": "[ {",
    "KC_RBRC": "] }", "KC_SCLN": "; :", "KC_QUOT": "' \"", "KC_NUHS": "\\ | (non-US #, with the US-based Windows layout)", "KC_NUBS": "\\ | (ISO key, with the US-based Windows layout)",
    "KC_COMM": ", <", "KC_DOT": ". >", "KC_SLSH": "/ ?", "KC_NO": "(nothing)",
    "RM_TOGG": "Lights on/off", "RM_NEXT": "Next effect", "RM_PREV": "Previous effect", "RM_VALU": "Lights brighter",
    "RM_VALD": "Lights dimmer", "RM_HUEU": "Hue +", "RM_HUED": "Hue -", "RM_SATU": "Saturation +", "RM_SATD": "Saturation -",
    "RM_SPDU": "Effect speed +", "RM_SPDD": "Effect speed -", "NK_TOGG": "NKRO on/off", "GU_TOGG": "Windows key lock",
    "VU_TOGG": "VU meter on/off", "ZK_VU": "VU meter on/off", "ZK_GAME": "Game mode on/off", "ZX_LOAD": 'Types LOAD "" + Enter (60 ms per key)',
    "ZX_EXT": "EXTENDED MODE (Ctrl + Left Shift)", "C(KC_LSFT)": "EXTENDED MODE (Ctrl + Left Shift)",
    "S(KC_1)": "Shift+1 (Spectrum EDIT)", "S(KC_2)": "Shift+2 (CAPS LOCK)", "S(KC_3)": "Shift+3 (TRUE VIDEO)",
    "S(KC_4)": "Shift+4 (INV VIDEO)", "S(KC_9)": "Shift+9 (GRAPHICS)", "S(KC_0)": "Shift+0 (DELETE)",
}
def describe(code):
    if code in NAMES: return NAMES[code]
    m = re.fullmatch(r"MO\((\w+)\)", code)
    if m: return f"Fn (hold for {m.group(1)})"
    m = re.fullmatch(r"KC_(F\d+)", code)
    if m: return m.group(1)
    m = re.fullmatch(r"KC_([A-Z0-9])", code)
    if m: return m.group(1)
    return code

# MiSTer ZX Spectrum core meaning of the keys it handles itself (Spectrum mode)
CORE = {"esc": "BREAK", "tab": "GRAPHICS", "cap": "CAPS LOCK", "bks": "DELETE", "lsh": "CAPS SHIFT",
        "lct": "SYMBOL SHIFT", "rct": "SYMBOL SHIFT", "rsh": "SYMBOL SHIFT (sends Right Ctrl)", "up": "Cursor up",
        "dn": "Cursor down", "lft": "Cursor left", "rt": "Cursor right", "f1": "Tape pause", "f2": "Tape previous part",
        "f3": "Tape next part", "f4": "CPU 3.5 MHz", "f5": "CPU 7 MHz", "f6": "CPU 14 MHz", "f7": "CPU 28 MHz",
        "f8": "CPU 56 MHz", "f9": "Pause / continue", "f10": "48K BASIC", "f11": "+D snapshot / file browser",
        "f12": "MiSTer OSD menu", "prt": "+D snapshot / file browser (F11)", "scr": "MiSTer OSD menu (F12)",
        "pau": "Pause / continue (F9)", "lbr": "(", "rbr": ")", "hsh": "*", "quo": '" (Symbol Shift + P)'}

# ---------- keycaps ----------
cap_colour = {GREY: "grey", BLACK: "black", F58: "light grey (F5-F8)"}
caps = []
for d in K:
    caps.append({"id": d["i"], "x": d["x"], "y": d["y"], "w": d["w"], "cap": cap_colour.get(d["c"], "grey"),
                 "main": d["m"], "sub": d["s"], "keyword": d["kw"]})
caps.append({"id": "ent", "x": 13.5, "y": 2.25, "w": 1.5, "h": 2, "cap": "grey", "main": "ENTER", "sub": "", "keyword": "",
             "note": "ISO Enter (2 rows tall)"})
caps.append({"id": "knob", "x": 15.12, "y": 0, "w": 1, "cap": "Keychron knob", "main": "KNOB", "sub": "", "keyword": "", "note": "rotary encoder, no LED"})
json.dump({"board": "Keychron Q3 UK ISO with knob (keychron/q3/iso_encoder)",
           "keycaps": "KAM Wraith ZX Spectrum, grey and black sets mixed",
           "units": "x/y/w in key units (1u = one standard key)", "keys": caps},
          open(os.path.join(OUT, "keycaps.json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)

# ---------- layout ----------
led, keys = 0, []
capmap = {c["id"]: c for c in caps}
for kid in ORDER:
    c = capmap[kid]
    entry = {"id": kid, "x": c["x"], "y": c["y"], "w": c["w"],
             "cap": " / ".join(p for p in (c["main"], c["sub"], c["keyword"]) if p) or "(blank)",
             "led": None if kid == "knob" else led,
             "keycodes": {L: layers[L][kid] for L in layers},
             "windows": describe(layers["WIN_BASE"][kid]),
             "spectrum": describe(layers["SPEC_BASE"][kid]),
             "fn": describe(layers["WIN_FN"][kid]) if layers["WIN_FN"][kid] != "_______" else ""}
    if kid in CORE: entry["spectrum_core"] = CORE[kid]
    if kid != "knob": led += 1
    keys.append(entry)
assert led == 88
json.dump({"board": "Keychron Q3 UK ISO with knob", "led_count": 88, "caps_lock_led": 49,
           "layers": {"0 SPEC_BASE": "Spectrum mode (side switch on Mac)", "1 SPEC_FN": "Fn in Spectrum mode",
                      "2 WIN_BASE": "Windows mode (side switch on Windows)", "3 WIN_FN": "Fn in Windows mode"},
           "knob_turn": enc, "keys": keys},
          open(os.path.join(OUT, "layout.json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)

# ---------- markdown ----------
def md_escape(s): return s.replace("|", "\\|")
with open(os.path.join(OUT, "keycaps.md"), "w", encoding="utf-8") as f:
    f.write("# Wraith keycap map\n\nKAM Wraith ZX Spectrum caps (grey and black sets mixed) on the Keychron Q3 UK ISO knob.\n"
            "Rows top to bottom, keys left to right. Picture: `keycaps.png`.\n\n"
            "| Key id | Cap colour | Main legend | Sub legend | BASIC keyword |\n|---|---|---|---|---|\n")
    for c in sorted(caps, key=lambda c: (c["y"], c["x"])):
        f.write(f"| {c['id']} | {c['cap']} | {md_escape(c['main'])} | {md_escape(c['sub'])} | {c['keyword']} |\n")
with open(os.path.join(OUT, "layout.md"), "w", encoding="utf-8") as f:
    f.write("# Keyboard layout and functions\n\nGenerated from `firmware/keymaps/d0k/keymap.c` by `tools/gen_keyboard_docs.py`.\n"
            "LED = RGB matrix index (Caps Lock = 49). Spectrum = what the keyboard sends with the switch on Mac;\n"
            "Core = how the MiSTer ZX Spectrum core interprets it. Fn = same in both modes.\n\n"
            f"Knob turn: Spectrum `{enc.get('SPEC_BASE','')}`, Windows `{enc.get('WIN_BASE','')}`, Fn `{enc.get('WIN_FN','')}`.\n\n"
            "| Key | Wraith cap | LED | Windows | Spectrum | Core | Fn |\n|---|---|---|---|---|---|---|\n")
    for k in keys:
        f.write(f"| {k['id']} | {md_escape(k['cap'])} | {'' if k['led'] is None else k['led']} | {md_escape(k['windows'])} | "
                f"{md_escape(k['spectrum'])} | {md_escape(k.get('spectrum_core',''))} | {md_escape(k['fn'])} |\n")

# ---------- pictures (HTML; PNGs made by the caller with a headless browser) ----------
w = lambda x: (x, "win"); fnk = lambda x: (x, "fn"); sp = lambda x: (x, "spec"); co = lambda x: (x, "core")
render("Wraith keycap map", "KAM Wraith ZX Spectrum caps on the Keychron Q3 UK ISO knob", {}, ("", None), None,
       os.path.join(OUT, "keycaps"))
SHORT = {"Screen brightness down": "Screen -", "Screen brightness up": "Screen +", "Lights dimmer": "Lights -",
         "Lights brighter": "Lights +", "Previous track": "Previous", "Next track": "Next", "Volume down": "Volume -",
         "Volume up": "Volume +", "Next effect": "Effect >", "Previous effect": "Effect <", "Saturation +": "Colour +",
         "Saturation -": "Colour -", "Effect speed +": "Speed +", "Effect speed -": "Speed -", "VU meter on/off": "VU on/off",
         "NKRO on/off": "NKRO", "Windows key lock": "Win lock", "Print Screen": "Print Scr", "Right Shift": "Shift",
         "Left Shift": "Shift", "Left Ctrl": "Ctrl", "Right Ctrl": "Ctrl"}
SPEC_LABEL = {"ins": "EDIT", "hom": "CAPS LOCK", "pgu": "TRUE VIDEO", "del": "INV VIDEO", "end": "GRAPHICS",
              "pgd": "DELETE", "iso": "EDIT", "rsh": "SYMBOL SHIFT", "lwn": "EXTEND", "rwn": 'LOAD ""',
              "prt": "Snap menu", "scr": "OSD menu", "pau": "Pause"}
WIN = {"_dim": 1}
FN = {"_dim": 1, "fn": ("HOLD", "hold")}
SP = {"_dim": 1, "fn": sp("Fn")}
for k in keys:
    kid = k["id"]
    if kid == "knob": continue
    if not re.fullmatch(r"KC_[A-Z0-9]|KC_F\d+|KC_(GRV|MINS|EQL|LBRC|RBRC|SCLN|QUOT|COMM|DOT|SLSH)", layers["WIN_BASE"][kid]):
        lab = k["windows"].split(" (")[0]
        WIN[kid] = w(SHORT.get(lab, lab))
    if k["fn"] and kid != "fn" and layers["WIN_FN"][kid] != "KC_NO":
        FN[kid] = fnk(SHORT.get(k["fn"], k["fn"]))
    if kid in SPEC_LABEL: SP[kid] = sp(SPEC_LABEL[kid])
    elif kid in CORE: SP[kid] = co(CORE[kid].split(" (")[0].replace("+D snapshot / file browser", "Snap menu")
                                   .replace("MiSTer OSD menu", "OSD menu").replace("Tape previous part", "Tape back")
                                   .replace("Tape next part", "Tape skip").replace("Pause / continue", "Pause")
                                   .replace("CPU ", "").replace("Cursor ", "Cursor ").replace("SYMBOL SHIFT", "SYMBOL SH"))
SP["ent"] = co("ENTER"); SP["spc"] = co("SPACE")
render("Windows mode (side switch on Windows)", "Orange strip = what the key does in Windows.", WIN,
       ("Turn: Volume | Press: Mute", "win"), None, os.path.join(OUT, "windows"))
render("Fn combinations (both modes)", "Hold EDIT / DEF FN (white outline), then press a blue key.", FN,
       ("Fn + turn: Lights | Fn + press: on/off", "fn"), None, os.path.join(OUT, "fn"))
render("Spectrum mode (side switch on Mac)", "Green = remapped on the keyboard. Purple = handled by the MiSTer core.", SP,
       ("Turn: cursor left / right", "core"), None, os.path.join(OUT, "spectrum"))
print("ok", len(caps), "caps,", len(keys), "layout entries")
