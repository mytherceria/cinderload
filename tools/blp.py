"""Writes a picture as a BLP2 texture the 1.12 client reads, the way its own loading screens are stored: DXT1, no
alpha, a full chain of smaller copies (mipmaps). ImageMagick does the DXT1 compression into a DDS file; this only
moves those blocks into a BLP2 header.

Usage as a module: write_blp(png_path, blp_path, width, height). The picture is resized to width x height exactly,
so a picture drawn at the screen's shape is squeezed into the texture and the game stretches it back.
"""
import os, struct, subprocess, tempfile


def write_blp(src, dst, width, height, alpha=False):
    """alpha: DXT5 with the picture's own alpha (the overlay's sheets); otherwise DXT1, no alpha, as the loading screens."""
    fmt = b'DXT5' if alpha else b'DXT1'
    with tempfile.TemporaryDirectory() as tmp:
        dds = os.path.join(tmp, 'tex.dds')
        subprocess.run(['magick', src, '-resize', f'{width}x{height}!'] + ([] if alpha else ['-alpha', 'off']) +
                       ['-define', 'dds:compression=' + fmt.decode().lower(),
                        '-define', 'dds:mipmaps=16', dds], check=True)
        data = open(dds, 'rb').read()
    if data[:4] != b'DDS ' or data[84:88] != fmt:
        raise SystemExit('ImageMagick did not write a %s DDS' % fmt.decode())
    h, w = struct.unpack_from('<II', data, 12)          # DDS_HEADER: height, then width
    mips = struct.unpack_from('<I', data, 28)[0]          # dwMipMapCount, the full-size picture included
    if (w, h) != (width, height):
        raise SystemExit(f'DDS is {w}x{h}, wanted {width}x{height}')
    mips = max(1, mips)
    offsets, sizes, pos, lw, lh = [], [], 128, w, h
    for _ in range(min(mips, 16)):
        n = max(1, (lw + 3) // 4) * max(1, (lh + 3) // 4) * (16 if alpha else 8)
        offsets.append(pos); sizes.append(n); pos += n
        lw, lh = max(1, lw // 2), max(1, lh // 2)
    if pos > len(data):
        raise SystemExit('DDS shorter than its mipmaps')
    header_len = 4 + 4 + 4 + 8 + 64 + 64 + 1024          # 1172, as in the client's own files
    blob = b''.join(data[o:o + s] for o, s in zip(offsets, sizes))
    out_offsets, p = [], header_len
    for s in sizes:
        out_offsets.append(p); p += s
    out_offsets += [0] * (16 - len(out_offsets)); out_sizes = sizes + [0] * (16 - len(sizes))
    header = b'BLP2' + struct.pack('<IBBBBII', 1, 2, 8 if alpha else 0, 7 if alpha else 0, 1 if len(sizes) > 1 else 0, w, h)
    header += struct.pack('<16I', *out_offsets) + struct.pack('<16I', *out_sizes) + bytes(1024)
    assert len(header) == header_len
    with open(dst, 'wb') as f:
        f.write(header + blob)
    return len(sizes), os.path.getsize(dst)


def read_blp(data):
    """A BLP2 of DXT1 or DXT5 blocks (as write_blp writes them) read back: (info, decode), info = {'w', 'h', 'levels',
    'dxt', 'alpha_depth', 'offs', 'lens'} with the levels counted as CinderLoad.dll's BlpParse counts them (from the
    whole picture down, while the header names a level whole inside the file), decode(level) = RGBA uint8 [h, w, 4].
    Pictures only (numpy); for checks and stills, never in the game."""
    import numpy as np
    if len(data) < 1172 or data[:4] != b'BLP2' or struct.unpack_from('<I', data, 4)[0] != 1 or data[8] != 2:
        raise ValueError('not a BLP2 of DXT blocks')
    ad, ae, mips = data[9], data[10], data[11]
    dxt = 1 if ae == 0 else 5 if ae == 7 else 3 if ae == 1 else 0
    if dxt not in (1, 5):
        raise ValueError('alpha encoding %d: only DXT1 and DXT5 are read here' % ae)
    w, h = struct.unpack_from('<II', data, 12)
    offs = struct.unpack_from('<16I', data, 20)
    lens = struct.unpack_from('<16I', data, 84)
    block = 8 if dxt == 1 else 16
    levels, lw, lh, keep_o, keep_l = 0, w, h, [], []
    for i in range(16):
        want = ((lw + 3) // 4) * ((lh + 3) // 4) * block
        if not offs[i] or lens[i] < want or offs[i] + want > len(data):
            break
        keep_o.append(offs[i]); keep_l.append(want); levels += 1
        if not mips or (lw == 1 and lh == 1):
            break
        lw, lh = max(1, lw // 2), max(1, lh // 2)
    info = dict(w=w, h=h, levels=levels, dxt=dxt, alpha_depth=ad, offs=keep_o, lens=keep_l, mips=mips)

    def colours(a):
        c0 = a[..., 0].astype(np.uint32) | (a[..., 1].astype(np.uint32) << 8)
        c1 = a[..., 2].astype(np.uint32) | (a[..., 3].astype(np.uint32) << 8)
        def rgb(c):
            r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
            return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1).astype(np.int32)
        p0, p1 = rgb(c0), rgb(c1)
        four = ((c0 > c1) | (dxt != 1))[..., None]
        p2 = np.where(four, (2 * p0 + p1) // 3, (p0 + p1) // 2)
        p3 = np.where(four, (p0 + 2 * p1) // 3, 0)
        al = np.full(c0.shape + (4,), 255, np.int32)
        if dxt == 1:
            al[..., 3] = np.where(four[..., 0], 255, 0)
        pal = np.concatenate([np.stack([p0, p1, p2, p3], 2), al[..., None]], -1)       # [bh, bw, 4, 4]
        bits = (a[..., 4].astype(np.uint32) | (a[..., 5].astype(np.uint32) << 8) |
                (a[..., 6].astype(np.uint32) << 16) | (a[..., 7].astype(np.uint32) << 24))
        ix = ((bits[..., None] >> (np.arange(16, dtype=np.uint32) * 2)) & 3).astype(np.int64)
        return np.take_along_axis(pal, ix[..., None].repeat(4, -1), axis=2)              # [bh, bw, 16, 4]

    def decode(level=0):
        lw, lh = max(1, w >> level), max(1, h >> level)
        bw, bh = (lw + 3) // 4, (lh + 3) // 4
        raw = np.frombuffer(data, np.uint8, bw * bh * block, info['offs'][level]).reshape(bh, bw, block)
        if dxt == 1:
            px = colours(raw)
        else:
            px = colours(raw[..., 8:])
            a0, a1 = raw[..., 0].astype(np.int32), raw[..., 1].astype(np.int32)
            k = np.arange(1, 7, dtype=np.int32)
            hi = ((7 - k)[None, None] * a0[..., None] + k[None, None] * a1[..., None]) // 7
            lo = ((5 - k[:4])[None, None] * a0[..., None] + k[:4][None, None] * a1[..., None]) // 5
            pal8 = np.where((a0 > a1)[..., None],
                            np.concatenate([a0[..., None], a1[..., None], hi], -1),
                            np.concatenate([a0[..., None], a1[..., None], lo,
                                            np.zeros_like(a0)[..., None], np.full_like(a0, 255)[..., None]], -1))
            bits = np.zeros(raw.shape[:2], np.uint64)
            for i in range(6):
                bits |= raw[..., 2 + i].astype(np.uint64) << np.uint64(8 * i)
            ix = ((bits[..., None] >> (np.arange(16, dtype=np.uint64) * np.uint64(3))) & np.uint64(7)).astype(np.int64)
            px[..., 3] = np.take_along_axis(pal8, ix, axis=2)
        out = px.reshape(bh, bw, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 4)
        return out[:lh, :lw].astype(np.uint8)
    return info, decode


def write_blp_mips(images, dst, best=False):
    """A BLP2 DXT1 (no alpha) of the given levels exactly, level 0 first, each half the one before (the satellite's
    detail tiles: their own area-averaged mips, down to 4 x 4, make_sat.py). ImageMagick compresses the list as it is
    (dds:mipmaps=fromlist); best=True asks for its cluster fit (about 2 dB closer to the picture, about 300 times
    slower). Returns (levels, bytes)."""
    w, h = images[0].size
    for k, im in enumerate(images):
        if im.size != (max(1, w >> k), max(1, h >> k)):
            raise SystemExit('level %d is %s, wanted %dx%d' % (k, im.size, max(1, w >> k), max(1, h >> k)))
    if len(images) > 16:
        raise SystemExit('a BLP2 holds 16 levels at most')
    with tempfile.TemporaryDirectory() as tmp:
        names = []
        for k, im in enumerate(images):
            names.append(os.path.join(tmp, 'l%02d.png' % k))
            im.convert('RGB').save(names[-1])
        dds = os.path.join(tmp, 'tex.dds')
        subprocess.run(['magick'] + names + ['-alpha', 'off', '-define', 'dds:compression=dxt1',
                                             '-define', 'dds:mipmaps=fromlist'] +
                       (['-define', 'dds:cluster-fit=true'] if best else []) + [dds], check=True,
                       env=dict(os.environ, MAGICK_THREAD_LIMIT=os.environ.get('MAGICK_THREAD_LIMIT', '1')))
        data = open(dds, 'rb').read()
    if data[:4] != b'DDS ' or data[84:88] != b'DXT1':
        raise SystemExit('ImageMagick did not write a DXT1 DDS')
    dh, dw = struct.unpack_from('<II', data, 12)
    if (dw, dh) != (w, h) or struct.unpack_from('<I', data, 28)[0] != len(images):
        raise SystemExit('the DDS is %dx%d in %d levels, wanted %dx%d in %d' % (
            dw, dh, struct.unpack_from('<I', data, 28)[0], w, h, len(images)))
    sizes = [max(1, (max(1, w >> k) + 3) // 4) * max(1, (max(1, h >> k) + 3) // 4) * 8 for k in range(len(images))]
    if 128 + sum(sizes) != len(data):
        raise SystemExit('the DDS is %d bytes, its levels want %d' % (len(data), 128 + sum(sizes)))
    header_len = 4 + 4 + 4 + 8 + 64 + 64 + 1024          # 1172, as write_blp
    offs, p = [], header_len
    for s in sizes:
        offs.append(p); p += s
    header = b'BLP2' + struct.pack('<IBBBBII', 1, 2, 0, 0, 1 if len(sizes) > 1 else 0, w, h)
    header += struct.pack('<16I', *(offs + [0] * (16 - len(offs)))) + struct.pack('<16I', *(sizes + [0] * (16 - len(sizes))))
    header += bytes(1024)
    with open(dst, 'wb') as f:
        f.write(header + data[128:])
    return len(sizes), os.path.getsize(dst)
