r"""A CinderLoad 0.4.0 test pack for one screen shape (13 Oct 2026): 0.3.0's shipped LoadingScreens-<shape>.MPQ with only
its world map picture swapped for the 3D one (tools/sat_overview.py's CinderMap-<shape>.blp). Every other file is
copied byte for byte: map.txt, the overlays (sea.bin, flow.bin, routes.txt, labels.txt, glow.txt, weather.bin, ...),
the sheets, the bars and the 66 loading screens. Nothing in the shape pack names the detail tiles: the DLL finds them
by their own path, Data\CinderLoad\CinderSat.MPQ (make_sat.py; ASSETS.md "Contract for the DLL"), and without that file
the satellite is the overview alone, as in 0.3.0.

Usage: STORMLIB=/path/to/libstorm.so python3 make_pack040.py <0.3.0 pack> <new CinderMap.blp> <out pack>
Writes <out pack> (never over an existing file), then reads every file back and checks: CinderMap.blp is the new one,
every other file is 0.3.0's to the byte, and the two archives hold the same names; through StormLib and through
shared/mpq.c (the DLL's own reader, tests/sat_read.c).
"""
import os, struct, sys, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sat_overview import Mpq
from make_pack import pack
from blp import read_blp
from make_sat import check_archive

MAP = 'Interface\\Glues\\LoadingScreens\\CinderMap.blp'


def build(old_pack, new_map, out):
    if os.path.exists(out):
        raise SystemExit('%s exists; not overwriting' % out)
    new = open(new_map, 'rb').read()
    info, _ = read_blp(new)
    if (info['w'], info['h'], info['dxt'], info['levels']) != (4096, 4096, 1, 13):
        raise SystemExit('%s is not a 4096 x 4096 DXT1 picture with its 13 levels: %s' % (new_map, info))
    a = Mpq(old_pack)
    names = a.names()
    head = open(old_pack, 'rb').read(32)
    bcount = struct.unpack_from('<I', head, 28)[0]
    if bcount != len(names) + 1:                        # the files and the listfile itself: nothing unnamed is lost
        raise SystemExit('%s: %d block entries, the listfile names %d files' % (old_pack, bcount, len(names)))
    if MAP not in names:
        raise SystemExit('%s has no %s' % (old_pack, MAP))
    with tempfile.TemporaryDirectory(dir=os.path.dirname(os.path.abspath(out))) as tmp:
        files, old = [], {}
        for i, n in enumerate(names):
            d = a.read(n)
            if d is None:
                raise SystemExit('%s: cannot read %s' % (old_pack, n))
            old[n] = d
            src = os.path.join(tmp, '%04d' % i)
            open(src, 'wb').write(new if n == MAP else d)
            files.append((src, n))
        a.close()
        os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
        pack(out, files)
        b = Mpq(out)
        if sorted(b.names()) != sorted(names):
            raise SystemExit('the new pack names other files than 0.3.0\'s')
        for n in names:
            if b.read(n) != (new if n == MAP else old[n]):
                raise SystemExit('%s reads back different' % n)
        b.close()
        line, _ = check_archive(out, files, tmp)        # and through shared/mpq.c, the DLL's own reader
        print('  ' + line)
    return len(names), os.path.getsize(out)


if __name__ == '__main__':
    n, size = build(*(os.path.expanduser(a) for a in sys.argv[1:4]))
    print('%s: %d files, %d bytes; CinderMap.blp swapped, the other %d byte for byte as 0.3.0' % (
        sys.argv[3], n, size, n - 1))
