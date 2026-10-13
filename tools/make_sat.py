r"""The satellite's detail tiles for CinderLoad 0.4.0 (13 Oct 2026): Data\CinderLoad\CinderSat.MPQ, ONE archive for every
screen shape, beside the shape packs (never copied into patch-~.MPQ, never part of the start-up byte compare). The
contract with the DLL is ASSETS.md's "Contract for the DLL" (inbox/cinderload-040-satellite-2026-10-13); in short:

  CinderSat\sat.txt           ASCII, '#' comments, LF:  px <n>  /  levels <n>  /  t <c> <X> <Y>  (one per tile present)
  CinderSat\<c>_<X>_<Y>.blp   BLP2 DXT1, no alpha, px x px, mips down to 4 x 4; row 0 north, column 0 west
  c = map.txt's continent id (0 Eastern Kingdoms = the client's Azeroth, 1 Kalimdor); X, Y = the client's tile indices:
  X = floor((ORIGIN - y) / TILE), Y = floor((ORIGIN - x) / TILE), TILE = 533.33333, ORIGIN = 32 * TILE (tools/satgeo.py)

What a tile holds: the 3D master (tiles/half, 1024 px a tile) composited, at 1024 px, over 0.3.0's own overview of the
sea (decoded from that shape's shipped CinderMap.blp, sampled bilinearly as the game magnifies it) through satmask.py's
land alpha, the same alpha the new overview uses (tools/sat_overview.py); then area-averaged to px and to every mip
below it. So a tile's sea is the overview's sea: at the --sea-shape (21x9 by default, his screen) the two agree to the
texel; at the other shapes the painted sea's coast lines and rhumb lines sit a little differently (each shape's sea
was painted on its own canvas), and the DLL's feather at the detail's edge is what hides that.
Every tile the master drew is packed (land tiles and the sea tiles of the render's 4 x 4 regions), except those 0.3.0
hid (GM Island); --no-sea leaves out the tiles with no land alpha at all (the overview shows there anyway).

PLACEHOLDER (his call): the tile size. 512 px (about 1 px a yard, his Elwynn picture) is the placeholder default; 256 px
is the client's own minimap size. Both are built by default; he picks one, and only that archive ships.

Usage: STORMLIB=/path/to/libstorm.so python3 make_sat.py <master dir> <0.3.0 packs dir> <out dir>
         [--px 512,256] [--sea-shape 21x9] [--best] [--no-sea] [--jobs N] [--only c_X_Y,...]
  --best   ImageMagick's DXT1 cluster fit (about 2 dB closer, about 300 times slower: some 20 minutes on 32 threads)
  --only   just these tiles, for a look (no archive)
Writes <out>/<px>/CinderSat.MPQ, <out>/<px>/sat.txt, <out>/<px>/tiles/<c>_<X>_<Y>.blp, <out>/<px>/BUILD.txt; then reads
every file back through StormLib and through shared/mpq.c (tests/sat_read.c, built here with gcc) and compares.
"""
import concurrent.futures as cf, ctypes, hashlib, math, multiprocessing as mp, os, subprocess, sys, tempfile, time
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import satgeo, satmask
from blp import read_blp, write_blp_mips
from sat_overview import Mpq, hidden_tiles, CLIENT

Image.MAX_IMAGE_PIXELS = None
SRC = 1024                       # the master's pixels a tile
MIN_LEVEL = 4                    # the contract: mips down to 4 x 4
G = {}                           # the workers' shared inputs (fork: copied on write, never pickled)


def levels_for(px):
    n, k = 0, px
    while k >= MIN_LEVEL:
        n += 1; k //= 2
    return n


def sea_sampler(old, table, c):
    """For tile pixels at 1024: the overview's colour, bilinear between its texel centres (as OpenGL magnifies it)."""
    def sample(X, Y):
        k = (np.arange(SRC, dtype=np.float64) + 0.5) / SRC
        xs = satgeo.ORIGIN - (Y + k) * satgeo.TILE              # world x of each row's centre
        ys = satgeo.ORIGIN - (X + k) * satgeo.TILE              # world y of each column's centre
        au, bu, av, bv = table[c]
        fu = (au + bu * ys) * old.shape[1] - 0.5                 # texel coordinates, texel centres at integers
        fv = (av + bv * xs) * old.shape[0] - 0.5
        u0 = np.clip(np.floor(fu).astype(int), 0, old.shape[1] - 2); wu = np.clip(fu - u0, 0, 1).astype(np.float32)
        v0 = np.clip(np.floor(fv).astype(int), 0, old.shape[0] - 2); wv = np.clip(fv - v0, 0, 1).astype(np.float32)
        ua, va = u0.min(), v0.min()                              # only the few texels the tile covers
        sub = old[va:v0.max() + 2, ua:u0.max() + 2].astype(np.float32)
        u0, v0 = u0 - ua, v0 - va
        rows = sub[v0] * (1 - wv)[:, None, None] + sub[v0 + 1] * wv[:, None, None]
        return rows[:, u0] * (1 - wu)[None, :, None] + rows[:, u0 + 1] * wu[None, :, None]
    return sample


def tile_job(key):
    c, X, Y = key
    depth = G['depth'][key]
    out = G['out']
    im = np.asarray(Image.open(satgeo.master_path(G['master'], c, X, Y)).convert('RGB'), np.float32)
    a = satmask.land_alpha(depth, SRC)
    sea = G['sea'][c](X, Y)
    comp = im * a[..., None] + sea * (1 - a[..., None])
    chain, k = {SRC: comp}, SRC
    while k > MIN_LEVEL:
        p = chain[k]
        chain[k // 2] = p.reshape(k // 2, 2, k // 2, 2, 3).mean((1, 3))     # the exact area average, level by level
        k //= 2
    res = dict(key=key, amax=float(a.max()), amean=float(a.mean()))
    for px in G['pxs']:
        ims = [Image.fromarray(np.clip(chain[px >> j] + 0.5, 0, 255).astype(np.uint8)) for j in range(levels_for(px))]
        dst = os.path.join(out, str(px), 'tiles', '%d_%d_%d.blp' % key)
        res[px] = write_blp_mips(ims, dst, best=G['best'])
    return res


def pack(mpq_path, files):
    """As make_pack.py's pack(): StormLib, a listfile, every file zlib-compressed (what shared/mpq.c reads)."""
    lib = ctypes.CDLL(os.environ.get('STORMLIB', 'libstorm.so'))
    lib.SFileCreateArchive.restype = ctypes.c_bool
    lib.SFileAddFileEx.restype = ctypes.c_bool
    lib.SFileCloseArchive.restype = ctypes.c_bool
    if os.path.exists(mpq_path):
        os.remove(mpq_path)
    h = ctypes.c_void_p()
    if not lib.SFileCreateArchive(mpq_path.encode(), 0x00100000, max(64, len(files) * 2), ctypes.byref(h)):
        raise SystemExit('could not create the archive')
    for src, name in files:
        if not lib.SFileAddFileEx(h, src.encode(), name.encode(), 0x00000200 | 0x80000000, 0x02, 0xFFFFFFFF):
            lib.SFileCloseArchive(h); os.remove(mpq_path); raise SystemExit(f'could not add {src}')
    if not lib.SFileCloseArchive(h):
        raise SystemExit('could not close the archive')


def check_archive(mpq_path, files, work):
    """Every file read back through StormLib and through shared/mpq.c, compared byte for byte with its source, and the
    archive's own header and tables checked against what shared/mpq.c accepts. Returns the C reader's line."""
    a = Mpq(mpq_path)
    names = set(n.lower() for n in a.names())
    for src, name in files:
        got = a.read(name)
        if got != open(src, 'rb').read():
            raise SystemExit('StormLib reads %s back different from %s' % (name, src))
    a.close()
    want = set(n.lower() for _, n in files)
    if names - {'(listfile)'} != want:
        raise SystemExit('the listfile names %d files, %d were packed' % (len(names), len(want)))
    head = open(mpq_path, 'rb').read(32)
    import struct
    magic, hsize, asize, ver, shift, hoff, boff, hcount, bcount = struct.unpack('<4sIIHHIIII', head)
    if magic != b'MPQ\x1a' or ver != 0 or hcount & (hcount - 1) or hcount < len(files) + 1 or shift > 15:
        raise SystemExit('archive header: %r version %d hash table %d sector shift %d' % (magic, ver, hcount, shift))
    exe = os.path.join(work, 'sat_read')
    root = os.path.dirname(HERE)
    subprocess.run(['gcc', '-std=c99', '-O2', '-Wall', '-I' + os.path.join(root, 'shared'), '-o', exe,
                    os.path.join(root, 'tests', 'sat_read.c'), os.path.join(root, 'shared', 'mpq.c'),
                    os.path.join(root, 'shared', 'inflate.c')], check=True)
    lst = os.path.join(work, 'sat_read.lst')
    with open(lst, 'w') as f:
        for src, name in files:
            f.write('%s\t%s\n' % (name, os.path.abspath(src)))
    r = subprocess.run([exe, mpq_path, lst], capture_output=True, text=True)
    line = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr
    if r.returncode:
        raise SystemExit('shared/mpq.c could not read the archive back:\n' + r.stdout[-3000:])
    return line, (ver, hcount, bcount, shift)


def sat_txt(px, keys):
    lines = ['# CinderLoad satellite detail tiles (tools/make_sat.py); the contract: ASSETS.md "Contract for the DLL"',
             '# px: tile edge in pixels; levels: mips in each BLP, px down to 4; t <c> <X> <Y>: tile present',
             '# c = map.txt continent (0 Eastern Kingdoms, 1 Kalimdor); X, Y = client tile column, row',
             'px %d' % px, 'levels %d' % levels_for(px)]
    lines += ['t %d %d %d' % k for k in sorted(keys, key=lambda k: (k[0], k[2], k[1]))]
    return '\n'.join(lines) + '\n'


def main():
    args = sys.argv[1:]
    opt = lambda k, d: (args.pop(args.index(k) + 1), args.remove(k))[0] if k in args else d
    flag = lambda k: (args.remove(k), True)[1] if k in args else False
    pxs = [int(p) for p in opt('--px', '512,256').split(',')]
    sea_shape = opt('--sea-shape', '21x9')
    jobs = int(opt('--jobs', str(min(30, os.cpu_count() or 4))))
    only = opt('--only', None)
    client = opt('--client', CLIENT)
    best, no_sea = flag('--best'), flag('--no-sea')
    master, packs, out = (os.path.expanduser(a) for a in args[:3])
    for px in pxs:
        if px not in (256, 512) and not only:
            raise SystemExit('--px: 512 or 256 (the contract)')
        os.makedirs(os.path.join(out, str(px), 'tiles'), exist_ok=True)
    t0 = time.time()
    tiles, _ = satgeo.master_tiles(master)
    hidden = hidden_tiles(client)
    keys = [(c, X, Y) for c, X, Y, _ in tiles if (X, Y) not in hidden[c]]
    if only:
        want = {tuple(int(v) for v in s.split('_')) for s in only.split(',')}
        keys = [k for k in keys if k in want]
    # the depth fields, one continent at a time, cut per tile (the continent's arrays are freed before the fork)
    depth = {}
    for c in (0, 1):
        mine = [k for k in keys if k[0] == c]
        if not mine:
            continue
        allc = [(X, Y) for cc, X, Y, _ in tiles if cc == c]
        cont = satmask.Continent(satgeo.MAPS[c], min(x for x, _ in allc), max(x for x, _ in allc),
                                 min(y for _, y in allc), max(y for _, y in allc), log=print)
        for k in mine:
            depth[k] = cont.tile_depth(k[1], k[2]).copy()
        del cont
    pk = Mpq(os.path.join(packs, 'LoadingScreens-%s.MPQ' % sea_shape))
    info, decode = read_blp(pk.read('Interface\\Glues\\LoadingScreens\\CinderMap.blp'))
    table, _ = satgeo.read_map_txt(pk.read('CinderLoad\\map.txt').decode('ascii'))
    pk.close()
    old = np.ascontiguousarray(decode(0)[..., :3])
    G.update(master=master, out=out, pxs=pxs, best=best, depth=depth,
             sea={c: sea_sampler(old, table, c) for c in (0, 1)})
    print('%d tiles to build at %s px (sea from %s, %s fit, %d workers)' % (
        len(keys), ','.join(map(str, pxs)), sea_shape, 'cluster' if best else 'range', jobs), flush=True)
    results = []
    ctx = mp.get_context('fork')
    with cf.ProcessPoolExecutor(max_workers=jobs, mp_context=ctx) as ex:
        for i, r in enumerate(ex.map(tile_job, keys, chunksize=4)):
            results.append(r)
            if (i + 1) % 200 == 0:
                print('  %d / %d tiles, %.0f s' % (i + 1, len(keys), time.time() - t0), flush=True)
    sea_only = sorted(r['key'] for r in results if r['amax'] <= 0.0)
    print('tiles: %d, of which %d hold no land alpha at all (painted sea only)' % (len(results), len(sea_only)))
    if only:
        return
    for px in pxs:
        d = os.path.join(out, str(px))
        keep = [r['key'] for r in results if not (no_sea and r['amax'] <= 0.0)]
        idx = os.path.join(d, 'sat.txt')
        with open(idx, 'w', encoding='ascii', newline='\n') as f:
            f.write(sat_txt(px, keep))
        files = [(idx, 'CinderSat\\sat.txt')] + [(os.path.join(d, 'tiles', '%d_%d_%d.blp' % k),
                                                  'CinderSat\\%d_%d_%d.blp' % k) for k in sorted(keep)]
        mpq = os.path.join(d, 'CinderSat.MPQ')
        pack(mpq, files)
        line, (ver, hc, bc, shift) = check_archive(mpq, files, d)
        raw = sum(os.path.getsize(s) for s, _ in files)
        size = os.path.getsize(mpq)
        sha = hashlib.sha256(open(mpq, 'rb').read()).hexdigest()
        report = ['CinderSat.MPQ at %d px (PLACEHOLDER: the tile size is his call)' % px,
                  'tiles %d (%d of them painted sea only%s), %d levels each, %d bytes a tile' % (
                      len(keep), sum(1 for k in keep if k in set(sea_only)), ', kept' if not no_sea else '',
                      levels_for(px), os.path.getsize(files[1][0])),
                  'archive %d bytes (%.1f MB), its files %d bytes before zlib (%.1f MB)' % (
                      size, size / 1e6, raw, raw / 1e6),
                  'MPQ format version %d, hash table %d, block table %d, sectors %d bytes' % (ver, hc, bc, 512 << shift),
                  'sea from 0.3.0\'s %s CinderMap.blp; DXT1 %s fit' % (sea_shape, 'cluster' if best else 'range'),
                  'StormLib read back: every file identical',
                  line,
                  'sha256 %s' % sha]
        open(os.path.join(d, 'BUILD.txt'), 'w').write('\n'.join(report) + '\n')
        print('\n'.join('  ' + l for l in report))
    print('done in %.0f s' % (time.time() - t0))


if __name__ == '__main__':
    main()
