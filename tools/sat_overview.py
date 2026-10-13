r"""The satellite's overview for CinderLoad 0.4.0 (13 Oct 2026): CinderMap.blp re-made from the 3D top-down render (his
pick: "the 3D top-down render of the whole world with half-size trees", broad daylight), at EXACTLY 0.3.0's layout,
so every overlay and map.txt line of 0.3.0 stays as it is.

For each screen shape:
  - the sea is 0.3.0's own picture for that shape (its lossless source from the kit's 8 Oct build when that is proven
    to be the shipped one, else the shipped CinderMap.blp decoded): the painted Great Sea, its
    depth shading, coast lines, compass roses and rhumb lines, and (inside the tiles the client's minimap drew) the
    minimap's own water, all exactly where 0.3.0 had them;
  - the land is the 3D master (tiles/half, 1024 px a client tile), each tile area-averaged to 128 px, then laid into
    its exact rectangle on the 4096 x 4096 picture by map.txt's own line (u = au + bu * y, v = av + bv * x), with a
    Lanczos filter sized to the squeeze (never point-sampled);
  - the two meet through satmask.py's land alpha (the world's heights against the ocean's surface: land and the
    render's own shallows 1, deep ocean 0, the coast a soft blend between), area-averaged to the picture the same way.
    The render's tile-grid edge is out in deep water, where the alpha is 0, so it never shows in the sea;
  - the tiles 0.3.0 left out are left out again: the client's hidden zone (GM Island, as stitch_minimap.py hid it).
Written as 0.3.0's were: tools/blp.py's write_blp, 4096 x 4096 DXT1, a full mip chain.

Usage: STORMLIB=/path/to/libstorm.so python3 sat_overview.py <master dir> <0.3.0 packs dir> <out dir> [shape ...]
         [--client <client Data dir>] [--work <scratch dir>]
  <master dir>    the 3D master (index.json, tiles/half/<Map>/<X>_<Y>.png)
  <0.3.0 packs>   the folder with 0.3.0's LoadingScreens-<shape>.MPQ (read only: CinderMap.blp and map.txt)
  --client        the client the 0.3.0 map was stitched from (its WorldMapArea for the hidden zone); default fogtest
  --work          where the 128-px mosaics are cached (about 400 MB); default ~/.cache/claude-scratch/sat040-assets
  --sea-src       the kit's 8 Oct build (w-<shape>/world-<shape>.png and CinderMap.blp): 0.3.0's pictures lossless,
                  used when proven to be the shipped ones; default inbox/loading-screens/living-map/repack-20261008
Writes <out>/world-<shape>.png (the new picture, 4096 x 4096), <out>/CinderMap-<shape>.blp, <out>/old-<shape>.png
(0.3.0's), <out>/sea-<shape>.png (0.3.0's softened under the blend: make_sat.py's sea) and <out>/alpha-<shape>.png (the
land alpha on the picture, for checking).
"""
import concurrent.futures as cf, ctypes, io, math, os, struct, sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import satgeo, satmask
from blp import write_blp, read_blp

Image.MAX_IMAGE_PIXELS = None
SIDE = 4096
P = 128                                  # mosaic pixels a client tile: 4.17 yd, finer than the picture's 7.2 yd texel
SHAPES = ('16x10', '16x9', '21x9', '32x9')
OLD_BLP = {}
SOFT_TEXELS = 2.0        # 0.3.0's picture softened by this (texels) under the render's blend, never where alpha is 0
SOFT_RAMP = 5.0          # fully softened from alpha 0.2 up
DEEP_FILL = (50, 82, 112)                # the render's own deep water, where it drew no tile (never shown: alpha 0)
CLIENT = os.path.expanduser('~/Games/RavenCraft-fogtest/Data')
WORK = os.path.expanduser('~/.cache/claude-scratch/sat040-assets')
SEA_SRC = os.path.expanduser('~/Desktop/RavenCraft-Dev-Archive/inbox/loading-screens/living-map/repack-20261008')


class Mpq:
    """Read-only files of one archive through StormLib (as make_pack.py's Client, for one archive)."""
    def __init__(self, path):
        self.L = L = ctypes.CDLL(os.environ.get('STORMLIB', 'libstorm.so'))
        L.SFileOpenArchive.argtypes = [ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p)]
        L.SFileOpenFileEx.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p)]
        L.SFileGetFileSize.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint)]
        L.SFileGetFileSize.restype = ctypes.c_uint
        L.SFileReadFile.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint),
                                    ctypes.c_void_p]
        L.SFileCloseFile.argtypes = [ctypes.c_void_p]
        L.SFileCloseArchive.argtypes = [ctypes.c_void_p]
        self.h = ctypes.c_void_p()
        if not L.SFileOpenArchive(path.encode(), 0, 0x100, ctypes.byref(self.h)):
            raise SystemExit('cannot open %s' % path)

    def read(self, name):
        f = ctypes.c_void_p()
        if not self.L.SFileOpenFileEx(self.h, name.encode(), 0, ctypes.byref(f)):
            return None
        hi = ctypes.c_uint(0)
        n = self.L.SFileGetFileSize(f, ctypes.byref(hi))
        buf = ctypes.create_string_buffer(max(1, n))
        got = ctypes.c_uint(0)
        ok = self.L.SFileReadFile(f, buf, n, ctypes.byref(got), None) or got.value == n
        self.L.SFileCloseFile(f)
        return buf.raw[:got.value] if ok and got.value == n else None

    def names(self):
        lst = self.read('(listfile)')
        return lst.decode('latin-1').split() if lst else []

    def close(self):
        self.L.SFileCloseArchive(self.h)


def hidden_tiles(data):
    """{c: set((X, Y))}: the tiles stitch_minimap.py left out of 0.3.0's map (GM Island, two tiles round its rectangle),
    by the same rule, from the client's own WorldMapArea.dbc and AreaTable.dbc."""
    from make_pack import Client
    client = Client(data)

    def dbc(name):
        d = client.read('DBFilesClient\\%s.dbc' % name)
        _, n, _, size, _ = struct.unpack('<4s4I', d[:20])
        s = d[20 + n * size:]
        return [d[20 + i * size:20 + (i + 1) * size] for i in range(n)], \
            (lambda o: s[o:s.index(b'\0', o)].decode('utf-8', 'replace'))
    rows, text = dbc('AreaTable')
    area = {struct.unpack('<25I', r)[0]: text(struct.unpack('<25I', r)[11]) for r in rows}
    rows, _ = dbc('WorldMapArea')
    out = {0: set(), 1: set()}
    Y = 533.3333                                      # stitch_minimap.py's YARDS, as it ran
    for r in rows:
        i = struct.unpack('<4I', r[:16])
        l, rr, t, b = struct.unpack('<4f', r[16:32])
        if i[1] in out and area.get(i[2]) == 'GM Island':
            for x in range(64):
                for y in range(64):
                    if 32 - l / Y - 2 <= x <= 32 - rr / Y + 2 and 32 - t / Y - 2 <= y <= 32 - b / Y + 2:
                        out[i[1]].add((x, y))
    return out


def _tile_job(job):
    master, c, X, Y, hidden, depth, shallow, deep = job
    if hidden:
        return c, X, Y, None, None
    im = np.asarray(Image.open(satgeo.master_path(master, c, X, Y)).convert('RGB'), np.float32)
    k = im.shape[0] // P
    rgb = im.reshape(P, k, P, k, 3).mean((1, 3))                      # the exact area average, 8 x 8 into 1
    a = satmask.land_alpha(depth, P * 4, shallow, deep)
    a = a.reshape(P, 4, P, 4).mean((1, 3))                            # the mask at 4x, then averaged: a soft coast
    return c, X, Y, np.clip(rgb + 0.5, 0, 255).astype(np.uint8), a.astype(np.float32)


def mosaics(master, hidden, work, log=print):
    """{c: (rgb uint8 [h,w,3], alpha float32 [h,w], X0, Y0)} at P px a tile, cached in work (rebuilt when the master's
    index.json or any tile is newer than the cache)."""
    tiles, _ = satgeo.master_tiles(master)
    newest = max(os.path.getmtime(os.path.join(master, 'index.json')),
                 max(os.path.getmtime(satgeo.master_path(master, c, X, Y)) for c, X, Y, _ in tiles))
    out = {}
    for c in (0, 1):
        cache = os.path.join(work, 'mosaic-%s-v%d-%d-%.0f-%g-%g.npz' % (satgeo.MAPS[c], satmask.VERSION, P, newest, satmask.SHALLOW_YD,
                                                                  satmask.DEEP_YD))
        if os.path.exists(cache):
            z = np.load(cache)
            out[c] = (z['rgb'], z['alpha'], int(z['X0']), int(z['Y0']))
            continue
        mine = [(X, Y) for cc, X, Y, _ in tiles if cc == c]
        X0, X1 = min(x for x, _ in mine), max(x for x, _ in mine)
        Y0, Y1 = min(y for _, y in mine), max(y for _, y in mine)
        rgb = np.empty(((Y1 - Y0 + 1) * P, (X1 - X0 + 1) * P, 3), np.uint8)
        rgb[:] = DEEP_FILL
        alpha = np.zeros(((Y1 - Y0 + 1) * P, (X1 - X0 + 1) * P), np.float32)
        cont = satmask.Continent(satgeo.MAPS[c], X0, X1, Y0, Y1, log=log)
        jobs = [(master, c, X, Y, (X, Y) in hidden[c], cont.tile_depth(X, Y).copy(), satmask.SHALLOW_YD, satmask.DEEP_YD)
                for X, Y in mine]
        del cont
        with cf.ProcessPoolExecutor(max_workers=min(16, os.cpu_count() or 4),
                                    mp_context=__import__('multiprocessing').get_context('fork')) as ex:
            for _, X, Y, t, a in ex.map(_tile_job, jobs, chunksize=8):
                if t is None:
                    continue
                r0, c0 = (Y - Y0) * P, (X - X0) * P
                rgb[r0:r0 + P, c0:c0 + P] = t
                alpha[r0:r0 + P, c0:c0 + P] = a
        np.savez(cache, rgb=rgb, alpha=alpha, X0=X0, Y0=Y0)
        log('  %s: %d tiles (%d hidden) into a %d x %d mosaic' % (satgeo.MAPS[c], len(mine),
                                                                   sum(1 for j in jobs if j[4]), rgb.shape[1], rgb.shape[0]))
        out[c] = (rgb, alpha, X0, Y0)
    return out


def edge_report(master, hidden, mos):
    """Every tile edge where the land alpha breaks off AT the edge: a straight line the composite would show. A pixel of
    the edge counts when the alpha jumps across the edge by more than 0.4 while on each side it changes by less than
    0.1 between the edge pixel and the one inside (a coast or a cliff that only happens to cross the edge changes on one
    side too, and is not counted). A tile the render never drew, or 0.3.0 hid, is alpha 0. [(pixels, worst jump, c, X,
    Y, dx, dy)], the longest first. (The render's own picture is continuous across tiles: each region was drawn with its
    neighbours loaded round it. The alpha is not, where the world's data is not.)"""
    tiles, _ = satgeo.master_tiles(master)
    drawn = {(c, X, Y) for c, X, Y, _ in tiles if (X, Y) not in hidden[c]}
    zero2 = np.zeros((P, 2), np.float32)

    def edge(c, X, Y, dx, dy):
        """The two pixel columns (or rows) nearest the side (dx, dy) of tile (X, Y), outermost first: [P, 2]."""
        if (c, X, Y) not in drawn:
            return zero2
        rgb, alpha, X0, Y0 = mos[c]
        a = alpha[(Y - Y0) * P:(Y - Y0 + 1) * P, (X - X0) * P:(X - X0 + 1) * P]
        return {(1, 0): a[:, [-1, -2]], (-1, 0): a[:, [0, 1]], (0, 1): a[[-1, -2]].T, (0, -1): a[[0, 1]].T}[dx, dy]
    out = []
    for c, X, Y in drawn:
        for dx, dy in ((1, 0), (0, 1), (-1, 0), (0, -1)):
            if (c, X + dx, Y + dy) in drawn and (dx, dy) in ((-1, 0), (0, -1)):
                continue                                   # a drawn pair is checked once, from its west or north tile
            m, t = edge(c, X, Y, dx, dy), edge(c, X + dx, Y + dy, -dx, -dy)
            jump = np.abs(m[:, 0] - t[:, 0])
            bad = (jump > 0.4) & (np.abs(m[:, 0] - m[:, 1]) < 0.1) & (np.abs(t[:, 0] - t[:, 1]) < 0.1)
            if bad.sum() >= 3:
                out.append((int(bad.sum()), float(jump[bad].max()), c, X, Y, dx, dy))
    return sorted(out, reverse=True)


def place(mos_img, table, c, X0, Y0, n_tiles_x, n_tiles_y, resample):
    """The mosaic (a PIL image, P px a tile, tile (X0, Y0) at its top left) resampled into its rectangle on the
    SIDE x SIDE picture: (image, (left, top)) covering the whole texels inside the rectangle."""
    uw, vn, _, _ = satgeo.tile_rect_uv(table, c, X0, Y0)
    _, _, ue, vs = satgeo.tile_rect_uv(table, c, X0 + n_tiles_x - 1, Y0 + n_tiles_y - 1)
    ix0, ix1 = max(0, math.ceil(uw * SIDE)), min(SIDE, math.floor(ue * SIDE))     # whole texels, on the picture
    iy0, iy1 = max(0, math.ceil(vn * SIDE)), min(SIDE, math.floor(vs * SIDE))
    W, H = mos_img.size
    sx = lambda i: (i / SIDE - uw) / (ue - uw) * W
    sy = lambda j: (j / SIDE - vn) / (vs - vn) * H
    box = (sx(ix0), sy(iy0), sx(ix1), sy(iy1))
    return mos_img.resize((ix1 - ix0, iy1 - iy0), resample, box=box), (ix0, iy0)


def old_picture(shape, old_blp, sea_src, log=print):
    """0.3.0's picture for the shape: its lossless source when the kit's build of 8 Oct is on this machine and proven
    to be the shipped one (its CinderMap.blp byte for byte the pack's; write_blp re-encodes that PNG to the same bytes,
    checked 13 Oct), so every 4 x 4 block of open sea encodes to 0.3.0's own bytes again; else the shipped BLP decoded."""
    import hashlib
    info, decode = read_blp(old_blp)
    shipped = decode(0)[..., :3]
    d = os.path.join(sea_src, 'w-%s' % shape) if sea_src else None
    if d and os.path.exists(os.path.join(d, 'CinderMap.blp')) and os.path.exists(os.path.join(d, 'world-%s.png' % shape)):
        if hashlib.sha256(open(os.path.join(d, 'CinderMap.blp'), 'rb').read()).digest() == hashlib.sha256(old_blp).digest():
            png = np.asarray(Image.open(os.path.join(d, 'world-%s.png' % shape)).convert('RGB'))
            err = np.abs(png.astype(np.int16) - shipped).mean()
            if png.shape == shipped.shape and err < 3:
                log('  %s: the sea from 0.3.0\'s lossless source %s (the shipped BLP is its encoding; mean DXT1 '
                    'difference %.2f)' % (shape, os.path.join(d, 'world-%s.png' % shape), err))
                return info, png
        log('  %s: %s is not the shipped picture\'s source; the shipped BLP decoded instead' % (shape, d))
    return info, shipped


def build_shape(shape, pack_dir, mos, out, log=print, sea_src=None):
    pk = Mpq(os.path.join(pack_dir, 'LoadingScreens-%s.MPQ' % shape))
    old_blp = pk.read('Interface\\Glues\\LoadingScreens\\CinderMap.blp')
    table, _ = satgeo.read_map_txt(pk.read('CinderLoad\\map.txt').decode('ascii'))
    pk.close()
    info, old = old_picture(shape, old_blp, sea_src, log)
    OLD_BLP[shape] = old_blp
    if old.shape[:2] != (SIDE, SIDE):
        raise SystemExit('%s: 0.3.0 CinderMap.blp is %s' % (shape, old.shape))
    new = np.zeros((SIDE, SIDE, 3), np.float32)
    alpha = np.zeros((SIDE, SIDE), np.float32)
    for c in (0, 1):
        rgb, a, X0, Y0 = mos[c]
        nx, ny = rgb.shape[1] // P, rgb.shape[0] // P
        im, (l, t) = place(Image.fromarray(rgb), table, c, X0, Y0, nx, ny, Image.LANCZOS)
        am, _ = place(Image.fromarray(a, 'F'), table, c, X0, Y0, nx, ny, Image.BOX)
        am = np.clip(np.asarray(am, np.float32), 0, 1)
        # a texel takes the continent with the most land there (the continents' rectangles overlap in open sea)
        sl = (slice(t, t + am.shape[0]), slice(l, l + am.shape[1]))
        take = am > alpha[sl]
        alpha[sl] = np.where(take, am, alpha[sl])
        new[sl] = np.where(take[..., None], np.asarray(im, np.float32), new[sl])
    # where the render comes in (alpha > 0), 0.3.0's picture under it is softened first: there it is mostly the
    # minimap's own coastal water, drawn in 17-yard texels and DXT blocks, and blending through it showed its staircase
    # at the landing (Durotar's coast, 13 Oct). Where the alpha is 0 it is untouched, to the byte.
    from scipy import ndimage
    g = np.clip(alpha * SOFT_RAMP, 0, 1)[..., None]
    soft = np.stack([ndimage.gaussian_filter(old[..., k].astype(np.float32), SOFT_TEXELS) for k in range(3)], -1)
    sea = old.astype(np.float32) * (1 - g) + soft * g
    del soft, g
    pic = sea * (1 - alpha[..., None]) + new * alpha[..., None]
    pic = np.clip(pic + 0.5, 0, 255).astype(np.uint8)
    os.makedirs(out, exist_ok=True)
    Image.fromarray(np.clip(sea + 0.5, 0, 255).astype(np.uint8)).save(os.path.join(out, 'sea-%s.png' % shape))
    del sea
    png = os.path.join(out, 'world-%s.png' % shape)
    Image.fromarray(pic).save(png)
    Image.fromarray(old).save(os.path.join(out, 'old-%s.png' % shape))
    Image.fromarray(np.clip(alpha * 255 + 0.5, 0, 255).astype(np.uint8)).save(os.path.join(out, 'alpha-%s.png' % shape))
    blp = os.path.join(out, 'CinderMap-%s.blp' % shape)
    levels, size = write_blp(png, blp, SIDE, SIDE)
    # the check: it decodes, at 4096 square with the full chain, as 0.3.0's did
    info2, decode2 = read_blp(open(blp, 'rb').read())
    if (info2['w'], info2['h'], info2['levels'], info2['dxt']) != (info['w'], info['h'], info['levels'], info['dxt']):
        raise SystemExit('%s: the new CinderMap.blp is %s, 0.3.0\'s was %s' % (shape, info2, info))
    back = decode2(0)[..., :3].astype(np.int16)
    err = np.abs(back - pic.astype(np.int16)).mean()
    # the open sea to the byte: level 0's 4 x 4 blocks against 0.3.0's, wherever the picture did not change
    newb = np.frombuffer(open(blp, 'rb').read(), np.uint8, SIDE * SIDE // 2, info2['offs'][0]).reshape(-1, 8)
    oldb = np.frombuffer(OLD_BLP[shape], np.uint8, SIDE * SIDE // 2, info['offs'][0]).reshape(-1, 8)
    same_px = (pic == old).all(-1).reshape(SIDE // 4, 4, SIDE // 4, 4).all((1, 3)).reshape(-1)
    same_b = (newb == oldb).all(1)
    log('  %s: of the %d blocks whose pixels are 0.3.0\'s, %d encode to 0.3.0\'s own bytes (%d blocks changed)' % (
        shape, int(same_px.sum()), int((same_b & same_px).sum()), int((~same_px).sum())))
    log('  %s: CinderMap.blp %d bytes, %d levels, decodes (mean DXT1 error %.2f levels); land on %.1f%% of the picture'
        % (shape, size, levels, err, 100 * (alpha > 0.5).mean()))
    return png, blp


if __name__ == '__main__':
    args = [a for a in sys.argv[1:]]
    opt = lambda k, d: (args.pop(args.index(k) + 1), args.remove(k))[0] if k in args else d
    client, work = opt('--client', CLIENT), opt('--work', WORK)
    sea_src = opt('--sea-src', SEA_SRC)
    master, packs, out = (os.path.expanduser(a) for a in args[:3])
    shapes = args[3:] or list(SHAPES)
    os.makedirs(work, exist_ok=True)
    hidden = hidden_tiles(client)
    print('hidden as 0.3.0 hid them:', {satgeo.MAPS[c]: len(v) for c, v in hidden.items()})
    mos = mosaics(master, hidden, work)
    worst = edge_report(master, hidden, mos)
    print('tile edges where the land alpha breaks off at the edge: %d' % len(worst))
    for n, j, c, X, Y, dx, dy in worst[:16]:
        print('  %3d px (jump %.2f) at %s %d_%d toward %+d,%+d' % (n, j, satgeo.MAPS[c], X, Y, dx, dy))
    for s in shapes:
        build_shape(s, packs, mos, out, sea_src=sea_src)
