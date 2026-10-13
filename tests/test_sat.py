r"""Tests of the satellite's assets for CinderLoad 0.4.0 (tools/satgeo.py, satmask.py, make_sat.py, sat_overview.py):
the contract's tile maths against a known place in the game's own data, the axes against map.txt and against the
pictures themselves, and the built files against the contract.

  python3 tests/test_sat.py [<0.3.0 packs dir> [<built sat dir> [<built overview dir>]]]
Defaults: the inbox's 0.3.0 assets, ~/.cache/claude-scratch/sat040-assets/sat and .../ov. A part whose inputs are not
on this machine is skipped and says so. Exit 0 when every check that ran passed.
"""
import glob, os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
import satgeo

INBOX = os.path.expanduser('~/Desktop/RavenCraft-Dev-Archive/inbox')
PACKS = sys.argv[1] if len(sys.argv) > 1 else os.path.join(INBOX, 'cinderload-release-0.3.0-tonight', 'assets')
SAT = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser('~/.cache/claude-scratch/sat040-assets/sat')
OV = sys.argv[3] if len(sys.argv) > 3 else os.path.expanduser('~/.cache/claude-scratch/sat040-assets/ov')
W3D = os.path.join(INBOX, 'world3d', 'data')
MASTER = os.path.join(INBOX, 'azeroth-topdown-2026-10-13')
checks = fails = 0


def check(ok, what):
    global checks, fails
    checks += 1
    if not ok:
        fails += 1
        print('FAIL:', what)


def skip(what):
    print('skipped:', what)


# Goldshire's inn, the Lion's Pride: where the client's own map tile Azeroth 31_49 places its building
# (world\wmo\azeroth\buildings\goldshireinn\goldshireinn.wmo, placement 17042.268, 56.526, 26530.914 in w3d's 31_49.npz)
INN_X, INN_Y = satgeo.ORIGIN - 26530.914, satgeo.ORIGIN - 17042.268        # -9464.25 north, 24.40 west


def test_contract_maths():
    X, Y, u, v = satgeo.tile_of(INN_X, INN_Y)
    check((X, Y) == (31, 49), 'the inn is in tile 31_49 (got %d_%d)' % (X, Y))
    check(0.95 < u < 0.96 and 0.74 < v < 0.75, 'the inn sits at u %.4f v %.4f of its tile (east edge, lower part)' % (u, v))
    for px in (256, 512, 1024):
        X2, Y2, c, r = satgeo.pixel_of(INN_X, INN_Y, px)
        cx, cy = satgeo.pixel_centre(X2, Y2, c, r, px)
        half = satgeo.TILE / px / 2 + 1e-6
        check((X2, Y2) == (31, 49) and abs(cx - INN_X) <= half and abs(cy - INN_Y) <= half,
              'px %d: the inn -> pixel %d,%d -> back within half a pixel' % (px, c, r))
        check(satgeo.pixel_of(cx, cy, px) == (X2, Y2, c, r), 'px %d: a pixel centre maps to its own pixel' % px)
    # the contract's formulas, spelt out as the DLL will spell them
    T, O = 533.33333, 32 * 533.33333
    import math
    check((math.floor((O - INN_Y) / T), math.floor((O - INN_X) / T)) == (31, 49), 'the contract formula as written')
    # corners: a tile's north-west corner is its own (u, v = 0, 0), the next tile starts where it ends
    for X, Y in ((0, 0), (31, 49), (63, 63)):
        x, y = satgeo.world_of(X, Y, 0.0, 0.0)
        check(satgeo.tile_of(x - 1e-3, y - 1e-3)[:2] == (X, Y), 'tile %d_%d owns its north-west corner' % (X, Y))
        x, y = satgeo.world_of(X, Y, 1.0, 1.0)
        check(satgeo.tile_of(x - 1e-3, y - 1e-3)[:2] == (X + 1, Y + 1), 'tile %d_%d ends where %d_%d starts' % (
            X, Y, X + 1, Y + 1))
    # east is +u, south is +v: one yard west (y + 1) is a smaller u, one yard north (x + 1) is a smaller v
    _, _, u1, v1 = satgeo.tile_of(INN_X, INN_Y + 1)
    _, _, u2, v2 = satgeo.tile_of(INN_X + 1, INN_Y)
    check(u1 < u and v1 == v and v2 < v and u2 == u, 'west lowers u, north lowers v')


def test_w3d_naming():
    """The client's own data names the tile the same way: the inn's building is in w3d's 31_49 (and w3d's grid is the
    master's and the client's), so the contract, the master and the world agree."""
    p = os.path.join(W3D, 'Azeroth', 'tiles', '31_49.npz')
    if not os.path.exists(p):
        return skip('w3d data (%s)' % p)
    import numpy as np
    with np.load(p) as z:
        names = [str(n).lower() for n in z['wnames']]
        rows = z['wmos']
    inn = [r for r in rows if 'goldshireinn' in names[int(r[0])]]
    check(len(inn) == 1, 'w3d 31_49 places one Goldshire inn')
    if inn:
        a, b, c = inn[0][1:4]
        check(abs((satgeo.ORIGIN - c) - INN_X) < 0.01 and abs((satgeo.ORIGIN - a) - INN_Y) < 0.01, 'the inn as placed')
        check(int(a // satgeo.TILE) == 31 and int(c // satgeo.TILE) == 49, 'w3d: X = placement x // TILE, Y = z // TILE')


def test_map_txt_agrees():
    """The detail tile's rectangle on the overview (through map.txt) holds the inn at the same u, v as map.txt puts it."""
    from sat_overview import Mpq
    for shape in ('16x10', '16x9', '21x9', '32x9'):
        p = os.path.join(PACKS, 'LoadingScreens-%s.MPQ' % shape)
        if not os.path.exists(p) or 'STORMLIB' not in os.environ:
            return skip('0.3.0 packs or STORMLIB')
        a = Mpq(p)
        table, other = satgeo.read_map_txt(a.read('CinderLoad\\map.txt').decode('ascii'))
        a.close()
        X, Y, u, v = satgeo.tile_of(INN_X, INN_Y)
        uw, vn, ue, vs = satgeo.tile_rect_uv(table, 0, X, Y)
        U, V = satgeo.overview_uv(table, 0, INN_X, INN_Y)
        check(abs(uw + (ue - uw) * u - U) < 1e-6 and abs(vn + (vs - vn) * v - V) < 1e-6,
              '%s: the inn through its tile = the inn through map.txt' % shape)
        check(ue > uw and vs > vn, '%s: on the overview a tile runs east = +u and south = +v' % shape)
        # Goldshire is in the Eastern Kingdoms' half of the picture, a little below the middle
        check(0.5 < U < 1.0 and 0.5 < V < 0.8, '%s: the inn at u %.3f v %.3f' % (shape, U, V))
        check(other.get('end') == [['3500']], '%s: map.txt keeps end 3500' % shape)


def test_alignment_on_the_pictures():
    """The axes on real pixels: the new overview's land (its alpha, from the world's heights and the contract's tile
    maths) lies on 0.3.0's land (the client's minimap, placed by map.txt) with no shift. A swapped axis, a flipped tile
    order or an off-by-one tile would move it 30 to 70 texels."""
    import numpy as np
    from PIL import Image
    Image.MAX_IMAGE_PIXELS = None
    for shape in ('16x10', '16x9', '21x9', '32x9'):
        pa, po = os.path.join(OV, 'alpha-%s.png' % shape), os.path.join(OV, 'old-%s.png' % shape)
        if not (os.path.exists(pa) and os.path.exists(po)):
            skip('overview %s not built in %s' % (shape, OV))
            continue
        al = np.asarray(Image.open(pa).reduce(4), np.float32) / 255
        old = np.asarray(Image.open(po).reduce(4), np.float32)
        land = ((old[..., 0] > old[..., 2] + 4) | (old.mean(-1) > 110)).astype(np.float32)   # brown, green, snow
        best, at = -1.0, None
        for dy in range(-6, 7):
            for dx in range(-6, 7):
                s = (np.roll(np.roll(al, dy, 0), dx, 1) * land).sum()
                if s > best:
                    best, at = s, (dx, dy)
        check(at == (0, 0), '%s: the new land lies on the old with no shift (best at %s, in 4-texel steps)' % (shape, at))
        ov = (al > 0.5).astype(np.float32)
        agree = (ov * land).sum() / max(1.0, ov.sum())
        check(agree > 0.85, '%s: %.1f%% of the new land is land in 0.3.0' % (shape, 100 * agree))


def test_built_archive():
    from blp import read_blp
    for px in (512, 256):
        d = os.path.join(SAT, str(px))
        idx = os.path.join(d, 'sat.txt')
        if not os.path.exists(idx):
            skip('no build at %s' % d)
            continue
        raw = open(idx, 'rb').read()
        check(raw.isascii() and b'\r' not in raw and not raw.endswith(b'\n\n'), '%d: sat.txt is ASCII, LF only' % px)
        lines = raw.decode().split('\n')[:-1]
        body = [l for l in lines if not l.startswith('#')]
        check(all(l == l.strip() and l for l in lines), '%d: no blank lines, no trailing spaces' % px)
        check(body[0] == 'px %d' % px and body[1] == 'levels %d' % (8 if px == 512 else 7), '%d: px and levels' % px)
        ts = [tuple(int(w) for w in l.split()[1:]) for l in body[2:]]
        check(all(l.startswith('t ') and len(l.split()) == 4 for l in body[2:]), '%d: every other line is a t line' % px)
        check(ts == sorted(ts, key=lambda k: (k[0], k[2], k[1])) and len(set(ts)) == len(ts), '%d: sorted, unique' % px)
        check(all(c in (0, 1) and 0 <= X < 64 and 0 <= Y < 64 for c, X, Y in ts), '%d: ids and indices in range' % px)
        check((0, 31, 49) in ts and (1, 1, 1) not in ts, '%d: Goldshire present, GM Island absent' % px)
        tiles = glob.glob(os.path.join(d, 'tiles', '*.blp'))
        check(len(tiles) == len(ts), '%d: one BLP per t line (%d, %d)' % (px, len(tiles), len(ts)))
        b = open(os.path.join(d, 'tiles', '0_31_49.blp'), 'rb').read()
        info, dec = read_blp(b)
        check(b[:4] == b'BLP2' and struct.unpack_from('<I', b, 4)[0] == 1 and b[8] == 2 and b[9] == 0 and b[10] == 0
              and b[11] == 1, '%d: BLP2, DXT, no alpha, DXT1, has mips' % px)
        check((info['w'], info['h'], info['levels']) == (px, px, 8 if px == 512 else 7), '%d: size and levels' % px)
        offs = struct.unpack_from('<16I', b, 20)
        lens = struct.unpack_from('<16I', b, 84)
        n = info['levels']
        check(all(o == 0 for o in offs[n:]) and all(l == 0 for l in lens[n:]), '%d: no slot past 4 x 4' % px)
        check(offs[n - 1] + lens[n - 1] == len(b) and lens[n - 1] == 8, '%d: the last level is one 4 x 4 block' % px)
        # the inn's pixel in the tile is a building, not grass: its roof (the master at the same place, for scale)
        import numpy as np
        X, Y, c, r = satgeo.pixel_of(INN_X, INN_Y, px)
        img = dec(0)[..., :3].astype(np.int16)
        mp = os.path.join(MASTER, 'tiles', 'half', 'Azeroth', '31_49.png')
        if os.path.exists(mp):
            from PIL import Image
            m = np.asarray(Image.open(mp).convert('RGB').resize((px, px), Image.BOX), np.int16)
            diff = np.abs(img - m).mean()
            check(diff < 6, '%d: the tile is the master on land (mean difference %.2f)' % (px, diff))


if __name__ == '__main__':
    test_contract_maths()
    test_w3d_naming()
    test_map_txt_agrees()
    test_alignment_on_the_pictures()
    test_built_archive()
    print('%d checks, %d failed' % (checks, fails))
    sys.exit(1 if fails else 0)
