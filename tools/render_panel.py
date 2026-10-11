r"""Draws the /cinderload window as the stubbed 1.12 UI holds it (tests/test_lua.lua layout=<tree>: every region's
place, size, texture, texture coordinates, colours, words, font and backdrop, as dll/switches.lua made them), with the
client's own art and fonts read out of its archives (the highest patch winning, as the game reads them) and the
window's pictures read out of CinderLoad's pack by the paths the window loads them by. A picture of what the code
makes, not a drawing of the mockup: where the stub's layout and the client's own drawing differ (font rasterising,
the exact width of words), this is an approximation.

Usage: STORMLIB=/path/to/libstorm.so python3 -I render_panel.py <tree tsv> <client Data dir> <pack mpq> <out png>
       [--game <frame png> --at <x> <y>] [--scale 2]
   The window is drawn at his screen (5120 x 2160, UI scale as the tree has it); with --game, over that picture
   (stretched to 5120 x 2160) with its top left at x, y, then cut out with a margin as the mockup's close-up is.
"""
import ctypes, io, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

SH = 2160
K = SH / 768.0                     # pixels per screen unit at scale 1 on his screen
LAYERS = {'BACKGROUND': 0, 'BORDER': 1, 'ARTWORK': 2, 'OVERLAY': 3, 'HIGHLIGHT': 4}


class Archives:
    """Files out of the client's archives (read only), the highest patch winning, then CinderLoad's pack over them."""
    def __init__(self, data, extra):
        self.L = L = ctypes.CDLL(os.environ.get('STORMLIB', 'libstorm.so'))
        L.SFileOpenArchive.argtypes = [ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p)]
        L.SFileOpenFileEx.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p)]
        L.SFileGetFileSize.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint)]
        L.SFileGetFileSize.restype = ctypes.c_uint
        L.SFileReadFile.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint), ctypes.c_void_p]
        L.SFileCloseFile.argtypes = [ctypes.c_void_p]
        L.SFileOpenArchive.restype = L.SFileOpenFileEx.restype = ctypes.c_bool
        names = {f.lower(): f for f in os.listdir(data) if f.lower().endswith('.mpq') and f.lower() != 'patch-~.mpq'}
        base = ['base.mpq', 'dbc.mpq', 'fonts.mpq', 'interface.mpq', 'misc.mpq', 'model.mpq', 'sound.mpq', 'speech.mpq',
                'terrain.mpq', 'texture.mpq', 'wmo.mpq', 'backup.mpq']
        order = [names[b] for b in base if b in names]
        order += sorted([n for k, n in names.items() if k.startswith('patch')], key=lambda n: (n.lower() != 'patch.mpq', n.lower()))
        self.h = []
        for n in [os.path.join(data, n) for n in order] + [extra]:
            h = ctypes.c_void_p()
            if L.SFileOpenArchive(n.encode(), 0, 0x100, ctypes.byref(h)):
                self.h.append((n, h))
        self.cache = {}

    def read(self, path):
        best = None
        for n, h in self.h:
            f = ctypes.c_void_p()
            if not self.L.SFileOpenFileEx(h, path.encode(), 0, ctypes.byref(f)):
                continue
            size = self.L.SFileGetFileSize(f, None)
            if size and size != 0xFFFFFFFF:
                b = ctypes.create_string_buffer(size)
                got = ctypes.c_uint()
                self.L.SFileReadFile(f, b, size, ctypes.byref(got), None)
                if got.value == size:
                    best = (b.raw, n)
            self.L.SFileCloseFile(f)
        return best

    def texture(self, path):
        if path not in self.cache:
            got = self.read(path if path.lower().endswith('.blp') else path + '.blp')
            self.cache[path] = Image.open(io.BytesIO(got[0])).convert('RGBA') if got else None
            if got:
                self.cache[path].info['from'] = os.path.basename(got[1])
        return self.cache[path]


def floats(s):
    return [float(x) for x in s.split()] if s else []


def load(tree):
    rows = []
    for line in open(tree, encoding='latin-1'):
        c = line.rstrip('\n').split('\t')
        rows.append(dict(i=int(c[0]), kind=c[1], name=c[2], parent=int(c[3]) if c[3] else 0, role=c[4], layer=c[5],
                         strata=c[6], level=int(float(c[7] or 0)), visible=c[8] == '1', rect=[float(x) if x else None for x in c[9:13]],
                         texture=c[13], tc=floats(c[14]), color=floats(c[15]), blend=c[16], text=c[17], font=c[18],
                         size=float(c[19] or 0), justify=c[20], checked=c[21] == '1', bg=c[22], edge=c[23],
                         edgeSize=float(c[24] or 0), insets=floats(c[25]), bdColor=floats(c[26]), bdBorder=floats(c[27]),
                         min=float(c[28] or 0), max=float(c[29] or 0), value=float(c[30] or 0), es=float(c[31] or 1),
                         point=c[32] if len(c) > 32 else ''))
    return {r['i']: r for r in rows}


def main():
    tree, data, pack, out = sys.argv[1:5]
    scale = float(sys.argv[sys.argv.index('--scale') + 1]) if '--scale' in sys.argv else 1.0
    arc = Archives(data, pack)
    R = load(tree)
    panel = next(r for r in R.values() if r['name'] == 'CinderLoadPanel')
    pl, pb, pr, pt = panel['rect']
    k = K * scale
    W, H = int(round((pr - pl) * k)), int(round((pt - pb) * k))
    pad = int(48 * scale)                                      # (the title rises over the top edge)
    canvas = Image.new('RGBA', (W + 2 * pad, H + 2 * pad), (0, 0, 0, 0))

    def box(r):
        l, b, rr, t = r['rect']
        return (l - pl) * k + pad, (pt - t) * k + pad, (rr - pl) * k + pad, (pt - b) * k + pad

    def put(im, x0, y0, x1, y1, tint=None):
        w, h = int(round(x1 - x0)), int(round(y1 - y0))
        if w <= 0 or h <= 0 or im is None:
            return
        im = im.resize((w, h), Image.LANCZOS)
        if tint:
            a = np.asarray(im).astype(np.float32)
            a[..., :3] *= np.array(tint[:3], np.float32)
            if len(tint) > 3:
                a[..., 3] *= tint[3]
            im = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
        canvas.alpha_composite(im, (int(round(x0)), int(round(y0))))

    def crop(im, tc):
        if not tc or im is None:
            return im
        u0, u1, v0, v1 = tc
        return im.crop((int(round(u0 * im.width)), int(round(v0 * im.height)), int(round(u1 * im.width)), int(round(v1 * im.height))))

    def backdrop(r):
        x0, y0, x1, y1 = box(r)
        e = r['edgeSize'] * r['es'] * k
        ins = [v * r['es'] * k for v in (r['insets'] or [0, 0, 0, 0])]
        if r['bg']:
            bg = arc.texture(r['bg'])
            c = r['bdColor']
            if c:                                              # (a colour on the plain tooltip background: that colour)
                fill = Image.new('RGBA', (max(1, int(x1 - x0 - ins[0] - ins[1])), max(1, int(y1 - y0 - ins[2] - ins[3]))),
                                 tuple(int(255 * v) for v in c[:3]) + (int(255 * (c[3] if len(c) > 3 else 1)),))
                canvas.alpha_composite(fill, (int(x0 + ins[0]), int(y0 + ins[2])))
            elif bg is not None:
                put(bg, x0 + ins[0], y0 + ins[2], x1 - ins[1], y1 - ins[3])
        if r['edge']:
            tex = arc.texture(r['edge'])
            if tex is None or e <= 0:
                return
            n = tex.width // 8
            seg = [tex.crop((i * n, 0, (i + 1) * n, tex.height)) for i in range(8)]
            tint = r['bdBorder'] or None
            top, bot = seg[2].transpose(Image.ROTATE_270), seg[3].transpose(Image.ROTATE_270)
            put(seg[0], x0, y0 + e, x0 + e, y1 - e, tint)
            put(seg[1], x1 - e, y0 + e, x1, y1 - e, tint)
            put(top, x0 + e, y0, x1 - e, y0 + e, tint)
            put(bot, x0 + e, y1 - e, x1 - e, y1, tint)
            for s, (cx, cy) in zip(seg[4:], [(x0, y0), (x1 - e, y0), (x0, y1 - e), (x1 - e, y1 - e)]):
                put(s, cx, cy, cx + e, cy + e, tint)

    fonts = {}

    def font(path, px):
        key = (path, int(round(px)))
        if key not in fonts:
            got = arc.read(path)
            fonts[key] = ImageFont.truetype(io.BytesIO(got[0]), max(1, int(round(px))))
        return fonts[key]

    def text(r):
        if not r['text']:
            return
        x0, y0, x1, y1 = box(r)
        f = font(r['font'] or 'Fonts\\FRIZQT__.TTF', r['size'] * r['es'] * k)
        c = tuple(int(255 * v) for v in (r['color'] or [1, 1, 1])[:3]) + (255,)
        j = r['justify'] or 'CENTER'
        pt = r['point']                                       # (one point: the words sized to themselves, held there)
        if pt:
            j = 'LEFT' if pt.endswith('LEFT') else 'RIGHT' if pt.endswith('RIGHT') else 'CENTER'
        x, anchor = ((x0, 'lm') if j == 'LEFT' else (x1, 'rm') if j == 'RIGHT' else ((x0 + x1) / 2, 'mm'))
        y = (y0 + y1) / 2
        d = ImageDraw.Draw(canvas)
        sh = max(1, round(r['es'] * k))                       # (the game fonts' shadow, 1 unit down and right)
        d.text((x + sh, y + sh), r['text'], font=f, fill=(0, 0, 0, 255), anchor=anchor)
        d.text((x, y), r['text'], font=f, fill=c, anchor=anchor)

    def visible_tex(r):
        if r['role'] in ('highlight', 'pushed', 'disabled', 'disabledchecked'):
            return False
        if r['role'] == 'checked':
            return R[r['parent']]['checked']
        return True

    # each region at its frame's level: the frame's backdrop, then its regions by layer, then the frames over it
    items = []
    for r in R.values():
        if not r['visible'] or r['rect'][0] is None and r['role'] != 'thumb':
            continue
        if r['kind'] in ('Frame', 'Button', 'CheckButton', 'Slider'):
            if r['i'] == panel['i'] or panel_has(R, r, panel['i']):
                if r['bg'] or r['edge']:
                    items.append(((r['level'], -1, r['i']), 'backdrop', r))
        elif r['kind'] in ('Texture', 'FontString') and panel_has(R, r, panel['i']):
            owner = R[r['parent']]
            items.append(((owner['level'], LAYERS.get(r['layer'], 2), r['i']), r['kind'], r))
    for _, what, r in sorted(items, key=lambda t: t[0]):
        if what == 'backdrop':
            backdrop(r)
        elif what == 'FontString':
            text(r)
        elif visible_tex(r):
            if r['role'] == 'thumb':                          # the slider's thumb, where its value puts it
                s = R[r['parent']]
                x0, y0, x1, y1 = box(s)
                f = (s['value'] - s['min']) / ((s['max'] - s['min']) or 1)
                cx, cy, half = x0 + 8 * s['es'] * k + (x1 - x0 - 16 * s['es'] * k) * f, (y0 + y1) / 2, 16 * s['es'] * k
                put(arc.texture(r['texture']), cx - half, cy - half, cx + half, cy + half)
                continue
            x0, y0, x1, y1 = box(r)
            if r['color'] and not r['texture']:
                c = r['color']
                fill = Image.new('RGBA', (max(1, int(round(x1 - x0))), max(1, int(round(y1 - y0)))),
                                 tuple(int(255 * v) for v in c[:3]) + (int(255 * (c[3] if len(c) > 3 else 1)),))
                canvas.alpha_composite(fill, (int(round(x0)), int(round(y0))))
            elif r['texture']:
                put(crop(arc.texture(r['texture']), r['tc']), x0, y0, x1, y1)
    if '--game' in sys.argv:
        game = Image.open(sys.argv[sys.argv.index('--game') + 1]).convert('RGB').resize((5120, 2160), Image.LANCZOS).convert('RGBA')
        i = sys.argv.index('--at')
        ax, ay = int(sys.argv[i + 1]), int(sys.argv[i + 2])
        one = canvas.resize((int(canvas.width / scale), int(canvas.height / scale)), Image.LANCZOS) if scale != 1 else canvas
        p1 = int(48)
        game.alpha_composite(one, (ax - p1, ay - p1))
        m, rise = 60, int(round(14 * 0.82 * K))
        cut = game.crop((ax - m, ay - m - rise, ax + int(W / scale) + m, ay + int(H / scale) + m)).convert('RGB')
        cut.resize((cut.width * 2, cut.height * 2), Image.LANCZOS).save(out)
    else:
        canvas.save(out)
    used = sorted({(p, im.info.get('from', '?')) for p, im in arc.cache.items() if im is not None})
    missing = sorted(p for p, im in arc.cache.items() if im is None)
    print('drawn', out, canvas.size)
    for p, a in used:
        print('  art', p, 'from', a)
    for p in missing:
        print('  MISSING', p)


def panel_has(R, r, pid):
    p = r['parent']
    while p:
        if p == pid:
            return True
        p = R[p]['parent']
    return False


if __name__ == '__main__':
    main()
