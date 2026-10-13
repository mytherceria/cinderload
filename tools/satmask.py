r"""Where the 3D render shows and where 0.3.0's painted sea shows: the land/sea mask of the satellite's new pictures
(CinderLoad 0.4.0, 13 Oct 2026), worked out from the world's own data, never from the render's pixels.

Why the world data and not the picture: the render draws the ocean in its own colours and stops at the edge of the
tiles it was given, so the open sea must come from 0.3.0's painted sea and the coast needs a line to blend along. The
render's water is drawn see-through over the sea floor (its shader: alpha 1 - exp(-depth / 4), colour shallow to deep
over 30 yards), so on its pixels the shallows, wet sand and beach run into one another; the heights know exactly where
the ocean is and how deep. (The stills that decided it: ASSETS.md, "The mask".)

Source: the world3d export the render itself read (w3d format, ~/Desktop/RavenCraft-Dev-Archive/inbox/world3d/data;
reader ~/Projects/cindermap/tools/world3d/w3d.py), one .npz per client tile:
  outer [129,129] heights at the cell corners, water [129,129] the liquid's surface (NaN none), wmask [128,128] the
  cells the liquid is drawn in, wtype [16,16] per chunk (2 = ocean; 1 water/river, 3 magma, 4 slime).

The rule, per cell corner (4.17 yards apart) over a whole continent at once, then bilinear to each pixel's centre:
  - SEA is water whose surface is at the sea's level (within LEVEL_YD) and which joins, corner to corner through such
    water, the client's ocean liquid (wtype 2) or a tile the world has no file for (open sea, as the render drew it).
    The client types some coastal water as plain water, not ocean, chunk by chunk (Feralas's coast at Kalimdor 25_36 and
    26_36): by type alone the coast showed the chunk grid as straight lines. A lake is never sea, even at the sea's
    level, unless water joins it to the ocean; a river is sea only where its surface comes down to the sea's level,
    blending over LEVEL_YD to 2 x LEVEL_YD above it.
  - The sea's depth is its surface minus the ground. Ground the client leaves dry below the sea's level in an ocean
    chunk (the sea floor showing as a tan stripe in a few world-edge tiles, Kalimdor 46_20 and its kin: the
    coordinator's note, 13 Oct) is sea at that depth; ground at exactly 0.0 in an ocean chunk, wet or dry (the client's
    unset ground: whole chunk strips in the world-edge ocean tiles, Kalimdor 44_36 to 44_40, 46_20, 47_20, Azeroth
    25_36, 26_36) is deep sea; and dry ground below FLOOR_YD is the open ocean's floor whatever its chunk's type (a
    chunk with no liquid at all at -515 in Azeroth 44_35 showed as a dark square in the sea), as is any liquid over
    such ground (four chunks of plain water at -23 and -11 over the floor in Kalimdor 26_54).
  - Everything else is LAND, lakes, rivers, magma and slime included (they stay the render's own). Dry ground above
    FLOOR_YD never counts as sea for being low (Un'Goro's floor is 311 yards below the sea, Thousand Needles 93).
  land alpha = 1 - smoothstep(SHALLOW_YD, DEEP_YD, depth): 1 on land and in the render's own shallows, 0 where the sea
  is deep; land counts as depth (sea level - ground), never above 0, so the coast lies where the heights put it.

PLACEHOLDER (his call): SHALLOW_YD and DEEP_YD, how far out the render's own shallows run before 0.3.0's sea takes over.
"""
import os
import numpy as np

W3D = os.path.expanduser('~/Desktop/RavenCraft-Dev-Archive/inbox/world3d/data')
SHALLOW_YD = 3.0     # PLACEHOLDER (his call): the render's water is all its own up to this depth (yards) ...
DEEP_YD = 24.0       # PLACEHOLDER (his call): ... and all 0.3.0's painted sea from this depth on
LEVEL_YD = 1.0       # water within this of the sea's level is sea (if it joins the ocean); blended out by twice this
OCEAN = 2            # wtype: the client's ocean liquid
LAND_D = -50.0       # the depth land counts as, far from the sea (enough to be alpha 1 whatever the placeholders)
FLOOR_YD = -400.0    # dry ground below this is the open ocean's floor (the client's empty ocean tiles lie at -515;
                     # the lowest land is Un'Goro's floor at -311)
VERSION = 5          # the rule's version: caches made by an older rule are not used


def tile_file(c_map, X, Y, root=W3D):
    return os.path.join(root, c_map, 'tiles', '%d_%d.npz' % (X, Y))


def water_surface(W, WM):
    """The render's own rule (cindermap-world tools/world3d/pack3d.py water_surface, copied so this needs no import from
    another repo): water height per corner, -1e9 where none; a corner is wet when a drawn liquid cell touches it, and a
    wet corner without a sane height takes its neighbours'."""
    wet = np.zeros(W.shape, bool)
    m = WM.astype(bool)
    wet[:-1, :-1] |= m; wet[1:, :-1] |= m; wet[:-1, 1:] |= m; wet[1:, 1:] |= m
    ok = ~np.isnan(W) & (np.abs(W) < 3000)
    v = np.where(ok & wet, W, np.nan)
    for _ in range(3):
        bad = wet & np.isnan(v)
        if not bad.any():
            break
        p = np.pad(v, 1, constant_values=np.nan)
        nb = np.stack([p[:-2, 1:-1], p[2:, 1:-1], p[1:-1, :-2], p[1:-1, 2:]])
        cnt = (~np.isnan(nb)).sum(0)
        mean = np.where(cnt > 0, np.nansum(nb, 0) / np.maximum(cnt, 1), np.nan)
        v = np.where(bad, mean, v)
    return np.where(np.isnan(v), -1e9, v).astype(np.float32)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


class Continent:
    """The depth field of one continent's tiles X0..X1, Y0..Y1 at every cell corner: self.d [rows, cols] float32, row
    Y*128 + r and column X*128 + c relative to (Y0, X0). Built once (about 1 GB at its peak for Kalimdor), then cut per
    tile by tile_alpha()."""

    def __init__(self, c_map, X0, X1, Y0, Y1, root=W3D, log=None):
        from scipy import ndimage
        self.X0, self.Y0 = X0, Y0
        nr, nc = (Y1 - Y0 + 1) * 128 + 1, (X1 - X0 + 1) * 128 + 1
        H = np.full((nr, nc), -400.0, np.float32)            # no file: the render's open sea (ground -400, water 0)
        Wv = np.full((nr, nc), 0.0, np.float32)
        oc = np.zeros((nr, nc), bool)
        open_sea = np.ones((nr, nc), bool)                    # corners of tiles with no file
        have = []
        for Y in range(Y0, Y1 + 1):
            for X in range(X0, X1 + 1):
                p = tile_file(c_map, X, Y, root)
                if not os.path.exists(p):
                    continue
                have.append((X, Y))
                r, c = (Y - Y0) * 128, (X - X0) * 128
                with np.load(p) as z:
                    h, W, WM, wt = z['outer'].astype(np.float32), z['water'], z['wmask'], z['wtype']
                w = water_surface(W, WM)
                big = np.repeat(np.repeat(wt == OCEAN, 8, 0), 8, 1)
                o = np.zeros((129, 129), bool)
                o[:-1, :-1] |= big; o[1:, :-1] |= big; o[:-1, 1:] |= big; o[1:, 1:] |= big
                # a corner on a shared edge: the tile with a file wins over open sea; between two files the later
                # one's corner stands (their heights agree but for the client's own seams)
                H[r:r + 129, c:c + 129] = h
                Wv[r:r + 129, c:c + 129] = w
                oc[r:r + 129, c:c + 129] = o
                open_sea[r:r + 129, c:c + 129] = False
        wet = (Wv > -1e8) | open_sea
        ocean_w = wet & oc
        level = float(np.median(Wv[ocean_w])) if ocean_w.any() else 0.0
        near = wet & (np.abs(Wv - level) < 2 * LEVEL_YD)
        lab, n = ndimage.label(near)                          # 4-connected, corner to corner
        seed = np.unique(lab[(ocean_w | open_sea) & near])
        sea = np.isin(lab, seed[seed > 0])
        del lab
        dry = ~wet
        d = np.minimum(level - H, 0.0)                        # land: how far above the sea, never sea
        d = np.maximum(d, LAND_D)
        t = smoothstep(LEVEL_YD, 2 * LEVEL_YD, np.abs(Wv - level))     # 0 at the sea's level .. 1 a river above it
        sea_d = Wv - H
        d = np.where(sea, sea_d * (1 - t) + LAND_D * t, d)
        d = np.where(dry & oc & (H < level), level - H, d)    # dry sea floor in an ocean chunk
        d = np.where(dry & (H < FLOOR_YD), 1e3, d)            # the open ocean's floor, whatever the chunk says
        d = np.where(~dry & (H < FLOOR_YD), 1e3, d)           # any liquid over it too (four chunks of plain water at
                                                              # -23 and -11 over the floor in Kalimdor 26_54)
        d = np.where(oc & (H == 0.0), 1e3, d)                 # the client's unset ground, wet or dry
        d = np.where(open_sea, 1e3, d)
        self.d = d.astype(np.float32)
        self.level = level
        if log:
            log('  %s: %d tiles with a file, sea level %.2f, %d water bodies at the level, %d of them the sea' % (
                c_map, len(have), level, n, len(seed[seed > 0])))

    def tile_depth(self, X, Y):
        r, c = (Y - self.Y0) * 128, (X - self.X0) * 128
        return self.d[r:r + 129, c:c + 129]


def land_alpha(depth, n, shallow=SHALLOW_YD, deep=DEEP_YD):
    """[n,n] float32: the land alpha at the centres of an n x n tile's pixels, from its [129,129] corner depths
    (None: open sea, all 0). Row 0 north, column 0 west, as the master and the detail tiles."""
    if depth is None:
        return np.zeros((n, n), np.float32)
    s = (np.arange(n, dtype=np.float64) + 0.5) * 128.0 / n          # pixel centres in corner units
    i0 = np.clip(np.floor(s).astype(int), 0, 127)
    f = (s - i0).astype(np.float32)
    d = np.minimum(depth, 1e3)
    top = d[i0][:, i0] * (1 - f)[None, :] + d[i0][:, i0 + 1] * f[None, :]
    bot = d[i0 + 1][:, i0] * (1 - f)[None, :] + d[i0 + 1][:, i0 + 1] * f[None, :]
    dd = top * (1 - f)[:, None] + bot * f[:, None]
    return (1.0 - smoothstep(shallow, deep, dd)).astype(np.float32)
