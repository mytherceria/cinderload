r"""The /cinderload window's pictures (dll/switches.lua loads them as Interface\CinderLoad\<name>), cut from two real
loading screens recorded in game: one with the parchment and the Retro bar, one with the satellite and the Neo bar
(1704 x 720 frames; the crops below are for that size). Written as BLP textures the client reads (tools/blp.py), into
<out dir>; make_pack.py --panel <out dir> puts them in each screen shape's pack, so they reach the game as
Data/patch-~.MPQ as the loading screens do.

Usage: python3 -I make_panel_art.py <parchment+retro frame png> <satellite+neo frame png> <out dir>
   e.g. ffmpeg -ss 112 -i recording.mp4 -frames:v 1 parch.png; ffmpeg -ss 61 -i recording.mp4 -frames:v 1 sat.png

The Smooth chip is the Heat chip smoothed: an illustration until the Smooth fill is drawn in game. The preview is a
still (the bar's foot of each map, the bar lifted out of it, and each bar on its own); the window lays the bar on it at
the chosen thickness and width.
"""
import os, sys
import numpy as np
from PIL import Image, ImageFilter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from blp import write_blp

# name: (texture width, height, alpha) for each picture, as the window names them
NAMES = {
    'PanelParchment': (512, 256, False), 'PanelSatellite': (512, 256, False),
    'PanelRetro': (512, 128, False), 'PanelNeo': (512, 128, False),
    'PanelHeat': (128, 32, False), 'PanelSmooth': (128, 32, False), 'PanelBlade': (128, 32, False),
    'PreviewParchment': (1024, 256, False), 'PreviewSatellite': (1024, 256, False),
    'PreviewRetro': (512, 64, True), 'PreviewNeo': (1024, 64, False),
}
STRIP = 245                       # the preview's strip: the frame's rows from here down (316 x 88 of the window's units)
RETRO_BAR = (614, 644, 1090, 686)  # the Retro bar alone in the parchment frame (476 x 42)
NEO_BAND = (668, 716)              # the Neo bar's frame band in the satellite frame, rows (edge to edge)


def lifted(frame, y0, y1, x0=0, x1=None):
    """The frame's strip with rows y0..y1 (the bar) covered by the rows just above them, mirrored, then dimmed."""
    a = np.asarray(frame).astype(np.float32).copy()
    x1 = x1 or a.shape[1]
    h = y1 - y0
    a[y0:y1, x0:x1] = a[y0 - h:y0, x0:x1][::-1]
    a = a[STRIP:] * 0.7
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def retro_bar(frame):
    """The Retro bar with its own outline: the dark frame and the channel kept, the parchment around it see-through."""
    bar = frame.crop(RETRO_BAR)
    r = np.asarray(bar).astype(np.float32) / 255
    m = ((0.2126 * r[..., 0] + 0.7152 * r[..., 1] + 0.0722 * r[..., 2]) < 0.24).astype(np.float32)
    m[9:34, 28:449] = 1
    mask = Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.MinFilter(3))
    mask = mask.filter(ImageFilter.GaussianBlur(0.8))
    out = bar.convert('RGBA')
    out.putalpha(mask)
    return out


def main():
    parch = Image.open(sys.argv[1]).convert('RGB')
    sat = Image.open(sys.argv[2]).convert('RGB')
    out = sys.argv[3]
    if parch.size != (1704, 720) or sat.size != (1704, 720):
        raise SystemExit('the two frames must be 1704 x 720, as recorded')
    os.makedirs(out, exist_ok=True)
    heat = parch.crop((810, 652, 906, 678))
    pics = {
        'PanelParchment': parch.crop((730, 200, 1560, 644)),
        'PanelSatellite': sat.crop((780, 150, 1610, 594)),
        'PanelRetro': parch.crop((612, 617, 1092, 715)),
        'PanelNeo': sat.crop((612, 622, 1092, 720)),
        'PanelHeat': heat,
        'PanelSmooth': heat.resize((8, 26), Image.BOX).resize((96, 26), Image.BICUBIC),
        'PanelBlade': sat.crop((560, 674, 671, 704)),
        'PreviewParchment': lifted(parch, RETRO_BAR[1] - 2, RETRO_BAR[3] + 4, 600, 1110),
        'PreviewSatellite': lifted(sat, NEO_BAND[0], 720),
        'PreviewRetro': retro_bar(parch),
        'PreviewNeo': sat.crop((0, NEO_BAND[0], 1704, NEO_BAND[1])),
    }
    assert set(pics) == set(NAMES)
    for name, im in pics.items():
        w, h, alpha = NAMES[name]
        png = os.path.join(out, name + '.png')
        im.save(png)
        n, size = write_blp(png, os.path.join(out, name + '.blp'), w, h, alpha=alpha)
        print(f'{name}.blp {w}x{h} {"DXT5" if alpha else "DXT1"}, {n} levels, {size} bytes')


if __name__ == '__main__':
    main()
