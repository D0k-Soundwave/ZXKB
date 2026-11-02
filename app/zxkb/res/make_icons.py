"""Draw ZXKB's tray icons (the four Spectrum stripes from ZX VU) as .ico files, no extra libraries.
zxkb.ico = keyboard connected (colour), zxkb_off.ico = not connected (grey)."""
import struct, sys, os

STRIPES = [(255, 0, 0), (255, 200, 0), (0, 220, 0), (0, 200, 255)]   # same as ZX VU's icon
GREY = (90, 90, 90)
SIZES = [16, 20, 24, 32, 40, 48]   # tray at 100-200% display scaling; larger sizes would only bloat the exe
SS = 4   # supersampling per axis, for smooth edges

def inside(px, py, poly):
    c = False
    for i in range(len(poly)):
        (x1, y1), (x2, y2) = poly[i], poly[i - 1]
        if (y1 > py) != (y2 > py) and px < (x2 - x1) * (py - y1) / (y2 - y1) + x1:
            c = not c
    return c

def draw(size, colours):
    # ZX VU drew on 64x64: stripe i = (x,60) (x+10,60) (x+30,4) (x+20,4) with x = 14 + 10i
    polys = [[(14 + i * 10, 60), (24 + i * 10, 60), (44 + i * 10, 4), (34 + i * 10, 4)] for i in range(4)]
    px = []
    for y in range(size):
        row = []
        for x in range(size):
            acc = [0, 0, 0, 0]
            for sy in range(SS):
                for sx in range(SS):
                    u = (x + (sx + 0.5) / SS) * 64 / size; v = (y + (sy + 0.5) / SS) * 64 / size
                    for k, poly in enumerate(polys):
                        if inside(u, v, poly):
                            r, g, b = colours[k]; acc[0] += r; acc[1] += g; acc[2] += b; acc[3] += 1
                            break
            n = SS * SS
            a = acc[3]
            row.append((acc[0] // a, acc[1] // a, acc[2] // a, 255 * a // n) if a else (0, 0, 0, 0))
        px.append(row)
    return px

def bmp(size, px):
    head = struct.pack('<IiiHHIIiiII', 40, size, size * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    data = b''.join(bytes((b, g, r, a)) for row in reversed(px) for (r, g, b, a) in row)
    mask_row = ((size + 31) // 32) * 4
    return head + data + b'\0' * (mask_row * size)

def ico(path, colours):
    images = [bmp(s, draw(s, colours)) for s in SIZES]
    out = struct.pack('<HHH', 0, 1, len(SIZES)); off = 6 + 16 * len(SIZES)
    for s, img in zip(SIZES, images):
        out += struct.pack('<BBBBHHII', s % 256, s % 256, 0, 0, 1, 32, len(img), off); off += len(img)
    open(path, 'wb').write(out + b''.join(images))

here = os.path.dirname(os.path.abspath(__file__))
ico(os.path.join(here, 'zxkb.ico'), STRIPES)
ico(os.path.join(here, 'zxkb_off.ico'), [GREY] * 4)
print('icons written')
