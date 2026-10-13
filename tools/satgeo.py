r"""The satellite's geometry and files, shared by make_sat.py (the detail tiles), sat_overview.py (the new CinderMap
picture) and tests/test_sat.py. CinderLoad 0.4.0 (13 Oct 2026).

The client's tile grid (fixed by the game; the DLL hard-codes the same numbers, ASSETS.md "Contract for the DLL"):
  TILE = 533.33333 yards a side, ORIGIN = 32 * TILE.
  For a world position (x north, y west, in yards, as the game gives it):
    X = floor((ORIGIN - y) / TILE)   the tile's column, west to east
    Y = floor((ORIGIN - x) / TILE)   the tile's row, north to south
    u = (ORIGIN - y) / TILE - X      inside the tile, 0 at its west edge .. 1 at its east edge
    v = (ORIGIN - x) / TILE - Y      inside the tile, 0 at its north edge .. 1 at its south edge
  This is the 3D master's own naming (tiles/<variant>/<Map>/<X>_<Y>.png; its index.json: pixel (c, r) of tile (X, Y)
  lies at placement x = (X + (c + .5)/px) * TILE east, z = (Y + (r + .5)/px) * TILE south, world_x = ORIGIN - z,
  world_y = ORIGIN - x) and the client's own ADT and minimap file names (Azeroth\map<X>_<Y>.blp).

The overview (CinderMap.blp) is placed by CinderLoad\map.txt, one line per continent:
  map <c> <au> <bu> <av> <bv>      u = au + bu * y, v = av + bv * x      (float bits in hex)
  with u, v over the whole picture (0..1, u east, v south), the picture's texel (i, j) covering u in [i/N, (i+1)/N).
Continent ids are map.txt's: 0 = the Eastern Kingdoms (the client's map Azeroth), 1 = Kalimdor.
"""
import json, math, os, struct

TILE = 533.33333
ORIGIN = 32 * TILE
MAPS = {0: 'Azeroth', 1: 'Kalimdor'}           # map.txt's continent id: the client's map folder
IDS = {v: k for k, v in MAPS.items()}


def tile_of(x, y):
    """(X, Y, u, v) of a world position (x north, y west): the contract's maths, exactly."""
    fx, fy = (ORIGIN - y) / TILE, (ORIGIN - x) / TILE
    X, Y = math.floor(fx), math.floor(fy)
    return X, Y, fx - X, fy - Y


def world_of(X, Y, u, v):
    """The world position (x north, y west) at (u, v) inside tile (X, Y); the inverse of tile_of."""
    return ORIGIN - (Y + v) * TILE, ORIGIN - (X + u) * TILE


def pixel_of(x, y, px):
    """(X, Y, c, r): the tile and the pixel (column from the west edge, row from the north edge) of a px-wide tile
    that holds the world position."""
    X, Y, u, v = tile_of(x, y)
    return X, Y, min(px - 1, int(u * px)), min(px - 1, int(v * px))


def pixel_centre(X, Y, c, r, px):
    """The world position (x, y) at the centre of pixel (c, r) of tile (X, Y) at px pixels a side."""
    return world_of(X, Y, (c + 0.5) / px, (r + 0.5) / px)


def unhex(h):
    return struct.unpack('<f', struct.pack('<I', int(h, 16)))[0]


def hexf(f):
    return '%08x' % struct.unpack('<I', struct.pack('<f', f))[0]


def read_map_txt(path_or_text):
    """{c: (au, bu, av, bv)} and the other keyword lines ({'end': '3500', ...}) of a map.txt."""
    text = path_or_text if '\n' in path_or_text else open(path_or_text, encoding='ascii').read()
    maps, other = {}, {}
    for line in text.splitlines():
        w = line.split('#', 1)[0].split()
        if not w:
            continue
        if w[0] == 'map' and len(w) == 6:
            maps[int(w[1])] = tuple(unhex(h) for h in w[2:6])
        else:
            other.setdefault(w[0], []).append(w[1:])
    return maps, other


def overview_uv(table, c, x, y):
    """The world position's u, v on the overview picture, by map.txt's line for continent c."""
    au, bu, av, bv = table[c]
    return au + bu * y, av + bv * x


def tile_rect_uv(table, c, X, Y):
    """Tile (X, Y)'s rectangle on the overview: (u west, v north, u east, v south)."""
    yw, ye = ORIGIN - X * TILE, ORIGIN - (X + 1) * TILE
    xn, xs = ORIGIN - Y * TILE, ORIGIN - (Y + 1) * TILE
    uw, vn = overview_uv(table, c, xn, yw)
    ue, vs = overview_uv(table, c, xs, ye)
    return uw, vn, ue, vs


def master_tiles(master):
    """[(c, X, Y, land)] of the 3D master's index.json, checked against its own numbers."""
    d = json.load(open(os.path.join(master, 'index.json')))
    if abs(d['tile_yards'] - TILE) > 1e-4 or abs(d['origin'] - ORIGIN) > 1e-3:
        raise SystemExit('the master is on another grid: tile %r origin %r' % (d['tile_yards'], d['origin']))
    return [(IDS[t['map']], t['x'], t['y'], bool(t['land'])) for t in d['tiles']], d


def master_path(master, c, X, Y, variant='half'):
    return os.path.join(master, 'tiles', variant, MAPS[c], '%d_%d.png' % (X, Y))
