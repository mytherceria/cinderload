r"""Stills of the satellite for his morning look (CinderLoad 0.4.0, 13 Oct 2026): 0.3.0 against 0.4.0 on one screen
shape, drawn the way the DLL frames a view (a square in the picture's u, v stretched over the screen; ViewRect,
DriftStep), from the files that ship (the BLPs decoded, not the PNGs), with the place names by zoom drawn as
LabelsBuild draws them (labels.txt items whose band holds the view's half height, faded over the band's last 20%,
sized for a 2160-row screen). Not a film: no clouds, waves, ships or glow (those are 0.3.0's, unchanged).

Usage: STORMLIB=/path/to/libstorm.so python3 sat_stills.py <0.3.0 packs> <ov dir> <sat dir> <out dir> [shape
         [<master dir> [world,landing,coasts]]]   (with the master: also the coast close-ups, the mask's evidence)
  <ov dir>   sat_overview.py's output (CinderMap-<shape>.blp, alpha-<shape>.png)
  <sat dir>  make_sat.py's output (<px>/sat.txt, <px>/tiles/*.blp) for 512 and 256
Writes numbered JPEGs (quality 92) and stills.txt (what each one is).
"""
import math, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import satgeo
from blp import read_blp
from sat_overview import Mpq

Image.MAX_IMAGE_PIXELS = None
SCREEN = {'16x10': (2560, 1600), '16x9': (3840, 2160), '21x9': (5120, 2160), '32x9': (5120, 1440)}
FONT = '/usr/share/fonts/TTF/DejaVuSans-Bold.ttf'
LANDING_YD = 3500
PLACES = [  # (file stem, title, continent, world x, world y)
    ('goldshire', 'Goldshire (the Lion\'s Pride inn)', 0, -9464.25, 24.40),
    ('stormwind', 'Stormwind', 0, -8830, 640),
    ('orgrimmar', 'Orgrimmar', 1, 1650, -4400),
    ('ironforge', 'Ironforge', 0, -4900, -1000),
    ('moonglade', 'Moonglade (Nighthaven)', 1, 7850, -2580),
    ('bootybay', 'Booty Bay', 0, -14297, 530),
    ('gilneas', 'Gilneas (RavenCraft custom zone)', 0, -1580, 1420),
    ('lapidis', 'Lapidis Isle (RavenCraft custom zone)', 0, -12001, 3483),
]


def font(px):
    try:
        return ImageFont.truetype(FONT, px)
    except OSError:
        return ImageFont.load_default()


class Shape:
    def __init__(self, packs, ov, shape):
        self.shape = shape
        pk = Mpq(os.path.join(packs, 'LoadingScreens-%s.MPQ' % shape))
        self.table, self.other = satgeo.read_map_txt(pk.read('CinderLoad\\map.txt').decode('ascii'))
        self.old = Image.fromarray(read_blp(pk.read('Interface\\Glues\\LoadingScreens\\CinderMap.blp'))[1](0)[..., :3])
        self.labels_txt = pk.read('CinderLoad\\labels.txt').decode('latin-1')
        sheet = read_blp(pk.read('Interface\\Glues\\LoadingScreens\\CinderLabels.blp'))[1](0)
        pk.close()
        self.sheet = Image.fromarray(sheet, 'RGBA')
        self.new = Image.fromarray(read_blp(open(os.path.join(ov, 'CinderMap-%s.blp' % shape), 'rb').read())[1](0)[..., :3])
        self.spr, self.items = {}, []
        for line in self.labels_txt.splitlines():
            w = line.split()
            if not w or w[0].startswith('#'):
                continue
            if w[0] == 's':
                self.spr[int(w[1])] = tuple(int(v) for v in w[2:6])
            elif w[0] == 'i':
                self.items.append((int(w[1]), satgeo.unhex(w[2]), satgeo.unhex(w[3]), int(w[4]), int(w[5]), int(w[6])))

    def view(self, pic, cu, cv, h, W, H):
        """The picture over the view (u, v in [cu-h, cu+h] x [cv-h, cv+h]) at W x H screen pixels."""
        N = pic.size[0]
        box = ((cu - h) * N, (cv - h) * N, (cu + h) * N, (cv + h) * N)
        return pic.resize((W, H), Image.LANCZOS if 2 * h * N > H else Image.BILINEAR, box=box)

    def labels(self, im, cu, cv, h):
        """labels.txt's items for this view, as LabelsBuild places and fades them."""
        W, H = im.size
        hk, k = 1000 * h, H / 2160.0
        out = im.convert('RGBA')
        for spr, u, v, hi, lo, dy in self.items:
            if not (lo <= hk <= hi):
                continue
            a, span = 1.0, hi - lo
            if hi < 500 and hk > hi - span * 0.2:
                a = (hi - hk) / (span * 0.2)
            if lo > 0 and hk < lo + span * 0.2:
                a = min(a, (hk - lo) / (span * 0.2))
            if a <= 0.01:
                continue
            x0, y0, x1, y1 = self.spr[spr]
            sw, sh = max(1, round((x1 - x0) * k)), max(1, round((y1 - y0) * k))
            sx = (u - (cu - h)) / (2 * h) * W
            sy = (v - (cv - h)) / (2 * h) * H + dy * k
            if sx + sw / 2 < 0 or sx - sw / 2 > W or sy + sh / 2 < 0 or sy - sh / 2 > H:
                continue
            s = self.sheet.crop((x0, y0, x1, y1)).resize((sw, sh), Image.LANCZOS)
            if a < 1:
                s.putalpha(Image.eval(s.getchannel('A'), lambda p: int(p * a)))
            layer = Image.new('RGBA', out.size, (0, 0, 0, 0))
            layer.paste(s, (round(sx - sw / 2), round(sy - sh / 2)))
            out = Image.alpha_composite(out, layer)
        return out.convert('RGB')

    def detail(self, base, sat_dir, px, u0, v0, u1, v1):
        """base (the overview over u0..u1, v0..v1) with the detail tiles laid over it, at full alpha (the landing is
        inside the placeholder fade's full range), as one mosaic resampled once, so no seam is the still's own."""
        W, H = base.size
        idx = os.path.join(sat_dir, str(px), 'sat.txt')
        present = {tuple(int(v) for v in l.split()[1:]) for l in open(idx) if l.startswith('t ')}
        out = base.copy()
        for c in (0, 1):
            au, bu, av, bv = self.table[c]
            # the view's corners in world yards, then in tiles (X runs with u, Y with v)
            fx = lambda u: (satgeo.ORIGIN - (u - au) / bu) / satgeo.TILE
            fy = lambda v: (satgeo.ORIGIN - (v - av) / bv) / satgeo.TILE
            X0, X1 = fx(u0), fx(u1)
            Y0, Y1 = fy(v0), fy(v1)
            xs = range(max(0, math.floor(X0)), min(63, math.floor(X1)) + 1)
            ys = range(max(0, math.floor(Y0)), min(63, math.floor(Y1)) + 1)
            keys = [(c, X, Y) for Y in ys for X in xs if (c, X, Y) in present]
            if not keys:
                continue
            xa, ya = min(k[1] for k in keys), min(k[2] for k in keys)
            xb, yb = max(k[1] for k in keys), max(k[2] for k in keys)
            mos = Image.new('RGBA', ((xb - xa + 1) * px, (yb - ya + 1) * px), (0, 0, 0, 0))
            for _, X, Y in keys:
                t = read_blp(open(os.path.join(sat_dir, str(px), 'tiles', '%d_%d_%d.blp' % (c, X, Y)), 'rb').read())[1](0)
                t[..., 3] = 255
                mos.paste(Image.fromarray(t, 'RGBA'), ((X - xa) * px, (Y - ya) * px))
            box = ((X0 - xa) * px, (Y0 - ya) * px, (X1 - xa) * px, (Y1 - ya) * px)
            # PIL wants the box inside the mosaic: pad it with transparent tiles' worth of margin
            pad = int(math.ceil(max(0, -box[0], -box[1], box[2] - mos.size[0], box[3] - mos.size[1]))) + 2
            big = Image.new('RGBA', (mos.size[0] + 2 * pad, mos.size[1] + 2 * pad), (0, 0, 0, 0))
            big.paste(mos, (pad, pad))
            box = (box[0] + pad, box[1] + pad, box[2] + pad, box[3] + pad)
            # tiles are opaque and holes black with alpha 0, so the RGB as it stands is already premultiplied
            rgb = big.convert('RGB').resize((W, H), Image.LANCZOS, box=box)
            alpha = big.getchannel('A').resize((W, H), Image.BILINEAR, box=box)
            a = np.asarray(alpha, np.float32)[..., None] / 255
            o = np.asarray(out, np.float32)
            o = np.asarray(rgb, np.float32) + o * (1 - a)
            out = Image.fromarray(np.clip(o + 0.5, 0, 255).astype(np.uint8))
        return out


COASTS = [  # (stem, title, continent, world x, world y, yards tall): the mask's feather and the render's edge
    ('coast-bootybay', 'Booty Bay and the Stranglethorn coast', 0, -14200, 700, 1400),
    ('coast-feralas', 'Feralas coast (the client types this water by chunk: ocean, then plain water)', 1, -2450, 3250, 1400),
    ('coast-theramore', 'Theramore Isle and the Dustwallow coast (lakes and marsh water beside the sea)', 1, -3900, -4100, 1600),
    ('coast-worldedge', 'The render\'s own edge, Kalimdor 44_36 to 44_40 (the client\'s unset ground: a tan strip)', 1, -3200, -6890, 1600),
]


def pixel_alpha(im):
    """The other candidate for the mask, for comparison only: the render's ocean from its own pixels (blue over
    brown, not too bright), feathered by a blur of 3 yards. It cannot tell the sea from a lake, nor the render's
    see-through shallows from the wet sand under them."""
    from scipy import ndimage
    a = im.astype(np.float32)
    water = (a[..., 2] > a[..., 0] + 12) & (a[..., 2] >= a[..., 1] - 10) & (a.max(-1) < 200)
    return np.clip(ndimage.gaussian_filter(1.0 - water.astype(np.float32), 3.0), 0, 1)


def coast_still(S, master, sat_dir, c, x, y, tall, W=1000, H=750):
    """Six panels over one window of the world (tall yards high, W x H): 0.3.0 | the 0.4.0 overview | the overview
    with the 512 px detail (the world-data mask) | the same with the pixel mask instead | the world-data alpha with
    the tile grid (red) and chunk grid (dim) | the render as it is (its own sea and edge)."""
    import satmask
    from make_sat import sea_sampler
    wide = tall * W / H
    x0, x1 = x + tall / 2, x - tall / 2                 # north, south
    y0, y1 = y + wide / 2, y - wide / 2                 # west, east
    au, bu, av, bv = S.table[c]
    u0, u1, v0, v1 = au + bu * y0, au + bu * y1, av + bv * x0, av + bv * x1
    N = S.old.size[0]
    view = lambda pic: pic.resize((W, H), Image.LANCZOS if (u1 - u0) * N > W else Image.BILINEAR,
                                  box=(u0 * N, v0 * N, u1 * N, v1 * N))
    old, new = view(S.old), view(S.new)
    det = S.detail(new, sat_dir, 512, u0, v0, u1, v1)
    # the window's tiles from the master at 1024: the world alpha, the pixel alpha, the sea, the raw render
    Xa, Ya, _, _ = satgeo.tile_of(x0, y0)
    Xb, Yb, _, _ = satgeo.tile_of(x1, y1)
    tiles, _ = satgeo.master_tiles(master)
    have = {(cc, X, Y) for cc, X, Y, _ in tiles}
    cont = satmask.Continent(satgeo.MAPS[c], Xa, Xb, Ya, Yb)
    sea = sea_sampler(np.asarray(S.old), S.table, c)
    P = 1024
    nx, ny = Xb - Xa + 1, Yb - Ya + 1
    raw = np.zeros((ny * P, nx * P, 3), np.uint8)
    aw = np.zeros((ny * P, nx * P), np.float32)
    ap = np.zeros((ny * P, nx * P), np.float32)
    sw = np.zeros((ny * P, nx * P, 3), np.float32)
    for Y in range(Ya, Yb + 1):
        for X in range(Xa, Xb + 1):
            sl = (slice((Y - Ya) * P, (Y - Ya + 1) * P), slice((X - Xa) * P, (X - Xa + 1) * P))
            sw[sl] = sea(X, Y)
            if (c, X, Y) not in have:
                raw[sl] = (0, 0, 0)
                continue
            m = np.asarray(Image.open(satgeo.master_path(master, c, X, Y)).convert('RGB'))
            raw[sl] = m
            aw[sl] = satmask.land_alpha(cont.tile_depth(X, Y), P)
            ap[sl] = pixel_alpha(m)
    comp_p = raw * ap[..., None] + sw * (1 - ap[..., None])
    box = ((satgeo.ORIGIN - y0) / satgeo.TILE - Xa, (satgeo.ORIGIN - x0) / satgeo.TILE - Ya,
           (satgeo.ORIGIN - y1) / satgeo.TILE - Xa, (satgeo.ORIGIN - x1) / satgeo.TILE - Ya)
    box = tuple(b * P for b in box)
    cut = lambda a, mode: Image.fromarray(a).resize((W, H), Image.LANCZOS, box=box) if mode == 'RGB' else \
        Image.fromarray(a).resize((W, H), Image.BILINEAR, box=box)
    pix = cut(np.clip(comp_p + 0.5, 0, 255).astype(np.uint8), 'RGB')
    rawv = cut(raw, 'RGB')
    al = np.asarray(cut((aw * 255).astype(np.uint8), 'L'))
    alv = Image.fromarray(np.stack([al] * 3, -1))
    d = ImageDraw.Draw(alv)
    sx = lambda X: (X * P - box[0]) / (box[2] - box[0]) * W
    sy = lambda Y: (Y * P - box[1]) / (box[3] - box[1]) * H
    for k in range(0, (nx + 1) * 16):                   # chunks dim, tiles red
        X = k / 16
        d.line([(sx(X), 0), (sx(X), H)], fill=(255, 0, 0) if k % 16 == 0 else (70, 70, 110), width=2 if k % 16 == 0 else 1)
    for k in range(0, (ny + 1) * 16):
        Y = k / 16
        d.line([(0, sy(Y)), (W, sy(Y))], fill=(255, 0, 0) if k % 16 == 0 else (70, 70, 110), width=2 if k % 16 == 0 else 1)
    return [old, new, det, pix, alv, rawv]


def coasts(S, master, sat_dir, save, shape):
    for stem, title, c, x, y, tall in COASTS:
        p = coast_still(S, master, sat_dir, c, x, y, tall)
        t = ['0.3.0', '0.4.0 overview', '+ 512 detail (world-data mask: used)', 'the pixel mask instead (rejected)',
             'world-data land alpha; tiles red, chunks blue', 'the render as it is']
        rows = [titled(p[:3], t[:3], size=30), titled(p[3:], t[3:], size=30)]
        head = titled([Image.new('RGB', (rows[0].width, 1))], [title + '  (%d yd tall, %s)' % (tall, shape)], size=40)
        save(stack([head, rows[0], rows[1]], gap=6), stem,
             '%s, %d yd tall: top 0.3.0 | the new overview | the overview with the 512 px detail; bottom: the same '
             'detail made with a mask from the render\'s pixels instead (the candidate rejected) | the world-data land '
             'alpha (white = the render, black = 0.3.0\'s sea; red = client tiles, blue = chunks) | the render as it '
             'is, its own sea' % (title, tall))


def titled(panels, titles, gap=12, size=40):
    """Panels side by side, each under a white title on black (his eyes: contrast)."""
    f = font(size)
    bar = size + 24
    W = sum(p.width for p in panels) + gap * (len(panels) - 1)
    H = max(p.height for p in panels) + bar
    out = Image.new('RGB', (W, H), (0, 0, 0))
    d = ImageDraw.Draw(out)
    x = 0
    for p, t in zip(panels, titles):
        d.text((x + 10, 10), t, font=f, fill=(255, 255, 255))
        out.paste(p, (x, bar))
        x += p.width + gap
    return out


def stack(rows, gap=12):
    W = max(r.width for r in rows)
    out = Image.new('RGB', (W, sum(r.height for r in rows) + gap * (len(rows) - 1)), (0, 0, 0))
    y = 0
    for r in rows:
        out.paste(r, (0, y))
        y += r.height + gap
    return out


def main():
    packs, ov, sat, out = (os.path.expanduser(a) for a in sys.argv[1:5])
    shape = sys.argv[5] if len(sys.argv) > 5 else '21x9'
    master = os.path.expanduser(sys.argv[6]) if len(sys.argv) > 6 else None
    only = sys.argv[7].split(',') if len(sys.argv) > 7 else None
    os.makedirs(out, exist_ok=True)
    S = Shape(packs, ov, shape)
    SW, SH = SCREEN[shape]
    HW, HH = SW // 2, SH // 2                   # the side-by-side panels: half his screen each way
    notes = []
    n = [0]

    def save(im, stem, what):
        n[0] += 1
        name = '%02d-%s-%s.jpg' % (n[0], shape, stem)
        im.save(os.path.join(out, name), quality=92)
        notes.append('%s: %s' % (name, what))
        print(name, im.size, flush=True)

    # (a) the whole world, and (b) each continent, old | new, names as the DLL shows them there
    views = [('world', 'the whole world', 0.5, 0.5, 0.5)]
    for c, nm in ((0, 'Eastern Kingdoms'), (1, 'Kalimdor')):
        au, bu, av, bv = S.table[c]
        # the continent's land on the picture: the overview's alpha
        al = np.asarray(Image.open(os.path.join(ov, 'alpha-%s.png' % shape)).reduce(8), np.float32) > 127
        cols = np.nonzero(al.any(0))[0]
        half = cols[cols * 8 / 4096 > 0.5] if c == 0 else cols[cols * 8 / 4096 < 0.5]
        rows = np.nonzero(al[:, half].any(1))[0]
        cu = (half.min() + half.max() + 1) * 8 / 2 / 4096
        cv = (rows.min() + rows.max() + 1) * 8 / 2 / 4096
        h = max((rows.max() - rows.min() + 1) * 8 / 4096 / 2 * 1.04, (half.max() - half.min() + 1) * 8 / 4096 / 2 * 1.04)
        h = min(h, 0.5)
        cu, cv = min(max(cu, h), 1 - h), min(max(cv, h), 1 - h)          # inside the picture, as DriftStep clamps
        views.append((nm.lower().replace(' ', ''), 'one continent: ' + nm, cu, cv, h))
    for stem, what, cu, cv, h in (views if not only or 'world' in only else []):
        old = S.labels(S.view(S.old, cu, cv, h, HW, HH), cu, cv, h)
        new = S.labels(S.view(S.new, cu, cv, h, HW, HH), cu, cv, h)
        save(titled([old, new], ['0.3.0 (shipped)  %s' % what, '0.4.0 overview (3D)  %s' % what]), stem,
             '%s, %s: 0.3.0 left, 0.4.0 right, place names as the DLL shows them at this zoom (view half height %.3f)'
             % (what, shape, h))
    # (c) the landing at each place: the whole screen, then 1:1 crops of his screen
    for stem, title, c, x, y in (PLACES if not only or 'landing' in only else []):
        au, bu, av, bv = S.table[c]
        cu, cv = au + bu * y, av + bv * x
        h = 0.5 * LANDING_YD * abs(bv)
        cu = min(max(cu, h), 1 - h); cv = min(max(cv, h), 1 - h)          # clamped inside the picture, as DriftStep
        plain_old = S.view(S.old, cu, cv, h, SW, SH)
        full_old = S.labels(plain_old, cu, cv, h)
        base = S.view(S.new, cu, cv, h, SW, SH)
        full_512 = S.detail(base, sat, 512, cu - h, cv - h, cu + h, cv + h)
        full_256 = S.detail(base, sat, 256, cu - h, cv - h, cu + h, cv + h)
        top = titled([full_old.resize((HW, HH), Image.LANCZOS), S.labels(full_512, cu, cv, h).resize((HW, HH), Image.LANCZOS)],
                     ['0.3.0  %s, landing (%d yd tall)' % (title, LANDING_YD), '0.4.0  overview + 512 px detail, names'])
        # a crop at his screen's own pixels, round the arrival (the screen's centre unless clamped)
        ax = (au + bu * y - (cu - h)) / (2 * h) * SW
        ay = (av + bv * x - (cv - h)) / (2 * h) * SH
        cw, ch = SW // 4, SH // 2
        bx = int(min(max(ax - cw / 2, 0), SW - cw)); by = int(min(max(ay - ch / 2, 0), SH - ch))
        crop = (bx, by, bx + cw, by + ch)
        full_new_only = base
        bottom = titled([plain_old.crop(crop), full_new_only.crop(crop), full_512.crop(crop), full_256.crop(crop)],
                        ['0.3.0, 1:1', '0.4.0 overview only, 1:1', '+ 512 px detail, 1:1', '+ 256 px detail, 1:1'],
                        size=34)
        save(stack([top, bottom]), stem, '%s, landing (%d yd tall) at %s %dx%d: top, the whole screen (halved): 0.3.0 '
             '| 0.4.0 with the 512 px detail and the names; bottom, a quarter of the screen at its own pixels round '
             'the arrival: 0.3.0 | the new overview alone | with 512 px detail | with 256 px detail (no names, so '
             'the ground shows)' % (title, LANDING_YD, shape, SW, SH))
    if master and (not only or 'coasts' in only):
        coasts(S, master, sat, save, shape)
    open(os.path.join(out, 'stills-%s.txt' % shape), 'w').write('\n'.join(notes) + '\n')


if __name__ == '__main__':
    main()
