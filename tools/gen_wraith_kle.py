"""Write D0k's KAM Wraith ZX Spectrum keycaps on the Keychron Q3 as a ZXKB keycap set (KLE JSON).

    python tools/gen_wraith_kle.py

Reads docs/keyboard/keycaps.json (positions, sizes, legends) and tools/wraith_caps.py (cap and legend colours) and
writes app/zxkb/keycaps/KAM Wraith ZX Spectrum (Keychron Q3 ISO).json in the format of keyboard-layout-editor.com,
which other keycap sets use too. Label positions (KLE numbering, 0 top left ... 11 front right), as ZXKB reads them:
0 big single character, 2 its shifted symbol, 4 a word legend, 7 the second word or the BASIC keyword. "c" is the cap
top colour, "zs" (ZXKB's own, ignored by KLE) the cap side colour, "t" the legend colours per label.
"""
import json, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import wraith_caps

caps = json.load(open(os.path.join(ROOT, 'docs', 'keyboard', 'keycaps.json'), encoding='utf-8'))['keys']
leds = {k['id']: k.get('led') for k in json.load(open(os.path.join(ROOT, 'docs', 'keyboard', 'layout.json'), encoding='utf-8'))['keys']}
# QMK's own Q3 description: LED i sits at matrix position rgb_matrix.layout[i].matrix; the knob press is the one
# matrix position in the key layout with no LED. ZXKB matches caps to keys by this matrix position ("zm").
QMK_KB = os.path.join(os.path.expanduser('~'), 'qmk_firmware', 'keyboards', 'keychron', 'q3', 'iso_encoder', 'keyboard.json')
qmk = json.load(open(QMK_KB, encoding='utf-8'))
led_matrix = [tuple(e['matrix']) for e in qmk['rgb_matrix']['layout']]
layout_matrix = [tuple(k['matrix']) for k in next(iter(qmk['layouts'].values()))['layout']]
no_led = [m for m in layout_matrix if m not in led_matrix]
colours = {k['i']: k for k in wraith_caps.K}

keys = []
for c in caps:
    kid = c['id']
    col = colours.get(kid)
    if kid == 'ent':
        top, side, mc, sc = wraith_caps.GREY[0], wraith_caps.GREY[1], wraith_caps.WH, wraith_caps.RD
    elif col:
        top, side, mc, sc = col['c'][0], col['c'][1], col['mc'], col['sc']
    else:  # the knob
        top, side, mc, sc = '#3a4048', '#2a2e34', '#9aa3ad', '#9aa3ad'
    labels = [''] * 12
    tcol = [''] * 12
    main, sub, kw = c.get('main', ''), c.get('sub', ''), c.get('keyword', '')
    if len(main) == 1:
        labels[0], tcol[0] = main, mc
        labels[2], tcol[2] = sub, sc
        labels[7], tcol[7] = kw, '#f2f5f8'
    else:
        labels[4], tcol[4] = main, mc
        if sub:
            labels[7], tcol[7] = sub, sc
        else:
            labels[7], tcol[7] = kw, '#f2f5f8'
    if kid == 'knob':
        labels[9] = 'e0'
    led = leds.get(kid)
    m = led_matrix[led] if led is not None else (no_led[0] if kid == 'knob' and len(no_led) == 1 else None)
    props = {'c': top, 'zs': side, 't': '\n'.join(tcol).rstrip('\n') or mc}
    if m:
        props['zm'] = '%d,%d' % m
    w, h = c['w'], c.get('h', 1)
    if w != 1:
        props['w'] = w
    if h != 1:
        props['h'] = h
    if kid == 'ent':  # ISO Enter: 1.25 x 2 below, 1.5 x 1 on the top row, 0.25 further left
        props.update({'x': 0.25, 'w': 1.25, 'h': 2, 'x2': -0.25, 'w2': 1.5, 'h2': 1})
    keys.append((c['y'], c['x'], props, '\n'.join(labels).rstrip('\n'), kid))

keys.sort(key=lambda k: (k[0], k[1]))
rows, row, row_y, cur_x = [{'name': 'KAM Wraith ZX Spectrum (Keychron Q3 ISO)', 'author': 'D0k'}], None, None, 0.0
prev_y = -1.0
for y, x, props, label, kid in keys:
    if row is None or y != row_y:
        row = []
        rows.append(row)
        props = dict(props)
        dy = round(y - (prev_y + 1), 4)
        if dy:
            props['y'] = dy
        prev_y, row_y, cur_x = y, y, 0.0
    else:
        props = dict(props)
    gap = round(x - cur_x, 4) + props.get('x', 0)
    if gap:
        props['x'] = round(gap, 4)
    else:
        props.pop('x', None)
    row.append(props)
    row.append(label)
    width = props.get('w', 1)
    cur_x = x + (0.25 if kid == 'ent' else 0) + width

out_dir = os.path.join(ROOT, 'app', 'zxkb', 'keycaps')
os.makedirs(out_dir, exist_ok=True)
out = os.path.join(out_dir, 'KAM Wraith ZX Spectrum (Keychron Q3 ISO).json')
with open(out, 'w', encoding='utf-8', newline='\n') as f:
    json.dump(rows, f, ensure_ascii=True, indent=1)
print(f'{len(keys)} keys -> {out}')
