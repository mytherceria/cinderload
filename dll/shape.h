/* The screen-shape choice of CinderLoad.dll, kept free of Windows calls so it can be tested on its own
 * (tests/test_shape.c). Included once, by cinderload.c.
 *
 * The loading screens come in one archive per screen shape, Data\CinderLoad\LoadingScreens-<shape>.MPQ. The
 * shape is read from the resolution the game saved (SET gxResolution "WxH" in WTF\Config.wtf) and the nearest
 * shape within 10% is used. The ultrawides are one shape: 2560x1080, 3440x1440, 3840x1600 and 5120x2160 are all
 * within 1.3% of 64:27.
 */
#ifndef CINDERLOAD_SHAPE_H
#define CINDERLOAD_SHAPE_H

static const struct { const char *name; int num, den; } kShapes[] = {
    { "16x10", 16, 10 },        /* 1.600 */
    { "16x9",  16, 9  },        /* 1.778 */
    { "21x9",  64, 27 },        /* 2.370: 2560x1080, 3440x1440, 3840x1600, 5120x2160 */
    { "32x9",  32, 9  },        /* 3.556: 5120x1440, 7680x2160 */
};
#define SHAPE_COUNT ((int)(sizeof kShapes / sizeof kShapes[0]))
#define SHAPE_TOLERANCE 100     /* per mille: within 10% of a shape, or none */

static char Lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

/* The resolution in a Config.wtf's text (NUL-terminated): the last SET gxResolution "WxH" line, as the game reads
 * the file top to bottom. 1 and *w, *h set when found and sane. A line may begin with spaces or tabs, and the file
 * with a UTF-8 byte-order mark (an editor's; the adversarial pass, 8 Oct night: either hid the line, and the monitor's
 * size picked the shape). The game writes the file clean again when it exits. */
static int ParseResolution(const char *text, int *w, int *h)
{
    static const char key[] = "set gxresolution \"";
    int found = 0;
    for (const char *p = text; *p; p++) {
        if (p != text && p[-1] != '\n') continue;          /* only at the start of a line */
        const char *s = p;
        if (s == text && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
            s += 3;                                         /* the file's UTF-8 mark */
        while (*s == ' ' || *s == '\t') s++;
        int i = 0;
        while (key[i] && Lower(s[i]) == key[i]) i++;
        if (key[i]) continue;
        const char *q = s + i;
        long a = 0, b = 0;
        int da = 0, db = 0;
        while (*q >= '0' && *q <= '9' && da < 6) { a = a * 10 + (*q++ - '0'); da++; }
        if (*q != 'x' && *q != 'X') continue;
        q++;
        while (*q >= '0' && *q <= '9' && db < 6) { b = b * 10 + (*q++ - '0'); db++; }
        if (*q != '"' || a < 320 || b < 200 || a > 32768 || b > 32768) continue;
        *w = (int)a; *h = (int)b;
        found = 1;
    }
    return found;
}

/* The game draws with OpenGL rather than Direct3D (the adversarial pass, 8 Oct night: its present is not the one the
 * blur hooks, so the view waited out its 3 seconds over the live world after every screen). As the 1.12 client decides
 * it: -d3d on its command line forces Direct3D, -opengl OpenGL; else the last SET gxApi line of Config.wtf (cfg may be
 * NULL), "OpenGL" in any case. A flag is a whole word of the command line. */
static int CommandLineSays(const char *cmd, const char *flag)
{
    for (const char *p = cmd; p && *p; p++) {
        if (p != cmd && p[-1] != ' ' && p[-1] != '\t') continue;
        int i = 0;
        while (flag[i] && Lower(p[i]) == flag[i]) i++;
        if (!flag[i] && (!p[i] || p[i] == ' ' || p[i] == '\t')) return 1;
    }
    return 0;
}
static int ConfigApiIsOpenGL(const char *text)
{
    static const char key[] = "set gxapi \"", gl[] = "opengl\"";
    int is = 0;
    for (const char *p = text; p && *p; p++) {
        if (p != text && p[-1] != '\n') continue;
        const char *s = p;
        if (s == text && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s += 3;
        while (*s == ' ' || *s == '\t') s++;
        int i = 0;
        while (key[i] && Lower(s[i]) == key[i]) i++;
        if (key[i]) continue;
        int j = 0;
        while (gl[j] && Lower(s[i + j]) == gl[j]) j++;
        is = !gl[j];                                        /* the last line counts, as the game reads it */
    }
    return is;
}
static int GameUsesOpenGL(const char *cmd, const char *cfg)
{
    if (CommandLineSays(cmd, "-d3d")) return 0;
    if (CommandLineSays(cmd, "-opengl")) return 1;
    return ConfigApiIsOpenGL(cfg);
}

/* The game shows in a window of its own rather than holding the whole screen (slice 5, 9 Oct: on Windows itself a game
 * in exclusive full screen would hide the view's window behind its own). As the 1.12 client decides it: -fullscreen
 * on its command line takes the whole screen, -windowed makes a window; else the last SET gxWindow line of Config.wtf
 * (cfg may be NULL), "0" the whole screen and any other value a window. With no such line, the client's own default
 * (byDefault): the client writes only values that differ from its defaults, and RavenCraft's WoW.exe registers
 * gxWindow as "1", so there a missing line means a window (the pre-release attack, 9 Oct: read as the whole screen,
 * the view would have stayed off for nearly every Windows player). */
static int ConfigWindowLine(const char *text)              /* -1 no SET gxWindow line; else 0 or 1, the last one's */
{
    static const char key[] = "set gxwindow \"";
    int win = -1;
    for (const char *p = text; p && *p; p++) {
        if (p != text && p[-1] != '\n') continue;
        const char *s = p;
        if (s == text && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s += 3;
        while (*s == ' ' || *s == '\t') s++;
        int i = 0;
        while (key[i] && Lower(s[i]) == key[i]) i++;
        if (key[i]) continue;
        win = s[i] != '0' && s[i] != '"';                  /* the last line counts, as the game reads it */
    }
    return win;
}
static int GameIsWindowed(const char *cmd, const char *cfg, int byDefault)
{
    if (CommandLineSays(cmd, "-fullscreen")) return 0;
    if (CommandLineSays(cmd, "-windowed")) return 1;
    int line = ConfigWindowLine(cfg);
    return line < 0 ? byDefault : line;
}

/* The index of the nearest shape within SHAPE_TOLERANCE of w:h, or -1. available[i] says whether that shape's
 * archive is there; only those count. */
static int ChooseShape(int w, int h, const int *available)
{
    if (w <= 0 || h <= 0) return -1;
    long aspect = (long)w * 1000 / h;
    int best = -1;
    long bestDiff = 0;
    for (int i = 0; i < SHAPE_COUNT; i++) {
        if (!available[i]) continue;
        long s = (long)kShapes[i].num * 1000 / kShapes[i].den;
        long diff = (aspect > s ? aspect - s : s - aspect) * 1000 / s;
        if (diff <= SHAPE_TOLERANCE && (best < 0 || diff < bestDiff)) { best = i; bestDiff = diff; }
    }
    return best;
}

#endif
