#define _GNU_SOURCE                                     /* (MAP_32BIT: the explored test's low memory) */
/* Runs CinderLoad.dll's own code (dll/cinderload.c) against fake game folders, on POSIX through tests/shim:
 *   gcc -std=c99 -Wall -Wno-unused-function -Itests/shim -Ishared -o /tmp/test_switch tests/test_switch.c \
 *       shared/mpq.c shared/inflate.c -lm && /tmp/test_switch <scratch dir> [<client folder>]
 * Each case is a fresh folder under <scratch dir>; nothing outside it is touched. With a client folder (one holding
 * Data/patch.MPQ and Data/sound.MPQ, opened for reading only), the fire sound is also built from that client's own
 * files and played through the mock's timeline into <scratch dir>/fire-timeline.wav, for listening to, and the places
 * the fire sound and the fire bar rely on are checked against that client's WoW.exe. */
/* The client's 32-bit addresses are not real ones here: they are read from the fake image, shim_module, as if it sat
 * at the client's base, 0x400000. */
#define ClientAt(va) ((unsigned)(va) - 0x400000u <= SHIM_MODULE_SIZE - 4u ? shim_module + ((unsigned)(va) - 0x400000u) \
                                                                       : (const unsigned char *)0)
static int TestParchUp(int slot);
#define PARCH_UP TestParchUp                           /* (the parchment's paintings kept here instead of OpenGL) */
#include "../dll/cinderload.c"
static const float kWholeUV[8] = { 0, 1, 1, 1, 0, 0, 1, 0 };   /* the whole picture: corners 0,0 1,0 0,1 1,1 */
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <strings.h>
#include <signal.h>
#include <sys/wait.h>

char shim_gamedir[512];
unsigned char shim_module[SHIM_MODULE_SIZE];
int shim_threads;
DWORD shim_ticks;
int shim_vq;
int shim_screen_w = 3440, shim_screen_h = 1440;
char shim_cmdline[512] = "\"C:\\Games\\WoW\\WoW.exe\"";
int shim_wine;
HWND shim_game;
int shim_game_alive;
DWORD shim_game_tid = 77;
HOOKPROC shim_hook;
DWORD shim_hook_tid;
int shim_hooks, shim_unhooks, shim_hook_fail, shim_posted, shim_post_fail;
UINT shim_posted_msg;

static int fails, checks;
static void Check(int cond, const char *what) { checks++; if (!cond) { printf("FAIL: %s\n", what); fails++; } }

static const char *g_root;
static const char *g_client;                        /* a real client's folder, when given */
static void Fresh(const char *name)
{
    snprintf(shim_gamedir, sizeof shim_gamedir, "%s/%s/", g_root, name);
    const char *dirs[] = { "", "Data", "Data/CinderLoad", "WTF", "Logs" };
    for (int i = 0; i < 5; i++) {
        char p[700];
        snprintf(p, sizeof p, "%s%s", shim_gamedir, dirs[i]);
        mkdir(p, 0755);
    }
    memset(shim_module, 0, sizeof shim_module);
    memcpy(shim_module + PATCH_RVA, kStock, sizeof kStock);
    memcpy(shim_module + BAR_FILL_RVA, kFillStock, 16);
    memcpy(shim_module + BAR_BORDER_RVA, kBorderStock, 16);
    for (int i = 0; i < FIRE_CODE_COUNT; i++)
        memcpy(shim_module + kFireCode[i].rva, kFireCode[i].bytes, (size_t)kFireCode[i].n);
    for (int i = 0; i < BAR_CODE_COUNT; i++)
        memcpy(shim_module + kBarCode[i].rva, kBarCode[i].bytes, (size_t)kBarCode[i].n);
    for (int i = 0; i < REVEAL_CODE_COUNT; i++)
        memcpy(shim_module + kRevealCode[i].rva, kRevealCode[i].bytes, (size_t)kRevealCode[i].n);
    memcpy(shim_module + WALL_LOAD_RVA, kWallLoad, sizeof kWallLoad);
    memcpy(shim_module + BG_DRAW_RVA, kBgDraw, sizeof kBgDraw);
    for (int i = 0; i < OVERLAY_CALLS; i++) {           /* the overlay's six calls, as WoW.exe makes them */
        unsigned rel = (kOverlayCalls[i].target - 0x400000u) - (kOverlayCalls[i].rva + 5);
        shim_module[kOverlayCalls[i].rva] = 0xE8;
        memcpy(shim_module + kOverlayCalls[i].rva + 1, &rel, 4);
    }
    g_ovOn = 0; g_seaOn = 0; g_seaTried = 0; g_seaTex = NULL; g_seaW = g_seaH = 0;
    g_logoOn = 0; g_logoTried = 0; g_logoTex = NULL; g_logo.ok = 0;
    g_marksOn = 0; g_marksTried = 0; g_marksTex = NULL; g_mael.ok = 0;
    g_wavesOn = g_wavesTried = 0; g_wavesTex = NULL; g_flowOn = g_flowTried = 0; g_flowTex = NULL; g_flowW = 0;
    g_glowOn = 0; g_glowTried = 0; g_glowTex = NULL; g_glowItems = g_glowSprites = 0; g_glowMap[0].ok = g_glowMap[1].ok = 0;
    g_glowPick = -2; g_glowFor = 0;
    g_class = g_classFrom = g_classEntered = 0; g_classSure = 0; g_barClassOk = 0; g_barClassUsed = 0;
    for (int i = 0; i < CLASS_CODE; i++) memcpy(shim_module + kClassCode[i].rva, kClassCode[i].bytes, (size_t)kClassCode[i].n);
    for (int i = 0; i < MARKS_SPRITES; i++) g_mark[i].ok = 0;
    g_driftOn = 0; g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; g_mapShown = 0; g_map.ok = 0; g_driftNew = 1; g_destOk = 0;
    g_dispNew = 1; g_dispAt = 0; g_gameAt = 0; g_fillFull = 0; g_screenWatch = 0; g_zoomScreens = 0; g_zoomHalf = 0; g_destEndOk = 0;
    for (int k = 0; k < 2; k++) g_lnMs[k] = g_lnFreeze[k] = g_lnN[k] = 0;
    g_fullMs = 0; g_endFreezeMs = 0; g_screenLogin = 0; g_screenMap = 0; g_zoomStart = 0; g_holdOn = 0; g_holdMs = HOLD_MS; g_timed = 0; g_blurOn = 0; g_blurUsed = 0; g_blurFrom = g_blurTo = 0; g_dissolveAt = 0;
    memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);  /* 0,1 1,1 0,0 1,0, as at start */
    g_walls.count = 0;
    g_wallPick = 0;
    g_barsOn = 0;
    g_barShown = 0;
    shim_screen_w = 3440; shim_screen_h = 1440;
    shim_threads = 0;
}
static void Put(const char *rel, const char *text)
{
    char p[700];
    snprintf(p, sizeof p, "%s%s", shim_gamedir, rel);
    FILE *f = fopen(p, "wb");
    fputs(text, f);
    fclose(f);
}
static const char *Get(const char *rel)
{
    static char buf[16384];
    char p[700];
    snprintf(p, sizeof p, "%s%s", shim_gamedir, rel);
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}
static int Is(const char *rel, const char *text) { const char *g = Get(rel); return g && !strcmp(g, text); }
static int Gone(const char *rel) { return Get(rel) == NULL; }
static int On(void) { return Same(shim_module + PATCH_RVA, kWide, sizeof kWide); }
static int BarWide(void) { return Same(shim_module + BAR_FILL_RVA, kFillWide, 16) && Same(shim_module + BAR_BORDER_RVA, kBorderWide, 16); }
static int BarStock(void) { return Same(shim_module + BAR_FILL_RVA, kFillStock, 16) && Same(shim_module + BAR_BORDER_RVA, kBorderStock, 16); }
static void Start(void) { DllMain(NULL, DLL_PROCESS_ATTACH, NULL); }
static int LogSays(const char *words) { const char *g = Get("Logs/CinderLoad.log"); return g && strstr(g, words); }

#define UW "Data/CinderLoad/LoadingScreens-21x9.MPQ"
#define HD "Data/CinderLoad/LoadingScreens-16x9.MPQ"
#define ACTIVE "Data/patch-~.MPQ"
#define MARK "Data/patch-~.cinderload"
#define OLDMARK "Data/CinderLoad/installed.txt"
#define RES(w) "SET gxWindow \"1\"\r\nSET gxResolution \"" w "\"\r\n"

/* ---- the fire sound ---- */
#include <math.h>
#include <time.h>

static int Near(double a, double b, double tol) { return fabs(a - b) <= tol; }

/* A fake fmod.dll: every call is written down, in order. */
static char g_calls[4096];
static void Called(const char *what) { strcat(g_calls, what); strcat(g_calls, " "); }
static fn_StreamCallback g_cb;
static unsigned g_mode;
static int g_rate = 8000, g_pan = 999, g_createOk = 1, g_streamTag;
static void *WINAPI FakeCreate(fn_StreamCallback cb, int len, unsigned int mode, int rate, void *user)
{
    (void)len; (void)user; Called("create"); g_cb = cb; g_mode = mode; g_rate = rate; return g_createOk ? &g_streamTag : NULL;
}
static void *WINAPI FakeGetSample(void *st) { (void)st; return NULL; }
static int WINAPI FakePlayEx(int ch, void *st, void *dsp, signed char paused) { (void)ch; (void)st; (void)dsp; Called(paused ? "start-paused" : "start"); return 3; }
static signed char WINAPI FakePriority(int ch, int pri) { (void)ch; (void)pri; Called("priority"); return 1; }
static signed char WINAPI FakeReserved(int ch, signed char r) { (void)ch; Called(r ? "reserve" : "unreserve"); return 1; }
static signed char WINAPI FakePan(int ch, int pan) { (void)ch; g_pan = pan; Called("pan"); return 1; }
static signed char WINAPI FakeVolume(int ch, int v) { (void)ch; (void)v; Called("volume"); return 1; }
static signed char WINAPI FakePaused(int ch, signed char p) { (void)ch; Called(p ? "pause" : "unpause"); return 1; }
static signed char WINAPI FakeStop(int ch) { (void)ch; Called("stop"); return 1; }
static signed char WINAPI FakeClose(void *st) { (void)st; Called("close"); return 1; }
static int WINAPI FakeRate(void) { return 8000; }
static float WINAPI FakeVersion(void) { return 3.75f; }
static int WINAPI FakeError(void) { return 0; }
static void FakeFmod(void)
{
    F.Stream_Create = FakeCreate; F.Stream_GetSample = FakeGetSample; F.Sample_SetDefaults = NULL; F.Stream_PlayEx = FakePlayEx;
    F.SetPriority = FakePriority; F.SetReserved = FakeReserved; F.SetPan = FakePan; F.SetVolume = FakeVolume;
    F.SetPaused = FakePaused; F.StopSound = FakeStop; F.Stream_Close = FakeClose; F.GetOutputRate = FakeRate;
    F.GetVersion = FakeVersion; F.GetError = FakeError;
    g_calls[0] = 0;
}
static void Client(unsigned layer, float progress, int soundUp)
{
    memcpy(shim_module + FIRE_LAYER_RVA, &layer, 4);
    memcpy(shim_module + FIRE_PROGRESS_RVA, &progress, 4);
    shim_module[FIRE_SOUNDUP_RVA] = (unsigned char)soundUp;
}
static float Published(void) { union { LONG bits; float f; } v; v.bits = g_fire.fillBits; return v.f; }

/* A recipe of one loop that is a constant (a bed, or motion): what a mixer test hears is then only the levels, the pan
 * and the fade. */
static float g_dc[64];
static SndSpec g_dcSpec;
static SndSet g_dcSet;
static void DcMix(FireMix *m, int rate, int motion, float value, float sweep)
{
    memset(m, 0, sizeof *m);
    FireSetRate(m, rate);
    for (int i = 0; i < 64; i++) g_dc[i] = value;
    const char *r = motion ? "level 1\nloop motion 1 0 0 Sound\\a.wav\n" : "level 1\nloop bed 1 1 1 Sound\\a.wav\n";
    SndParse(&g_dcSpec, r, (unsigned)strlen(r));
    g_dcSpec.sweep = sweep;
    memset(&g_dcSet, 0, sizeof g_dcSet);
    g_dcSet.loop[0] = g_dc; g_dcSet.len[0] = 64; g_dcSet.n = 1;
}
static short g_pcm[2 * 48000];
/* frames rendered at one gate and fill; the last frame's left and right */
static void Run(FireMix *m, int gate, float fill, int frames, int *l, int *r)
{
    while (frames > 0) {
        int n = frames < 48000 ? frames : 48000;
        SndRender(m, &g_dcSet, &g_dcSpec, gate, fill, g_pcm, n);
        frames -= n;
        if (l) *l = g_pcm[2 * n - 2];
        if (r) *r = g_pcm[2 * n - 1];
    }
}
static size_t Wav(unsigned char *out, int format, int channels, int bits, int rate, const short *s, int frames)
{
    unsigned data = (unsigned)(frames * channels * bits / 8);
    unsigned char *p = out;
    memcpy(p, "RIFF", 4); p += 4; unsigned v = 36 + 12 + data; memcpy(p, &v, 4); p += 4; memcpy(p, "WAVE", 4); p += 4;
    memcpy(p, "LIST", 4); p += 4; v = 4; memcpy(p, &v, 4); p += 4; memcpy(p, "INFO", 4); p += 4;   /* a chunk to skip */
    memcpy(p, "fmt ", 4); p += 4; v = 16; memcpy(p, &v, 4); p += 4;
    unsigned short h[8] = { (unsigned short)format, (unsigned short)channels, (unsigned short)(rate & 0xFFFF), (unsigned short)(rate >> 16),
                            0, 0, (unsigned short)(channels * bits / 8), (unsigned short)bits };
    unsigned bps = (unsigned)(rate * channels * bits / 8);
    h[4] = (unsigned short)(bps & 0xFFFF); h[5] = (unsigned short)(bps >> 16);
    memcpy(p, h, 16); p += 16;
    memcpy(p, "data", 4); p += 4; memcpy(p, &data, 4); p += 4;
    memcpy(p, s, data); p += data;
    return (size_t)(p - out);
}

static void FireTests(void)
{
    int il, ir;

    /* the sweep: the old fire's pan curve, equal power, 80/20 by loudness at both ends at a sweep of 1 */
    Check(Near(FirePan(0), 0.295, 1e-6) && Near(FirePan(1), 0.705, 1e-6) && Near(FirePan(0.5f), 0.5, 1e-6) &&
          Near(FirePan(-1), 0.295, 1e-6) && Near(FirePan(2), 0.705, 1e-6), "pan: 0.295 at an empty bar, 0.705 at a full one");
    double worst = 0;
    for (int i = 0; i <= 1000; i++) {
        double x = i * (M_PI / 2) / 1000;
        worst = fmax(worst, fmax(fabs(FireSin((float)x) - sin(x)), fabs(FireCos((float)x) - cos(x))));
    }
    Check(worst < 1e-6 && Near(FireExp(-0.001), exp(-0.001), 1e-15) && Near(FireExp(-3), exp(-3), 1e-12),
          "the arithmetic without a C runtime: sin, cos and exp as the library's");

    /* the glide: a jump in the bar moves the sound over about a third of a second, never at once */
    FireMix m;
    DcMix(&m, 1000, 0, 0.1f, 1);
    Run(&m, 1, 0, 10, NULL, NULL);
    Run(&m, 1, 1, 1, NULL, NULL);
    float first = m.front;
    Run(&m, 1, 1, 299, NULL, NULL);
    float at300 = m.front;
    Run(&m, 1, 1, 50, NULL, NULL);
    Check(first < 0.005f && Near(at300, 1 - exp(-1), 0.01) && Near(m.front, 1 - exp(-0.35 / 0.3), 0.01),
          "glide: one frame after a jump the front has hardly moved; 63% at 0.3 s, 69% at 0.35 s");
    DcMix(&m, 1000, 0, 0.1f, 1);
    Run(&m, 1, 0, 1000, &il, &ir);
    double startRight = (double)ir * ir / ((double)il * il + (double)ir * ir);
    Run(&m, 1, 1, 3000, &il, &ir);
    double endRight = (double)ir * ir / ((double)il * il + (double)ir * ir);
    Check(Near(startRight, 0.2, 0.002) && Near(endRight, 0.8, 0.002), "sweep 1: the sound goes from 20% right to 80% right with the bar");
    DcMix(&m, 1000, 0, 0.1f, 0.5f);
    Run(&m, 1, 1, 3000, &il, &ir);
    double halfRight = (double)ir * ir / ((double)il * il + (double)ir * ir);
    DcMix(&m, 1000, 0, 0.1f, 0);
    Run(&m, 1, 1, 3000, &il, &ir);
    Check(halfRight > 0.6 && halfRight < 0.7 && il == ir, "sweep: a half sweep goes half as far; none, and the sound stays centred");
    DcMix(&m, 1000, 0, 0.1f, 1);
    Run(&m, 1, 0.7f, 1, NULL, NULL);
    Check(Near(m.front, 0.7, 1e-6), "glide: a screen starts where its bar is, with no sweep across");

    /* motion: loud while the fill moves, soft while it stalls */
    DcMix(&m, 1000, 1, 0.1f, 0);
    Run(&m, 1, 0, 500, NULL, NULL);
    for (int step = 0; step < 100; step++) Run(&m, 1, step * 0.005f, 20, NULL, NULL);   /* 0.25 a second, in 20 ms steps */
    float moving = FireCrackleLevel(m.speed);
    Run(&m, 1, 0.5f, 1, &il, &ir);
    double loudMoving = sqrt((double)il * il + (double)ir * ir);
    Run(&m, 1, 0.5f, 2000, &il, &ir);
    float stalled = FireCrackleLevel(m.speed);
    double loudStalled = sqrt((double)il * il + (double)ir * ir);
    Check(Near(moving, 1, 0.1) && stalled < 0.02f && loudMoving > 20 * loudStalled,
          "motion: at a quarter of the bar a second it is at 1; two seconds of stall and it has all but gone");
    DcMix(&m, 1000, 1, 0.1f, 0);
    Run(&m, 1, 0, 100, NULL, NULL);
    float peak = 0;
    for (int i = 0; i < 2000; i++) {
        Run(&m, 1, 1, 1, NULL, NULL);
        if (m.speed > peak) peak = m.speed;
    }
    Check(peak <= FIRE_MOVE_CAP + 1e-6f && FireCrackleLevel(10) == FIRE_MOVE_MAX, "motion: a leap of the whole bar is held to its cap");

    /* the fades */
    DcMix(&m, 1000, 0, 0.1f, 0);
    Run(&m, 1, 0.5f, 200, NULL, NULL);
    float half = m.env;
    Run(&m, 1, 0.5f, 200, NULL, NULL);
    float full = m.env;
    Run(&m, 0, 0.5f, 200, NULL, NULL);
    float out = m.env;
    int going = SndRender(&m, &g_dcSet, &g_dcSpec, 0, 0.5f, g_pcm, 199);
    int quiet = !SndRender(&m, &g_dcSet, &g_dcSpec, 0, 0.5f, g_pcm, 10) && g_pcm[18] == 0 && g_pcm[19] == 0;
    Check(Near(half, 0.5, 0.01) && Near(full, 1, 1e-6) && Near(out, 0.5, 0.01) && going && quiet,
          "fades: in over 0.4 s as a screen comes up, out over 0.4 s when it goes, then silence");
    /* the limit: as it was up to 0.8, then bending toward 1 and never past it, so a loud moment rounds off */
    int lim = SndLimit(0.5f) == 0.5f && SndLimit(-0.8f) == -0.8f && SndLimit(0.9f) > 0.8f && SndLimit(0.9f) < 0.9f &&
              SndLimit(5) < 1 && SndLimit(5) > SndLimit(2) && SndLimit(-5) > -1 && SndLimit(-5) == -SndLimit(5);
    DcMix(&m, 1000, 0, 3.0f, 0);
    Run(&m, 1, 1, 1000, &il, &ir);
    Check(lim && il > 0.9f * 32767 * 0.70710678f && il < 32767 && il == ir,
          "limit: up to 0.8 untouched, past it bent toward 1; a mix three times too loud stays below full scale, never wrapping");

    /* the fill the worker passes on */
    FireFill ff;
    FireFillStart(&ff, 0);
    float a = FireFillRead(&ff, 0.5f), b = FireFillRead(&ff, 0), c = FireFillRead(&ff, 0.2f), d = FireFillRead(&ff, 0.55f);
    Check(a == 0.5f && b == 0.5f && c == 0.5f && d == 0.55f, "fill: the client's passing 0 (and part sums) never reach the sound");
    FireFillRead(&ff, 0.05f); FireFillRead(&ff, 0.05f);
    float e = FireFillRead(&ff, 0.05f);
    Check(e == 0.05f, "fill: a fall that lasts three reads is a new screen in the same window, and is taken");
    Check(FireFillRead(&ff, NAN) == 0.05f && FireFillRead(&ff, 1.7f) == 1, "fill: NaN counts as 0, over 1 as 1");

    /* the loops, made as the mock made them */
    static float src[4000], loop[9000];
    for (int i = 0; i < 1000; i++) src[i] = 0.5f;
    unsigned period = FireMakeLoop(src, 1000, 1000, 1000, 0, loop);
    int flat = period == 1000 - 120;
    for (unsigned i = 0; i < period; i++) flat &= Near(loop[i], 0.5, 1e-6);
    Check(flat, "loop: n - 0.12 s long, and a steady sound stays steady through the seam");
    for (int i = 0; i < 1000; i++) src[i] = (float)i;
    period = FireMakeLoop(src, 1000, 1000, 1000, 300, loop);
    Check(period == 880 && loop[120] == 420 && loop[500] == 800 && loop[700] == 0 && loop[0] == 180,
          "loop: started 300 frames in, as the mock's roll, with the seam laid over that start");
    period = FireMakeLoop(src, 1000, 1000, 2000, 0, loop);
    int line = period == 2000 - 240;
    for (unsigned j = 240; j < period; j++) line &= Near(loop[j], j / 2.0, 1e-3);
    Check(line, "loop: 1 kHz to 2 kHz by straight lines between frames");
    Check(FireResampledLength(66152, 22050, 44100) == 132304 && FireResampledLength(66152, 22050, 48000) == 144004,
          "loop: as many frames at the mixer's rate as the mock's resampler makes");

    /* a recipe (7 Oct): its lines read carefully; a bed rising with the fill, a swell silent until its start and whole by
     * its end, stingers once each as the fill passes their marks (one at 0 as it begins), sparks more often as it fills */
    {
        static const char tab[] = "# a recipe\nlevel 0.8632\nsweep 0.6\nloop bed 0.950 0.000 1.000 Sound\\Ambience\\a.wav\n"
            "loop swell 0.550 0.150 0.600 x0.8 Sound\\Doodad\\MoonWellLightLoop.wav\r\nloop swell 0.700 0.450 0.950 X1.25 Sound\\b.wav\n"
            "loop motion 0.350 0.000 0.000 Sound\\Spells\\c.wav\nloop hum 1 0 1 Sound\\x.wav\nloop bed 1 0 1 x9 Sound\\y.wav\n"
            "loop bed 9 0 1 Sound\\y.wav\nspark 0.5 0 1 Sound\\s.wav\nstinger 0.6 0 Sound\\Spells\\Start.wav\n"
            "stinger 0.900 0.970 Sound\\Spells\\Tranquility.wav\nstinger 1 0.5 Sound\\t3.wav\nstinger 1 0.6 Sound\\t4.wav\n";
        static SndSpec sp;
        int ok = SndParse(&sp, tab, sizeof tab - 1) && sp.n == 4 && sp.stings == 3 && sp.sparks == 1 && Near(sp.level, 0.8632, 1e-4) &&
                 Near(sp.sweep, 0.6, 1e-6) && sp.l[0].kind == SND_BED && sp.l[1].kind == SND_SWELL && sp.l[3].kind == SND_MOTION &&
                 Near(sp.l[1].from, 0.15, 1e-6) && Near(sp.l[1].speed, 0.8, 1e-6) && Near(sp.l[2].speed, 1.25, 1e-6) &&
                 Near(sp.l[0].speed, 1, 0) && Near(sp.l[2].to, 0.95, 1e-6) && !strcmp(sp.l[1].path, "Sound\\Doodad\\MoonWellLightLoop.wav") &&
                 !strcmp(sp.sting[1].path, "Sound\\Spells\\Tranquility.wav") && Near(sp.sting[1].from, 0.97, 1e-6) &&
                 sp.sting[0].from == 0 && !strcmp(sp.spark[0].path, "Sound\\s.wav");
        ok &= !SndParse(&sp, "loop bed 1 0 1 Sound\\a.wav\n", 27);                      /* no level: none */
        Check(ok, "recipe: its lines read (CRLF, comments, a speed; an unknown kind, a speed out of range, a gain over 4 and a fourth stinger skipped); no level, no sound");

        static float one[100], sting[50], spk[20];
        for (int i = 0; i < 100; i++) one[i] = 1;
        for (int i = 0; i < 50; i++) sting[i] = 1;
        for (int i = 0; i < 20; i++) spk[i] = 1;
        static SndSet st;
        memset(&st, 0, sizeof st);
        SndParse(&sp, tab, sizeof tab - 1);
        sp.sweep = 0; sp.sparks = 0; sp.stings = 2;
        sp.sting[0].from = 0.99f;                                                  /* (both stingers late, for the bed) */
        for (int i = 0; i < 4; i++) { st.loop[i] = one; st.len[i] = 100; }
        st.n = 4; st.sting[0] = st.sting[1] = sting; st.stingLen[0] = st.stingLen[1] = 50;
        SndReset(&st);
        FireMix mx;
        memset(&mx, 0, sizeof mx);
        FireSetRate(&mx, 1000);
        mx.env = 1;                                                                /* past the fade */
        static short pcm[2 * 500];
        SndRender(&mx, &st, &sp, 1, 0.0f, pcm, 50);                                 /* still: no motion */
        float at0 = pcm[2 * 49] / 32767.0f;
        int centred = pcm[2 * 49] == pcm[2 * 49 + 1];
        for (int k = 0; k < 12; k++) SndRender(&mx, &st, &sp, 1, 0.1f, pcm, 400);   /* the glide settled: still again */
        float at10 = pcm[2 * 399] / 32767.0f;
        float want0 = 0.95f * 0.55f * 0.8632f * 0.70710678f;
        float bed10 = 0.55f + 0.45f * SndSmooth(0, 1, 0.1f), want10 = 0.95f * bed10 * 0.8632f * 0.70710678f;
        ok = centred && Near(at0, want0, 2e-4) && Near(at10, want10, 2e-3) && st.stingState[1] == 0;
        SndRender(&mx, &st, &sp, 1, 0.97f, pcm, 1);                                 /* the mark: the stinger starts */
        int playing = st.stingState[1] == 1 && st.stingState[0] == 0;
        SndRender(&mx, &st, &sp, 1, 0.98f, pcm, 60);
        int played = st.stingState[1] == 2;
        SndRender(&mx, &st, &sp, 1, 0.99f, pcm, 60);
        ok &= playing && played && st.stingState[1] == 2 && st.stingState[0] >= 1;   /* each once, each at its own mark */
        SndReset(&st);
        ok &= st.stingState[0] == 0 && st.stingState[1] == 0 && st.pos[0] == 0;
        Check(ok, "recipe: the bed at 0.55 rising, the swells silent before their start, each stinger once at its own mark, centred");

        sp.sting[0].from = 0;                                                      /* a stinger at 0: as the screen begins */
        SndReset(&st);
        mx.env = 0;
        SndRender(&mx, &st, &sp, 1, 0.0f, pcm, 1);
        int begun = st.stingState[0] == 1;
        /* sparks: ten seconds at an empty bar, then ten at a full one */
        sp.stings = 0; sp.sparks = 1; sp.spark[0].from = 0.2f; sp.spark[0].to = 0.8f;
        st.spark[0] = spk; st.sparkLen[0] = 20;
        SndReset(&st);
        int slow = 0, fast = 0;
        for (int k = 0; k < 10000; k++) { int was = st.sparkOn[0]; SndRender(&mx, &st, &sp, 1, 0.0f, pcm, 1); slow += !was && st.sparkOn[0]; }
        for (int k = 0; k < 10000; k++) { int was = st.sparkOn[0]; SndRender(&mx, &st, &sp, 1, 1.0f, pcm, 1); fast += !was && st.sparkOn[0]; }
        int closed = 0;
        for (int k = 0; k < 3000; k++) { int was = st.sparkOn[0]; SndRender(&mx, &st, &sp, 0, 1.0f, pcm, 1); closed += !was && st.sparkOn[0]; }
        Check(begun && slow >= 3 && slow <= 7 && fast >= 14 && fast <= 30 && closed == 0,
              "recipe: a stinger at 0 plays as the screen begins; sparks every 2.4 s or so at an empty bar, every half second at a full one, none once it has gone");
    }
    Check(FireMakeLoop(src, 200, 1000, 1000, 0, loop) == 0, "loop: one too short to cross-fade is refused");

    /* the source files */
    static unsigned char wav[20000];
    static short pcm[4000];
    for (int i = 0; i < 4000; i++) pcm[i] = (short)(i * 8);
    int rate = 0, ch = 0;
    unsigned frames = 0;
    const unsigned char *data = NULL;
    size_t n = Wav(wav, 1, 1, 16, 22050, pcm, 2000);
    int ok = FireWavInfo(wav, (unsigned)n, &rate, &ch, &data, &frames) && rate == 22050 && ch == 1 && frames == 2000;
    FireWavRead(data, ch, frames, src);
    Check(ok && Near(src[1000], 8000 / 32768.0, 1e-7), "wav: 16-bit mono PCM, as the client's fire loops are");
    n = Wav(wav, 1, 2, 16, 44100, pcm, 1000);
    ok = FireWavInfo(wav, (unsigned)n, &rate, &ch, &data, &frames) && ch == 2 && frames == 1000;
    FireWavRead(data, ch, frames, src);
    Check(ok && Near(src[10], (160 + 168) / 2 / 32768.0, 1e-7), "wav: stereo is taken as the average of its sides");
    Check(!FireWavInfo(wav, (unsigned)Wav(wav, 1, 1, 8, 22050, pcm, 2000), &rate, &ch, &data, &frames) &&
          !FireWavInfo(wav, (unsigned)Wav(wav, 0x11, 1, 16, 22050, pcm, 2000), &rate, &ch, &data, &frames) &&
          !FireWavInfo(wav, (unsigned)Wav(wav, 1, 1, 16, 22050, pcm, 2000) - 2, &rate, &ch, &data, &frames) &&
          !FireWavInfo((const unsigned char *)"RIFF\4\0\0\0WAVE", 12, &rate, &ch, &data, &frames),
          "wav: 8-bit, ADPCM, a cut-off file and one with no sound in it are refused");

    /* the places the sound relies on: all must hold their bytes, or no sound at all */
    Fresh("fire-on");
    Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(shim_threads == 1 && LogSays("sound on:") && On() && BarWide(), "the client's bytes all there: the fire's thread starts");
    int silent = 1;
    for (int i = 0; i < FIRE_CODE_COUNT; i++) {
        Fresh("fire-other");
        Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
        shim_module[kFireCode[i].rva + (unsigned)kFireCode[i].n - 1] ^= 0x40;
        Start();
        char words[80];
        snprintf(words, sizeof words, "other bytes at RVA 0x%lX (%s)", (unsigned long)kFireCode[i].rva, kFireCode[i].what);
        silent &= shim_threads == 0 && LogSays(words) && On() && BarWide();
    }
    Check(silent, "any one of the eight places changed: no thread, no sound, the log names it; screens and bar unchanged");

    /* the stream's life, against a fake fmod.dll */
    Fresh("fire-stream");
    GameDir();                                              /* the log goes to this folder */
    FakeFmod();
    memset(&g_fire, 0, sizeof g_fire);
    g_fire.base = shim_module;
    g_fire.channel = -1;
    DcMix(&g_fire.mix, 8000, 0, 0.1f, 0);
    g_sndSpec = g_dcSpec; g_snd = g_dcSet; g_sndNow = SOUND_HEARTH; g_sndRate = 8000; g_sndBad = 0;   /* the default, made */
    g_fire.built = 1;
    shim_ticks = 1000;
    Client(0x1234, 0.1f, 0);
    int unwatched = !g_screenWatch;
    FireStep(shim_ticks);
    Check(!g_calls[0] && !g_fire.stream, "sound engine down (sound off in the game): a loading screen gets no FMOD call");
    Check(unwatched && g_screenWatch, "the worker's first look: from then on screens ending are watched for (no fallback resets)");
    Client(0, 0, 1);
    FireStep(shim_ticks);
    Check(!g_calls[0], "no loading screen: no FMOD call");
    Client(0x1234, 0.1f, 1);
    FireStep(shim_ticks);
    Check(!strcmp(g_calls, "create start-paused priority reserve pan volume unpause ") && g_pan == FSOUND_STEREOPAN &&
          g_mode == (FSOUND_16BITS | FSOUND_SIGNED | FSOUND_STEREO | FSOUND_2D) && g_rate == 8000 && g_fire.gate == 1 &&
          Published() == 0.1f, "a loading screen comes up: one stereo stream, our own pan, started silent and let go");
    g_calls[0] = 0;
    Client(0x1234, 0, 1); FireStep(shim_ticks += 20);
    float held = Published();
    Client(0x1234, 0.6f, 1); FireStep(shim_ticks += 20);
    Check(held == 0.1f && Published() == 0.6f && !g_calls[0], "while it shows: the fill passed on, a passing 0 ignored, no FMOD calls");
    static short buf[2 * FIRE_BLOCK_FRAMES];
    signed char more = g_cb(&g_streamTag, buf, (int)sizeof buf, NULL);
    Check(more == 1 && buf[2 * FIRE_BLOCK_FRAMES - 2] != 0 && !g_fire.quiet, "FMOD asks for sound: it gets the fire, fading in");
    Client(0, 0.6f, 1); FireStep(shim_ticks += 20);
    Check(g_fire.gate == 0 && g_fire.stream && !g_calls[0] && LogSays("its sound (hearth) played for") && LogSays("the bar reached 60%"),
          "the screen goes: the sound fades out, and the log says which played and for how long");
    for (int i = 0; i < 5; i++) g_cb(&g_streamTag, buf, (int)sizeof buf, NULL);
    FireStep(shim_ticks += 20);
    Check(!strcmp(g_calls, "unreserve stop close ") && !g_fire.stream && g_fire.quiet, "faded out: the stream closed, the channel given back");
    g_calls[0] = 0;
    Client(0x1234, 0, 1); FireStep(shim_ticks += 20);
    Client(0, 1, 1); FireStep(shim_ticks += 20);
    FireStep(shim_ticks += 1000);
    Check(!strstr(g_calls, "close"), "a fade FMOD has not played yet: the stream is left to finish");
    FireStep(shim_ticks += 600);
    Check(strstr(g_calls, "close") && !g_fire.stream, "but closed after 1.5 s at most");
    g_calls[0] = 0;
    Client(0x1234, 0.2f, 1); FireStep(shim_ticks += 20);
    Client(0x1234, 0.2f, 0); FireStep(shim_ticks += 20);
    Client(0, 0.2f, 0); FireStep(shim_ticks += 2000);
    Check(!strcmp(g_calls, "create start-paused priority reserve pan volume unpause ") && !g_fire.stream &&
          LogSays("closed its sound engine"), "the game closes its sound engine mid-screen: the stream is forgotten, never called");
    g_calls[0] = 0;
    g_createOk = 0;
    Client(0x1234, 0.2f, 1); FireStep(shim_ticks += 20);
    Client(0x1234, 0.4f, 1); FireStep(shim_ticks += 20);
    Check(!strcmp(g_calls, "create ") && !g_fire.stream && LogSays("would not make the stream"),
          "the engine will not make the stream: logged, silent for that screen");
    g_createOk = 1;
    g_calls[0] = 0;
    Client(0, 0.4f, 1); FireStep(shim_ticks += 20);
    Client(0x1234, 0, 1); FireStep(shim_ticks += 20);
    Check(strstr(g_calls, "create start-paused") && g_fire.stream, "and tried again at the next screen");

    Fresh("fire-no-files");
    GameDir();                                              /* the log goes to this folder */
    FakeFmod();
    memset(&g_fire, 0, sizeof g_fire);
    memset(&g_snd, 0, sizeof g_snd); g_sndNow = -1; g_sndBad = 0; g_sndCycle = 0;
    g_fire.base = shim_module;
    g_fire.channel = -1;
    Client(0x1234, 0.1f, 1);
    FireStep(shim_ticks += 20);
    Client(0x1234, 0.2f, 1);
    FireStep(shim_ticks += 20);
    Check(!g_calls[0] && g_fire.playing == -1 && LogSays("none of hearth's loops") && g_sndBad == 1u << SOUND_HEARTH,
          "the client's sounds are not to be had: logged once, no FMOD call, the screens as ever");
    g_snd.n = 0; g_sndNow = -1; g_sndBad = 0;

    /* the recipes he auditions (7 Oct): every one reads, with the client's own paths; sound.txt picks, and the cycles move
     * on a screen at a time, kept across sessions */
    int recipes = 1, named = 0;
    for (int k = 0; k < SOUNDS; k++) {
        if (!kSounds[k].text[0]) continue;
        static SndSpec rs;
        named++;
        recipes &= SndParse(&rs, kSounds[k].text, (unsigned)strlen(kSounds[k].text)) && rs.n >= 2 && rs.level > 0;
        for (int i = 0; i < rs.n; i++) recipes &= !strncmp(rs.l[i].path, "Sound\\", 6);
        if (k >= SOUND_CLASS0) recipes &= !strcmp(kSounds[k].name, kClassName[k - SOUND_CLASS0]);
    }
    Check(recipes && named == 12, "the recipes: the hum, the Hearthstone, the portal and the nine classes', each two loops or more of the client's own");
    Fresh("sound-pick");
    GameDir();
    g_sndCycle = 0;
    const char *w;
    int pk = SoundPick(9, 1, &w) == SOUND_HEARTH && !strcmp(w, "hearth");          /* no file: the Hearthstone's (his pick) */
    Put("Data/CinderLoad/sound.txt", "portal\r\n");
    pk &= SoundPick(9, 1, &w) == SOUND_PORTAL;
    Put("Data/CinderLoad/sound.txt", "HEARTH  # the Hearthstone\n");
    pk &= SoundPick(9, 1, &w) == SOUND_HEARTH && !strcmp(w, "hearth");
    Put("Data/CinderLoad/sound.txt", "class");
    pk &= SoundPick(9, 1, &w) == SOUND_CLASS0 + 9 && SoundPick(0, 1, &w) == SOUND_HEARTH;   /* a warlock's; no class: hearth */
    Put("Data/CinderLoad/sound.txt", "warlock\n");
    pk &= SoundPick(3, 1, &w) == SOUND_CLASS0 + 9 && !strcmp(w, "warlock");              /* that class's, whoever plays */
    Put("Data/CinderLoad/sound.txt", "fire\n");
    pk &= SoundPick(9, 1, &w) == SOUND_HEARTH && g_sndCycle == 0;                       /* a word it does not know */
    Put("Data/CinderLoad/sound.txt", "hum\n");
    pk &= SoundPick(9, 1, &w) == SOUND_HUM;
    Check(pk, "sound.txt: none, or a word it does not know, is the Hearthstone's (his pick, 7 Oct); hum, portal, hearth (any case), class (the character's), or a class by name");
    Put("Data/CinderLoad/sound.txt", "cycle\n");
    int seq[6];
    for (int i = 0; i < 6; i++) seq[i] = SoundPick(9, 1, &w);
    int look = SoundPick(9, 0, &w), cyc = seq[0] == SOUND_HUM && seq[1] == SOUND_HEARTH && seq[2] == SOUND_PORTAL &&
               seq[3] == SOUND_CLASS0 + 9 && seq[4] == SOUND_HUM && seq[5] == SOUND_HEARTH && look == SOUND_PORTAL &&
               SoundPick(9, 0, &w) == SOUND_PORTAL && g_sndCycle == 6;
    char ct[1024] = "";
    FILE *cf = fopen((snprintf(ct, sizeof ct, "%sData/CinderLoad/sound-cycle.txt", shim_gamedir), ct), "rb");
    int six = 0;
    if (cf) { six = fgetc(cf) == '6'; fclose(cf); }
    g_sndCycle = 0;
    SoundCycleLoad();
    cyc &= six && g_sndCycle == 6 && SoundPick(9, 1, &w) == SOUND_PORTAL;                 /* the next session goes on */
    Check(cyc, "sound.txt cycle: hum, hearth, portal, the class's, round again, a screen at a time; looking ahead moves nothing; kept across sessions");
    Put("Data/CinderLoad/sound.txt", "classes\n");
    g_sndCycle = 0;
    unsigned seen = 0;
    for (int i = 0; i < 9; i++) seen |= 1u << (SoundPick(1, 1, &w) - SOUND_CLASS0);
    Check(seen == (1u << 1 | 1u << 2 | 1u << 3 | 1u << 4 | 1u << 5 | 1u << 7 | 1u << 8 | 1u << 9 | 1u << 11) &&
          SoundPick(1, 1, &w) == SOUND_CLASS0 + 1, "sound.txt classes: all nine classes' in nine screens, whoever plays, then round again");
    remove(ct);
    g_sndCycle = 0;
}

/* ---- the fire bar ---- */

/* A small MPQ (format 0, files stored as they are), written the way Storm lays one out, for the DLL's own reader. */
static unsigned g_crypt[0x500];
static void CryptInit(void)
{
    unsigned seed = 0x00100001u;
    for (unsigned i = 0; i < 0x100; i++)
        for (unsigned j = 0; j < 5; j++) {
            seed = (seed * 125u + 3u) % 0x2AAAABu;
            unsigned hi = (seed & 0xFFFFu) << 16;
            seed = (seed * 125u + 3u) % 0x2AAAABu;
            g_crypt[i + j * 0x100] = hi | (seed & 0xFFFFu);
        }
}
static unsigned HashName(const char *s, unsigned type)
{
    unsigned s1 = 0x7FED7FEDu, s2 = 0xEEEEEEEEu;
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
        s1 = g_crypt[(type << 8) + ch] ^ (s1 + s2);
        s2 = ch + s1 + s2 + (s2 << 5) + 3u;
    }
    return s1;
}
static void Encrypt(unsigned *d, unsigned n, unsigned key)
{
    unsigned seed = 0xEEEEEEEEu;
    for (unsigned i = 0; i < n; i++) {
        seed += g_crypt[0x400 + (key & 0xFFu)];
        unsigned plain = d[i];
        d[i] = plain ^ (key + seed);
        key = ((~key << 21) + 0x11111111u) | (key >> 11);
        seed = plain + seed + (seed << 5) + 3u;
    }
}
#define MPQ_FILES 32                                     /* (16 until the glow's pack needed more, 4 Oct 2026) */
static void MakeMpq(const char *rel, int n, const char *const *names, const char *const *data, const unsigned *lens)
{
    static unsigned char out[1 << 20];
    if (n > MPQ_FILES) { printf("FAIL: a test archive of %d files, more than MPQ_FILES\n", n); exit(1); }
    unsigned hash[64 * 4], block[MPQ_FILES * 4], at = 32;
    memset(hash, 0xFF, sizeof hash);
    for (int i = 0; i < n; i++) {
        unsigned len = lens ? lens[i] : (unsigned)strlen(data[i]);
        memcpy(out + at, data[i], len);
        block[4 * i] = at; block[4 * i + 1] = len; block[4 * i + 2] = len; block[4 * i + 3] = 0x80000000u;
        at += len;
        unsigned slot = HashName(names[i], 0) & 63u;
        while (hash[4 * slot + 3] != 0xFFFFFFFFu) slot = (slot + 1) & 63u;
        hash[4 * slot] = HashName(names[i], 1); hash[4 * slot + 1] = HashName(names[i], 2);
        hash[4 * slot + 2] = 0; hash[4 * slot + 3] = (unsigned)i;
    }
    Encrypt(hash, 64 * 4, HashName("(hash table)", 3));
    Encrypt(block, (unsigned)n * 4, HashName("(block table)", 3));
    unsigned hashOff = at, blockOff = at + sizeof hash, size = blockOff + (unsigned)n * 16;
    memcpy(out + hashOff, hash, sizeof hash);
    memcpy(out + blockOff, block, (size_t)n * 16);
    unsigned head[8] = { 0x1A51504Du, 32, size, 3u << 16, hashOff, blockOff, 64, (unsigned)n };   /* "MPQ\x1A", v0, 4 KB */
    memcpy(out, head, 32);
    char p[700];
    snprintf(p, sizeof p, "%s%s", shim_gamedir, rel);
    FILE *f = fopen(p, "wb");
    fwrite(out, 1, size, f);
    fclose(f);
}

#define TEX "Interface\\Glues\\LoadingBar\\"
static const char kBars[] =
    "# CinderLoad: each loading screen's fire\r\n"
    "\r\n"
    "loadscreendeadmines fel\r\n"
    "LoadScreenMoltenCore.blp   Aqua   # its lava is aqua in this test\r\n"
    "loadscreenstrat orange\r\n"
    "loading fel\r\n"
    "default arcane\r\n";
/* Our pack, as make_pack.py builds it with fire bars: fel, aqua and arcane carried, orange only named. */
static void PackWithBars(const char *bars)
{
    const char *names[] = { "Interface\\Glues\\LoadingScreens\\LoadScreenDeadmines.blp", "CinderLoad\\bars.txt",
                            TEX "Loading-BarFill-fel.blp", TEX "Loading-BarBorder-fel.blp",
                            TEX "Loading-BarFill-aqua.blp", TEX "Loading-BarBorder-aqua.blp",
                            TEX "Loading-BarFill-arcane.blp", TEX "Loading-BarBorder-arcane.blp" };
    const char *data[] = { "BLP2screen", bars, "BLP2f", "BLP2b", "BLP2f", "BLP2b", "BLP2f", "BLP2b" };
    MakeMpq(UW, bars ? 8 : 1, names, data, NULL);
    Put("WTF/Config.wtf", RES("5120x2160"));
}

/* The client's tables, as the game has them while a screen begins: Map.dbc's and LoadingScreens.dbc's records by id. */
#define VA(rva) (0x400000u + (unsigned)(rva))
static void Poke(unsigned va, unsigned v) { memcpy(shim_module + va - 0x400000u, &v, 4); }
static void PokeText(unsigned va, const char *s) { memcpy(shim_module + va - 0x400000u, s, strlen(s) + 1); }
/* Written from the facts read out of WoW.exe, not from the DLL's names for them, so a slip in one shows. */
static void FakeTables(void)
{
    Poke(0xC0DAA8, VA(0x860000)); Poke(0xC0DAAC, 600);   /* Map.dbc's records by id; the largest id */
    Poke(0xC0DB0C, VA(0x861000)); Poke(0xC0DB10, 50);    /* LoadingScreens.dbc's */
    static const struct { int map, screen; const char *file; } k[] = {
        { 36, 5, "Interface\\Glues\\LoadingScreens\\LoadScreenDeadmines.blp" },
        { 409, 6, "Interface\\Glues\\LoadingScreens\\LoadScreenMoltenCore.blp" },
        { 0, 7, "Interface\\Glues\\LoadingScreens\\LoadScreenEasternKingdom.blp" },
        { 329, 8, "Interface\\Glues\\LoadingScreens\\LoadScreenStrat.blp" },
        { 3, 99, NULL },                                   /* a LoadingScreenID past the table's end */
        { 4, 9, NULL },                                    /* a LoadingScreenID with no record */
        { 5, -1, NULL },                                   /* a negative one */
        { 6, 10, "Interface\\Glues\\LoadingScreens\\LoadScreenWithANameMuchLongerThanAnyBarsTxtCouldList.blp" },
        { 600, 50, "Interface\\Glues\\LoadingScreens\\LoadScreenMoltenCore.blp" },   /* the largest ids: in range */
        { 601, 6, NULL },                                  /* one past Map.dbc's largest id, a record there all the same */
        { 7, 51, "Interface\\Glues\\LoadingScreens\\LoadScreenMoltenCore.blp" },     /* one past LoadingScreens' */
    };
    for (int i = 0; i < (int)(sizeof k / sizeof k[0]); i++) {
        unsigned map = VA(0x862000 + 0x100 * i), rec = VA(0x864000 + 0x20 * i), file = VA(0x866000 + 0x100 * i);
        Poke(VA(0x860000) + 4u * (unsigned)k[i].map, map);
        Poke(map + 0x98, (unsigned)k[i].screen);           /* the Map record's LoadingScreenID */
        if (!k[i].file) continue;
        Poke(VA(0x861000) + 4u * (unsigned)k[i].screen, rec);
        Poke(rec + 8, file);                               /* the LoadingScreens record's file name */
        PokeText(file, k[i].file);
    }
}
static const char *GameFill(void) { return (const char *)shim_module + BAR_FILL_PATH_RVA; }
static const char *GameBorder(void) { return (const char *)shim_module + BAR_BORDER_PATH_RVA; }
static const char *ForMap(int map) { Poke(0x82F00C, (unsigned)map); return BarPath(GameFill()); }
static int Ours(const char *p) { return p >= (const char *)&g_bars && p < (const char *)(&g_bars + 1); }
static int Call(void) { return Rd32(shim_module + BAR_CALL_RVA + 1); }
static int CallStock(void) { return Same(shim_module + BAR_LOAD_RVA, kBarLoad, (int)sizeof kBarLoad); }

static void FireBarTests(void)
{
    CryptInit();

    /* on: the call sent to the stub, every other byte as it was */
    Fresh("firebar-on");
    PackWithBars(kBars);
    Start();
    unsigned rel = (unsigned)((unsigned long)BarStub - (unsigned long)(shim_module + 0x6891 + 5));
    Check(LogSays("fire bar on") && (unsigned)Call() == rel && shim_module[0x6891] == 0xE8 &&
          Same(shim_module + BAR_LOAD_RVA, kBarLoad, BAR_REL_AT) && shim_module[0x6896] == 0x89,
          "fire bar on: the rel32 at RVA 0x6891 is stub - (RVA 0x6891 + 5 + base), the E8 and every other byte kept");
    Check(g_barLoader == shim_module + 0x49D90, "the stub goes on to the game's loader, VA 0x449D90");
    Check(On() && BarWide() && shim_threads == 1 && LogSays("sound on:"), "the screens, the bar's size and the fire sound as ever");
    Check(LogSays("bars.txt names 4 screens, and a default for the rest, 0 lines skipped") &&
          LogSays("families in the pack: fel, aqua, arcane; named but not in it (their screens keep the pack's orange): orange"),
          "the log says what bars.txt named and which families the pack carries");

    /* which bar each screen gets, from the client's own tables */
    FakeTables();
    const char *p = ForMap(36);
    Check(Ours(p) && !strcmp(p, TEX "Loading-BarFill-fel"), "Deadmines (map 36): fel");
    Check(!strcmp(BarPath(GameBorder()), TEX "Loading-BarBorder-fel"), "and its border: fel too");
    Check(!strcmp(ForMap(409), TEX "Loading-BarFill-aqua"), "Molten Core, named LoadScreenMoltenCore.blp in bars.txt: aqua");
    char shown[BAR_SHOWN_MAX];
    BarShownSince(shown, (int)sizeof shown);
    Check(!strcmp(shown, ", its fire aqua") && g_barShown == 0, "the worker's line says which fire the screen had, once");
    Check(!strcmp(ForMap(0), TEX "Loading-BarFill-arcane"), "a continent bars.txt does not list: the default family");
    Check(ForMap(329) == GameFill(), "Stratholme, orange, whose files the pack does not carry: the game's own path back");
    BarShownSince(shown, (int)sizeof shown);
    Check(!strcmp(shown, ", its fire the pack's orange"), "and the worker's line says so");
    /* the longest line there can be (the pack's orange and wallpaper 64), and a buffer too small: nothing written past
     * the end (3 Oct 2026: a 48-byte buffer was overrun on a screen with a wallpaper) */
    unsigned char guard[BAR_SHOWN_MAX + 16];
    int keepCount = g_walls.count; LONG keepPick = g_wallPick;
    g_walls.count = 64; g_wallPick = 64;
    lstrcpyA(g_walls.path[63], "Interface\\Glues\\LoadingScreens\\CinderWall64.blp");
    memset(guard, 0xAA, sizeof guard);
    InterlockedExchange(&g_barShown, -1);
    BarShownSince((char *)guard, BAR_SHOWN_MAX);
    int fits = !strcmp((char *)guard, ", its fire the pack's orange, its wallpaper CinderWall64.blp");
    for (int i = BAR_SHOWN_MAX; i < (int)sizeof guard; i++) fits &= guard[i] == 0xAA;
    memset(guard, 0xAA, sizeof guard);
    InterlockedExchange(&g_barShown, -1);
    BarShownSince((char *)guard, 20);
    int cut = strlen((char *)guard) == 19 && guard[19] == 0;
    for (int i = 20; i < (int)sizeof guard; i++) cut &= guard[i] == 0xAA;
    g_walls.count = keepCount; g_wallPick = keepPick;
    Check(fits && cut, "the worker's line: the longest one fits, and a small buffer is cut short, never overrun");
    Check(!strcmp(ForMap(-1), TEX "Loading-BarFill-fel"), "map -1 (no screen of its own): the default screen, 'loading'");
    Check(!strcmp(ForMap(601), TEX "Loading-BarFill-fel") && !strcmp(ForMap(-5), TEX "Loading-BarFill-fel"),
          "a map id past either end of Map.dbc: the default screen");
    Check(!strcmp(ForMap(2), TEX "Loading-BarFill-fel"), "a map with no Map record: the default screen");
    Check(!strcmp(ForMap(3), TEX "Loading-BarFill-fel") && !strcmp(ForMap(4), TEX "Loading-BarFill-fel") &&
          !strcmp(ForMap(5), TEX "Loading-BarFill-fel"), "a LoadingScreenID past the end, negative, or with no record: the default screen");
    Check(!strcmp(ForMap(6), TEX "Loading-BarFill-arcane"), "a screen whose name is too long to be listed: the default family");
    Check(!strcmp(ForMap(600), TEX "Loading-BarFill-aqua"), "the largest map id and LoadingScreenID: in range, as the game's jg");
    Check(!strcmp(ForMap(601), TEX "Loading-BarFill-fel") && !strcmp(ForMap(7), TEX "Loading-BarFill-fel"),
          "one past the largest of either: the default screen, whatever lies there");
    Check(Rd32(kBarMapSet + 5) == BAR_MAP_VA && Rd32(kBarMapSet2 + 1) == BAR_MAP_VA && Rd32(kBarPick + 1) == BAR_MAP_VA &&
          Rd32(kBarPickWalk + 4) == BAR_MAPS_MAX_VA && Rd32(kBarPickWalk + 12) == BAR_MAPS_VA &&
          Rd32(kBarPickWalk + 25) == BAR_MAP_SCREEN && Rd32(kBarPickWalk + 35) == BAR_SCREENS_MAX_VA &&
          Rd32(kBarPickWalk + 43) == BAR_SCREENS_VA && kBarPickWalk[56] == BAR_SCREEN_FILE && Rd32(kBarLoad + BAR_REL_AT) == 0x434FA &&
          BAR_CALL_RVA + 5 + 0x434FA == 0x49D90, "every address the lookup reads is the one in the verified code it mirrors");
    Poke(0xC0DAA8, 0x100);
    Check(!strcmp(ForMap(36), TEX "Loading-BarFill-fel"), "Map.dbc's table unreadable: the default screen, nothing read past it");
    FakeTables();
    Poke(VA(0x864000) + 8, 0x200);
    Check(!strcmp(ForMap(36), TEX "Loading-BarFill-fel"), "a file name the game could not read either: the default screen");
    FakeTables();
    Poke(0x82F00C, 36);
    static const char spark[] = "Interface\\Glues\\LoadingBar\\Loading-BarSpark";
    Check(BarPath(spark) == spark && BarPath(NULL) == NULL && BarPath(TEX "Loading-BarFill-fel") != NULL &&
          !strcmp(BarPath(TEX "Loading-BarFill-fel"), TEX "Loading-BarFill-fel"),
          "a path that is neither of the bar's two: returned as it came");
    Check(!strcmp(BarPath("interface/glues/loadingbar/LOADING-BARFILL"), TEX "Loading-BarFill-fel"),
          "the bar's path in other case or slashes: still the bar's");
    g_barsOn = 0;
    Check(ForMap(36) == GameFill(), "with the redirect off, the game's own path back");
    g_barsOn = 1;

    Start();
    Check(LogSays("fire bar already on") && (unsigned)Call() == rel, "a second start in the same process: already on, left as it is");

    /* any one place changed: no redirect, the log names it and what it found */
    int stock = 1;
    for (int i = 0; i < BAR_CODE_COUNT; i++) {
        Fresh("firebar-other");
        PackWithBars(kBars);
        shim_module[kBarCode[i].rva + (unsigned)kBarCode[i].n - 1] ^= 0x40;
        Start();
        char words[200], there[200];
        snprintf(words, sizeof words, "fire bar off: other bytes at RVA 0x%lX (%s)", (unsigned long)kBarCode[i].rva, kBarCode[i].what);
        int len = snprintf(there, sizeof there, "there:");
        for (int k = 0; k < 8 && k < kBarCode[i].n; k++) len += snprintf(there + len, sizeof there - (size_t)len, " %02X", shim_module[kBarCode[i].rva + (unsigned)k]);
        int callKept = kBarCode[i].rva == BAR_LOAD_RVA ? Same(shim_module + BAR_LOAD_RVA, kBarLoad, (int)sizeof kBarLoad - 1)
                                                       : CallStock();
        stock &= callKept && !g_barsOn && LogSays(words) && LogSays(there) && On() && BarWide() && shim_threads == 1 &&
                 LogSays("sound on:");
        if (!(callKept && LogSays(words))) printf("  (place %d)\n", i);
    }
    Check(stock, "any one of the seven places changed: the call left alone, the log names it and its bytes; screens, bar, sound as ever");
    Fresh("firebar-other-rel");
    PackWithBars(kBars);
    shim_module[BAR_CALL_RVA + 2] = 0x35;
    Start();
    Check(shim_module[BAR_CALL_RVA + 2] == 0x35 && Rd32(shim_module + BAR_CALL_RVA + 1) == 0x000435FA &&
          LogSays("RVA 0x6863 (the bar's textures being loaded)"), "the call going somewhere else already: left alone");

    /* never without our screens and the switch */
    Fresh("firebar-no-screens");
    Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(CallStock() && !LogSays("fire bar"), "no loading screens of ours: the call never touched");
    Fresh("firebar-other-exe");
    PackWithBars(kBars);
    shim_module[PATCH_RVA + 1] = 0x11;
    Start();
    Check(CallStock() && !On() && !LogSays("fire bar"), "a client the switch is not for: the call never touched");
    Fresh("firebar-foreign");
    PackWithBars(kBars);
    Put(ACTIVE, "FOREIGN");
    Start();
    Check(CallStock() && !LogSays("fire bar"), "someone else's patch-~.MPQ: the call never touched");

    /* His call, 4 Oct 2026: the class's own fire. bars.txt names classes as if they were screens; a class it names burns
     * its fire on every screen (the druid's orange: the pack's own), a class it does not name or no class known yet
     * keeps the picture's, and none of it when the character list's code has moved. */
    {
        static const char bars[] = "loadscreendeadmines fel\nclassmage aqua\nclassdruid orange\nclasswarlock arcane\n"
                                   "default arcane\n";
        char said[BAR_SHOWN_MAX];
        Fresh("firebar-class");
        PackWithBars(bars);
        Start();
        FakeTables();
        Check(g_barClassOk && LogSays("fire bar: each class burns its own fire where bars.txt names one (mage, warlock, druid)"),
              "the class's fire: on, and the log names the classes bars.txt gives a fire");
        Poke(CHAR_COUNT_VA, 0);
        Check(!strcmp(ForMap(36), TEX "Loading-BarFill-fel"), "no class known yet: the picture's fire (the Deadmines, fel)");
        BarShownSince(said, (int)sizeof said);
        Check(!strcmp(said, ", its fire fel"), "and the worker's line names no class");
        Poke(CHAR_COUNT_VA, 3); Poke(CHAR_LIST_VA, 0x8A0000); Poke(CHAR_PICKED_VA, 1);
        shim_module[0x8A0000 - 0x400000 + 0 * 0x120 + 0x101] = 11;   /* a druid, a mage, a hunter */
        shim_module[0x8A0000 - 0x400000 + 1 * 0x120 + 0x101] = 8;
        shim_module[0x8A0000 - 0x400000 + 2 * 0x120 + 0x101] = 3;
        Check(!strcmp(ForMap(36), TEX "Loading-BarFill-aqua") && !strcmp(BarPath(GameBorder()), TEX "Loading-BarBorder-aqua"),
              "a mage at the login: the mage's aqua on the Deadmines, whose picture is fel; its border too");
        BarShownSince(said, (int)sizeof said);
        Check(!strcmp(said, ", its fire aqua, the mage's"), "and the worker's line says it was the mage's");
        Check(!strcmp(ForMap(0), TEX "Loading-BarFill-aqua"), "a mage: aqua on a continent, whose picture takes the default");
        Poke(CHAR_COUNT_VA, 0);
        Check(!strcmp(ForMap(409), TEX "Loading-BarFill-aqua"), "the list gone once in the world: the mage's fire kept");
        Poke(CHAR_COUNT_VA, 3); Poke(CHAR_PICKED_VA, 0);
        Check(ForMap(36) == GameFill(), "a druid: the pack's own orange, even on the Deadmines");
        BarShownSince(said, (int)sizeof said);
        Check(!strcmp(said, ", its fire the pack's orange, the druid's"), "and the worker's line says it was the druid's");
        Poke(CHAR_PICKED_VA, 2);
        Check(!strcmp(ForMap(36), TEX "Loading-BarFill-fel") && !strcmp(ForMap(0), TEX "Loading-BarFill-arcane"),
              "a hunter, whom bars.txt does not name: each picture's own fire");
        BarShownSince(said, (int)sizeof said);
        Check(!strcmp(said, ", its fire arcane"), "and no class in the worker's line");

        Fresh("firebar-class-moved");
        PackWithBars(bars);
        shim_module[kClassCode[3].rva + 3] ^= 0x02;                    /* the class at another offset */
        Start();
        FakeTables();
        Poke(CHAR_COUNT_VA, 3); Poke(CHAR_LIST_VA, 0x8A0000); Poke(CHAR_PICKED_VA, 1);
        Check(!g_barClassOk && LogSays("bars.txt names the classes' fires, but the character list is not where") &&
              !strcmp(ForMap(36), TEX "Loading-BarFill-fel") && g_class == 0,
              "the character list's code moved: no class read, every bar its picture's fire, and the log says why");

        Fresh("firebar-class-none");
        PackWithBars(kBars);
        Start();
        Check(!g_barClassOk && !LogSays("each class burns") && !LogSays("classes' fires"),
              "a bars.txt with no class lines: nothing said about classes, the pictures' fires as before");
    }

    /* a pack made for the garland (retired 7 Oct 2026): its "style <family> garland" lines are skipped like any other
     * line that is not two words, and the druid's family burns as any fire does, at the log's size */
    {
        static const char bars[] = "default fel\nclassdruid druid\nclasshunter fel\nstyle druid garland\nstyle fel bonfire\n";
        static const char wt[] = "# wisps\ns 0 0 0 160 160\ns 1 162 0 418 96\ns 4 164 162 260 356\ns 5 262 162 298 198\n"
                                 "s 6 402 162 450 210\ns 3 98 162 162 226\ns 9 0 0 10 10\ns 5 500 500 600 600\n";
        const char *names[] = { "Interface\\Glues\\LoadingScreens\\LoadScreenDeadmines.blp", "CinderLoad\\bars.txt",
                                "CinderLoad\\reveal.txt", TEX "Loading-BarFill-fel.blp", TEX "Loading-BarBorder-fel.blp",
                                TEX "Loading-BarFill-druid.blp", TEX "Loading-BarBorder-druid.blp", TEX "Loading-BarBack-druid.blp",
                                TEX "Loading-BarWisps-druid.blp", "CinderLoad\\wisps-druid.txt", TEX "Loading-BarBlooms-druid.blp",
                                "CinderLoad\\blooms-druid.txt" };
        const char *data[] = { "BLP2screen", bars, "revealed", "BLP2f", "BLP2b", "BLP2f", "BLP2b", "BLP2back", "BLP2wisps", wt,
                               "BLP2blooms", "# blooms\ncell 128\ntypes 4\nframes 8\n" };
        Fresh("garland");
        MakeMpq(UW, 12, names, data, NULL);
        Put("WTF/Config.wtf", RES("5120x2160"));
        Start();
        FakeTables();
        int fd = -1, ff = -1;
        for (int f = 0; f < g_bars.families; f++) {
            if (!strcmp(g_bars.family[f].name, "druid")) fd = f;
            if (!strcmp(g_bars.family[f].name, "fel")) ff = f;
        }
        int gl = fd >= 0 && ff >= 0 && g_bars.family[fd].ok && g_bars.family[ff].ok && g_bars.skipped == 2 &&
                 LogSays("2 lines skipped") && !LogSays("garland");
        unsigned char *fill = shim_module + BAR_FILL_RVA, *border = shim_module + BAR_BORDER_RVA;
        Poke(CHAR_COUNT_VA, 2); Poke(CHAR_LIST_VA, 0x8A0000); Poke(CHAR_PICKED_VA, 0);
        shim_module[0x8A0000 - 0x400000 + 0 * 0x120 + 0x101] = 11;       /* a druid, a hunter */
        shim_module[0x8A0000 - 0x400000 + 1 * 0x120 + 0x101] = 3;
        gl &= !strcmp(ForMap(36), TEX "Loading-BarFill-druid") && Same(fill, kBorderWide, 16) && Same(border, kBorderWide, 16);
        Poke(CHAR_PICKED_VA, 1);                                           /* the hunter: fel, the log's size */
        gl &= !strcmp(ForMap(36), TEX "Loading-BarFill-fel") && Same(fill, kBorderWide, 16) && Same(border, kBorderWide, 16);
        Check(gl, "a pack made for the garland: its style lines skipped, the druid's bar its family's fire at the log's size, as the hunter's");

        /* his teleports (5 Oct, Moonglade and the hearth back): a later load the game fills in a third of a second, its
         * bar then on the plan's 4.2 s. The fill's front is the bar as shown on the game's frame (RevealFill), and on the
         * view's the blade's tip rides it */
        {
            Poke(CHAR_PICKED_VA, 0);                                        /* the druid again */
            ForMap(36);
            g_mapShown = 1;
            { const char *mt = "map 0 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\n"; MapParse(mt, (unsigned)strlen(mt)); }
            g_map.ok = 1;
            Poke(BAR_MAP_VA, 0);
            Poke(MAP_DEST_MAP_VA, 0);
            { float x = -5000.0f, y = 10000.0f; unsigned u; memcpy(&u, &x, 4); Poke(MAP_DEST_X_VA, u); memcpy(&u, &y, 4); Poke(MAP_DEST_Y_VA, u); }
            g_driftOn = 1; g_screenWatch = 1; g_holdMs = 5000;
            g_lnMs[1] = 950; g_lnFreeze[1] = 800; g_lnN[1] = 3;            /* a quick machine's later loads: the plan 4.2 s */
            g_zoomScreens = 1;
            DriftScreenGone();
            /* the game's frame: a second of them, its bar full from 0.3 s */
            float vg[12] = { 0, 0, 0, 0, 0, 0, 0, 0.18f, 0, 0, 0.18f, 0 };
            for (int k = 0; k <= 50; k++) {
                float h = k * 20 < 300 ? k * 20 / 300.0f : 1.0f;
                unsigned u; memcpy(&u, &h, 4); Poke(0x882BE4, u);
                vg[3] = vg[9] = h;
                shim_ticks += 20; RevealFill(vg);
            }
            float front = g_fillVerts[6 * (g_fillCount / 2 - 1)];          /* the fill's own front */
            int lt = g_disp == 1.0f && front > 0.15f && front < 0.35f;      /* the game's full, the bar as shown not */
            /* the view's frames (7 ms), the next teleport: one bar for every class (his call, 6 Oct 2026), the blade, its
             * tip at the bar as shown */
            InitializeCriticalSection(&g_stepLock);
            g_viewOn = 1; g_ovOn = 1;
            for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
            g_viewTex[VT_SPARK] = 1;
            DriftScreenGone();
            { float h = 0; unsigned u; memcpy(&u, &h, 4); Poke(0x882BE4, u); }
            DWORD t = shim_ticks + 3000;
            GameStep(0.0f, t);                                              /* the game's first frame begins it */
            for (int k = 1; k <= 145; k++) {
                t += 7;
                ViewStep(k * 7 < 300 ? k * 7 / 300.0f : 1.0f, t, 5120, 2160);
            }
            int foreign = 0, tipAt = -1;
            for (int i = 1; i < g_vbN; i++) foreign += g_vb[i].tex != -1 && g_vb[i].tex != VT_SPARK;
            for (int i = 1; i < g_vbN; i++) if (g_vb[i].tex == VT_SPARK && g_vb[i].add && tipAt < 0) tipAt = g_vb[i].first;
            lt &= g_viewSteps == 1 && g_disp == 1.0f && g_viewShown > 0.15f && g_viewShown < 0.35f && foreign == 0 && tipAt >= 0 &&
                  fabsf((g_vv[tipAt].x + g_vv[tipAt + 1].x) / 2 - g_viewShown * 5120) < 0.5f;
            g_viewOn = 0; g_viewSteps = 0; g_ovOn = 0; g_mapShown = 0; g_map.ok = 0; g_driftOn = 0; g_screenWatch = 0;
            g_zoomScreens = 0; g_destEndOk = 0; g_holdMs = HOLD_MS; g_lnN[1] = 0;
            for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
            Check(lt, "his teleports: a later load the game fills in 0.3 s, its bar on the plan's 4.2 s: the game's frame draws its fill's front at the bar as shown; the view draws the blade alone, its tip at the bar as shown");
        }
    }

    /* bars.txt missing, broken or with nothing the pack carries: no redirect, logged */
    Fresh("firebar-no-bars");
    PackWithBars(NULL);
    Start();
    Check(CallStock() && On() && BarWide() && shim_threads == 1 && LogSays("Data\\patch-~.MPQ holds no CinderLoad\\bars.txt"),
          "a pack with no bars.txt: no redirect, logged, all else as ever");
    Fresh("firebar-not-mpq");
    Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(CallStock() && On() && LogSays("could not be read from Data\\patch-~.MPQ (malformed archive header"), "a pack the reader cannot open: no redirect");
    Fresh("firebar-garbage");
    PackWithBars("\x01\x02\x03 \x7F\xFF\xFE\nonly-one-word\n\n");
    Start();
    Check(CallStock() && LogSays("2 lines skipped") && LogSays("no family bars.txt names"), "a bars.txt of garbage: no redirect");
    Fresh("firebar-no-files");
    PackWithBars("loadscreendeadmines orange\ndefault green\n");
    Start();
    Check(CallStock() && LogSays("families in the pack: none") && LogSays("no family bars.txt names"),
          "families whose textures are not in the pack: no redirect");
    Fresh("firebar-too-big");
    {
        static char big[BAR_TEXT_MAX + 100];
        memset(big, '#', sizeof big - 1);
        PackWithBars(big);
    }
    Start();
    Check(CallStock() && LogSays("is over 64 KB"), "a bars.txt over 64 KB: refused, no redirect");
    Fresh("firebar-half");
    {
        const char *names[] = { "CinderLoad\\bars.txt", TEX "Loading-BarFill-fel.blp", TEX "Loading-BarFill-aqua.blp",
                                TEX "Loading-BarBorder-aqua.blp" };
        const char *data[] = { "loadscreendeadmines fel\ndefault aqua\n", "BLP2", "BLP2", "BLP2" };
        MakeMpq(UW, 4, names, data, NULL);
        Put("WTF/Config.wtf", RES("5120x2160"));
    }
    Start();
    FakeTables();
    Check(LogSays("fire bar on") && LogSays("in the pack: aqua; named but not in it (their screens keep the pack's orange): fel") &&
          ForMap(36) == GameFill() && BarPath(GameBorder()) == GameBorder() && !strcmp(ForMap(0), TEX "Loading-BarFill-aqua"),
          "a family with its fill but no border: never returned, its screens keep the pack's orange");

    /* bars.txt, read tolerantly */
    static BarTable t;
    BarParse(&t, kBars, (unsigned)strlen(kBars));
    Check(t.screens == 4 && t.families == 4 && t.skipped == 0 && t.fallback == 3 && !strcmp(t.screen[1].name, "loadscreenmoltencore") &&
          !strcmp(t.family[t.screen[1].family].name, "aqua"), "bars.txt: comments, blank lines, CRLF, case and a .blp ending");
    static const char made[] =                       /* make_pack.py's own output, as it writes bars.txt */
        "# CinderLoad: the fire of each loading screen's bar, \"<screen> <family>\"; \"default\" is every screen\n"
        "# not listed. A family whose two textures are not in this archive keeps the orange bar. (make_pack.py)\n"
        "loading orange\nloadscreendeadmines fel\nloadscreenmoltencore aqua\ndefault orange\n";
    BarParse(&t, made, (unsigned)strlen(made));
    Check(t.screens == 3 && t.families == 3 && t.skipped == 0 && t.fallback == 0, "bars.txt as make_pack.py writes it");
    static const char odd[] = "loadscreena fel\n"
                              "loadscreenb fel-green\n"                       /* not letters and digits */
                              "loadscreenc fel green\n"                       /* three words */
                              "loadscreend\n"                                 /* one */
                              "loadscreene sixteencharacters\n"               /* a family name over 15 */
                              "loadscreenaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa fel\n"   /* a name over 47 */
                              "loadscreenf\tAQUA\n"
                              "loadscreena aqua\n"                            /* a later line wins */
                              "loadscreeng fel";                              /* no line end at the end */
    BarParse(&t, odd, (unsigned)strlen(odd));
    Check(t.screens == 3 && t.skipped == 5 && t.fallback == -1 && !strcmp(t.family[t.screen[0].family].name, "aqua") &&
          !strcmp(t.screen[2].name, "loadscreeng"), "bars.txt: bad lines skipped and counted, a later line wins, no default");
    static char longLine[600];
    snprintf(longLine, sizeof longLine, "loadscreena fel # %0*d\ndefault fel\n", 300, 0);
    BarParse(&t, longLine, (unsigned)strlen(longLine));
    Check(t.screens == 0 && t.skipped == 1 && t.fallback == 0, "bars.txt: a line over 200 characters skipped whole, never cut");
    static const char nul[] = "loadscreena fe\0l\ndefault fel\n";
    BarParse(&t, nul, (unsigned)sizeof nul - 1);
    Check(t.screens == 0 && t.skipped == 1 && t.fallback == 0, "bars.txt: a NUL inside a line: that line skipped");
    static char many[40000];
    int len = 0;
    for (int i = 0; i < 300; i++) len += snprintf(many + len, sizeof many - (size_t)len, "screen%d f%d\n", i, i % 20);
    BarParse(&t, many, (unsigned)len);
    Check(t.families == BAR_FAMILIES && t.screens <= BAR_SCREENS && t.skipped == 300 - t.screens,
          "bars.txt: more families or screens than there is room for: the rest skipped, nothing overrun");

    /* no default line: a screen it does not list keeps the pack's orange */
    Fresh("firebar-no-default");
    PackWithBars("loadscreendeadmines fel\n");
    Start();
    FakeTables();
    Check(LogSays("fire bar on") && LogSays("no default") && !strcmp(ForMap(36), TEX "Loading-BarFill-fel") &&
          ForMap(0) == GameFill() && ForMap(-1) == GameFill(), "no default line: unlisted screens and the default one keep the pack's orange");
}

/* A real client's WoW.exe: the file offset of an RVA, from its section table. -1 when no section holds it. */
static long ExeOffset(FILE *x, unsigned rva)
{
    unsigned char h[64], sec[40];
    if (fseek(x, 0, SEEK_SET) || fread(h, 1, 64, x) != 64) return -1;
    unsigned pe = Rd32(h + 60);
    unsigned char fh[24];
    if (fseek(x, (long)pe, SEEK_SET) || fread(fh, 1, 24, x) != 24 || memcmp(fh, "PE\0\0", 4)) return -1;
    unsigned count = (unsigned)fh[6] | (unsigned)fh[7] << 8, opt = (unsigned)fh[20] | (unsigned)fh[21] << 8;
    for (unsigned i = 0; i < count; i++) {
        if (fseek(x, (long)(pe + 24 + opt + 40 * i), SEEK_SET) || fread(sec, 1, 40, x) != 40) return -1;
        unsigned va = Rd32(sec + 12), raw = Rd32(sec + 16), ptr = Rd32(sec + 20);
        if (rva >= va && rva < va + raw) return (long)(ptr + rva - va);
    }
    return -1;
}
static void FireBarRealClient(const char *client)
{
    char exe[1024];
    snprintf(exe, sizeof exe, "%s/WoW.exe", client);
    FILE *x = fopen(exe, "rb");
    int match = x != NULL;
    for (int i = 0; x && i < BAR_CODE_COUNT; i++) {
        unsigned char got[64];
        long off = ExeOffset(x, kBarCode[i].rva);
        match &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, (size_t)kBarCode[i].n, x) == (size_t)kBarCode[i].n &&
                 !memcmp(got, kBarCode[i].bytes, (size_t)kBarCode[i].n);
        if (!match) { printf("FAIL: RVA 0x%lX (%s) differs in %s\n", (unsigned long)kBarCode[i].rva, kBarCode[i].what, exe); break; }
    }
    for (int i = 0; x && match && i <= REVEAL_CODE_COUNT; i++) {   /* the reveal's five places, then the wallpaper call */
        DWORD rva = i < REVEAL_CODE_COUNT ? kRevealCode[i].rva : WALL_LOAD_RVA;
        int len = i < REVEAL_CODE_COUNT ? kRevealCode[i].n : (int)sizeof kWallLoad;
        const unsigned char *want = i < REVEAL_CODE_COUNT ? kRevealCode[i].bytes : kWallLoad;
        unsigned char got[64];
        long off = ExeOffset(x, rva);
        match &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, (size_t)len, x) == (size_t)len && !memcmp(got, want, (size_t)len);
        if (!match) printf("FAIL: RVA 0x%lX differs in %s\n", (unsigned long)rva, exe);
    }
    if (x && match) {                                               /* the drift's one place */
        unsigned char got[64];
        long off = ExeOffset(x, BG_DRAW_RVA);
        match &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, sizeof kBgDraw, x) == sizeof kBgDraw &&
                 !memcmp(got, kBgDraw, sizeof kBgDraw);
        if (!match) printf("FAIL: RVA 0x%lX (the background's texture coordinates) differs in %s\n", (unsigned long)BG_DRAW_RVA, exe);
    }
    for (int i = 0; x && match && i < OVERLAY_CALLS; i++) {         /* the overlay's six calls */
        unsigned char got[5];
        long off = ExeOffset(x, kOverlayCalls[i].rva);
        unsigned rel;
        match &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, 5, x) == 5 && got[0] == 0xE8;
        memcpy(&rel, got + 1, 4);
        match &= kOverlayCalls[i].rva + 5 + rel == kOverlayCalls[i].target - 0x400000u;
        if (!match) printf("FAIL: RVA 0x%lX (%s) does not call 0x%X in %s\n", (unsigned long)kOverlayCalls[i].rva,
                           kOverlayCalls[i].what, kOverlayCalls[i].target, exe);
    }
    for (int i = 0; x && match && i < CLASS_CODE; i++) {             /* the badge's class: where its addresses come from */
        unsigned char got[8];
        long off = ExeOffset(x, kClassCode[i].rva);
        match &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, (size_t)kClassCode[i].n, x) == (size_t)kClassCode[i].n &&
                 !memcmp(got, kClassCode[i].bytes, (size_t)kClassCode[i].n);
        if (!match) printf("FAIL: RVA 0x%lX (%s) differs in %s\n", (unsigned long)kClassCode[i].rva, kClassCode[i].what, exe);
    }
    for (int i = 0; x && match && i <= 3; i++) {                     /* the hold: the frame's start, its three calls */
        unsigned rva = (i == 3 ? HOLD_FRAME_VA : kHoldCall[i]) - 0x400000u;
        unsigned char got[9];
        long off = ExeOffset(x, rva);
        match &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, 9, x) == 9;
        if (i == 3) match &= !memcmp(got, kHoldFrame, sizeof kHoldFrame);
        else {
            unsigned rel;
            memcpy(&rel, got + 1, 4);
            match &= got[0] == 0xE8 && rva + 5 + rel == HOLD_FRAME_VA - 0x400000u;
        }
        if (!match) printf("FAIL: RVA 0x%X (the hold's %s) differs in %s\n", rva, i == 3 ? "loading frame" : "call", exe);
    }
    if (x && match) {                                               /* the present, where the ending is drawn */
        unsigned char got[sizeof kPresent];
        long off = ExeOffset(x, PRESENT_VA - 0x400000u);
        match &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, sizeof got, x) == sizeof got &&
                 !memcmp(got, kPresent, sizeof kPresent);
        if (!match) printf("FAIL: VA 0x%X (the present) differs in %s\n", PRESENT_VA, exe);
    }
    if (x && match) {                                               /* the glue music's stop */
        unsigned char got[sizeof kGlueStop];
        long off = ExeOffset(x, GLUE_STOP_VA - 0x400000u);
        match &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, sizeof got, x) == sizeof got &&
                 !memcmp(got, kGlueStop, sizeof kGlueStop);
        if (!match) printf("FAIL: VA 0x%X (the glue music's stop) differs in %s\n", GLUE_STOP_VA, exe);
    }
    for (int i = 0; x && match && i < 2; i++) {                       /* the arrival's height: read and written as taken */
        unsigned char got[8];
        long off = ExeOffset(x, kDestZCode[i].rva);
        match &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, (size_t)kDestZCode[i].n, x) == (size_t)kDestZCode[i].n &&
                 !memcmp(got, kDestZCode[i].bytes, (size_t)kDestZCode[i].n);
        if (!match) printf("FAIL: RVA 0x%lX (the arrival's height) differs in %s\n", (unsigned long)kDestZCode[i].rva, exe);
    }
    if (x) fclose(x);
    Check(match, "a real client: all eight places hold the bytes the fire bar and the drift expect, in its WoW.exe (and the arrival's height is read and written where it is taken)");
}

/* The mock's timeline (render_frames.py KEYS) through the sound built from a real client's files, polled as the game
 * would be: the fill changes every 20 ms. Written as 16-bit stereo for listening; a few checks on the way. */
/* The case's Data folder given every archive of a real client (symlinks, read only), named as the DLL asks for them. */
static int LinkClient(const char *client)
{
    char from[1024], to[1024];
    DIR *dd = NULL;
    snprintf(from, sizeof from, "%s/Data", client);
    if (!(dd = opendir(from))) { printf("FAIL: could not read %s\n", from); fails++; return 0; }
    for (struct dirent *de; (de = readdir(dd));) {
        size_t nl = strlen(de->d_name);
        if (nl < 5 || strcasecmp(de->d_name + nl - 4, ".mpq")) continue;
        char name[256];
        snprintf(name, sizeof name, "%.*s.MPQ", (int)(nl - 4), de->d_name);
        if (!strncasecmp(name, "patch-", 6) && name[6]) name[6] = (char)toupper((unsigned char)name[6]);
        snprintf(from, sizeof from, "%s/Data/%s", client, de->d_name);
        snprintf(to, sizeof to, "%sData/%s", shim_gamedir, name);
        if (symlink(from, to)) { printf("FAIL: could not link %s\n", from); fails++; closedir(dd); return 0; }
    }
    closedir(dd);
    return 1;
}
static void FireRealClient(const char *client)
{
    static const float keys[][2] = { { 0, 0 }, { 0.6f, 0.02f }, { 1.6f, 0.15f }, { 2.8f, 0.18f }, { 3.1f, 0.42f }, { 5.8f, 0.55f },
                                     { 6.2f, 0.80f }, { 9.6f, 0.94f }, { 11.2f, 1.04f }, { 12.5f, 1.04f } };
    /* the places the sound relies on, against the real WoW.exe on disk (its .text sits at file offset = RVA) */
    char exe[1024];
    snprintf(exe, sizeof exe, "%s/WoW.exe", client);
    FILE *x = fopen(exe, "rb");
    int match = x != NULL;
    for (int i = 0; x && i < FIRE_CODE_COUNT; i++) {
        unsigned char got[32];
        match &= !fseek(x, (long)kFireCode[i].rva, SEEK_SET) && fread(got, 1, (size_t)kFireCode[i].n, x) == (size_t)kFireCode[i].n &&
                 !memcmp(got, kFireCode[i].bytes, (size_t)kFireCode[i].n);
        if (!match) { printf("FAIL: RVA 0x%lX (%s) differs in %s\n", (unsigned long)kFireCode[i].rva, kFireCode[i].what, exe); break; }
    }
    if (x) fclose(x);
    Check(match, "a real client: all eight places hold the bytes the sound expects, in its WoW.exe");

    Fresh("fire-real");
    GameDir();                                              /* the log goes to this folder */
    if (!LinkClient(client)) return;
    /* every recipe, from this client's own files, over the timeline: all its sounds made, it builds, it never clips;
     * each written out for listening (sound-<name>.wav), with its loudness to set the recipes' levels against */
    static short pcm[2 * 12 * 44100 + 2 * 44100];
    static unsigned char out[sizeof pcm + 64];
    int total = (int)(12.5 * 44100), pollFrames = 882, all = 1, builds = 1, clean = 1;
    for (int k = 0; k < SOUNDS; k++) {
        if (!kSounds[k].text[0]) continue;
        memset(&g_fire, 0, sizeof g_fire);
        g_sndBad = 0;
        int made = SoundBuild(k, 44100), want = g_sndSpec.n, extra = g_sndSpec.sparks + g_sndSpec.stings, have = 0;
        for (int i = 0; i < g_sndSpec.n; i++) have += g_snd.loop[i] != NULL;
        for (int i = 0; i < g_sndSpec.sparks; i++) have += g_snd.spark[i] != NULL;
        for (int i = 0; i < g_sndSpec.stings; i++) have += g_snd.sting[i] != NULL;
        if (!made || have != want + extra) { printf("FAIL: recipe %s: %d of %d sounds made\n", kSounds[k].name, have, want + extra); all = 0; continue; }
        FireReset(&g_fire.mix);
        SndReset(&g_snd);
        int done = 0;
        for (int poll = 0; done < total; poll++) {
            float t = poll * 0.02f, fill = 0;
            for (int q = 0; q + 1 < 10; q++)
                if (t >= keys[q][0] && t <= keys[q + 1][0]) fill = keys[q][1] + (keys[q + 1][1] - keys[q][1]) * (t - keys[q][0]) / (keys[q + 1][0] - keys[q][0]);
            int n = total - done < pollFrames ? total - done : pollFrames;
            SndRender(&g_fire.mix, &g_snd, &g_sndSpec, t < 11.6f, fill, pcm + 2 * done, n);
            done += n;
        }
        double early = 0, late = 0;
        int peak = 0;
        for (int i = 1 * 44100; i < 3 * 44100; i++) early += (double)pcm[2 * i] * pcm[2 * i] + (double)pcm[2 * i + 1] * pcm[2 * i + 1];
        for (int i = 8 * 44100; i < 11 * 44100; i++) late += (double)pcm[2 * i] * pcm[2 * i] + (double)pcm[2 * i + 1] * pcm[2 * i + 1];
        for (int i = 0; i < 2 * total; i++) if (abs(pcm[i]) > peak) peak = abs(pcm[i]);
        double rmsLate = sqrt(late / (2.0 * 3 * 44100)) / 32767;
        builds &= late / 3 > early / 2;                                  /* its last stretch louder than its start */
        clean &= peak < 32767;
        printf("sound %-8s rms(8-11 s) %.3f, peak %.3f, last stretch %.1f times the start in power\n", kSounds[k].name, rmsLate,
               peak / 32767.0, early > 0 ? (late / 3) / (early / 2) : 0);
        char path[1024];
        snprintf(path, sizeof path, "%s/sound-%s.wav", g_root, kSounds[k].name);
        size_t len = Wav(out, 1, 2, 16, 44100, pcm, total);
        FILE *f = fopen(path, "wb");
        if (f) { fwrite(out, 1, len, f); fclose(f); }
    }
    const char *why;
    unsigned at1 = 0, at08 = 0;
    float *s1 = SndLoad("Sound\\Spells\\PreCastNatureMagicHigh.wav", 44100, 1, 0, &at1, &why);
    float *s08 = SndLoad("Sound\\Spells\\PreCastNatureMagicHigh.wav", 44100, 0.8f, 0, &at08, &why);
    Check(s1 && s08 && Near((double)at08 / at1, 1.25, 0.001), "a real client: a sound at x0.8 lasts a quarter longer (the Hearthstone's whir, stretched)");
    if (s1) HeapFree(GetProcessHeap(), 0, s1);
    if (s08) HeapFree(GetProcessHeap(), 0, s08);
    SoundFree();
    Check(all, "a real client: every recipe's every sound made from its own files");
    Check(builds && clean, "a real client: every recipe builds as the bar fills (its last stretch louder than its start) and never clips");
}

/* ---- tonight's three: the fire revealed with a soft, live front, and the continents' wallpapers ---- */
static void PutFloat(unsigned va, float f) { memcpy(shim_module + va - 0x400000u, &f, 4); }
static int g_testFrames;
static float g_testFade;
static void WINAPI TestGlueStop(float s) { g_testFade = s; Poke(GLUE_MUSIC_VA, 0); }
static void TestHoldFrame(void)                         /* the game's loading frame, as the hold sees it: 16 ms, the bar full */
{
    shim_ticks += 16; g_testFrames++;
    float sh = DisplayStep(1.0f, shim_ticks);
    DriftStep(sh, shim_ticks);
}
static unsigned FBits(float f) { unsigned u; memcpy(&u, &f, 4); return u; }
/* flow.bin for the tests: 4 x 4, the equator at eq, every cell the same current (cx, cv) times 0.01 */
static unsigned char kFlowBin[16 + 32];
static void FlowBinMake(float eq, int cx, int cv)
{
    float scale = 0.01f;
    memcpy(kFlowBin, "CFLW", 4); kFlowBin[4] = 4; kFlowBin[5] = 0; kFlowBin[6] = 4; kFlowBin[7] = 0;
    memcpy(kFlowBin + 8, &eq, 4); memcpy(kFlowBin + 12, &scale, 4);
    for (int i = 0; i < 16; i++) { kFlowBin[16 + 2 * i] = (unsigned char)(signed char)cx; kFlowBin[17 + 2 * i] = (unsigned char)(signed char)cv; }
}
/* glow.bin for the tests: map 0, 100-yard cells from x 1000, y 2000, 4 columns along y by 3 rows along x; the middle
 * two cells of the middle row are the first subzone, the cell after them the second */
static unsigned char kGlowBin[6 + 18 + 24];
static void GlowBinMake(void)
{
    unsigned char *d = kGlowBin;
    float cell = 100, x0 = 1000, y0 = 2000;
    memcpy(d, "CGLW", 4); d[4] = 1; d[5] = 0;
    d[6] = 0; d[7] = 0; memcpy(d + 8, &cell, 4); memcpy(d + 12, &x0, 4); memcpy(d + 16, &y0, 4); d[20] = 4; d[21] = 0; d[22] = 3; d[23] = 0;
    unsigned short cells[12] = { 0, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0 };
    memcpy(d + 24, cells, sizeof cells);
}
/* sea.bin for the tests: 4 x 4, the left half land, the right half open sea */
static const unsigned char kSeaBin[8 + 16] = { 'C', 'S', 'E', 'A', 4, 0, 4, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255,
                                               0, 0, 255, 255 };
static void PackLive(int reveal, int walls)
{
    const char *names[32], *data[32];
    unsigned lens[32] = { 0 };
    int n = 0;
    names[n] = "Interface\\Glues\\LoadingScreens\\LoadScreenDeadmines.blp"; data[n++] = "BLP2screen";
    names[n] = "CinderLoad\\bars.txt";
    data[n++] = walls ? "cinderwall01 aqua\r\ncinderwall02 fel\r\ndefault arcane\r\n" : kBars;
    static const char *const fam[] = { "fel", "aqua", "arcane" };
    static char fills[3][80], borders[3][80];
    for (int f = 0; f < 3; f++) {
        snprintf(fills[f], sizeof fills[f], TEX "Loading-BarFill-%s.blp", fam[f]);
        snprintf(borders[f], sizeof borders[f], TEX "Loading-BarBorder-%s.blp", fam[f]);
        names[n] = fills[f]; data[n++] = "BLP2f";
        names[n] = borders[f]; data[n++] = "BLP2b";
    }
    if (reveal) { names[n] = "CinderLoad\\reveal.txt"; data[n++] = "revealed"; }
    if (reveal & 2) { names[n] = "CinderLoad\\drift.txt"; data[n++] = "drifts"; }
    if (reveal & 8) {                                    /* the sea: its sheet, and its table (16: one a byte short) */
        names[n] = "Interface\\Glues\\LoadingScreens\\CinderSea.blp"; data[n++] = "BLP2sea";
        names[n] = "CinderLoad\\sea.bin"; data[n] = (const char *)kSeaBin; lens[n++] = sizeof kSeaBin - ((reveal & 16) ? 1 : 0);
    }
    if (reveal & 512) { names[n] = "Interface\\Glues\\LoadingScreens\\CinderWaves.blp"; data[n++] = "BLP2waves"; }
    if (reveal & 1024) {                                 /* the currents: their sheet and field */
        FlowBinMake(0.6f, 100, 0);
        names[n] = "Interface\\Glues\\LoadingScreens\\CinderFlow.blp"; data[n++] = "BLP2flow";
        names[n] = "CinderLoad\\flow.bin"; data[n] = (const char *)kFlowBin; lens[n++] = sizeof kFlowBin;
    }
    if (reveal & 128) {                                  /* the glow: its sheet, table and grid */
        GlowBinMake();
        static char gt[200];
        snprintf(gt, sizeof gt, "# g\ns 0 0 0 64 32\ns 1 64 0 128 64\ng 0 %08x %08x %08x %08x\ng 1 %08x %08x %08x %08x\n",
                 FBits(0.40f), FBits(0.50f), FBits(0.46f), FBits(0.53f), FBits(0.46f), FBits(0.50f), FBits(0.48f), FBits(0.52f));
        names[n] = "Interface\\Glues\\LoadingScreens\\CinderGlow.blp"; data[n++] = "BLP2glow";
        names[n] = "CinderLoad\\glow.txt"; data[n++] = gt;
        names[n] = "CinderLoad\\glow.bin"; data[n] = (const char *)kGlowBin; lens[n++] = sizeof kGlowBin - ((reveal & 256) ? 1 : 0);
    }
    if (reveal & 64) {                                   /* the marks: their sheet and table, the Maelstrom at 0.5, 0.44 */
        static char mk[200];
        snprintf(mk, sizeof mk, "# m\ns 0 0 0 256 256\ns 1 256 0 512 256\ns 2 0 256 128 384\ns 4 0 384 128 512\n"
                 "s 24 384 320 448 384\nmaelstrom %08x %08x %08x\n", FBits(0.5f), FBits(0.44f), FBits(0.068f));
        names[n] = "Interface\\Glues\\LoadingScreens\\CinderMarks.blp"; data[n++] = "BLP2marks";
        names[n] = "CinderLoad\\marks.txt"; data[n++] = mk;
    }
    if (reveal & 32) {                                   /* the logo: its sheet and its table */
        names[n] = "Interface\\Glues\\LoadingScreens\\CinderLogo.blp"; data[n++] = "BLP2logo";
        names[n] = "CinderLoad\\logo.txt"; data[n++] = "# logo\r\nlogo 1024 512 4 4 1020 484\r\n";
    }
    if (reveal & 4) {                                    /* the world map: Eastern Kingdoms only, u = 0.5 - 2e-5 y */
        static char mapTxt[400];
        int mt = snprintf(mapTxt, sizeof mapTxt, "# test\r\nmap 0 %08x %08x %08x %08x\r\nmap 2 0 0 0 0\r\nend 3000\r\n",
                          FBits(0.5f), FBits(-2e-5f), FBits(0.5f), FBits(-2e-5f));
        if (reveal & 4096)                               /* entrances: the Deadmines (36) here, Molten Core (409) on Kalimdor */
            snprintf(mapTxt + mt, sizeof mapTxt - (size_t)mt, "ent 24 0 %08x %08x\r\nent 199 1 %08x %08x\r\n",
                     FBits(-1000.0f), FBits(1000.0f), FBits(500.0f), FBits(500.0f));
        names[n] = "CinderLoad\\map.txt"; data[n++] = mapTxt;
        names[n] = "Interface\\Glues\\LoadingScreens\\CinderMap.blp"; data[n++] = "BLP2map";
    }
    if (walls) {
        names[n] = "Interface\\Glues\\LoadingScreens\\CinderWall01.blp"; data[n++] = "BLP2wall1";
        names[n] = "Interface\\Glues\\LoadingScreens\\CinderWall02.blp"; data[n++] = "BLP2wall2";
    }
    for (int i = 0; i < n; i++) if (!lens[i]) lens[i] = (unsigned)strlen(data[i]);
    MakeMpq(UW, n, names, data, lens);
    Put("WTF/Config.wtf", RES("5120x2160"));
}
static int SetupCall(void) { return Rd32(shim_module + 0x725E + 1); }
static int StripCall(void) { return Rd32(shim_module + 0x7288 + 1); }
static int StretchKept(void)
{
    return Same(shim_module + REVEAL_DRAW_RVA, kRevealDraw, (int)sizeof kRevealDraw) &&
           Same(shim_module + REVEAL_STRIP_RVA, kRevealStrip, (int)sizeof kRevealStrip);
}

static void LiveTests(void)
{
    /* the reveal: both calls ours, the fill as tall as the log */
    Fresh("reveal-on");
    PackLive(1, 0);
    Start();
    unsigned setup = (unsigned)((unsigned long)RevealStub - (unsigned long)(shim_module + 0x725E + 5));
    unsigned strip = (unsigned)((unsigned long)RevealStripStub - (unsigned long)(shim_module + 0x7288 + 5));
    Check(LogSays("fire reveal on") && (unsigned)SetupCall() == setup && (unsigned)StripCall() == strip &&
          shim_module[0x725E] == 0xE8 && shim_module[0x7288] == 0xE8 &&
          g_drawSetup == shim_module + 0x18A2A0 && g_drawStrip == shim_module + 0x18A2E0,
          "reveal on: RVA 0x725E to the setup stub, 0x7288 to the strip stub, each going on to the game's own (0x58A2A0, 0x58A2E0)");
    Check(Same(shim_module + BAR_FILL_RVA, kBorderWide, 16) && Same(shim_module + BAR_BORDER_RVA, kBorderWide, 16) &&
          LogSays("its fire as tall as the log") && LogSays("fire bar on"),
          "reveal on: the fill takes the log's own size, the fire bar still on");
    Start();
    Check(LogSays("fire reveal already on") && (unsigned)SetupCall() == setup, "a second start: already on, nothing written");

    Fresh("reveal-asked-not");
    PackLive(0, 0);
    Start();
    Check(!LogSays("fire reveal") && StretchKept() && Same(shim_module + BAR_FILL_RVA, kFillWide, 16) && LogSays("fire bar on"),
          "a pack without reveal.txt: stretched as before, the fill its old height");

    int quiet = 1;
    for (int i = 0; i < REVEAL_CODE_COUNT; i++) {
        Fresh("reveal-other");
        PackLive(1, 0);
        shim_module[kRevealCode[i].rva + (unsigned)kRevealCode[i].n - 1] ^= 0x40;
        Start();
        char words[160];
        snprintf(words, sizeof words, "fire reveal off: other bytes at RVA 0x%lX (%s)", (unsigned long)kRevealCode[i].rva,
                 kRevealCode[i].what);
        int kept = kRevealCode[i].rva == REVEAL_DRAW_RVA || kRevealCode[i].rva == REVEAL_STRIP_RVA
                       ? Same(shim_module + REVEAL_DRAW_RVA, kRevealDraw, (int)sizeof kRevealDraw - (kRevealCode[i].rva == REVEAL_DRAW_RVA)) &&
                         Same(shim_module + REVEAL_STRIP_RVA, kRevealStrip, (int)sizeof kRevealStrip - (kRevealCode[i].rva == REVEAL_STRIP_RVA))
                       : StretchKept();
        quiet &= LogSays(words) && kept && Same(shim_module + BAR_FILL_RVA, kFillWide, 16) && LogSays("fire bar on");
    }
    Check(quiet, "any of the reveal's five places changed: logged, neither call touched, the fill stretched at its old height");

    /* the live strip */
    float v[12] = { 0.0f, 0.0f, 0.5f,  0.5f, 0.0f, 0.5f,  0.0f, 0.09f, 0.5f,  0.5f, 0.09f, 0.5f };
    PutFloat(0x882BE4, 0.5f);
    g_barsOn = 1;
    RevealFill(v);
    int n = g_fillCount / 2, rising = 1;
    for (int k = 1; k < n; k++) rising &= g_fillVerts[6 * k] > g_fillVerts[6 * (k - 1)];
    float uSpan = g_fillUV[4 * (n - 1)] - g_fillUV[0];
    Check(g_fillCount % 2 == 0 && n >= 3 && n <= FILL_COLUMNS && rising && g_fillVerts[0] == 0.0f &&
          g_fillVerts[6 * (n - 1)] == 0.5f && g_fillVerts[1] == 0.09f && g_fillVerts[4] == 0.0f && g_fillVerts[2] == 0.5f,
          "the strip: columns from the left edge to the front in order, top then bottom, at the game's height and depth");
    Check(Near(uSpan, 0.25, 1e-4) && g_fillUV[1] == 0 && g_fillUV[3] == 1,
          "the lava: half its texture spans the whole bar, so half a bar's progress shows a quarter of it");
    unsigned front = g_fillColor[2 * (n - 1)], left = g_fillColor[0];
    int grey = 0, bright = 0;
    for (int k = 0; k < n; k++) {
        float d = (0.5f - g_fillVerts[6 * k]) / 1.0f;
        int gk = (int)(g_fillColor[2 * k] & 0xFF);
        if (d > 0.4f && gk > grey) grey = gk;
        if (d > 0.04f && d < 0.1f && gk > bright) bright = gk;
    }
    Check((front >> 24) == 0 && (left >> 24) == 255 && bright > grey && grey >= 70 && g_fillColor[1] == g_fillColor[0],
          "the heat: the front fades in from nothing, brightest just behind it, cooling (but still alight) far behind");
    PutFloat(0x882BE4, 0.0f);
    float v0[12] = { 0.0f, 0.0f, 0.5f,  0.0f, 0.0f, 0.5f,  0.0f, 0.09f, 0.5f,  0.0f, 0.09f, 0.5f };
    RevealFill(v0);
    Check(g_fillCount >= 2 && g_fillCount <= 2 * FILL_COLUMNS, "an empty bar: a strip of nothing, never past its arrays");
    PutFloat(0x882BE4, 2.0f);
    float v1[12] = { 0.0f, 0.0f, 0.5f,  1.0f, 0.0f, 0.5f,  0.0f, 0.09f, 0.5f,  1.0f, 0.09f, 0.5f };
    g_dispNew = 1;                                                    /* a fresh screen: the bar as shown starts at the game's */
    RevealFill(v1);
    Check(g_fillCount <= 2 * FILL_COLUMNS && g_fillVerts[6 * (g_fillCount / 2 - 1)] == 1.0f, "a progress past 1 is taken as 1");
    /* his 7 Oct load: after the blade had faded, the game's own lava bar showed through the end's blur for half a second.
     * A screen the view has drawn: the game's fill is drawn dark; the next screen (the view may not draw it) has it back */
    PutFloat(0x882BE4, 0.6f);
    g_viewDrew = 1;
    RevealFill(v1);
    int dark = g_fillCount > 0;
    for (int k = 0; k < g_fillCount; k++) dark &= (g_fillColor[k] >> 24) == 0;
    g_dispNew = 1;
    RevealFill(v1);
    int back = g_viewDrew == 0 && (g_fillColor[0] >> 24) == 255;
    Check(dark && back, "a screen the view has drawn: the game's own fire bar under it is dark (no lava through the end's blur); a new screen has it back");

    /* his ask, 4 Oct: the bar ran to two thirds, stopped while the game drew nothing, then leapt; it should move at a steady
     * rate. The bar as shown: the game's progress in lumps (5% every 0.2 s, frames every 20 ms) is followed at an even pace,
     * a frame's step never more than DISP_FASTEST allows, and close behind */
    {
        g_dispNew = 1; g_screenWatch = 1;
        DWORD t = 300000;
        float p = 0, worst = 0, lag = 0, last = 0, speed = 0, jerk = 0;
        DisplayStep(0, t);
        for (int f = 1; f <= 120; f++) {                              /* 2.4 s */
            t += 20;
            if (f % 10 == 0) p += 0.05f;                              /* the game's lumps */
            float d = DisplayStep(p, t), v = (d - last) / 0.02f;
            if (d - last > worst) worst = d - last;
            if (f > 40 && p - d > lag) lag = p - d;
            if (f > 40 && fabsf(v - speed) > jerk) jerk = fabsf(v - speed);   /* its speed, frame to frame */
            last = d; speed = v;
        }
        int steady = worst <= DISP_FASTEST * 0.02f + 1e-5f && worst < 0.05f * 0.5f && lag < 0.11f &&   /* a lump, + 6% */
                     jerk < 0.05f;                                    /* changing speed gently, never at a stroke */
        /* a stall with frames still coming: it eases to a stop just past the game, never back */
        float atStall = last, moved = 0;
        for (int f = 0; f < 50; f++) { t += 20; float d = DisplayStep(p, t); if (f == 4) moved = d - atStall; last = d; }
        steady &= moved > 0 && last <= p + DISP_LEAD + 1e-5f && last >= atStall;
        /* the game draws nothing for 1.2 s, then its bar has leapt from 60% to 91%: the bar as shown was waiting at its lead
         * before the pause, so it comes out of it still (no leap: his "freezes, then jumps up a ways"), then glides there */
        p = 0.91f; t += 1200;
        float before = last, first = DisplayStep(p, t), reached = 0;
        for (int f = 1; f <= 60 && !reached; f++) { t += 20; if (DisplayStep(p, t) > 0.9f) reached = f * 0.02f; }
        steady &= first - before <= 0.01f && reached > 0.1f && reached < 1.0f;
        /* the game's dip to nothing at the very end is not shown; a full bar is reached */
        float held = DisplayStep(0, t + 20);
        steady &= held >= 0.9f;
        t += 20;
        for (int f = 0; f < 50; f++) { t += 20; last = DisplayStep(1, t); }
        steady &= last > 0.999f && g_frmGap == 1200 && g_frmGapFrom == 60 && g_frmGapTo == 91 && g_frmDips == 1 &&
                  g_frmShown == 100;
        Check(steady, "the bar as shown: lumps followed at an even pace, a stall eased into, a leap after a pause glided, no dip, full at the end; its pause logged");
        /* the view stepping it (his 7 Oct load: the game paused at 54%, then its bar read 100% and froze there two
         * seconds; the bar shown leapt from 52% to full in one frame; then, 10 Oct, "the bar loads all of a sudden from half
         * to full, the bar should load at an even rate all the way up to 100% with no stutters or jumps"): it goes on at an
         * even rate on the clock, from where it was, never back, full EVEN_EARLY_MS before the world is due */
        g_disp = g_dispV = g_dispRate = g_dispMax = 0; g_dispAt = 0;
        t = 500000; last = 0;
        for (int f = 0; f < 300 && last < 0.52f; f++) { t += 20; p = 0.54f * f / 299.0f + 0.3f; if (p > 0.54f) p = 0.54f; last = DisplayStep(p, t); }
        InterlockedExchange(&g_viewSteps, 1);
        g_evenOn = 0; g_evenU = 0; g_evenEndAt = 0;
        LONG fzv = g_lnN[g_screenKind] > 0 ? g_lnFreeze[g_screenKind] : FREEZE_GUESS_MS;
        LONG tailv = g_blurOn && g_blurMs > HOLD_TAIL_MS ? g_blurMs : HOLD_TAIL_MS;
        g_planEndAt = t + 2000;
        DWORD due = g_planEndAt + (DWORD)tailv + (DWORD)fzv + VIEW_WORLD_GAP_MS - EVEN_EARLY_MS;
        float from = last, step = 0, least = 1, full = 0, back = 0;
        for (int f = 1; f <= 1200 && !full; f++) {
            t += 7;
            float d = DisplayStep(1, t);
            if (d - last > step) step = d - last;
            if (d < 1 && d - last < least) least = d - last;
            if (d < last) back = 1;
            last = d;
            if (d > 0.999f) full = (float)(LONG)(t - due) + 10000;      /* (ms from when it was due, offset to stay non-zero) */
        }
        /* the game behind its plan (stalled at 54% long past it): the view's bar creeps on, slower, never full before the game's */
        g_disp = g_dispV = g_dispRate = g_dispMax = 0; g_dispAt = 0; g_evenOn = 0; g_evenU = 0; g_evenEndAt = 0;
        DisplayStep(0.54f, t += 7);
        g_planEndAt = t + 500;
        float slow = 0;
        for (int f = 0; f < 1500; f++) { t += 7; slow = DisplayStep(0.54f, t); }    /* (ten seconds stalled) */
        int waited = slow <= EVEN_WAIT + 1e-6f && slow > 0.9f;
        for (int f = 0; f < 600 && slow < 0.999f; f++) { t += 7; slow = DisplayStep(1, t); }
        waited &= slow > 0.999f;
        g_evenOn = 0; g_evenU = 0; g_dispMax = 0.5f;                     /* (taken past EVEN_WAIT, the game behind: held there, never back) */
        float hold1 = EvenStep(16, t += 16, 0.985f), hold2 = EvenStep(16, t += 16, 0.985f);
        waited &= hold1 >= 0.985f && hold2 >= hold1;
        InterlockedExchange(&g_viewSteps, 0);
        Check(waited, "the view's bar with the game behind its plan: on, slower, never full before the game's bar is, then full");
        Check(from < 0.6f && !back && fabsf(full - 10000) <= 30 && step < 1.6f * least && step < 0.01f,
              "the view's bar when the game's leaps to 100% and freezes: on at an even rate from where it was (never back, no step much bigger than another), full EVEN_EARLY_MS before the world is due (no leap from half to full)");
    }
    /* his three logins (4 Oct) paused 0.2 to 0.6 s at 41, 44, 50 and 54%, the game drawing nothing: a bar moving steadily
     * comes out of a pause where its pace would have taken it (no lag to hurry back from), and a long freeze counts
     * DISP_GAP_MAX at most */
    {
        g_dispNew = 1; g_screenWatch = 1;
        DWORD t = 1100000;
        float p = 0, d = 0;
        DisplayStep(0, t);
        for (int f = 1; f <= 150; f++) { t += 20; p = f * 0.002f; d = DisplayStep(p, t); }   /* 3 s at 10% a second */
        float pace = g_dispV;
        t += 500; p += 0.05f;                                           /* half a second drawing nothing, the load going on */
        float out = DisplayStep(p, t);
        int through = pace > 0.08f && pace < 0.12f && fabsf((out - d) - pace * 0.5f) < 0.01f;
        for (int f = 0; f < 50; f++) { t += 20; p += 0.002f; d = DisplayStep(p, t); }
        pace = g_dispV;
        t += 3000; p += 0.3f;                                           /* three seconds frozen */
        out = DisplayStep(p, t);
        through &= (out - d) <= pace * DISP_GAP_MAX + 0.01f && (out - d) > pace * DISP_GAP_MAX - 0.01f;
        g_screenWatch = 0;
        Check(through, "a bar moving steadily comes out of a pause where its pace would have taken it; a long freeze counts DISP_GAP_MAX at most");
    }
    /* the zoom too: a freeze counts DISP_GAP_MAX at most, so it never flies across in one frame */
    {
        g_mapShown = 1;
        { const char *mt = "map 0 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\n"; MapParse(mt, (unsigned)strlen(mt)); }
        g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; g_driftNew = 1;
        DWORD at = 1200000;
        DriftStep(0, at);
        for (int i = 0; i < 50; i++) { at += 20; DriftStep(1, at); }
        float before = g_drift;
        at += 3000; DriftStep(1, at);
        int zg = g_drift - before <= DISP_GAP_MAX / g_map.pace + 1e-4f && g_drift - before > 0.05f;
        g_mapShown = 0;
        Check(zg, "the zoom: a three-second freeze moves it no further than its pace allows in DISP_GAP_MAX");
    }

    /* with the worker watching, a pause and a dip mid-screen are never taken for a new screen (the Stormwind load, 4 Oct:
     * "1 resets mid-screen" and the view went back out to the whole world); without one, the old rule stands */
    {
        g_dispNew = 1; g_screenWatch = 1;
        DWORD t = 400000;
        for (int f = 0; f < 100; f++) { t += 20; DisplayStep(0.7f, t); }
        t += 1500;
        float d = DisplayStep(0.05f, t);
        int watched = d >= 0.7f;
        g_screenWatch = 0;
        for (int f = 0; f < 10; f++) { t += 20; DisplayStep(0.7f, t); }
        t += 1500;
        d = DisplayStep(0.05f, t);
        watched &= d < 0.1f;
        /* and the drift the same */
        g_screenWatch = 1; g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; g_driftNew = 1;
        t = 500000;
        for (int f = 0; f < 100; f++) { t += 20; DriftStep(0.7f, t); }
        t += 1500; DriftStep(0.05f, t);
        watched &= g_driftResets == 0 && g_driftMax >= 0.7f;
        g_screenWatch = 0;
        Check(watched, "with the worker watching, a pause then a dip mid-screen starts nothing over (bar or zoom); without it, it does");
    }
    /* his stutters, 4 Oct (at 20, 30, 50, 55, 60, 75 and 80%): each screen logs its hitches (no frame for 0.12 s or more)
     * and its stalls (frames on, the game's bar unmoved 0.3 s or more), the longest six of each, in the bar's order */
    {
        Fresh("hitches");
        GameDir();
        g_dispNew = 1; g_screenWatch = 1;
        DWORD t = 950000;
        float p = 0;
        DisplayStep(0, t);
        for (int f = 0; f < 10; f++) { t += 20; p += 0.02f; DisplayStep(p, t); }      /* to 20% */
        t += 300; DisplayStep(p, t);                                                    /* nothing drawn: 0.3 s at 20% */
        for (int f = 0; f < 15; f++) { t += 20; p += 0.02f; DisplayStep(p, t); }      /* to 50% */
        t += 150; DisplayStep(p, t);                                                    /* 0.15 s at 50% */
        t += 20; p = 0.55f; DisplayStep(p, t);
        for (int f = 0; f < 25; f++) { t += 20; DisplayStep(p, t); }                   /* frames on, its bar still 0.5 s */
        t += 20; p = 0.6f; DisplayStep(p, t);
        for (int f = 0; f < 10; f++) { t += 20; DisplayStep(p, t); }
        char lm[220];
        HitchLog(lm);
        int hl = g_hitchN == 2 && g_stallN == 1 &&
                 LogSays("its hitches (the game drew nothing): 2; 0.30 s at 20%, 0.15 s at 50%") &&
                 LogSays("its stalls (frames on, its bar still): 1; 0.52 s at 55%");
        for (int f = 0; f < 8; f++) { t += 200; p += 0.02f; DisplayStep(p, t); }      /* eight more: the six longest kept */
        hl &= g_hitchN == 10;
        int kept = 0;
        for (int i = 0; i < HITCH_KEEP; i++) kept += g_hitchMs[i] > 0;
        hl &= kept == HITCH_KEEP;
        g_screenWatch = 0;
        Check(hl, "each screen logs its hitches and its stalls, the longest six of each, in the bar's order");
    }
    /* the worker's word that a screen has gone starts the bar as shown afresh; a leap from nothing to full is crossed no
     * quicker than DISP_FASTEST allows */
    {
        g_dispNew = 1; g_screenWatch = 1;
        DWORD t = 700000;
        for (int f = 0; f < 100; f++) { t += 20; DisplayStep(0.8f, t); }
        DriftScreenGone();
        t += 20;
        int fresh = DisplayStep(0.05f, t) < 0.06f;
        float last = DisplayStep(0.05f, t += 20), fastest = 0;
        for (int f = 0; f < 100; f++) { t += 20; float d = DisplayStep(0.95f, t); if (d - last > fastest) fastest = d - last; last = d; }
        fresh &= fastest <= DISP_FASTEST * 0.02f + 1e-5f && fastest > DISP_FASTEST * 0.02f * 0.8f && last > 0.95f;
        /* and the game's bar full: the bar as shown full at once (his loads ended frozen at 91 to 98%) */
        DriftScreenGone();
        DisplayStep(0, t += 20);
        for (int f = 0; f < 5; f++) DisplayStep(0.3f, t += 20);
        fresh &= DisplayStep(1.0f, t += 20) == 1.0f;
        g_screenWatch = 0;
        Check(fresh, "a screen gone starts the bar as shown afresh; a leap from nothing to full is crossed at DISP_FASTEST, no quicker");
    }
    /* the fill is drawn to the bar as shown: a frame after the game leaps from 20% to 90%, the front has only begun to move */
    {
        float vf[12] = { 0.0f, 0.0f, 0.5f,  0.2f, 0.0f, 0.5f,  0.0f, 0.09f, 0.5f,  0.2f, 0.09f, 0.5f };
        g_dispNew = 1; g_fillFull = 0; shim_ticks = 600000;
        g_driftOn = 1; g_driftNew = 1; g_driftAt = 0; g_mapShown = 0;
        PutFloat(0x882BE4, 0.2f);
        RevealFill(vf);
        float at20 = g_fillVerts[6 * (g_fillCount / 2 - 1)];
        PutFloat(0x882BE4, 0.9f);
        vf[3] = vf[9] = 0.9f;
        shim_ticks += 20;
        RevealFill(vf);
        float next = g_fillVerts[6 * (g_fillCount / 2 - 1)];
        shim_ticks += 20;
        RevealFill(vf);                                                  /* the frame after: on its way */
        float then = g_fillVerts[6 * (g_fillCount / 2 - 1)];
        g_driftOn = 0;
        /* and the zoom is fed the bar as shown too, not the game's leap */
        int zoomShown = g_driftMax < 0.3f;
        Check(fabsf(at20 - 0.2f) < 1e-5f && next >= 0.2f && next < 0.25f && then > 0.2f && then < 0.3f &&
              fabsf(g_fillFull - 1.0f) < 1e-5f && zoomShown,
              "the fill's front is the bar as shown: a leap from 20% to 90% has only begun a frame later, the zoom's too");
    }

    /* his rule, 4 Oct: with entrances in map.txt every loading screen is the world map, and a dungeon's, raid's or
     * battleground's zooms onto its entrance; without them, as before */
    {
        const char *dmPath = "Interface\\Glues\\LoadingScreens\\LoadScreenDeadmines.blp";
        Fresh("map-entrances");
        PackLive(1 | 4 | 4096, 0);
        Start();
        FakeTables();
        int en = g_map.ok && g_map.ents == 2 && LogSays("on every loading screen") && LogSays("(2 listed)");
        ForMap(36);
        en &= g_mapShown == 1 && !strcmp(BarPath(dmPath), MAP_PATH);
        float du, dv, hEnd;
        Poke(BAR_MAP_VA, 36);
        en &= MapDest(&du, &dv, &hEnd, 0) && fabsf(du - (0.5f - 2e-5f * 1000)) < 1e-5f &&
              fabsf(dv - (0.5f - 2e-5f * -1000)) < 1e-5f && g_destMap == 0;
        Poke(BAR_MAP_VA, 409);                           /* its entrance is on Kalimdor, which this map cannot show */
        en &= !MapDest(&du, &dv, &hEnd, 0);
        ForMap(409);
        en &= g_mapShown == 1;                           /* the map all the same, the whole world */
        Poke(BAR_MAP_VA, 0xFFFFFFFFu);                   /* the default screen: no entrance */
        en &= !MapDest(&du, &dv, &hEnd, 0);
        Check(en, "with entrances: every screen the world map; the Deadmines zooms onto its entrance; one on a continent the map lacks, or none, the whole world");

        Fresh("map-no-entrances");
        PackLive(1 | 4, 0);
        Start();
        FakeTables();
        ForMap(36);
        Check(g_mapShown == 0 && BarPath(dmPath) == dmPath && LogSays("on Eastern Kingdoms and Kalimdor screens"),
              "without entrances: a dungeon keeps its own picture, as before");

        /* the ent lines, read carefully: a map id over 1, a continent of 0 or 1, a place inside the world; a later line wins */
        char t[2400];
        int n = snprintf(t, sizeof t, "map 0 %08x %08x %08x %08x\nend 3000\nent 1 0 %08x %08x\nent 24 2 %08x %08x\n"
                         "ent 24 0 7fc00000 %08x\nent 24 0 %08x %08x\nent 24 0 %08x %08x\nent 30 0 %08x %08x\n",
                         FBits(0.5f), FBits(-2e-5f), FBits(0.5f), FBits(-2e-5f), FBits(1), FBits(1), FBits(1), FBits(1), FBits(1),
                         FBits(10), FBits(10), FBits(20), FBits(20), FBits(-30000), FBits(5));
        MapParse(t, (unsigned)n);
        unsigned c; float x, y;
        int rd = g_map.ents == 1 && MapEntrance(0x24, &c, &x, &y) && c == 0 && x == 20 && y == 20 && !MapEntrance(1, &c, &x, &y) &&
                 !MapEntrance(0x30, &c, &x, &y);
        n = 0;
        for (int i = 0; i < 120; i++) n += snprintf(t + n, sizeof t - (size_t)n, "ent %x 0 0 0\n", 100 + i);
        MapParse(t, (unsigned)n);
        rd &= g_map.ents == MAP_ENTS;
        Check(rd, "map.txt's entrances: an id over 1, continent 0 or 1, a place inside the world, a later line winning, no more than MAP_ENTS");
        g_map.ok = 0; g_map.ents = 0;
    }

    /* the wallpapers */
    Fresh("walls-on");
    PackLive(1, 1);
    Start();
    unsigned wallRel = (unsigned)((unsigned long)BarStub - (unsigned long)(shim_module + 0x6EBA + 5));
    Check(LogSays("wallpapers: 2 in the pack") && LogSays("wallpapers on") && Rd32(shim_module + 0x6EBA + 1) == wallRel &&
          shim_module[0x6EBA] == 0xE8 && Same(shim_module + 0x6EAF, kWallLoad, 12),
          "wallpapers on: the picture's load at RVA 0x6EBA goes to the bar's stub, every other byte kept");
    FakeTables();
    const char *ekPath = "Interface\\Glues\\LoadingScreens\\LoadScreenEasternKingdom.blp";
    int ok = 1, last = -1, changed = 1;
    for (int i = 0; i < 6; i++) {
        const char *fill = ForMap(0);
        const char *pic = BarPath(ekPath);
        int w = strstr(pic, "CinderWall01") ? 1 : (strstr(pic, "CinderWall02") ? 2 : 0);
        ok &= w && !strcmp(fill, w == 1 ? TEX "Loading-BarFill-aqua" : TEX "Loading-BarFill-fel");
        if (last >= 0) changed &= w != last;
        last = w;
    }
    Check(ok && changed, "Eastern Kingdoms: a wallpaper drawn at the fill, its fire with it, the picture then that same wallpaper, never twice in a row");
    const char *dmPath = "Interface\\Glues\\LoadingScreens\\LoadScreenDeadmines.blp";
    const char *dmFill = ForMap(36);
    Check(BarPath(dmPath) == dmPath && !strcmp(dmFill, TEX "Loading-BarFill-arcane"),
          "a dungeon (the Deadmines): its own picture, its fire from bars.txt");

    Fresh("walls-none");
    PackLive(1, 0);
    Start();
    Check(!LogSays("wallpapers") && Same(shim_module + WALL_LOAD_RVA, kWallLoad, (int)sizeof kWallLoad),
          "no wallpapers in the pack: the picture's load is never touched");

    Fresh("walls-other");
    PackLive(1, 1);
    shim_module[WALL_LOAD_RVA + 1] ^= 0x40;
    Start();
    Check(LogSays("wallpapers off: other bytes at RVA 0x6EAF") && LogSays("fire bar on") &&
          shim_module[WALL_LOAD_RVA + 1] == (0x4E ^ 0x40) && Same(shim_module + WALL_CALL_RVA, kWallLoad + WALL_REL_AT - 1, 5),
          "the picture's load holding other bytes: logged, untouched, the fire bar still on");

    /* the picture's drift: one pushed address pointed at g_bgUV, eased by the reveal's stub */
    unsigned uvAt = (unsigned)(unsigned long)g_bgUV;
    Fresh("drift-on");
    PackLive(3, 0);
    Start();
    Check(LogSays("picture drift on") && Rd32(shim_module + BG_PUSH_RVA + 1) == uvAt && g_driftOn &&
          shim_module[BG_PUSH_RVA] == 0x68 && Same(shim_module + BG_DRAW_RVA, kBgDraw, BG_PTR_AT) &&
          Same(shim_module + BG_PUSH_RVA + 5, kBgDraw + BG_PTR_AT + 4, (int)sizeof kBgDraw - BG_PTR_AT - 4),
          "drift on: only the background's pushed address changes, to g_bgUV");
    Start();
    Check(LogSays("picture drift already on") && Rd32(shim_module + BG_PUSH_RVA + 1) == uvAt, "a second start: already on");

    Fresh("drift-not-asked");
    PackLive(1, 0);
    Start();
    Check(!LogSays("picture drift") && Same(shim_module + BG_DRAW_RVA, kBgDraw, (int)sizeof kBgDraw) && !g_driftOn,
          "a pack without drift.txt: the background untouched");

    Fresh("drift-other");
    PackLive(3, 0);
    shim_module[BG_DRAW_RVA + 2] ^= 0x01;
    Start();
    Check(LogSays("picture drift off: other bytes at RVA 0x6DB6") && !g_driftOn &&
          Rd32(shim_module + BG_PUSH_RVA + 1) == 0x00882B8C, "the background holding other bytes: logged, untouched");

    Fresh("drift-no-reveal");
    PackLive(3, 0);
    shim_module[kRevealCode[0].rva] ^= 0x01;
    Start();
    Check(!LogSays("picture drift on") && !g_driftOn && Rd32(shim_module + BG_PUSH_RVA + 1) == 0x00882B8C,
          "no reveal (its stub runs the drift): no drift either");

    /* the drift itself */
    g_drift = 0; g_driftAt = 0; g_zoomScreens = 0;
    DriftStep(0, 1000);
    int ident = g_bgUV[0] == 0.5f - 0.5f && g_bgUV[1] == 1 && g_bgUV[6] == 1 && g_bgUV[7] == 0;
    DWORD now = 1000;
    for (int i = 0; i < 400; i++) { now += 20; DriftStep(1, now); }
    float h = 0.5f / (1 + DRIFT_ZOOM);
    int full = fabsf(g_bgUV[0] - (0.5f - h)) < 1e-4f && fabsf(g_bgUV[1] - (0.5f + h)) < 1e-4f &&
               fabsf(g_bgUV[6] - (0.5f + h)) < 1e-4f && fabsf(g_bgUV[7] - (0.5f - h)) < 1e-4f;
    float mid = g_drift;
    now += 20; DriftStep(0.0f, now);                                  /* the bar dips for a frame (4 Oct): held */
    int held = g_drift > 0.95f && fabsf(g_bgUV[0] - (0.5f - h)) < 1e-3f;
    now += 2000; DriftStep(0.0f, now);                                /* the next screen, after a gap: starts again */
    int reset = held && g_drift < 0.05f && mid > 0.95f;
    g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; now = 5000; DriftStep(0, now);
    now += 20; DriftStep(1, now);
    int gentle = g_drift > 0 && g_drift < 0.1f;                       /* one frame after a leap: a small step, not a jump */
    float vv[12] = { 0.0f, 0.0f, 0.5f,  0.5f, 0.0f, 0.5f,  0.0f, 0.09f, 0.5f,  0.5f, 0.09f, 0.5f };
    g_driftOn = 1; g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; shim_ticks = 9000;
    PutFloat(0x882BE4, 1.0f);
    RevealFill(vv); shim_ticks += 50; RevealFill(vv);
    int framed = g_drift > 0 && g_bgUV[0] > 0;                        /* the stub's frame moved the picture */
    g_driftOn = 0; g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; shim_ticks += 50; RevealFill(vv);
    framed &= g_drift == 0;                                           /* and only while the drift is on */
    Check(ident && full && reset && gentle && framed,
          "the drift: the whole picture at 0, 12% closer at a full bar, eased (no jump on a leap), starting again on a new screen");

    /* the world map zoom */
    Fresh("map-on");
    PackLive(1 | 2 | 4, 0);
    Start();
    Check(LogSays("zoom: the world map is in the pack") && LogSays("wallpapers on") && g_map.ok && g_map.m[0].ok &&
          !g_map.m[1].ok && g_map.endYards == 3000 && LogSays("picture drift on"),
          "map.txt and CinderMap.blp in the pack: the zoom on (Eastern Kingdoms only, as the table says), the picture's load ours");
    FakeTables();
    const char *ek = "Interface\\Glues\\LoadingScreens\\LoadScreenEasternKingdom.blp";
    const char *dm = "Interface\\Glues\\LoadingScreens\\LoadScreenDeadmines.blp";
    ForMap(0);
    int shown = g_mapShown == 1 && !strcmp(BarPath(ek), MAP_PATH) && g_wallPick == 0;
    char line[BAR_SHOWN_MAX];
    InterlockedExchange(&g_barShown, -1);
    BarShownSince(line, (int)sizeof line);
    shown &= strstr(line, ", the world map") != NULL && strstr(line, "wallpaper") == NULL;
    ForMap(36);
    shown &= g_mapShown == 0 && BarPath(dm) == dm;
    Check(shown, "the Eastern Kingdoms screen gets the world map (and says so in its line); the Deadmines keeps its own");

    /* the glide: the whole world at the start, onto the arrival at a full bar */
    float hEnd = 0.5f * 3000 * 2e-5f;
    ForMap(0);
    Poke(MAP_DEST_MAP_VA, 0);
    PutFloat(MAP_DEST_X_VA, -5000.0f);                                /* v = 0.5 + 2e-5 * 5000 = 0.6 */
    PutFloat(MAP_DEST_Y_VA, 10000.0f);                                /* u = 0.5 - 2e-5 * 10000 = 0.3 */
    g_drift = 0; g_driftAt = 0; g_zoomScreens = 0;
    DWORD at = 20000;
    DriftStep(0, at);
    int whole = g_bgUV[0] == 0 && g_bgUV[1] == 1 && g_bgUV[2] == 1 && g_bgUV[5] == 0;
    for (int i = 0; i < 500; i++) { at += 20; DriftStep(1, at); }
    int lands = fabsf(g_bgUV[0] - (0.3f - hEnd)) < 2e-3f && fabsf(g_bgUV[2] - (0.3f + hEnd)) < 2e-3f &&
                fabsf(g_bgUV[5] - (0.6f - hEnd)) < 2e-3f && fabsf(g_bgUV[1] - (0.6f + hEnd)) < 2e-3f;
    Check(whole && lands, "the zoom: the whole world as the screen begins, the arrival in the middle of a 3000-yard view at a full bar");

    Poke(MAP_DEST_MAP_VA, 1);                                         /* the position is still the last map's */
    g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; at = 40000;
    for (int i = 0; i < 500; i++) { at += 20; DriftStep(1, at); }
    int stays = g_bgUV[0] == 0 && g_bgUV[2] == 1;
    g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; at = 60000;
    for (int i = 0; i < 100; i++) { at += 20; DriftStep(0.5f, at); }
    Poke(MAP_DEST_MAP_VA, 0);                                         /* and now it lands */
    at += 20; DriftStep(0.5f, at);
    int glides = g_viewU < 0.5f && g_viewU > 0.45f;                   /* one frame: on its way, not there */
    /* half way: the middle of the view has come as far as the zoom has, not all the way to the arrival */
    g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; at = 80000;
    for (int i = 0; i < 500; i++) { at += 20; DriftStep(0.5f, at); }
    float e5 = 1 - (1 - g_drift) * (1 - g_drift), h5 = 0.5f * powf(hEnd / 0.5f, e5);
    float want = 0.5f + (0.3f - 0.5f) * (0.5f - h5) / (0.5f - hEnd);
    glides &= fabsf(g_viewU - want) < 3e-3f && fabsf(g_viewH - h5) < 3e-3f;
    Check(stays && glides, "a position from the last map is not used (the whole world held); when the new one lands, the view glides to it");

    /* his find, 4 Oct: the bar dips to 0 for a frame at the very end, and the view jumped back out to the whole world */
    Poke(MAP_DEST_MAP_VA, 0);
    g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; at = 100000;
    for (int i = 0; i < 500; i++) { at += 20; DriftStep(1, at); }
    at += 20; DriftStep(0, at);
    at += 20; DriftStep(0, at);
    int holds = fabsf(g_bgUV[0] - (0.3f - hEnd)) < 3e-3f && fabsf(g_bgUV[5] - (0.6f - hEnd)) < 3e-3f;
    /* and the approach is done by 90% of the bar, so its end rests on the arrival */
    g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; at = 120000;
    for (int i = 0; i < 500; i++) { at += 20; DriftStep(0.9f, at); }
    int early = g_viewH < hEnd * 1.06f;
    Check(holds && early, "the bar dipping at the end leaves the view on the arrival; the zoom is in by 90% of the bar");

    /* his find, 4 Oct, the real one: the game freezes for a moment at the very end of a load; a second without a frame
     * reset the view, and the next frame showed the first image again. A pause at a full bar now changes nothing */
    g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; at = 180000;
    for (int i = 0; i < 500; i++) { at += 20; DriftStep(1, at); }
    at += 1800; DriftStep(1, at);                                     /* the end-of-load freeze */
    at += 20; DriftStep(1, at);
    int freeze = fabsf(g_bgUV[0] - (0.3f - hEnd)) < 3e-3f && g_driftResets == 0;
    /* the arrival, once known, is kept even if the game clears it at the end */
    Poke(MAP_DEST_MAP_VA, 7);
    at += 20; DriftStep(1, at);
    at += 20; DriftStep(1, at);
    freeze &= fabsf(g_bgUV[0] - (0.3f - hEnd)) < 3e-3f;
    Poke(MAP_DEST_MAP_VA, 0);
    /* the worker says the screen has gone: the whole world at once (no frame of the old view), and the next screen starts
     * from it */
    DriftScreenGone();
    int gone = g_bgUV[0] == 0 && g_bgUV[2] == 1 && g_bgUV[5] == 0 && g_bgUV[1] == 1 && g_driftNew == 1;
    at += 20; DriftStep(0, at);
    gone &= g_drift < 0.01f && g_driftNew == 0 && g_viewH > 0.49f;
    Check(freeze && gone, "a freeze at the end of a load keeps the view on the arrival, as does the game clearing it; a screen gone starts the next from the whole world");

    /* the overlay: on only with the world map, the sea's sheet and table, and its six calls where they are */
    Fresh("overlay-no-sea");
    PackLive(1 | 2 | 4, 0);
    Start();
    int ov = !g_ovOn && !LogSays("overlay on");
    Fresh("overlay-sea-short");
    PackLive(1 | 2 | 4 | 8 | 16, 0);
    Start();
    ov &= !g_ovOn && !g_seaOn && g_seaW == 0;
    Fresh("overlay-on");
    PackLive(1 | 2 | 4 | 8, 0);
    Start();
    ov &= g_ovOn && g_seaOn && g_seaW == 4 && LogSays("overlay on: glints on the open sea");
    Fresh("overlay-moved");
    PackLive(1 | 2 | 4 | 8, 0);
    shim_module[0x6E0D + 2] ^= 0x10;                                  /* "after a draw" calls somewhere else */
    Start();
    ov &= !g_ovOn && LogSays("overlay off: the calls it draws with");
    Check(ov, "the overlay: off without the sea's sheet or with a sea.bin a byte short, on with them, off (and saying so) when one of its calls has moved");

    /* the logo: on with its sheet and table, said in the log; top centre, steady, the logo's own shape; narrowed on a
     * narrow screen; a table it cannot use leaves it off */
    {
        Fresh("overlay-logo");
        PackLive(1 | 2 | 4 | 32, 0);
        Start();
        int lg = g_ovOn && g_logoOn && !g_seaOn && LogSays("overlay on: the RavenCraft logo at the top");
        Fresh("overlay-all");
        PackLive(1 | 2 | 4 | 8 | 32, 0);
        Start();
        lg &= g_ovOn && g_logoOn && g_seaOn && LogSays("overlay on: glints on the open sea, the RavenCraft logo at the top");
        float asp = 1016.0f / 480.0f;
        g_scrW = g_surfW = 5120; g_scrH = g_surfH = 2160;                                 /* his 21:9: LOGO_H tall, its own shape */
        LogoBuild();
        float w = g_logoPos[3] - g_logoPos[0], h = g_logoPos[7] - g_logoPos[1];
        lg &= fabsf(h - LOGO_H) < 1e-5f && fabsf(w * 5120 / (h * 2160) - asp) < 1e-3f &&
              fabsf((g_logoPos[0] + g_logoPos[3]) / 2 - 0.5f) < 1e-6f && fabsf(g_logoPos[7] - (1 - LOGO_TOP)) < 1e-6f &&
              g_logoUV[0] == 4.0f / 1024 && g_logoUV[2] == 1020.0f / 1024 && g_logoUV[1] == 484.0f / 512 &&
              g_logoUV[5] == 4.0f / 512 && g_logoCol[0] == 0xFFFFFFFFu;
        g_scrW = g_surfW = 2560; g_scrH = g_surfH = 1600;                                 /* 16:10: narrowed to LOGO_W, still its own shape */
        LogoBuild();
        w = g_logoPos[3] - g_logoPos[0]; h = g_logoPos[7] - g_logoPos[1];
        lg &= fabsf(w - LOGO_W) < 1e-5f && h < LOGO_H && fabsf(w * 2560 / (h * 1600) - asp) < 1e-3f;
        { const char *t = "logo 1024 512 4 4 1030 484\n"; LogoParse(t, (unsigned)strlen(t)); }   /* past the sheet */
        lg &= !g_logo.ok;
        { const char *t = "logo 1024 512 4 4 1020\n"; LogoParse(t, (unsigned)strlen(t)); }        /* a number short */
        lg &= !g_logo.ok;
        { const char *t = "# x\nlogos 1 1 0 0 1 1\nlogo 1024 512 4 4 1020 484\n"; LogoParse(t, (unsigned)strlen(t)); }
        lg &= g_logo.ok;
        Check(lg, "the logo: on with its sheet and table, in the log; top centre, LOGO_H tall in its own shape, narrowed on 16:10; a bad table leaves it off");
    }

    /* the marks: marks.txt read (bad lines left out); on with their sheet, said in the log */
    {
        char mt[300];
        snprintf(mt, sizeof mt, "# t\ns 0 0 0 256 256\ns 1 256 0 512 256\ns 40 0 0 8 8\ns 2 0 256 600 384\n"
                 "maelstrom %08x %08x %08x\nmaelstrom %08x %08x %08x\n", FBits(0.5f), FBits(0.44f), FBits(0.068f),
                 FBits(0.5f), FBits(0.44f), FBits(0.5f));
        MarksParse(mt, (unsigned)strlen(mt));
        int mk = g_mark[0].ok && g_mark[1].ok && !g_mark[2].ok && g_mael.ok && fabsf(g_mael.r - 0.068f) < 1e-6f &&
                 g_mark[1].x0 == 256 && g_mark[1].y1 == 256;
        Fresh("overlay-marks");
        PackLive(1 | 2 | 4 | 8 | 64, 0);
        Start();
        mk &= g_marksOn && LogSays("overlay on: glints on the open sea, the Maelstrom, the ripple and the badge");
        Check(mk, "marks.txt read (an id past 31, a rectangle past the sheet, a radius of half the world left out); the marks on, in the log");

        /* the Maelstrom: in the world's place and size, square on screen, turning anticlockwise, gone close in */
        g_scrW = g_surfW = 5120; g_scrH = g_surfH = 2160;
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        MaelstromBuild(0);
        float k = 2160.0f / 5120, hs = 0.068f;                         /* the whole world: a picture height is the screen's */
        int ml = g_mkQuads == 1 && fabsf(g_mkPos[0] - (0.5f - hs * k)) < 1e-4f && fabsf(g_mkPos[1] - (0.56f - hs)) < 1e-4f &&
                 fabsf(g_mkPos[9] - (0.5f + hs * k)) < 1e-4f && fabsf(g_mkPos[10] - (0.56f + hs)) < 1e-4f &&
                 (g_mkCol[0] >> 24) == 255 && g_mkUV[0] == 0.5f && g_mkUV[2] == 1.0f && g_mkUV[1] == 0.5f;
        MaelstromBuild(MAEL_TURN / 4);                                  /* a quarter turn: the first corner where the second was */
        ml &= fabsf(g_mkPos[0] - (0.5f + hs * k)) < 1e-3f && fabsf(g_mkPos[1] - (0.56f - hs)) < 1e-3f;
        g_bgUV[0] = g_bgUV[4] = 0.25f; g_bgUV[2] = g_bgUV[6] = 0.75f;  /* half way in, h = 0.25: twice the size, fading */
        g_bgUV[5] = g_bgUV[7] = 0.19f; g_bgUV[1] = g_bgUV[3] = 0.69f;
        MaelstromBuild(0);
        ml &= g_mkQuads == 1 && fabsf(g_mkPos[7] - g_mkPos[1] - 4 * hs) < 1e-4f && (g_mkCol[0] >> 24) < 255 &&
              (g_mkCol[0] >> 24) > 100;
        g_bgUV[0] = g_bgUV[4] = 0.4f; g_bgUV[2] = g_bgUV[6] = 0.6f;    /* zoomed in, h = 0.1 */
        g_bgUV[5] = g_bgUV[7] = 0.34f; g_bgUV[1] = g_bgUV[3] = 0.54f;
        MaelstromBuild(0);
        ml &= g_mkQuads == 0;
        Check(ml, "the Maelstrom: the world's place and size, square on screen, a quarter turn anticlockwise in a quarter of MAEL_TURN, gone close in");

        /* the Maelstrom in strokes (8 Oct, his asks): a vortex, the strokes near the eye turning faster than those at
         * the rim, anticlockwise; drained slowly inward and made anew at the rim, the spread holding steady over a
         * minute; drawn by the view with the soft spark, in the world's place and size, the halo first, every stroke
         * soft across its width; gone close in, and nothing without the spark */
        {
            int sm = 1;
            unsigned spark = g_viewTex[VT_SPARK];
            g_mlInit = 0; g_mlSeed = 12345u;
            MaelStep(0);
            g_ml[0].r = 0.4f; g_ml[0].th = 0; g_ml[0].age = 5;
            g_ml[1].r = 1.4f; g_ml[1].th = 0; g_ml[1].age = 5;
            MaelStep(1.0f);
            sm &= g_ml[1].th > 0 && g_ml[0].th > 2.5f * g_ml[1].th;
            sm &= g_ml[0].r < 0.4f && g_ml[1].r < 1.4f && g_ml[1].r > 1.4f * 0.9f;
            for (int i = 0; i < 600; i++) MaelStep(0.1f);
            int inner = 0, out = 0;
            float mid = SaberSqrt(MAEL_R_IN * MAEL_R_OUT);
            for (int i = 0; i < MAEL_STROKES; i++) {
                if (g_ml[i].r < MAEL_R_IN || g_ml[i].r > MAEL_R_OUT) out++;
                if (g_ml[i].r < mid) inner++;
            }
            sm &= !out && inner > MAEL_STROKES * 3 / 10 && inner < MAEL_STROKES * 7 / 10;
            g_viewTex[VT_SPARK] = 7;
            g_scrW = g_surfW = 5120; g_scrH = g_surfH = 2160;
            memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
            ViewListStart();
            MaelstromStrokes(5120, 2160, 0, 0);
            float cx = 0.5f * 5120, cy = 0.44f * 2160, rp = 0.068f * 2160;
            int quads = g_vvN / 4, far = 0, soft = 1;
            for (int i = 0; i < g_vvN; i++) {
                float dx = g_vv[i].x - cx, dy = g_vv[i].y - cy;
                if (dx * dx + dy * dy > (2.6f * rp) * (2.6f * rp)) far++;
            }
            for (int i = 4; i < g_vvN - 4; i++) soft &= g_vv[i].u == 0.5f && (g_vv[i].v == 0 || g_vv[i].v == 1);
            sm &= g_vbN == 1 && g_vb[0].tex == VT_SPARK && g_vb[0].add == 0 && quads > MAEL_CURL_SEGS + 100 &&
                  quads <= MAEL_QUADS && !far && soft;
            sm &= fabsf(g_vv[0].x - (cx - 1.55f * rp)) < 0.5f && fabsf(g_vv[2].y - (cy + 1.55f * rp)) < 0.5f && g_vv[0].c[3] < 100;
            float e0x = (g_vv[g_vvN - 4].x + g_vv[g_vvN - 2].x) / 2, e0y = (g_vv[g_vvN - 4].y + g_vv[g_vvN - 2].y) / 2;
            ViewListStart();
            MaelstromStrokes(5120, 2160, 1.2f, 0);                          /* a moment on: the eye has wandered, the rim has not */
            float e1x = (g_vv[g_vvN - 4].x + g_vv[g_vvN - 2].x) / 2, e1y = (g_vv[g_vvN - 4].y + g_vv[g_vvN - 2].y) / 2;
            float moved = SaberSqrt((e1x - e0x) * (e1x - e0x) + (e1y - e0y) * (e1y - e0y));
            sm &= moved > 0.02f * rp && moved < 0.3f * rp && fabsf(g_vv[0].x - (cx - 1.55f * rp)) < 0.5f;
            g_bgUV[0] = g_bgUV[4] = 0.4f; g_bgUV[2] = g_bgUV[6] = 0.6f;    /* zoomed in, h = 0.1 */
            g_bgUV[5] = g_bgUV[7] = 0.34f; g_bgUV[1] = g_bgUV[3] = 0.54f;
            ViewListStart();
            MaelstromStrokes(5120, 2160, 0, 0);
            sm &= g_vvN == 0;
            memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
            g_viewTex[VT_SPARK] = 0;
            ViewListStart();
            MaelstromStrokes(5120, 2160, 0, 0);
            sm &= g_vvN == 0;
            g_viewTex[VT_SPARK] = spark;
            Check(sm, "the Maelstrom in strokes: a vortex (nearer the eye, faster; anticlockwise), drained inward and made anew, steady over a minute; in the world's place and size, the halo first, soft across; the eye wandering, the rim still; gone close in and without the spark");
        }

        /* the place's name, top centre (8 Oct, his asks): titles.txt read (bad lines left out; a bad title kept empty so the
         * next keep their numbers); the continent's as a screen begins, the zone's (the smallest box holding the arrival)
         * once in past TITLE_ZONE_H, the subzone's (the arrival's glow) once in past TITLE_SUB_H, a dungeon's by its map
         * and the continent of its entrance before that; set out letter by letter (backs, then faces) at the top centre at
         * the screen's scale; coming in: faded in and into focus together through the blurred layers; a change: the old
         * blurred and faded out over TITLE_OUT, only then the new; a change part way: the old goes on from where it was;
         * begun again on a new screen; in the sheet's own colours for any class; not graded by the hour */
        {
            char tt[2400];
            snprintf(tt, sizeof tt, "# t\nS 1024 512 90\nS 2048 2048 10\n"
                     "L 65 0 0 0 40 60 -5 -80\nL 65 1 40 0 80 60 -5 -80\nL 65 2 80 0 130 70 -10 -85\nL 65 3 130 0 190 80 -15 -90\n"
                     "L 66 0 200 0 240 60 -5 -80\nL 66 1 240 0 280 60 -5 -80\nL 66 2 280 0 330 70 -10 -85\nL 66 3 330 0 390 80 -15 -90\n"
                     "L 0 0 0 0 10 10 0 0\nL 65 4 0 0 10 10 0 0\nL 67 0 1000 0 1100 10 0 0\n"
                     "T 0 -5 35 1 65 0\nT 1 -5 35 1 66 0\nT 2 -5 75 2 65 0 66 40\nT 3 -5 75 2 65 0 300 40\nT 9 -5 35 1 65 0\n"
                     "T 4 -5 75 2 66 0 65 40\nT 5 -5 115 3 66 0 65 40 66 80\nT 6 -5 75 2 65 0 65 40\n"
                     "c 0 0\nc 1 1\nc 2 1\n"
                     "z 0 %08x %08x %08x %08x 2\nz 0 %08x %08x %08x %08x 4\nz 0 %08x %08x %08x %08x 7\ni 36 6\ni 1 6\n"
                     "a 0 5\na 1 3\na 5000 5\na 0 99\n",
                     FBits(0), FBits(-2000), FBits(-4000), FBits(-6000),
                     FBits(-800), FBits(-1200), FBits(-4800), FBits(-5200),
                     FBits(0), FBits(-10), FBits(0), FBits(-10));
            TitlesParse(tt, (unsigned)strlen(tt));
            int ti = g_ttTitles == 7 && g_ttLetters == 8 && g_ttSheetW == 1024 && g_ttSheetH == 512 && g_ttBase == 90 &&
                     g_ttCont[0] == 0 && g_ttCont[1] == 1 && g_ttZones == 2 && g_ttInsts == 1 && g_ttSubs == 2 &&
                     g_ttTitle[3].n == 0 && g_ttTitle[5].n == 3 && g_ttSub[0] == 5 && g_ttSub[1] == 3 && g_ttSub[2] == -1;
            /* the arrival's glow: a grid of 2 x 2 cells of 100 yards from x -6000, y -1100: the first row subzones 1, 2 */
            int keepItems = g_glowItems, keepSprites = g_glowSprites;
            unsigned short keepCells[4];
            memcpy(keepCells, g_glowCells, sizeof keepCells);
            __typeof__(g_glowMap[0]) keepGlow[2] = { g_glowMap[0], g_glowMap[1] };
            { const char *t = "s 0 0 0 64 32\ng 0 3ecccccd 3f000000 3eeb851f 3f07ae14\ng 0 3eeb851f 3f000000 3ef5c28f 3f051eb8\n";
              GlowParse(t, (unsigned)strlen(t)); }
            {
                unsigned char gb[6 + 18 + 8];
                float cell = 100, gx0 = -6000, gy0 = -1100;
                memcpy(gb, "CGLW\1\0", 6); gb[6] = 0; gb[7] = 0;
                memcpy(gb + 8, &cell, 4); memcpy(gb + 12, &gx0, 4); memcpy(gb + 16, &gy0, 4);
                gb[20] = 2; gb[21] = 0; gb[22] = 2; gb[23] = 0;
                unsigned char cells[8] = { 1, 0, 2, 0, 0, 0, 0, 0 };
                memcpy(gb + 24, cells, 8);
                ti &= GlowGridParse(gb, sizeof gb) && g_glowItems == 2;
            }
            int keepMap = g_destMap, keepId = g_titleMapId;
            float keepX = g_destWX, keepY = g_destWY;
            g_titlesOn = 1;
            memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
            g_titleMapId = 0; g_destMap = -1;
            ti &= TitleWant() == 0;
            g_bgUV[0] = g_bgUV[4] = 0.4f; g_bgUV[2] = g_bgUV[6] = 0.6f;            /* in close, h = 0.1, its width too */
            g_bgUV[5] = g_bgUV[7] = 0.34f; g_bgUV[1] = g_bgUV[3] = 0.54f;
            ti &= TitleWant() == 0;                                              /* the arrival not yet known */
            g_destMap = 0; g_destWX = -5000; g_destWY = -1000;
            ti &= TitleWant() == 4;                                              /* in both boxes: the smaller */
            g_destWX = -4500; g_destWY = -500;
            ti &= TitleWant() == 2;
            g_destWX = 100; g_destWY = 100;
            ti &= TitleWant() == 0;                                              /* in no box: the continent */
            g_destWX = -5950; g_destWY = -1050;
            ti &= TitleWant() == 5;                                              /* in a subzone, close in: its own */
            {                                                                    /* the glow's pick for the screen holds */
                DWORD since = g_destSince, gfor = g_glowFor; int pick = g_glowPick;
                g_destSince = 777; g_glowFor = 777; g_glowPick = 0;
                g_destWX = -5850;                                                /* (the arrival moved to no subzone) */
                ti &= TitleWant() == 5;
                g_destSince = since; g_glowFor = gfor; g_glowPick = pick; g_destWX = -5950;
            }
            g_destWY = -950;
            ti &= TitleWant() == 3;                                              /* (its title, empty as it is) */
            g_destWX = -5850; g_destWY = -1050;
            ti &= TitleWant() == 2;                                              /* a cell of no subzone: the zone's */
            g_destWX = -5950;
            g_bgUV[0] = g_bgUV[4] = 0.35f; g_bgUV[2] = g_bgUV[6] = 0.65f;          /* not yet that close: the zone's */
            ti &= TitleWant() == 2;
            g_bgUV[0] = g_bgUV[4] = 0.4f; g_bgUV[2] = g_bgUV[6] = 0.6f;
            g_glowMap[0].ok = 0;
            ti &= TitleWant() == 2;                                              /* no glow: the zone's */
            g_glowMap[0].ok = 1;
            g_titleMapId = 36; g_destMap = 0;
            ti &= TitleWant() == 6;                                              /* a dungeon: its own */
            memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
            ti &= TitleWant() == 0;                                              /* wide: its entrance's continent */
            g_titleMapId = 1; g_destMap = 1;
            ti &= TitleWant() == 1;
            unsigned tex = g_viewTex[VT_TITLES];
            g_viewTex[VT_TITLES] = 9;
            g_title.last = 0;
            ViewListStart(); TitleBuild(5120, 2160, 100000);
            ti &= g_vvN == 0;                                                    /* the first frame: not yet shown */
            ViewListStart(); TitleBuild(5120, 2160, 100080);                     /* coming in: a lot blurred, then a little */
            ti &= g_vvN == 8 && g_vb[g_vbN - 1].tex == VT_TITLES && fabsf(g_vv[0].u - 330 / 1024.0f) < 1e-6f &&
                  fabsf(g_vv[4].u - 280 / 1024.0f) < 1e-6f && g_vv[0].c[3] > 20 && g_vv[0].c[3] < 35 && g_vv[4].c[3] < 20;
            ViewListStart(); TitleBuild(5120, 2160, 100160);                     /* halfway: the little blur alone */
            ti &= g_vvN == 4 && fabsf(g_vv[0].u - 280 / 1024.0f) < 1e-6f && g_vv[0].c[3] > 120 && g_vv[0].c[3] < 136 &&
                  fabsf(g_vv[0].x - (2545 - 10)) < 0.01f && fabsf(g_vv[0].y - 81) < 0.01f &&
                  fabsf(g_vv[2].x - g_vv[0].x - 50) < 0.01f && fabsf(g_vv[2].y - g_vv[0].y - 70) < 0.01f &&
                  fabsf(g_vv[2].v - 70 / 512.0f) < 1e-6f;
            ViewListStart(); TitleBuild(5120, 2160, 100400);                     /* in: sharp, its back then its face */
            ti &= g_vvN == 8 && fabsf(g_vv[0].u - 200 / 1024.0f) < 1e-6f && fabsf(g_vv[4].u - 240 / 1024.0f) < 1e-6f &&
                  g_vv[0].c[3] == 255 && g_vv[4].c[3] == 255 && fabsf(g_vv[0].x - 2540) < 0.01f && fabsf(g_vv[0].y - 86) < 0.01f;
            ViewListStart(); TitleBuild(2560, 1080, 100500);                     /* at half the screen, half the size */
            ti &= g_vvN == 8 && fabsf(g_vv[0].x - 1270) < 0.01f && fabsf(g_vv[0].y - 43) < 0.01f &&
                  fabsf(g_vv[2].x - g_vv[0].x - 20) < 0.01f && fabsf(g_vv[2].y - g_vv[0].y - 30) < 0.01f;
            g_titleMapId = 0; g_destMap = 0;
            ViewListStart(); TitleBuild(5120, 2160, 101000);                     /* a change: the old still there, sharp, */
            ti &= g_vvN == 8 && fabsf(g_vv[0].u - 200 / 1024.0f) < 1e-6f && g_vv[0].c[3] == 255;
            ViewListStart(); TitleBuild(5120, 2160, 101110);                     /* blurring as it fades, the new not yet */
            ti &= g_vvN == 4 && fabsf(g_vv[0].u - 280 / 1024.0f) < 1e-6f && g_vv[0].c[3] > 120 && g_vv[0].c[3] < 136;
            ViewListStart(); TitleBuild(5120, 2160, 101225);
            ti &= g_vvN == 0;                                                    /* the old gone, the new about to come */
            ViewListStart(); TitleBuild(5120, 2160, 101380);
            ti &= g_vvN == 4 && fabsf(g_vv[0].u - 80 / 1024.0f) < 1e-6f;           /* the new, halfway in */
            g_bgUV[0] = g_bgUV[4] = 0.4f; g_bgUV[2] = g_bgUV[6] = 0.6f;            /* and on in to the zone, part way: */
            g_bgUV[5] = g_bgUV[7] = 0.34f; g_bgUV[1] = g_bgUV[3] = 0.54f;
            g_destWX = -4500; g_destWY = -500;
            ViewListStart(); TitleBuild(5120, 2160, 101390);
            ViewListStart(); TitleBuild(5120, 2160, 101450);                     /* the part-shown one goes from where it was */
            ti &= g_vvN == 8 && fabsf(g_vv[0].u - 130 / 1024.0f) < 1e-6f && fabsf(g_vv[4].u - 80 / 1024.0f) < 1e-6f;
            ViewListStart(); TitleBuild(5120, 2160, 101900);
            ti &= g_vvN == 16 && g_vv[0].c[3] == 255 && g_vv[15].c[3] == 255 &&   /* the zone's two letters: backs, then faces */
                  fabsf(g_vv[0].u - 0) < 1e-6f && fabsf(g_vv[4].u - 200 / 1024.0f) < 1e-6f && fabsf(g_vv[8].u - 40 / 1024.0f) < 1e-6f &&
                  fabsf(g_vv[4].x - g_vv[0].x - 40) < 0.01f;
            unsigned todStyle = g_todStyle; float todAt = g_todAt; TodLight tod = g_tod;
            g_todStyle |= TOD_GRADE; g_todAt = 120;
            for (int c = 0; c < 3; c++) g_tod.west[c] = g_tod.east[c] = 0.4f;
            unsigned char before[3] = { g_vv[4].c[0], g_vv[4].c[1], g_vv[4].c[2] };
            TodGrade(5120, 0, g_vvN);
            ti &= g_vv[4].c[0] == before[0] && g_vv[4].c[1] == before[1] && g_vv[4].c[2] == before[2];   /* not graded */
            g_todStyle = todStyle; g_todAt = todAt; g_tod = tod;
            LONG keepClass = g_class;
            g_class = 9;                                                         /* any class: the sheet's own colours */
            ViewListStart(); TitleBuild(5120, 2160, 101950);
            ti &= g_vvN >= 4 && g_vv[g_vvN - 4].c[0] == 255 && g_vv[g_vvN - 4].c[1] == 255 && g_vv[g_vvN - 4].c[2] == 255;
            g_class = keepClass;
            memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);                            /* out again: the continent's, */
            ViewListStart(); TitleBuild(5120, 2160, 102000);
            g_titleMapId = 1; g_destMap = 1;                                     /* and before the zone's has gone, Kalimdor's: */
            ViewListStart(); TitleBuild(5120, 2160, 102050);
            ViewListStart(); TitleBuild(5120, 2160, 102100);                     /* the zone's goes on going, no jump */
            ti &= g_vvN == 24 && fabsf(g_vv[0].u - 80 / 1024.0f) < 1e-6f && fabsf(g_vv[8].u - 0) < 1e-6f;
            ViewListStart(); TitleBuild(5120, 2160, 106000);                     /* a new screen: come in again */
            ti &= g_vvN == 0;
            ViewListStart(); TitleBuild(5120, 2160, 106400);
            ti &= g_vvN > 0;                                                     /* (shown again) */
            g_viewScreen++;                                                      /* the next screen, half a second on: */
            ViewListStart(); TitleBuild(5120, 2160, 106900);
            ti &= g_vvN == 0;                                                    /* nothing of the last one's name */
            g_viewTex[VT_TITLES] = tex; g_titlesOn = 0;
            g_destMap = keepMap; g_titleMapId = keepId; g_destWX = keepX; g_destWY = keepY;
            g_glowItems = keepItems; g_glowSprites = keepSprites; g_glowMap[0] = keepGlow[0]; g_glowMap[1] = keepGlow[1];
            memcpy(g_glowCells, keepCells, sizeof keepCells);
            memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
            Check(ti, "the place's name: titles.txt read (bad lines out, a bad title kept empty); the continent wide, the smallest zone in close, the subzone closer still, a dungeon's own and its entrance's continent; set out letter by letter at the top centre at the screen's scale; into focus as it fades in, blurred out before the next, a part-shown one going from where it was; begun again; in the sheet's own colours for any class; not graded by the hour");
        }

        /* the zone's line under its name (8 Oct night, his "word, build the zone level line"): read (bad lines out); the
         * zone's through its subzones, the dungeon's, none wide or with no line; the levels coloured as WoW colours a
         * quest against your level, the territory as its zone text (friendly, hostile, contested; neutral unknown); set
         * out levels, separator, territory, centred under the title at the line's baseline, each in its own colour */
        {
            char tt[2400];
            snprintf(tt, sizeof tt, "S 1024 512 90\n"
                     "L 65 0 0 0 40 60 -5 -80\nL 65 1 40 0 80 60 -5 -80\nL 65 2 80 0 130 70 -10 -85\nL 65 3 130 0 190 80 -15 -90\n"
                     "L 128 0 0 100 40 150 -3 -40\nL 128 1 40 100 80 150 -3 -40\nL 128 2 80 100 130 160 -8 -45\nL 128 3 130 100 190 170 -13 -50\n"
                     "T 0 -5 35 1 65 0\nT 1 -5 75 2 65 0 65 40\nT 2 -5 35 1 65 0\nT 3 -5 35 1 65 0\n"
                     "T 4 -3 67 2 128 0 128 30\nT 5 -3 97 3 128 0 128 30 128 60\nT 6 -3 97 3 128 0 128 30 128 60\n"
                     "T 7 -3 97 3 128 0 128 30 128 60\nT 8 -3 37 1 128 0\n"
                     "c 0 0\nz 0 %08x %08x %08x %08x 1\ni 36 3\nB 245 33\nB 0 33\n"
                     "y 0 5\ny 1 6\ny 2 7\ny 3 5\nj 8\n"
                     "v 1 4 1 10 1\nv 3 4 17 26 -1\nv 2 -1 0 0 -1\nv 0 4 5 1 0\nv 9 4 1 10 1\n"
                     "r 1 1\nr 2 2\nr 40 1\nr 3 3\n",
                     FBits(0), FBits(-2000), FBits(-4000), FBits(-6000));
            TitlesParse(tt, (unsigned)strlen(tt));
            int zl = g_ttTitles == 9 && g_ttLines == 2 && g_ttTerr[0] == 5 && g_ttTerr[1] == 6 && g_ttTerr[2] == 7 && g_ttSep == 8 &&
                     g_ttLineBase == 245 && g_ttLineGap == 33 && g_ttFaction[1] == 1 && g_ttFaction[2] == 2 && g_ttFaction[3] == 0 &&
                     g_ttLine[1].on && g_ttLine[3].on && !g_ttLine[2].on && !g_ttLine[0].on;
            int keepMap = g_destMap, keepId = g_titleMapId;
            float keepX = g_destWX, keepY = g_destWY;
            g_titlesOn = 1;
            memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
            g_titleMapId = 0; g_destMap = 0; g_destWX = -5000; g_destWY = -1000;
            zl &= LineWant() == -1;                                              /* wide: none */
            g_bgUV[0] = g_bgUV[4] = 0.47f; g_bgUV[2] = g_bgUV[6] = 0.53f;          /* in close (past the subzone's step too) */
            g_bgUV[5] = g_bgUV[7] = 0.34f; g_bgUV[1] = g_bgUV[3] = 0.54f;
            zl &= LineWant() == 1;                                               /* the zone's, the subzone or not */
            {
                int keepItems = g_glowItems, keepSprites = g_glowSprites;
                unsigned short keepCell = g_glowCells[0];
                __typeof__(g_glowMap[0]) keepGlow[2] = { g_glowMap[0], g_glowMap[1] };
                const char *gt = "s 0 0 0 64 32\ng 0 3ecccccd 3f000000 3eeb851f 3f07ae14\n";
                GlowParse(gt, (unsigned)strlen(gt));
                unsigned char gb[6 + 18 + 2] = { 'C', 'G', 'L', 'W', 1, 0, 0, 0 };
                float cell = 100, gx0 = -5050, gy0 = -1050;
                memcpy(gb + 8, &cell, 4); memcpy(gb + 12, &gx0, 4); memcpy(gb + 16, &gy0, 4);
                gb[20] = 1; gb[22] = 1; gb[24] = 1; gb[25] = 0;
                g_ttSub[0] = 2;                                                  /* (the subzone: glow 0, title 2) */
                zl &= GlowGridParse(gb, sizeof gb) && TitleWant() == 2 && LineWant() == 1;   /* a subzone's name, the zone's line */
                g_glowItems = keepItems; g_glowSprites = keepSprites; g_glowMap[0] = keepGlow[0]; g_glowMap[1] = keepGlow[1];
                g_glowCells[0] = keepCell;
            }
            g_destWX = 100;
            zl &= LineWant() == -1;                                              /* in no zone */
            g_titleMapId = 36;
            zl &= LineWant() == 3;                                               /* a dungeon's */
            g_titleMapId = 0; g_destWX = -5000;
            /* the levels' colours: red 5 and more above you, orange 3, yellow in reach, green below, grey all grey */
            g_level = 0;  zl &= LineLevelColour(1, 10) == LINE_NEUTRAL;
            g_level = 1;  zl &= LineLevelColour(10, 20) == 0xFF1A1Au;
            g_level = 7;  zl &= LineLevelColour(10, 20) == 0xFF8040u;
            g_level = 5;  zl &= LineLevelColour(1, 10) == 0xFFFF00u;
            g_level = 13; zl &= LineLevelColour(1, 10) == 0x40BF40u;
            g_level = 25; zl &= LineLevelColour(1, 10) == 0x808080u;
            g_level = 60; zl &= LineLevelColour(45, 50) == 0x40BF40u && LineLevelColour(40, 48) == 0x808080u && LineLevelColour(60, 60) == 0xFFFF00u;
            g_level = 5;  zl &= LineLevelColour(10, 20) == 0xFF1A1Au;                 /* the edges: five above, red */
            g_level = 6;  zl &= LineLevelColour(10, 20) == 0xFF8040u;                 /* four, orange */
            g_level = 8;  zl &= LineLevelColour(10, 20) == 0xFFFF00u;                 /* two, yellow */
            g_level = 12; zl &= LineLevelColour(1, 10) == 0xFFFF00u;                  /* the top two below you: yellow */
            g_level = 25; zl &= LineLevelColour(1, 18) == 0x808080u && LineLevelColour(1, 19) == 0x40BF40u;   /* grey at 18 */
            /* the territory's: contested orange always; friendly green, hostile red once the race is known */
            g_race = 0; zl &= LineTerrColour(0) == 0xFFB300u && LineTerrColour(1) == LINE_NEUTRAL;
            g_race = 1; zl &= LineTerrColour(1) == 0x1AFF1Au && LineTerrColour(2) == 0xFF1A1Au;
            g_race = 2; zl &= LineTerrColour(1) == 0xFF1A1Au && LineTerrColour(2) == 0x1AFF1Au;
            /* drawn: a dwarf-ish Alliance character of level 5 in an Alliance zone of 1-10 */
            g_race = 1; g_level = 5;
            unsigned tex = g_viewTex[VT_TITLES];
            g_viewTex[VT_TITLES] = 9;
            g_title.last = g_line.last = 0;
            ViewListStart(); TitleBuild(5120, 2160, 200000);
            zl &= g_vvN == 0;
            ViewListStart(); TitleBuild(5120, 2160, 200400);
            zl &= g_vvN == 64 &&                                                 /* the name's 4 quads, the line's 12 */
                  fabsf(g_vv[16].x - 2422) < 0.01f && fabsf(g_vv[16].y - 281) < 0.01f &&      /* the levels first, centred */
                  g_vv[16].c[0] == 255 && g_vv[16].c[1] == 255 && g_vv[16].c[2] == 0 &&       /* yellow: in reach */
                  fabsf(g_vv[24].x - 2525) < 0.01f && g_vv[24].c[0] == 255 && g_vv[24].c[1] == 237 && g_vv[24].c[2] == 194 &&
                  fabsf(g_vv[28].x - 2598) < 0.01f && g_vv[28].c[0] == 26 && g_vv[28].c[1] == 255 && g_vv[28].c[2] == 26 &&
                  fabsf(g_vv[40].x - 2422) < 0.01f && fabsf(g_vv[40].u - 40 / 1024.0f) < 1e-6f;   /* all backs, then faces */
            g_titleMapId = 36;                                                   /* a dungeon: its levels alone, no separator */
            ViewListStart(); TitleBuild(5120, 2160, 210000);
            ViewListStart(); TitleBuild(5120, 2160, 210400);
            zl &= g_vvN == 8 + 16 && fabsf(g_vv[8].x - 2525) < 0.01f && g_vv[8].c[0] == 255 && g_vv[8].c[1] == 26;   /* (17-26 at 5: red) */
            g_viewTex[VT_TITLES] = tex; g_titlesOn = 0;
            g_race = g_level = 0;
            g_destMap = keepMap; g_titleMapId = keepId; g_destWX = keepX; g_destWY = keepY;
            memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
            Check(zl, "the zone's line: read (bad lines out); the zone's through its subzones, a dungeon's, none wide or out of a zone; levels coloured as WoW colours a quest against yours, territory friendly, hostile or contested (neutral unknown); levels, separator, territory centred under the name, each its own colour; a dungeon's levels alone");
        }

        /* the world's first frame: the view waits for it (the blur on, none seen yet, under VIEW_WORLD_WAIT_MS); a screen
         * that faded with none seen all session teaches it the hook is blind, and it waits no more (8 Oct night) */
        {
            LONG blurOn = g_blurOn, frames = g_worldFrames, blind = g_worldBlind;
            g_blurOn = 1; g_worldBlind = 0; g_worldFrames = 0;
            int wf = !ViewFadeDue(1000, 900, 0) && ViewFadeDue(900 + VIEW_WORLD_WAIT_MS, 900, 0);
            g_worldFrames = 5;
            wf &= !ViewWorldBlind(5) && !g_worldBlind;                           /* frames seen this session: not blind */
            wf &= ViewFadeDue(1000, 900, 4);                                     /* (one seen since it went: due) */
            g_worldFrames = 0;
            wf &= ViewWorldBlind(0) && g_worldBlind && !ViewWorldBlind(0);       /* none all session: blind, once */
            wf &= ViewFadeDue(1000, 900, 0);                                     /* and then due at once */
            g_blurOn = blurOn; g_worldFrames = frames; g_worldBlind = blind;
            Check(wf, "the world's first frame: waited for, up to VIEW_WORLD_WAIT_MS; a screen that faded with no frame of the game seen all session makes the view fade as each screen goes from then on");
        }

        /* vsync promised but not kept: thirty frames in a row under 2 ms and the view paces itself; one slower frame
         * starts the count again; a 500 Hz screen's 2 ms frames never count (8 Oct night) */
        {
            int fast = 0, decided = 0, at = -1;
            for (int f = 0; f < 29; f++) decided |= ViewSpinStep(&fast, 1);
            int vs = !decided;
            ViewSpinStep(&fast, 5);                                              /* a slow one: start again */
            for (int f = 0; f < 40 && at < 0; f++) if (ViewSpinStep(&fast, 0)) at = f;
            vs &= at == 29;
            fast = 0; decided = 0;
            for (int f = 0; f < 200; f++) decided |= ViewSpinStep(&fast, 2);    /* 500 Hz, waiting as it should */
            vs &= !decided;
            Check(vs, "vsync promised but not kept: thirty frames in a row quicker than any screen and the view paces itself; a slower frame starts the count again; 500 Hz never counts");
        }

        /* the game's own frames leave the Maelstrom and the logo to the view only while it will draw the screen: up, and
         * drawn it or in its first moments; a screen it has not taken by then, or the view off, keeps them (8 Oct night) */
        {
            LONG live = g_viewLive, drew = g_viewDrew;
            DWORD at = g_dispAt;
            g_dispAt = 50000;
            g_viewLive = 0; g_viewDrew = 0;
            int vw = !ViewWillDraw(50100);                                       /* the view off: the game's own */
            g_viewLive = 1;
            vw &= ViewWillDraw(50100) && ViewWillDraw(50000 + VIEW_TAKE_MS - 1); /* its first moments: the view's */
            vw &= !ViewWillDraw(50000 + VIEW_TAKE_MS + 1);                       /* not taken by then: the game's own */
            g_viewDrew = 1;
            vw &= ViewWillDraw(90000);                                           /* drawn: the view's, however long */
            g_viewLive = live; g_viewDrew = drew; g_dispAt = at;
            Check(vw, "the game's frames leave the Maelstrom and logo to the view only while it will draw the screen");
        }

        /* the swells bend round it, the more the nearer, and not at all beyond its reach */
        static const unsigned char allSea[8 + 16] = { 'C', 'S', 'E', 'A', 4, 0, 4, 0, 255, 255, 255, 255, 255, 255, 255, 255,
                                                      255, 255, 255, 255, 255, 255, 255, 255 };
        SeaParse(allSea, sizeof allSea);
        g_scrW = g_surfW = 3840; g_scrH = g_surfH = 2160;
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        g_mael.u = 0.5f; g_mael.v = 0.5f; g_mael.r = 0.08f; g_mael.ok = 1;
        float nearOn = WaveLight(0.5f * 1.778f + 0.04f, 0.53f, 20.0f, 0.35f, 1.778f), farOn = WaveLight(0.1f, 0.1f, 20.0f, 0.35f, 1.778f);
        g_mael.ok = 0;
        int sw = WaveLight(0.5f * 1.778f + 0.04f, 0.53f, 20.0f, 0.35f, 1.778f) != nearOn &&
                 WaveLight(0.1f, 0.1f, 20.0f, 0.35f, 1.778f) == farOn;
        float x = 0.5f * 1.778f + 0.05f, v = 0.5f, x0 = x, v0 = v;
        g_mael.ok = 1;
        SwirlAt(&x, &v, 1.778f);                                         /* east of it: turned clockwise on screen, so v grows less... */
        float dx = x - 0.5f * 1.778f, dv = v - 0.5f;
        sw &= fabsf(dx * dx + dv * dv - 0.0025f) < 1e-5f && dv < 0 && (x0 != x || v0 != v);   /* ...it moves up: v falls */
        Check(sw, "the waves' swells bend round the Maelstrom near it (keeping their distance from its middle, clockwise inward), and not far off");

        /* the ripple: nothing until the arrival is known; then rings at it, square, opening and fading, appearing over RIPPLE_IN */
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        g_destOk = 0;
        RippleBuild(1.0f, 50000);
        int rp = g_mkQuads == 0;
        g_destOk = 1; g_destU = 0.3f; g_destV = 0.6f; g_destSince = 50000 - 2000;
        RippleBuild(1.5f, 50000);                                       /* (no ring being born at 1.5 s: phases 1/8 apart) */
        rp &= g_mkQuads == RIPPLE_RINGS;
        float lastR = 0, lastA = 2;
        int ordered = 1;
        for (int q = 0; q < g_mkQuads; q++) {
            int slot = q ? q * 6 : 0;
            float cx = (g_mkPos[slot * 3] + g_mkPos[(slot + 1) * 3]) / 2, r = (g_mkPos[(slot + 2) * 3 + 1] - g_mkPos[slot * 3 + 1]) / 2;
            float a = (g_mkCol[slot] >> 24) / 255.0f;
            rp &= fabsf(cx - 0.3f) < 1e-4f && fabsf(g_mkPos[slot * 3 + 1] + r - 0.4f) < 1e-4f && r <= RIPPLE_PX / 2160 + 1e-5f &&
                  r >= RIPPLE_PX * 0.12f / 2160 - 1e-5f && fabsf((g_mkPos[(slot + 1) * 3] - g_mkPos[slot * 3]) * 3840 / 2160 - 2 * r) < 1e-4f;
            if (q && (r > lastR) == (a > lastA)) ordered = 0;          /* the wider a ring, the fainter */
            lastR = r; lastA = a;
        }
        rp &= ordered;
        g_destSince = 50000 - 100;                                       /* known a tenth of a second ago: faint yet */
        RippleBuild(1.5f, 50000);
        int faint = 1;
        for (int q = 0; q < g_mkQuads; q++) faint &= (g_mkCol[q ? q * 6 : 0] >> 24) < 60;
        rp &= faint;
        g_destSince = 50000 - 2000;                                      /* his ask, 4 Oct: it keeps going as the view closes in */
        g_bgUV[0] = g_bgUV[4] = 0.29f; g_bgUV[2] = g_bgUV[6] = 0.31f;
        RippleBuild(1.5f, 50000);
        int near = g_mkQuads == RIPPLE_RINGS, bright = 0;
        for (int q = 0; q < g_mkQuads; q++) bright |= (g_mkCol[q ? q * 6 : 0] >> 24) > 150;
        rp &= near && bright && RIPPLE_PX >= 200;
        /* his ask, 7 Oct: the pulse round the character in the class's colour (a druid's orange, a hunter's green), the
         * ring's old gold while the class is not known */
        LONG was = g_class;
        g_class = 11; RippleBuild(1.5f, 50000);
        int tinted = g_mkQuads > 0 && (g_mkCol[0] & 0xFFFFFFu) == SaberVivid(kClassColour[11]);
        g_class = 3; RippleBuild(1.5f, 50000);
        tinted &= g_mkQuads > 0 && (g_mkCol[0] & 0xFFFFFFu) == SaberVivid(kClassColour[3]);
        g_class = 0; RippleBuild(1.5f, 50000);
        tinted &= g_mkQuads > 0 && (g_mkCol[0] & 0xFFFFFFu) == 0xFFE49Cu;
        g_class = was;
        Check(tinted, "the ripple round the arrival in the class's colour (a druid's orange, a hunter's green), the ring's old gold while the class is not known");
        g_destOk = 0; g_mael.ok = 0; g_seaW = 0;
        Check(rp, "the ripple: none before the arrival is known; then rings at it, square, the wider the fainter, faint at first, still there and bright close in");
    }

    /* the badge: the class read from the character list (a mage, the second of three), kept when the list goes, a bad
     * byte never taken; drawn shadow, icon, ring at the arrival, a steady size; the ring alone with no class; and none of
     * it read when the code the addresses came from has changed */
    {
        Fresh("badge");
        PackLive(1 | 2 | 4 | 8 | 64, 0);
        Start();
        int bd = g_marksOn && g_classSure && !LogSays("badge: the character list is not where");
        Poke(CHAR_COUNT_VA, 3); Poke(CHAR_LIST_VA, 0x8A0000); Poke(CHAR_PICKED_VA, 1); Poke(CHAR_ENTERED_VA, 0x8A0000 + CHAR_SIZE);
        shim_module[0x8A0000 - 0x400000 + 0 * 0x120 + 0x101] = 1;     /* the layout read from GetCharacterInfo */
        shim_module[0x8A0000 - 0x400000 + 1 * 0x120 + 0x101] = 8;
        shim_module[0x8A0000 - 0x400000 + 2 * 0x120 + 0x101] = 6;     /* no class 6 in 1.12 */
        shim_module[0x8A0000 - 0x400000 + 3 * 0x120 + 0x101] = 4;     /* past the list's end: never read */
        shim_module[0x8A0000 - 0x400000 + 1 * 0x120 + 0x100] = 7;     /* the mage a gnome, level 23 (the zone's line) */
        shim_module[0x8A0000 - 0x400000 + 1 * 0x120 + 0x108] = 23;
        shim_module[0x8A0000 - 0x400000 + 0 * 0x120 + 0x100] = 0;     /* the first: no race, a level past any */
        shim_module[0x8A0000 - 0x400000 + 0 * 0x120 + 0x108] = 200;
        g_race = g_level = 0;
        ClassStep();
        bd &= g_class == 8 && g_classFrom == 1 && g_classEntered == 8 && g_race == 7 && g_level == 23;
        Poke(CHAR_PICKED_VA, 0);
        ClassStep();
        bd &= g_class == 1 && g_race == 0 && g_level == 0;                /* a bad race or level: none, never another's */
        Poke(CHAR_PICKED_VA, 1);
        ClassStep();
        Poke(CHAR_PICKED_VA, 3);
        ClassStep();
        bd &= g_class == 8 && g_classFrom == 2;                          /* a pick past the end: not read */
        Poke(CHAR_PICKED_VA, 1);
        Poke(CHAR_PICKED_VA, 2);
        ClassStep();
        bd &= g_class == 8 && g_classFrom == 2;                          /* a bad byte: the mage kept */
        Poke(CHAR_PICKED_VA, 1); Poke(CHAR_COUNT_VA, 0);                /* the list gone: kept, and said so */
        ClassStep();
        bd &= g_class == 8 && g_classFrom == 2;
        g_scrW = g_surfW = 3840; g_scrH = g_surfH = 2160;
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        g_destOk = 0;
        BadgeBuild(90000);
        bd &= g_mkQuads == 0;
        g_destOk = 1; g_destU = 0.3f; g_destV = 0.6f; g_destSince = 80000;
        BadgeBuild(90000);
        float hs = BADGE_PX / 2160.0f;
        bd &= g_mkQuads == 3 && g_mkUV[0] == 0.0f && g_mkUV[1] == 1.0f &&                    /* first the shadow (0,384..128,512) */
              fabsf(g_mkUV[(6 + 0) * 2] - 384.0f / 512) < 1e-6f &&                          /* then the mage's icon */
              fabsf(g_mkUV[(12 + 0) * 2 + 1] - 384.0f / 512) < 1e-6f &&                     /* then the ring (its bottom at 384) */
              fabsf((g_mkPos[12 * 3] + g_mkPos[13 * 3]) / 2 - 0.3f) < 1e-5f &&            /* centred on the arrival */
              fabsf(g_mkPos[14 * 3 + 1] - g_mkPos[12 * 3 + 1] - 2 * hs) < 1e-5f && (g_mkCol[12] >> 24) == 255;
        {
            char lm[220];
            g_mapShown = 1; g_classFrom = 1; g_classEntered = 8;
            BadgeLog(lm);
            g_classFrom = 2; g_classEntered = 0;
            BadgeLog(lm);
            g_mapShown = 0;
            bd &= LogSays("its badge: mage, from the character list; the entered character's own entry says mage") &&
                  LogSays("its badge: mage, kept from an earlier screen (the list is not there now); the entered character's own entry says none");
        }
        g_class = 0;
        BadgeBuild(90000);
        bd &= g_mkQuads == 2;                                            /* no class: the shadow and the ring */
        /* the code moved: nothing read, said so */
        Fresh("badge-moved");
        PackLive(1 | 2 | 4 | 8 | 64, 0);
        shim_module[kClassCode[3].rva + 3] ^= 0x02;                       /* the class at another offset */
        Start();
        bd &= !g_classSure && LogSays("badge: the character list is not where it is in the client this was made for");
        Poke(CHAR_COUNT_VA, 3); Poke(CHAR_LIST_VA, 0x8A0000); Poke(CHAR_PICKED_VA, 1);
        shim_module[0x8A0000 - 0x400000 + 1 * 0x120 + 0x101] = 8;
        ClassStep();
        bd &= g_class == 0;
        g_destOk = 0;
        /* the addresses and layout the DLL uses are the ones in the bytes checked against the real client */
        unsigned at[4];
        memcpy(&at[0], kClassCode[0].bytes + 2, 4); memcpy(&at[1], kClassCode[1].bytes + 2, 4);
        memcpy(&at[2], kClassCode[4].bytes + 1, 4); memcpy(&at[3], kClassCode[5].bytes + 1, 4);
        bd &= at[0] == CHAR_COUNT_VA && at[1] == CHAR_LIST_VA && at[2] == CHAR_PICKED_VA && at[3] == CHAR_ENTERED_VA &&
              (kClassCode[3].bytes[3] | kClassCode[3].bytes[4] << 8) == CHAR_CLASS && 9 * (1 << kClassCode[2].bytes[5]) == CHAR_SIZE &&
              (kClassCode[9].bytes[3] | kClassCode[9].bytes[4] << 8) == CHAR_RACE &&
              (kClassCode[10].bytes[3] | kClassCode[10].bytes[4] << 8) == CHAR_LEVEL &&
              kClassCode[6].bytes[2] == CHAR_MAP && kClassCode[7].bytes[2] == CHAR_X && kClassCode[8].bytes[2] == CHAR_Y && kClassCode[11].bytes[2] == CHAR_Z;
        g_race = g_level = 0;
        Check(bd, "the badge: the class from the list (a bad byte never taken, kept when the list goes), its race and level with it; shadow, icon, ring at the arrival; the ring alone without; off when the code moved");
    }

    /* his find, 4 Oct: three characters in three places all showed at Alterac (the game's arrival read 0, 0 at a login).
     * A login takes the character list's own place at once; a teleport waits for the game to write the new place (the
     * old one is the last screen's); from half the bar, the game's place as it stands; never 0, 0 */
    {
        Fresh("arrival");
        PackLive(1 | 2 | 4 | 64, 0);
        Start();
        g_mapShown = 1;
        { const char *mt = "map 0 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\n"; MapParse(mt, (unsigned)strlen(mt)); }
        Poke(BAR_MAP_VA, 0);
        Poke(MAP_DEST_MAP_VA, 0); PutFloat(MAP_DEST_X_VA, 0); PutFloat(MAP_DEST_Y_VA, 0);   /* nothing written yet */
        Poke(CHAR_COUNT_VA, 2); Poke(CHAR_LIST_VA, 0x8B0000); Poke(CHAR_PICKED_VA, 1);
        unsigned e = 0x8B0000 + 0x120;                                  /* the list's own layout, read from its parser */
        Poke(e + 0x38, 0); PutFloat(e + 0x44, -5000); PutFloat(e + 0x48, 10000); PutFloat(e + 0x4C, 501.5f);
        int ar = g_classSure;
        g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; g_driftNew = 1;
        DWORD at = 900000;
        DriftStep(0, at);
        ar &= g_destOk && fabsf(g_destU - 0.3f) < 1e-4f && fabsf(g_destV - 0.6f) < 1e-4f && g_destMap == 0 &&
              g_destWX == -5000 && g_destWY == 10000 && g_destZOk && g_destWZ == 501.5f;   /* a login: the list's place, at once, its height */
        /* a teleport: no list; the old place (the last screen's) is not taken, the new one is the moment it lands */
        Poke(CHAR_COUNT_VA, 0);
        PutFloat(MAP_DEST_X_VA, -5000); PutFloat(MAP_DEST_Y_VA, 10000);  /* where the last screen arrived */
        g_driftNew = 1;
        at += 20; DriftStep(0.1f, at);
        ar &= !g_destOk;
        at += 20; DriftStep(0.2f, at);
        ar &= !g_destOk;
        PutFloat(MAP_DEST_X_VA, -2500); PutFloat(MAP_DEST_Y_VA, 5000);   /* the game writes the new place */
        PutFloat(MAP_DEST_Z_VA, -52.25f);
        at += 20; DriftStep(0.2f, at);
        ar &= g_destOk && fabsf(g_destU - 0.4f) < 1e-4f && fabsf(g_destV - 0.55f) < 1e-4f && !g_destZOk;   /* (its height's code not as read) */
        for (int i = 0; i < 2; i++) memcpy(shim_module + kDestZCode[i].rva, kDestZCode[i].bytes, (size_t)kDestZCode[i].n);
        g_destZSure = -1;
        at += 20; DriftStep(0.25f, at);
        ar &= g_destOk && g_destZOk && g_destWZ == -52.25f;               /* (as read: its height with it) */
        shim_module[kDestZCode[1].rva + 2] ^= 0x01;                       /* (written somewhere else: not taken) */
        g_destZSure = -1;
        at += 20; DriftStep(0.26f, at);
        ar &= g_destOk && !g_destZOk;
        shim_module[kDestZCode[1].rva + 2] ^= 0x01;
        g_destZSure = -1;
        /* the same place again (a hearth to where you stand): taken from half the bar */
        g_driftNew = 1;
        at += 20; DriftStep(0.3f, at);
        ar &= !g_destOk;
        at += 20; DriftStep(0.6f, at);
        ar &= g_destOk && fabsf(g_destU - 0.4f) < 1e-4f;
        /* never 0, 0, not even past half the bar */
        PutFloat(MAP_DEST_X_VA, 0); PutFloat(MAP_DEST_Y_VA, 0);
        g_driftNew = 1;
        at += 20; DriftStep(0.9f, at);
        at += 20; DriftStep(1.0f, at);
        ar &= !g_destOk;
        /* the list's place on another map than the screen's: not taken */
        Poke(CHAR_COUNT_VA, 2); Poke(e + 0x38, 1);
        g_driftNew = 1;
        at += 20; DriftStep(0.1f, at);
        ar &= !g_destOk;
        /* a pick past the list's end, or the list's code not as read: not taken */
        Poke(e + 0x38, 0);
        unsigned past = 0x8B0000 + 2 * 0x120;
        Poke(past + 0x38, 0); PutFloat(past + 0x44, -5000); PutFloat(past + 0x48, 10000);
        Poke(CHAR_PICKED_VA, 2);
        g_driftNew = 1;
        at += 20; DriftStep(0.1f, at);
        ar &= !g_destOk;
        Poke(CHAR_PICKED_VA, 1);
        g_classSure = 0;
        g_driftNew = 1;
        at += 20; DriftStep(0.1f, at);
        ar &= !g_destOk;
        g_classSure = 1;
        g_driftNew = 1;
        at += 20; DriftStep(0.1f, at);
        ar &= g_destOk;                                                  /* (and with it as read, taken) */
        Poke(CHAR_COUNT_VA, 0); g_mapShown = 0; g_destOk = 0;
        Check(ar, "the arrival: a login's from the character list at once; a teleport's only once the game writes it (or from half the bar); never 0, 0; its height with it (the list's +0x4C, the game's 0x882694), the game's only where WoW.exe reads and writes it as in the client this was made for");
    }

    /* the arrival's glow: on with its sheet, table and grid (a grid a byte short: off); its subzone found from the arrival's
     * world place; lit as the view closes in; the round glow where no subzone is */
    {
        Fresh("glow-short");
        PackLive(1 | 2 | 4 | 64 | 128 | 256, 0);
        Start();
        int gw = !g_glowOn;
        Fresh("glow");
        PackLive(1 | 2 | 4 | 64 | 128, 0);
        Start();
        gw &= g_glowOn && g_glowItems == 2 && g_glowSprites == 2 &&
              LogSays("overlay on: the Maelstrom, the ripple and the badge, the arrival's glow, drawn over");
        gw &= GlowFind(0, 1150, 2150) == 0 && GlowFind(0, 1150, 2250) == 0 && GlowFind(0, 1150, 2350) == 1 &&
              GlowFind(0, 1050, 2150) == -1 && GlowFind(0, 1150, 1999) == -1 && GlowFind(0, 1301, 2150) == -1 &&
              GlowFind(1, 1150, 2150) == -1 && GlowFind(0, 1050, 2550) == -1;   /* past the last column: never the next row's */
        {
            unsigned char longer[sizeof kGlowBin + 1];
            memcpy(longer, kGlowBin, sizeof kGlowBin); longer[sizeof kGlowBin] = 0;
            gw &= !GlowGridParse(longer, sizeof longer) && GlowGridParse(kGlowBin, sizeof kGlowBin);   /* a byte too many: off */
        }
        { const char *t = "s 0 0 0 64 32\ns 2 0 0 8 8\ng 0 3f000000 3f000000 3f100000 3f100000\ng 5 0 0 0 0\n";   /* a bad g line: none */
          GlowParse(t, (unsigned)strlen(t)); }
        gw &= g_glowSprites == 1 && g_glowItems == 0;
        { const char *t = "s 0 0 0 64 32\ns 1 64 0 128 64\ng 0 3ecccccd 3f000000 3eeb851f 3f07ae14\ng 1 3eeb851f 3f000000 3ef5c28f 3f051eb8\n";
          GlowParse(t, (unsigned)strlen(t)); }
        gw &= g_glowItems == 2;
        /* the arrival in the first subzone: x 1150, y 2250 */
        g_destMap = 0; g_destWX = 1150; g_destWY = 2250;              /* the arrival's world place, as MapPlace keeps it */
        g_destOk = 1; g_destU = 0.43f; g_destV = 0.515f; g_destSince = 200000; g_glowFor = 0; g_glowPick = -2;
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);                    /* far out: not lit yet */
        GlowBuild(201000);
        gw &= g_glowPick == 0 && g_glowQuads == 0;
        g_bgUV[0] = g_bgUV[4] = 0.37f; g_bgUV[2] = g_bgUV[6] = 0.49f;  /* close in, h = 0.06: lit, over its own rectangle */
        g_bgUV[5] = g_bgUV[7] = 0.455f; g_bgUV[1] = g_bgUV[3] = 0.575f;
        GlowBuild(201000);
        gw &= g_glowQuads == 1 && (g_glowCol[0] >> 24) == 255 && fabsf(g_glowPos[0] - (0.40f - 0.37f) / 0.12f) < 1e-4f &&
              fabsf(g_glowPos[3] - (0.46f - 0.37f) / 0.12f) < 1e-4f && fabsf(g_glowPos[7] - (1 - (0.50f - 0.455f) / 0.12f)) < 1e-4f &&
              fabsf(g_glowPos[1] - (1 - (0.53f - 0.455f) / 0.12f)) < 1e-4f && g_glowUV[0] == 0 && g_glowUV[2] == 64.0f / 2048;
        GlowBuild(200100);                                               /* known a tenth of a second ago: faint yet */
        gw &= g_glowQuads == 1 && (g_glowCol[0] >> 24) < 60;
        /* nowhere a subzone is: the round glow, from the marks, its size in the world's */
        g_destWX = 1050; g_destSince = 300000;
        GlowBuild(301000);
        gw &= g_glowPick == -1 && g_glowQuads == 0 && g_mkQuads == 0;    /* no subzone, and no round glow in this pack: nothing */
        g_mark[3].ok = 1; g_mark[3].x0 = 128; g_mark[3].y0 = 256; g_mark[3].x1 = 256; g_mark[3].y1 = 384;
        g_map.ok = 1; g_map.m[0].ok = 1; g_map.m[0].bv = -2e-5f;
        GlowBuild(301000);
        gw &= g_glowQuads == 0 && g_mkQuads == 1 && fabsf((g_mkPos[7] - g_mkPos[1]) - GLOW_YARDS * 2e-5f / 0.12f) < 1e-4f;
        g_destOk = 0;
        Check(gw, "the glow: on with its files (a short grid: off); the arrival's subzone from its world place; lit closing in, faint at first; the round glow elsewhere");
    }

    /* sea.bin: anything but a whole table is left out; between the cells' middles, how open the sea is */
    {
        unsigned char bad[8 + 16];
        memcpy(bad, kSeaBin, sizeof bad);
        int sea = SeaParse(bad, sizeof bad) && g_seaW == 4 && g_seaH == 4;
        bad[0] = 'X';
        sea &= !SeaParse(bad, sizeof bad) && g_seaW == 0;
        memcpy(bad, kSeaBin, sizeof bad);
        bad[5] = 1;                                                   /* 260 wide */
        sea &= !SeaParse(bad, sizeof bad);
        sea &= !SeaParse(kSeaBin, sizeof kSeaBin - 1);
        unsigned char longer[8 + 17];
        memcpy(longer, kSeaBin, sizeof kSeaBin); longer[sizeof kSeaBin] = 0;
        sea &= !SeaParse(longer, sizeof longer);                      /* a byte too many */
        sea &= SeaParse(kSeaBin, sizeof kSeaBin);
        sea &= SeaAt(0.125f, 0.5f) == 0 && SeaAt(0.875f, 0.5f) == 1 && fabsf(SeaAt(0.5f, 0.5f) - 0.5f) < 1e-5f &&
               SeaAt(-0.1f, 0.5f) == 0 && SeaAt(1.1f, 0.5f) == 0 && SeaAt(0.99f, 0.01f) == 1;
        sea &= SeaFade(0.5f) == 1 && SeaFade(0.30f) == 1 && fabsf(SeaFade(0.22f) - 0.5f) < 1e-5f && SeaFade(0.14f) == 0 &&
               SeaFade(0.06f) == 0;
        float worst = 0;
        for (float x = -60; x < 60; x += 0.0137f) { float d = fabsf(SeaSin(x) - sinf(x)); if (d > worst) worst = d; }
        sea &= worst < 2e-5f;
        Check(sea, "sea.bin read only whole; how open the sea is, between cells, 0 off the picture; full far out, gone close in; a sine for any angle");
    }

    /* the waves, take two (his word on the swells: "continent sized waves"): tiles of small crests over the open sea only,
     * their cells the same size on screen at the whole world and at half of it (the next octave), drifting; none without sea.bin */
    {
        SeaParse(kSeaBin, sizeof kSeaBin);                               /* the left half land, the right half sea */
        g_scrW = g_surfW = 3840; g_scrH = g_surfH = 2160;
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);                       /* the whole world: one octave */
        WaveBuild(20.0f);
        int wv = g_wvQuads > 50 && g_wvQuads <= WAVE_QUADS, landLit = 0, uvOut = 0, slots = g_wvQuads * 6 - 2;
        for (int k = 0; k < slots; k++) {
            if (g_wvPos[k * 3] < 0.3f && (g_wvCol[k] >> 24)) landLit++;
            if (g_wvUV[k * 2] < 0 || g_wvUV[k * 2] > 1 || g_wvUV[k * 2 + 1] < 0 || g_wvUV[k * 2 + 1] > 1) uvOut++;
        }
        float cellWhole = (float)sqrt((g_wvPos[6] - g_wvPos[0]) * (g_wvPos[6] - g_wvPos[0]) * 1.778f * 1.778f +
                                      (g_wvPos[7] - g_wvPos[1]) * (g_wvPos[7] - g_wvPos[1]));   /* its left side, in screen heights */
        float xsum = 0;
        for (int k = 0; k < slots; k++) xsum += g_wvPos[k * 3];
        wv &= !landLit && !uvOut && fabsf(cellWhole - WAVE_TILE / 8) < 0.002f;
        g_bgUV[0] = g_bgUV[4] = 0.25f; g_bgUV[2] = g_bgUV[6] = 0.75f;   /* half the world: the next octave, the same size on screen */
        g_bgUV[5] = g_bgUV[7] = 0.25f; g_bgUV[1] = g_bgUV[3] = 0.75f;
        WaveBuild(20.0f);
        float cellHalf = (float)sqrt((g_wvPos[6] - g_wvPos[0]) * (g_wvPos[6] - g_wvPos[0]) * 1.778f * 1.778f +
                                     (g_wvPos[7] - g_wvPos[1]) * (g_wvPos[7] - g_wvPos[1]));
        wv &= g_wvQuads > 0 && fabsf(cellHalf - cellWhole) < 0.002f;
        (void)xsum;
        {                                                                 /* drifting: all sea, so every cell is drawn and only */
            static const unsigned char allSeaW[8 + 16] = { 'C', 'S', 'E', 'A', 4, 0, 4, 0, 255, 255, 255, 255, 255, 255, 255, 255,
                                                           255, 255, 255, 255, 255, 255, 255, 255 };   /* the drift moves them */
            SeaParse(allSeaW, sizeof allSeaW);
            memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
            WaveBuild(20.0f);
            float x20 = g_wvPos[0], y20 = g_wvPos[1];
            WaveBuild(21.0f);
            float x21 = g_wvPos[0], y21 = g_wvPos[1];
            float moved = (float)sqrt((x21 - x20) * (x21 - x20) * 1.778f * 1.778f + (y21 - y20) * (y21 - y20));
            float S = WAVE_TILE, want = WAVE_DRIFT * S * (float)sqrt(1 + 0.35f * 0.35f);   /* a second's drift, in screen heights */
            wv &= fabsf(moved - want) < 0.002f || fabsf(moved - want) > S / 8 - 0.002f;  /* (or a cell's step, if it wrapped) */
            wv &= moved > 0.001f;
            SeaParse(kSeaBin, sizeof kSeaBin);
        }
        g_seaW = 0;
        WaveBuild(30.0f);
        wv &= g_wvQuads == 0;
        Check(wv, "the waves: small crests on the open sea only, the same size on screen at every octave, drifting; none without sea.bin");
    }

    /* the currents (his idea: the sea moving as Earth's, the equator through Booty Bay): flow.bin read only whole; drops born
     * on open sea in view, more near the equator; carried along the current at FLOW_SPEED of the screen a second; warm at the
     * equator, cold forty degrees north; sinking sooner in the far north; gone at a coast or in the Maelstrom's eye; two streaks */
    {
        static const unsigned char allSea2[8 + 16] = { 'C', 'S', 'E', 'A', 4, 0, 4, 0, 255, 255, 255, 255, 255, 255, 255, 255,
                                                       255, 255, 255, 255, 255, 255, 255, 255 };
        FlowBinMake(0.6f, 100, 0);                                       /* eastward everywhere */
        int fl = FlowParse(kFlowBin, sizeof kFlowBin) && fabsf(g_flowUsual - 1.0f) < 1e-4f;
        fl &= !FlowParse(kFlowBin, sizeof kFlowBin - 1) && g_flowW == 0;
        {
            static unsigned char longer[sizeof kFlowBin + 1];
            memcpy(longer, kFlowBin, sizeof kFlowBin);
            fl &= !FlowParse(longer, sizeof longer);                     /* a byte too many */
        }
        fl &= FlowParse(kFlowBin, sizeof kFlowBin);
        float cx, cv;
        FlowAt(0.37f, 0.81f, &cx, &cv);
        fl &= fabsf(cx - 1.0f) < 1e-5f && cv == 0;
        SeaParse(allSea2, sizeof allSea2);
        g_scrW = g_surfW = 3840; g_scrH = g_surfH = 2160;
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        g_mael.ok = 0;
        memset(g_drop, 0, sizeof g_drop);
        g_dropSeed = 7;
        FlowBuild(0, 0.02f);
        int live = 0, nearEq = 0;
        for (int i = 0; i < FLOW_DROPS; i++) if (g_drop[i].live) { live++; if (fabsf(FlowLat(g_drop[i].v)) < 12) nearEq++; }
        fl &= live > FLOW_DROPS * 9 / 10 && (float)nearEq / live > 0.26f && (float)nearEq / live < 0.38f;   /* evenly: the band is 0.32 of it */
        int pick = -1;
        for (int i = 0; i < FLOW_DROPS && pick < 0; i++) if (g_drop[i].live && g_drop[i].u > 0.2f && g_drop[i].u < 0.7f) pick = i;
        float u0 = g_drop[pick].u;
        for (int f = 0; f < 50; f++) FlowBuild(0, 0.02f);
        fl &= g_drop[pick].live && fabsf((g_drop[pick].u - u0) - FLOW_SPEED / 1.778f) < 0.001f && g_drop[pick].age > 0.99f;
        /* half the world in view: the same speed on screen, so half the speed on the map */
        g_bgUV[0] = g_bgUV[4] = 0.25f; g_bgUV[2] = g_bgUV[6] = 0.75f;
        g_bgUV[5] = g_bgUV[7] = 0.25f; g_bgUV[1] = g_bgUV[3] = 0.75f;
        g_drop[pick].u = 0.4f; g_drop[pick].v = 0.5f; g_drop[pick].age = 1; g_drop[pick].life = 100; g_drop[pick].live = 1;
        g_drop[pick].n = 0; g_drop[pick].since = 0; g_drop[pick].dying = 0;
        for (int f = 0; f < 50; f++) FlowBuild(0, 0.02f);
        fl &= fabsf((g_drop[pick].u - 0.4f) - FLOW_SPEED * 0.5f / 1.778f) < 0.001f;
        /* its trail: a point every FLOW_TRAIL_S, newest first, behind it (west of it, the current running east) */
        fl &= abs(g_drop[pick].n - (int)(1.0f / FLOW_TRAIL_S + 0.5f)) <= 1 && g_drop[pick].tu[0] < g_drop[pick].u &&
              g_drop[pick].tu[1] < g_drop[pick].tu[0];
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        /* its trail drawn along its way (eastward: the head right of the tail) */
        int drew = g_flQuads > 0, eastward = g_flPos[3] > g_flPos[0];
        fl &= drew && eastward;
        /* born only on open sea: with the left half land, every newborn drop on the right */
        SeaParse(kSeaBin, sizeof kSeaBin);
        memset(g_drop, 0, sizeof g_drop);
        FlowBuild(0, 0.02f);
        int onLand = 0, born = 0;
        for (int i = 0; i < FLOW_DROPS; i++) if (g_drop[i].live) { born++; if (g_drop[i].u < 0.45f) onLand++; }
        fl &= born > 100 && !onLand;
        SeaParse(allSea2, sizeof allSea2);
        /* the far north: sinking sooner; a coast and the Maelstrom's eye: gone */
        FlowBinMake(0.9f, 100, 0); FlowParse(kFlowBin, sizeof kFlowBin);
        g_drop[0].u = 0.5f; g_drop[0].v = 0.11f; g_drop[0].age = 0; g_drop[0].life = 100; g_drop[0].live = 1; g_drop[0].dying = 0;
        for (int f = 0; f < 50; f++) FlowBuild(0, 0.02f);
        fl &= g_drop[0].age > 2.9f;                                      /* sixty north: three times as fast */
        SeaParse(kSeaBin, sizeof kSeaBin);
        g_drop[1].u = 0.1f; g_drop[1].v = 0.5f; g_drop[1].age = 1; g_drop[1].life = 100; g_drop[1].live = 1; g_drop[1].dying = 0;
        g_mael.u = 0.8f; g_mael.v = 0.5f; g_mael.r = 0.1f; g_mael.ok = 1;
        g_drop[2].u = 0.8f; g_drop[2].v = 0.5f; g_drop[2].age = 1; g_drop[2].life = 100; g_drop[2].live = 1; g_drop[2].dying = 0;
        g_dropSeed = 99;
        FlowBuild(0, 0.02f);
        /* on land and in the eye: stopped, fading within 0.6 s (or already reborn elsewhere) */
        fl &= (g_drop[1].dying && g_drop[1].u < 0.101f && g_drop[1].life <= g_drop[1].age + 0.6f + 1e-4f) &&
              (g_drop[2].dying && g_drop[2].life <= g_drop[2].age + 0.6f + 1e-4f);
        for (int f = 0; f < 40; f++) FlowBuild(0, 0.02f);
        fl &= (!g_drop[1].live || !g_drop[1].dying || g_drop[1].u > 0.3f) && (!g_drop[2].live || g_drop[2].age < 0.8f);
        g_mael.ok = 0; g_flowW = 0;
        Check(fl, "the currents: flow.bin whole; drops born on open sea, more near the equator; carried at FLOW_SPEED, a trail behind; sinking in the north; stopped and faded at coasts and in the Maelstrom's eye; drawn along their way");
    }
    /* every drop moves at least FLOW_SLOWEST of the usual pace on screen (a still gyre's middle shows), at most FLOW_FASTEST */
    {
        static const unsigned char allSea3[8 + 16] = { 'C', 'S', 'E', 'A', 4, 0, 4, 0, 255, 255, 255, 255, 255, 255, 255, 255,
                                                       255, 255, 255, 255, 255, 255, 255, 255 };
        SeaParse(allSea3, sizeof allSea3);
        FlowBinMake(0.6f, 100, 0); FlowParse(kFlowBin, sizeof kFlowBin);       /* the usual: 1.0 */
        FlowBinMake(0.6f, 10, 0);                                                /* a tenth of it everywhere... */
        memcpy(g_flowCell, kFlowBin + 16, 32);                                   /* ...keeping the usual as it was */
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        g_scrW = g_surfW = 3840; g_scrH = g_surfH = 2160;
        memset(g_drop, 0, sizeof g_drop);
        g_drop[0].u = 0.3f; g_drop[0].v = 0.5f; g_drop[0].age = 1; g_drop[0].life = 100; g_drop[0].live = 1;
        for (int f = 0; f < 50; f++) FlowBuild(0, 0.02f);
        int sl = fabsf((g_drop[0].u - 0.3f) - FLOW_SPEED * FLOW_SLOWEST / 1.778f) < 0.001f;
        for (int i = 0; i < 32; i += 2) g_flowCell[i] = 127;                     /* 1.27 times the usual everywhere: as it is */
        g_drop[0].u = 0.3f;
        for (int f = 0; f < 50; f++) FlowBuild(0, 0.02f);
        sl &= fabsf((g_drop[0].u - 0.3f) - FLOW_SPEED * 1.27f / 1.778f) < 0.001f;
        g_flowW = 0;
        Check(sl, "a slow current's drops still move at FLOW_SLOWEST of the usual pace on screen; a usual one's at its own");
    }
    /* his blues by speed (4 Oct): the cuts from the field's own speeds, each shade as much of the moving sea as the next;
     * a trail's stretches each in the shade the water ran there, bright and wide at its head, faint and thin at its tail;
     * a crowd thinned (a cell over FLOW_CROWD times its share ages three times as fast) */
    {
        static const unsigned char allSea4[8 + 16] = { 'C', 'S', 'E', 'A', 4, 0, 4, 0, 255, 255, 255, 255, 255, 255, 255, 255,
                                                       255, 255, 255, 255, 255, 255, 255, 255 };
        SeaParse(allSea4, sizeof allSea4);
        FlowBinMake(0.6f, 100, 0);
        for (int c = 0; c < 16; c++) { kFlowBin[16 + 2 * c] = (unsigned char)(signed char)(8 * (c + 1)); kFlowBin[17 + 2 * c] = 0; }
        int sh = FlowParse(kFlowBin, sizeof kFlowBin);
        for (int c = 1; c < FLOW_SHADES - 1; c++) sh &= g_flowCut[c] > g_flowCut[c - 1];
        float slowest = 8 * 0.01f / g_flowUsual, fastest = 128 * 0.01f / g_flowUsual;
        sh &= FlowShade(slowest) == 0 && FlowShade(fastest) == FLOW_SHADES - 1;
        int bins[FLOW_SHADES] = { 0 };
        for (int c = 0; c < 16; c++) bins[FlowShade(8 * (c + 1) * 0.01f / g_flowUsual)]++;
        for (int b = 0; b < FLOW_SHADES; b++) sh &= bins[b] >= 2 && bins[b] <= 4;              /* 16 cells, about even */
        for (int x = 1; x < 100; x++) sh &= FlowShade(x * 0.03f) >= FlowShade((x - 1) * 0.03f);   /* never darker when faster */
        Check(sh, "his blues by speed: cuts from the field, each shade about as much of the sea, faster never darker");
        /* a drop come from slow water into fast: its head's stretch in the fast shade, the stretches behind in the slow one */
        {
            memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
            g_scrW = g_surfW = 3840; g_scrH = g_surfH = 2160; g_mael.ok = 0;
            memset(g_drop, 0, sizeof g_drop);
            for (int i = 1; i < FLOW_DROPS; i++) g_drop[i].live = 0;
            g_drop[0].u = 0.875f; g_drop[0].v = 0.875f; g_drop[0].age = 1; g_drop[0].life = 100; g_drop[0].live = 1;
            g_drop[0].n = 3;
            for (int j = 0; j < 3; j++) { g_drop[0].tu[j] = 0.85f - 0.02f * j; g_drop[0].tv[j] = 0.875f; g_drop[0].ts[j] = 0; }
            g_dropSeed = 3;
            FlowBuild(0, 0.001f);
            float head = g_flUV[0 * 2 + 1], behind = g_flUV[6 * 2 + 1];   /* quad 0's first corner; quad 1's (after the join) */
            int rows = g_drop[0].shade == FLOW_SHADES - 1 && fabsf(head - (5 * 8 + 7.5f) / 64) < 1e-4f && fabsf(behind - 7.5f / 64) < 1e-4f;
            Check(rows, "a trail keeps the shade of the water it passed: its head fast, the stretches behind slow");
        }

        FlowBinMake(0.6f, 100, 0); FlowParse(kFlowBin, sizeof kFlowBin);
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        g_scrW = g_surfW = 3840; g_scrH = g_surfH = 2160; g_mael.ok = 0;
        memset(g_drop, 0, sizeof g_drop);
        for (int i = 1; i < FLOW_DROPS; i++) { g_drop[i].u = 0.95f; g_drop[i].v = 0.95f; g_drop[i].live = 1; g_drop[i].dying = 1;
                                               g_drop[i].age = 0; g_drop[i].life = 1000; }    /* parked out of the way */
        g_drop[0].u = 0.3f; g_drop[0].v = 0.5f; g_drop[0].age = 1; g_drop[0].life = 100; g_drop[0].live = 1;
        for (int f = 0; f < 150; f++) FlowBuild(0, 0.02f);                       /* three seconds: a full trail */
        int tr = g_drop[0].n == FLOW_TRAIL;
        /* drop 0's quads are the first ones: as many as its trail's points, the head's corners the brightest */
        int q0 = 0;
        for (int i = 0; i < 1; i++) q0 = g_drop[0].n;
        unsigned headA = g_flCol[1] >> 24, tailA = g_flCol[(q0 - 1) * 6 - 2 + 0] >> 24;
        tr &= headA > 150 && tailA < headA / 3;
        /* the head's half width wider than the tail's */
        float headW = fabsf(g_flPos[1 * 3 + 1] - g_flPos[3 * 3 + 1]), tailW = fabsf(g_flPos[((q0 - 1) * 6 - 2) * 3 + 1] - g_flPos[((q0 - 1) * 6) * 3 + 1]);
        tr &= headW > tailW * 1.5f;
        Check(tr, "a trail: a full one after its time, its head bright and wide, its tail faint and thin");

        /* a crowd: 900 drops in one cell age three times as fast as one alone */
        memset(g_drop, 0, sizeof g_drop);
        for (int i = 0; i < FLOW_DROPS; i++) { g_drop[i].u = i < 900 ? 0.31f : 0.05f + 0.9f * (i - 900) / 100.0f; g_drop[i].v = i < 900 ? 0.51f : 0.2f;
                                               g_drop[i].age = 1; g_drop[i].life = 1000; g_drop[i].live = 1; }
        FlowBuild(0, 0.1f);
        int cr = fabsf(g_drop[0].age - 1.3f) < 1e-3f && fabsf(g_drop[950].age - 1.1f) < 1e-3f;
        g_flowW = 0;
        Check(cr, "a crowd thins: drops in a cell over FLOW_CROWD times its share age three times as fast, the rest as ever");
    }
    /* his ask, 4 Oct: the first load after a cold boot is long, the rest a fraction of it: the session's first map screen
     * zooms from the whole world, every later one from halfway in */
    {
        g_mapShown = 1;
        { const char *mt = "map 0 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\n"; MapParse(mt, (unsigned)strlen(mt)); }
        Poke(BAR_MAP_VA, 0);
        Poke(MAP_DEST_MAP_VA, 0); PutFloat(MAP_DEST_X_VA, -5000.0f); PutFloat(MAP_DEST_Y_VA, 10000.0f);
        g_classSure = 0;
        g_zoomScreens = 0; g_driftNew = 1; g_drift = 0; g_driftAt = 0;
        DWORD at = 1300000;
        for (int i = 0; i < 60; i++) { at += 20; DriftStep(0.0f, at); }   /* the first: the arrival known (from half the bar... */
        for (int i = 0; i < 30; i++) { at += 20; DriftStep(0.6f, at); }   /* ...so it lands now) and the view starts whole */
        float hFirst = g_viewH;
        g_driftNew = 1;
        for (int i = 0; i < 60; i++) { at += 20; DriftStep(0.0f, at); }
        for (int i = 0; i < 30; i++) { at += 20; DriftStep(0.6f, at); }
        float hEnd = 0.5f * 3000 * 2e-5f, half = 0.5f * (float)sqrt(hEnd / 0.5f);
        int hw = g_zoomHalf && g_viewH < hFirst && g_viewH <= half * 1.02f;
        g_mapShown = 0; g_zoomScreens = 0; g_zoomHalf = 0;
        Check(hw, "the session's first map screen zooms from the whole world; a later one from halfway in");

        /* his rule, 4 Oct (later loads take 3 to 5 s): a later screen whose arrival the game wrote before it began
         * starts at once halfway in, its middle halfway from the world's middle to the arrival, no glide from the whole
         * world; a login's stale place (as the last screen left it) never counts */
        g_mapShown = 1;
        Poke(MAP_DEST_MAP_VA, 0); PutFloat(MAP_DEST_X_VA, -5000.0f); PutFloat(MAP_DEST_Y_VA, 10000.0f);
        DriftScreenGone();                                              /* the last screen left the arrival here */
        PutFloat(MAP_DEST_Y_VA, 15000.0f);                              /* NEW_WORLD first: u = 0.5 - 2e-5 * 15000 = 0.2 */
        g_zoomScreens = 1; at += 3000; DriftStep(0.0f, at);
        float h0 = 0.5f * powf(hEnd / 0.5f, LATER_START);
        int lt = g_zoomHalf && g_destOk && g_destKnownWay == 5 && g_destKnownPct == 0 && fabsf(g_viewH - h0) < 1e-4f &&
                 fabsf(g_viewU - (0.5f + (0.2f - 0.5f) * LATER_MIDDLE)) < 1e-4f &&
                 fabsf(g_viewV - (0.5f + (0.6f - 0.5f) * LATER_MIDDLE)) < 1e-4f;
        for (int i = 0; i < 50; i++) { at += 20; DriftStep(1.0f, at); } /* a second at a full bar: the pace, 1.5 times */
        lt &= fabsf(g_drift - 1.0f / g_map.pace) < 0.01f;              /* (untimed here: the pace; timed, the plan) */
        for (int i = 0; i < 400; i++) { at += 20; DriftStep(1.0f, at); }
        lt &= fabsf(g_viewH - hEnd) < 2e-3f && fabsf(g_viewU - 0.2f) < 2e-3f;   /* and it still lands on the arrival */
        DriftScreenGone();                                              /* this one ends where it arrived */
        g_zoomScreens = 1; at += 3000; DriftStep(0.0f, at);             /* the next, the game's arrival unchanged */
        lt &= !g_destOk && g_viewH > 0.49f && g_destKnownPct == -1;
        g_mapShown = 0; g_zoomScreens = 0; g_zoomHalf = 0; g_destEndOk = 0;
        Check(lt, "a later screen: the arrival written before it counts at once; it starts halfway in, its middle halfway there, no glide, and lands; an unchanged arrival does not count");

        /* his rule, 4 Oct: the five-second floor, on every machine. A screen plans its end from what this machine has
         * learned for its kind, never under the floor less the game's own end freeze; the bar and the zoom run on that
         * plan and never ahead of the game */
        g_mapShown = 1; g_holdMs = 5000;
        g_lnMs[1] = 950; g_lnFreeze[1] = 800; g_lnN[1] = 3;            /* a quick machine's later loads */
        PutFloat(MAP_DEST_Y_VA, 15000.0f); DriftScreenGone();
        PutFloat(MAP_DEST_Y_VA, 10000.0f);                              /* NEW_WORLD wrote the next (u 0.3) */
        g_zoomScreens = 1; at += 3000;
        DWORD t0 = at;
        float sh = DisplayStep(0.0f, at); DriftStep(sh, at);
        int pl = g_timed && g_screenKind == 1 && g_planEndAt == t0 + 4200 && fabsf(g_zoomStart - LATER_START) < 1e-6f &&
                 fabsf(g_viewH - 0.5f * powf(hEnd / 0.5f, LATER_START)) < 1e-4f;
        for (int k = 0; k < 25; k++) { at += 20; sh = DisplayStep(1.0f, at); DriftStep(sh, at); }   /* the game done at 0.5 s */
        pl &= sh > 0.10f && sh < 0.14f;                                 /* the bar on the plan (0.5 / 4.2), not the game's */
        g_lnMs[1] = 10000; g_lnN[1] = 3;                                /* a slow machine: its own ten seconds */
        DriftScreenGone(); g_zoomScreens = 1; at += 3000; t0 = at;
        sh = DisplayStep(0.0f, at); DriftStep(sh, at);
        pl &= g_planEndAt == t0 + 10000;
        for (int k = 0; k < 100; k++) { at += 20; sh = DisplayStep(0.1f, at); DriftStep(sh, at); }   /* the game slower */
        pl &= sh <= 0.1f + DISP_LEAD + 1e-4f && g_planU > 0.19f;        /* the game's (and its lead), not the plan's */
        g_lnN[0] = 0; g_zoomScreens = 0;                                /* the session's first, nothing learned */
        DriftScreenGone(); at += 3000; t0 = at;
        sh = DisplayStep(0.0f, at); DriftStep(sh, at);
        pl &= g_screenKind == 0 && g_planEndAt == t0 + FIRST_GUESS_MS && g_zoomStart == 0;
        DriftScreenGone(); at += 3000; sh = DisplayStep(0.5f, at);     /* a screen's first frame: the plan's 0, not the game's */
        pl &= sh == 0.0f;
        Check(pl, "the plan: a quick machine's later load ends on the floor less the end freeze (4.2 s), the bar on it; a slow one on its own 10 s, never ahead of the game; a first one on its guess, from the whole world");

        /* the hold: the game done first, HoldFrame draws its frame until the plan is met, then the view has landed */
        g_lnMs[1] = 950; g_lnFreeze[1] = 800; g_lnN[1] = 3;
        PutFloat(MAP_DEST_Y_VA, 15000.0f); DriftScreenGone(); PutFloat(MAP_DEST_Y_VA, 10000.0f);
        g_zoomScreens = 1; at += 3000; t0 = at; shim_ticks = at;
        sh = DisplayStep(0.0f, at); DriftStep(sh, at);
        for (int k = 0; k < 25; k++) { at += 20; shim_ticks = at; sh = DisplayStep(0.6f, at); DriftStep(sh, at); }
        PutFloat(HOLD_BAR_VA, 1.0f);
        g_holdFrame = TestHoldFrame; g_holdOn = 1; g_testFrames = 0;
        HoldFrame();                                                    /* the setter's call, the game's bar full */
        at = shim_ticks;
        int hd = g_testFrames > 200 && at >= t0 + 4200 + HOLD_TAIL_MS && at < t0 + 4200 + HOLD_TAIL_MS + 40 &&
                 g_holdLast > 3500 && g_planU > 0.999f && fabsf(g_viewH - hEnd) < 2e-3f && fabsf(g_viewU - 0.3f) < 2e-3f;
        int n1 = g_testFrames;
        HoldFrame();                                                    /* once a screen */
        hd &= g_testFrames == n1 + 1;
        g_holdMs = 20000;                                               /* never past HOLD_MAX_MS */
        DriftScreenGone(); g_zoomScreens = 1; at += 3000; shim_ticks = at; t0 = at;
        sh = DisplayStep(0.0f, at); DriftStep(sh, at);
        g_testFrames = 0; HoldFrame();
        hd &= shim_ticks - t0 <= HOLD_MAX_MS + 40 && shim_ticks - t0 >= HOLD_MAX_MS;
        g_testFrames = 0; HoldFrame();                                  /* cut short by the cap: still once a screen */
        hd &= g_testFrames == 1;
        g_holdMs = 5000; g_lnMs[1] = 8000;                              /* the game done after the floor: settle quickly */
        DriftScreenGone(); g_zoomScreens = 1; at += 3000; shim_ticks = at; t0 = at;
        sh = DisplayStep(0.0f, at); DriftStep(sh, at);
        for (int k = 0; k < 300; k++) { at += 20; shim_ticks = at; sh = DisplayStep(0.9f, at); DriftStep(sh, at); }
        g_testFrames = 0; HoldFrame();
        hd &= shim_ticks - at <= HOLD_SETTLE_MS + HOLD_TAIL_MS + 40 && g_planU > 0.999f;
        /* his clip, 4 Oct 22:04: a first load, the game near a tenth long past its plan, then done in a rush with the
         * view still far out. It glides in over the settle, lands on the arrival as the blur comes on, and a frame
         * after the game's freeze shows that same view (it used to blur where it was, then jump in mid-freeze) */
        g_lnMs[0] = 2000; g_lnFreeze[0] = 800; g_lnN[0] = 3; g_blurOn = 1; g_blurMs = 300;
        DriftScreenGone(); g_zoomScreens = 0; at += 3000; shim_ticks = at;
        sh = DisplayStep(0.0f, at); DriftStep(sh, at);
        for (int k = 0; k < 250; k++) { at += 20; shim_ticks = at; sh = DisplayStep(0.1f, at); DriftStep(sh, at); }
        float hOut = g_viewH;
        DWORD land = shim_ticks + 16 + HOLD_SETTLE_MS;                  /* the hold draws one frame (16 ms here) first */
        g_testFrames = 0; HoldFrame();
        float vh = g_viewH, vu = g_viewU, vv = g_viewV;
        int st = hOut > 4 * hEnd && g_blurFrom == land - BLUR_LEAD_MS && g_blurTo == land + 300 && g_testFrames > 40 &&
                 fabsf(vh - hEnd / BLUR_PUSH) < 0.03f * hEnd && fabsf(vu - 0.3f) < 2e-3f;
        at = shim_ticks + 2400; shim_ticks = at;                        /* the game's freeze, then one more frame */
        sh = DisplayStep(1.0f, at); DriftStep(sh, at);
        st &= fabsf(g_viewH - vh) < 1e-5f && fabsf(g_viewU - vu) < 1e-5f && fabsf(g_viewV - vv) < 1e-5f;
        g_lnN[0] = 0; g_blurOn = 0; g_blurFrom = g_blurTo = 0;
        Check(st, "the game done long after its plan, the view far out: it glides in over the settle and lands as the blur comes on, and the frame after the freeze shows that same view");
        g_lnMs[1] = 950; g_holdOn = 0; g_testFrames = 0;                /* off: the frame alone */
        DriftScreenGone(); g_zoomScreens = 1; at += 3000; shim_ticks = at;
        sh = DisplayStep(0.0f, at); DriftStep(sh, at);
        HoldFrame();
        hd &= g_testFrames == 1;
        PutFloat(HOLD_BAR_VA, 0.0f);
        /* its setup: the three calls sent to HoldFrame only when the frame and every call are as read; else none */
        memcpy(shim_module + HOLD_FRAME_VA - 0x400000u, kHoldFrame, sizeof kHoldFrame);
        for (int c = 0; c < 3; c++) {
            unsigned char *cp = shim_module + kHoldCall[c] - 0x400000u;
            unsigned rel = (unsigned)(HOLD_FRAME_VA - (kHoldCall[c] + 5));
            cp[0] = 0xE8; memcpy(cp + 1, &rel, 4);
        }
        int hs = 1;
        g_map.ok = 1; g_holdMs = 5000; g_holdOn = 0;
        shim_module[kHoldCall[1] - 0x400000u + 2] ^= 0x10;               /* one call not as read: none changed */
        HoldSetup();
        hs &= !g_holdOn && LogSays("hold off: a loading phase's call");
        shim_module[kHoldCall[1] - 0x400000u + 2] ^= 0x10;
        HoldSetup();
        for (int c = 0; c < 3; c++) {
            unsigned rel; memcpy(&rel, shim_module + kHoldCall[c] - 0x400000u + 1, 4);
            hs &= rel == (unsigned)((ULONG_PTR)HoldFrame - (ULONG_PTR)(shim_module + kHoldCall[c] - 0x400000u + 5));
        }
        hs &= g_holdOn && LogSays("hold on:") && g_holdFrame == (void (*)(void))(shim_module + HOLD_FRAME_VA - 0x400000u);
        HoldSetup();                                                    /* again in the same process: ours already, kept */
        hs &= g_holdOn;
        shim_module[HOLD_FRAME_VA - 0x400000u + 1] ^= 1;                /* another client's frame: off */
        HoldSetup();
        hs &= !g_holdOn && LogSays("hold off: the game's loading frame");
        memset(shim_module + HOLD_FRAME_VA - 0x400000u, 0, sizeof kHoldFrame);
        for (int c = 0; c < 3; c++) memset(shim_module + kHoldCall[c] - 0x400000u, 0, 5);
        g_holdOn = 0; g_map.ok = 0; g_holdFrame = NULL;
        hd &= hs;
        /* the character list's music: faded out with the pack's fade as a screen begins, only while it plays */
        memcpy(shim_module + GLUE_STOP_VA - 0x400000u, kGlueStop, sizeof kGlueStop);
        g_musicFadeMs = 250; MusicSetup();
        int mu = g_musicOn && LogSays("music: the character list's music fades out over 250 ms");
        g_glueStop = TestGlueStop; g_testFade = -1;
        Poke(GLUE_MUSIC_VA, 0x1234);                                    /* the glue music still plays */
        DriftScreenGone(); at += 3000; DisplayStep(0.0f, at);
        mu &= fabsf(g_testFade - 0.25f) < 1e-6f && g_musicFaded;
        g_testFade = -1; Poke(GLUE_MUSIC_VA, 0);                        /* in the world: none plays, nothing called */
        DriftScreenGone(); at += 3000; DisplayStep(0.0f, at);
        mu &= g_testFade < 0 && !g_musicFaded;
        shim_module[GLUE_STOP_VA - 0x400000u + 5] ^= 1;                 /* another client: never set up */
        MusicSetup();
        mu &= !g_musicOn && LogSays("music: the glue music's stop is not where it was read");
        memset(shim_module + GLUE_STOP_VA - 0x400000u, 0, sizeof kGlueStop);
        g_glueStop = NULL;
        hd &= mu;

        /* the ending: the blur after the view lands, inside the floor; held for it even when the game is done late;
         * then the world's dissolve; its hook set only on the bytes as read */
        int bl = 1;
        g_blurOn = 1; g_blurMs = BLUR_MS; g_blurDim = 35; g_holdOn = 1; g_holdFrame = TestHoldFrame; g_map.ok = 1; g_mapShown = 1;
        g_lnMs[1] = 950; g_lnFreeze[1] = 800; g_lnN[1] = 3; g_holdMs = 5000;
        DriftScreenGone(); g_zoomScreens = 1; at += 3000; shim_ticks = at; t0 = at;
        sh = DisplayStep(0.0f, at); DriftStep(sh, at);
        PutFloat(HOLD_BAR_VA, 1.0f);
        HoldFrame();
        bl &= g_blurTo - g_blurFrom == 500 && g_blurTo == t0 + 4200 && shim_ticks >= t0 + 4200 && shim_ticks < t0 + 4240;   /* the last half second */
        bl &= g_viewH < hEnd * 0.6f && g_viewH > hEnd / BLUR_PUSH * 0.95f && fabsf(g_viewU - 0.3f) < 2e-3f;   /* closer still, on the arrival */
        float am = 0;
        bl &= BlurStep(1, t0 + 3650, &am) == 0 && BlurStep(1, t0 + 3950, &am) == 1 && am > 0.4f && am < 0.6f &&
              BlurStep(1, t0 + 4300, &am) == 1 && am > 0.999f && g_blurUsed;
        bl &= BlurStep(0, t0 + 6000, &am) == 2 && am > 0.999f && g_dissolveAt == t0 + 6000;   /* the world, after the freeze */
        bl &= BlurStep(0, t0 + 6250, &am) == 2 && am > 0.4f && am < 0.6f;
        bl &= BlurStep(0, t0 + 6500, &am) == 3 && !g_blurUsed && BlurStep(0, t0 + 6600, &am) == 0;
        g_lnMs[1] = 8000;                                               /* the game done after its plan: held for the blur */
        DriftScreenGone(); g_zoomScreens = 1; at = shim_ticks + 3000; shim_ticks = at; t0 = at;
        sh = DisplayStep(0.0f, at); DriftStep(sh, at);
        for (int k = 0; k < 450; k++) { at += 20; shim_ticks = at; sh = DisplayStep(1.0f, at); DriftStep(sh, at); }
        g_testFrames = 0; HoldFrame();
        bl &= g_testFrames > 10 && g_blurTo - g_blurFrom == BLUR_MS && shim_ticks >= g_blurTo;   /* (no lead once late) */
        { const char *mt = "map 0 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\nblur 400 100\n"; MapParse(mt, (unsigned)strlen(mt)); }
        bl &= g_blurMs == 400 && g_blurDim == 100 && g_map.ok;
        { const char *mt = "map 0 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\n"; MapParse(mt, (unsigned)strlen(mt)); }
        bl &= g_blurMs == BLUR_MS && g_blurDim == BLUR_DIM;
        memcpy(shim_module + PRESENT_VA - 0x400000u, kPresent, sizeof kPresent);
        g_holdOn = 1; PresentSetup();
        unsigned char *pa = shim_module + PRESENT_VA - 0x400000u;
        unsigned prel; memcpy(&prel, pa + 1, 4);
        bl &= g_blurOn && pa[0] == 0xE9 && pa[5] == 0x90 && prel == (unsigned)((ULONG_PTR)PresentStub - (ULONG_PTR)(pa + 5)) &&
              g_presentBack == pa + 6 && LogSays("blur on:");
        PresentSetup();                                                 /* again in the same process: ours, kept */
        bl &= g_blurOn;
        memcpy(pa, kPresent, sizeof kPresent); g_gameGL = 1;            /* the game on OpenGL: never set, and the view */
        PresentSetup();                                                 /* fades as each screen goes */
        bl &= !g_blurOn && pa[0] == kPresent[0] && LogSays("blur off: the game draws with OpenGL");
        g_gameGL = 0;
        memcpy(pa, kPresent, sizeof kPresent); pa[8] ^= 1;              /* another client: never set */
        PresentSetup();
        bl &= !g_blurOn && LogSays("blur off: the game's present is not where it was read");
        memset(pa, 0, sizeof kPresent);
        g_blurOn = 0; g_holdOn = 0; g_holdFrame = NULL; g_blurFrom = g_blurTo = 0; g_blurUsed = 0; g_map.ok = 0; g_mapShown = 0;
        PutFloat(HOLD_BAR_VA, 0.0f);
        for (int k = 0; k < 2; k++) g_lnMs[k] = g_lnFreeze[k] = g_lnN[k] = 0;
        Check(bl, "the ending: blurred and darkened over its last half second, closing in, inside the floor; held for it when the game is late; the world dissolves out of it over DISSOLVE_MS; map.txt sets it; hooked only on the bytes as read");
        Check(hd, "the hold: the game done early, its frame drawn on until the plan (4.2 s) and the view landed; once a screen; never past HOLD_MAX_MS; after the floor it settles in HOLD_SETTLE_MS; off, the frame alone; set up only on the bytes as read; the character list's music faded with the pack's fade only while it plays");

        /* the lessons: each kind's time to a full bar and its end freeze, kept in loadtimes.txt and read back, the newest
         * counting half; never from a later login or a time no load takes */
        g_dispNew = 1; DisplayStep(0.0f, 900000); DisplayStep(0.5f, 900400); DisplayStep(1.0f, 901000);
        DisplayStep(1.0f, 902500);                                       /* the game's own freeze: 1.5 s */
        int ln = g_fullMs == 1000 && g_endFreezeMs == 1500;
        char lm[256];
        for (int k = 0; k < 2; k++) g_lnMs[k] = g_lnFreeze[k] = g_lnN[k] = 0;
        g_screenKind = 1; g_screenLogin = 0; g_screenMap = 1;
        LearnScreen(lm);
        ln &= g_lnMs[1] == 1000 && g_lnFreeze[1] == 1500 && g_lnN[1] == 1 && g_fullMs == 0;
        g_fullMs = 2000; g_endFreezeMs = 500; LearnScreen(lm);
        ln &= g_lnMs[1] == 1500 && g_lnFreeze[1] == 1000 && g_lnN[1] == 2 &&
              LogSays("learned: this machine's later loads fill the bar in 1.50 s and then freeze 1.00 s");
        g_fullMs = 6400; g_endFreezeMs = 2600; g_screenKind = 0; LearnScreen(lm);   /* a cold boot's first */
        ln &= g_lnMs[0] == 6400 && g_lnFreeze[0] == 2600 && g_lnN[0] == 1;
        for (int k = 0; k < 2; k++) g_lnMs[k] = g_lnFreeze[k] = g_lnN[k] = 0;
        LearnLoad();
        ln &= g_lnMs[1] == 1500 && g_lnN[1] == 2 && g_lnMs[0] == 6400 && g_lnFreeze[0] == 2600;   /* read back */
        g_fullMs = 9000; g_screenKind = 1; g_screenLogin = 1; LearnScreen(lm);       /* a later login teaches nothing */
        ln &= g_lnMs[1] == 1500;
        g_fullMs = 50; g_screenLogin = 0; LearnScreen(lm);                           /* nor a time no load takes */
        ln &= g_lnMs[1] == 1500;
        g_screenMap = 0; g_mapShown = 0; g_zoomScreens = 0; g_zoomHalf = 0; g_destEndOk = 0;
        for (int k = 0; k < 2; k++) g_lnMs[k] = g_lnFreeze[k] = g_lnN[k] = 0;
        Check(ln, "the lessons: each kind's time to a full bar and its end freeze, kept in loadtimes.txt and read back, the newest counting half; never from a later login or a time no load takes");
    }
    /* the overlay with the waves and the currents in the pack */
    {
        Fresh("overlay-waves");
        PackLive(1 | 2 | 4 | 8 | 512 | 1024, 0);
        Start();
        Check(g_wavesOn && g_flowOn && LogSays("overlay on: waves, currents and glints on the open sea"),
              "the overlay: the waves and the currents on with their files, in the log");
    }

    /* the glints: only where the sea is open, never more than there are, held where they appeared, a steady size, none close in */
    {
        SeaParse(kSeaBin, sizeof kSeaBin);
        g_scrW = g_surfW = 3840; g_scrH = g_surfH = 2160;                                    /* its own screen, whatever ran before */
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        memset(g_glint, 0, sizeof g_glint);
        int gl = 1, most = 0, seen = 0, held = 1, crest = 0;
        float minX = 1, maxW = 0;
        for (int f = 0; f < 400; f++) {
            float t = 100.0f + f * 0.05f, z = 0.5f - f * 0.0002f;    /* the view closing in a little, as in a zoom */
            g_bgUV[0] = g_bgUV[4] = 0.5f - z; g_bgUV[2] = g_bgUV[6] = 0.5f + z;
            g_bgUV[5] = g_bgUV[7] = 0.5f - z; g_bgUV[1] = g_bgUV[3] = 0.5f + z;
            float wasU[GLINTS]; int wasCycle[GLINTS];
            for (int g = 0; g < GLINTS; g++) { wasU[g] = g_glint[g].u; wasCycle[g] = g_glint[g].cycle; }
            GlintsBuild(t);
            for (int g = 0; g < GLINTS; g++)
                if (f && g_glint[g].cycle == wasCycle[g] && g_glint[g].u != wasU[g]) held = 0;
            if (g_glQuads > most) most = g_glQuads;
            for (int q = 0; q < g_glQuads; q++) {
                int slot = q ? q * 6 - 2 + 2 : 0;                     /* a later quad's own first corner, after the joins */
                float x0 = g_glPos[slot * 3], x1 = g_glPos[(slot + 1) * 3];
                if ((x0 + x1) / 2 < minX) minX = (x0 + x1) / 2;
                if (x1 - x0 > maxW) maxW = x1 - x0;
                float cu = g_bgUV[0] + (x0 + x1) / 2 * (g_bgUV[2] - g_bgUV[0]);
                float cv = g_bgUV[5] + (1 - (g_glPos[slot * 3 + 1] + g_glPos[(slot + 2) * 3 + 1]) / 2) * (g_bgUV[1] - g_bgUV[5]);
                if (SwellAt(cu * 3840.0f / 2160.0f, cv, t) > 0) crest++;
                seen++;
            }
        }
        gl &= seen > 150 && crest > seen * 0.85f && most >= 3 && most <= GLINTS && minX > 0.52f && held &&
              maxW <= GLINT_PX * (2160.0f / 2160.0f) / 3840.0f + 1e-6f && maxW > GLINT_PX * 0.8f / 3840.0f;
        g_bgUV[0] = g_bgUV[4] = 0.6f; g_bgUV[2] = g_bgUV[6] = 0.8f;    /* h = 0.1: too close in */
        GlintsBuild(150.0f);
        gl &= g_glQuads == 0;
        Check(gl, "the glints: on open sea only, on the swells' crests, at most 40, each held where it appeared, a steady size on screen, none close in");
    }

    /* the log's black on the map's screens: the whole width, the log's height from the bottom, the sheet's black square */
    BarBlackBuild();
    Check(g_blackPos[0] == 0 && g_blackPos[1] == 0 && g_blackPos[3] == 1 && fabsf(g_blackPos[7] - (0.09f + 2.0f / 2160)) < 1e-6f &&
          g_blackPos[10] == fabsf(g_blackPos[7]) && g_blackUV[0] == SEA_SQUARE_U && g_blackUV[1] == SEA_DEEP_V &&
          g_blackCol[0] == 0xFFFFFFFFu && g_blackCol[3] == 0xFFFFFFFFu,
          "the log's black: the whole width, the log's height from the bottom, solid, from the sheet's black square");

    /* the names and icons: the table, the zoom bands and their fades, a steady size on screen, the strips' joins */
    {
        char tb[600];
        snprintf(tb, sizeof tb, "# t\ns 0 0 0 100 50\ns 1 100 0 200 40\ns 7 0 0 1 1\ni 0 %08x %08x 500 0 0\n"
                 "i 1 %08x %08x 320 30 0\ni 9 %08x %08x 500 0 0\ni 1 %08x %08x 100 200 0\n",
                 FBits(0.5f), FBits(0.5f), FBits(0.3f), FBits(0.6f), FBits(0.1f), FBits(0.1f), FBits(0.1f), FBits(0.1f));
        LabelsParse(tb, (unsigned)strlen(tb));
    }
    int lbl = g_lblSprites == 2 && g_lblItems == 2;                   /* an out-of-order sprite, a bad item, a band upside down */
    g_scrW = g_surfW = 3840; g_scrH = g_surfH = 2160;
    memcpy(g_bgUV, kWholeUV, sizeof kWholeUV); /* the whole world: h = 0.5 */
    LabelsBuild();
    lbl &= g_lblQuads == 1 && fabsf((g_lblPos[0] + g_lblPos[3]) / 2 - 0.5f) < 1e-5f &&
           fabsf((g_lblPos[3] - g_lblPos[0]) - 100.0f / 3840) < 1e-6f && (g_lblCol[0] >> 24) == 255;
    g_bgUV[0] = g_bgUV[4] = 0.2f; g_bgUV[2] = g_bgUV[6] = 0.4f;        /* a view 0.2 across around (0.3, 0.6): h = 0.1 */
    g_bgUV[5] = g_bgUV[7] = 0.5f; g_bgUV[1] = g_bgUV[3] = 0.7f;
    LabelsBuild();
    lbl &= g_lblQuads == 1 && fabsf((g_lblPos[0] + g_lblPos[3]) / 2 - 0.5f) < 1e-4f &&
           fabsf((g_lblPos[1] + g_lblPos[7]) / 2 - 0.5f) < 1e-4f && (g_lblCol[0] >> 24) == 255;   /* the other one is off screen */
    g_bgUV[0] = g_bgUV[4] = 0.0f; g_bgUV[2] = g_bgUV[6] = 0.6f;        /* h = 0.3: inside the top fifth of 30..320 */
    LabelsBuild();
    int fadeA = 0;
    for (int q = 0; q < g_lblQuads; q++) { int slot = q ? q * 6 - 1 : 0;   /* a later quad: after its 2 joining corners */ if ((g_lblCol[slot] >> 24) < 255) fadeA = g_lblCol[slot] >> 24; }
    lbl &= fadeA > 70 && fadeA < 110;                                   /* (320 - 300) / 58: about 0.34 */
    Check(lbl, "labels.txt read (bad lines left out); an item only in its band, fading at its edge, off screen left out, a steady size");
    /* 130 items in view: two strips, the first quad of each with 4 corners, every later one joined by 2 repeats */
    g_lblSprites = 1; g_lblItems = 130;
    for (int i = 0; i < 130; i++) { g_lblItem[i].u = 0.1f + i * 0.005f; g_lblItem[i].v = 0.5f; g_lblItem[i].spr = 0;
                                    g_lblItem[i].hmax = 500; g_lblItem[i].hmin = 0; g_lblItem[i].dy = 0; }
    memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
    LabelsBuild();
    int strips = g_lblQuads == 130 && g_lblPos[4 * 3] == g_lblPos[3 * 3] && g_lblPos[5 * 3] == g_lblPos[6 * 3] &&
                 g_lblPos[718 * 3] != g_lblPos[717 * 3] && fabsf(g_lblPos[718 * 3] - (0.1f + 120 * 0.005f - 50.0f / 3840)) < 1e-4f;
    Check(strips, "the strips: 120 quads to one, joined by repeated corners; the 121st starts the next strip afresh");

    /* his call, 4 Oct: a two-second load must not fly at you. The bar leaping to full at once: the zoom goes at the
     * pace (7 s all the way in when map.txt gives none), half way after 3.5 s; a slow bar is simply followed */
    g_mapShown = 1;                                                   /* its own setting, whatever ran before */
    { const char *mt = "map 0 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\n"; MapParse(mt, (unsigned)strlen(mt)); }
    int paced = g_map.pace == 7 && g_map.ok;
    g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; at = 140000;
    DriftStep(0, at);
    for (int i = 0; i < 175; i++) { at += 20; DriftStep(1, at); }    /* 3.5 s */
    paced &= g_drift > 0.47f && g_drift < 0.51f;
    for (int i = 0; i < 200; i++) { at += 20; DriftStep(1, at); }    /* 7.5 s in all */
    paced &= g_drift > 0.99f;
    g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; at = 160000;
    float lag = 0;
    for (int i = 0; i <= 500; i++) {                                  /* a bar filling over 10 s */
        at += 20; DriftStep(i / 500.0f, at);
        float d = i / 500.0f - g_drift; if (d > lag) lag = d;
    }
    paced &= lag < 0.03f;
    { const char *mt = "map 1 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\npace 10\n"; MapParse(mt, (unsigned)strlen(mt)); }
    paced &= g_map.ok && g_map.pace == 10;
    { const char *mt = "map 1 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\npace 0\n"; MapParse(mt, (unsigned)strlen(mt)); }
    paced &= g_map.pace == 7;
    Check(paced, "the speed limit: a bar leaping to full is zoomed at the pace (half way at 3.5 s); a slow bar is followed; pace from map.txt");

    /* the table: lines it cannot use are left out */
    { const char *mt = "map 0 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 50\n"; MapParse(mt, (unsigned)strlen(mt)); }
    int lowEnd = !g_map.ok;
    { const char *mt = "map 0 3f000000 b7a7c5ac 3f000000\nend 3000\n"; MapParse(mt, (unsigned)strlen(mt)); }
    int short4 = !g_map.ok;
    { const char *mt = "map 1 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\n"; MapParse(mt, (unsigned)strlen(mt)); }
    int one = g_map.ok && g_map.m[1].ok && !g_map.m[0].ok;
    Check(lowEnd && short4 && one, "map.txt: an end under 100 yards or a line short of its four numbers is not used; a good line is");
}

/* ---- the view (option 1, slice 1, 4 Oct 2026): its picture, its frame, and whose thread moves the bar and the zoom ---- */
/* A BLP2 of w x h DXT blocks, levels of them (each filled with its level's number + 1), mips its flag for smaller ones */
static unsigned char *BlpMake(unsigned w, unsigned h, int dxt, int levels, int mips, unsigned *len)
{
    unsigned block = dxt == 1 ? 8 : 16, total = 1172, sizes[16], lw = w, lh = h;
    for (int i = 0; i < levels; i++) {
        sizes[i] = ((lw + 3) / 4) * ((lh + 3) / 4) * block; total += sizes[i];
        lw = lw > 1 ? lw / 2 : 1; lh = lh > 1 ? lh / 2 : 1;
    }
    unsigned char *d = calloc(1, total);
    memcpy(d, "BLP2", 4); d[4] = 1; d[8] = 2; d[9] = dxt == 5 ? 8 : 0; d[10] = dxt == 5 ? 7 : 0; d[11] = (unsigned char)mips;
    memcpy(d + 12, &w, 4); memcpy(d + 16, &h, 4);
    unsigned at = 1172;
    for (int i = 0; i < levels; i++) {
        memcpy(d + 20 + 4 * i, &at, 4); memcpy(d + 84 + 4 * i, &sizes[i], 4);
        memset(d + at, i + 1, sizes[i]); at += sizes[i];
    }
    *len = total;
    return d;
}
static int Px(const unsigned char *o, int i, int r, int g, int b, int a)
{
    return o[4 * i] == r && o[4 * i + 1] == g && o[4 * i + 2] == b && o[4 * i + 3] == a;
}
static void ViewTests(void)
{
    /* the picture's file: every level found and checked against the file's length, from the whole picture down */
    {
        unsigned n;
        BlpInfo b;
        unsigned char *d = BlpMake(16, 8, 1, 5, 1, &n);
        int ok = BlpParse(d, n, &b) && b.w == 16 && b.h == 8 && b.dxt == 1 && b.alpha == 0 && b.levels == 5 &&
                 b.off[0] == 1172 && b.len[0] == 64 && b.off[1] == 1172 + 64 && b.len[1] == 16 && b.len[4] == 8 &&
                 b.off[4] + b.len[4] == n;
        ok &= BlpParse(d, n - 1, &b) && b.levels == 4;                  /* cut short: the last level not whole, so not used */
        d[11] = 0; ok &= BlpParse(d, n, &b) && b.levels == 1;           /* no smaller copies: the picture alone */
        d[11] = 1; d[10] = 1; ok &= !BlpParse(d, n, &b);                 /* DXT3: not one it reads */
        d[10] = 0; d[3] = '1'; ok &= !BlpParse(d, n, &b);                /* BLP1 */
        d[3] = '2'; ok &= !BlpParse(d, 1000, &b);                        /* no room for its header */
        unsigned past = n; memcpy(d + 20, &past, 4); ok &= !BlpParse(d, n, &b);   /* its first level past the end */
        free(d);
        d = BlpMake(6, 8, 1, 1, 0, &n); ok &= !BlpParse(d, n, &b); free(d);   /* not whole blocks */
        d = BlpMake(8, 8, 5, 4, 1, &n);
        ok &= BlpParse(d, n, &b) && b.dxt == 5 && b.alpha == 8 && b.levels == 4 && b.len[0] == 64 && b.len[3] == 16;
        free(d);
        Check(ok, "the view's picture: a BLP2's DXT1 or DXT5 levels found and checked against its length; anything else refused");
    }
    /* decoded here for a driver without DXT: the four colours, DXT1's see-through, DXT5's alpha, a level under a block */
    {
        unsigned char k[16] = { 0x00, 0xF8, 0x1F, 0x00, 0xE4, 0, 0, 0 }, o[16 * 4];
        DxtDecode(k, 4, 4, 1, o);
        int ok = Px(o, 0, 255, 0, 0, 255) && Px(o, 1, 0, 0, 255, 255) && Px(o, 2, 170, 0, 85, 255) && Px(o, 3, 85, 0, 170, 255) &&
                 Px(o, 4, 255, 0, 0, 255);
        unsigned char k3[8] = { 0x1F, 0x00, 0x00, 0xF8, 0xE4, 0, 0, 0 };   /* c0 < c1: three colours and see-through */
        DxtDecode(k3, 4, 4, 1, o);
        ok &= Px(o, 0, 0, 0, 255, 255) && Px(o, 2, 127, 0, 127, 255) && Px(o, 3, 0, 0, 0, 0);
        unsigned char k5[16] = { 255, 0, 0x88, 0, 0, 56, 0, 0, 0x00, 0xF8, 0x1F, 0x00, 0, 0, 0, 0 };
        DxtDecode(k5, 4, 4, 5, o);
        ok &= o[3] == 255 && o[7] == 0 && o[11] == 218 && o[9 * 4 + 3] == 36 && o[15 * 4 + 3] == 255 && Px(o, 1, 255, 0, 0, 0);
        k5[0] = 0; k5[1] = 255; k5[5] = 7 << 3 | 0; k5[2] = 6 << 3; k5[3] = 0;   /* a0 <= a1: six steps, then 0 and 255 */
        DxtDecode(k5, 4, 4, 5, o);
        ok &= o[1 * 4 + 3] == 0 && o[9 * 4 + 3] == 255 && o[3] == 0;
        unsigned char *small = malloc(2 * 2 * 4);                        /* a 2 x 2 level: four texels, nothing past them */
        DxtDecode(k, 2, 2, 1, small);
        ok &= Px(small, 0, 255, 0, 0, 255) && Px(small, 1, 0, 0, 255, 255) && Px(small, 2, 255, 0, 0, 255);
        free(small);
        Check(ok, "the view's picture decoded here: DXT1's four colours and its see-through fourth, DXT5's alpha both ways, a level smaller than a block");
    }
    /* slice 3: palette pictures (the fire bars): indices, then 8, 4 or 1 bits of alpha; the palette B G R */
    {
        unsigned w = 4, h = 2, n = 1172 + 8 + 8 + 2 + 2, l0 = 16, l1 = 4, o0 = 1172, o1 = 1172 + 16;
        unsigned char *d = calloc(1, n);
        memcpy(d, "BLP2", 4); d[4] = 1; d[8] = 1; d[9] = 8; d[10] = 8; d[11] = 1;
        memcpy(d + 12, &w, 4); memcpy(d + 16, &h, 4);
        memcpy(d + 20, &o0, 4); memcpy(d + 24, &o1, 4); memcpy(d + 84, &l0, 4); memcpy(d + 88, &l1, 4);
        d[BLP_PALETTE + 4 * 1] = 10; d[BLP_PALETTE + 4 * 1 + 1] = 20; d[BLP_PALETTE + 4 * 1 + 2] = 30;   /* colour 1: B G R */
        d[BLP_PALETTE + 4 * 2 + 2] = 200;                                                                  /* colour 2: red */
        for (int i = 0; i < 8; i++) { d[o0 + i] = (unsigned char)(i & 1 ? 2 : 1); d[o0 + 8 + i] = (unsigned char)(i * 30); }
        BlpInfo b;
        unsigned char o[32];
        int ok = BlpParse(d, n, &b) && b.dxt == 0 && b.alpha == 8 && b.w == 4 && b.h == 2 && b.levels == 2 && b.len[0] == 16 &&
                 b.len[1] == 4;
        BlpPalDecode(d, &b, 0, 4, 2, o);
        ok &= Px(o, 0, 30, 20, 10, 0) && Px(o, 1, 200, 0, 0, 30) && Px(o, 7, 200, 0, 0, 210);
        d[9] = 1; ok &= BlpParse(d, n, &b) && b.len[0] == 8 + 1;          /* one bit: a byte for the eight */
        d[o0 + 8] = 0x01; BlpPalDecode(d, &b, 0, 4, 2, o);
        ok &= o[3] == 255 && o[7] == 0 && o[31] == 0;                    /* texel 0 on alone: low bit first */
        d[9] = 4; ok &= BlpParse(d, n, &b) && b.len[0] == 8 + 4;
        d[o0 + 8] = 0xF3; BlpPalDecode(d, &b, 0, 4, 2, o);
        ok &= o[3] == 3 * 17 && o[7] == 255;                              /* low nibble first */
        d[9] = 2; ok &= !BlpParse(d, n, &b);                              /* two bits: not a depth BLP2 has */
        free(d);
        Check(ok, "slice 3: palette pictures read (8, 4 and 1 bits of alpha, low bits first, the palette's B G R); another alpha depth refused");
    }
    Check(TextHas("GL_ARB_multitexture GL_EXT_texture_compression_s3tc GL_EXT_bgra", "GL_EXT_texture_compression_s3tc") &&
          !TextHas("GL_EXT_texture_compression_s3tc_srgb GL_EXT_bgra", "GL_EXT_texture_compression_s3tc") &&
          !TextHas("GL_EXT_texture", "GL_EXT_texture_compression_s3tc") && !TextHas("", "GL_EXT_bgra") &&
          TextHas("  GL_EXT_bgra ", "GL_EXT_bgra"), "OpenGL's extensions: a whole word only");
    /* its frame: the world map over the whole window at the zoom's own corners, then the blade along the bottom (6 Oct
     * 2026, his design: one bar for every class, the same size, only its colour the class's, unstable like the Darksaber,
     * the logo on it as its halfway mark; 7 Oct: solid, in the dark metal frame of his reference, flush with the bottom
     * of the screen; then, from his reference: a smooth fill, light fullest in the middle; then alive: surges, an orb,
     * lightning). The blade's batches, in order: the channel, the glow (added), the fill (and the way to go), its
     * filaments (added), the frame, its shine (added), the lips' light and the lightning (added), the light (the spark,
     * added: the orb, its embers, the surges' light, where the lightning strikes, the logo's), the logo */
    {
        static const float uv[8] = { 0.2f, 0.7f, 0.6f, 0.7f, 0.2f, 0.3f, 0.6f, 0.3f };
        memcpy(g_bgUV, uv, sizeof uv);
        Check(g_saberStyle == (SABER_PULSE | SABER_RAW), "the blade's qualities: pulse and raw, his pick (7 Oct: \"merge 2 and 4\"), the others there to switch on");
        g_saberStyle = 0;                                                /* (the blade itself, as it was, below) */
        g_ovOn = 0; g_logoOn = 0; g_class = 0; g_frameOn = 0;
        for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
        ViewBuild(1000, 500, 0.0f, 0);
        const ViewVert *v = g_vv;
        int ok = g_vb[0].tex == VT_MAP && g_vb[0].first == 0 && g_vb[0].count == 4 && v[0].x == 0 && v[0].y == 0 &&
                 v[0].u == 0.2f && v[0].v == 0.3f && v[2].x == 1000 && v[2].y == 500 && v[2].u == 0.6f && v[2].v == 0.7f &&
                 v[1].x == 1000 && v[1].y == 0 && v[0].c[0] == 255 && v[0].c[3] == 255;
        Check(ok, "the view's frame: the world map over the whole window at the zoom's corners, top left to bottom right");
        /* the frame's table: its sheet, four pieces, the rail band and the channel; a bad one is no frame */
        static const char bar[] = "# the frame\nsheet 1024 256\nleft 0 0 132 180\nrail 136 0 232 180\r\ncentre 236 0 336 180\n"
                                  "right 340 0 470 180\nband 33 123\nchannel 56 101\nends 82 20\nhum 1 2 3 4\n";
        FrameParse(bar, sizeof bar - 1);
        int parsed = g_frame.ok && g_frame.sw == 1024 && g_frame.r[FR_RAIL][0] == 136 && g_frame.r[FR_RIGHT][2] == 470 &&
                     g_frame.band[1] == 123 && g_frame.chan[0] == 56 && g_frame.ends[0] == 82 && g_frame.ends[1] == 20;
        static const char noRight[] = "sheet 1024 256\nleft 0 0 132 180\nrail 136 0 232 180\ncentre 236 0 336 180\nband 33 123\nchannel 56 101\n";
        static const char offSheet[] = "sheet 256 256\nleft 0 0 132 180\nrail 136 0 232 180\ncentre 236 0 336 180\nright 340 0 470 180\nband 33 123\nchannel 56 101\n";
        static const char outside[] = "sheet 1024 256\nleft 0 0 132 180\nrail 136 0 232 180\ncentre 236 0 336 180\nright 340 0 470 180\nband 33 123\nchannel 20 101\n";
        FrameParse(noRight, sizeof noRight - 1);
        parsed &= !g_frame.ok;                                            /* no right cap */
        FrameParse(offSheet, sizeof offSheet - 1);
        parsed &= !g_frame.ok;                                            /* a piece off its sheet */
        FrameParse(outside, sizeof outside - 1);
        parsed &= !g_frame.ok;                                            /* a channel outside the band */
        static const char noEnds[] = "sheet 1024 256\nleft 0 0 132 180\nrail 136 0 232 180\ncentre 236 0 336 180\nright 340 0 470 180\nband 33 123\nchannel 56 101\n";
        FrameParse(noEnds, sizeof noEnds - 1);
        parsed &= g_frame.ok && g_frame.ends[0] == 132 && g_frame.ends[1] == 0;   /* no ends line: at the caps' inner edges */
        static const char farEnds[] = "sheet 1024 256\nleft 0 0 132 180\nrail 136 0 232 180\ncentre 236 0 336 180\nright 340 0 470 180\nband 33 123\nchannel 56 101\nends 200 20\n";
        FrameParse(farEnds, sizeof farEnds - 1);
        parsed &= !g_frame.ok;                                            /* an end past its cap */
        parsed &= g_frame.lips[0] == 132 && g_frame.lips[1] == 0;          /* no lips line: the lit lips cap to cap */
        static const char lips[] = "sheet 1024 256\nleft 0 0 132 180\nrail 136 0 232 180\ncentre 236 0 336 180\nright 340 0 470 180\nband 33 123\nchannel 56 101\nlips 100 40\n";
        FrameParse(lips, sizeof lips - 1);
        parsed &= g_frame.ok && g_frame.lips[0] == 100 && g_frame.lips[1] == 40;
        static const char farLips[] = "sheet 1024 256\nleft 0 0 132 180\nrail 136 0 232 180\ncentre 236 0 336 180\nright 340 0 470 180\nband 33 123\nchannel 56 101\nlips 100 131\n";
        FrameParse(farLips, sizeof farLips - 1);
        parsed &= !g_frame.ok;                                            /* lips past their cap */
        FrameParse(bar, sizeof bar - 1);
        Check(parsed && g_frame.ok, "the frame's table read (CRLF, comments, an unknown line skipped); one missing a piece, with a piece off its sheet, its channel outside its band, an end or a lip past its cap is no frame; no ends line, the channel's ends at the caps' inner edges; no lips line, the lips lit from cap to cap");
        g_frameOn = 1; g_viewTex[VT_FRAME] = 1;
        float s = FRAME_BAND * 500 / 90, top = 500 - 123 * s, cy = top + 78.5f * s, ch = 22.5f * s;
        float fs, ftop, fcy, fch, fx0, fx1, x0 = 82 * s, x1 = 1000 - (130 - 20) * s, span = x1 - x0;
        FrameLayout(1000, 500, &fs, &ftop, &fcy, &fch, &fx0, &fx1);
        Check(Near(fs, s, 1e-5) && Near(ftop, top, 1e-3) && Near(fcy, cy, 1e-3) && Near(fch, ch, 1e-3) && Near(ftop + 123 * fs, 500, 1e-3) &&
              Near(90 * fs, FRAME_BAND * 500, 1e-3) && Near(fx0, x0, 1e-3) && Near(fx1, x1, 1e-3),
              "the frame's place: its rail band a fifteenth of the screen tall, its bottom ON the bottom of the screen (his call: no gap), the channel the blade runs in where the table says, from inside the left cap to inside the right (his call, 7 Oct: 'inside of the metal')");
        /* nothing loaded yet: the dark channel and the way to go, then the frame over them */
        ViewBuild(1000, 500, 0.0f, 0);
        const ViewVert *chq = g_vv + g_vb[1].first, *trq = chq + 4;
        float rnd = ch * 1.2f;
        ok = g_vbN == 3 && g_vb[1].tex == -1 && !g_vb[1].add && g_vb[1].count == 8 && g_vb[2].tex == VT_FRAME && !g_vb[2].add &&
             Near(chq[0].x, x0, 1e-3) && Near(chq[1].x, x1, 1e-3) && Near(chq[0].y, cy - ch, 1e-3) && Near(chq[2].y, cy + ch, 1e-3) &&
             Near(trq[0].x, x0, 1e-3) && Near(trq[1].x, x1, 1e-3) && trq[0].c[3] == 0x5A && trq[0].c[0] == 0 && Near((trq[0].y + trq[3].y) / 2, cy, 1e-3);
        Check(ok, "the blade, nothing loaded: its dark channel from cup to cup (the caps' back walls) and the way to go a faint dark line along it, in its frame");
        /* the frame's pieces: a cap at each edge, the diamond at the centre, the rail repeated between with no gap, all
         * at one scale, their tops at the frame's top */
        const ViewVert *fr = g_vv + g_vb[2].first;
        int nq = g_vb[2].count / 4, tiles = nq - 3, edges = 1, ends = 0;
        float wl = 132 * s, wr = 130 * s, wc = 100 * s, wt = 96 * s, cx = (1000 - wc) / 2, at = wl;
        for (int q = 0; q < tiles; q++) {                                 /* the rails, left run then right */
            const ViewVert *t2 = fr + 4 * q;
            if (Near(t2[0].x, cx + wc, 1e-3) && q) at = cx + wc;
            edges &= Near(t2[0].x, at, 1e-3) && Near(t2[0].y, top, 1e-3) && Near(t2[2].y, top + 180 * s, 1e-3) &&
                     Near(t2[0].u, 136.5 / 1024.0, 1e-6) && t2[1].x <= (at < cx ? cx : 1000 - wr) + 1e-3f &&
                     (t2[1].x - t2[0].x > wt - 1e-3f ? Near(t2[1].u, 231.5 / 1024.0, 1e-6)
                                                      : Near(t2[1].u - t2[0].u, (95 / 1024.0) * (t2[1].x - t2[0].x) / wt, 1e-6));
            at = t2[1].x;
            ends += Near(t2[1].x, cx, 1e-3) || Near(t2[1].x, 1000 - wr, 1e-3);   /* each run reaches what it meets */
        }
        const ViewVert *lc = fr + 4 * tiles, *cc = lc + 4, *rc = cc + 4;
        edges &= Near(lc[0].x, 0, 1e-6) && Near(lc[1].x, wl, 1e-3) && Near(cc[0].x, cx, 1e-3) && Near(cc[1].x, cx + wc, 1e-3) &&
                 Near(rc[1].x, 1000, 1e-3) && Near(rc[0].x, 1000 - wr, 1e-3) && Near(rc[0].u, 340.5 / 1024.0, 1e-6) &&
                 Near(rc[1].u, 469.5 / 1024.0, 1e-6) &&
                 Near(rc[2].v, 180 / 256.0, 1e-6) && tiles > 6 && tiles < FRAME_TILES && ends == 2;
        Check(edges, "the frame laid across the screen: the caps at its edges, the diamond at its centre, the rail repeated between them with no gap (each run's last piece cut where it meets), all at one scale, each piece half a texel in from its edges on the sheet (no seam where two meet)");
        /* a quarter loaded, a warlock */
        g_class = 9; g_viewTex[VT_SPARK] = 1;
        ViewBuild(1000, 500, 0.25f, 1234);
        enum { CH = 1, GL, CO, STR, FRAME, SHINE, LIPS, LIGHT };
        ok = g_vbN == 9 && g_vb[CH].tex == -1 && !g_vb[CH].add && g_vb[CH].count == 4 && g_vb[GL].tex == -1 && g_vb[GL].add &&
             g_vb[CO].tex == -1 && !g_vb[CO].add && g_vb[STR].tex == -1 && g_vb[STR].add && g_vb[STR].count > 0 &&
             g_vb[STR].count % (8 * SABER_STRANDS) == 0 && g_vb[FRAME].tex == VT_FRAME &&
             !g_vb[FRAME].add && g_vb[LIPS].tex == -1 && g_vb[LIPS].add && g_vb[LIPS].count == 8 + 8 * ARC_SEGS * SABER_ARCS &&
             g_vb[LIGHT].tex == VT_SPARK && g_vb[LIGHT].add && g_vb[LIGHT].count == 4 * (ORB_GLOWS + SABER_SPARKS + SURGE_GLOWS + SABER_DROPS + SABER_BLOOMS + SABER_ARCS);
        int cols = (g_vb[CO].count / 4 - 1) / 2;                         /* the way to go, then two quads a column */
        float front = x0 + 0.25f * span;
        ok &= cols > 30 && g_vb[CO].count == 4 * (1 + 2 * cols) && g_vb[GL].count == 16 * cols;
        const ViewVert *tr = g_vv + g_vb[CO].first, *co = tr + 4, *last = co + 8 * (cols - 1);
        ok &= Near(tr[0].x, front, 1e-3) && Near(tr[1].x, x1, 1e-3) && Near((tr[0].y + tr[3].y) / 2, cy, 1e-3);   /* to go */
        ok &= Near(co[0].x, x0, 1e-3) && Near(last[1].x, front, 1e-3) && last[2].x == last[1].x;          /* cap to tip */
#define NEAR(c, r, g, b) (abs((c)[0] - (r)) <= 1 && abs((c)[1] - (g)) <= 1 && abs((c)[2] - (b)) <= 1)
        int body = 1, lit = 1;
        for (int k = 0; k < cols; k++) {
            const ViewVert *q = co + 8 * k, *lo = q + 4;                  /* its top half and its bottom half */
            body &= q[3].x == q[0].x && q[2].x == q[1].x && q[1].x > q[0].x && lo[0].x == q[0].x && lo[1].x == q[1].x;
            body &= q[2].y == cy && q[3].y == cy && lo[0].y == cy && lo[1].y == cy;                /* meeting on the line */
            if (q[1].x <= front - rnd && q[0].x >= x0 + rnd)                                       /* smooth: it fills */
                body &= Near(q[0].y, cy - ch, 1e-3) && Near(q[1].y, cy - ch, 1e-3) && Near(lo[3].y, cy + ch, 1e-3);   /* it all */
            body &= q[0].y >= cy - ch - 1e-3f && lo[3].y <= cy + ch + 1e-3f && q[0].c[3] == 247 && q[3].c[3] == 247;
            body &= q[3].c[0] >= q[0].c[0] && q[3].c[1] >= q[0].c[1] && q[3].c[2] >= q[0].c[2];   /* paler at its centre */
            if (k + 1 < cols) body &= co[8 * (k + 1)].x == q[1].x;                                   /* unbroken */
        }
        int mc = cols / 2;                                                /* (mid-blade: its foot and tip are tapered) */
        const ViewVert *gl0 = g_vv + g_vb[GL].first + 16 * mc, *tip = g_vv + g_vb[LIGHT].first;
        const ViewVert *lip = g_vv + g_vb[LIPS].first;
        lit &= NEAR(gl0[0].c, 132, 91, 255) && gl0[0].c[3] == 0 && gl0[3].c[3] > 100 &&      /* a richer purple than */
               gl0[0].y < gl0[3].y && fabsf(gl0[3].y - co[8 * mc].y) < 1e-3f;               /* Blizzard's pale one */
        lit &= fabsf((tip[0].x + tip[1].x) / 2 - front) < 1e-3f && fabsf((tip[0].y + tip[2].y) / 2 - cy) < 1e-3f;
        lit &= Near(lip[0].x, 132 * s, 1e-3) && Near(lip[1].x, front, 1e-3) && NEAR(lip[3].c, 181, 157, 255) && lip[3].c[3] > 150 && lip[0].c[3] == 0 &&
               Near(lip[3].y, cy - ch + 500 / 1440.0f, 1e-3) && lip[4 + 0].y > cy;                  /* the lips, lit */
        /* its foot: full height right up to the back of the left cap's cup (the metal shapes it: his call, "filled in
         * until it hits the metal"); only its moving tip is rounded */
        float foot0 = co[0].y, mid = co[8 * (cols / 2)].y, tipy = last[1].y;
        ok &= Near(foot0, cy - ch, 1e-3) && Near(mid, cy - ch, 1e-3) && Near(tipy, cy, 1e-3);
        Check(ok && body, "the blade, a quarter loaded: its glow added, the way to go a dark line from the tip to the right cap, the fill SOLID and SMOOTH the whole height of its channel (his calls, 7 Oct), paler along its centre line, from the back of the left cap's cup (square, under the metal) to its rounded tip in columns that join");
        Check(lit, "the blade's colour: a warlock's purple glow, richer than Blizzard's pale text colour, fading out from the fill, the tip flaring at the bar as shown, the frame's lips lit from the left cap's rim to the tip");
        /* the blade's light on the metal (his ask, 7 Oct: "make it look cooler"): the frame again, added, in the class's
         * light, four bands a piece (fading in above the rail band, full across it, fading out below); lit over the
         * stretch the blade has filled, brightest at its tip, dark ahead of it */
        int nfq = g_vb[FRAME].count / 4, shine = g_vb[SHINE].tex == VT_FRAME && g_vb[SHINE].add && g_vb[SHINE].count == 16 * nfq;
        const ViewVert *sh = g_vv + g_vb[SHINE].first, *fq = g_vv + g_vb[FRAME].first;
        float best = 0, bestX = 0;
        int rgbOk = 1;
        for (int q = 0; q < nfq && shine; q++) {
            const ViewVert *b0 = sh + 16 * q, *pc = fq + 4 * q;
            shine &= b0[0].c[3] == 0 && b0[1].c[3] == 0 && b0[3].c[3] == b0[4].c[3] && b0[12 + 2].c[3] == 0 &&   /* fade in, out */
                     Near(b0[0].y, pc[0].y, 1e-3) && Near(b0[15].y, pc[3].y, 1e-3) && Near(b0[0].x, pc[0].x, 1e-3) &&
                     Near(b0[3].y, top + 33 * s, 1e-3) && Near(b0[7].y, cy, 1e-3) && Near(b0[11].y, top + 123 * s, 1e-3);
            rgbOk &= NEAR(b0[4].c, 132, 91, 255);                    /* the warlock's own purple, as the glow */
            float xa = (b0[4].x + b0[5].x) / 2;
            int a = b0[4].c[3] + b0[5].c[3];
            if (a > best) { best = (float)a; bestX = xa; }
            if (b0[4].x > front + 4 * SHINE_REACH * 1000) shine &= b0[4].c[3] == 0 && b0[5].c[3] == 0;   /* dark ahead */
            if (b0[5].x < front - 4 * SHINE_REACH * 1000) shine &= b0[4].c[3] >= (int)(SHINE_LIT * 255 + 0.5f) - 1;   /* lit behind */
        }
        shine &= rgbOk && fabsf(bestX - front) < 96 * s && best > 2 * SHINE_LIT * 255 + 20;   /* brightest at the tip */
        Check(shine, "the blade's light on the metal: the frame again, added, in the class's colour, full across the rail band and fading to nothing at the pieces' tips; lit over the stretch the blade has filled, brightest where its tip is, dark ahead of it");
        /* the light fullest in the middle of the bar (his reference): whiter along the centre line there, deeper toward
         * the ends, the fill's top and bottom edges deeper still */
        float mC = 0, eC = 0, mE = 0, eE = 0, eR = 0, eR0 = 0, mG = 0;           /* (on the whole: the surges pass) */
        for (int i = 0; i < 30; i++) {
            ViewBuild(1000, 500, 1.0f, 1234 + 97 * i);
            int fc = g_vb[CO].count / 8;
            const ViewVert *mq = g_vv + g_vb[CO].first + 8 * (fc / 2), *eq = g_vv + g_vb[CO].first + 8 * (fc / 12);
            mC += mq[3].c[0] + mq[3].c[1]; eC += eq[3].c[0] + eq[3].c[1]; mE += mq[0].c[0] + mq[0].c[1]; eE += eq[0].c[0] + eq[0].c[1];
            eR += eq[3].c[0]; eR0 += eq[0].c[0]; mG += mq[3].c[1];
        }
        int whiter = mC / 30 > eC / 30 + 60 && mE / 30 > eE / 30 + 30 && eR > eR0 && mG / 30 > 180;
        Check(whiter, "the light fullest in the middle of the bar (his reference): near white along its centre there, the class's colour deeper toward the bar's ends and at the fill's edges");
        ViewBuild(1000, 500, 0.25f, 1234);
        Check(SaberVivid(0xFFFFFF) == 0xFFFFFF && SaberVivid(0x0070DE) == 0x0081FF && SaberVivid(0xFF7D0A) == 0xFF7B06,
              "a class's light keeps its hue: a priest's stays white, a shaman's blue and a druid's orange only brighten");
        /* the same blade for every class, only its colour: a hunter's at the same moment */
        static float pos[2 * VIEW_QUADS_MAX * 4];
        int nv = g_vvN;
        for (int i = 0; i < nv; i++) { pos[2 * i] = g_vv[i].x; pos[2 * i + 1] = g_vv[i].y; }
        g_class = 3;
        ViewBuild(1000, 500, 0.25f, 1234);
        int same = g_vvN == nv, recoloured = 0;
        for (int i = 0; i < nv && same; i++) same &= pos[2 * i] == g_vv[i].x && pos[2 * i + 1] == g_vv[i].y;
        recoloured = NEAR(g_vv[g_vb[GL].first].c, 179, 255, 76);
        Check(same && recoloured, "the blade is the same for every class, only its colour the class's (a hunter's green, every corner where the warlock's was)");
        /* never quite still: two seconds on the light has breathed, the fill's shape and place the same */
        g_class = 9;
        ViewBuild(1000, 500, 0.25f, 1234);
        static unsigned char before[4];
        int nv2 = g_vvN;
        for (int i = 0; i < nv2; i++) { pos[2 * i] = g_vv[i].x; pos[2 * i + 1] = g_vv[i].y; }
        memcpy(before, g_vv[g_vb[CO].first + 4 + 8 * (cols / 2) + 3].c, 4);
        ViewBuild(1000, 500, 0.25f, 1234 + 2000);
        int steady = g_vvN == nv2;
        for (int i = g_vb[CO].first; i < g_vb[CO].first + g_vb[CO].count && steady; i++)     /* (the fill's corners) */
            steady &= pos[2 * i] == g_vv[i].x && pos[2 * i + 1] == g_vv[i].y;
        int moved = memcmp(g_vv[g_vb[CO].first + 4 + 8 * (cols / 2) + 3].c, before, 3) != 0;
        g_class = 3;
        Check(steady && moved, "the blade breathes: two seconds on its light has changed, the fill's shape and place have not (smooth, not crackling: his call)");
        /* alive (his ask, 7 Oct: "it needs to 'be alive' and be more dynamic, like it's powerful, and the leading edge
         * needs to be more variable like a pulsing orb of energy, and the whole bar should have some kind of effects"):
         * the surges run from the foot to the tip at one speed, leaving unevenly, and each that arrives makes the orb flare
         * (a warlock's blade throughout, its purple's green the measure of how white it runs) */
        g_class = 9;
        {
            float px[8], pa[8], qx[8], qa[8], fl = 0, fl2 = 0, dt = 1.0f / 120, xs0 = 100, sspan = 800, sfront = 700, sch = 10;
            int runs = 1, arrivals = 0, flared = 0, beats = 0, high = 0;
            float lastBeat = -1, gapLo = 1e9f, gapHi = 0;
            for (int f = 0; f < 120 * 30; f++) {
                float tt = 10 + f * dt;
                int a = SaberSurges(tt, xs0, sspan, sfront, sch, px, pa, 8, &fl);
                int b2 = SaberSurges(tt + dt, xs0, sspan, sfront, sch, qx, qa, 8, &fl2);
                runs &= fl >= 0 && fl <= 1 && a <= 8;
                for (int i = 0; i < a; i++) {
                    runs &= px[i] > xs0 && px[i] < sfront && pa[i] > 0 && pa[i] <= 1;
                    float want = px[i] + dt / ORB_TRAVEL * sspan;
                    int found = 0;
                    for (int j = 0; j < b2; j++) found |= fabsf(qx[j] - want) < 1e-2f;
                    if (want < sfront - 1e-2f) runs &= found;               /* on its way: a step on, at one speed */
                    else if (!found) {                                       /* arrived: the orb flares within 60 ms */
                        float most = 0, d1[8], d2[8];
                        for (int k = 1; k <= 8; k++) {
                            float ff;
                            SaberSurges(tt + k * dt, xs0, sspan, sfront, sch, d1, d2, 8, &ff);
                            if (ff > most) most = ff;
                        }
                        arrivals++;
                        flared += most >= 0.5f;
                    }
                }
                if (fl2 >= 0.45f && !high) {                                 /* a beat */
                    if (lastBeat >= 0) { float gp = tt - lastBeat; if (gp < gapLo) gapLo = gp; if (gp > gapHi) gapHi = gp; }
                    lastBeat = tt; beats++;
                }
                high = fl2 >= 0.45f ? 1 : fl2 < 0.3f ? 0 : high;
            }
            Check(runs && arrivals > 20 && flared == arrivals && beats >= 20 && beats <= 60 && gapLo < 0.35f && gapHi > 0.8f,
                  "alive: power runs down the blade: surges leave its foot unevenly (beats as close as a third of a second and as far as most of one), run the bar at one speed, and each one that reaches the tip makes the orb there flare");
        }
        /* the orb: at the tip, bigger than the channel, swelling as it flares; the same at the same moment */
        {
            float rLo = 1e9f, rHi = 0, f0 = x0 + 0.4f * span;
            int at = 1, same = 1;
            for (int f = 0; f < 60 * 6; f++) {
                DWORD nw = 50000 + (DWORD)(f * 1000 / 60);
                ViewBuild(1000, 500, 0.4f, nw);
                const ViewVert *o = g_vv + g_vb[LIGHT].first, *body = o + 4;
                float r = (body[2].y - body[0].y) / 2 / 1.55f;
                at &= g_vb[LIGHT].tex == VT_SPARK && fabsf((o[0].x + o[1].x) / 2 - f0) < 1e-3f && fabsf((o[0].y + o[2].y) / 2 - cy) < 1e-3f &&
                      fabsf((body[0].x + body[1].x) / 2 - f0) < 1e-3f && NEAR(o[0].c, 132, 91, 255) && NEAR(body[0].c, 132, 91, 255);
                if (r < rLo) rLo = r;
                if (r > rHi) rHi = r;
                if (f == 100) {
                    static ViewVert keep[VIEW_QUADS_MAX * 4];
                    int nk = g_vvN;
                    memcpy(keep, g_vv, sizeof(ViewVert) * nk);
                    ViewBuild(1000, 500, 0.4f, nw);
                    same = g_vvN == nk && !memcmp(keep, g_vv, sizeof(ViewVert) * nk);
                }
            }
            Check(at && same && rLo >= ch * 1.0f && rHi >= rLo * 1.25f,
                  "alive: the orb at the tip, centred on it, bigger than the channel (its light spills onto the metal), swelling by a quarter and more as it flares; the same at the same moment");
        }
        /* lightning: from the orb onto the metal and into the dark way ahead, crackles off the fill anywhere along it;
         * never off the bar (the frame's band); a dark one drawn dark */
        {
            float f0 = x0 + 0.5f * span, bandTop = top + 33 * s, bandBot = top + 123 * s;
            int inside = 1, fromOrb = 1, metal = 0, ahead = 0, along = 0, lit = 0;
            for (int f = 0; f < 30 * 4; f++) {
                ViewBuild(1000, 500, 0.5f, 70000 + (DWORD)(f * 1000 / 30));
                const ViewVert *ar = g_vv + g_vb[LIPS].first + 8;
                inside &= g_vb[LIPS].count == 8 + 8 * ARC_SEGS * SABER_ARCS;
                for (int a = 0; a < SABER_ARCS; a++) {
                    const ViewVert *q = ar + a * 8 * ARC_SEGS, *endq2 = q + 8 * (ARC_SEGS - 1);
                    if (q[0].c[3] == 0) continue;
                    lit++;
                    for (int v = 0; v < 8 * ARC_SEGS; v++) inside &= q[v].y >= bandTop - 1.5f && q[v].y <= bandBot + 1.5f;
                    float ex = endq2[1].x, ey = endq2[1].y;
                    if (a < ORB_ARCS) {
                        float ddx = q[0].x - f0, ddy = q[0].y - cy;
                        fromOrb &= ddx * ddx + ddy * ddy <= (ch * 1.9f * 0.3f + 1) * (ch * 1.9f * 0.3f + 1);
                        metal += fabsf(ey - cy) > ch + 0.5f;
                        ahead += ex > f0 + ch * 1.2f && fabsf(ey - cy) < ch;       /* in the channel, ahead */
                    } else {
                        inside &= fabsf(q[0].y - cy) <= ch;                   /* off the fill */
                        along += q[0].x < f0 - 0.3f * span;
                    }
                }
            }
            Check(inside && fromOrb && lit > 40 && metal > 5 && ahead > 5 && along > 2,
                  "alive: lightning from the orb onto the metal above and below and into the dark way ahead, and crackles off the fill onto the rails anywhere along the bar; never off the frame's band");
        }
        /* the filaments: inside the fill, wandering along it */
        {
            float f0 = x0 + 0.5f * span;
            ViewBuild(1000, 500, 0.5f, 80000);
            int in = g_vb[STR].count > 0, some = 0, nv3 = g_vb[STR].count;
            static float ys[VIEW_QUADS_MAX * 4];
            for (int i = 0; i < nv3; i++) {
                const ViewVert *v = g_vv + g_vb[STR].first + i;
                in &= fabsf(v->y - cy) <= ch && v->x >= x0 - 1e-3f && v->x <= f0 + 1e-3f;
                some |= v->c[3] > 0;
                ys[i] = v->y;
            }
            ViewBuild(1000, 500, 0.5f, 80500);
            int moved2 = 0;
            for (int i = 0; i < nv3 && i < g_vb[STR].count; i++) moved2 |= fabsf(g_vv[g_vb[STR].first + i].y - ys[i]) > 0.5f;
            Check(in && some && moved2, "alive: faint filaments of the light inside the fill, behind the orb, wandering as they flow");
        }
        /* a surge whitens the fill right across where it is passing (the class's colour elsewhere: the churn never
         * reaches white), lights the metal over it, and its light spills over the rails */
        {
            int seen = 0, whiter = 1, plain = 1, onMetal = 0, spill = 0;
            unsigned g0 = SaberVivid(kClassColour[9]) >> 8 & 255;
            for (int f = 0; f < 30 * 6; f++) {
                ViewBuild(1000, 500, 0.8f, 90000 + (DWORD)(f * 1000 / 30));
                int cols2 = (g_vb[CO].count / 4 - 1) / 2;
                for (int k = 0; k <= cols2; k++) {
                    unsigned gt = g_sbTop[k] >> 8 & 255;
                    if (g_sbSurge[k] >= 0.5f) { seen++; whiter &= gt >= g0 + 20; }
                    if (g_sbSurge[k] == 0) plain &= gt <= g0;
                }
                float sx2[8], sa2[8], fl3;
                int n2 = SaberSurges((float)((90000 + f * 1000 / 30) % 3600000u) / 1000.0f, x0, span, x0 + 0.8f * span, ch, sx2, sa2, 7, &fl3);
                const ViewVert *sg = g_vv + g_vb[LIGHT].first + 4 * (ORB_GLOWS + SABER_SPARKS);
                for (int i = 0; i < n2; i++) {
                    if (sa2[i] < 0.5f || sx2[i] > x0 + 0.8f * span - 6 * SHINE_REACH * 1000) continue;
                    spill += fabsf((sg[4 * i].x + sg[4 * i + 1].x) / 2 - sx2[i]) < 1e-3f && sg[4 * i].c[3] > 0;
                    for (int q = 0; q < g_vb[SHINE].count / 16; q++) {
                        const ViewVert *b0 = g_vv + g_vb[SHINE].first + 16 * q;
                        if (fabsf((b0[4].x + b0[5].x) / 2 - sx2[i]) < 96 * s * 0.6f) onMetal += b0[4].c[3] > (int)(SHINE_LIT * 255) + 15;
                    }
                }
            }
            Check(seen > 50 && whiter && plain && onMetal > 10 && spill > 10,
                  "alive: a surge whitens the fill right across as it passes (elsewhere it keeps the class's colour, however it churns), lights the metal over it, and its light spills over the rails");
        }
        /* as the blade fills, a last surge runs the whole bar */
        {
            ViewBuild(1000, 500, 0.9f, 100000);
            ViewBuild(1000, 500, 1.0f, 100500);                              /* it fills */
            ViewBuild(1000, 500, 1.0f, 100700);                              /* 0.2 s on */
            float head = x0 + 0.2f / SWEEP_S * span;
            int cols3 = g_vb[CO].count / 8, k = (int)((head - x0) / (span / (float)cols3) + 0.5f);
            int swept = k > 0 && k < cols3 && g_sbSurge[k] >= 1.25f;
            ViewBuild(1000, 500, 0.5f, 101000);                              /* (a new screen) */
            Check(swept && g_sbFullAt == 0, "alive: as the blade fills a last surge runs its whole length (a new screen begins it afresh)");
        }
        g_class = 3;
        /* full: edge to edge, no way to go, no tip; the metal flashing as it fills, then lit along its whole length */
        ViewBuild(1000, 500, 1.0f, 2000);
        int flashed = 255;
        for (int i = g_vb[SHINE].first + 4; i < g_vb[SHINE].first + g_vb[SHINE].count; i += 16) if (g_vv[i].c[3] < flashed) flashed = g_vv[i].c[3];
        ViewBuild(1000, 500, 1.0f, 5000);
        cols = g_vb[CO].count / 8;
        const ViewVert *endq = g_vv + g_vb[CO].first + 8 * (cols - 1);
        ok = g_vbN == 9 && g_vb[SHINE].add && g_vb[SHINE].tex == VT_FRAME && g_vb[CO].count == 8 * cols && Near(g_vv[g_vb[CO].first].x, x0, 1e-3) &&
             g_vb[LIPS].count == 8 + 8 * ARC_SEGS * BAR_ARCS && g_vb[LIGHT].count == 4 * (SURGE_GLOWS + SABER_BLOOMS + BAR_ARCS) &&
             Near(endq[1].x, x1, 1e-3) && Near(endq[1].y, cy - ch, 1e-3) && g_vv[g_vb[CO].first].c[3] == 247 && cols == (int)(span / SABER_COL_PX);
        int settled = 255;
        for (int i = g_vb[SHINE].first + 4; i < g_vb[SHINE].first + g_vb[SHINE].count; i += 16) if (g_vv[i].c[3] < settled) settled = g_vv[i].c[3];
        ok &= settled >= (int)(SHINE_FULL * 0.94f * 255) - 1 && flashed > settled + 60;   /* the whole frame lit; the flash */
        Check(ok, "the blade full: the fill from cup to cup, full height right up to the metal at both ends (no dead space: his call), nothing still to go, no orb, its light on the whole frame, flashing as it fills; still crackling");
        /* the logo on it at the centre, its halfway mark, standing on the bottom of the screen: over the tip, grey before
         * the tip reaches it, lit after */
        g_logoOn = 1; g_logo.ok = 1; g_logo.aspect = 2; g_logo.tu0 = 0.1f; g_logo.tv0 = 0.2f; g_logo.tu1 = 0.9f; g_logo.tv1 = 0.8f;
        g_viewTex[VT_LOGO] = 1;
        ViewBuild(1000, 500, 0.25f, 3000);
        const ViewVert *lg = g_vv + g_vb[g_vbN - 1].first;
        float lh = SABER_LOGO_H * 500, foot = 500 - SABER_LOGO_FOOT * 500;
        ok = g_vb[g_vbN - 1].tex == VT_LOGO && !g_vb[g_vbN - 1].add && g_vb[g_vbN - 1].count == 4 && g_vb[g_vbN - 2].tex == VT_SPARK &&
             g_vb[g_vbN - 2].count == 4 * (ORB_GLOWS + SABER_SPARKS + SURGE_GLOWS + SABER_DROPS + SABER_BLOOMS + SABER_ARCS) && fabsf(lg[0].x - (500 - lh)) < 1e-3f && fabsf(lg[2].x - (500 + lh)) < 1e-3f &&
             fabsf(lg[2].y - foot) < 1e-3f && fabsf(lg[0].y - (foot - lh)) < 1e-3f && lg[0].u == 0.1f && lg[0].v == 0.2f &&
             lg[2].u == 0.9f && lg[2].v == 0.8f && lg[0].c[0] == 210 && lg[0].c[3] == 255;
        ViewBuild(1000, 500, 0.6f, 3000);
        lg = g_vv + g_vb[g_vbN - 1].first;
        const ViewVert *aura = g_vv + g_vb[g_vbN - 2].first + g_vb[g_vbN - 2].count - 8;
        ok &= g_vb[g_vbN - 1].tex == VT_LOGO && lg[0].c[0] == 255 && g_vb[g_vbN - 2].count == 4 * (ORB_GLOWS + SABER_SPARKS + SURGE_GLOWS + SABER_DROPS + SABER_BLOOMS + SABER_ARCS + 2) &&
              fabsf((aura[0].x + aura[1].x) / 2 - 500) < 1e-3f && fabsf((aura[0].y + aura[2].y) / 2 - (foot - lh / 2)) < 1e-3f &&
              NEAR(aura[0].c, 179, 255, 76) && aura[0].c[3] > 0;
        Check(ok, "the RavenCraft logo stands on the bottom of the screen at the blade's centre, its halfway mark, over the tip as it passes: grey before the tip reaches it, then white and lit behind in the class's colour");
        g_viewTex[VT_SPARK] = 0;                                         /* no spark: no tip, no light, the rest as ever */
        ViewBuild(1000, 500, 0.6f, 3000);
        ok = g_vbN == 9 && g_vb[FRAME].tex == VT_FRAME && g_vb[SHINE].add && g_vb[LIPS].add && g_vb[8].tex == VT_LOGO;
        Check(ok, "without its spark the blade has no orb or light, and the rest is drawn (the lightning too)");
        g_viewTex[VT_FRAME] = 0;                                         /* no frame: a channel of its size, flush, no metal */
        ViewBuild(1000, 500, 0.6f, 3000);
        FrameLayout(1000, 500, &fs, &ftop, &fcy, &fch, &fx0, &fx1);
        ok = fs == 0 && Near(fcy, 500 - FRAME_BAND * 500 / 2, 1e-3) && fx0 == 0 && fx1 == 1000;
        for (int i = 0; i < g_vbN; i++) ok &= g_vb[i].tex != VT_FRAME;
        Check(ok, "without its frame's sheet the blade runs in a channel of the same size at the bottom, edge to edge, and no frame is drawn");
        g_logoOn = 0; g_logo.ok = 0; g_class = 0; g_frameOn = 0; g_frame.ok = 0;
        for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
    }
    /* whose thread: a screen begins on the game's; the view's then moves the bar and the zoom, through the game's stalls */
    {
        Fresh("view-steps");
        g_mapShown = 1;
        { const char *mt = "map 0 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\n"; MapParse(mt, (unsigned)strlen(mt)); }
        Poke(BAR_MAP_VA, 0);
        Poke(MAP_DEST_MAP_VA, 0); PutFloat(MAP_DEST_X_VA, -5000.0f); PutFloat(MAP_DEST_Y_VA, 10000.0f);
        g_classSure = 0; g_holdMs = 0; g_driftOn = 1; g_screenWatch = 1;
        InitializeCriticalSection(&g_stepLock);
        g_viewOn = 1;
        DriftScreenGone();                                               /* the last screen ended; its view had moved them */
        int ok = g_stepLock.held == 0 && g_dispNew && g_driftNew;
        g_viewSteps = 1;
        DWORD t = 2000000;
        LONG seen = g_gameSeq;
        int ours = 0;
        ok &= !ViewReady(seen, &ours);                                   /* up, but the game has not drawn it yet */
        ok &= !ViewStep(0.9f, t, 1000, 500) && g_dispNew && g_driftNew;  /* and the view never begins it */
        PutFloat(MAP_DEST_Y_VA, 15000.0f);                               /* NEW_WORLD, during this screen: u 0.2 */
        float s = GameStep(0.0f, t);                                     /* the game's first frame begins it */
        ok &= g_viewSteps == 0 && !g_dispNew && !g_driftNew && s == 0 && g_frmCount == 1 && g_loadFrame == 1 &&
              g_gameSeq == seen + 1 && g_stepLock.held == 0 && g_screenMap == 1;
        ok &= ViewReady(seen, &ours) && ours;
        g_screenMap = 0; ViewReady(seen, &ours); ok &= !ours; g_screenMap = 1;   /* not the world map: not the view's */
        LONG now = g_gameSeq;                                            /* a screen the worker never saw begin: */
        ok &= !ViewReady(now, &ours);                                    /* still the game's first frame first */
        for (int i = 0; i < 25; i++) { t += 20; GameStep(0.02f * i, t); }   /* half a second of the game's frames */
        float p = 0.48f;
        ok &= g_destOk && g_viewH < 0.5f;
        /* the view takes them: from here its frames (7 ms) step them, the game's frame only notes its facts */
        t += 7;
        g_scrW = g_surfW = 1920; g_scrH = g_surfH = 1080;                /* Config.wtf's 16:9, the window 2:1: */
        ok &= ViewStep(p, t, 1000, 500) && g_viewSteps == 1 && g_stepLock.held == 0 && g_vbN >= 1 && g_vb[0].tex == VT_MAP;
        ok &= g_surfW == 1000 && g_surfH == 500 && g_scrW == 1920 && g_scrH == 1080;   /* built for the window it draws */
        DWORD at = g_dispAt;
        float d = g_disp, h = g_viewH;
        LONG frames = g_frmCount;
        t += 3;
        s = GameStep(p, t);
        ok &= s == g_viewShown && g_dispAt == at && g_disp == d && g_viewH == h && g_frmCount == frames + 1 && g_stepLock.held == 0;
        /* each thread reads its clock before the lock: a view step stamped before the game's last one is no pause of 49
         * days (it was a leap of three quarters of a second, at once) */
        g_viewSteps = 0; GameStep(p, t + 4);                             /* (as if the game had stepped last, at t + 4) */
        float d0 = g_disp, h0 = g_viewH, dr = g_drift;
        ok &= ViewStep(p, t + 1, 1000, 500) && g_disp == d0 && fabsf(g_viewH - h0) < 1e-6f && g_drift == dr && g_dispAt == t + 4;
        t += 4;
        /* the game stalls 0.6 s (a big add-on file): no frame of its own, its bar where it was; the view's go on, every
         * one of them a step closer, and its bar on at an even rate (his rule, 10 Oct: "the bar should load at an even rate
         * all the way up to 100% with no stutters or jumps"), never full before the game's */
        DWORD stall = t;
        int moving = 1, steps = 0;
        float lastH = g_viewH, lastU = g_viewU, most = 0, lastS = g_viewShown, dMin = 1, dMax = 0;
        while (t - stall < 600) {
            t += 7;
            ViewStep(p, t, 1000, 500);
            if (t - stall <= 300) moving &= g_viewH < lastH && g_viewU != lastU;
            lastH = g_viewH; lastU = g_viewU;
            float dS = g_viewShown - lastS;
            if (dS < dMin) dMin = dS;
            if (dS > dMax) dMax = dS;
            lastS = g_viewShown;
            if (g_viewShown > most) most = g_viewShown;
            steps++;
        }
        ok &= moving && steps > 80 && most <= EVEN_WAIT && dMin > 0 && dMax < 1.25f * dMin;   /* (on through the stall, evenly) */
        t += 7;
        GameStep(0.55f, t);                                              /* the game's next frame: its own hitch, logged */
        ok &= g_hitchN == 1 && g_hitchMs[0] + g_hitchMs[1] + g_hitchMs[2] + g_hitchMs[3] + g_hitchMs[4] + g_hitchMs[5] >= 600 &&
              g_viewSteps == 1;
        /* the hold, with the view on: its numbers set under the lock, never held through its frames */
        g_holdMs = 5000; g_map.ok = 1; g_lnMs[1] = 950; g_lnFreeze[1] = 800; g_lnN[1] = 3;
        t += 7; ViewStep(1.0f, t, 1000, 500);                            /* (the plan's on: timed) */
        PutFloat(HOLD_BAR_VA, 1.0f);
        g_holdFrame = TestHoldFrame; g_holdOn = 1; g_testFrames = 0; shim_ticks = t;
        {                                                                /* (the plan a second and a half past the floor: held to it, never hurried) */
            LONG fzh = g_lnN[g_screenKind] > 0 ? g_lnFreeze[g_screenKind] : FREEZE_GUESS_MS, bl = g_blurOn && g_blurMs > 0 ? g_blurMs : 0;
            DWORD floorAt = g_scrStartAt + (DWORD)(g_holdMs - fzh - bl > 0 ? g_holdMs - fzh - bl : 0);
            g_planEndAt = ((LONG)(floorAt - t) > 0 ? floorAt : t) + 1500;
        }
        DWORD planWas = g_planEndAt;
        HoldFrame();
        ok &= g_testFrames > 1 && g_stepLock.held == 0 && g_settleTo != 0;
        /* with the view, its last glide lands as the world appears (the hold's end, the machine's freeze, the world's moment
         * after the screen goes), the push closer its last stretch: it landed by the hold's end and pushed in half a second
         * (his 00:22 clip, 5 Oct: "rushes to get to stormwind fully zoomed in and then just sits there") */
        {
            LONG fz = g_lnN[g_screenKind] > 0 ? g_lnFreeze[g_screenKind] : FREEZE_GUESS_MS;
            DWORD land = g_settleTo, at2 = shim_ticks;
            float hEnd = g_destH / BLUR_PUSH, lastH = 1;
            ok &= g_settleView == 1 && (LONG)(land - planWas) >= fz + VIEW_WORLD_GAP_MS && g_evenEndAt == land;   /* (with the view, no
                                                               * hurry to settle: on to the plan, his 10 Oct "it rushes"; the even bar lands with it) */
            ViewStep(1.0f, at2, 1000, 500);
            ok &= g_viewH > hEnd * 1.3f;                                      /* the push still to come at the hold's end */
            int down = 1;
            for (DWORD tt = at2; (LONG)(tt - land) <= 0; tt += 7) { ViewStep(1.0f, tt, 1000, 500); down &= g_viewH <= lastH + 1e-6f; lastH = g_viewH; }
            ViewStep(1.0f, land + 7, 1000, 500);
            ok &= down && fabsf(g_viewH - hEnd) < 1e-3f * hEnd + 1e-6f && fabsf(g_viewU - g_destU) < 1e-4f && fabsf(g_viewV - g_destV) < 1e-4f;
        }
        {                                                                    /* already closer than that: it never backs out */
            DWORD at3 = g_settleTo + 100;
            g_settleH = g_destH / BLUR_PUSH * 0.5f; g_settleU = g_destU; g_settleV = g_destV;
            g_settleFrom = at3; g_settleTo = at3 + 1000;
            ViewStep(1.0f, at3 + 500, 1000, 500);
            ok &= g_viewH <= g_settleH * 1.0001f;
        }
        PutFloat(HOLD_BAR_VA, 0.0f); g_holdOn = 0; g_lnN[1] = 0; g_holdMs = HOLD_MS;
        /* the screen ends: the next begins on the game's thread again */
        DriftScreenGone();
        ok &= !ViewStep(0.2f, t + 7, 1000, 500) && g_stepLock.held == 0;
        GameStep(0.0f, t + 3000);
        ok &= g_viewSteps == 0 && !g_dispNew && g_stepLock.held == 0;
        g_viewOn = 0; g_viewSteps = 0; g_mapShown = 0; g_driftOn = 0; g_screenWatch = 0; g_zoomScreens = 0; g_destEndOk = 0;
        Check(ok, "the view: a screen begins on the game's first frame, never on the view's; then the view's frames move the bar and the zoom, every one of them through a 0.6 s stall of the game's, the game's frames only noting their facts; the lock always let go");
    }
    /* the character list: only the game's thread reads it (the game frees it during a login's load); the view's thread
     * has the place the game's last frame read there */
    {
        Fresh("view-charlist");
        g_mapShown = 1;
        { const char *mt = "map 0 3f000000 b7a7c5ac 3f000000 b7a7c5ac\nend 3000\n"; MapParse(mt, (unsigned)strlen(mt)); }
        Poke(BAR_MAP_VA, 0);
        Poke(MAP_DEST_MAP_VA, 0); PutFloat(MAP_DEST_X_VA, 0); PutFloat(MAP_DEST_Y_VA, 0);   /* nothing written yet */
        Poke(CHAR_COUNT_VA, 2); Poke(CHAR_LIST_VA, 0x8B0000); Poke(CHAR_PICKED_VA, 1);
        unsigned e = 0x8B0000 + 0x120;
        Poke(e + 0x38, 0); PutFloat(e + 0x44, -5000); PutFloat(e + 0x48, 10000);
        g_classSure = 1; g_driftOn = 1; g_screenWatch = 1; g_holdMs = 0;
        InitializeCriticalSection(&g_stepLock);
        g_viewOn = 1;
        DWORD t = 3000000;
        GameStep(0, t);                                                  /* a login's first frame: the list's place */
        int ok = g_destOk && fabsf(g_destU - 0.3f) < 1e-4f && fabsf(g_destV - 0.6f) < 1e-4f && g_charKept[0] == 1;
        ok &= ViewStep(0.1f, t + 7, 100, 100) && fabsf(g_destU - 0.3f) < 1e-4f;
        PutFloat(e + 0x44, -2500); PutFloat(e + 0x48, 5000);             /* the list changes under it */
        ViewStep(0.1f, t + 14, 100, 100);
        ok &= fabsf(g_destU - 0.3f) < 1e-4f;                             /* the view's thread never read it */
        GameStep(0.1f, t + 20);                                          /* the game's frame reads it, and keeps it */
        ViewStep(0.1f, t + 27, 100, 100);
        ok &= fabsf(g_destU - 0.4f) < 1e-4f && fabsf(g_destV - 0.55f) < 1e-4f;
        Poke(CHAR_COUNT_VA, 0);                                          /* the list gone */
        GameStep(0.1f, t + 40);
        ViewStep(0.1f, t + 47, 100, 100);
        ok &= g_charKept[0] == 0 && g_destOk && fabsf(g_destU - 0.4f) < 1e-4f && g_stepLock.held == 0;
        g_viewOn = 0; g_viewSteps = 0; g_mapShown = 0; g_driftOn = 0; g_screenWatch = 0; g_classSure = 0; g_zoomScreens = 0;
        Check(ok, "the view: the character list read on the game's thread only, its place kept for the view's; the view takes what the game's last frame read, and keeps the arrival once the list is gone");
    }
    /* WoW.exe's .data, found in its own headers: a read inside it needs no VirtualQuery (under Wine that waits on the
     * game's memory lock); anywhere else every read is still checked; a header that is not a PE image's finds nothing */
    {
        Fresh("view-data");
        unsigned char *h = shim_module;
        unsigned pe = 0x100, vs = 0x3FE000, va = 0x1000, v = 0;
        h[0] = 'M'; h[1] = 'Z'; memcpy(h + 0x3C, &pe, 4);
        memcpy(h + pe, "PE\0\0", 4); h[pe + 6] = 2; h[pe + 20] = 0xE0;   /* two sections, after 0xE0 of optional header */
        unsigned char *sec = h + pe + 24 + 0xE0;
        memcpy(sec, ".text\0\0\0", 8); memcpy(sec + 8, &vs, 4); memcpy(sec + 12, &va, 4);
        vs = 0x4D4002; va = 0x427000;                                    /* as in his WoW.exe, from 0x827000 */
        memcpy(sec + 40, ".data\0\0\0", 8); memcpy(sec + 48, &vs, 4); memcpy(sec + 52, &va, 4);
        ClientDataFind();
        Poke(BAR_MAP_VA, 7);
        int before = shim_vq;
        int ok = g_dataLo == (ULONG_PTR)(shim_module + 0x427000) && g_dataHi == g_dataLo + 0x4D4002 &&
                 ClientWord(BAR_MAP_VA, &v) && v == 7 && ClientWord(MAP_DEST_X_VA, &v) && shim_vq == before;
        ok &= ClientWord(0x401000, &v) && shim_vq == before + 1;            /* its code: checked */
        ok &= ClientWord(0x827000 + 0x4D4000, &v) && shim_vq == before + 2;       /* across its end: checked */
        h[pe + 1] = 'F'; ClientDataFind();
        ok &= !g_dataLo && !g_dataHi;
        memset(h, 0, 0x400);
        Check(ok, "WoW.exe's .data found in its own headers: reads inside it skip VirtualQuery, every other read is checked, and a header that is not a PE image's finds nothing");
    }
    /* slice 2: the overlay's quads into the view's list (QuadPut's strips, screen fractions up from the bottom left) as
     * pixels down from the top left, each quad round its corners, across a strip's end too; the list never overruns */
    {
        static float pos[130 * 6 * 3], uv[130 * 6 * 2];
        static unsigned col[130 * 6];
        for (int q = 0; q < 122; q++) {
            float x0 = q * 0.001f, x[4] = { x0, x0 + 0.5f, x0, x0 + 0.5f }, y[4] = { 0.1f, 0.1f, 0.3f, 0.3f };
            float tu[4] = { 0, 1, 0, 1 }, tv[4] = { 1, 1, 0, 0 };
            QuadPut(pos, uv, col, q, x, y, tu, tv, 0x80FF0000u | (unsigned)q);
        }
        ViewListStart();
        ViewStrips(VT_LABELS, pos, uv, col, 122, 1000, 500);
        int ok = g_vbN == 1 && g_vb[0].tex == VT_LABELS && g_vb[0].count == 122 * 4;
        for (int q = 0; q < 122; q++) {
            const ViewVert *w = g_vv + 4 * q;
            float x0 = q * 1.0f;                                         /* q * 0.001 of 1000 pixels */
            ok &= fabsf(w[0].x - x0) < 1e-3f && fabsf(w[0].y - 450) < 1e-3f && fabsf(w[1].x - x0 - 500) < 1e-3f &&
                  fabsf(w[2].y - 350) < 1e-3f && fabsf(w[2].x - x0 - 500) < 1e-3f && fabsf(w[3].x - x0) < 1e-3f &&
                  w[0].u == 0 && w[0].v == 1 && w[2].u == 1 && w[2].v == 0 && w[0].c[0] == 0xFF && w[0].c[2] == (unsigned char)q &&
                  w[0].c[3] == 0x80;
        }
        ViewListStart();
        for (int i = 0; i < VIEW_BATCHES + 8; i++) ViewRect(i & 1 ? VT_SEA : VT_MARKS, 0, 0, 1, 1, 0, 0, 1, 1, 0);
        ok &= g_vbN == VIEW_BATCHES && g_vvN == VIEW_BATCHES * 4;            /* (and nothing after the first left out) */
        Check(ok, "slice 2: the overlay's strips into the view's list as pixels from the top left, round each quad's corners, across a strip's end; colours as RGBA; no more batches than it holds");
    }
    /* slice 2: the layers in the game's own order, each on its sheet, a layer without one left out */
    {
        Fresh("view-overlay");
        PackLive(1 | 2 | 4 | 8 | 32 | 64, 0);
        Start();
        int ok = g_ovOn && g_seaOn && g_logoOn && g_marksOn;
        g_mapShown = 1; g_scrW = g_surfW = 1000; g_scrH = g_surfH = 500;
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
        g_viewTex[VT_MAP] = g_viewTex[VT_MARKS] = g_viewTex[VT_SEA] = g_viewTex[VT_LOGO] = 1;
        ViewBuild(1000, 500, 0.5f, 100000);
        int order[VIEW_BATCHES], n = 0, logo = -1;
        for (int i = 0; i < g_vbN; i++) { order[n++] = g_vb[i].tex; if (g_vb[i].tex == VT_LOGO) logo = i; }
        /* the map, the overlay's layers, then the blade (its channel, glow, core, edge and steel) and the logo on it, last:
         * nothing across the top and no black band along the bottom (his call, 6 Oct 2026: the map gets the screen) */
        ok &= n >= 7 && order[0] == VT_MAP && order[1] == VT_MARKS && logo == n - 1;
        for (int i = n - 5; i <= n - 2; i++) ok &= order[i] == -1 && g_vb[i].add == ((n - 1 - i) % 2 == 1);   /* channel, glow, */
        for (int i = 2; i < n - 5; i++) ok &= order[i] == VT_SEA || order[i] == VT_MARKS;   /* core, edge; the overlay */
        for (int i = 0; i < g_vvN; i++) ok &= !(g_vv[i].y == 0 && g_vv[i].x == 1000 && i >= 4 && i < g_vb[n - 5].first);
        if (logo >= 0) {
            const ViewVert *w = g_vv + g_vb[logo].first;
            ok &= g_vb[logo].count == 4 && fabsf((w[0].x + w[2].x) / 2 - 500) < 1e-3f && fabsf(w[2].y - (500 - SABER_LOGO_FOOT * 500)) < 1e-3f &&
                  w[0].u == g_logo.tu0 && w[0].v == g_logo.tv0 && w[2].u == g_logo.tu1 && w[2].v == g_logo.tv1;
        } else ok = 0;
        g_viewTex[VT_LOGO] = 0;
        ViewBuild(1000, 500, 0.5f, 100020);
        for (int i = 0; i < g_vbN; i++) ok &= g_vb[i].tex != VT_LOGO;
        g_mapShown = 0; g_ovOn = 0;
        for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
        Check(ok, "slice 2, 6 Oct: the view's layers in the game's order (the map, the Maelstrom, the sea's glints and marks), then the blade and the RavenCraft logo on it at the centre; nothing across the top, no black band; a layer whose sheet OpenGL lacks is left out");
    }
    /* the view's own clock (his yes, 5 Oct: "hide the jump"): steady frames keep the wall clock; after a pause behind the
     * game's burst on the card it goes on from where it stopped, then catches up a quarter faster, never past the wall */
    {
        DWORD real = 5000000, v = 0, prev;
        ViewClockStart(real);
        int ok = 1;
        for (int i = 0; i < 100; i++) { real += 7; v = ViewClock(real); ok &= v == real; }      /* 147 a second */
        prev = v;
        real += 114; v = ViewClock(real);                                /* a pause of 114 ms */
        ok &= v - prev == VIEW_CLOCK_MIN;                                /* goes on, it does not leap the 114 */
        int frames = 0, fastest = 0, mono = 1;
        while (v != real && frames < 200) {
            prev = v; real += 7; v = ViewClock(real); frames++;
            if ((int)(v - prev) > fastest) fastest = (int)(v - prev);
            mono &= v >= prev && v <= real;
        }
        ok &= mono && v == real && frames >= 30 && frames <= 60 && fastest <= 9;   /* about a third of a second, at most 9 ms a 7 */
        for (int i = 0; i < 100; i++) { real += 17; v = ViewClock(real); }       /* a 60 Hz screen: the usual frame moves */
        ok &= v == real;
        prev = v; real += 30; v = ViewClock(real); ok &= v == real;      /* a missed refresh at 60 Hz is no pause */
        ViewClockStart(9000000); ok &= ViewClock(9000025) == 9000020;   /* a first step is never over VIEW_CLOCK_MIN */
        Check(ok, "the view's own clock: steady frames keep the wall clock; a 114 ms pause goes on from where it stopped and catches up in about a third of a second, a quarter faster at most, never past the wall; a 60 Hz screen's usual frame is learned");
    }
    /* the fade: once the game has shown the world (it showed the game's last loading frame for 0.8 s, then the world at
     * once); without the present's hook, at once; never more than VIEW_WORLD_WAIT_MS */
    {
        g_blurOn = 0;
        LONG w = g_worldFrames;
        int ok = ViewFadeDue(1000, 1000, w);
        g_blurOn = 1;
        ok &= !ViewFadeDue(1500, 1000, w) && !ViewFadeDue(1000 + VIEW_WORLD_WAIT_MS - 1, 1000, w);
        g_worldFrames = w + 1; ok &= ViewFadeDue(1001, 1000, w);
        g_worldFrames = w; ok &= ViewFadeDue(1000 + VIEW_WORLD_WAIT_MS, 1000, w);
        g_blurOn = 0;
        Check(ok, "the view's fade begins once the game shows the world, at once without the present's hook, and never later than VIEW_WORLD_WAIT_MS");
    }
}

/* The ships and zeppelins (7 Oct 2026): their table read, their time-lapsed clock with its dock waits, each where its
 * clock has it, turned to its way, sized to the zoom, in its own accent, with a wake behind a ship under way. */
static unsigned ShipHex(float f) { union { float f; unsigned u; } c; c.f = f; return c.u; }
static void TestShips(void)
{
    static char t[2048];
    int n = snprintf(t, sizeof t,
        "# the living map\nspeed 100\ndock 500\nt 0 c82828 A ship\n"
        "p %08x %08x 2000\np %08x %08x 0\n"                                  /* docks at (0.2, 0.5) for 2 s of the game's */
        "t 1 8a4ac8 A zeppelin\np %08x %08x 0\np %08x %08x 0\np %08x %08x 0\n"
        "t 2 ffffff not a kind\np 0 0 0\np 3e800000 0 0\nt 0 123456 no way\np %08x %08x 0\n"   /* a bad kind, a one-point way: dropped */
        "t 0 0000ff in the wrong order\np %08x %08x 0\np zz 1 0\np %08x %08x 0\n",
        ShipHex(0.2f), ShipHex(0.5f), ShipHex(0.6f), ShipHex(0.5f),
        ShipHex(0.3f), ShipHex(0.2f), ShipHex(0.3f), ShipHex(0.4f), ShipHex(0.4f), ShipHex(0.4f),
        ShipHex(0.5f), ShipHex(0.5f), ShipHex(0.1f), ShipHex(0.1f), ShipHex(0.1f), ShipHex(0.2f));
    ShipsParse(t, (unsigned)n);
    int ok = g_ships == 3 && g_ship[0].kind == 0 && g_ship[0].rgb == 0xC82828 && g_ship[0].n == 2 && g_ship[1].kind == 1 &&
             g_ship[1].rgb == 0x8A4AC8 && g_ship[1].n == 3 && g_ship[2].n == 2 && g_ship[2].rgb == 0x0000FF &&
             Near(g_shipSpeed, 0.1, 1e-6) && Near(g_shipDock, 0.5, 1e-6) && Near(g_shipWait[g_ship[0].first], 2.0, 1e-6);
    Check(ok, "the transports' table read: its speed and dock lines, each transport's kind and accent, its points and dock waits; a bad kind, a way of one point and a bad point left out");

    /* the clock: the ship (speed 0.1 heights a second, the picture's u counted twice) waits 1 s at its dock (2 s of the
     * game's at 500), sails 0.8 heights in 8 s, and back in 8 s: a cycle of 17 s */
    ShipsTime(2.0f);
    int moving = -1;
    float w = ShipWay(0, 0.5f, &moving), u, v;
    ok = Near(g_ship[0].len, 1.6, 1e-5) && Near(g_ship[0].cycle, 17, 1e-4) && Near(w, 0, 1e-6) && moving == 0;
    w = ShipWay(0, 5.0f, &moving);                                         /* 4 s under way: half way out */
    ShipAt(0, w, &u, &v);
    ok &= moving == 1 && Near(w, 0.4, 1e-5) && Near(u, 0.4, 1e-5) && Near(v, 0.5, 1e-5);
    w = ShipWay(0, 13.0f, &moving);                                        /* 4 s into the way back */
    ShipAt(0, w, &u, &v);
    ok &= moving == 1 && Near(u, 0.4, 1e-5);
    ShipAt(0, w + 1.6f, &u, &v);                                           /* the way goes round */
    ok &= Near(u, 0.4, 1e-4);
    ok &= Near(g_ship[1].cycle * 0.1 * ZEP_FASTER, g_ship[1].len, 1e-4);   /* a zeppelin: quicker, never docking */
    Check(ok, "the transports' clock: a dock's wait its share of the game's, then under way at the speed, out and back round its loop, a zeppelin quicker");

    /* drawn: the whole world in view, the ship 4 s under way heading east, the zeppelin over it */
    static const float whole[8] = { 0, 1, 1, 1, 0, 0, 1, 0 };
    memcpy(g_bgUV, whole, sizeof whole);
    g_shipsOn = 1; g_viewTex[VT_SHIPS] = 1; g_viewTex[VT_SPARK] = 1;
    g_ship[0].phase = 4.0f; g_ship[1].phase = 0; g_ship[2].phase = 0;
    ViewListStart();
    ShipsBuild(1000, 500, 1000, 0);                                        /* (now 1 s: the ship's clock at 5 s) */
    ok = g_vbN == 3 && g_vb[0].tex == -1 && g_vb[0].add && g_vb[0].count == 4 * 2 * SHIP_WAKE_BANDS &&   /* wakes, */
         g_vb[1].tex == VT_SPARK && g_vb[1].add && g_vb[1].count == 4 * 2 * SHIP_WAKE_SPARKS &&   /* foam, */
         g_vb[2].tex == VT_SHIPS && !g_vb[2].add && g_vb[2].count == 4 * 2 * 2;   /* two ships under way: frame + accent each */
    const ViewVert *q = g_vv + g_vb[2].first, *acc = q + 4;
    float hs = SHIP_SIZE * 500;
    ok &= Near((q[0].x + q[1].x) / 2, 400, 1e-2) && Near((q[0].y + q[2].y) / 2, 250, 1e-2) && Near(q[1].x - q[0].x, 2 * hs, 1e-3);
    ok &= Near(q[0].u, 0.5 / SHIP_SHEET_W, 1e-6) && Near(q[1].u, (SHIP_FRAME - 0.5) / SHIP_SHEET_W, 1e-6) && Near(q[0].v, 0.5 / SHIP_SHEET_H, 1e-6);   /* east: frame 0 */
    ok &= acc[0].c[0] >= 0xE0 && acc[0].c[1] >= 0xC8 && acc[0].c[2] >= 0xB8 && acc[0].c[0] > acc[0].c[1] + 12 &&
          acc[0].c[0] > acc[0].c[2] + 12 && acc[0].c[3] == 255 && q[0].c[0] == 255 &&
          Near(acc[0].v, (SHIP_FRAME + 0.5) / SHIP_SHEET_H, 1e-6);                               /* its sails white, a hint of red, from the accent row */
    const ViewVert *wk = g_vv + g_vb[0].first, *fm = g_vv + g_vb[1].first + 4 * (2 * SHIP_CAPS + SHIP_STERN_FOAM);
    ok &= wk[0].x > 400 && wk[4 * (2 * SHIP_WAKE_SEGS - 1) + 1].x < 400 - 2 * hs && wk[0].c[3] > wk[4 * (2 * SHIP_WAKE_SEGS - 1) + 1].c[3] &&
          (fm[0].x + fm[2].x) / 2 > 400 + 0.4f * hs && fm[0].c[3] > 0;   /* foam at its bow; the wake behind it, fading */
    /* on the way back the ship faces west (frame 8) */
    ViewListStart();
    ShipsBuild(1000, 500, 9000, 0);                                        /* (its clock at 13 s) */
    q = g_vv + g_vb[g_vbN - 1].first;
    ok &= Near(q[0].u, (8 * SHIP_FRAME + 0.5) / SHIP_SHEET_W, 1e-6);
    Check(ok, "the ships drawn: each where its clock has it, in the frame of its heading (east out, west back), its sails over it white with a hint of its colour, foam at its bow and a fading wake behind it");

    /* docked: no wake; the zeppelins drawn in their own pass from their own rows, heading south down their way */
    g_ship[0].phase = 0; g_ship[2].phase = 0;
    ViewListStart();
    ShipsBuild(1000, 500, 500, 0);                                         /* the ship's clock at 0.5 s: at its dock */
    int litDocked = 0, litOther = 0;                                       /* (its quads all there, dark) */
    for (int i = 0; i < g_vb[0].count; i++) *(i < 4 * SHIP_WAKE_BANDS ? &litDocked : &litOther) += g_vv[g_vb[0].first + i].c[3] > 0;
    for (int i = 0; i < g_vb[1].count; i++) *(i < 4 * SHIP_WAKE_SPARKS ? &litDocked : &litOther) += g_vv[g_vb[1].first + i].c[3] > 0;
    ok = g_vbN == 3 && g_vb[0].count == 4 * 2 * SHIP_WAKE_BANDS && litDocked == 0 && litOther > 0;   /* only the other ship's */
    ViewListStart();
    ShipsBuild(1000, 500, 100, 1);
    q = g_vv + g_vb[0].first;
    ok &= g_vbN == 2 && g_vb[0].tex == VT_SHIPS && g_vb[0].count == 8 && Near(q[0].v, (2 * SHIP_FRAME + 0.5) / SHIP_SHEET_H, 1e-6) &&
          g_vb[1].tex == -1 && g_vb[1].add && g_vb[1].count == 4 * ZEP_STREAK_Q &&   /* the wind past it, over it */
          Near(q[4].v, (3 * SHIP_FRAME + 0.5) / SHIP_SHEET_H, 1e-6) && Near(q[0].u, (4 * SHIP_FRAME + 0.5) / SHIP_SHEET_W, 1e-6) &&
          Near(q[1].x - q[0].x, 2 * ZEP_SIZE * 500, 1e-3) && q[4].c[0] == 0x8A && q[4].c[1] == 0x4A && q[4].c[2] == 0xC8;   /* (its balloon's own) */
    Check(ok, "a ship at its dock leaves no wake; the zeppelins drawn on their own, from their own rows, turned down their way, in their colour");

    /* zoomed in: bigger (as the square root of the zoom, capped), and a transport off screen not drawn */
    static const float near[8] = { 0.35f, 0.6f, 0.45f, 0.6f, 0.35f, 0.4f, 0.45f, 0.4f };   /* a fifth of the height */
    memcpy(g_bgUV, near, sizeof near);
    g_ship[0].phase = 4.0f;                                                /* (under way again, at u 0.4: in view) */
    ViewListStart();
    ShipsBuild(1000, 500, 1000, 0);
    int quads = 0;
    for (int b = 0; b < g_vbN; b++) if (g_vb[b].tex == VT_SHIPS) quads += g_vb[b].count / 4;
    q = g_vv + g_vb[g_vbN - 1].first;
    float grow = (float)sqrt(1 / 0.2);
    ok = g_vb[g_vbN - 1].tex == VT_SHIPS;
    ok &= quads == 2 && Near(q[1].x - q[0].x, 2 * SHIP_SIZE * 500 * (grow < SHIP_GROW ? grow : SHIP_GROW), 1e-2);
    static const float nearer[8] = { 0.38f, 0.55f, 0.42f, 0.55f, 0.38f, 0.45f, 0.42f, 0.45f };   /* a tenth: past the cap */
    memcpy(g_bgUV, nearer, sizeof nearer);
    ViewListStart();
    ShipsBuild(1000, 500, 1000, 0);
    q = g_vv + g_vb[g_vbN - 1].first;
    ok &= sqrt(1 / 0.1) > SHIP_GROW && Near(q[1].x - q[0].x, 2 * SHIP_SIZE * 500 * SHIP_GROW, 1e-2);
    memcpy(g_bgUV, whole, sizeof whole);
    g_viewTex[VT_SHIPS] = 0;
    ViewListStart();
    ViewOverlayBuild(1000, 500, 1000);
    for (int b = 0; b < g_vbN; b++) ok &= g_vb[b].tex != VT_SHIPS;
    Check(ok, "nearer in the transports grow (as the square root of the zoom, capped); one off screen is not drawn; without their sheet none are");
    g_shipsOn = 0; g_ships = 0; g_viewTex[VT_SPARK] = 0;
}

/* The wakes and the wind (his ask, 7 Oct: "a little white foam at the front where they part the waves", "a little
 * widening path in the water as they pass"; for a zeppelin "little effects to make it look like the wind is streaking
 * past them"): one transport, sailing a way of 300 pixels across a 1000 x 500 window heading f (the picture's u counted
 * twice, so 500 pixels to a height), waiting wait seconds at each end. */
static void WakeOne(int kind, int f, float wait)
{
    float c = (float)cos(f * 22.5 * M_PI / 180), sn = (float)sin(f * 22.5 * M_PI / 180), du = 0.3f * c, dv = 0.6f * sn;
    g_ships = 1; g_shipPts = 2;
    g_ship[0].kind = kind; g_ship[0].rgb = 0xFFFFFF; g_ship[0].first = 0; g_ship[0].n = 2; g_ship[0].phase = 0;
    g_shipU[0] = 0.5f - du / 2; g_shipV[0] = 0.5f - dv / 2; g_shipU[1] = 0.5f + du / 2; g_shipV[1] = 0.5f + dv / 2;
    g_shipWait[0] = g_shipWait[1] = wait;
    g_shipSpeed = 0.1f; g_shipDock = 1;
    ShipsTime(2.0f);
    g_ship[0].phase = 0;                                   /* (its clock the view's) */
}
/* Quad i of batch b: the middle of its corners, and its alpha at corner k. */
static float WakeMidX(int b, int i) { const ViewVert *v = g_vv + g_vb[b].first + 4 * i; return (v[0].x + v[2].x) / 2; }
static float WakeMidY(int b, int i) { const ViewVert *v = g_vv + g_vb[b].first + 4 * i; return (v[0].y + v[2].y) / 2; }
/* Point j of the n of a soft band (ShipBand's: two quads a piece) whose first quad is quad q0 of batch b, and its reach
 * to the side there. */
static const ViewVert *WakePt(int b, int q0, int n, int j)
{
    const ViewVert *v = g_vv + g_vb[b].first + 4 * q0;
    return j < n - 1 ? v + 8 * j : v + 8 * (n - 2) + 1;
}
static float WakeReach(int b, int q0, int n, int j)
{
    const ViewVert *v = g_vv + g_vb[b].first + 4 * q0 + 8 * (j < n - 1 ? j : n - 2);
    int a = j < n - 1 ? 0 : 1, c = j < n - 1 ? 3 : 2;
    return sqrtf((v[c].x - v[a].x) * (v[c].x - v[a].x) + (v[c].y - v[a].y) * (v[c].y - v[a].y));
}
/* The most the way of a soft band (n points from quad q0 of batch b) turns between neighbouring pieces, in degrees; and
 * how far it turns in all, into *total. */
static float WakeKink(int b, int q0, int n, float *total)
{
    float most = 0, prev = 0, sum = 0;
    int have = 0;
    for (int j = 0; j + 1 < n; j++) {
        const ViewVert *p = WakePt(b, q0, n, j), *q = WakePt(b, q0, n, j + 1);
        float dx = q->x - p->x, dy = q->y - p->y, a = atan2f(dy, dx);
        if (dx * dx + dy * dy < 1e-4f) continue;           /* (a piece of no length: where it ends) */
        if (have) {
            float d = a - prev;
            while (d > (float)M_PI) d -= 2 * (float)M_PI;
            while (d < -(float)M_PI) d += 2 * (float)M_PI;
            sum += d;
            d = fabsf(d) * 180 / (float)M_PI;
            most = d > most ? d : most;
        }
        prev = a; have = 1;
    }
    *total = fabsf(sum) * 180 / (float)M_PI;
    return most;
}
static void TestWakes(void)
{
    memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
    g_shipsOn = 1; g_viewTex[VT_SHIPS] = 1; g_viewTex[VT_SPARK] = 1;
    float hs = SHIP_SIZE * 500, len = SHIP_WAKE_LEN * hs;
    int okFoam = 1, okArms = 1, okDock = 1, okLeave = 1, okStern = 1, fm0 = 2 * SHIP_CAPS + SHIP_STERN_FOAM, churned = 0;
    int lane = 4 * SHIP_WAKE_SEGS + 2 * SHIP_STERN_SEGS, ln = SHIP_STERN_SEGS + 1;   /* (the stern's middle lane) */
    for (int f = 0; f < 16; f++) {
        float hx = kShipHead[f][0], hy = kShipHead[f][1], wx = 500, wy = 250 + SHIP_WATER * hs;   /* its waterline's middle */
        WakeOne(0, f, 0);
        ViewListStart();
        ShipsBuild(1000, 500, 3000, 0);                    /* half way out, 150 pixels from its turn: its whole wake made */
        if (g_vbN != 3 || g_vb[0].tex != -1 || g_vb[1].tex != VT_SPARK || g_vb[2].tex != VT_SHIPS ||
            g_vb[0].count != 4 * SHIP_WAKE_BANDS || g_vb[1].count != 4 * SHIP_WAKE_SPARKS ||
            !Near(g_vv[g_vb[2].first].u, (f * SHIP_FRAME + 0.5) / SHIP_SHEET_W, 1e-6)) { okFoam = okArms = okStern = 0; continue; }
        /* the foam at its bow: ahead of its waterline along the frame's heading, at the prow and either side of it */
        float sides[SHIP_FOAM];
        for (int i = 0; i < SHIP_FOAM; i++) {
            float dx = WakeMidX(1, fm0 + i) - wx, dy = WakeMidY(1, fm0 + i) - wy, ahead = dx * hx + dy * hy;
            sides[i] = dx * -hy + dy * hx;               /* (the hull is shorter up and down the screen: a little aside) */
            okFoam &= ahead > 0.3f * hs && fabsf(sides[i]) < 0.2f * hs && g_vv[g_vb[1].first + 4 * (fm0 + i)].c[3] > 0;
        }
        okFoam &= (sides[1] - sides[0]) * (sides[2] - sides[0]) < 0 && fabsf(sides[1] - sides[0]) > 0.05f * hs &&
                  fabsf(sides[2] - sides[0]) > 0.05f * hs;   /* the prow's, and one either side of it */
        /* the arms: from its bow, along the way it came, parting at the Kelvin angle, wider and fainter as they go */
        const ViewVert *L = g_vv + g_vb[0].first, *R = L + 4 * 2 * SHIP_WAKE_SEGS;
        float s0 = 0, sPrev = -1, wPrev = -1, mx0 = 0, my0 = 0, mxN = 0, myN = 0, sN = 0, s1 = 0, d1 = 0, aFirst = L[0].c[3], aLate = 0;
        for (int j = 0; j <= SHIP_WAKE_SEGS; j++) {
            const ViewVert *l = j < SHIP_WAKE_SEGS ? L + 8 * j : L + 8 * (j - 1) + 1, *r = j < SHIP_WAKE_SEGS ? R + 8 * j : R + 8 * (j - 1) + 1;
            float sx = l->x - r->x, sy = l->y - r->y, sep = sqrtf(sx * sx + sy * sy), mx = (l->x + r->x) / 2, my = (l->y + r->y) / 2;
            okArms &= sep > sPrev;                          /* parting */
            sPrev = sep;
            if (j < SHIP_WAKE_SEGS) {                       /* widening: its reach at each point */
                float w = sqrtf((l[3].x - l[0].x) * (l[3].x - l[0].x) + (l[3].y - l[0].y) * (l[3].y - l[0].y));
                okArms &= w > wPrev;
                wPrev = w;
            }
            if (j == 0) { s0 = sep; mx0 = mx; my0 = my; }
            if (j == 1) { s1 = sep; d1 = sqrtf((mx - mx0) * (mx - mx0) + (my - my0) * (my - my0)); }
            if (j == SHIP_WAKE_SEGS) { sN = sep; mxN = mx; myN = my; }
            if (2 * j >= SHIP_WAKE_SEGS) aLate = l->c[3] > aLate ? l->c[3] : aLate;
        }
        float dN = sqrtf((mxN - mx0) * (mxN - mx0) + (myN - my0) * (myN - my0));
        okArms &= Near(dN, len, 0.5) && Near((sN - s1) / (dN - d1), 2 * tan(19.47 * M_PI / 180), 0.02) &&   /* the Kelvin angle */
                  Near(s0, 2 * SHIP_PART * hs, 0.05) && (mxN - wx) * hx + (myN - wy) * hy < -2 * hs &&   /* behind it */
                  aFirst > 2 * aLate && aFirst > 60 && L[8 * (SHIP_WAKE_SEGS - 1) + 1].c[3] == 0;   /* fading to nothing */
        /* the wake off its stern: from its transom, the hull's width there, widening and fading over several hull lengths
         * behind it; foam in it; waves off the transom's corners parting wider than it */
        {
            const ViewVert *p0 = WakePt(0, lane, ln, 0), *pN = WakePt(0, lane, ln, ln - 1);
            float tx = p0->x - wx, ty = p0->y - wy, back = tx * hx + ty * hy, side = tx * -hy + ty * hx;
            float dist = (pN->x - p0->x) * -hx + (pN->y - p0->y) * -hy, wPrev = -1, aEarly = 0, aLate2 = 0, aPrev = 1e9f;
            okStern &= back < -0.45f * hs && fabsf(side) < 0.15f * hs && Near(WakeReach(0, lane, ln, 0), SHIP_BEAM * hs, 0.05) &&
                       Near(dist, SHIP_STERN_LEN * hs, 1.0) && pN->c[3] == 0 &&
                       SHIP_STERN_LEN >= 3 * (SHIP_BOW_X + SHIP_STERN_X);   /* (several hull lengths) */
            int rises = 0;
            for (int j = 0; j < ln; j++) {
                float w = WakeReach(0, lane, ln, j), a = WakePt(0, lane, ln, j)->c[3];
                okStern &= w > wPrev;
                wPrev = w;
                rises += a > aPrev;                         /* (churned: its light comes and goes as it fades) */
                aPrev = a;
                if (3 * j <= ln) aEarly = a > aEarly ? a : aEarly;
                if (2 * j >= ln) aLate2 = a > aLate2 ? a : aLate2;
            }
            okStern &= aEarly > 2 * aLate2;
            churned += rises > 0;
            float foamFar = 0;
            int foamLit = 0;
            for (int i = 0; i < SHIP_STERN_FOAM; i++) {
                float fx = WakeMidX(1, 2 * SHIP_CAPS + i) - p0->x, fy = WakeMidY(1, 2 * SHIP_CAPS + i) - p0->y;
                float dc = fx * -hx + fy * -hy, lat = fx * -hy + fy * hx;
                okStern &= dc > -0.1f * hs && dc < SHIP_STERN_LEN * hs + 0.5f &&
                           fabsf(lat) <= 0.8f * (SHIP_BEAM + SHIP_STERN_W * dc / (SHIP_STERN_LEN * hs)) * hs + 0.5f;
                if (g_vv[g_vb[1].first + 4 * (2 * SHIP_CAPS + i)].c[3] > 0) { foamLit++; foamFar = dc > foamFar ? dc : foamFar; }
            }
            okStern &= foamLit >= SHIP_STERN_FOAM / 2 && foamFar > 0.5f * SHIP_STERN_LEN * hs;   /* (all along it) */
            int q = 4 * SHIP_WAKE_SEGS + 6 * SHIP_STERN_SEGS, m = SHIP_QUARTER_SEGS + 1;
            const ViewVert *a0 = WakePt(0, q, m, 0), *b0 = WakePt(0, q + 2 * SHIP_QUARTER_SEGS, m, 0);
            const ViewVert *aN = WakePt(0, q, m, m - 1), *bN = WakePt(0, q + 2 * SHIP_QUARTER_SEGS, m, m - 1);
            float sep0 = sqrtf((a0->x - b0->x) * (a0->x - b0->x) + (a0->y - b0->y) * (a0->y - b0->y));
            float sepN = sqrtf((aN->x - bN->x) * (aN->x - bN->x) + (aN->y - bN->y) * (aN->y - bN->y));
            okStern &= Near(sep0, 2 * SHIP_BEAM * hs, 0.05) && Near(sepN, 2 * (SHIP_BEAM + SHIP_QUARTER * SHIP_QUARTER_LEN) * hs, 0.1) &&
                       a0->c[3] > 0 && aN->c[3] == 0;
        }
        /* at its dock: all of it there, and dark */
        WakeOne(0, f, 2.0f);                               /* (2 s at each end: at the first until 2 s) */
        ViewListStart();
        ShipsBuild(1000, 500, 1000, 0);
        okDock &= g_vbN == 3 && g_vb[0].count == 4 * SHIP_WAKE_BANDS && g_vb[1].count == 4 * SHIP_WAKE_SPARKS;
        for (int b = 0; b < 2; b++) for (int i = 0; i < g_vb[b].count; i++) okDock &= g_vv[g_vb[b].first + i].c[3] == 0;
        /* a fifth of a second out of it: its wake only as far back as it has come, its foam as it gathers way */
        ViewListStart();
        ShipsBuild(1000, 500, 2200, 0);
        float far = 0, foamA = g_vv[g_vb[1].first + 4 * fm0].c[3];
        for (int j = 0; j < 2 * SHIP_WAKE_SEGS; j++) {
            const ViewVert *v = g_vv + g_vb[0].first + 4 * j, *b = g_vv + g_vb[0].first;   /* (from its bow) */
            float d = (v[0].x - b[0].x) * -hx + (v[0].y - b[0].y) * -hy;
            if (v[0].c[3] > 0 && d > far) far = d;
        }
        const ViewVert *L0 = g_vv + g_vb[0].first, *e = L0 + 8 * (SHIP_WAKE_SEGS - 1) + 1;   /* (its arm's far end) */
        float reach = sqrtf((e->x - L0[0].x) * (e->x - L0[0].x) + (e->y - L0[0].y) * (e->y - L0[0].y));   /* (never past it) */
        const ViewVert *st0 = WakePt(0, lane, ln, 0), *stN = WakePt(0, lane, ln, ln - 1);   /* (and its stern's) */
        float sreach = sqrtf((stN->x - st0->x) * (stN->x - st0->x) + (stN->y - st0->y) * (stN->y - st0->y));
        okLeave &= far > 0 && far < 0.02f * 500 + 0.5f * hs && foamA > 0 && foamA < 0.5f * 255 && reach < 0.02f * 500 * 1.1f + 1 &&
                   e->c[3] == 0 && sreach < 0.02f * 500 + 1 && stN->c[3] == 0 && st0->c[3] > 0 &&   /* (and soft where it ends) */
                   reach > 0.8f * 0.02f * 500 && sreach > 0.8f * 0.02f * 500;   /* (but all of that: never squeezed) */
    }
    Check(okFoam, "the foam at a ship's bow: ahead of its waterline along its frame's heading, for all 16 headings, at its prow and either side of it");
    Check(okArms, "a ship's wake: two arms from its bow along the way it came, parting at the Kelvin angle (19.5 degrees either side), widening and fading to nothing as they go, behind it, for all 16 headings");
    Check(okDock, "a ship at its dock: its wake and foam all there and all dark (the list keeps its shape)");
    Check(okLeave, "a ship leaving its dock: its wakes only as far back as it has come, its foam faint as it gathers way");
    Check(okStern && churned >= 8, "the wake off a ship's stern, for all 16 headings: from its transom, the hull's width there, widening and fading over several hull lengths behind it, churned, foam in it, waves off the transom's corners parting wider than it");
    {                                                      /* plain close in, subtler with the whole world in view */
        static const float nearUV[8] = { 0.45f, 0.55f, 0.55f, 0.55f, 0.45f, 0.45f, 0.55f, 0.45f };   /* (a tenth) */
        float most = 0, whole[SHIP_STERN_SEGS + 1];
        for (int zoomed = 1; zoomed >= 0; zoomed--) {
            memcpy(g_bgUV, zoomed ? nearUV : kWholeUV, sizeof kWholeUV);
            WakeOne(0, 0, 0);
            ViewListStart();
            ShipsBuild(1000, 500, 3000, 0);
            for (int j = 0; j < ln; j++) {
                float a = WakePt(0, lane, ln, j)->c[3];
                if (zoomed && 3 * j <= ln) most = a > most ? a : most;
                if (!zoomed) whole[j] = a;
            }
        }
        int moving, ok = most >= 80;                           /* (the whole world's: half its strength, all else the same) */
        float way = ShipWay(0, 3.0f, &moving);
        ViewListStart();
        ShipStern(0, 500, 250, 1000, 500, way, (SHIP_STERN_LEN + SHIP_SMOOTH) * hs, (0.5f * SHIP_SMOOTH + 1) * hs, 1, 0, hs, 500, 3.0f, 1.0f, 0);
        for (int j = 0; j + 1 < ln; j++) ok &= fabsf(whole[j] - 0.5f * WakePt(0, 2 * SHIP_STERN_SEGS, ln, j)->c[3]) <= 1;
        ok &= whole[1] > 0;
        Check(ok, "the wake off a ship's stern: plain close in, half as strong with the whole world in view");
    }
    /* across its clock's turn: a loop whose clock starts out at sea (from the middle across, back, and to the middle) */
    {
        g_ships = 1; g_shipPts = 3;
        g_ship[0].kind = 0; g_ship[0].first = 0; g_ship[0].n = 3;
        float uu[3] = { 0.5f, 0.6f, 0.4f };
        for (int i = 0; i < 3; i++) { g_shipU[i] = uu[i]; g_shipV[i] = 0.5f; g_shipWait[i] = 0; }
        g_shipSpeed = 0.1f;
        ShipsTime(2.0f);
        g_ship[0].phase = 0;
        ViewListStart();
        ShipsBuild(1000, 500, 300, 0);                     /* 15 pixels past the clock's start, heading east */
        const ViewVert *L = g_vv + g_vb[0].first;
        Check(g_vbN == 3 && Near(L[0].x - L[8 * (SHIP_WAKE_SEGS - 1) + 1].x, len, 0.5) && L[8].c[3] > 0,
              "a ship's wake carries on across its clock's turn (its whole length behind it, lit)");
        /* round a bend: 100 pixels down and right, then east; 20 pixels past the bend its wake follows the way it came */
        float u3[3] = { 0.4f, 0.5f, 0.7f }, v3[3] = { 0.3f, 0.5f, 0.5f };
        for (int i = 0; i < 3; i++) { g_shipU[i] = u3[i]; g_shipV[i] = v3[i]; }
        ShipsTime(2.0f);
        g_ship[0].phase = 0;
        ViewListStart();
        ShipsBuild(1000, 500, (DWORD)((100 * sqrt(2.0) + 20) / 50 * 1000), 0);
        L = g_vv + g_vb[0].first;
        const ViewVert *R = L + 4 * 2 * SHIP_WAKE_SEGS, *le = L + 8 * (SHIP_WAKE_SEGS - 1) + 1, *re = R + 8 * (SHIP_WAKE_SEGS - 1) + 1;
        float by = (L[0].y + R[0].y) / 2, my = (le->y + re->y) / 2, mx = (le->x + re->x) / 2, bx = (L[0].x + R[0].x) / 2;
        const ViewVert *t0 = WakePt(0, lane, ln, 0), *tN = WakePt(0, lane, ln, ln - 1);
        Check(g_vbN == 3 && Near(g_vv[g_vb[2].first].u, 0.5 / SHIP_SHEET_W, 1e-6) && my < by - 0.5f * (len - 20) && mx > bx - len &&
              tN->y < t0->y - 0.5f * (SHIP_STERN_LEN * hs - 20),
              "round a bend a ship's wakes follow the way it came, not straight back from its heading");
        /* round a sharp corner (his note, 7 Oct: "the water wake behind boats abruptly changes direction when the boat
         * turns when it should be curved"): 200 pixels east, then a right angle and 200 down; at each moment as it comes
         * round, its arms and its stern's wake curve, turning only a few degrees from one piece to the next */
        float u4[3] = { 0.3f, 0.5f, 0.5f }, v4[3] = { 0.5f, 0.5f, 0.9f };
        for (int i = 0; i < 3; i++) { g_shipU[i] = u4[i]; g_shipV[i] = v4[i]; }
        ShipsTime(2.0f);
        g_ship[0].phase = 0;
        float worst = 0, rounds = 0, tot;
        for (int k = -4; k < 8; k++) {                     /* coming up to the corner, and 5 to 75 pixels past it */
            ViewListStart();
            ShipsBuild(1000, 500, (DWORD)((4 + (5 + 10 * k) / 50.0) * 1000), 0);
            if (g_vbN != 3) { worst = 180; break; }
            float m[3] = { WakeKink(0, 0, SHIP_WAKE_SEGS + 1, &tot), WakeKink(0, 2 * SHIP_WAKE_SEGS, SHIP_WAKE_SEGS + 1, &tot),
                           WakeKink(0, lane, ln, &tot) };
            rounds = tot > rounds ? tot : rounds;
            for (int i = 0; i < 3; i++) worst = m[i] > worst ? m[i] : worst;
        }
        Check(worst <= 6 && rounds >= 45, "round a sharp corner a ship's wakes curve: its arms and its stern's wake turn no more than a few degrees from one piece to the next, and round the corner with it");
        /* round a hairpin, the right angle, and turns of 135 and 155 degrees (his note, 8 Oct: "the wakes were good except sometimes when a boat would
         * turn, the front of it would send wakes out forwards"): at every moment of the turn no lit piece of its wakes lies
         * ahead of its bow as the hull is drawn, and its bow wake is still there, lit, behind it */
        {
            float hu[4][4] = { { 0.3f, 0.5f, 0.5f, 0.3f }, { 0.3f, 0.5f, 0.5f, 0.3f }, { 0.3f, 0.5f, 0.35f, 0.3f }, { 0.3f, 0.5f, 0.32f, 0.3f } };
            float hv[4][4] = { { 0.5f, 0.5f, 0.53f, 0.53f }, { 0.5f, 0.5f, 0.9f, 0.9f }, { 0.5f, 0.5f, 0.8f, 0.8f }, { 0.5f, 0.5f, 0.65f, 0.65f } };
            float aheadMost = -9, litLeast = 255, leaves = -9, parts = 9, curls = -9;
            for (int shape = 0; shape < 4; shape++) {
                g_ships = 1; g_shipPts = 4; g_ship[0].n = 4;
                for (int i = 0; i < 4; i++) { g_shipU[i] = hu[shape][i]; g_shipV[i] = hv[shape][i]; g_shipWait[i] = 0; }
                ShipsTime(2.0f);
                g_ship[0].phase = 0;
                for (int k = 0; k <= 80; k++) {                 /* from 150 pixels before its turn to 150 past it */
                    ViewListStart();
                    ShipsBuild(1000, 500, (DWORD)((1.0 + 0.075 * k) * 1000), 0);
                    if (g_vbN != 3) { aheadMost = 9; break; }
                    const ViewVert *sp = g_vv + g_vb[2].first;
                    int f = (int)(sp[0].u * SHIP_SHEET_W / SHIP_FRAME);
                    float hx, hy, bx, by, lit = 0;
                    ShipAxis(f, &hx, &hy);
                    ShipEnd(f, (sp[0].x + sp[2].x) / 2, (sp[0].y + sp[2].y) / 2, hs, 0, &bx, &by);
                    for (int i = 0; i < 4 * SHIP_WAKE_BANDS; i++) {
                        const ViewVert *v = g_vv + g_vb[0].first + i;
                        if (v->c[3] <= 2) continue;
                        float fwd = ((v->x - bx) * hx + (v->y - by) * hy) / hs;
                        aheadMost = fwd > aheadMost ? fwd : aheadMost;
                        if (i < 4 * 4 * SHIP_WAKE_SEGS) lit = v->c[3] > lit ? v->c[3] : lit;
                    }
                    for (int i = 0; i < 2 * SHIP_CAPS + SHIP_STERN_FOAM; i++) {   /* (its whitecaps and the stern's foam too) */
                        const ViewVert *v = g_vv + g_vb[1].first + 4 * i;
                        if (v->c[3] <= 2) continue;
                        float fwd = (((v[0].x + v[2].x) / 2 - bx) * hx + ((v[0].y + v[2].y) / 2 - by) * hy) / hs;
                        aheadMost = fwd > aheadMost ? fwd : aheadMost;
                    }
                    litLeast = lit < litLeast ? lit : litLeast;
                    int j5 = SHIP_WAKE_SEGS / (int)(2 * SHIP_WAKE_LEN);   /* (half a half size back along it) */
                    const ViewVert *L5 = WakePt(0, 0, SHIP_WAKE_SEGS + 1, j5), *R5 = WakePt(0, 2 * SHIP_WAKE_SEGS, SHIP_WAKE_SEGS + 1, j5);
                    if (L5->c[3] > 20 && R5->c[3] > 20) {           /* its bow wake leaves its bow behind it, parting either side */
                        float mf = (((L5->x + R5->x) / 2 - bx) * hx + ((L5->y + R5->y) / 2 - by) * hy) / hs;
                        float sl = ((L5->x - bx) * -hy + (L5->y - by) * hx) / hs, sr = ((R5->x - bx) * -hy + (R5->y - by) * hx) / hs;
                        leaves = mf > leaves ? mf : leaves;
                        float p = sl * sr < 0 ? -(fabsf(sl) < fabsf(sr) ? fabsf(sl) : fabsf(sr)) : 1;
                        parts = p < parts ? p : parts;
                    }
                    for (int arm = 0; arm < 2; arm++) {             /* and its arms never curl back toward it */
                        float back = 0;
                        for (int j = 0; j <= SHIP_WAKE_SEGS; j++) {
                            const ViewVert *v = WakePt(0, arm * 2 * SHIP_WAKE_SEGS, SHIP_WAKE_SEGS + 1, j);
                            float fwd = ((v->x - bx) * hx + (v->y - by) * hy) / hs;
                            back = fwd < back ? fwd : back;
                            if (v->c[3] > 2) curls = fwd - back > curls ? fwd - back : curls;
                        }
                    }
                }
            }
            Check(aheadMost <= 0.16f && litLeast >= 40 && leaves < -0.27f && parts < -0.08f && curls <= 0.31f,
                  "round a hairpin, a right angle and sharper turns no lit piece of a ship's wakes or foam lies ahead of its bow, and its bow wake is still there: leaving its bow behind it, parting either side of the hull, its arms never curling back toward it");
            /* the pieces: the hull's way on the screen as drawn (its slant), a turn through the quarter turn between two
             * ways more than a right angle apart, and a wake's keep (all behind its bow, none 0.15 half sizes ahead of
             * it, never rising again along it) */
            float ax, ay, rx, ry, keep = 1;
            ShipAxis(2, &ax, &ay);
            int ok = Near(ax, 0.58 / sqrt(0.58 * 0.58 + 0.42 * 0.42), 1e-4) && Near(ay, 0.42 / sqrt(0.58 * 0.58 + 0.42 * 0.42), 1e-4);
            ShipTurnTo(1, 0, -1, 0.0001f, 0.5f, &rx, &ry);      /* (east to west, by the north side: north half way) */
            ok &= Near(rx, 0, 1e-3) && Near(ry, 1, 1e-3);
            ShipTurnTo(1, 0, -1, -0.0001f, 0.25f, &rx, &ry);     /* (by the south side: south-east a quarter way) */
            ok &= Near(rx, 0.7071, 1e-3) && Near(ry, -0.7071, 1e-3);
            ShipTurnTo(1, 0, 0, 1, 0, &rx, &ry); ok &= Near(rx, 1, 1e-6) && Near(ry, 0, 1e-6);
            ShipTurnTo(1, 0, 0, 1, 1, &rx, &ry); ok &= Near(rx, 0, 1e-6) && Near(ry, 1, 1e-6);
            ok &= Near(ShipKeep(10, 0, 20, 0, 1, 0, 100, &keep), 1, 1e-6);   /* (behind) */
            ok &= Near(ShipKeep(27.5f, 0, 20, 0, 1, 0, 100, &keep), 0.5, 1e-5) && Near(ShipKeep(10, 0, 20, 0, 1, 0, 100, &keep), 0.5, 1e-5);
            ok &= Near(ShipKeep(35, 0, 20, 0, 1, 0, 100, &keep), 0, 1e-6);
            Check(ok, "a ship's hull's way as it is drawn, a turn through the quarter turn between two ways, and a wake's keep: all behind its bow, none 0.15 half sizes ahead, never rising again");
        }
        /* slowing into its dock (2 s waits at each end: arriving at 8 s): its wakes and foam fade with its way */
        WakeOne(0, 0, 2.0f);
        ViewListStart();
        ShipsBuild(1000, 500, 7900, 0);
        float armA = 0, sternA = 0, foamA = g_vv[g_vb[1].first + 4 * (2 * SHIP_CAPS + SHIP_STERN_FOAM)].c[3];
        for (int j = 0; j < ln; j++) sternA = WakePt(0, lane, ln, j)->c[3] > sternA ? WakePt(0, lane, ln, j)->c[3] : sternA;
        for (int j = 0; j <= SHIP_WAKE_SEGS; j++) armA = WakePt(0, 0, SHIP_WAKE_SEGS + 1, j)->c[3] > armA ? WakePt(0, 0, SHIP_WAKE_SEGS + 1, j)->c[3] : armA;
        Check(g_vbN == 3 && armA > 0 && armA < 40 && sternA > 0 && sternA < 40 && foamA > 0 && foamA < 70,
              "a ship slowing into its dock: its wakes and its foam fading with its way");
        /* a tenth of a second from its dock its wake runs on evenly behind it (its smoothing looks no further on than
         * the dock, and on past it as if straight on), back along its way */
        ViewListStart();
        ShipsBuild(1000, 500, 7800, 0);
        int even = g_vbN == 3;
        for (int j = 0; j < 8; j++) {
            const ViewVert *p0 = WakePt(0, lane, ln, j), *p1 = WakePt(0, lane, ln, j + 1);
            float dx = p1->x - p0->x, dy = p1->y - p0->y;
            even &= Near(sqrtf(dx * dx + dy * dy), SHIP_STERN_LEN * hs / SHIP_STERN_SEGS, 0.05 * SHIP_STERN_LEN * hs / SHIP_STERN_SEGS) &&
                    dx < 0 && fabsf(dy) < 0.02f * fabsf(dx);
        }
        Check(even, "coming into its dock a ship's wake runs on evenly behind it, back along its way");
        /* on a steady curve (round a circle of 150 pixels) its wake leaves it along its way there, not lagging round it */
        {
            enum { CP = 48 };
            g_ships = 1; g_shipPts = CP; g_ship[0].n = CP;
            for (int i = 0; i < CP; i++) {
                double a = 2 * M_PI * i / CP;
                g_shipU[i] = 0.5f + 0.15f * (float)cos(a); g_shipV[i] = 0.5f + 0.3f * (float)sin(a); g_shipWait[i] = 0;
            }
            ShipsTime(2.0f);
            g_ship[0].phase = 0;
            float lag = 0;
            for (int k = 0; k < 6; k++) {
                DWORD now = 2000 + 437 * (DWORD)k;
                ViewListStart();
                ShipsBuild(1000, 500, now, 0);
                int moving;
                float w = ShipWay(0, (float)now / 1000, &moving), u, v;
                ShipAt(0, w, &u, &v);
                float rx = (u - 0.5f) * 1000, ry = (v - 0.5f) * 500, bx = ry, by = -rx;   /* (back round it: clockwise on screen) */
                const ViewVert *p0 = WakePt(0, lane, ln, 0), *p1 = WakePt(0, lane, ln, 2);
                float d = atan2f(p1->y - p0->y, p1->x - p0->x) - atan2f(by, bx);
                while (d > (float)M_PI) d -= 2 * (float)M_PI;
                while (d < -(float)M_PI) d += 2 * (float)M_PI;
                lag = fabsf(d) > lag ? fabsf(d) : lag;
            }
            Check(lag * 180 / (float)M_PI < 4, "on a steady curve a ship's wake leaves it along its way there, not lagging round it");
        }
    }

    /* the wind round a zeppelin: its ways bend out round the balloon and come together behind it */
    {
        int ok = 1;
        for (int i = 0; i <= 20; i++) {
            float l0 = 0.12f + 0.33f * (float)i / 20;
            ok &= Near(ZepFlow(1.6f, l0, ZEP_ROUND), l0, 0.01) && Near(ZepFlow(0, l0, ZEP_ROUND), l0 + ZEP_ROUND, 1e-4) &&
                  ZepFlow(-1.6f, l0, ZEP_ROUND) < 0.2f * ZepFlow(0, l0, ZEP_ROUND);
            for (int k = 4; k < 16; k++) ok &= ZepFlow(-0.1f * (float)(k + 1), l0, ZEP_ROUND) < ZepFlow(-0.1f * (float)k, l0, ZEP_ROUND);
        }
        Check(ok, "the wind's ways round a zeppelin: straight out ahead, bending out round its balloon, coming together behind it");
    }
    float zs = ZEP_SIZE * 500;
    int okPar = 1, okAft = 1, okDark = 1, okAir = 1, aft = 0, wispAft = 0;
    enum { SQ = 2 * (ZEP_STREAK_PTS - 1), TQ = 2 * (ZEP_TRAIL_PTS - 1), WQ = 2 * (ZEP_WISP_PTS - 1) };
    for (int f = 0; f < 16; f++) {
        float hx = kShipHead[f][0], hy = kShipHead[f][1], mid0[ZEP_STREAKS], wisp0[ZEP_WISPS], lit = 0;
        for (int pass = 0; pass < 2; pass++) {
            WakeOne(1, f, 0);
            ViewListStart();
            ShipsBuild(1000, 500, 2000 + 30 * (DWORD)pass, 1);
            if (g_vbN != 2 || g_vb[0].tex != VT_SHIPS || g_vb[1].tex != -1 || !g_vb[1].add || g_vb[1].count != 4 * ZEP_STREAK_Q) { okPar = 0; break; }
            float zx = WakeMidX(0, 0), zy = WakeMidY(0, 0);   /* (from the zeppelin, which sails on too) */
            for (int i = 0; i < ZEP_STREAKS; i++) {           /* the streaks: fore end to aft end along its ways round it */
                float prev = 1e9f, sgn = 0;
                for (int e = 0; e < ZEP_STREAK_PTS; e++) {
                    const ViewVert *v = WakePt(1, SQ * i, ZEP_STREAK_PTS, e);
                    float along = ((v->x - zx) * hx + (v->y - zy) * hy) / zs, side = ((v->x - zx) * -hy + (v->y - zy) * hx) / zs;
                    okPar &= along < prev;
                    prev = along;
                    if (e == 0) sgn = side;
                    okPar &= side * sgn > 0;                                          /* one side of it */
                    if (fabsf(along) < 0.15f) okPar &= fabsf(side) > 0.5f;            /* round its balloon */
                    if (along < -1.6f) okPar &= fabsf(side) < 0.12f;                 /* together behind it */
                    if (along > 1.0f) okPar &= fabsf(side) > 0.1f && fabsf(side) < 0.56f;   /* straight out ahead */
                    if (e == ZEP_STREAK_PTS / 2) {
                        if (v->c[3] > lit) lit = v->c[3];
                        if (pass == 0) mid0[i] = along;
                        else {
                            float moved = along - mid0[i];        /* aft, a little; or a new streak, well ahead */
                            okAft &= (moved < 0 && moved > -0.2f) || moved > 0.5f;
                            aft += moved < 0;
                        }
                    }
                }
                okPar &= WakePt(1, SQ * i, ZEP_STREAK_PTS, 0)->c[3] == 0 && WakePt(1, SQ * i, ZEP_STREAK_PTS, ZEP_STREAK_PTS - 1)->c[3] == 0;
            }
            float wPrev = -1;                                  /* its air wake: a faint trail behind it, widening, */
            for (int e = 0; e < ZEP_TRAIL_PTS; e++) {
                const ViewVert *v = WakePt(1, SQ * ZEP_STREAKS, ZEP_TRAIL_PTS, e);
                float along = ((v->x - zx) * hx + (v->y - zy) * hy) / zs, side = ((v->x - zx) * -hy + (v->y - zy) * hx) / zs;
                float w = WakeReach(1, SQ * ZEP_STREAKS, ZEP_TRAIL_PTS, e);
                okAir &= along < -0.5f && fabsf(side) < 0.15f && w > wPrev && v->c[3] <= 0.17f * 255;
                wPrev = w;
            }
            okAir &= WakePt(1, SQ * ZEP_STREAKS, ZEP_TRAIL_PTS, 1)->c[3] > 0;
            for (int i = 0; i < ZEP_WISPS; i++) {             /* and faint wisps curling in it, drifting aft */
                int q0 = SQ * ZEP_STREAKS + TQ + WQ * i;
                float sum = 0;
                for (int e = 0; e < ZEP_WISP_PTS; e++) {
                    const ViewVert *v = WakePt(1, q0, ZEP_WISP_PTS, e);
                    float along = ((v->x - zx) * hx + (v->y - zy) * hy) / zs, side = ((v->x - zx) * -hy + (v->y - zy) * hx) / zs;
                    okAir &= along < -0.5f && fabsf(side) < 0.3f && v->c[3] <= 0.21f * 255;
                    sum += along;
                }
                if (pass == 0) wisp0[i] = sum / ZEP_WISP_PTS;
                else {
                    float moved = sum / ZEP_WISP_PTS - wisp0[i];
                    okAir &= moved < 0.05f || moved > 0.5f;
                    wispAft += moved < 0;
                }
            }
        }
        okPar &= lit > 0;
        WakeOne(1, f, 2.0f);                               /* at its tower: no wind */
        ViewListStart();
        ShipsBuild(1000, 500, 1000, 1);
        okDark &= g_vbN == 2 && g_vb[1].count == 4 * ZEP_STREAK_Q;
        for (int i = 0; i < g_vb[1].count; i++) okDark &= g_vv[g_vb[1].first + i].c[3] == 0;
    }
    Check(okPar, "the wind round a zeppelin, for all 16 headings: thin streaks along its ways, straight out ahead, round its balloon and together behind it, fading at both ends, some lit");
    {                                                      /* heading east: further round on its gondola's side, below it */
        int below = 0, above = 0, ok = 1;
        for (int k = 0; k < 40; k++) {
            WakeOne(1, 0, 0);
            ViewListStart();
            ShipsBuild(1000, 500, 2000 + 97 * (DWORD)k, 1);
            float zx = WakeMidX(0, 0), zy = WakeMidY(0, 0);
            for (int i = 0; i < ZEP_STREAKS; i++)
                for (int e = 0; e < ZEP_STREAK_PTS; e++) {
                    const ViewVert *v = WakePt(1, SQ * i, ZEP_STREAK_PTS, e);
                    float along = (v->x - zx) / zs, side = (v->y - zy) / zs;
                    if (fabsf(along) >= 0.15f) continue;
                    if (side > 0) { below++; ok &= side > 0.68f; } else { above++; ok &= side < -0.5f; }
                }
        }
        Check(ok && below > 0 && above > 0, "the wind round a zeppelin heading east goes further round below it, on its gondola's side");
    }
    Check(okAft && aft >= 16 * (ZEP_STREAKS - 1), "the wind's streaks slide aft past the zeppelin as the clock runs (a new one from ahead)");
    Check(okAir && wispAft >= 16 * (ZEP_WISPS - 1), "a zeppelin's tiny air wake: a faint trail behind it, widening, and faint wisps curling in it, drifting aft");
    Check(okDark, "a zeppelin at its tower: its wind all there and all dark");

    /* the budget: every transport in view, all under way, ships or zeppelins or half of each */
    int okBudget = 1;
    for (int mix = 0; mix < 3; mix++) {
        g_ships = SHIPS_MAX; g_shipPts = 2 * SHIPS_MAX;
        for (int r = 0; r < SHIPS_MAX; r++) {
            g_ship[r].kind = mix == 2 ? (r & 1) : mix; g_ship[r].first = 2 * r; g_ship[r].n = 2; g_ship[r].rgb = 0x808080;
            g_shipU[2 * r] = 0.3f + 0.01f * r; g_shipV[2 * r] = 0.3f; g_shipU[2 * r + 1] = 0.3f + 0.01f * r; g_shipV[2 * r + 1] = 0.7f;
            g_shipWait[2 * r] = g_shipWait[2 * r + 1] = 0;
        }
        ShipsTime(2.0f);
        for (int r = 0; r < SHIPS_MAX; r++) g_ship[r].phase = 0;
        ViewListStart();
        int others = 2 + WAVE_QUADS + FLOW_DROP_Q + LBL_FRAME + GLINTS + 6 * MARKS_FRAME + SABER_QUADS + SKY_QUADS;   /* (the rest of the view at its most) */
        for (int i = 0; i < others; i++) ViewQuadMode(VT_MAP, 0);
        ShipsBuild(1000, 500, 1000, 0);
        ShipsBuild(1000, 500, 1000, 1);
        int zeps = mix == 2 ? SHIPS_MAX / 2 : mix * SHIPS_MAX, ships = SHIPS_MAX - zeps;
        okBudget &= !g_vbFull && g_vvN - 4 * others == 4 * (ships * (2 + SHIP_WAKE_Q) + zeps * (2 + ZEP_STREAK_Q)) && g_vbN <= 6;
    }
    Check(okBudget, "every transport in view and under way: their wakes, foam and wind all in the list, within the view's budget for them, in at most five batches");
    g_shipsOn = 0; g_ships = 0; g_viewTex[VT_SHIPS] = 0; g_viewTex[VT_SPARK] = 0;
}

/* The weather (7 Oct 2026): its grid read, his circulation's winds, clouds born only where the grid has them, raining,
 * snowing or blowing dust by the season's chances under them, standing above their shadows with the rain between, and
 * thinning away as the camera comes down through them. */
static unsigned char g_wtest[16 + 4 * 2 * 13];
static void SkyGrid(int cover, int rain, int snow, int sand)
{
    unsigned char *d = g_wtest;
    float veq = 0.5f, deg = 76;
    memcpy(d, "CWTH", 4); d[4] = 4; d[5] = 0; d[6] = 2; d[7] = 0;
    memcpy(d + 8, &veq, 4); memcpy(d + 12, &deg, 4);
    for (int c = 0; c < 8; c++) {
        unsigned char *e = d + 16 + 13 * c;
        e[0] = (unsigned char)(c == 1 ? cover : 0);          /* clouds only in the second cell of the top row */
        for (int k = 0; k < 4; k++) { e[1 + 3 * k] = (unsigned char)rain; e[2 + 3 * k] = (unsigned char)snow; e[3 + 3 * k] = (unsigned char)sand; }
    }
}
static void TestSky(void)
{
    SkyGrid(100, 40, 0, 0);
    int ok = SkyParse(g_wtest, sizeof g_wtest) && g_sky.cols == 4 && g_sky.rows == 2 && Near(g_sky.veq, 0.5, 1e-6) &&
             g_sky.season == 3;                                            /* (the shim's month 0: winter, as December) */
    ok &= !SkyParse(g_wtest, sizeof g_wtest - 1);                          /* a short grid */
    g_wtest[0] = 'X'; ok &= !SkyParse(g_wtest, sizeof g_wtest); g_wtest[0] = 'C';
    ok &= SkyParse(g_wtest, sizeof g_wtest) && SkyCell(0.3f, 0.2f) == g_skyCell + 13 && !SkyCell(1.2f, 0.2f);
    Check(ok, "the weather's grid read (its cells, the equator, the season from the month); a short one or a wrong header is no weather");

    /* his circulation (the equator at v 0.5, 76 degrees to the picture's height): latitude = (0.5 - v) * 76 */
    float e, n;
    SkyWind(0.5f - 2 / 76.0f, &e, &n);   ok = e < 0 && e > -0.4f && n == 0;           /* 2 N: the doldrums, light, west */
    SkyWind(0.5f - 15 / 76.0f, &e, &n);  ok &= e < -0.5f && n < 0;                     /* 15 N: the trades, west and south */
    SkyWind(0.5f + 15 / 76.0f, &e, &n);  ok &= e < -0.5f && n > 0;                     /* 15 S: west and north (mirrored) */
    SkyWind(0.5f - 32 / 76.0f, &e, &n);  ok &= e > -0.2f && e < 0.2f;                  /* 32 N: the horse latitudes, still */
    SkyWind(0.5f - 45 / 76.0f, &e, &n);  ok &= e > 0.5f && n > 0;                      /* 45 N: the westerlies, east and north */
    SkyWind(0.5f - 65 / 76.0f, &e, &n);  ok &= e < 0;                                  /* 65 N: the polar easterlies */
    float e1, n1;
    SkyWind(0.5f - 29.5f / 76.0f, &e1, &n1);   ok &= e1 > -0.85f && e1 < 0.1f;         /* blended into the next band */
    Check(ok, "the winds by latitude (his circulation): the doldrums light and westward, the trades from the northeast (mirrored south of the equator), the horse latitudes still, the westerlies, the polar easterlies, each blending into the next");

    /* clouds: born only where the grid has them (the top row's second cell), drifting on its wind */
    static const float whole[8] = { 0, 1, 1, 1, 0, 0, 1, 0 };
    memcpy(g_bgUV, whole, sizeof whole);
    g_shipAsp = 2;
    int born = 0, outside = 0;
    SkyCloud c;
    for (int i = 0; i < SKY_CLOUDS; i++)
        if (SkyAt(i, 5000, 1000, 500, &c)) {
            born++;
            float u = c.gx / 1000, v = c.gy / 500;
            outside += !(u > 0.15f && u < 0.6f && v < 0.55f);
        }
    ok = born > SKY_CLOUDS / 4 && outside == 0;
    SkyGrid(0, 40, 0, 0); SkyParse(g_wtest, sizeof g_wtest);
    born = 0;
    for (int i = 0; i < SKY_CLOUDS; i++) born += SkyAt(i, 5000, 1000, 500, &c);
    ok &= born == 0;
    SkyAt(3, 7000, 1000, 500, &c);
    Check(ok, "clouds are born only where the grid says clouds are, and none where it says none");

    /* what falls: rain where it rains (40%, every cloud), snow where snow is the likelier, dust over sand, as the season */
    int rain = 0, snow = 0, dust = 0, all = 0;
    SkyGrid(100, 40, 0, 0); SkyParse(g_wtest, sizeof g_wtest);
    for (int i = 0; i < SKY_CLOUDS; i++) if (SkyAt(i, 5000, 1000, 500, &c)) { all++; rain += c.rain; snow += c.snow; }
    ok = all > 0 && rain == all && snow == 0;
    SkyGrid(100, 10, 40, 0); SkyParse(g_wtest, sizeof g_wtest); rain = snow = all = 0;
    for (int i = 0; i < SKY_CLOUDS; i++) if (SkyAt(i, 5000, 1000, 500, &c)) { all++; rain += c.rain; snow += c.snow; }
    ok &= all > 0 && snow == all && rain == 0;
    SkyGrid(100, 0, 0, 40); SkyParse(g_wtest, sizeof g_wtest); all = 0;
    for (int i = 0; i < SKY_CLOUDS; i++) if (SkyAt(i, 5000, 1000, 500, &c)) { all++; dust += c.dust; }
    ok &= all > 0 && dust == all;
    SkyGrid(100, 0, 0, 0); SkyParse(g_wtest, sizeof g_wtest); rain = snow = dust = 0;
    for (int i = 0; i < SKY_CLOUDS; i++) if (SkyAt(i, 5000, 1000, 500, &c)) { rain += c.rain; snow += c.snow; dust += c.dust; }
    ok &= rain == 0 && snow == 0 && dust == 0;
    Check(ok, "under each cloud what the season usually brings there: rain where it rains, snow where snow is likelier, blowing sand over the deserts, nothing where it is dry");

    /* 3D: a cloud stands above its ground (its shadow), rain falls between; nearer in it swells past the ground's own
     * zoom, and once the camera is down among them it is gone */
    SkyGrid(100, 40, 0, 0); SkyParse(g_wtest, sizeof g_wtest);
    int i0 = -1;
    for (int i = 0; i < SKY_CLOUDS && i0 < 0; i++) if (SkyAt(i, 5000, 1000, 500, &c)) i0 = i;
    SkyAt(i0, 5000, 1000, 500, &c);
    float size1 = c.size, ppu1 = 500;
    ok = i0 >= 0 && c.y < c.gy && c.base < c.gy && c.size > 0;
    ok &= c.alt >= 0.8f * SKY_ALT - 1e-6f && c.alt <= 1.2f * SKY_ALT + 1e-6f && Near(c.p, SKY_CAM / (SKY_CAM - c.alt), 1e-4) &&
          Near(c.x, 500 + (c.gx - 500) * c.p, 1e-2) && Near(c.y, 250 + (c.gy - 250) * c.p - c.alt * 500 * SKY_TILT * c.p, 1e-2);
    static const float near1[8] = { 0.2f, 0.45f, 0.6f, 0.45f, 0.2f, 0.25f, 0.6f, 0.25f };   /* a fifth of the height */
    memcpy(g_bgUV, near1, sizeof near1);
    SkyCloud d;
    int seenNear = SkyAt(i0, 5000, 1000, 500, &d);
    float hc = SKY_CAM * 0.2f;
    ok &= !seenNear || d.size / (500 / 0.2f) > size1 / ppu1 * 1.0001f * (hc > SKY_ALT * 1.05f ? 1 : 0);   /* swells */
    ok &= Near(d.p, hc > d.alt * 1.05f ? (hc / (hc - d.alt) < 3 ? hc / (hc - d.alt) : 3) : 3, 1e-4) && d.p > c.p;
    static const float low[8] = { 0.38f, 0.43f, 0.42f, 0.43f, 0.38f, 0.41f, 0.42f, 0.41f };   /* the camera below them */
    memcpy(g_bgUV, low, sizeof low);
    int gone = 1;
    for (int i = 0; i < SKY_CLOUDS; i++) gone &= !SkyAt(i, 5000, 1000, 500, &c);
    ok &= gone;
    memcpy(g_bgUV, whole, sizeof whole);
    Check(ok, "a cloud stands above its ground with the rain falling between; nearer in it swells past the map's own zoom; with the camera down among them they are gone");

    /* no clouds' tile: no clouds, so nothing rains (the passes themselves: TestClouds) */
    g_viewTex[VT_SPARK] = 1; g_viewTex[VT_SKY] = 1; g_cloudOk = 0;
    ViewListStart();
    SkyBuild(1000, 500, 5000);
    Check(g_vbN == 0, "without the clouds' tile no cloud is drawn, and no rain falls from one");
    g_viewTex[VT_SPARK] = 0; g_viewTex[VT_SKY] = 0; g_sky.ok = 0;
}

/* The clouds as light makes them (7 Oct 2026): a field composited from the tile (clouds.bin) over the weather's grid,
 * in three parts (the shadows on the ground, the low clouds, the high), premultiplied, drawn through the view each frame
 * at their layers' heights. */
static unsigned char g_ctile[8 + 2 * CLOUD_N * CLOUD_N];
static unsigned char g_cgrid[16 + 16 * 8 * 13];
static float g_cd[CLOUD_N][CLOUD_N];
/* A tile: round heaps on a 64-texel lattice (blobs 1), cloud everywhere (2), or smooth noise (0); lit as clouds.py
 * lights it (slopes toward the upper left bright, the lower flank greyer). */
static void CloudTileMake(int blobs)
{
    for (int y = 0; y < CLOUD_N; y++)
        for (int x = 0; x < CLOUD_N; x++) {
            float d;
            if (blobs == 2) d = 0.9f;
            else if (blobs == 3) d = x % 64 < 32 && y % 64 < 32 ? 1 : 0;      /* sharp-edged squares */
            else if (blobs) {
                float dx = (float)(x % 64) - 32, dy = (float)(y % 64) - 32, r = sqrtf(dx * dx + dy * dy);
                d = r < 24 ? 1 - r / 24 : 0;
            } else {
                d = 0;
                for (int o = 0; o < 2; o++) {
                    int s = o ? 16 : 32;
                    float fx = (float)x / s, fy = (float)y / s;
                    int ix = (int)fx, iy = (int)fy, n = CLOUD_N / s;
                    float ax = fx - ix, ay = fy - iy;
                    ax = ax * ax * (3 - 2 * ax); ay = ay * ay * (3 - 2 * ay);
                    float a = SeaRand((unsigned)(ix % n) + 97u * o, (unsigned)(iy % n)), b = SeaRand((unsigned)((ix + 1) % n) + 97u * o, (unsigned)(iy % n));
                    float c = SeaRand((unsigned)(ix % n) + 97u * o, (unsigned)((iy + 1) % n)), e = SeaRand((unsigned)((ix + 1) % n) + 97u * o, (unsigned)((iy + 1) % n));
                    d += (o ? 0.35f : 0.65f) * ((a + (b - a) * ax) * (1 - ay) + (c + (e - c) * ax) * ay);
                }
            }
            g_cd[y][x] = d;
        }
    memcpy(g_ctile, "CCLD", 4); g_ctile[4] = CLOUD_N & 255; g_ctile[5] = CLOUD_N >> 8; g_ctile[6] = 2; g_ctile[7] = 0;
    for (int y = 0; y < CLOUD_N; y++)
        for (int x = 0; x < CLOUD_N; x++) {
            float gx = g_cd[y][(x + 1) % CLOUD_N] - g_cd[y][(x + CLOUD_N - 1) % CLOUD_N];
            float gy = g_cd[(y + 1) % CLOUD_N][x] - g_cd[(y + CLOUD_N - 1) % CLOUD_N][x];
            float L = 0.5f + 4.0f * (gx + gy) - 3.0f * (gy < 0 ? -gy : 0);
            g_ctile[8 + 2 * (y * CLOUD_N + x)] = (unsigned char)(int)(g_cd[y][x] * 255 + 0.5f);
            g_ctile[9 + 2 * (y * CLOUD_N + x)] = (unsigned char)(int)(FireClamp(L, 0, 1) * 255 + 0.5f);
        }
}
/* A 16 x 8 grid, the equator at v 0.5, 76 degrees high: cover (raw, the grid's percent) in the cells c0..c1, r0..r1
 * (all: -1), rain each season. */
static void CloudGrid(int cover, int rain, int c0, int c1, int r0, int r1)
{
    unsigned char *d = g_cgrid;
    float veq = 0.5f, deg = 76;
    memcpy(d, "CWTH", 4); d[4] = 16; d[5] = 0; d[6] = 8; d[7] = 0;
    memcpy(d + 8, &veq, 4); memcpy(d + 12, &deg, 4);
    for (int j = 0; j < 8; j++)
        for (int i = 0; i < 16; i++) {
            unsigned char *e = d + 16 + 13 * (j * 16 + i);
            e[0] = (unsigned char)(c0 < 0 || (i >= c0 && i <= c1 && j >= r0 && j <= r1) ? cover : 0);
            for (int k = 0; k < 4; k++) { e[1 + 3 * k] = (unsigned char)rain; e[2 + 3 * k] = 0; e[3 + 3 * k] = 0; }
        }
    SkyParse(g_cgrid, sizeof g_cgrid);
}
static void CloudView(float u0, float vt, float span)
{
    float uv[8] = { u0, vt + span, u0 + span, vt + span, u0, vt, u0 + span, vt };
    memcpy(g_bgUV, uv, sizeof uv);
}
/* A whole picture of the sky for the view now, made and shown (as the view's thread: begun, worked, swapped). */
static void CloudMake(float W, float H, DWORD now)
{
    g_cj.on = 0;
    CloudBegin(W, H, now);
    CloudWork(1);
    CloudSwap();
}
static unsigned CloudPx(int k, int i, int j)
{
    const CloudPart *c = &g_cloudShown.part[k];
    return g_cloudPx[g_cloudFront][(c->y + j) * CLOUD_TEX_W + c->x + i];
}
static double CloudSumA(int k)
{
    double s = 0;
    for (int j = 0; j < g_cloudShown.part[k].h; j++)
        for (int i = 0; i < g_cloudShown.part[k].w; i++) s += CloudPx(k, i, j) >> 24;
    return s;
}
/* The centroid of part k's alpha on screen (its layer's mapping), and its ground (picture) centroid. */
static void CloudCentroid(int k, float W, float H, float *sx, float *sy, float *gu, float *gv)
{
    const CloudPart *c = &g_cloudShown.part[k];
    int l = k == 2;
    float p = k ? g_cj.p[l] : 1, lift = k ? g_cj.lift[l] : 0;
    double a = 0, x = 0, y = 0, u = 0, v = 0;
    for (int j = 0; j < c->h; j++)
        for (int i = 0; i < c->w; i++) {
            double w = CloudPx(k, i, j) >> 24;
            float cu = c->u0 + ((float)i + 0.5f) * c->du, cv = c->v0 + ((float)j + 0.5f) * c->dv;
            float gx = (cu - g_cj.u0) / g_cj.us * W, gy = (cv - g_cj.vt) / g_cj.vs * H;
            a += w; u += w * cu; v += w * cv;
            x += w * (W / 2 + (gx - W / 2) * p); y += w * (H / 2 + (gy - H / 2) * p - lift);
        }
    *sx = (float)(x / a); *sy = (float)(y / a); *gu = (float)(u / a); *gv = (float)(v / a);
}
static double CloudMs(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
static void TestClouds(void)
{
    /* the tile read; a wrong one is no clouds */
    CloudTileMake(0);
    int ok = CloudParse(g_ctile, sizeof g_ctile) && g_cloudTile[5] == (g_ctile[18] | (unsigned)g_ctile[19] << 16) && g_cloudSteep > 0;
    ok &= !CloudParse(g_ctile, sizeof g_ctile - 1);
    g_ctile[3] = 'X'; ok &= !CloudParse(g_ctile, sizeof g_ctile); g_ctile[3] = 'D';
    ok &= CloudParse(g_ctile, sizeof g_ctile);
    Check(ok, "the clouds' tile read (density and light, its steepness); a short one or a wrong header is no clouds");
    unsigned keepTile[4] = { g_cloudTile[0], g_cloudTile[1], g_cloudTile[CLOUD_N], g_cloudTile[CLOUD_N + 1] };
    g_cloudTile[0] = 0 | 200u << 16; g_cloudTile[1] = 200 | 0u << 16; g_cloudTile[CLOUD_N] = 100; g_cloudTile[CLOUD_N + 1] = 100;
    unsigned t1 = CloudTap(65536 / 4, 0), t2 = CloudTap(0, 65536 / 4);
    ok = Near(t1 & 255, 50, 1) && Near(t1 >> 16, 150, 1) && Near(t2 & 255, 25, 1) && Near(t2 >> 16, 150, 1);
    ok &= Near(CloudDense(65536 / 4, 0), 50 * 256, 64) && Near(CloudDense(0, 65536 / 4), 25 * 256, 64) &&
          Near(CloudDense(65536 / 4 + 256, 0) - CloudDense(65536 / 4, 0), 200, 4) &&
          Near(CloudDense(65536 / 4 + 512, 0) - CloudDense(65536 / 4 + 256, 0), 200, 4);   /* (sixteen bits: no steps) */
    g_cloudTile[0] = keepTile[0]; g_cloudTile[1] = keepTile[1]; g_cloudTile[CLOUD_N] = keepTile[2]; g_cloudTile[CLOUD_N + 1] = keepTile[3];
    Check(ok, "the tile read between its texels, density and light at once, each the nearer the more");

    /* deterministic; premultiplied (no channel over its alpha); none where the cover is nothing; more with more cover */
    float W = 1600, H = 675;
    g_shipAsp = W / H;
    CloudGrid(100, 0, -1, 0, 0, 0);
    CloudView(0, 0, 1);
    g_destOk = 0;
    CloudMake(W, H, 5000000);
    static unsigned first[CLOUD_TEX_W * CLOUD_TEX_H];
    memcpy(first, g_cloudPx[g_cloudFront], sizeof first);
    CloudMake(W, H, 5000000);
    ok = !memcmp(first, g_cloudPx[g_cloudFront], sizeof first) && CloudSumA(1) > 0;
    Check(ok, "the sky's picture is the same for the same moment and view (all from the clock), and has clouds where the cover is full");
    ok = 1;
    for (int i = 0; i < CLOUD_TEX_W * g_cloudShown.rows; i++) {
        unsigned q = g_cloudPx[g_cloudFront][i], a = q >> 24;
        ok &= (q & 255) <= a && (q >> 8 & 255) <= a && (q >> 16 & 255) <= a;
    }
    Check(ok, "premultiplied: no texel's colour is over its alpha (drawn ONE, ONE_MINUS_SRC_ALPHA)");
    double full = CloudSumA(1), fullShadow = CloudSumA(0);
    CloudGrid(70, 0, -1, 0, 0, 0); CloudMake(W, H, 5000000);
    double most = CloudSumA(1);
    double fullHigh = CloudSumA(2);
    CloudGrid(35, 0, -1, 0, 0, 0); CloudMake(W, H, 5000000);
    double some = CloudSumA(1), someHigh = CloudSumA(2);
    CloudGrid(0, 0, -1, 0, 0, 0); CloudMake(W, H, 5000000);
    ok = some > 0 && most > some * 1.3 && full > most * 1.1 && fullShadow > 0 && fullHigh > someHigh * 1.2 && CloudSumA(0) == 0 && CloudSumA(1) == 0 && CloudSumA(2) == 0;
    ViewListStart();
    CloudDraw(0, W, H, 5000000); CloudDraw(1, W, H, 5000000); CloudDraw(2, W, H, 5000000);
    ok &= g_vbN == 0;
    Check(ok, "no cloud and no shadow where the cover is nothing (and nothing drawn); more cloud as the cover rises");

    /* lit toward the sun in the upper left; the bases darker; darker still where it rains */
    CloudTileMake(1); CloudParse(g_ctile, sizeof g_ctile);
    CloudGrid(100, 0, -1, 0, 0, 0); CloudMake(W, H, 5000000);
    double lum[4] = { 0, 0, 0, 0 }, cnt[4] = { 0, 0, 0, 0 };
    const CloudPart *lo = &g_cloudShown.part[1];
    for (int j = 1; j < lo->h - 1; j++)
        for (int i = 1; i < lo->w - 1; i++) {
            unsigned q = CloudPx(1, i, j);
            int a = (int)(q >> 24);
            if (a < 60) continue;
            int gx = (int)(CloudPx(1, i + 1, j) >> 24) - (int)(CloudPx(1, i - 1, j) >> 24), gy = (int)(CloudPx(1, i, j + 1) >> 24) - (int)(CloudPx(1, i, j - 1) >> 24);
            double l = ((q & 255) + (q >> 8 & 255) + (q >> 16 & 255)) / (3.0 * a);
            if (gx > 4 && gy > 4) { lum[0] += l; cnt[0]++; }             /* the upper-left flank: facing the sun */
            if (gx < -4 && gy < -4) { lum[1] += l; cnt[1]++; }           /* the lower-right */
            if (gy < -4 && gx > -3 && gx < 3) { lum[2] += l; cnt[2]++; } /* the base, below */
            if (gy > 4 && gx > -3 && gx < 3) { lum[3] += l; cnt[3]++; }  /* the top */
        }
    for (int k = 0; k < 4; k++) lum[k] = cnt[k] ? lum[k] / cnt[k] : 0;
    ok = cnt[0] > 20 && cnt[1] > 20 && cnt[2] > 20 && cnt[3] > 20 && lum[0] > lum[1] + 0.08 && lum[3] > lum[2] + 0.08;
    double fair = lum[0] * cnt[0] + lum[1] * cnt[1];
    CloudGrid(100, 40, -1, 0, 0, 0); CloudMake(W, H, 5000000);
    double sl = 0, sn = 0;
    for (int j = 0; j < lo->h; j++)
        for (int i = 0; i < lo->w; i++) {
            unsigned q = CloudPx(1, i, j);
            int a = (int)(q >> 24);
            if (a >= 60) { sl += ((q & 255) + (q >> 8 & 255) + (q >> 16 & 255)) / (3.0 * a); sn++; }
        }
    ok &= sn > 0 && sl / sn < fair / (cnt[0] + cnt[1]) - 0.1;
    Check(ok, "the clouds lit from the upper left (the flanks facing the sun brighter than the far ones), their bases darker than their tops, and greyer where it rains");

    /* the shadow on the ground away from the sun: below and right of its cloud, which stands up the screen from it */
    CloudGrid(100, 0, 6, 9, 2, 5);
    CloudMake(W, H, 5000000);
    float cx, cy, cu, cv, sx, sy, su, sv;
    CloudCentroid(1, W, H, &cx, &cy, &cu, &cv);
    CloudCentroid(0, W, H, &sx, &sy, &su, &sv);
    /* the shadow's shape is its cloud's, where the sun's ray from each ground point meets the clouds: it matches the
     * clouds best shifted that way, not as they are overhead nor shifted the other way */
    double match[3] = { 0, 0, 0 };
    const CloudPart *gp0 = &g_cloudShown.part[0], *cp = &g_cloudShown.part[1];
    for (int o = 0; o < 3; o++)
        for (int j = 0; j < gp0->h; j++)
            for (int i = 0; i < gp0->w; i++) {
                float k = (float)(1 - o), gu = gp0->u0 + ((float)i + 0.5f) * gp0->du, gv = gp0->v0 + ((float)j + 0.5f) * gp0->dv;
                float lu = gu + k * CLOUD_SUN_U * CLOUD_ALT_LO / g_shipAsp, lv = gv + k * CLOUD_SUN_V * CLOUD_ALT_LO;
                int ci = (int)((lu - cp->u0) / cp->du), cj = (int)((lv - cp->v0) / cp->dv);
                if (ci < 0 || cj < 0 || ci >= cp->w || cj >= cp->h) continue;
                double sa = (CloudPx(0, i, j) >> 24) / 255.0, ca = (CloudPx(1, ci, cj) >> 24) > 100 ? 1 : 0;
                match[o] += sa * ca - sa * (1 - ca);
            }
    ok = sx > cx && sy > cy + 20 && su > cu && sv > cv && match[0] > match[1] * 1.1 && match[0] > match[2] * 1.2;
    Check(ok, "each cloud's shadow on the ground falls away from the sun (below and right of it), the same shapes where the sun's ray meets them");

    /* the layers part as the camera comes down: the high swells and climbs faster than the low (cloud everywhere) */
    CloudTileMake(2); CloudParse(g_ctile, sizeof g_ctile);
    CloudGrid(100, 0, -1, 0, 0, 0);
    float ratio[2];
    for (int z = 0; z < 2; z++) {
        CloudView(z ? 0.3f : 0, z ? 0.3f : 0, z ? 0.3f : 1);
        CloudMake(W, H, 5000000);
        float k[3];
        for (int part = 1; part < 3; part++) {
            ViewListStart();
            CloudDraw(part, W, H, 5000000);
            const ViewVert *q = g_vv + g_vb[0].first;
            k[part] = (q[1].x - q[0].x) / ((q[1].u - q[0].u) * CLOUD_TEX_W * g_cloudShown.part[part].du);   /* pixels a picture unit */
            ok &= g_vbN == 1 && g_vb[0].tex == VT_SKY && g_vb[0].add == 2;
        }
        ratio[z] = k[2] / k[1];
    }
    ok &= ratio[0] > 1.01f && ratio[1] > ratio[0] * 1.15f;
    Check(ok, "the high clouds swell past the low ones as the view comes down (parallax: the layers part), drawn premultiplied from the sky's picture");

    /* below the camera they are gone: the high first, then the low (a sky of cloud everywhere) */
    CloudTileMake(2); CloudParse(g_ctile, sizeof g_ctile);
    CloudView(0.4f, 0.4f, 0.2f); CloudMake(W, H, 5000000);
    ok = CloudSumA(2) > 0 && CloudSumA(1) > 0;
    CloudView(0.4f, 0.4f, 0.13f); CloudMake(W, H, 5000000);
    ok &= CloudSumA(2) == 0 && CloudSumA(1) > 0;
    CloudView(0.45f, 0.45f, 0.05f); CloudMake(W, H, 5000000);
    ok &= CloudSumA(1) == 0 && CloudSumA(0) == 0;
    Check(ok, "the camera coming down through them: the high clouds gone first, then the low (and their shadows)");

    /* parting over the arrival: at the end of the zoom, clear where you arrive, cloud further out */
    CloudView(0.44f, 0.44f, 0.12f);
    g_destOk = 1; g_destU = 0.5f; g_destV = 0.5f;
    CloudMake(W, H, 5000000);
    double nearA = 0, farA = 0;
    for (int j = 0; j < lo->h; j++)
        for (int i = 0; i < lo->w; i++) {
            float cu2 = lo->u0 + ((float)i + 0.5f) * lo->du, cv2 = lo->v0 + ((float)j + 0.5f) * lo->dv;
            float x = W / 2 + ((cu2 - g_cj.u0) / g_cj.us * W - W / 2) * g_cj.p[0], y = H / 2 + ((cv2 - g_cj.vt) / g_cj.vs * H - H / 2) * g_cj.p[0] - g_cj.lift[0];
            float d = sqrtf((x - W / 2) * (x - W / 2) + (y - H / 2) * (y - H / 2));
            if (d < g_cj.r0) nearA += CloudPx(1, i, j) >> 24;
            if (d > g_cj.r1) farA += CloudPx(1, i, j) >> 24;
        }
    double shade = 0;                                                  /* no shadow on the arrival either */
    const CloudPart *gp = &g_cloudShown.part[0];
    for (int j = 0; j < gp->h; j++)
        for (int i = 0; i < gp->w; i++) {
            float x = (gp->u0 + ((float)i + 0.5f) * gp->du - g_cj.u0) / g_cj.us * W, y = (gp->v0 + ((float)j + 0.5f) * gp->dv - g_cj.vt) / g_cj.vs * H;
            if ((x - W / 2) * (x - W / 2) + (y - H / 2) * (y - H / 2) < g_cj.r0 * g_cj.r0) shade += CloudPx(0, i, j) >> 24;
        }
    ok = g_cj.clear > 0.99f && nearA == 0 && farA > 0 && shade == 0 && CloudSumA(0) > 0;
    g_destOk = 0;
    CloudTileMake(0); CloudParse(g_ctile, sizeof g_ctile);
    Check(ok, "the clouds part over where you arrive as the view comes down onto it, and cast no shadow on it");

    /* blown on the wind of their latitude; every so often morphing into the next field, never jumping */
    CloudView(0, 0, 1);
    double moved = 0, still = 0;
    float e, n;
    CloudWind(0.5f - 45 / 76.0f, &e, &n);                             /* 45 N: the westerlies */
    for (int s = 0; s < 40; s++) {
        float u = 0.1f + 0.02f * s, v = 0.5f - 45 / 76.0f;
        DWORD t0 = 5000000 + 2000;                                    /* (early in its field's life: no morph) */
        int a = CloudThick(u, v, t0);
        int b = CloudThick(u + e * SKY_WIND * 1.0f / g_shipAsp, v - n * SKY_WIND * 1.0f, t0 + 1000), c = CloudThick(u, v, t0 + 1000);
        moved += fabs((double)(a - b)); still += fabs((double)(a - c));
    }
    ok = e > 0.3f && moved < still * 0.2;
    /* at v 0.3 the field turns over when T plus its phase (0.6 of an epoch) passes a whole number of epochs: tb ms */
    double ph = CLOUD_EPOCH * 0.6;
    DWORD tb = (DWORD)((ceil((5000 + ph) / CLOUD_EPOCH) * CLOUD_EPOCH - ph) * 1000 + 0.5), tm = tb - (DWORD)(CLOUD_MORPH * 1000);
    double across = 0, before = 0, other = 0;
    for (int s = 0; s < 64; s++) {
        float u = 0.05f + 0.014f * s;
        int a = CloudThick(u, 0.3f, tb - 1), b = CloudThick(u, 0.3f, tb + 1);
        across += fabs((double)(a - b));
        before += fabs((double)(CloudThick(u, 0.3f, tm + 1499) - CloudThick(u, 0.3f, tm + 1501)));     /* mid-morph */
        other += fabs((double)(CloudThick(u, 0.3f, tm - 2000) - CloudThick(u, 0.3f, tb + 1)));         /* a field and its next */
    }
    double start = 0, m0 = 0, m1 = 0, q0 = 0, q1 = 0;
    for (int s = 0; s < 256; s++) {
        float u = 0.01f + 0.0037f * s;
        start += fabs((double)(CloudThick(u, 0.3f, tm - 1) - CloudThick(u, 0.3f, tm + 1)));           /* the morph begins */
        double a = CloudThick(u, 0.3f, tm - 2000), b = CloudThick(u, 0.3f, tm + (DWORD)(CLOUD_MORPH * 500));
        m0 += a; q0 += a * a; m1 += b; q1 += b * b;
    }
    double sd0 = sqrt(q0 / 256 - (m0 / 256) * (m0 / 256)), sd1 = sqrt(q1 / 256 - (m1 / 256) * (m1 / 256));
    for (int s = 0; s <= 7; s++) {                                    /* the two fields' shares keep the spread: squares whole */
        CloudRow rr;
        float sx[2] = { 300, 2400 };
        CloudRowSet(&rr, 0.5f, 0.3f, (float)tm / 1000.0f + CLOUD_MORPH * (float)s / 8, g_shipAsp, sx, sx, 1, 0);
        ok &= Near(rr.ka * rr.ka + rr.kb * rr.kb, 65536, 1200) && rr.la + rr.lb == 256 && (s == 0 ? rr.kb == 0 : rr.kb > 0);
    }
    for (int s = 0; s < 20; s++) {                                    /* only some latitudes morphing at any moment */
        int morphing = 0, k;
        float age;
        for (int r = 0; r < 100; r++) morphing += CloudEpoch(0.005f + 0.01f * r, 5000.0f + 1.3f * s, &k, &age) > 0;
        ok &= morphing >= 8 && morphing <= 32;
    }
    ok &= across / 64 < 200 && before / 64 < 200 && other / 64 > 4000 && start / 256 < 200 && sd1 > 0.7 * sd0;
    double mean = 0;
    CloudMake(W, H, tb);
    memcpy(first, g_cloudPx[g_cloudFront], sizeof first);
    CloudMake(W, H, tb + 15);
    for (int i = 0; i < CLOUD_TEX_W * g_cloudShown.rows; i++) mean += fabs((double)(first[i] >> 24) - (double)(g_cloudPx[g_cloudFront][i] >> 24));
    ok &= mean / (CLOUD_TEX_W * g_cloudShown.rows) < 0.5;
    Check(ok, "the field drifts on the wind of its latitude (the westerlies carry it east), and morphs into the next one without a jump");

    /* the detail as the view comes down: none at the whole world, finer shapes coming down (the finer tile on) */
    CloudView(0, 0, 1); CloudMake(W, H, 5000000);
    ok = g_cj.detail == 0;
    CloudView(0.25f, 0.25f, 0.5f); CloudMake(W, H, 5000000);
    ok &= g_cj.detail == 0;                                          /* (still too fine there: it would speckle) */
    CloudView(0.44f, 0.44f, 0.1f);
    int edges[2];
    for (int d = 0; d < 2; d++) {
        g_cj.on = 0;
        CloudBegin(W, H, 5000000);
        if (!d) { ok &= g_cj.detail > 0; g_cj.detail = 0; }
        CloudWork(1); CloudSwap();
        edges[d] = 0;
        for (int j = 0; j < lo->h; j++)
            for (int i = 1; i < lo->w; i++) edges[d] += (CloudPx(1, i, j) >> 24 > 100) != (CloudPx(1, i - 1, j) >> 24 > 100);
    }
    ok &= edges[1] > edges[0] * 1.3;
    Check(ok, "the finer tile comes in as the view comes down, once its texels show its shapes: none at the whole world or half way, finer shapes near the end");

    /* the edges never sharper than a texel shows (no stairs where the picture is magnified): hard on the lit side as
     * designed where the texels are fine, widened to a texel's change of density where they are coarse */
    CloudTileMake(3); CloudParse(g_ctile, sizeof g_ctile);
    float litW[2];
    for (int z = 0; z < 2; z++) {
        CloudView(z ? 0.3f : 0, z ? 0.3f : 0, z ? 0.3f : 1);
        CloudMake(z ? W : 400, z ? H : 169, 5000000);
        float least = 1.0f * g_cloudSteep * ((float)g_cj.dx[0] / 65536 + 0.7f * (float)g_cj.dx[1] / 65536 * (float)g_cj.detail / 256);
        litW[z] = 64.0f / (float)g_cj.kw[255];
        ok = 64.0f / (float)g_cj.kw[0] >= litW[z] && Near(litW[z], least > g_cloudW[255] ? least : g_cloudW[255], 0.003);
    }
    ok &= litW[0] > 0.05f && litW[1] < litW[0] * 0.8f;
    int widths = ok;
    CloudTileMake(0); CloudParse(g_ctile, sizeof g_ctile);
    CloudView(0, 0, 1); CloudMake(5120, 2160, 5000000);                  /* (his screen: its coarsest texels) */
    int steps = 0, hard = 0;
    for (int j = 0; j < lo->h; j++)
        for (int i = 1; i < lo->w; i++) {
            int d = (int)(CloudPx(1, i, j) >> 24) - (int)(CloudPx(1, i - 1, j) >> 24);
            if (d < 0) d = -d;
            steps += d > 8; hard += d > 120;
        }
    ok = widths && steps > 100 && hard * 50 < steps;
    Check(ok, "the clouds' edges soft enough for their texels: no stairs at the whole world's view");

    /* between pictures the parts ride the wind (at the view's middle, the doldrums: westward) */
    ViewListStart(); CloudDraw(1, W, H, 5000000);
    float x0 = g_vv[g_vb[0].first].x;
    ViewListStart(); CloudDraw(1, W, H, 5000300);
    float x1 = g_vv[g_vb[0].first].x, want = g_cloudShown.windU * 0.3f / g_cj.us * W * g_cj.p[0];
    ok = g_cloudShown.windU < 0 && Near(x1 - x0, want, 0.01) && x1 < x0;
    Check(ok, "between its pictures the sky rides the wind of the view's middle, so the drift never steps");

    /* the picture shown is never touched while the next is made */
    CloudMake(W, H, 5000000);
    memcpy(first, g_cloudPx[g_cloudFront], sizeof first);
    g_cj.on = 0;
    CloudView(0.2f, 0.2f, 0.4f);
    CloudBegin(W, H, 5000500); CloudWork(1);
    ok = !memcmp(first, g_cloudPx[g_cloudFront], sizeof first);
    CloudSwap();
    ok &= memcmp(first, g_cloudPx[g_cloudFront], sizeof first) != 0 && g_cloudUpload;
    CloudView(0, 0, 1);
    Check(ok, "the next picture made beside the one shown (two of them), the shown one untouched until it is swapped");

    /* drawn each frame through the view: in their passes, the picture shown only while it is fresh */
    CloudGrid(100, 40, -1, 0, 0, 0);
    g_skyOn = 1; g_viewTex[VT_SKY] = 1; g_viewTex[VT_SPARK] = 1; g_cloudShownOk = 0; g_cj.on = 0;
    ViewListStart();
    SkyBuild(W, H, 6000000);                                          /* begun: nothing to draw yet */
    int none = 1;
    for (int b = 0; b < g_vbN; b++) none &= g_vb[b].tex != VT_SKY;
    CloudWork(1);
    ViewListStart();
    SkyBuild(W, H, 6000040);
    /* each of the sky's quads by the part of the picture it shows, in the order drawn (the shadows' and the low clouds'
     * are one batch, the same picture drawn the same way) */
    int order[4] = { -1, -1, -1, -1 }, last[4] = { -1, -1, -1, -1 }, rn = -1, seq = 0, pre = 1;
    for (int b = 0; b < g_vbN; b++) {
        if (g_vb[b].tex == -1 && !g_vb[b].add && rn < 0) rn = seq++;
        if (g_vb[b].tex != VT_SKY) continue;
        pre &= g_vb[b].add == 2;
        for (int q = g_vb[b].first; q < g_vb[b].first + g_vb[b].count; q += 4) {
            float tu = g_vv[q].u * CLOUD_TEX_W, tv = g_vv[q].v * CLOUD_TEX_H;
            int k = tv < g_cloudShown.part[0].y - 1 ? 1 : tu < g_cloudShown.part[2].x - 1 ? 0 : 2;
            if (order[k] < 0) order[k] = seq;
            last[k] = seq++;
        }
    }
    ok = none && pre && g_cloudUpload && order[0] == 0 && order[1] > last[0] && rn > last[1] && order[2] > rn;
    ViewListStart();
    SkyBuild(W, H, 6000040 + CLOUD_STALE + 1000);                      /* (much later, its picture not made yet) */
    none = 1;
    for (int b = 0; b < g_vbN; b++) none &= g_vb[b].tex != VT_SKY;
    ok &= none;
    Check(ok, "the weather drawn in its passes: the shadows, the low clouds, the rain out of their undersides, the high over all; a picture of another moment not drawn");

    /* rain only from the thick of the clouds */
    CloudGrid(10, 40, -1, 0, 0, 0);                                   /* (10%: cells born, but too few clouds to show) */
    int born = 0;
    SkyCloud cc;
    for (int i = 0; i < SKY_CLOUDS; i++) born += SkyAt(i, 6000000, W, H, &cc) && cc.rain;
    ViewListStart();
    SkyBuild(W, H, 6000000);
    int rain = 0;
    for (int b = 0; b < g_vbN; b++) rain += g_vb[b].tex == -1;
    ok = born > 0 && rain == 0;
    Check(ok, "rain and snow fall only from the thick of the clouds, none from a clear sky");

    /* the time a picture takes, at his screen's size with cloud everywhere and the detail in (the worst there is) */
    CloudTileMake(0); CloudParse(g_ctile, sizeof g_ctile);
    CloudGrid(100, 20, -1, 0, 0, 0);
    g_shipAsp = 5120.0f / 2160.0f;
    double best[2] = { 1e9, 1e9 };
    for (int z = 0; z < 2; z++) {
        CloudView(z ? 0.44f : 0, z ? 0.44f : 0, z ? 0.12f : 1);           /* (the whole world; the end, the detail in) */
        for (int r = 0; r < 12; r++) {
            double t0 = CloudMs();
            g_cj.on = 0;
            CloudBegin(5120, 2160, 5000000u + (DWORD)r * 1700u);
            CloudWork(1);
            double t = CloudMs() - t0;
            if (t < best[z]) best[z] = t;
        }
    }
#ifdef __OPTIMIZE__
    double budget = 3.0;
#else
    double budget = 6.0;                                               /* (this build unoptimised: the DLL is -O2) */
#endif
    printf("  the sky's picture at 5120 x 2160 (cloud everywhere): %.2f ms at the whole world, %.2f ms at the end of the zoom (%d + %d + %d texels)\n",
           best[0], best[1], g_cj.pic.part[1].w * g_cj.pic.part[1].h, g_cj.pic.part[0].w * g_cj.pic.part[0].h, g_cj.pic.part[2].w * g_cj.pic.part[2].h);
    Check(best[0] <= budget && best[1] <= budget, "the sky's picture made at 5120 x 2160 within its budget (3 ms optimised)");
    g_skyOn = 0; g_viewTex[VT_SKY] = 0; g_viewTex[VT_SPARK] = 0; g_sky.ok = 0; g_cloudOk = 0; g_cloudShownOk = 0; g_cj.on = 0;
}

/* The time of day (8 Oct 2026): the realm's clock (Central European, the EU's summer time), the light through the day by
 * its keys, eased, the world graded by it, the clouds lit and shadowed by it, the capitals' lights, the client's sun or
 * moon in the top right. */
static SYSTEMTIME TodUtc(int y, int mo, int d, int h, int mi, int s, int ms)
{
    SYSTEMTIME u;
    memset(&u, 0, sizeof u);
    u.wYear = (unsigned short)y; u.wMonth = (unsigned short)mo; u.wDay = (unsigned short)d; u.wHour = (unsigned short)h;
    u.wMinute = (unsigned short)mi; u.wSecond = (unsigned short)s; u.wMilliseconds = (unsigned short)ms;
    return u;
}
static void TestTimeOfDay(void)
{
    /* the clock: his minimap's 9:03 at 7:03 UTC (0:03 PDT, 7 Oct); the EU's summer time ends 25 Oct 2026, begins 29 Mar */
    SYSTEMTIME u = TodUtc(2026, 10, 7, 7, 3, 0, 0);
    int ok = Near(TodServerMinutes(&u), 9 * 60 + 3, 1e-3) && TodWeekday(2026, 10, 25) == 0 && TodWeekday(2026, 3, 29) == 0 &&
             TodWeekday(2000, 1, 1) == 6;
    u = TodUtc(2026, 10, 25, 0, 59, 0, 0); ok &= Near(TodServerMinutes(&u), 2 * 60 + 59, 1e-3);      /* still summer: 2:59 */
    u = TodUtc(2026, 10, 25, 1, 0, 0, 0);  ok &= Near(TodServerMinutes(&u), 2 * 60, 1e-3);           /* then 2:00 again */
    u = TodUtc(2026, 10, 24, 23, 0, 0, 0); ok &= TodOffset(&u) == 120;
    u = TodUtc(2026, 10, 26, 0, 0, 0, 0);  ok &= TodOffset(&u) == 60;
    u = TodUtc(2026, 3, 29, 0, 59, 0, 0);  ok &= Near(TodServerMinutes(&u), 1 * 60 + 59, 1e-3);       /* winter: 1:59 */
    u = TodUtc(2026, 3, 29, 1, 0, 0, 0);   ok &= Near(TodServerMinutes(&u), 3 * 60, 1e-3);            /* then 3:00 */
    u = TodUtc(2026, 3, 28, 12, 0, 0, 0);  ok &= TodOffset(&u) == 60;
    u = TodUtc(2026, 1, 15, 12, 0, 0, 0);  ok &= TodOffset(&u) == 60;
    u = TodUtc(2026, 7, 15, 23, 30, 0, 0); ok &= TodOffset(&u) == 120 && Near(TodServerMinutes(&u), 90, 1e-3);   /* past midnight */
    u = TodUtc(2026, 10, 7, 7, 3, 30, 500); ok &= Near(TodServerMinutes(&u), 9 * 60 + 3 + 30.5 / 60, 1e-3);    /* never steps */
    float keep = g_todForce;
    g_todForce = -1; shim_utc = TodUtc(2026, 10, 7, 19, 0, 0, 0);
    ok &= Near(TodMinutes(), 21 * 60, 1e-3);                                                  /* (the clock itself) */
    g_todForce = 1440 + 30; ok &= Near(TodMinutes(), 30, 1e-3);                               /* (an hour forced: any minutes) */
    g_todForce = keep;
    Check(ok, "the realm's clock: Central European time from UTC (his minimap's 9:03 at 7:03 UTC), summer time to 25 Oct and from 29 Mar 2026 at 01:00 UTC, its seconds in");

    /* the sun by day, the moon by night, as the client's minimap swaps them (5:30, 9 pm) */
    float f, up;
    ok = TodBody(329.9f, &f, &up) == 0 && TodBody(330, &f, &up) == 1 && Near(f, 0, 1e-4) && TodBody(1259.9f, &f, &up) == 1 &&
         f > 0.99f && TodBody(1260, &f, &up) == 0 && Near(f, 0, 1e-4);
    TodBody((330 + 1260) / 2.0f, &f, &up); ok &= Near(f, 0.5, 1e-3) && up > 0.99f;
    TodBody(0, &f, &up); ok &= f > 0.3f && f < 0.4f && up > 0.8f;
    Check(ok, "the sun from 5:30 to 9 pm and the moon through the night, as the client's minimap has them, each along its arc");

    /* the light through the day: the plain day at noon, never a jump from minute to minute, round midnight too */
    TodLight L, M;
    TodLightAt(720, &L);
    ok = L.west[0] == 1 && L.east[2] == 1 && L.lit[0] == 255 && L.lit[1] == 253 && L.base[2] == 110 && Near(L.shadow, CLOUD_SHADOW, 1e-6) &&
         L.turn == 0 && L.city == 0;
    float most = 0, mostC = 0;
    for (int m = 0; m < 1440 * 4; m++) {
        TodLightAt(m / 4.0f, &L); TodLightAt(m / 4.0f + 0.25f, &M);
        for (int c = 0; c < 3; c++) {
            float d = fabsf(L.west[c] - M.west[c]) + fabsf(L.east[c] - M.east[c]), e = fabsf(L.lit[c] - M.lit[c]) + fabsf(L.base[c] - M.base[c]);
            if (d > most) most = d;
            if (e > mostC) mostC = e;
        }
    }
    TodLightAt(1439.999f, &L); TodLightAt(0, &M);
    ok &= most < 0.02f && mostC < 5 && Near(L.west[2], M.west[2], 1e-3) && Near(L.lit[0], M.lit[0], 0.05);
    Check(ok, "the light through the day eased from key to key: the plain day at noon, never a jump in a quarter of a minute, midnight's join unseen");

    /* each key as he asked: dawn warm from the east, the afternoon gold, sunset amber and rose from the west, night blue */
    TodLight dawn, aft, sunset, night, day;
    TodLightAt(360, &dawn); TodLightAt(1020, &aft); TodLightAt(1230, &sunset); TodLightAt(0, &night); TodLightAt(720, &day);
    ok = dawn.east[0] > dawn.west[0] && dawn.east[0] > dawn.east[2] && dawn.lit[0] > dawn.lit[2] + 40;             /* dawn */
    ok &= aft.west[0] >= aft.east[0] && aft.lit[2] < day.lit[2] - 20 && aft.lit[0] > aft.lit[2] && aft.lit[1] > sunset.lit[1] &&
          aft.shadow > day.shadow;                                                                                  /* afternoon */
    ok &= sunset.west[0] > sunset.east[0] && sunset.west[0] > sunset.west[2] && sunset.lit[0] > sunset.lit[2] + 100;   /* sunset */
    ok &= night.west[2] > night.west[0] && night.west[0] < 0.45f && night.city > 0.9f && night.lit[2] > night.lit[0];  /* night */
    Check(ok, "dawn warm and low from the east, the afternoon gold and lengthening, sunset amber and rose from the west, night deep blue with the capitals lit");

    /* the clouds' sun: the map's own (upper left) through the day; from the east at dawn, the west at sunset, low and long */
    float x, y, len, x2, y2, len2;
    TodToward(720, &x, &y, &len);
    ok = Near(x, -0.766f, 0.01) && Near(y, -0.643f, 0.01) && Near(len, 0.47f, 0.01);
    TodToward(345, &x, &y, &len); ok &= x > 0.5f && len > 1.0f;
    TodToward(1245, &x, &y, &len); ok &= x < -0.9f && len > 1.0f;
    TodToward(1020, &x, &y, &len); TodToward(780, &x2, &y2, &len2); ok &= len > len2 && x <= x2 + 1e-4f;   /* afternoon */
    Check(ok, "the clouds' sun from the map's own upper left by day, rising in the east and setting in the west, their shadows long and away from it, lengthening through the afternoon");

    /* the world graded: the plain day untouched; dawn lighter in the east than the west; night darker and blue; names half */
    float W = 1600, H = 675;
    static const float whole[8] = { 0, 1, 1, 1, 0, 0, 1, 0 };
    memcpy(g_bgUV, whole, sizeof whole);
    g_viewTex[VT_LABELS] = 1;
    int lefts[3][3], names[3];
    float times[3] = { 720, 360, 0 };
    for (int k = 0; k < 3; k++) {
        g_todForce = times[k];
        ViewListStart(); TodSet();
        ViewRect(VT_MAP, 0, 0, W, H, 0, 0, 1, 1, 0xFFFFFFFFu);
        ViewRect(VT_LABELS, 100, 100, 120, 120, 0, 0, 1, 1, 0xFFFFFFFFu);
        TodGrade(W, 0, g_vvN);
        for (int c = 0; c < 3; c++) lefts[k][c] = g_vv[0].c[c];
        names[k] = g_vv[4].c[0];
        if (k == 1) ok = g_vv[1].c[0] > g_vv[0].c[0] && g_vv[1].c[0] > g_vv[1].c[2];         /* dawn: the east warmer */
    }
    ok &= lefts[0][0] == 255 && lefts[0][2] == 255 && lefts[2][0] < 110 && lefts[2][2] > lefts[2][0] + 40 && names[2] > lefts[2][0] + 40;
    g_todStyle &= ~TOD_GRADE; g_todForce = 0;
    ViewListStart(); TodSet(); ViewRect(VT_MAP, 0, 0, W, H, 0, 0, 1, 1, 0xFFFFFFFFu); TodGrade(W, 0, g_vvN);
    ok &= g_vv[0].c[0] == 255;
    g_todStyle |= TOD_GRADE;
    Check(ok, "the world map graded by the hour: the plain day as painted, dawn warmer in the east, night dark and blue, the names half as much; without the switch, as ever");

    /* the capitals' lights: by night, near each, warm and left as they are; none by day */
    g_map.ok = 1; g_map.m[0].ok = g_map.m[1].ok = 1;
    for (int c = 0; c < 2; c++) { g_map.m[c].au = 0.5f; g_map.m[c].bu = 1 / 40000.0f; g_map.m[c].av = 0.5f; g_map.m[c].bv = -1 / 40000.0f; }
    g_viewTex[VT_SPARK] = 1;
    g_todForce = 0;
    ViewListStart(); TodSet(); ViewRect(VT_MAP, 0, 0, W, H, 0, 0, 1, 1, 0xFFFFFFFFu);
    TodCityBuild(W, H, 1000);
    int lights = (g_todCityTo - g_todCityFrom) / 4, warm = g_todCityTo > g_todCityFrom ? g_vv[g_todCityFrom].c[0] : 0;
    float sx = (0.5f + 554.0f / 40000.0f) * W, sy = (0.5f + 8913.0f / 40000.0f) * H;                 /* (Stormwind, here) */
    float gx = (g_vv[g_todCityFrom].x + g_vv[g_todCityFrom + 1].x) / 2, gy = (g_vv[g_todCityFrom].y + g_vv[g_todCityFrom + 2].y) / 2;
    TodGrade(W, 0, g_vvN);
    ok = lights == TOD_CITIES * (1 + TOD_CITY_LIGHTS) && Near(gx, sx, 1) && Near(gy, sy, 1) && g_vv[g_todCityFrom].c[0] == warm &&
         warm == 0xFF && g_vb[g_vbN - 1].add == 1;
    g_todForce = 720;
    ViewListStart(); TodSet(); TodCityBuild(W, H, 1000);
    ok &= g_vvN == 0;
    Check(ok, "the capitals' lights by night: a warm glow and a few twinkling lights at each, never graded; none by day");

    /* the sun or moon in the top right: the client's own art, clear of the logo and the bar, crossing its arc */
    g_viewTex[VT_TIME] = 1;
    float px[4], py[4], pu[4];
    float at[4] = { 340, 795, 1250, 0 };
    for (int k = 0; k < 4; k++) {
        g_todForce = at[k];
        ViewListStart(); TodSet(); TodDiscBuild(W, H);
        ok &= g_vbN == 1 && g_vb[0].tex == VT_TIME && !g_vb[0].add;
        px[k] = (g_vv[0].x + g_vv[1].x) / 2; py[k] = (g_vv[0].y + g_vv[2].y) / 2; pu[k] = g_vv[0].u;
        ok &= px[k] > W - 0.3f * H && py[k] < 0.15f * H && g_vv[1].x <= W && g_vv[0].y >= 0 && Near(g_vv[1].x - g_vv[0].x, TOD_DISC_H * H, 0.5);
    }
    ok &= pu[0] < 0.5f && pu[1] < 0.5f && pu[2] < 0.5f && pu[3] > 0.5f;                 /* sun, sun, sun, moon */
    ok &= px[0] > px[1] && px[1] > px[2] && py[1] < py[0] && py[1] < py[2];              /* rising right (east), high at midday */
    g_todForce = 340; ViewListStart(); TodSet(); TodDiscBuild(W, H); float m1 = g_vv[0].x;
    g_todForce = 341; ViewListStart(); TodSet(); TodDiscBuild(W, H);
    ok &= fabsf(g_vv[0].x - m1) < 0.5f;                                                   /* a minute: hardly moves */
    g_todForce = 330.1f; ViewListStart(); TodSet(); TodDiscBuild(W, H);
    ok &= g_vbN == 0 || g_vv[0].c[3] < 40;                                               /* fading in as it rises */
    g_todStyle &= ~TOD_ARC;
    float still[2];
    for (int k = 0; k < 2; k++) { g_todForce = k ? 1000 : 400; ViewListStart(); TodSet(); TodDiscBuild(W, H); still[k] = g_vv[0].x; }
    ok &= still[0] == still[1] && g_vv[0].c[3] == 255;
    g_todStyle |= TOD_ARC;
    g_viewTex[VT_TIME] = 0;
    ViewListStart(); TodSet(); TodDiscBuild(W, H);
    ok &= g_vbN == 0;
    Check(ok, "the client's sun or moon in the top right (the moon by night), clear of the logo and the bar, crossing its arc east to west and highest at midday, fading in as it rises; still without the arc; none without its picture");

    /* the clouds by the hour: the plain day as they were; dawn's warm colours, its shadows west, its light from the east */
    CloudTileMake(1); CloudParse(g_ctile, sizeof g_ctile);
    CloudGrid(100, 0, -1, 0, 0, 0);
    g_shipAsp = W / H; g_destOk = 0; g_skyOn = 1;
    static unsigned plain[CLOUD_TEX_W * CLOUD_TEX_H];
    g_todAt = -1; g_tod = (TodLight){ { 1, 1, 1 }, { 1, 1, 1 }, { 255, 253, 247 }, { 200, 206, 222 }, { 80, 88, 110 }, .34f, 0, 0 };
    CloudMake(W, H, 5000000);
    memcpy(plain, g_cloudPx[g_cloudFront], sizeof plain);
    g_todForce = 720; TodSet(); CloudMake(W, H, 5000000);
    ok = !memcmp(plain, g_cloudPx[g_cloudFront], sizeof plain) && Near(g_cj.sunU, CLOUD_SUN_U, 0.01) && Near(g_cj.sunV, CLOUD_SUN_V, 0.01);
    g_todForce = 345; TodSet(); CloudMake(W, H, 5000000);
    ok &= g_cj.sunU > 0.4f && g_cj.turnX != 0;                                          /* the shadows' cloud east of them */
    double east = 0, west = 0, ne = 0, nw = 0, red = 0, blue = 0;
    const CloudPart *lp = &g_cloudShown.part[1];
    for (int j = 1; j < lp->h - 1; j++)
        for (int i = 1; i < lp->w - 1; i++) {
            unsigned q = CloudPx(1, i, j);
            int a = (int)(q >> 24);
            if (a < 60) continue;
            int gxa = (int)(CloudPx(1, i + 1, j) >> 24) - (int)(CloudPx(1, i - 1, j) >> 24);
            double l = ((q & 255) + (q >> 8 & 255) + (q >> 16 & 255)) / (3.0 * a);
            if (gxa < -4) { east += l; ne++; }                                                /* the east flanks */
            if (gxa > 4) { west += l; nw++; }
            red += (q & 255); blue += (q >> 16 & 255);
        }
    ok &= ne > 20 && nw > 20 && east / ne > west / nw + 0.05 && red > blue;
    Check(ok, "the clouds by the hour: at noon exactly as they were; at dawn warm, lit from the east (their east flanks brighter), their shadows thrown west");
    g_todForce = 720; g_skyOn = 0; g_sky.ok = 0; g_cloudOk = 0; g_cloudShownOk = 0; g_cj.on = 0; g_map.ok = 0;
    g_viewTex[VT_SPARK] = 0; g_viewTex[VT_LABELS] = 0;

    /* the client's own art is DXT3 (4 bits of alpha a texel): read and decoded */
    static unsigned char blp[1172 + 16];
    memset(blp, 0, sizeof blp);
    memcpy(blp, "BLP2", 4); blp[4] = 1; blp[8] = 2; blp[9] = 8; blp[10] = 1; blp[11] = 0;
    blp[12] = 4; blp[16] = 4; blp[20] = 0x94; blp[21] = 4; blp[84] = 16;               /* 4 x 4, one level at 1172 */
    unsigned char *k = blp + 1172;
    k[0] = 0x0F; k[1] = 0xF0;                                                            /* texels 0, 3 opaque */
    k[8] = 0x00; k[9] = 0xF8; k[10] = 0x1F; k[11] = 0x00;                                /* red, blue */
    BlpInfo b;
    unsigned char rgba[64];
    ok = BlpParse(blp, sizeof blp, &b) && b.dxt == 3 && b.w == 4 && b.levels == 1;
    DxtDecode(blp + b.off[0], 4, 4, 3, rgba);
    ok &= rgba[3] == 255 && rgba[7] == 0 && rgba[11] == 0 && rgba[15] == 255 && rgba[0] == 255 && rgba[2] == 0;
    Check(ok, "the client's minimap art read as it is: DXT3, its alpha four bits a texel");
}

/* Slice 5 (9 Oct): the view on by default. view-off.txt is the player's switch, ahead of all; on Windows itself a game
 * holding the whole screen keeps it off, unless view.txt asks for it; under Wine, or in a window, it runs. */
static void Unput(const char *rel) { char p[800]; snprintf(p, sizeof p, "%s%s", shim_gamedir, rel); remove(p); }
static void TestViewDefault(void)
{
    const char *why = NULL;
    Check(ViewWanted(0, 0, 1, 0, &why) && ViewWanted(0, 0, 0, 1, &why) && ViewWanted(0, 1, 0, 0, &why) &&
          ViewWanted(0, 1, 1, 1, &why), "view: on under Wine, in a window, or asked for by view.txt");
    why = NULL;
    Check(!ViewWanted(0, 0, 0, 0, &why) && why && strstr(why, "exclusive full screen") && strstr(why, "view.txt"),
          "view: off on Windows when the game holds the whole screen, and the log says why and what turns it on");
    why = NULL;
    Check(!ViewWanted(1, 0, 1, 1, &why) && why && strstr(why, "view-off.txt"), "view: view-off.txt turns it off under Wine, windowed");
    why = NULL;
    Check(!ViewWanted(1, 1, 0, 1, &why) && why && strstr(why, "view-off.txt"), "view: view-off.txt beats view.txt");

    Fresh("view-default");
    GameDir();
    shim_wine = 0; g_gameWindowed = 0;
    Check(!ViewGate(&why), "view gate: Windows, the whole screen, no files: off");
    Put("Data/CinderLoad/view.txt", "x");
    Check(ViewGate(&why), "view gate: view.txt there: on even so");
    Put("Data/CinderLoad/view-off.txt", "x");
    Check(!ViewGate(&why), "view gate: view-off.txt beats view.txt");
    Unput("Data/CinderLoad/view.txt");
    shim_wine = 1;
    Check(!ViewGate(&why), "view gate: view-off.txt under Wine too");
    Unput("Data/CinderLoad/view-off.txt");
    Check(ViewGate(&why) && RunsUnderWine(), "view gate: under Wine with no files: on (Wine found by wine_get_version)");
    shim_wine = 0;
    Check(!RunsUnderWine(), "Wine: not without wine_get_version");
    g_gameWindowed = 1;
    Check(ViewGate(&why), "view gate: Windows in a window: on");

    /* where g_gameWindowed comes from: the screen choice reads Config.wtf and the command line */
    Put("WTF/Config.wtf", "SET gxResolution \"1920x1080\"\nSET gxWindow \"1\"\n");
    g_gameWindowed = 0; ChooseScreens();
    Check(g_gameWindowed == 1, "the screen choice: gxWindow 1 = a window");
    Put("WTF/Config.wtf", "SET gxResolution \"1920x1080\"\n");
    ChooseScreens();
    Check(g_gameWindowed == 0, "the screen choice: no gxWindow line, a client with another default: the whole screen");
    shim_module[GXWINDOW_DEFAULT_VA - 0x400000u] = 0xEB; shim_module[GXWINDOW_DEFAULT_VA - 0x400000u + 1] = 0x05;
    ChooseScreens();
    Check(g_gameWindowed == 1, "the screen choice: no gxWindow line in RavenCraft's client (its default \"1\"): a window");
    Put("WTF/Config.wtf", "SET gxResolution \"1920x1080\"\nSET gxWindow \"0\"\n");
    ChooseScreens();
    Check(g_gameWindowed == 0, "the screen choice: gxWindow 0 in RavenCraft's client: the whole screen");
    shim_module[GXWINDOW_DEFAULT_VA - 0x400000u + 1] = 0x06;
    Put("WTF/Config.wtf", "SET gxResolution \"1920x1080\"\n");
    ChooseScreens();
    Check(g_gameWindowed == 0, "the screen choice: other bytes where the default is chosen: the whole screen");
    char keep[512];
    strcpy(keep, shim_cmdline); strcat(shim_cmdline, " -windowed");
    ChooseScreens();
    Check(g_gameWindowed == 1, "the screen choice: -windowed = a window");
    strcpy(shim_cmdline, keep);
    g_gameWindowed = 0;
}

/* Vanilla's own bar (the parchment style's, slice 1; his pick 10 Oct 2026, "b4", after four rounds of mocks): the
 * client's frame and glass at the art's own proportions, 512 x 64 texels at H / 768, centred, its centre 0.075 of the
 * height up (the client's own); the fill the class's colour with raw heat and no white, the edge lit and a halo past
 * it, full a sweep and the metal's flash; no spark (his "no spark"); bar-vanilla.txt asks for it, the client's newest
 * copies read, ours passed over. */
static int VBarPure(const ViewVert *v, unsigned rgb, float *k)       /* a corner in the class's colour, k of it, no white mixed in */
{
    float r = (float)(rgb >> 16 & 255), g = (float)(rgb >> 8 & 255), b = (float)(rgb & 255), m = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float c[3] = { v->c[0], v->c[1], v->c[2] }, t[3] = { r, g, b }, top = m == r ? c[0] : m == g ? c[1] : c[2];
    *k = top / m;
    for (int i = 0; i < 3; i++) if (fabsf(c[i] - t[i] * *k) > 1.6f || c[i] > t[i] * 1.06f + 1) return 0;   /* (hot: up to 1.05 of it, as his mock) */
    return 1;
}
static void TestVanillaBar(void)
{
    float s, bx, by;
    VBarLayout(1920, 1080, &s, &bx, &by);
    int ok = Near(s, 1080.0 / 768, 1e-5) && Near(bx, 600, 1e-3) && Near(by + 32 * s, 1080 - 81, 1e-3);
    VBarLayout(5120, 2160, &s, &bx, &by);
    ok &= Near(512 * s, 1440, 1e-2) && Near(64 * s, 180, 1e-2) && Near(bx, 1840, 1e-2) && Near(by + 32 * s, 2160 - 162, 1e-2);
    VBarLayout(600, 1000, &s, &bx, &by);
    ok &= Near(512 * s, 576, 1e-2) && Near(bx, 12, 1e-2);
    Check(ok, "vanilla's bar: the art's own proportions at the UI's height scale (720 x 90 at 1080, 1440 x 180 on his 2160), centred, its centre the client's own 0.075 of the height up; a window narrower than 2:3, as wide as it fits");

    /* the heat: it drifts toward the tip, its shape kept; deep and full both in it, as in his mock */
    int drift = 1, dark = 0, full = 0, n = 0;
    for (int i = 0; i < 40; i++) {
        float x = 11.0f * i, y = (float)(i % 29), t = 0.37f * i;
        drift &= Near(VBarHeat(x + VBAR_DRIFT * 1.5f, y, t + 1.5f), VBarHeat(x, y, t), 1e-4);
    }
    drift &= !Near(VBarHeat(0, 0, 0), VBarHeat(60, 0, 0), 0.02) || !Near(VBarHeat(0, 0, 0), VBarHeat(130, 9, 0), 0.02);
    for (int t = 0; t < 40; t++) for (int x = 0; x < 456; x += 4) for (int y = 0; y <= 28; y += 7) {
        float l = VBarLevel(VBarHeat((float)x, (float)y, t * 3.7f));
        dark += l < 0.35f; full += l >= 1.0f; n++;
    }
    Check(drift && dark > n / 14 && dark < n / 5 && full > n / 4 && full < n / 2,
          "vanilla's bar: its heat drifts toward the tip, its shape kept; dark over about an eighth of the fill and the class's colour at full over about two fifths (his mock's)");

    /* a warlock's at 60%: the map, then the channel, the fill and the halo, the glass over the channel, the frame */
    static const float uv[8] = { 0.2f, 0.7f, 0.6f, 0.7f, 0.2f, 0.3f, 0.6f, 0.3f };
    memcpy(g_bgUV, uv, sizeof uv);
    g_ovOn = 0; g_logoOn = 0; g_frameOn = 0; g_class = 9;
    for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
    g_barVanilla = 1; g_viewTex[VT_VBORDER] = g_viewTex[VT_VGLASS] = 1; g_vbFullAt = 0;
    ViewBuild(1920, 1080, 0.6f, 5000);
    VBarLayout(1920, 1080, &s, &bx, &by);
    float front = 0.6f * 456, cx0 = bx + 28 * s;
    int cols = (int)(front / 4) + 1, fillQ = cols * VBAR_ROWS, haloQ = 15;
    const ViewVert *gl = g_vv + g_vb[2].first, *bd = g_vv + g_vb[3].first, *ch = g_vv + g_vb[1].first;
    ok = g_vbN == 4 && g_vb[0].tex == VT_MAP && g_vb[1].tex == -1 && !g_vb[1].add && g_vb[1].count == 4 * (1 + fillQ + haloQ) &&
         g_vb[2].tex == VT_VGLASS && !g_vb[2].add && g_vb[2].count == 4 && g_vb[3].tex == VT_VBORDER && !g_vb[3].add && g_vb[3].count == 4 &&
         Near(bd[0].x, 600, 1e-3) && Near(bd[2].x, 1320, 1e-3) && Near(bd[0].y, by, 1e-3) && Near(bd[2].y, by + 90, 1e-3) &&
         bd[0].u == 0 && bd[0].v == 0 && bd[2].u == 1 && bd[2].v == 1 && bd[0].c[0] == 255 && bd[0].c[3] == 255 &&
         Near(gl[0].x, cx0, 1e-3) && Near(gl[2].x, cx0 + 456 * s, 1e-3) && Near(gl[0].y, by + 18 * s, 1e-3) && Near(gl[2].y, by + 46 * s, 1e-3) &&
         Near(gl[0].u, 28.0 / 512, 1e-6) && Near(gl[2].u, 484.0 / 512, 1e-6) && Near(gl[0].v, 18.0 / 64, 1e-6) && Near(gl[2].v, 46.0 / 64, 1e-6) &&
         Near(ch[0].x, cx0, 1e-3) && Near(ch[1].x, cx0 + 456 * s, 1e-3) && ch[0].c[0] == 13 && ch[0].c[3] == 255;
    const ViewVert *last = ch + 4 * fillQ;                             /* (the last column's quad, its bottom band) */
    ok &= Near(last[1].x, cx0 + front * s, 1e-2) && Near(ch[4].x, cx0, 1e-3);
    Check(ok, "vanilla's bar, filling: the map, then its dark channel, the fill in columns up to the front and a halo past it, the client's glass over the channel only and its frame over all; no blade, no spark, nothing else");

    /* the fill: the class's colour, k of it, never whiter (his "b4"), away from the lit edge; deep and full both seen */
    int pure = 1, edge = 0, halo = 0;
    float kmin = 9, kmax = 0;
    for (int f = 0; f < 6; f++) {
        ViewBuild(1920, 1080, 0.6f, 5000 + 12000 * (DWORD)f);
        ch = g_vv + g_vb[1].first;
        for (int q = 1; q <= fillQ; q++)
            for (int j = 0; j < 4; j++) {
                const ViewVert *v = ch + 4 * q + j;
                float k, xt = (v->x - cx0) / s;
                if (xt > front - 3 * VBAR_EDGE) continue;
                pure &= VBarPure(v, 0x9482C9, &k);
                if (k < kmin) kmin = k;
                if (k > kmax) kmax = k;
            }
        const ViewVert *tip = ch + 4 * fillQ + 1;                        /* the front: the edge lit from within (paler) */
        edge += (tip->c[0] + tip->c[1]) / (float)tip->c[2] > (148.0f + 130.0f) / 201.0f + 0.08f;
        const ViewVert *h0 = ch + 4 * (1 + fillQ), *hn = ch + 4 * (fillQ + haloQ) + 1;   /* the halo: lit at the front, dark at its end */
        halo += h0->c[2] > 13 + 40 && h0->c[2] > h0->c[1] && hn->c[0] <= 14 && hn->c[2] <= 16;
    }
    Check(pure && kmin < 0.35f && kmax > 0.99f, "vanilla's bar: the fill is the class's colour, dark where the heat is cool and at full where it is hot, never whiter than the class's colour (his \"b4\": no white)");
    Check(edge == 6 && halo == 6, "vanilla's bar: (b) its front lit from within, paler, and a faint halo in the class's light past it, fading to the dark channel");

    /* the same for every class, only its colour: a hunter's at the same moment */
    ViewBuild(1920, 1080, 0.6f, 5000);
    static ViewVert wl[4 * 600];
    int nv = g_vb[1].count < 4 * 600 ? g_vb[1].count : 4 * 600;
    memcpy(wl, g_vv + g_vb[1].first, sizeof(ViewVert) * (size_t)nv);
    g_class = 3;
    ViewBuild(1920, 1080, 0.6f, 5000);
    int same = g_vb[1].count == nv, recol = 0;
    for (int i = 0; same && i < nv; i++) {
        same &= wl[i].x == g_vv[g_vb[1].first + i].x && wl[i].y == g_vv[g_vb[1].first + i].y;
        recol |= memcmp(wl[i].c, g_vv[g_vb[1].first + i].c, 4) != 0;
    }
    Check(same && recol, "vanilla's bar: the same for every class, only its colour the class's (a hunter's where the warlock's was)");

    /* full: a last sweep runs the channel, then the frame flashes in the class's light, then settles; a new screen afresh */
    g_class = 9;
    ViewBuild(1920, 1080, 0.9f, 10000);
    ViewBuild(1920, 1080, 1.0f, 10500);                                  /* it fills */
    ViewBuild(1920, 1080, 1.0f, 10700);                                  /* 0.2 s on: the sweep a third of the way */
    ch = g_vv + g_vb[1].first;
    cols = 456 / 4;
    int swept = 0, calm = 1;
    for (int q = 1; q <= cols * VBAR_ROWS; q++) {
        const ViewVert *v = ch + 4 * q;
        float k, xt = (v->x - cx0) / s;
        if (fabsf(xt - 152) < 8) swept |= !VBarPure(v, 0x9482C9, &k);
        if (xt > 260 && xt < 456 - 3 * VBAR_EDGE) calm &= VBarPure(v, 0x9482C9, &k);
    }
    int noFlash = g_vbN == 4;
    ViewBuild(1920, 1080, 1.0f, 11100);                                  /* 0.6 s on: the flash */
    int flash = g_vbN == 5 && g_vb[4].tex == VT_VBORDER && g_vb[4].add == 1 && g_vb[4].count == 8 && g_vv[g_vb[4].first].c[3] == 255;
    ViewBuild(1920, 1080, 1.0f, 14500);                                  /* 4 s on: settled */
    int settled = g_vbN == 4;
    ViewBuild(1920, 1080, 0.3f, 15000);                                  /* a new screen */
    Check(swept && calm && noFlash && flash && settled && g_vbFullAt == 0,
          "vanilla's bar, full: a last sweep runs the channel (paler where it passes, the class's colour elsewhere), then the frame flashes in the class's light (added, past full strength: twice), then settles; a new screen begins it afresh");

    g_barVanilla = 0;
    ViewBuild(1920, 1080, 0.6f, 5000);
    int blade = 1;
    for (int b = 0; b < g_vbN; b++) blade &= g_vb[b].tex != VT_VBORDER && g_vb[b].tex != VT_VGLASS;
    Check(blade, "vanilla's bar off (no bar-vanilla.txt, or its pictures never reached OpenGL): the blade as it was");
    g_viewTex[VT_VBORDER] = g_viewTex[VT_VGLASS] = 0; g_class = 0; g_vbFullAt = 0;

    /* reading it (whichever bar is chosen: it may be chosen in game at any time): each picture the client's newest copy,
     * ours (patch-~) and a zero-length stub passed over, the base archive its last word; the log says where each came from */
    static const char *const names[2] = { VBAR_BORDER_PATH, VBAR_GLASS_PATH };
    unsigned char *vblp[VT_COUNT] = { 0 };
    unsigned vlen[VT_COUNT] = { 0 };
    Fresh("vanilla-bar");
    GameDir();
    VBarRead(vblp, vlen);
    ok = !vblp[VT_VBORDER] && !vblp[VT_VGLASS] && LogSays("not read: in none of the client's archives");
    static const char *const inBase[2] = { "base-border", "base-glass" }, *const inFive[1] = { "patch5-border" };
    static const char *const ours[2] = { "ours-border", "ours-glass" }, *const stub[1] = { "" };
    MakeMpq("Data/interface.MPQ", 2, names, inBase, NULL);
    MakeMpq("Data/patch-5.MPQ", 1, names, inFive, NULL);
    MakeMpq("Data/patch-~.MPQ", 2, names, ours, NULL);
    MakeMpq("Data/patch-9.MPQ", 1, names + 1, stub, NULL);
    VBarRead(vblp, vlen);
    ok &= vblp[VT_VBORDER] && vlen[VT_VBORDER] == 13 && !memcmp(vblp[VT_VBORDER], "patch5-border", 13) &&
          vblp[VT_VGLASS] && vlen[VT_VGLASS] == 10 && !memcmp(vblp[VT_VGLASS], "base-glass", 10) &&
          LogSays("Loading-BarBorder.blp read from patch-5.MPQ") && LogSays("Loading-BarGlass.blp read from interface.MPQ");
    for (int i = VT_VBORDER; i <= VT_VGLASS; i++) { if (vblp[i]) HeapFree(GetProcessHeap(), 0, vblp[i]); vblp[i] = NULL; }
    Fresh("vanilla-bar-ours-only");
    GameDir();
    MakeMpq("Data/patch-~.MPQ", 2, names, ours, NULL);
    VBarRead(vblp, vlen);
    ok &= !vblp[VT_VBORDER] && !vblp[VT_VGLASS] && LogSays("not read: in none of the client's archives");
    Check(ok, "vanilla's bar read as the view starts, whichever bar is chosen (no switch file needed); the client's newest copy of each picture (patch-5 over the base), ours and a zero-length stub passed over; none of the client's: not read, and the log says so");
}

/* The parchment (his "fix it", 10 Oct 2026: he had seen only the living map under the new bar): the client's world map
 * tables read, the arrival's zone found (the smallest box holding it; else the continent), its painting made from its
 * twelve tiles with the seams blended and every overlay piece laid on, then drawn by candlelight, easing in toward the
 * arrival, fading up once it is made, with vanilla's bar; no living map under it. */
static unsigned char g_tb[1 << 18];
static unsigned TestWdbc(unsigned char *out, int rows, int fields, const unsigned *vals, const char *strs, unsigned sl)
{
    unsigned head[5] = { 0x43424457u, (unsigned)rows, (unsigned)fields, 4u * (unsigned)fields, sl };   /* "WDBC" */
    memcpy(out, head, 20);
    memcpy(out + 20, vals, 4u * (unsigned)(rows * fields));
    memcpy(out + 20 + 4 * rows * fields, strs, sl);
    return 20 + 4u * (unsigned)(rows * fields) + sl;
}
static unsigned TestF(float f) { unsigned u; memcpy(&u, &f, 4); return u; }
static unsigned char *TestPalBlp(unsigned w, unsigned h, unsigned rgb, int alphaBits, unsigned *len)
{
    unsigned n = 1172 + w * h + (w * h * (unsigned)alphaBits + 7) / 8;
    unsigned char *d = calloc(1, n);
    memcpy(d, "BLP2", 4); d[4] = 1; d[8] = 1; d[9] = (unsigned char)alphaBits; d[10] = 8; d[11] = 0;
    unsigned v[2] = { w, h }, off = 1172, l = w * h + (w * h * (unsigned)alphaBits + 7) / 8;
    memcpy(d + 12, v, 8); memcpy(d + 20, &off, 4); memcpy(d + 84, &l, 4);
    d[148] = (unsigned char)(rgb & 255); d[149] = (unsigned char)(rgb >> 8 & 255); d[150] = (unsigned char)(rgb >> 16 & 255);
    if (alphaBits) memset(d + 1172 + w * h, 0xFF, (w * h * (unsigned)alphaBits + 7) / 8);   /* (index 0 everywhere, opaque) */
    *len = n;
    return d;
}
/* ---- the glide's helpers (his rule, 10 Oct 2026: one smooth pan and zoom from the world over the Maelstrom to the
 * character's own spot, on the even bar) ---- */
static void TestGlideState(void)
{
    for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
    g_wmc[0].ok = g_wmc[1].ok = 1; g_wmc[0].ox = 14.5f; g_wmc[0].oy = -7; g_wmc[0].s = 0.75f;
    g_wmc[1].ox = -19; g_wmc[1].oy = -0.322f; g_wmc[1].s = 0.75f;
    g_worldFrameOk = 1; g_parchFade = PARCH_FADE_A; g_parchOn = 1;
    g_worldUp = 0; g_worldTW = 0; g_worldK = 1; g_worldAt = 0;
    g_parchZone = g_parchCont = -1; g_contMatch = g_zoneMatch = -1;
    g_parchForScreen = -1; g_parchForMap = -2; g_parchWant = g_parchWantCont = -1;
    g_parchScrSeen = -1; g_glide.at = 0; g_glide.tau = 0; g_glide.u0 = 0;
    g_destMap = -1;
    g_ptry.world = g_ptry.cont = g_ptry.zone = 0; g_pbitsSeq = -1; g_pmadeKnown = -1;
    g_lpWorld.ok = g_lpCont.ok = 0;
}
/* this screen's arrival taken (zone want, its continent cont; want a continent's row: at sea), both painted and up */
static void TestGlideArrive(int map, int want, int cont, float x, float y, DWORD at)
{
    g_destMap = map; g_destWX = x; g_destWY = y;
    g_parchForMap = map; g_parchForX = x; g_parchForY = y; g_parchForScreen = g_viewScreen; g_parchAt = at;
    g_parchWant = want; g_parchWantCont = cont;
    g_parchCont = cont; g_viewTex[VT_PARCH2] = 1; g_contTW = 2048; g_contK = 2; g_contAt = 0;   /* (HD paintings) */
    g_parchZone = g_pz[want].area ? want : -1; g_viewTex[VT_PARCH] = 1; g_zoneTW = 2048; g_zoneK = 2; g_zoneAt = 0;
    g_glide.at = 0;
}
static int TestBatch(int tex) { for (int b = 0; b < g_vbN; b++) if (g_vb[b].tex == tex) return b; return -1; }
static const ViewVert *TestVertNear(int b, float x, float y)
{
    static const ViewVert none = { -1e9f, -1e9f, 0, 0, { 0, 0, 0, 0 } };
    if (b < 0) return &none;
    const ViewVert *best = &g_vv[g_vb[b].first];
    float bd = 1e30f;
    for (int i = g_vb[b].first; i < g_vb[b].first + g_vb[b].count; i++) {
        float d = (g_vv[i].x - x) * (g_vv[i].x - x) + (g_vv[i].y - y) * (g_vv[i].y - y);
        if (d < bd) { bd = d; best = &g_vv[i]; }
    }
    return best;
}
/* the glide's time for its eased progress e (the bar, with the glide begun at an empty bar) */
static float TestTauFor(float e)
{
    float lo = 0, hi = 1;
    for (int i = 0; i < 60; i++) { float m = 0.5f * (lo + hi); if (ParchEase(m) < e) lo = m; else hi = m; }
    return 0.5f * (lo + hi);
}
static void TestHoldStub(void) {}
static int g_upOrder[16], g_upN, g_upFail;
static int TestParchUp(int slot)
{
    if (g_upN < 16) g_upOrder[g_upN++] = slot;
    if (g_upFail || !ParchMips()) return 0;
    g_viewTex[slot] = 1;
    return 1;
}

static const unsigned char *TestPx(int x, int y) { return g_parchPx + 4 * ((unsigned)y * (unsigned)g_parchTW + (unsigned)x); }
static void TestParchment(void)
{
    /* the tables: a continent, a zone, a smaller zone inside it, a zone on the other continent, and a bad box; pieces */
    static const char wstr[] = "\0Azeroth\0Elwynn\0Inner\0Durotar\0Bad\0";
    unsigned wv[5 * 8] = { 14, 0, 0, 1, TestF(16000), TestF(-19200), TestF(7466), TestF(-16000),
                           30, 0, 12, 9, TestF(1535), TestF(-1935), TestF(-7939), TestF(-10254),
                           99, 0, 77, 16, TestF(200), TestF(-200), TestF(-9000), TestF(-9400),
                           4, 1, 14, 22, TestF(-1962), TestF(-7250), TestF(1808), TestF(-1716),
                           5, 0, 15, 30, TestF(-5), TestF(5), TestF(10), TestF(20) };
    static unsigned char wma[512], wmo[512];
    unsigned wn = TestWdbc(wma, 5, 8, wv, wstr, sizeof wstr);
    static const char ostr[] = "\0PIECE\0OTHER\0NONE\0";
    unsigned ov[3 * 17] = { 0 };
    unsigned o0[13] = { 1, 30, 0, 0, 0, 0, 0, 0, 1, 300, 200, 100, 50 }, o1[13] = { 2, 4, 0, 0, 0, 0, 0, 0, 7, 256, 256, 0, 0 },
             o2[13] = { 3, 30, 0, 0, 0, 0, 0, 0, 13, 0, 128, 0, 0 };
    memcpy(ov, o0, sizeof o0); memcpy(ov + 17, o1, sizeof o1); memcpy(ov + 34, o2, sizeof o2);
    unsigned on = TestWdbc(wmo, 3, 17, ov, ostr, sizeof ostr);
    ParchTables(wma, wn, wmo, on);
    int ok = g_pzN == 4 && g_ppN == 2 && !strcmp(g_pz[1].name, "Elwynn") && g_pz[1].id == 30 && Near(g_pz[1].l, 1535, 1e-3) &&
             !strcmp(g_pp[0].tex, "PIECE") && g_pp[0].zone == 30 && g_pp[0].w == 300 && g_pp[0].ox == 100 && g_pp[0].oy == 50;
    ParchTables(NULL, 0, wmo, 7);
    ok &= g_pzN == 0 && g_ppN == 0;                                   /* (no tables: nothing; a short one: nothing) */
    ParchTables(wma, wn, wmo, on);
    int elw = ParchFind(0, -9464, 62), inner = ParchFind(0, -9200, 0), cont = ParchFind(0, -500, 500), none = ParchFind(5, 0, 0);
    ok &= elw == 1 && inner == 2 && cont == 0 && none == -1 && ParchFind(1, 0, -3000) == 3;
    Check(ok, "the parchment's tables: the client's WorldMapArea (zones' boxes, a bad one left out) and WorldMapOverlay (pieces, an empty one left out); the arrival's zone the smallest box holding it, else its continent, else none");

    /* its painting: the twelve tiles (each its own colour), the seams blended, the zone's piece laid on (two tiles wide,
     * cut at its 300 texels) from a newer archive; another zone's piece and a missing tile */
    Fresh("parchment");
    GameDir();
    static const char *tnames[12];
    static char tbuf[12][64];
    const char *tdata[12];
    unsigned tlen[12];
    for (int i = 0; i < 12; i++) {
        snprintf(tbuf[i], sizeof tbuf[i], "Interface\\WorldMap\\Elwynn\\Elwynn%d.blp", i + 1);
        tnames[i] = tbuf[i];
        tdata[i] = (const char *)TestPalBlp(256, 256, (unsigned)(40 + 15 * i) << 16 | 0x4020, 0, &tlen[i]);
    }
    MakeMpq("Data/interface.MPQ", 12, tnames, tdata, tlen);
    static const char *pnames[2] = { "Interface\\WorldMap\\Elwynn\\PIECE1.blp", "Interface\\WorldMap\\Elwynn\\PIECE2.blp" };
    const char *pdata[2];
    unsigned plen[2];
    for (int i = 0; i < 2; i++) pdata[i] = (const char *)TestPalBlp(256, 256, 0xFFFFFF, 1, &plen[i]);
    MakeMpq("Data/patch-5.MPQ", 2, pnames, pdata, plen);
    const char *why = "";
    ok = ParchMake(1, &why) && g_parchK == 1 && g_parchTW == 1024 && g_parchPieces == 1 && g_parchMade == 1;
    const unsigned char *c1 = TestPx(40, 40), *c6 = TestPx(256 + 128, 256 + 128);   /* (clear of the piece and the seams) */
    ok &= c1[2] == 0x20 && c1[1] == 0x40 && c1[0] == 40 && c6[0] == 40 + 15 * 5 && c6[3] == 255;   /* (RGBA; well away from the seams) */
    const unsigned char *in1 = TestPx(110, 60), *in2 = TestPx(390, 60), *out1 = TestPx(406, 60), *below = TestPx(110, 260);
    ok &= in1[0] == 255 && in1[1] == 255 && in2[0] == 255 && out1[1] != 255 && below[1] != 255;
    int step = abs((int)TestPx(255, 400)[0] - (int)TestPx(256, 400)[0]), far = TestPx(256 + 40, 400)[0] == 40 + 15 * 5;
    ok &= step <= 3 && far;
    Check(ok, "the parchment's painting: the zone's twelve tiles in their places, the tiles' seams blended (the step across one gone, its colour unchanged further off), its overlay piece laid on from the newest archive, two tiles wide and cut at its own width, no other zone's");
    MakeMpq("Data/interface.MPQ", 11, tnames, tdata, tlen);           /* (its twelfth tile gone) */
    ok = !ParchMake(1, &why) && strstr(why, "twelve") && g_parchMade == -1;
    ParchMake(1, &why);
    MakeMpq("Data/interface.MPQ", 12, tnames, tdata, tlen);
    ok &= ParchMake(1, &why);
    Check(ok, "the parchment's painting: a tile missing, none made, and why");
    static unsigned char foundNone[EXPLORED_BYTES];                      /* this character has found nothing: its piece left blank */
    g_makeBits = foundNone;
    ok = ParchMake(1, &why) && g_parchPieces == 0 && g_parchAll == 1 && TestPx(110, 60)[1] != 255;
    g_makeBits = NULL;
    ok &= ParchMake(1, &why) && g_parchPieces == 1 && g_parchAll == 1 && TestPx(110, 60)[1] == 255;
    Check(ok, "the parchment's painting: the pieces this screen's character has not found left blank, every piece when its explored areas are not known");
    for (int i = 0; i < 12; i++) free((void *)tdata[i]);
    for (int i = 0; i < 2; i++) free((void *)pdata[i]);

    /* drawn (the glide, his rule 10 Oct: from the world over the Maelstrom to the character's own spot, one smooth pan
     * and zoom on the even bar): the dark paper, the world's painting coming up over it as the screen begins, on the
     * opening until the arrival is taken; then the one zoom to the character's spot at the zone's zoom, the continent's
     * and the zone's paintings coming up over it, then vanilla's bar; no living map. */
    TestGlideState();
    g_class = 11; g_barVanilla = 1; g_viewTex[VT_VBORDER] = g_viewTex[VT_VGLASS] = 1; g_vbFullAt = 0;
    g_worldUp = 1; g_viewTex[VT_PWORLD] = 1; g_worldTW = 2048; g_worldK = 2; g_worldAt = 0;
    g_viewScreen = 7;
    ViewBuild(1920, 1080, 0.0f, 1000);
    ok = g_vb[0].tex == -1 && TestBatch(VT_PWORLD) < 0 && g_pcam.on;    /* (made this moment: not up yet) */
    ViewBuild(1920, 1080, 0.0f, 1000 + PARCH_FADE_MS / 2);
    int half = g_vv[g_vb[TestBatch(VT_PWORLD)].first].c[3];
    ViewBuild(1920, 1080, 0.0f, 1000 + PARCH_FADE_MS);
    int wb = TestBatch(VT_PWORLD), whole = g_vv[g_vb[wb].first].c[3];
    double s0 = 1080.0 / (PARCH_WORLD_FH * PARCH_OPEN_TALL);
    const ViewVert *w0 = wb >= 0 ? g_vv + g_vb[wb].first : NULL;
    ok &= wb == 1 && TestBatch(VT_PARCH) < 0 && TestBatch(VT_PARCH2) < 0 && TestBatch(VT_MAP) < 0 && TestBatch(VT_SPARK) < 0 &&
          TestBatch(VT_VGLASS) > wb && TestBatch(VT_VBORDER) > wb && w0 && Near(g_pcam.v.x, PARCH_MAEL_FX, 1e-6) &&
          Near(g_pcam.v.y, PARCH_MAEL_FY, 1e-6) && Near(g_pcam.v.s, s0, 1e-6) &&
          Near(w0[0].x, 960 - PARCH_MAEL_FX * s0, 0.01) && Near(w0[0].y, 0, 1e-4) && Near(w0[0].u, 0, 1e-6) &&
          Near(w0[0].v, (PARCH_MAEL_FY - 540 / s0) / PARCH_WORLD_FH * 668 / 1024, 1e-5);
    Check(ok, "the parchment's opening: the dark paper, then the client's world painting, its middle on the Maelstrom, 95% of it down the window; no continent or zone yet, no living map, vanilla's bar over it");
    g_worldUp = 0;
    ViewBuild(1920, 1080, 0.0f, 1000 + PARCH_FADE_MS);
    ok = half > 100 && half < 155 && whole == 255 && g_vbN && g_vb[0].tex == -1 && TestBatch(VT_PWORLD) < 0;
    g_worldUp = 1;
    Check(ok, "the parchment's opening: the world's painting fades up as each screen begins; until it is made, the dark paper alone");
    /* the arrival taken: Elwynn near Goldshire; its continent and zone up */
    TestGlideArrive(0, 1, 0, -9464, 62, 2000);
    ViewBuild(1920, 1080, 0.0f, 2000);
    ok = TestBatch(VT_PWORLD) == 1 && TestBatch(VT_PARCH2) < 0 && TestBatch(VT_PARCH) < 0 && Near(g_pcam.v.s, s0, 1e-6) && g_pcam.e == 0;
    float fx, fy;
    ParchSpot(0, -9464, 62, &fx, &fy);
    ViewBuild(1920, 1080, 1.0f, 9000);                                 /* the bar full: landed */
    int zb = TestBatch(VT_PARCH);
    const ViewVert *mid = zb >= 0 ? TestVertNear(zb, 960, 540) : NULL;
    float zu = (1535.0f - 62) / (1535.0f + 1935), zv = (-7939.0f + 9464) / (-7939.0f + 10254);
    ParchRect zr = ParchBox(1);
    ok &= zb > TestBatch(VT_PARCH2) && TestBatch(VT_PARCH2) > TestBatch(VT_PWORLD) && mid && Near(mid->x, 960, 1e-3) && Near(mid->y, 540, 1e-3) &&
          Near(mid->u, zu * 1002 / 1024, 2e-5) && Near(mid->v, zv * 668 / 1024, 2e-5) && mid->c[3] == 255 && g_pcam.e == 1 &&
          Near(g_pcam.v.x, fx, 1e-5) && Near(g_pcam.v.y, fy, 1e-5) && Near(g_pcam.v.s, 1920 * PARCH_ZOOM / (zr.x1 - zr.x0), 1e-3) &&
          Near(g_pcam.px, 960, 1e-3) && Near(g_pcam.py, 540, 1e-3);
    Check(ok, "the parchment's glide: from the opening, once the arrival is taken, to the character's own spot in the middle of the window at the zone's zoom (the zone 1.3 times wider than the window) as the bar is full, the zone's painting whole over its continent's over the world's");
    /* A: the continent's painting comes up over the world's as the world's would go soft, the zone's over the continent's */
    TestGlideArrive(0, 1, 0, -9464, 62, 2000);
    ViewBuild(1920, 1080, 0.0f, 2000);
    float e0, e1, z0, z1;
    ParchView va = ParchStart(1920, 1080, 1, ParchBox(0)), vz = ParchEnd(1920, 1080, zr, PARCH_ZOOM, fx, fy);
    float px = 1;                                                       /* (the fades as on the films he approved: a window
                                                                         * 1080 tall, its own pixels) */
    ParchSpan(&va, &vz, px * 2004.0f / PARCH_WORLD_FW, PARCH_MAG0, PARCH_MAG1, &e0, &e1);
    ParchRect cr0 = ParchBox(0);
    ParchSpan(&va, &vz, px * 2004.0f / (cr0.x1 - cr0.x0), PARCH_MAG0, PARCH_MAG1, &z0, &z1);
    float tm = TestTauFor(0.5f * (e0 + e1));                           /* (the bar where the continent is halfway up) */
    ViewBuild(1920, 1080, tm, 3000);
    int cb = TestBatch(VT_PARCH2);
    const ViewVert *cm = cb >= 0 ? TestVertNear(cb, g_pcam.px, g_pcam.py) : NULL;
    ok = e0 >= 0.02f && e1 > e0 && z0 >= e0 && z1 > e0 && z1 < 0.5f && cm && cm->c[3] > 100 && cm->c[3] < 155;
    int half1080 = cm ? cm->c[3] : -1;
    TestGlideArrive(0, 1, 0, -9464, 62, 2000);                         /* (the same framing on a window twice the size: the same fade) */
    ViewBuild(3840, 2160, 0.0f, 2000);
    ViewBuild(3840, 2160, tm, 3000);
    cb = TestBatch(VT_PARCH2);
    cm = cb >= 0 ? TestVertNear(cb, g_pcam.px, g_pcam.py) : NULL;
    ok &= cm && abs(cm->c[3] - half1080) <= 2;
    /* the continent-to-zone moment (his films, 1080 tall: the continent comes up, then the zone over it; at 2160 they came
     * up together): as the continent is halfway up the zone has barely begun, on any window */
    ok &= e0 > 0.03f && z0 > e0 + 0.05f && 0.5f * (z0 + z1) > 0.5f * (e0 + e1) + 0.05f;
    for (int big = 0; big < 2; big++) {
        TestGlideArrive(0, 1, 0, -9464, 62, 2000);
        ViewBuild(big ? 3840 : 1920, big ? 2160 : 1080, 0.0f, 2000);
        ViewBuild(big ? 3840 : 1920, big ? 2160 : 1080, tm, 3000);
        int zb2 = TestBatch(VT_PARCH), cb2 = TestBatch(VT_PARCH2);
        const ViewVert *zm = zb2 >= 0 ? TestVertNear(zb2, g_pcam.px, g_pcam.py) : NULL, *cm2 = cb2 >= 0 ? TestVertNear(cb2, g_pcam.px, g_pcam.py) : NULL;
        ok &= cm2 && abs(cm2->c[3] - half1080) <= 2 && (!zm || zm->c[3] < 26);
    }
    TestGlideArrive(0, 1, 0, -9464, 62, 2000);
    ViewBuild(1920, 1080, 0.0f, 2000);
    ViewBuild(1920, 1080, 0.999f, 3100);
    ok &= TestBatch(VT_PARCH) >= 0 && TestVertNear(TestBatch(VT_PARCH), 960, 540)->c[3] == 255;
    float eHigh = g_pcam.e;
    ViewBuild(1920, 1080, 0.5f, 3116);                                 /* (a bar that went back: the glide does not) */
    ok &= g_pcam.e == eHigh;
    g_zoneAt = 3116 - PARCH_FADE_MS / 4;                               /* (the zone's painting made late: it comes up over PARCH_FADE_MS) */
    ViewBuild(1920, 1080, 0.5f, 3116);
    int lateZ = TestVertNear(TestBatch(VT_PARCH), 960, 540)->c[3];
    g_zoneAt = 0;
    ok &= lateZ > 50 && lateZ < 80;
    Check(ok, "the fade A: each finer painting comes up over the coarser one as the coarser one is drawn larger than its own texels (halfway where, on a window 1080 tall as his approved films, it would be drawn 1.3 pixels a texel: the same map scale and framing on any window, 1920 x 1080 or 3840 x 2160), the zone barely begun over it as the continent is halfway up over the world's, the zone's whole well before the end");
    /* the player's marker (his pick, 11 Oct, option 1, no ripple): the world map's own arrow pointing north over a dark disc,
     * the candle's amber glow breathing over 2 s, over everything, coming up with the zone's painting */
    {
        static const char *mn[2] = { PARCH_MARK_PATH, PARCH_GLOW_PATH };
        unsigned ml[2];
        const char *md[2] = { (const char *)TestPalBlp(64, 64, 0xC0C0D0, 8, &ml[0]), (const char *)TestPalBlp(32, 32, 0x203080, 0, &ml[1]) };
        MakeMpq("Data/patch-I.MPQ", 2, mn, md, ml);
        int rd = ParchMarkRead();
        ok = rd && g_markPic[0].w == 64 && g_markPic[1].w == 32 && g_markPic[1].px[3] == 0x80 && g_markPic[0].px[3] == 255;   /* (the glow's alpha its brightness) */
        for (int k = 0; k < 2; k++) { g_markTW[k] = g_markTH[k] = g_markPic[k].w; g_viewTex[VT_PMARK + k] = 1; }
        g_markUp = 1;
        TestGlideArrive(0, 1, 0, -9464, 62, 2000);
        ViewBuild(1920, 1080, 0.0f, 2000);
        ok &= TestBatch(VT_PMARK) < 0 && TestBatch(VT_PGLOW) < 0;      /* (the zone not up yet: no marker) */
        ViewBuild(1920, 1080, 1.0f, 9500);                             /* (landed: the zone whole, the marker on the spot) */
        int gb = -1, db = -1, ab = TestBatch(VT_PMARK);
        for (int b = 0; b < g_vbN; b++) if (g_vb[b].tex == VT_PGLOW) { if (g_vb[b].add) gb = b; else db = b; }
        int last = 1;
        for (int b = 0; b < g_vbN; b++) if (b > ab && (g_vb[b].tex == VT_PARCH || g_vb[b].tex == VT_PWORD0 || g_vb[b].tex == VT_PARCH2)) last = 0;
        float k = 0.5f + 0.5f * sinf(6.2831853f * 9500 / 2000);      /* (his 2 s breath; the mock's sizes: markers.py) */
        const ViewVert *gv = gb >= 0 ? &g_vv[g_vb[gb].first] : NULL, *dv = db >= 0 ? &g_vv[g_vb[db].first] : NULL, *av = ab >= 0 ? &g_vv[g_vb[ab].first] : NULL;
        ok &= gv && dv && av && gb < db && db < ab && last && g_vb[gb].count == 4 && g_vb[ab].count == 4 &&
              Near(gv[2].x - gv[0].x, 230 * (0.94f + 0.06f * k), 0.01) && Near(0.5f * (gv[0].x + gv[2].x), 960, 0.01) && Near(0.5f * (gv[0].y + gv[2].y), 540, 0.01) &&
              Near(dv[2].x - dv[0].x, 88 * 1.05f, 0.01) && Near(av[2].x - av[0].x, 88, 0.01) &&
              Near(0.5f * (av[0].y + av[2].y), 540 - 6, 0.01) && av[0].c[3] == 255 && av[0].c[0] == 255 &&
              abs(gv[0].c[3] - (int)(255 * (0.16f + 0.16f * k) + 0.5f)) <= 1 && gv[0].c[0] == 255 && abs(gv[0].c[1] - 189) <= 1 && abs(gv[0].c[2] - 102) <= 1 &&
              abs(dv[0].c[3] - (int)(255 * 0.62f + 0.5f)) <= 1 && dv[0].c[0] < 20 && av[0].v == 0 && av[2].v == 1;   /* (north: its picture upright) */
        float g0 = gv ? gv[2].x - gv[0].x : 0;
        ViewBuild(1920, 1080, 1.0f, 10500);                            /* (a second later: the breath's other half) */
        gb = -1;
        for (int b = 0; b < g_vbN; b++) if (g_vb[b].tex == VT_PGLOW && g_vb[b].add) gb = b;
        float g1 = gb >= 0 ? g_vv[g_vb[gb].first + 2].x - g_vv[g_vb[gb].first].x : 0, k1 = 1 - k;
        ok &= Near(g1, 230 * (0.94f + 0.06f * k1), 0.01);
        ViewBuild(1920, 1080, 1.0f, 11500);                            /* (two seconds on: as it was) */
        for (int b = 0; b < g_vbN; b++) if (g_vb[b].tex == VT_PGLOW && g_vb[b].add) gb = b;
        ok &= Near(g_vv[g_vb[gb].first + 2].x - g_vv[g_vb[gb].first].x, g0, 0.01) && fabsf(g0 - g1) > 10;
        TestGlideArrive(0, 1, 0, -9464, 62, 2000);                     /* (his window, twice as tall: twice as big) */
        ViewBuild(3840, 2160, 0.0f, 2000);
        ViewBuild(3840, 2160, 1.0f, 9000);
        ab = TestBatch(VT_PMARK);
        ok &= ab >= 0 && Near(g_vv[g_vb[ab].first + 2].x - g_vv[g_vb[ab].first].x, 2 * 88, 0.01) &&
              Near(0.5f * (g_vv[g_vb[ab].first].y + g_vv[g_vb[ab].first + 2].y), 1080 - 12, 0.01);
        TestGlideArrive(0, 1, 0, -9464, 62, 2000);                     /* (21:9, as tall: the same size, by the height) */
        ViewBuild(2560, 1080, 0.0f, 2000);
        ViewBuild(2560, 1080, 1.0f, 9000);
        ab = TestBatch(VT_PMARK);
        ok &= ab >= 0 && Near(g_vv[g_vb[ab].first + 2].x - g_vv[g_vb[ab].first].x, 88, 0.01);
        TestGlideArrive(0, 0, 0, -500, 500, 2000);                     /* (at sea: with its continent's painting) */
        ViewBuild(1920, 1080, 0.0f, 2000);
        ViewBuild(1920, 1080, 0.6f, 2500);
        ab = TestBatch(VT_PMARK);
        ok &= ab >= 0 && g_pcam.mc > 0.05f && abs(g_vv[g_vb[ab].first].c[3] - (int)(255 * g_pcam.mc + 0.5f)) <= 1;
        TestGlideArrive(0, 1, 0, -9464, 62, 2000);                     /* (halfway up with the zone's painting) */
        ViewBuild(1920, 1080, 0.0f, 2000);
        ViewBuild(1920, 1080, TestTauFor(0.5f * (z0 + z1)), 3000);
        ab = TestBatch(VT_PMARK);
        ok &= ab >= 0 && abs(g_vv[g_vb[ab].first].c[3] - (int)(255 * g_pcam.mz + 0.5f)) <= 1 && g_pcam.mz > 0.3f && g_pcam.mz < 0.7f;
        g_markUp = 0;                                                  /* (not read from the client: none) */
        ViewBuild(1920, 1080, 1.0f, 9000);
        ok &= TestBatch(VT_PMARK) < 0 && TestBatch(VT_PGLOW) < 0;
        for (int k2 = 0; k2 < 2; k2++) { ParchPicFree(&g_markPic[k2]); g_viewTex[VT_PMARK + k2] = 0; free((void *)md[k2]); }
        Check(ok, "the player's marker (his pick, option 1): the world map's own arrow (MinimapArrow.blp) pointing north over a dark disc, the candle's amber glow (GenericGlow2_32.blp, its alpha its brightness) added round it breathing over 2 s, centred on the character's spot over everything, sized for a window 1080 tall (twice on 2160), coming up with the zone's painting; none before it, none without its pictures");
    }
    TestGlideArrive(0, 1, 0, -9464, 62, 4000);                         /* (taken late, the bar 40% along: the glide starts there) */
    ViewBuild(1920, 1080, 0.4f, 4000);
    ok = g_pcam.e == 0 && Near(g_pcam.v.s, s0, 1e-6);
    ViewBuild(1920, 1080, 0.7f, 4100);
    ok &= Near(g_pcam.tau, 0.5, 1e-5);
    Check(ok, "the parchment's glide: an arrival taken late (the bar part way) starts its glide from the opening there and runs on the rest of the bar");
    /* a screen whose arrival is not yet taken for it (the last screen's): the opening, nothing of the last arrival's */
    g_viewScreen = 8;
    ViewBuild(1920, 1080, 0.5f, 20000);
    ok = TestBatch(VT_PARCH) < 0 && TestBatch(VT_PARCH2) < 0 && Near(g_pcam.v.s, s0, 1e-6) && g_pcam.e == 0;
    ViewBuild(1920, 1080, 0.9f, 20500);                                /* (and on: never toward the last screen's arrival) */
    ok &= TestBatch(VT_PARCH) < 0 && Near(g_pcam.v.s, s0, 1e-6) && g_pcam.e == 0;
    Check(ok, "the parchment's glide: a new screen, until its own arrival is taken, on the opening, never the last arrival's end (no flash of it)");
    g_viewScreen = 7;                                                   /* (on its own screen, the game's arrival written anew: until it is
                                                                        * taken, the glide goes on to the one taken, no frame of the opening) */
    TestGlideArrive(0, 1, 0, -9464, 62, 20000);
    ViewBuild(1920, 1080, 0.0f, 20000);
    ViewBuild(1920, 1080, 0.3f, 20500);
    float sHalf = (float)g_pcam.v.s;
    g_destWX = -9300;
    ViewBuild(1920, 1080, 0.35f, 20533);
    ok = TestBatch(VT_PARCH) >= 0 && g_pcam.v.s > sHalf && g_pcam.v.s > 1.5 * s0;
    g_destWX = -9464;
    Check(ok, "the parchment's glide: the game's arrival written anew on its screen, until it is taken the glide goes on to the one taken, never a frame of the opening");
    /* without the world's painting (the client's frame not as read): the continent's whole across the window first */
    g_worldFrameOk = 0;
    TestGlideArrive(0, 1, 0, -9464, 62, 2000);
    ViewBuild(1920, 1080, 0.0f, 2000);
    ok = TestBatch(VT_PWORLD) < 0 && TestBatch(VT_PARCH2) >= 0 && Near(g_pcam.v.s, 1920.0 / (cr0.x1 - cr0.x0), 1e-4) &&
         Near(g_pcam.v.x, 0.5 * (cr0.x0 + cr0.x1), 1e-4) && g_vv[g_vb[TestBatch(VT_PARCH2)].first].c[3] == 255;
    ViewBuild(1920, 1080, 1.0f, 9000);
    ok &= Near(g_pcam.px, 960, 1e-3) && TestVertNear(TestBatch(VT_PARCH), 960, 540)->c[3] == 255;
    g_worldFrameOk = 1;
    Check(ok, "the parchment's glide without the client's world frame: it opens on the arrival's continent's painting, whole across the window, and glides from there to the character's spot");
    g_worldFrameOk = 0;                                                 /* (no continent's either: the zone's whole painting first) */
    TestGlideArrive(0, 1, -1, -9464, 62, 2000);
    ViewBuild(1920, 1080, 0.0f, 2000);
    ok = TestBatch(VT_PARCH2) < 0 && TestBatch(VT_PARCH) >= 0 && Near(g_pcam.v.s, 1920.0 / (zr.x1 - zr.x0), 1e-4) &&
         Near(g_pcam.v.x, 0.5 * (zr.x0 + zr.x1), 1e-4) && TestVertNear(TestBatch(VT_PARCH), 960, 540)->c[3] == 255;
    ViewBuild(1920, 1080, 1.0f, 9000);
    ok &= Near(g_pcam.px, 960, 1e-3) && Near(g_pcam.v.s, 1920 * PARCH_ZOOM / (zr.x1 - zr.x0), 1e-3);
    g_worldFrameOk = 1;
    Check(ok, "the parchment's glide with neither the world's nor a continent's painting: it opens on the zone's whole painting and glides in to the character's spot");
    /* at sea (no zone holds it): the continent's painting the finest, the glide ending PARCH_SEA_ZOOM in on it */
    TestGlideArrive(0, 0, 0, -500, 500, 2000);
    ViewBuild(1920, 1080, 0.0f, 2000);
    ViewBuild(1920, 1080, 1.0f, 9000);
    ParchSpot(0, -500, 500, &fx, &fy);
    cb = TestBatch(VT_PARCH2);
    ok = TestBatch(VT_PARCH) < 0 && cb >= 0 && TestVertNear(cb, 960, 540)->c[3] == 255 && Near(g_pcam.v.x, fx, 1e-5) &&
         Near(g_pcam.v.s, 1920 * PARCH_SEA_ZOOM / (cr0.x1 - cr0.x0), 1e-3);
    Check(ok, "the parchment's glide to an arrival at sea: its continent's painting the finest, whole at the end, the character's spot in the middle, PARCH_SEA_ZOOM in");
    /* at a zone's edge: still the character's spot in the middle; the zone's painting where it is, soft at its edge, its
     * continent's past it */
    TestGlideArrive(0, 1, 0, -10254 + 1, 1535 - 1, 2000);              /* (its box's bottom left corner) */
    ViewBuild(1920, 1080, 0.0f, 2000);
    ViewBuild(1920, 1080, 1.0f, 9000);
    zb = TestBatch(VT_PARCH);
    const ViewVert *z0v = zb >= 0 ? g_vv + g_vb[zb].first : NULL;
    ok = zb >= 0 && Near(g_pcam.px, 960, 1e-3) && Near(g_pcam.py, 540, 1e-3) && z0v && z0v[0].x >= 959 && z0v[0].c[3] == 0 &&
         TestVertNear(TestBatch(VT_PARCH2), 100, 100)->c[3] == 255;
    Check(ok, "the parchment's glide to a zone's very edge: the character's spot still in the middle (not kept on the zone), the zone's painting only where it is, soft at its edge, its continent's past it");
    /* B: from the character outward */
    g_parchFade = PARCH_FADE_B;
    TestGlideArrive(0, 1, 0, -9464, 62, 2000);
    ViewBuild(1920, 1080, 0.0f, 2000);
    float b0, b1;
    ParchSpan(&va, &vz, px * 2004.0f / (cr0.x1 - cr0.x0), PARCH_MAG0, PARCH_BLOOM, &b0, &b1);
    ViewBuild(1920, 1080, TestTauFor(b0 + 0.05f * (b1 - b0)), 2500);  /* (early, the circle well inside the zone: its own span's radius) */
    zb = TestBatch(VT_PARCH);
    float rEarlyGot = 0;
    for (int i = zb >= 0 ? g_vb[zb].first : 0; zb >= 0 && i < g_vb[zb].first + g_vb[zb].count; i++) {
        float d = sqrtf((g_vv[i].x - g_pcam.px) * (g_vv[i].x - g_pcam.px) + (g_vv[i].y - g_pcam.py) * (g_vv[i].y - g_pcam.py));
        if (g_vv[i].c[3] > 0 && d > rEarlyGot) rEarlyGot = d;
    }
    float rEarly = ParchSmooth(0.05f) * (sqrtf(1920.0f * 1920 + 1080.0f * 1080) + PARCH_BLOOM_MIN * 1080);
    int earlyOk = zb >= 0 && fabsf(rEarlyGot - rEarly) < 0.1f * rEarly + 1;
    ViewBuild(1920, 1080, TestTauFor(b0 + 0.3f * (b1 - b0)), 3000);
    zb = TestBatch(VT_PARCH);
    float rx = 0, nearA = zb >= 0 ? TestVertNear(zb, g_pcam.px, g_pcam.py)->c[3] : 0, ext = 0;
    for (int i = zb >= 0 ? g_vb[zb].first : 0; zb >= 0 && i < g_vb[zb].first + g_vb[zb].count; i++) {
        float d = sqrtf((g_vv[i].x - g_pcam.px) * (g_vv[i].x - g_pcam.px) + (g_vv[i].y - g_pcam.py) * (g_vv[i].y - g_pcam.py));
        if (g_vv[i].c[3] > 0 && d > rx) rx = d;
        if (d > ext) ext = d;
    }
    float rWant = ParchSmooth(0.3f) * (sqrtf(1920.0f * 1920 + 1080.0f * 1080) + PARCH_BLOOM_MIN * 1080), eWant = rWant * PARCH_BLOOM_EDGE;
    if (eWant < PARCH_BLOOM_MIN * 1080) eWant = PARCH_BLOOM_MIN * 1080;
    ok = zb >= 0 && nearA == 255 && rx < rWant + 1 && ext < rWant * 1.42f + 1 && eWant > 0;
    ok &= earlyOk;
    ViewBuild(1920, 1080, 1.0f, 9000);
    ok &= TestVertNear(TestBatch(VT_PARCH), 960, 540)->c[3] == 255 && TestVertNear(TestBatch(VT_PARCH), 200, 300)->c[3] == 255;
    Check(ok, "the fade B: the finer painting first round the character's spot, whole there, nothing past a soft-edged circle that spreads as the zoom closes in, the whole window by the end");
    /* C: its detail in the colours under it, then its own colours (the colour-matched copy, then itself) */
    g_parchFade = PARCH_FADE_C;
    g_viewTex[VT_PZONEM] = g_viewTex[VT_PCONTM] = 1; g_zoneMatch = 1; g_contMatch = 0;
    TestGlideArrive(0, 1, 0, -9464, 62, 2000);
    ViewBuild(1920, 1080, 0.0f, 2000);
    ParchSpan(&va, &vz, px * 2004.0f / (cr0.x1 - cr0.x0), PARCH_MAG0, PARCH_MAG0 * PARCH_DETAIL, &z0, &z1);
    ViewBuild(1920, 1080, TestTauFor(z1 + 0.001f), 3000);
    int mb = TestBatch(VT_PZONEM), ob = TestBatch(VT_PARCH);
    ok = mb >= 0 && TestVertNear(mb, g_pcam.px, g_pcam.py)->c[3] == 255 && (ob < 0 || TestVertNear(ob, g_pcam.px, g_pcam.py)->c[3] < 10);
    ViewBuild(1920, 1080, 1.0f, 9000);
    mb = TestBatch(VT_PZONEM); ob = TestBatch(VT_PARCH);
    ok &= mb >= 0 && ob > mb && TestVertNear(ob, 960, 540)->c[3] == 255 && TestBatch(VT_PCONTM) >= 0 && TestBatch(VT_PCONTM) < mb;
    g_zoneMatch = 7;                                                    /* (its copy not made: as A) */
    ViewBuild(1920, 1080, 1.0f, 9000);
    ok &= TestBatch(VT_PZONEM) < 0 && TestVertNear(TestBatch(VT_PARCH), 960, 540)->c[3] == 255;
    Check(ok, "the fade C: the finer painting's copy in the colours under it comes up first (its own painting not yet), then its own colours over it, whole at the end; with no copy made, as A");
    g_parchFade = PARCH_FADE_A; g_zoneMatch = g_contMatch = -1; g_viewTex[VT_PZONEM] = g_viewTex[VT_PCONTM] = 0;

    /* a screen's first frame, made as the view opens: the dark paper and the empty bar (the parchment), nothing (the
     * living map); never the last screen's */
    int Q = PARCH_NX * PARCH_NY;
    ViewFirstList(1920, 1080, 40000);
    ok = g_vbN == 3 && g_vb[0].tex == -1 && g_vb[0].count >= 4 * Q && g_vb[1].tex == VT_VGLASS && g_vb[2].tex == VT_VBORDER;   /* (the bar's channel joins the paper's batch) */
    for (int b = 0; b < g_vbN; b++) ok &= g_vb[b].tex != VT_PARCH && g_vb[b].tex != VT_MAP && g_vb[b].tex != VT_PWORLD;
    g_parchOn = 0;
    ViewFirstList(1920, 1080, 40000);
    ok &= g_vbN == 0;
    g_parchOn = 1;
    Check(ok, "a screen's first frame, made at once as the view opens: the dark paper and the empty bar for the parchment, nothing for the living map; never the last screen's frame");
    ParchBlackBuild();                                               /* the game's own frames before the view's: black */
    ok = g_blackPos[0] == 0 && g_blackPos[1] == 0 && g_blackPos[3] == 1 && g_blackPos[4] == 0 && g_blackPos[6] == 0 && g_blackPos[7] == 1 &&
         g_blackPos[9] == 1 && g_blackPos[10] == 1 && g_blackUV[0] == SEA_SQUARE_U && g_blackUV[1] == SEA_DEEP_V && g_blackCol[0] == 0xFFFFFFFFu;
    Check(ok, "with the parchment on, the game's own frames of a screen before the view's are black from corner to corner (the sea sheet's deep square), none of the living map's picture");
    TestGlideState();
    g_parchOn = 0; g_barVanilla = 0; g_class = 0; g_viewScreen = 0;
    for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
    if (g_parchPx) { HeapFree(GetProcessHeap(), 0, g_parchPx); g_parchPx = NULL; }

    /* read as the view starts, whichever map is chosen (it may be chosen in game at any time): the tables from the
     * client's dbc archive */
    Fresh("parchment-asked");
    GameDir();
    ParchRead();
    ok = g_pzN == 0;
    static const char *dn[2] = { "DBFilesClient\\WorldMapArea.dbc", "DBFilesClient\\WorldMapOverlay.dbc" };
    const char *dd[2] = { (const char *)wma, (const char *)wmo };
    unsigned dl[2] = { wn, on };
    MakeMpq("Data/dbc.MPQ", 2, dn, dd, dl);
    ParchRead();
    unsigned char *vb[VT_COUNT] = { 0 };
    unsigned vl[VT_COUNT] = { 0 };
    VBarRead(vb, vl);
    ok &= g_pzN == 4 && g_ppN == 2 && LogSays("the parchment: 4 zones (WorldMapArea.dbc from dbc.MPQ)");
    for (int i = VT_VBORDER; i <= VT_VGLASS; i++) if (vb[i]) HeapFree(GetProcessHeap(), 0, vb[i]);
    Check(ok, "the parchment's tables read as the view starts, whichever map is chosen (no switch file needed), and logged");
}

/* One load through the DLL's own steps (the game's frames, its hold at a full bar, the view's frames), the camera logged:
 * the game's bar full at full ms (jumpy: it stalls near half, drawing nothing for stretches), its learned first-login
 * time learned ms, the floor hold ms. Out: the frames' character on the screen, zoom, time and bar. */
#define GL_MAXF 4000
static float g_glX[GL_MAXF], g_glY[GL_MAXF], g_glS[GL_MAXF], g_glT[GL_MAXF], g_glB[GL_MAXF];
static int g_glN;
static float g_glTakeAt, g_glMoveAt = 2, g_glMoveDx;                 /* the arrival taken first at this bar; moved at that bar by so many yards */
static float g_glPace[GL_MAXF], g_glVX[GL_MAXF], g_glVY[GL_MAXF];
static int g_glZA[GL_MAXF], g_glCA[GL_MAXF];                          /* the zone's and the continent's alpha at the window's middle */
static void TestGlideRetake(float x, float y, DWORD at)               /* (the arrival taken anew, as ParchWork takes it) */
{
    g_destWX = x; g_destWY = y;
    g_parchForX = x; g_parchForY = y; g_parchAt = at;
}
static void TestGlideLoad(DWORD full, LONG learned, LONG freeze, LONG hold, float W, float H)
{
    Fresh("glide-load");
    GameDir();
    TestGlideState();
    g_worldUp = 1; g_viewTex[VT_PWORLD] = 1; g_worldTW = 1024; g_worldK = 1;
    g_map.ok = 1; g_map.m[0].ok = g_map.m[1].ok = 1; g_map.endYards = 3500; g_map.pace = 7;
    g_map.m[0].bu = g_map.m[0].bv = 1e-5f; g_map.m[0].au = g_map.m[0].av = 0.5f;
    g_mapShown = 1; g_driftOn = 1; g_screenWatch = 1; g_holdOn = 1; g_holdMs = hold; g_blurOn = 0;
    for (int k = 0; k < 2; k++) { g_lnN[k] = learned ? 3 : 0; g_lnMs[k] = learned; g_lnFreeze[k] = freeze; }   /* (either kind of screen) */
    g_holdDone = 0;
    g_evenOn = 0; g_evenU = 0; g_evenEndAt = 0; g_viewSteps = 0; g_dispNew = 1; g_driftNew = 1; g_dispAt = 0; g_gameAt = 0; g_driftAt = 0;
    g_holdFrame = TestHoldStub;
    g_viewScreen = 11;
    unsigned one = 0x3F800000u;
    DWORD base = 7000000, nextGame = base, endAt = 0;
    int held = 0, gone = 0;
    g_glN = 0;
    for (int f = 0; f < GL_MAXF && !gone; f++) {
        DWORD now = base + (DWORD)f * 16;
        shim_ticks = now;
        while (!held && (LONG)(nextGame - now) <= 0) {
            float k = (float)(nextGame - base) / (float)full, p = k >= 1 ? 1 : k < 0.45f ? k : k < 0.95f ? 0.54f : k;
            int stall = k > 0.6f && k < 0.95f && ((int)(k * 40) % 3);
            if (!stall || p >= 0.999f) GameStep(p, nextGame);
            if (p >= 0.999f) {                                           /* (HoldFrame's own test of the game's bar) */
                memcpy(shim_module + (HOLD_BAR_VA - 0x400000u), &one, 4);
                shim_ticks = nextGame;
                HoldFrame();
                endAt = g_evenEndAt ? g_evenEndAt - VIEW_WORLD_GAP_MS : nextGame + 1000;   /* (the screen goes) */
                held = 1;
            }
            nextGame += 33;
        }
        if (held && (LONG)(now - endAt) >= 0) gone = 1;
        if (gone || !ViewStep(g_gameMax, now, W, H)) continue;
        if (g_parchForScreen != g_viewScreen && g_viewShown >= g_glTakeAt) TestGlideArrive(0, 1, 0, -9464, 62, now);   /* (taken, as ParchWork) */
        else if (g_parchForScreen == g_viewScreen && g_viewShown >= g_glMoveAt && g_parchForX == -9464) TestGlideRetake(-9464 + g_glMoveDx, 62, now);
        g_glX[g_glN] = g_pcam.px; g_glY[g_glN] = g_pcam.py; g_glS[g_glN] = (float)g_pcam.v.s; g_glT[g_glN] = g_pcam.tau; g_glB[g_glN] = g_viewShown;
        g_glPace[g_glN] = g_evenPace; g_glVX[g_glN] = (float)g_pcam.v.x; g_glVY[g_glN] = (float)g_pcam.v.y;
        { int zb = TestBatch(VT_PARCH), cb = TestBatch(VT_PARCH2);
          g_glZA[g_glN] = zb >= 0 ? TestVertNear(zb, W / 2, H / 2)->c[3] : 0; g_glCA[g_glN] = cb >= 0 ? TestVertNear(cb, W / 2, H / 2)->c[3] : 0; }
        g_glN++;
    }
    memset(shim_module + (HOLD_BAR_VA - 0x400000u), 0, 4);
    g_holdMs = HOLD_MS; g_holdOn = 0; g_map.ok = 0; g_mapShown = 0; g_driftOn = 0; g_viewSteps = 0;
    for (int k = 0; k < 2; k++) g_lnN[k] = g_lnMs[k] = g_lnFreeze[k] = 0;
}
/* Its glide: the character's spot moves on one straight line, out to the middle, never back (off it by at most *dev
 * pixels; *jump its largest change of speed from one frame to the next, against its fastest), the zoom never back out,
 * landed (time 1) on the frame the bar is full. */
static int TestGlideGood(float W, float H, float *dev, float *jump)
{
    int first = 1;
    while (first < g_glN && g_glT[first] <= 0) first++;
    first--;
    if (g_glN < 10 || first < 0) return 0;
    float x0 = g_glX[first], y0 = g_glY[first], x1 = g_glX[g_glN - 1], y1 = g_glY[g_glN - 1];
    float L = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)), ux = (x1 - x0) / L, uy = (y1 - y0) / L;
    int ok = L > 1 && Near(x1, W / 2, 1e-3) && Near(y1, H / 2, 1e-3), land = -1, fullAt = -1;
    float worst = 0, most = 0, change = 0, along = -1, lastStep = -1;
    for (int i = first; i < g_glN; i++) {
        float d = fabsf((g_glX[i] - x0) * uy - (g_glY[i] - y0) * ux), a = (g_glX[i] - x0) * ux + (g_glY[i] - y0) * uy;
        if (d > worst) worst = d;
        if (a < along - 1e-3f) ok = 0;                                  /* (never back toward where it began) */
        if (i > first) {
            float st = a - along;
            if (st > most) most = st;
            if (lastStep >= 0 && fabsf(st - lastStep) > change) change = fabsf(st - lastStep);
            lastStep = st;
        }
        along = a;
        if (i > first && g_glS[i] < g_glS[i - 1] * (1 - 1e-6f)) ok = 0;   /* (the zoom never back out) */
        if (i > first && g_glT[i] < g_glT[i - 1]) ok = 0;
        if (land < 0 && g_glT[i] >= 1) land = i;
        if (fullAt < 0 && g_glB[i] >= 1) fullAt = i;
    }
    *dev = worst; *jump = most > 0 ? change / most : 99;
    return ok && land >= 0 && land == fullAt;
}
/* The arrival's zone as the client finds it (the skeptic's find, 11 Oct 2026): its terrain chunk's area, walked up to
 * its zone, that zone's page; a capital the terrain does not know by its building; the boxes only without a tile's file */
static unsigned char g_adt[2][256 * 1024];
static float g_testGround = -1e30f;                                    /* (above -1e29: every chunk flat at this height, its MCVT) */
static int g_testTile[2], g_testOcean;                                 /* (the tile's column and row, for the chunks' places; their
                                                                        * flags: ocean) */
static const unsigned *g_testAreas;                                    /* (each chunk's area, 16 x 16, when given) */
static const unsigned char *g_testSea;                                 /* (each chunk the sea's: ocean, 5 under its level) */
static unsigned TestAdt(unsigned char *out, int cx, int cy, unsigned area, unsigned others, const unsigned char *modf, unsigned nModf,
                        const char *wmoName)
{
    unsigned at = 0;
    unsigned v[2] = { 0x4D564552u, 4 }, ver = 18;                    /* MVER */
    memcpy(out, v, 8); memcpy(out + 8, &ver, 4); at = 12;
    unsigned mcin = at + 8, hd[2] = { ADT_MCIN, 256 * 16 };
    memcpy(out + at, hd, 8); at += 8 + 256 * 16;
    if (wmoName) {                                                     /* MWMO, MWID, MODF */
        unsigned nl = (unsigned)strlen(wmoName) + 1, h1[2] = { ADT_MWMO, nl }, h2[2] = { ADT_MWID, 4 }, zero = 0, h3[2] = { ADT_MODF, 64 * nModf };
        memcpy(out + at, h1, 8); memcpy(out + at + 8, wmoName, nl); at += 8 + nl;
        memcpy(out + at, h2, 8); memcpy(out + at + 8, &zero, 4); at += 12;
        memcpy(out + at, h3, 8); memcpy(out + at + 8, modf, 64 * nModf); at += 8 + 64 * nModf;
    }
    int hv = g_testGround > -1e29f;
    unsigned hs = hv ? 0x80 : 0x38;                                    /* (its header: with heights, all of it) */
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {                                 /* every chunk: its header, its place, its area */
            unsigned size = hs + 8 + (hv ? 8 + 145 * 4 : 0), h[2] = { ADT_MCNK, size }, head[32] = { 0 };
            int sea = g_testSea && g_testSea[j * 16 + i];
            head[0] = g_testOcean || sea ? 0x8 : 0;
            head[1] = (unsigned)i; head[2] = (unsigned)j; head[13] = g_testAreas ? g_testAreas[j * 16 + i] : i == cx && j == cy ? area : others;
            if (hv) {
                float px = (32 - g_testTile[1]) * 533.33333f - j * 33.333333f, py = (32 - g_testTile[0]) * 533.33333f - i * 33.333333f;
                float hz = sea ? -5.0f : g_testGround;
                head[5] = 8 + hs + 8;                                  /* (its heights, from the chunk's start) */
                memcpy(&head[26], &px, 4); memcpy(&head[27], &py, 4); memcpy(&head[28], &hz, 4);
            }
            unsigned ent[4] = { at, 8 + size, 0, 0 };
            memcpy(out + mcin + 16 * (j * 16 + i), ent, 16);
            memcpy(out + at, h, 8); memcpy(out + at + 8, head, hs); memset(out + at + 8 + hs, 0, 8); at += 8 + hs + 8;
            if (hv) {
                unsigned mh[2] = { ADT_MCVT, 145 * 4 };
                memcpy(out + at, mh, 8); memset(out + at + 8, 0, 145 * 4); at += 8 + 145 * 4;
            }
        }
    return at;
}
static void TestModf(unsigned char *e, float x0, float y0, float x1, float y1, unsigned set)
{
    memset(e, 0, 64);
    float lo[3] = { 17066.666f - y1, -500, 17066.666f - x1 }, hi[3] = { 17066.666f - y0, 500, 17066.666f - x0 };
    memcpy(e + 32, lo, 12); memcpy(e + 44, hi, 12);
    unsigned s = set;
    memcpy(e + 60, &s, 4);
}
/* City.wmo: its root (MOHD: id 208; MOGI: one group, over world x -4950..-4850, y -950..-850 at a placement of no
 * offset or turn, 90 to 200 high) and its group (its MOGP id group, a floor of two faces at 100 high, their MOPY flags) */
static unsigned char g_wmoRoot[160], g_wmoGroup[256];
static void TestWmo(unsigned group, unsigned char flags, unsigned *len)
{
    unsigned char *r = g_wmoRoot, *g = g_wmoGroup;
    unsigned wh[3] = { 0x4D564552u, 4, 17 }, mh[2] = { WMO_MOHD, 64 }, gh[2] = { WMO_MOGI, 32 }, id = 208, gf = 0x2000;
    float lx0 = 17066.666f + 4850, lx1 = 17066.666f + 4950, ly0 = 17066.666f + 850, ly1 = 17066.666f + 950;   /* (its own axes) */
    float box[6] = { lx0, ly0, 90, lx1, ly1, 200 };
    memset(r, 0, sizeof g_wmoRoot);
    memcpy(r, wh, 12); memcpy(r + 12, mh, 8); memcpy(r + 20 + 0x20, &id, 4); memcpy(r + 84, gh, 8); memcpy(r + 92, &gf, 4); memcpy(r + 96, box, 24);
    len[0] = 92 + 32;
    memset(g, 0, sizeof g_wmoGroup);
    unsigned ph[2] = { WMO_MOGP, 0x44 + 8 + 4 + 8 + 12 + 8 + 48 }, yh[2] = { WMO_MOPY, 4 }, ih[2] = { WMO_MOVI, 12 }, vh[2] = { WMO_MOVT, 48 };
    memcpy(g, wh, 12); memcpy(g + 12, ph, 8); memcpy(g + 20 + 0x38, &group, 4);
    unsigned at = 20 + 0x44;
    memcpy(g + at, yh, 8); g[at + 8] = flags; g[at + 10] = flags; at += 8 + 4;
    unsigned short idx[6] = { 0, 1, 2, 0, 2, 3 };
    memcpy(g + at, ih, 8); memcpy(g + at + 8, idx, 12); at += 8 + 12;
    float v[12] = { lx0, ly0, 100, lx1, ly0, 100, lx1, ly1, 100, lx0, ly1, 100 };
    memcpy(g + at, vh, 8); memcpy(g + at + 8, v, 48); at += 8 + 48;
    len[1] = at;
}
static void TestZoneRule(void)
{
    Fresh("zone-rule");
    GameDir();
    TestGlideState();
    static const char wstr[] = "\0Azeroth\0Elwynn\0Inner\0Westfall\0Ironforge\0DunMorogh\0";
    unsigned wv[6 * 8] = { 14, 0, 0, 1, TestF(16000), TestF(-19200), TestF(7466), TestF(-16000),
                           30, 0, 12, 9, TestF(1535), TestF(-1935), TestF(-7939), TestF(-10254),
                           99, 0, 77, 16, TestF(200), TestF(-200), TestF(-9000), TestF(-9600),
                           39, 0, 40, 22, TestF(3016), TestF(-483), TestF(-9400), TestF(-11733),
                           341, 0, 1537, 31, TestF(-713), TestF(-1504), TestF(-4569), TestF(-5096),
                           27, 0, 1, 41, TestF(1802), TestF(-3122), TestF(-3877), TestF(-7160) };
    static unsigned char wma[512];
    unsigned wn = TestWdbc(wma, 6, 8, wv, wstr, sizeof wstr);
    ParchTables(wma, wn, NULL, 0);
    unsigned av[9 * 11] = { 0 };                                       /* AreaTable: id, map, parent, bit, flags .. field 10 */
    static const unsigned arow[9][5] = { { 1, 0, 0, 1, 0x40 }, { 12, 0, 0, 2, 0x40 }, { 40, 0, 0, 3, 0x40 }, { 77, 0, 0, 4, 0 },
                                         { 87, 0, 12, 5, 0x40 }, { 131, 0, 1, 6, 0 }, { 600, 0, 0, 7, 0 }, { 1537, 0, 0, 8, 0x138 },
                                         { 2101, 0, 77, 9, 0 } };
    for (int i = 0; i < 9; i++) memcpy(av + 11 * i, arow[i], sizeof arow[i]);
    static unsigned char atb[1024];
    unsigned an = TestWdbc(atb, 9, 11, av, "\0", 1);
    ParchAreas(atb, an);
    static const char mstr[] = "\0Azeroth\0Kalimdor\0";
    unsigned mv[2 * 3] = { 0, 1, 0, 1, 9, 0 };
    static unsigned char mpd[128];
    unsigned mn = TestWdbc(mpd, 2, 3, mv, mstr, sizeof mstr);
    ParchMaps(mpd, mn);
    unsigned wav[4 * 11] = { 0 };                                      /* WMOAreaTable: id, wmo, set, group .. area (field 10) */
    static const unsigned warow[4][11] = { { 1, 208, 0, 0xFFFFFFFFu, 0, 0, 0, 0, 0, 0, 1537 }, { 2, 208, 0, 3, 0, 0, 0, 0, 0, 0, 77 },
                                           { 3, 208, 1, 0xFFFFFFFFu, 0, 0, 0, 0, 0, 0, 2101 }, { 4, 500, 0, 0xFFFFFFFFu, 0, 0, 0, 0, 0, 0, 0 } };
    for (int i = 0; i < 4; i++) memcpy(wav + 11 * i, warow[i], sizeof warow[i]);
    static unsigned char wat[512];
    unsigned wtn = TestWdbc(wat, 4, 11, wav, "\0", 1);
    ParchWmoAreas(wat, wtn);
    int ok = g_mapDirN == 2 && !strcmp(g_mapDir[1].dir, "Kalimdor") && g_waN == 3 && g_waArea[0] == 1537 && g_waGroup[1] == 3 && g_waSet[2] == 1 &&
             WmoArea(208, 0, -1) == 1537 && WmoArea(208, 0, 3) == 77 && WmoArea(208, 1, -1) == 2101 && WmoArea(208, 0, 4) == 0 && WmoArea(500, 0, -1) == 0 &&
             g_atParent[AreaRow(87)] == 12 && g_atFlags[AreaRow(1537)] == 0x138 && AreaZone(87) == 12 && AreaZone(2101) == 77 && AreaZone(131) == 1 &&
             AreaZone(12) == 12 && AreaZone(5555) == -1;
    Check(ok, "the arrival's zone, its tables: Map.dbc's folders, WMOAreaTable.dbc's buildings' areas (each group's and the root's, those naming an area), AreaTable's parents and flags; an area's zone its last parent");

    /* Goldshire (-9464, 62): tile 31_49, chunk 14 across, 11 down; that chunk Goldshire's, the rest of the tile Westfall's;
     * a smaller box (Inner) holds it too */
    unsigned n0 = TestAdt(g_adt[0], 14, 11, 87, 40, NULL, 0, NULL);
    static unsigned char modf[2][64];                                  /* Dun Morogh's tile: the capital's building over a place, an inn over another */
    TestModf(modf[0], -5000, -1000, -4800, -800, 0);
    TestModf(modf[1], -5250, -750, -5150, -650, 1);
    unsigned n1;
    {                                                                  /* (both placements: one file name, two sets) */
        static unsigned char both[128];
        memcpy(both, modf[0], 64); memcpy(both + 64, modf[1], 64);
        n1 = TestAdt(g_adt[1], 0, 0, 1, 1, both, 2, "World\\wmo\\Test\\City.wmo");
    }
    unsigned wl[2];
    TestWmo(7, 0x20, wl);
    static const char *tn[2] = { "World\\Maps\\Azeroth\\Azeroth_31_49.adt", "World\\Maps\\Azeroth\\Azeroth_33_41.adt" };
    const char *td[2] = { (const char *)g_adt[0], (const char *)g_adt[1] };
    unsigned tl[2] = { n0, n1 };
    MakeMpq("Data/terrain.MPQ", 2, tn, td, tl);
    static const char *wn2[2] = { "World\\wmo\\Test\\City.wmo", "World\\wmo\\Test\\City_000.wmo" };
    const char *wd[2] = { (const char *)g_wmoRoot, (const char *)g_wmoGroup };
    MakeMpq("Data/patch-3.MPQ", 2, wn2, wd, wl);
    int way = -1;
    int z = ParchZoneAt(0, -9464, 62, 0, 0, &way), old = ParchFind(0, -9464, 62);
    ok = z == 1 && way == 1 && old == 2;
    z = ParchZoneAt(0, -9464 - 34, 62, 0, 0, &way);                          /* (the next chunk down: the rest of the tile) */
    ok &= z == 3 && way == 1;
    unsigned char *tmpAdt = malloc(n0);                                /* (a list that points at another chunk: not believed) */
    memcpy(tmpAdt, g_adt[0], n0);
    memcpy(tmpAdt + 12 + 8 + 16 * (11 * 16 + 14), tmpAdt + 12 + 8, 4);
    ok &= AdtArea(tmpAdt, n0, 14, 11) == -1 && AdtArea(g_adt[0], n0, 14, 11) == 87;
    free(tmpAdt);
    Check(ok, "the arrival's zone as the client finds it: its terrain chunk's area (Goldshire), walked up to its zone (Elwynn), that zone's page; not the smaller box that also holds it; the chunk beside it its own; a chunk list pointing at another chunk not believed");
    g_parchOn = 1; g_ptry.world = 1; g_worldUp = 0;                    /* the parchment's work takes the arrival so, and says how */
    g_destMap = 0; g_destWX = -9464; g_destWY = 62; g_destZOk = 0; g_viewScreen = 21;
    ParchWork(5000);
    ok = g_parchWant == 1 && g_parchWantCont == 0 && LogSays("the arrival (map 0, -9464, 62) is in Elwynn, by its terrain's area, as the client");
    Check(ok, "the parchment's work: the arrival's zone found as the client finds it, and logged");
    TestGlideState();
    /* the building (City.wmo, its one group over world x -4950..-4850, y -950..-850, from 90 to 200 high, its floor at
     * 100): in it as the client finds it, by the place's height and the group's own faces */
    z = ParchZoneAt(0, -4900, -900, 150, 1, &way);                     /* (over its floor, in its group: its root's area) */
    ok = z == 4 && way == 2;
    z = ParchZoneAt(0, -4900, -900, 150, 0, &way);                     /* (its height not known: the terrain's) */
    ok &= z == 5 && way == 1;
    z = ParchZoneAt(0, -4900, -900, 80, 1, &way);                      /* (under its group: the terrain's) */
    ok &= z == 5 && way == 1;
    z = ParchZoneAt(0, -4900, -900, 250, 1, &way);                     /* (over its group, on the roof: the terrain's) */
    ok &= z == 5 && way == 1;
    z = ParchZoneAt(0, -4980, -980, 150, 1, &way);                     /* (in the placement's bounds, not its group's: its gate) */
    ok &= z == 5 && way == 1;
    z = ParchZoneAt(0, -5200, -700, 150, 1, &way);                     /* (the other placement, the place not in its group) */
    ok &= z == 5 && way == 1;
    z = ParchZoneAt(0, -5100, -1000, 150, 1, &way);                    /* (no building over it: the terrain's) */
    ok &= z == 5 && way == 1;
    g_parchOn = 1; g_ptry.world = 1; g_worldUp = 0;                    /* (the work takes the arrival's height, and says so) */
    g_destMap = 0; g_destWX = -4900; g_destWY = -900; g_destWZ = 150; g_destZOk = 1; g_viewScreen = 22;
    ParchWork(6000);
    ok &= g_parchWant == 4 && LogSays("the arrival (map 0, -4900, -900, 150 high) is in Ironforge, by the building's floor under it, as the client");
    g_destZOk = 0;
    TestGlideState();
    Check(ok, "the arrival's zone (his rule, 11 Oct: \"the way the client does (height, and the building's own groups), not a wider box\"): in a building only over a floor of its own group's faces, inside the group's box; under it, over it, beside it in its placement's bounds (a city's gate), or its height not known: the terrain's");
    g_testTile[0] = 33; g_testTile[1] = 41;                            /* (the terrain with heights: over the floor, or under it) */
    g_testGround = 120;
    { static unsigned char both[128]; memcpy(both, modf[0], 64); memcpy(both + 64, modf[1], 64);
      n1 = TestAdt(g_adt[1], 0, 0, 1, 1, both, 2, "World\\wmo\\Test\\City.wmo"); tl[1] = n1; MakeMpq("Data/terrain.MPQ", 2, tn, td, tl); }
    z = ParchZoneAt(0, -4900, -900, 150, 1, &way);                     /* (the ground between it and the floor: on the ground) */
    ok = z == 5 && way == 1;
    z = ParchZoneAt(0, -4900, -900, 119, 1, &way);                     /* (within AREA_ABOVE of the ground over the floor: the ground) */
    ok &= z == 5 && way == 1;
    g_testGround = 160;
    { static unsigned char both[128]; memcpy(both, modf[0], 64); memcpy(both + 64, modf[1], 64);
      n1 = TestAdt(g_adt[1], 0, 0, 1, 1, both, 2, "World\\wmo\\Test\\City.wmo"); tl[1] = n1; MakeMpq("Data/terrain.MPQ", 2, tn, td, tl); }
    z = ParchZoneAt(0, -4900, -900, 150, 1, &way);                     /* (the ground over it: a city under its mountain) */
    ok &= z == 4 && way == 2;
    g_testGround = -1e30f;
    TestWmo(3, 0x20, wl);                                               /* (its group's own row: that area's zone) */
    MakeMpq("Data/patch-3.MPQ", 2, wn2, wd, wl);
    z = ParchZoneAt(0, -4900, -900, 150, 1, &way);
    ok &= z == 2 && way == 2;
    TestWmo(3, 0x04, wl);                                               /* (faces that do not collide: no floor) */
    MakeMpq("Data/patch-3.MPQ", 2, wn2, wd, wl);
    z = ParchZoneAt(0, -4900, -900, 150, 1, &way);
    ok &= z == 5 && way == 1;
    TestWmo(3, 0x08, wl);                                               /* (collision-only faces: a floor) */
    MakeMpq("Data/patch-3.MPQ", 2, wn2, wd, wl);
    z = ParchZoneAt(0, -4900, -900, 150, 1, &way);
    ok &= z == 2 && way == 2;
    { static unsigned char both[128]; memcpy(both, modf[0], 64); memcpy(both + 64, modf[1], 64);
      n1 = TestAdt(g_adt[1], 0, 0, 1, 1, both, 2, "World\\wmo\\Test\\City.wmo"); tl[1] = n1; MakeMpq("Data/terrain.MPQ", 2, tn, td, tl); }
    Check(ok, "the arrival's zone: the terrain over a building's floor and under the place (or within 2 yards over it) puts it on the ground, the terrain's; the ground over the place (a city under its mountain), the building's; a group's own WMOAreaTable row before its root's; only faces that collide are floors");
    TestModf(modf[0], -5000, -1000, -4800, -800, 3);                  /* (the placement's name set has no row: the terrain's) */
    { static unsigned char both[128]; memcpy(both, modf[0], 64); memcpy(both + 64, modf[1], 64);
      n1 = TestAdt(g_adt[1], 0, 0, 1, 1, both, 2, "World\\wmo\\Test\\City.wmo"); tl[1] = n1; MakeMpq("Data/terrain.MPQ", 2, tn, td, tl); }
    z = ParchZoneAt(0, -4900, -900, 150, 1, &way);
    ok = z == 5 && way == 1;
    n0 = TestAdt(g_adt[0], 14, 11, 600, 600, NULL, 0, NULL);           /* (a zone with no page: its continent's) */
    tl[0] = n0;
    MakeMpq("Data/terrain.MPQ", 2, tn, td, tl);
    z = ParchZoneAt(0, -9464, 62, 0, 0, &way);
    ok &= z == 0 && way == 3;
    MakeMpq("Data/terrain.MPQ", 1, tn + 1, td + 1, tl + 1);            /* (no tile's file: the boxes, as before) */
    z = ParchZoneAt(0, -9464, 62, 0, 0, &way);
    ok &= z == 2 && way == 0;
    z = ParchZoneAt(1, -9464, 62, 0, 0, &way);                               /* (Kalimdor's: no such tile) */
    ok &= z == -1 && way == 0;
    g_mapDirN = 0;                                                     /* (no folder for the map: the boxes) */
    ok &= ParchZoneAt(0, -9464, 62, 0, 0, &way) == 2 && way == 0;
    g_mapDirN = 2;
    unsigned char bad[64];                                             /* a tile's file that is not one: the boxes */
    memset(bad, 0x41, sizeof bad);
    ok &= AdtArea(bad, sizeof bad, 14, 11) == -1 && AdtArea(g_adt[1], n1, 16, 0) == -1;
    Check(ok, "the arrival's zone: a building whose name set has no row (its group's nor its root's) leaves the terrain's; a zone with no page, its continent's (as the client's map shows); no tile's file, no folder for the map, or a file that is not one: the smallest box, as before");
    g_mapDirN = 0; g_waN = 0;
}
/* The cut (his pick, 11 Oct: "go with the COMBO", grown so "the blur doesn't take away anything from inside the zone"):
 * a painting cut to its own area as the coarser one under it draws it, whole and sharp inside, fading and softening only
 * in the grown margin past its border */
static void TestPaintNew(int K)
{
    if (g_parchPx) HeapFree(GetProcessHeap(), 0, g_parchPx);
    g_parchK = K; g_parchTW = 1024 * K;
    g_parchPx = (unsigned char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)g_parchTW * g_parchTW * 4);
}
static void TestPaintFill(int x0, int y0, int x1, int y1, unsigned rgb)
{
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        unsigned char *p = g_parchPx + 4 * ((size_t)y * g_parchTW + x);
        p[0] = (unsigned char)(rgb >> 16); p[1] = (unsigned char)(rgb >> 8); p[2] = (unsigned char)rgb; p[3] = 255;
    }
}
static const unsigned char *TestPaintAt(int x, int y) { return g_parchPx + 4 * ((size_t)y * g_parchTW + x); }
static void TestCut(void)
{
    /* the coarser painting (a continent's, 256-px tiles): parchment, a zone outlined in dark ink (a gap of one texel in its
     * border, closed), a lake inside drawn in ink, a neighbour beside it */
    TestPaintNew(1);
    TestPaintFill(0, 0, 1002, 668, 0xC8A060);
    TestPaintFill(300, 200, 600, 400, 0x302010);                     /* (the zone's border: a 4-texel line) */
    TestPaintFill(304, 204, 596, 396, 0xC0A070);
    TestPaintFill(450, 200, 451, 204, 0xC8A060);                      /* (a one-texel gap in it) */
    TestPaintFill(400, 280, 440, 320, 0x302010);                      /* (a lake in ink: a hole) */
    TestPaintFill(404, 284, 436, 316, 0x6080A0);
    ParchRect rc = { 0, 0, 10.02f, 6.68f };                           /* (a texel a hundredth of a tile) */
    ParchLinesKeep(&g_linesCont, rc, 0, 1);
    int ok = g_linesCont.m && g_linesCont.w == 1002 && g_linesCont.h == 668 && g_linesCont.row == 0 &&
             (g_linesCont.m[201 * 1002 + 350] & CUT_DARK) && !(g_linesCont.m[300 * 1002 + 350] & CUT_DARK) && (g_linesCont.m[300 * 1002 + 350] & CUT_INK) == 0;
    TestPaintFill(700, 100, 900, 104, 0x908060);                      /* (a border drawn lighter, darker only than round it) */
    ParchLinesKeep(&g_linesCont, rc, 0, 1);
    ok &= (g_linesCont.m[102 * 1002 + 800] & CUT_DARK) && !(g_linesCont.m[120 * 1002 + 800] & CUT_DARK);
    Check(ok, "the cut: the coarser painting kept as its lines (dark, or darker than round them by PARCH_CUT_REL; ink), a byte a texel");
    TestPaintFill(700, 100, 900, 104, 0xC8A060);
    ParchLinesKeep(&g_linesCont, rc, 0, 1);
    /* the finer painting (the zone's, 256-px tiles): its box on the frame 2.5 .. 6.5 x 1.5 .. 4.5 tiles: the outline
     * (3.0..6.0 x 2.0..4.0) maps to its texels 125..877 x 148..519 */
    ParchRect rz = { 2.5f, 1.5f, 6.5f, 4.5f };
    TestPaintNew(1);
    for (int y = 0; y < 668; y++) for (int x = 0; x < 1002; x++) {   /* (a fine checker: its detail) */
        unsigned char *p = g_parchPx + 4 * ((size_t)y * g_parchTW + x);
        int v = ((x / 3 + y / 3) & 1) ? 200 : 60;
        p[0] = (unsigned char)v; p[1] = (unsigned char)(255 - v); p[2] = 90; p[3] = 255;
    }
    static unsigned char before[1002 * 668 * 4];
    for (int y = 0; y < 668; y++) memcpy(before + 4 * (size_t)y * 1002, g_parchPx + 4 * (size_t)y * g_parchTW, 1002 * 4);
    float seeds[4] = { 420, 260, 330, 230 }, share = 0;               /* (the first in the lake: its hole filled anyway) */
    const char *why = "";
    ok = ParchCut(&g_linesCont, rz, seeds, 2, 0, &share, &why) && !g_cutBoxed;
    int grow = PARCH_CUT_GROW / 2;                                    /* (a 256-px painting: half an HD one's) */
    int sharpIn = 1, wholeIn = 1;
    for (int y = 160; y < 505; y += 7) for (int x = 140; x < 862; x += 7) {   /* inside its border: whole, untouched */
        const unsigned char *p = TestPaintAt(x, y), *b = before + 4 * ((size_t)y * 1002 + x);
        if (p[3] != 255) wholeIn = 0;
        if (p[0] != b[0] || p[1] != b[1]) sharpIn = 0;
    }
    int lake = TestPaintAt(426, 334)[3] == 255 && TestPaintAt(500, 333)[3] == 255;   /* (the lake's place inside) */
    int xb = 135;                                                     /* (its border, inside the ink: 3.04 tiles, (3.04 - 2.5) / 4 of 1002) */
    float aEdge = TestPaintAt(xb - 2, 300)[3] / 255.0f, aMid = TestPaintAt(xb - grow / 2, 300)[3] / 255.0f, aOut = TestPaintAt(xb - grow - 3, 300)[3] / 255.0f;
    float bEdge = (float)abs((int)TestPaintAt(xb - 2, 300)[0] - (int)before[4 * (300 * 1002 + xb - 2)]);
    float bOut = (float)abs((int)TestPaintAt(xb - grow + 4, 300)[0] - (int)before[4 * (300 * 1002 + xb - grow + 4)]);
    ok &= sharpIn && wholeIn && lake && aEdge > 0.97f && aMid > 0.35f && aMid < 0.65f && aOut == 0 && bEdge < 20 && bOut > 30 &&
          share > 0.3f && share < 0.5f;
    printf("  the cut: alpha just past its border %.3f, halfway out %.3f, past the grown line %.3f; its colours softened by %.0f just past it, %.0f near the line\n",
           aEdge, aMid, aOut, bEdge, bOut);
    Check(ok, "the cut: inside the zone's own border (as its continent draws it, its lake's hole filled) the painting is whole and untouched; past it, it fades from whole at the border to nothing at the outline grown PARCH_CUT_GROW, its colours softened only out there, more toward the line");
    /* a border the painting draws open: every fill leaks; kept to its own box */
    TestPaintNew(1);
    TestPaintFill(0, 0, 1002, 668, 0xC8A060);
    TestPaintFill(300, 200, 600, 400, 0x302010);
    TestPaintFill(304, 204, 596, 396, 0xC0A070);
    TestPaintFill(440, 200, 470, 204, 0xC8A060);                      /* (a gap of thirty texels: open) */
    ParchLinesKeep(&g_linesCont, rc, 0, 1);
    TestPaintNew(1);
    TestPaintFill(0, 0, 1002, 668, 0x80C040);
    ok = ParchCut(&g_linesCont, rz, seeds + 2, 1, 0, &share, &why) && g_cutBoxed && strstr(why, "own box");
    TestPaintNew(1);                                                  /* (no lines at all: nothing, and why) */
    ParchLines none = { 0 };
    ok &= !ParchCut(&none, rz, seeds, 2, 0, &share, &why) && strstr(why, "no lines");
    Check(ok, "the cut: a zone whose border the painting under it draws open is kept to its own box (and the log says so); with no painting under it, none");
    /* a continent over the world: every seed's piece of land that holds, together; a seed at sea leaks and is left out */
    TestPaintNew(1);
    TestPaintFill(0, 0, 1002, 668, 0x707060);                         /* (the sea) */
    TestPaintFill(100, 100, 300, 300, 0x302010); TestPaintFill(105, 105, 295, 295, 0xC8A060);   /* (two islands, their coasts) */
    TestPaintFill(400, 150, 600, 450, 0x302010); TestPaintFill(405, 155, 595, 445, 0xC8A060);
    ParchRect rw = { 0, 0, 10.02f, 6.68f };
    ParchLinesKeep(&g_linesWorld, rw, -2, 1);
    TestPaintNew(1);
    TestPaintFill(0, 0, 1002, 668, 0x40A040);
    float cs[6] = { 200, 200, 500, 300, 800, 600 };
    ok = ParchCut(&g_linesWorld, rw, cs, 3, 1, &share, &why) && TestPaintAt(200, 200)[3] == 255 && TestPaintAt(500, 300)[3] == 255 &&
         TestPaintAt(800, 600)[3] == 0 && TestPaintAt(350, 200)[3] == 0 && share > 0.1f && share < 0.2f;
    Check(ok, "the cut, a continent over the world: every piece of its land a seed finds (two islands here), the sea a seed leaks into left out");
    /* the outline's own checks (a page with no land of its own, an instance's map, is still cut so; the skeptic's three
     * mutants on 89d75cf): the box 250..650 x 150..450 of the painting under it, its crop 46..854 x -4..604 */
    float one[2] = { 450, 300 };
    TestPaintNew(1);                                                  /* (a region far bigger than its box: not the zone alone) */
    TestPaintFill(0, 0, 1002, 668, 0xC8A060);
    TestPaintFill(60, 20, 840, 580, 0x302010); TestPaintFill(64, 24, 836, 576, 0xC8A060);
    ParchLinesKeep(&g_linesCont, rc, 0, 1);
    TestPaintNew(1); TestPaintFill(0, 0, 1002, 668, 0x80C040);
    ok = ParchCut(&g_linesCont, rz, one, 1, 0, &share, &why) && g_cutBoxed;
    TestPaintNew(1);                                                  /* (only a lake found: too little to be the zone) */
    TestPaintFill(0, 0, 1002, 668, 0xC8A060);
    TestPaintFill(400, 280, 440, 320, 0x302010); TestPaintFill(404, 284, 436, 316, 0xC8A060);
    ParchLinesKeep(&g_linesCont, rc, 0, 1);
    TestPaintNew(1); TestPaintFill(0, 0, 1002, 668, 0x80C040);
    float pond[2] = { 420, 300 };
    ok &= !ParchCut(&g_linesCont, rz, pond, 1, 0, &share, &why) && strstr(why, "too little");
    TestPaintNew(1);                                                  /* (its region grown into a faint border line out to the crop's
                                                                       * edge: not trusted) */
    TestPaintFill(0, 0, 1002, 668, 0xC8A060);
    TestPaintFill(47, 150, 400, 452, 0x585850); TestPaintFill(49, 152, 398, 450, 0xC8A060);
    ParchLinesKeep(&g_linesCont, rc, -1, 1);
    TestPaintNew(1); TestPaintFill(0, 0, 1002, 668, 0x80C040);
    float west[2] = { 300, 300 };
    ok &= ParchCut(&g_linesCont, rz, west, 1, 0, &share, &why) && g_cutBoxed;
    Check(ok, "the outline (a page with no land of its own): a fill far bigger than its box, one too small to be the zone, or a region grown out to the crop's edge is not trusted: kept to its box, or none");
    if (g_linesCont.m) HeapFree(GetProcessHeap(), 0, g_linesCont.m);
    if (g_linesWorld.m) HeapFree(GetProcessHeap(), 0, g_linesWorld.m);
    g_linesCont.m = g_linesWorld.m = NULL;
    ParchFree();
}
/* A zone's land as its terrain gives it, and the cut to it (his rule, 11 Oct: the cut must not "take away anything from
 * inside the zone"; the skeptic's finds: its own town cut away, a cut run on past its edge). One tile (31_50) is the
 * zone's box: its chunks 62.6 texels across and 41.75 down on its 256-px painting, 20.9 x 13.9 on its continent's (a
 * painting three tiles each way, the zone's in the middle). */
static const unsigned *TestLandAreas(void)
{
    static unsigned a[256];
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            unsigned v = 40;                                           /* (its neighbour's) */
            if (i >= 2 && i < 8 && j >= 2 && j < 14) v = 87;           /* (its land: a town's area, its zone Elwynn's) */
            if (i >= 2 && i < 8 && j == 14) v = 12;                    /* (its own sea) */
            if (i >= 11 && i < 13 && j >= 3 && j < 5) v = 12;          /* (an island of its own) */
            a[j * 16 + i] = v;
        }
    return a;
}
/* the continent's painting (1002 x 668 over 3 x 3 tiles): parchment, the zone's border a 4-texel line in ink 0.4 of a
 * chunk past its land (P x 368..512, y 244..426); what: 0 closed, a river across its land; 1 a 20-texel gap on the right
 * into a neighbour closed off inside the crop; 2 a 6-texel gap into a corridor running out past the crop; 3 its region all
 * ink but a pocket at its left edge */
static void TestLandCont(int what)
{
    TestPaintNew(1);
    TestPaintFill(0, 0, 1002, 668, 0xC8A060);
    if (what == 3) {
        TestPaintFill(350, 232, 524, 440, 0x302010);
        TestPaintFill(362, 296, 388, 316, 0xC8A060);
    } else {
        TestPaintFill(368, 244, 512, 426, 0x302010);
        TestPaintFill(372, 248, 508, 422, 0xC8A060);
        if (what == 0) TestPaintFill(440, 248, 443, 422, 0x302010);
        if (what == 1) {
            TestPaintFill(508, 230, 704, 440, 0x302010); TestPaintFill(512, 234, 700, 436, 0xC8A060);
            TestPaintFill(508, 320, 512, 340, 0xC8A060);
        }
        if (what == 2) {
            TestPaintFill(508, 316, 1002, 331, 0x302010); TestPaintFill(508, 320, 1002, 327, 0xC8A060);
        }
        if (what == 4) {                                               /* (its right border two chunks past its land) */
            TestPaintFill(508, 248, 512, 422, 0xC8A060);
            TestPaintFill(512, 244, 547, 248, 0x302010); TestPaintFill(512, 422, 547, 426, 0x302010); TestPaintFill(543, 244, 547, 426, 0x302010);
        }
    }
    ParchRect rc = { 29.3125f, 37.875f, 32.3125f, 40.875f };
    ParchLinesKeep(&g_linesCont, rc, 0, 1);
}
/* another archive (a patch with one small file of its own: the archives' print differs) */
static void TestLandPatch(const char *rel, const char *text)
{
    static const char *nm[1] = { "Interface\\Test\\x.txt" };
    const char *d[1] = { text };
    unsigned l[1] = { (unsigned)strlen(text) };
    MakeMpq(rel, 1, nm, d, l);
    g_landPrint = 0;
}
static void TestLandPaint(void)
{
    TestPaintNew(1);
    TestPaintFill(0, 0, 1002, 668, 0x80C040);
}
static void TestLand(void)
{
    Fresh("land");
    GameDir();
    TestGlideState();
    g_wmc[0].ok = 0;                                                   /* (the frame: map tiles, no continent's offset) */
    static const char wstr[] = "\0Azeroth\0Elwynn\0Ironforge\0";
    unsigned wv[3 * 8] = { 14, 0, 0, 1, TestF(16000), TestF(-19200), TestF(7466), TestF(-16000),
                           30, 0, 12, 9, TestF(533.33333f), TestF(0), TestF(-9600), TestF(-10133.333f),
                           341, 0, 1537, 17, TestF(533.33333f), TestF(0), TestF(-9600), TestF(-10133.333f) };
    static unsigned char wma[256];
    unsigned wn = TestWdbc(wma, 3, 8, wv, wstr, sizeof wstr);
    ParchTables(wma, wn, NULL, 0);
    unsigned av[5 * 11] = { 0 };
    static const unsigned arow[5][5] = { { 12, 0, 0, 1, 0x40 }, { 40, 0, 0, 2, 0x40 }, { 87, 0, 12, 3, 0x40 }, { 1537, 0, 0, 4, 0x138 }, { 9, 0, 1537, 5, 0 } };
    for (int i = 0; i < 5; i++) memcpy(av + 11 * i, arow[i], sizeof arow[i]);
    static unsigned char atb[512];
    unsigned an = TestWdbc(atb, 5, 11, av, "\0", 1);
    ParchAreas(atb, an);
    static const char mstr[] = "\0Azeroth\0";
    unsigned mv[3] = { 0, 1, 0 };
    static unsigned char mpd[64];
    ParchMaps(mpd, TestWdbc(mpd, 1, 3, mv, mstr, sizeof mstr));
    unsigned wav[11] = { 1, 208, 0, 0xFFFFFFFFu, 0, 0, 0, 0, 0, 0, 1537 };
    static unsigned char wat[128];
    ParchWmoAreas(wat, TestWdbc(wat, 1, 11, wav, "\0", 1));
    g_testAreas = TestLandAreas();
    static unsigned char sea[256];
    for (int i = 2; i < 8; i++) sea[14 * 16 + i] = 1;
    g_testSea = sea; g_testGround = 10; g_testTile[0] = 31; g_testTile[1] = 50;
    unsigned n0 = TestAdt(g_adt[0], 0, 0, 0, 0, NULL, 0, NULL);
    static const char *tn[1] = { "World\\Maps\\Azeroth\\Azeroth_31_50.adt" };
    const char *td[1] = { (const char *)g_adt[0] };
    unsigned tl[1] = { n0 };
    MakeMpq("Data/terrain.MPQ", 1, tn, td, tl);
    g_landPrint = 0;

    /* its land: the chunks of its box whose area's zone is its own, the sea not; kept in its file, read back from it */
    ParchLand L = { 0 };
    const char *why = "";
    int ok = ParchLandMake(1, &L, &why) && L.n == 6 * 12 + 2 * 2 && L.tiles == 1 && !L.fromFile && L.gx0 == 495 && L.gy0 == 799 &&
             LandOwn(&L, 496 + 2, 800 + 2) && LandOwn(&L, 496 + 7, 800 + 13) && !LandOwn(&L, 496 + 8, 800 + 2) && !LandOwn(&L, 496 + 1, 800 + 2) &&
             LandOwn(&L, 496 + 11, 800 + 3) && !LandOwn(&L, 496 + 3, 800 + 14) && L.m[(800 + 14 - 799) * L.w + 496 + 3 - 495] == 2 &&
             Get("Data/CinderLoad/land-Elwynn.bin") != NULL;
    ParchLand L2 = { 0 };
    ok &= ParchLandMake(1, &L2, &why) && L2.fromFile && L2.n == L.n && L2.tiles == 0 && !memcmp(L2.m, L.m, (size_t)L.w * L.h);
    TestLandPatch("Data/patch-2.MPQ", "x");                            /* (another archive: the print differs, made again) */
    ok &= ParchLandMake(1, &L2, &why) && !L2.fromFile && L2.n == L.n;
    g_pz[1].t = -9500;                                                 /* (another box: made again) */
    ok &= ParchLandMake(1, &L2, &why) && !L2.fromFile && L2.n == L.n;
    g_pz[1].t = -9600;
    ok &= ParchLandMake(1, &L2, &why) && !L2.fromFile;
    ok &= ParchLandMake(1, &L2, &why) && L2.fromFile;
    Check(ok, "a zone's land as its terrain gives it: every chunk of its box whose area's zone is its own (a town's, an island's), not the sea under its own coast's area; made once and kept in Data/CinderLoad/land-<zone>.bin, read back from it while the client's archives and its box are as they were, made again when either changes");
    /* a city inside its building: its placement's chunks */
    static unsigned char modf[64];
    TestModf(modf, -9900, 200, -9800, 300, 0);
    g_testAreas = NULL; g_testSea = NULL;
    unsigned n1 = TestAdt(g_adt[1], 0, 0, 40, 40, modf, 1, "World\\wmo\\Test\\City.wmo");
    unsigned wl[2];
    TestWmo(7, 0x20, wl);
    static const char *wn2[3] = { "World\\Maps\\Azeroth\\Azeroth_31_50.adt", "World\\wmo\\Test\\City.wmo", "World\\wmo\\Test\\City_000.wmo" };
    const char *wd[3] = { (const char *)g_adt[1], (const char *)g_wmoRoot, (const char *)g_wmoGroup };
    unsigned wl3[3] = { n1, wl[0], wl[1] };
    MakeMpq("Data/terrain.MPQ", 1, wn2, wd, wl3);
    MakeMpq("Data/wmo.MPQ", 2, wn2 + 1, wd + 1, wl3 + 1);
    g_landPrint = 0;
    ParchLand Lc = { 0 };
    float gx, gy;
    LandChunkAt(-9850, 250, &gx, &gy);
    ok = ParchLandMake(2, &Lc, &why) && Lc.n >= 9 && Lc.n <= 16 && LandOwn(&Lc, (int)gx, (int)gy) && !LandOwn(&Lc, (int)gx + 5, (int)gy);
    g_waArea[0] = 40;                                                  /* (its building another zone's: none) */
    TestLandPatch("Data/patch-3.MPQ", "xy");
    ok &= ParchLandMake(2, &Lc, &why) && Lc.n == 0;
    g_waArea[0] = 1537;
    Check(ok, "a city inside its building (Ironforge, Undercity: no terrain of its own): its land the chunks its building's placement covers, when the building's WMOAreaTable rows name the city's area");
    ParchLandFree(&Lc); ParchLandFree(&L2);
    MakeMpq("Data/terrain.MPQ", 1, tn, td, tl);
    TestLandPatch("Data/patch-3.MPQ", "xyz");
    g_testAreas = TestLandAreas(); g_testSea = sea;
    ParchLandMake(1, &L, &why);

    /* the cut to it: its land whole, the outline refining its edge within a chunk, nothing far past it */
    ParchRect rz = ParchBox(1);
    float share = 0;
    int held = 0;
    TestLandCont(0);
    TestLandPaint();
    ok = ParchLandCut(&g_linesCont, 0, rz, &L, &share, &held, &why) && held >= 2;
    int whole = 1;
    for (int y = 84; y < 584; y += 9) for (int x = 126; x < 500; x += 9) if (TestPaintAt(x, y)[3] != 255) whole = 0;   /* (its land: whole, the river too) */
    for (int y = 126; y < 208; y += 9) for (int x = 690; x < 813; x += 9) if (TestPaintAt(x, y)[3] != 255) whole = 0;   /* (its island, no outline round it) */
    ok &= whole && TestPaintAt(118, 300)[3] == 255 && TestPaintAt(100, 300)[3] < 255 && TestPaintAt(60, 300)[3] == 0 &&   /* (its outline 11 texels past its land) */
          TestPaintAt(300, 595)[3] == 255 && TestPaintAt(300, 625)[3] < 255 && TestPaintAt(900, 350)[3] == 0 && share > 0.25f && share < 0.4f;
    TestLandCont(4);                                                   /* (an outline drawn two chunks past its land: a chunk of it) */
    TestLandPaint();
    ok &= ParchLandCut(&g_linesCont, 0, rz, &L, &share, &held, &why) && held >= 1 && TestPaintAt(540, 300)[3] == 255 && TestPaintAt(600, 300)[3] < 128;
    Check(ok, "the cut to its land: every texel of the zone's land whole (crossed by a river in ink, an island with no outline round it); past it, the outline its continent's painting draws refines its edge within a chunk, never further; its own sea and its neighbour's land past the grown margin, nothing");
    /* a fill that runs through a gap into a neighbour (closed off inside the crop) is not the zone's: its land alone */
    TestLandCont(1);
    TestLandPaint();
    ok = ParchLandCut(&g_linesCont, 0, rz, &L, &share, &held, &why) && held == 0;
    ok &= TestPaintAt(118, 300)[3] < 255 && TestPaintAt(140, 300)[3] == 255 && TestPaintAt(540, 300)[3] < 128;
    /* one that runs on out of the crop (a corridor, little of it far): not the zone's either */
    TestLandCont(2);
    TestLandPaint();
    ok &= ParchLandCut(&g_linesCont, 0, rz, &L, &share, &held, &why) && held == 0 && TestPaintAt(118, 300)[3] < 255 && TestPaintAt(140, 300)[3] == 255;
    /* a fill that finds only a pocket (a lake, a town in ink): too little of the land to trust */
    TestLandCont(3);
    TestLandPaint();
    ok &= ParchLandCut(&g_linesCont, 0, rz, &L, &share, &held, &why) && held == 0 && TestPaintAt(110, 250)[3] < 255 && TestPaintAt(140, 250)[3] == 255;
    Check(ok, "the cut stops at the zone's edge: a fill that runs through a gap in its border into a neighbour (much of it more than three chunks from its land), or on out past the crop, or that finds only a pocket of it is not trusted: its land alone, whole, the rest in the margin");
    /* its overlay pieces as the client lays them, whole wherever they lie (a capital's piece on its zone's map) */
    TestLandCont(0);
    TestLandPaint();
    g_pieceA = (unsigned char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)g_parchTW * g_parchTW);
    for (int y = 280; y < 320; y++) for (int x = 880; x < 940; x++) g_pieceA[y * g_parchTW + x] = 200;
    for (int y = 280; y < 320; y++) for (int x = 950; x < 960; x++) g_pieceA[y * g_parchTW + x] = 100;
    ok = ParchLandCut(&g_linesCont, 0, rz, &L, &share, &held, &why) && TestPaintAt(910, 300)[3] == 255 && TestPaintAt(955, 300)[3] < 255 &&
         TestPaintAt(900, 450)[3] == 0;
    HeapFree(GetProcessHeap(), 0, g_pieceA); g_pieceA = NULL;
    Check(ok, "the cut to its land keeps the zone's overlay pieces whole where the client lays them (their alpha half or more): the map it shows for the zone, a capital's piece on its zone's map too");
    /* the work: a zone with land cut to it (and logged), one with none the outline as before */
    g_parchOn = 1; g_ptry.world = 1; g_worldUp = 0; g_destZOk = 0;
    TestLandCont(0);
    ParchZoneMade(1, 0, 30.8f, 39.3f, &why);
    ok = g_zoneCut && g_cutLand && g_land.n == L.n;
    g_testAreas = NULL; g_testSea = NULL;
    n0 = TestAdt(g_adt[0], 0, 0, 40, 40, NULL, 0, NULL);
    tl[0] = n0;
    MakeMpq("Data/terrain.MPQ", 1, tn, td, tl);
    TestLandPatch("Data/patch-3.MPQ", "xyzw");
    TestLandPaint();
    ParchZoneMade(1, 0, 30.8f, 39.3f, &why);
    ok &= !g_cutLand && g_land.n == 0;
    Check(ok, "the zone's cut: to its land when its terrain gives it some; none (an instance's map), the outline as before");
    ParchLandFree(&L); ParchLandFree(&g_land);
    g_testGround = -1e30f; g_testTile[0] = g_testTile[1] = 0;
    if (g_linesCont.m) HeapFree(GetProcessHeap(), 0, g_linesCont.m);
    g_linesCont.m = NULL;
    ParchFree();
    g_mapDirN = 0; g_waN = 0;
    TestGlideState();
}
/* The continent's name, one label in one place (his note, 11 Oct): the world's kept, cut out of its painting; the
 * continent's own painted out of its painting with the world's painting there */
static void TestWordLetters(int x0, int y0, int x1, int y1, int K)
{
    /* a word's letters as the client paints them: dark strokes, each between light rims; stems three of K wide */
    for (int x = x0 + 6 * K; x + 8 * K < x1; x += 14 * K) {
        TestPaintFill(x - 2 * K, y0 + 3 * K, x + 5 * K, y1 - 3 * K, 0xF0E0C0);   /* (its rim) */
        TestPaintFill(x, y0 + 5 * K, x + 3 * K, y1 - 5 * K, 0x302010);           /* (its stroke) */
    }
}
static void TestWords(void)
{
    /* the letters: strokes between rims found; the sea's hatching (dark lines, no rims) not */
    TestPaintNew(1);
    TestPaintFill(0, 0, 1002, 668, 0x908060);
    int x0 = 650, y0 = 360, x1 = 820, y1 = 388;                       /* (a word's box) */
    for (int y = y0 - 30; y < y1 + 30; y += 5) TestPaintFill(x0 - 60, y, x0 - 5, y + 2, 0x403020);   /* (hatching beside it) */
    TestWordLetters(x0, y0, x1, y1, 1);
    static unsigned char m[400 * 100];
    float sh = ParchLetters(g_parchPx, g_parchTW, 1002, 668, x0, y0, x1, y1, 1, 1, m);
    int bw = x1 - x0, onStroke = m[(14 - y0 + y0) * bw + 7] > 200, offStroke = m[(14) * bw + 13] < 60;
    float hs = ParchLetters(g_parchPx, g_parchTW, 1002, 668, x0 - 60, y0, x0 - 5, y1, 1, 1, m);
    int ok = sh > 0.12f && sh < 0.75f && onStroke && offStroke && hs == 0;
    Check(ok, "the names' letters: each stroke dark between two light rims, as the client paints its names, found; the sea's hatching (dark, no rims) not");
    /* the world's names cut out, the continent's own painted out with the world's painting there, and the name drawn over
     * every painting at the world's place */
    TestGlideState();
    static const char wstr[] = "\0Azeroth\0Elwynn\0Kalimdor\0";
    unsigned wv[3 * 8] = { 14, 0, 0, 1, TestF(16000), TestF(-19200), TestF(7466), TestF(-16000),
                           30, 0, 12, 9, TestF(1535), TestF(-1935), TestF(-7939), TestF(-10254),
                           13, 1, 0, 23, TestF(17066), TestF(-19733), TestF(12799), TestF(-11733) };
    static unsigned char wma[256];
    unsigned wmn = TestWdbc(wma, 3, 8, wv, wstr, sizeof wstr);
    ParchTables(wma, wmn, NULL, 0);
    TestPaintNew(1);                                                  /* the world's painting: its two names */
    TestPaintFill(0, 0, 1002, 668, 0x908060);
    for (int i = 0; i < PARCH_WORDS; i++) {
        int a0, b0, a1, b1;
        if (lstrcmpiA(kWords[i].folder, "World") || !WordBox(i, 1, 1002, 668, &a0, &b0, &a1, &b1)) continue;
        TestWordLetters(a0, b0, a1, b1, 1);
    }
    static unsigned char world[1002 * 668 * 4];
    for (int y = 0; y < 668; y++) memcpy(world + 4 * (size_t)y * 1002, g_parchPx + 4 * (size_t)y * g_parchTW, 1002 * 4);
    int conts[2] = { 0, 2 };
    g_worldK = 1;
    ParchWorldWords(conts);
    ok = g_wordPic[0].ok && g_wordPic[1].ok && g_wordUnder[0].ok && g_wordPic[0].x0 <= 658 && g_wordPic[0].x0 + g_wordPic[0].w >= 815;
    int sx = 658 + 6 + 1 - g_wordPic[0].x0, sy = 375 - g_wordPic[0].y0;   /* (a stroke of EASTERN) */
    ok &= g_wordPic[0].px[4 * (sy * g_wordPic[0].w + sx) + 3] > 200 && g_wordPic[0].px[4 * (sy * g_wordPic[0].w + sx)] == 0x30;
    Check(ok, "the world's names, cut out of its painting: each a picture of the painting's own texels, its alpha the letters");
    TestPaintNew(1);                                                  /* Azeroth's painting: its own name, larger and elsewhere */
    TestPaintFill(0, 0, 1002, 668, 0xB09050);
    for (int i = 0; i < PARCH_WORDS; i++) {
        int a0, b0, a1, b1;
        if (lstrcmpiA(kWords[i].folder, "Azeroth") || !WordBox(i, 1, 1002, 668, &a0, &b0, &a1, &b1)) continue;
        TestWordLetters(a0, b0, a1, b1, 1);
    }
    ParchLinesKeep(&g_linesCont, ParchBox(0), 0, 1);
    int a0, b0, a1, b1;
    WordBox(3, 1, 1002, 668, &a0, &b0, &a1, &b1);                     /* (Azeroth's EASTERN) */
    int lx = a0 + 6 + 1, ly = (b0 + b1) / 2;
    int was = TestPaintAt(lx, ly)[0];
    ok = ParchContWords(0, &g_linesCont) && was == 0x30 && TestPaintAt(lx, ly)[0] > 0x60 && (g_linesCont.m[ly * 1002 + lx] & CUT_WORD) &&
         TestPaintAt(20, 20)[0] == 0xB0;
    Check(ok, "a continent's own name painted out of its painting, with the world's painting there in its own colours; its letters marked so a zone's outline may take them back; the rest of it untouched");
    /* the name drawn: over the zone's painting, at the world's place on the frame, the same place every frame */
    g_viewScreen = 30; g_worldUp = 1; g_viewTex[VT_PWORLD] = 1; g_worldTW = 1024; g_worldK = 1; g_worldAt = 0;
    g_wordUp[0] = 1; g_viewTex[VT_PWORD0] = 1; g_wordTW[0] = 256; g_wordTH[0] = 64; g_contWords = 0;
    TestGlideArrive(0, 1, 0, -9464, 62, 2000);
    ViewBuild(1920, 1080, 0.0f, 2000);
    float px0[3];
    int order = 1, nb = -1;
    for (int f = 0; f < 3; f++) {
        ViewBuild(1920, 1080, 0.04f * (float)f + 0.02f, 2600 + 400 * f);   /* (while it is on the window) */
        nb = TestBatch(VT_PWORD0);
        int zb = TestBatch(VT_PARCH), cb = TestBatch(VT_PARCH2);
        order &= nb >= 0 && nb > zb && nb > cb;
        const ViewVert *v0 = nb >= 0 ? g_vv + g_vb[nb].first : NULL;
        float fxw = v0 ? (float)(g_pcam.v.x + (v0->x - 960) / g_pcam.v.s) : -1;   /* (its first corner back on the frame) */
        px0[f] = fxw;
    }
    ParchRect want = { g_wordPic[0].x0 * PARCH_WORLD_FW / 1002.0f, 0, 0, 0 };
    ok = order && Near(px0[0], want.x0, 0.02) && Near(px0[1], px0[0], 0.02) && Near(px0[2], px0[0], 0.02);
    nb = TestBatch(VT_PWORD0);
    ViewBuild(1920, 1080, 0.02f, 2600);
    nb = TestBatch(VT_PWORD0);
    float vmax = 0;
    for (int i = nb >= 0 ? g_vb[nb].first : 0; nb >= 0 && i < g_vb[nb].first + g_vb[nb].count; i++) if (g_vv[i].v > vmax) vmax = g_vv[i].v;
    ok &= Near(vmax, (double)g_wordPic[0].h / 64, 1e-4);              /* (its texture's height its own) */
    g_contWords = -1;                                                  /* (its own name not painted out: no second one drawn) */
    TestGlideArrive(0, 1, 0, -9464, 62, 2000);
    ViewBuild(1920, 1080, 0.0f, 2000);
    ViewBuild(1920, 1080, 0.02f, 2600);
    ok &= TestBatch(VT_PWORD0) < 0;
    Check(ok, "the continent's name, the world's, drawn over every painting (the zone's too) at its own place on the frame, the same place every frame of the glide; none drawn over a continent whose own name was not painted out");
    /* it fades out once wider than the window (his answer, 11 Oct: "yes the continent name should fade out"): whole up to
     * as wide as the window, half at 1.41 times, gone at twice that and by the landing at the latest */
    g_contWords = 0;
    {
        float nw = g_wordPic[0].w * PARCH_WORLD_FW / 1002.0f;          /* (its width on the frame, map tiles) */
        int whole = 1, gone = 1, half = 0, seen = 0, fades[2] = { 0, 0 };
        for (int run = 0; run < 2; run++) {                            /* Elwynn; then at sea on the name itself (a landing zoom
                                                                        * under twice its own: gone by the landing all the same) */
            if (run) TestGlideArrive(0, 0, 0, -7200, 577, 2000); else TestGlideArrive(0, 1, 0, -9464, 62, 2000);
            ViewBuild(1920, 1080, 0.0f, 2000);
            for (int f = 1; f <= 100; f++) {
                ViewBuild(1920, 1080, f / 100.0f, 2600 + 20 * f);
                float wpx = nw * (float)g_pcam.v.s, r = wpx / 1920;
                int b = TestBatch(VT_PWORD0), a = 0;
                for (int i = b >= 0 ? g_vb[b].first : 0; b >= 0 && i < g_vb[b].first + g_vb[b].count; i++) if (g_vv[i].c[3] > a) a = g_vv[i].c[3];
                if (b >= 0) seen++;
                if (!run && r <= 1 && b >= 0 && a != 255) whole = 0;
                if ((!run && r >= 2.0f && b >= 0) || (f == 100 && b >= 0)) gone = 0;
                if (!run && r > 1.38f && r < 1.45f && b >= 0 && a > 110 && a < 145) half = 1;
                if (!run && r > 1 && r < 2 && b >= 0) {                     /* (on the zoom's log, eased: smoothstep) */
                    float t = logf(r) / logf(2), want = 255 * (1 - t * t * (3 - 2 * t));
                    if (fabsf((float)a - want) > 6) whole = 0;
                }
                if (b >= 0 && a > 0 && a < 255) fades[run]++;
            }
        }
        ok = whole && gone && half && seen > 20 && fades[0] > 2 && fades[1] > 2;
    }
    Check(ok, "the continent's name fades out once it is wider than the window (his answer: \"yes the continent name should fade out\"): whole up to as wide as the window, as the opening shows it, half at 1.41 times, gone at twice (eased on the zoom's log) and by the landing at the latest, never always drawn");
    /* the work's steps as each painting is made: the continent's lines kept, its own name painted out, it cut to its land
     * (the world's painting under it); the zone cut to its own area */
    TestPaintNew(1);                                                  /* the world's lines: Azeroth's land a block of it */
    TestPaintFill(0, 0, 1002, 668, 0x707060);
    TestPaintFill(380, 50, 900, 600, 0x302010); TestPaintFill(385, 55, 895, 595, 0xC8A060);
    ParchLinesKeep(&g_linesWorld, ParchWorldRect(), -2, 1);
    TestPaintNew(1);
    TestPaintFill(0, 0, 1002, 668, 0xB09050);
    for (int i = 0; i < PARCH_WORDS; i++) {
        int c0, d0, c1, d1;
        if (lstrcmpiA(kWords[i].folder, "Azeroth") || !WordBox(i, 1, 1002, 668, &c0, &d0, &c1, &d1)) continue;
        TestWordLetters(c0, d0, c1, d1, 1);
    }
    const char *why = "";
    g_worldUp = 1;
    int words = ParchContMade(0, &why);
    ok = words && g_contWords == 0 && g_linesCont.row == 0 && g_contCut && g_contShare > 0.05f;
    TestPaintNew(1);
    TestPaintFill(0, 0, 1002, 668, 0x40A040);
    float fx, fy;
    ParchSpot(0, -9464, 62, &fx, &fy);
    ParchZoneMade(1, 0, fx, fy, &why);
    ok &= g_zoneCut;
    g_linesCont.row = 5;                                               /* (another continent's lines: not under it) */
    ParchZoneMade(1, 0, fx, fy, &why);
    ok &= !g_zoneCut && strstr(why, "not under it");
    g_worldUp = 0;
    words = ParchContMade(0, &why);
    ok &= !words && g_contWords == -1 && !g_contCut;
    Check(ok, "as each painting is made: the continent's own lines kept, its own name painted out and it cut to its land as the world's painting draws it (none of it without the world's); the zone cut to its own area (not without its continent's lines)");
    for (int c = 0; c < 2; c++) { ParchPicFree(&g_wordPic[c]); ParchPicFree(&g_wordUnder[c]); g_wordUp[c] = 0; }
    if (g_linesCont.m) HeapFree(GetProcessHeap(), 0, g_linesCont.m);
    if (g_linesWorld.m) HeapFree(GetProcessHeap(), 0, g_linesWorld.m);
    g_linesCont.m = g_linesWorld.m = NULL;
    ParchFree();
    TestGlideState();
}
static void TestGlide(void)
{
    /* the path: one zoom about one fixed point; every point on a straight line on the screen, the zoom's log even in the
     * eased progress, landing exactly on the end */
    ParchView a = { 31.25, 21.3, 27.2 }, b = { 45.73, 27.18, 682.0 };
    ParchView p0 = ParchPath(a, b, 0), p1 = ParchPath(a, b, 1);
    int ok = p0.x == a.x && p0.y == a.y && p0.s == a.s && p1.x == b.x && p1.y == b.y && p1.s == b.s;
    double fx = (b.s * b.x - a.s * a.x) / (b.s - a.s), fy = (b.s * b.y - a.s * a.y) / (b.s - a.s);
    double sx0 = (fx - a.x) * a.s, sy0 = (fy - a.y) * a.s, worst = 0, worstLine = 0, lnPrev = log(a.s), stepPrev = -1;
    static const double pts[3][2] = { { 45.73, 27.18 }, { 10, 30 }, { 50, 5 } };
    for (int k = 0; k <= 1000; k++) {
        float e = k / 1000.0f;
        ParchView v = ParchPath(a, b, e);
        worst = fmax(worst, fabs((fx - v.x) * v.s - sx0) + fabs((fy - v.y) * v.s - sy0));   /* the fixed point stays put */
        for (int q = 0; q < 3; q++) {                                    /* any point: on the line from its start through its end */
            double X = (pts[q][0] - a.x) * a.s, Y = (pts[q][1] - a.y) * a.s, X1 = (pts[q][0] - b.x) * b.s, Y1 = (pts[q][1] - b.y) * b.s;
            double x = (pts[q][0] - v.x) * v.s, y = (pts[q][1] - v.y) * v.s, L = hypot(X1 - X, Y1 - Y);
            worstLine = fmax(worstLine, fabs((x - X) * (Y1 - Y) - (y - Y) * (X1 - X)) / L);
        }
        double ln = log(v.s), step = ln - lnPrev;
        if (k > 0 && k < 1000 && stepPrev >= 0 && fabs(step - stepPrev) > 1e-5) ok = 0;   /* (the log of the zoom even in e) */
        if (k > 0) stepPrev = step;
        lnPrev = ln;
    }
    ok &= worst < 1e-6 && worstLine < 1e-6;
    ParchView c = { 10, 10, 50 }, d2 = { 20, 15, 50 }, m = ParchPath(c, d2, 0.25f);  /* (the same zoom at both ends: a pan, straight) */
    ok &= Near(m.x, 12.5, 1e-9) && Near(m.y, 11.25, 1e-9) && m.s == 50;
    ParchView out = ParchPath(b, a, 0.5f);                               /* (out instead of in: still one zoom about one point) */
    double ofx = (a.s * a.x - b.s * b.x) / (a.s - b.s);
    ok &= Near((ofx - out.x) * out.s, (ofx - b.x) * b.s, 1e-6);
    Check(ok, "the glide's path: one zoom about a single fixed point (it stays put on the screen), every point of the paintings on a straight line on the screen, the zoom's log even in the eased progress, starting and landing exactly on its two views; equal zooms a straight pan");
    ok = ParchEase(0) == 0 && ParchEase(1) == 1 && ParchEase(-1) == 0 && ParchEase(2) == 1 && Near(ParchEase(0.2f), 0.16032, 1e-5) &&
         Near(ParchEase(0.5f), 0.65625, 1e-5);
    double sp[100], prevE = 0;                                           /* (its speed, a hundredth at a time) */
    int calm = 1, never = 1;
    for (int k = 0; k < 100; k++) { double ek = ParchEase((k + 1) / 100.0f); sp[k] = (ek - prevE) * 100; prevE = ek; never &= sp[k] >= 0; }
    for (int k = 70; k < 99; k++) calm &= sp[k + 1] <= sp[k] + 1e-3;     /* (slowing all the way in, to nothing) */
    ok &= sp[0] < 0.06 && sp[99] < 0.01 && sp[98] < 0.02 && calm && never;
    ok &= ParchEase(0.1f) > 3 * (0.1 * 0.1 * 0.1 * (0.1 * (0.1 * 6 - 15) + 10));   /* (moving at once: three times smootherstep's at a tenth) */
    Check(ok, "the glide's easing: from rest but moving at once (0.16 of the way at a fifth of its time, smootherstep 0.058), never back, landing with no speed and no change of speed");

    /* the world map's frame, as WoW.exe places a continent on it (0x4A5F42, its numbers at 0x806524) */
    TestGlideState();
    float x, y;
    ParchSpot(0, 7466.6f, 16000, &x, &y);                              /* Azeroth's box's top left */
    ok = Near(x, 23.3125, 1e-3) && Near(y, 3.375, 2e-3);
    ParchSpot(0, -16000, -19200, &x, &y);
    ok &= Near(x, 72.8125, 1e-3) && Near(y, 36.375, 1e-3);
    ParchSpot(1, -1280, 130, &x, &y);                                  /* (Thunder Bluff, on the west of the world) */
    ok &= Near(x, (-130.0 / 533.333333) * 0.75 - 19 + 31.3125, 1e-4) && Near(y, (1280.0 / 533.333333) * 0.75 - 0.322 + 20.875, 1e-4);
    g_wmc[1].ok = 0;
    ParchSpot(1, -1280, 130, &x, &y);
    ok &= Near(x, -130.0 / 533.333333 + 31.3125, 1e-4);
    ok &= Same(kParchFrame, (const unsigned char *)"\x30\x37\xC4\x3C\x75\xCF\x82\x3C\x00\x00\xA7\x41\x00\x80\xFA\x41\x00\x00\x00\x42", 20);
    float k4[5];
    memcpy(k4, kParchFrame, 20);
    ok &= Near(1 / k4[0], PARCH_WORLD_FH, 1e-3) && Near(1 / k4[1], PARCH_WORLD_FW, 1e-3) && k4[2] == PARCH_WORLD_FH / 2 && k4[3] == PARCH_WORLD_FW / 2 &&
          k4[4] == PARCH_GRID_MID;
    Check(ok, "the glide's frame: a world place on the client's world map as WoW.exe places it (each continent's WorldMapContinent offsets and scale, 62.625 x 41.75 map tiles), its five numbers as in the client; a map with no row on its own tiles");
    unsigned wc[3 * 13] = { 1, 0, 23, 47, 15, 61, TestF(14.5f), TestF(-7), TestF(0.75f), 0, 0, 0, 0,
                            2, 1, 23, 48, 9, 52, TestF(-19), TestF(-0.322f), TestF(0.75f), 0, 0, 0, 0,
                            3, 42, 23, 47, 15, 61, TestF(14.5f), 0xC1F00000u, TestF(0.75f), 0, 0, 0, 0 };
    static unsigned char wcd[256];
    unsigned wn = TestWdbc(wcd, 3, 13, wc, "\0", 1);
    ParchConts(wcd, wn);
    ok = g_wmc[0].ok && g_wmc[1].ok && g_wmc[0].ox == 14.5f && g_wmc[0].oy == -7 && g_wmc[0].s == 0.75f && g_wmc[1].ox == -19 && g_wmc[1].s == 0.75f;
    wc[8] = TestF(0);                                                  /* (a scale of nothing: left out) */
    wn = TestWdbc(wcd, 3, 13, wc, "\0", 1);
    ParchConts(wcd, wn);
    ok &= !g_wmc[0].ok && g_wmc[1].ok;
    ParchConts(NULL, 0);
    ok &= !g_wmc[0].ok && !g_wmc[1].ok;
    Check(ok, "the glide's frame: WorldMapContinent.dbc's rows for the two continents read (offsets, scale), another map's and a bad scale left out, none from no table");

    /* the spans: never before the glide has begun, always whole by its end */
    TestGlideState();
    ParchView o = { 31.25, 21.3, 27.2 }, e = { 45.0, 27.0, 682 };
    float s0, s1;
    ParchSpan(&o, &e, 16, PARCH_MAG0, PARCH_MAG1, &s0, &s1);           /* (the coarser one soft before the glide begins) */
    ok = Near(s0, 0.02, 1e-6) && s1 > s0;
    ParchSpan(&o, &e, 600, PARCH_MAG0, PARCH_MAG1, &s0, &s1);          /* (so fine it would come after the end) */
    ok &= s1 == 1 && s0 <= 0.95f && s0 < s1;
    ParchSpan(&o, &e, 40, PARCH_MAG0, PARCH_MAG1, &s0, &s1);
    ok &= Near(s0, log(40 / 27.2) / log(682 / 27.2), 1e-5) && Near(s1 - s0, log(PARCH_MAG1 / PARCH_MAG0) / log(682 / 27.2), 1e-5);
    ParchSpan(&o, &o, 40, PARCH_MAG0, PARCH_MAG1, &s0, &s1);           /* (no zoom at all) */
    ok &= s0 < s1 && s1 <= 1;
    ok &= Near(ParchMix(0.5f, 0.4f, 0.6f), 0.5, 1e-5) && ParchMix(0.39f, 0.4f, 0.6f) == 0 && ParchMix(0.61f, 0.4f, 0.6f) == 1 && ParchMix(0.5f, 0.5f, 0.5f) == 1;
    Check(ok, "the glide's fades: each over the zoom where the coarser painting would go soft, never before the glide has begun, whole by its end");
    /* a painting's part of the window: where it, the window and its clip meet, its texels mapped straight, soft inside the
     * clip's edges */
    ViewListStart();
    ParchRect pr = { 0, 0, 10, 10 }, pc = { 2, 2, 8, 8 };
    ParchView pv = { 5, 5, 100 };
    ParchLayer(VT_PARCH2, 1024, 1002, 668, pr, pv, 1000, 1000, 1, 0, &pc, NULL);
    const ViewVert *q0 = g_vbN ? g_vv + g_vb[0].first : NULL, *qm = g_vbN ? TestVertNear(0, 500, 500) : NULL;
    float ymin = 1e9f, ymax = -1e9f;
    for (int i = 0; g_vbN && i < g_vb[0].count; i++) { if (g_vv[g_vb[0].first + i].y < ymin) ymin = g_vv[g_vb[0].first + i].y; if (g_vv[g_vb[0].first + i].y > ymax) ymax = g_vv[g_vb[0].first + i].y; }
    ok = g_vbN == 1 && g_vb[0].tex == VT_PARCH2 && q0 && Near(q0->x, 200, 1e-3) && Near(q0->y, 200, 1e-3) && q0->c[3] == 0 &&
         Near(q0->u, 0.2 * 1002 / 1024, 1e-6) && Near(q0->v, 0.2 * 668 / 1024, 1e-6) && qm && Near(qm->x, 500, 1e-3) && qm->c[3] == 255 &&
         Near(ymin, 200, 1e-3) && Near(ymax, 800, 1e-3);
    int soft = TestVertNear(0, 500.0f - 300 + 600.0f * 2 / PARCH_GX, 500)->c[3];   /* (a quarter of a tile inside its edge) */
    ok &= soft > 30 && soft < 50;
    for (int i = 0; g_vbN && i < g_vb[0].count; i++) ok &= g_vv[g_vb[0].first + i].x >= 200 - 1e-3f && g_vv[g_vb[0].first + i].x <= 800 + 1e-3f;
    ViewListStart();
    ParchLayer(VT_PARCH2, 1024, 1002, 668, pr, pv, 1000, 1000, 0, 0, NULL, NULL);
    ok &= g_vbN == 0;
    ViewListStart();                                                    /* the bloom: whole inside, nothing past its circle */
    ParchBloom pb = { 1, 500, 500, 200, 50 };
    ParchLayer(VT_PARCH2, 1024, 1002, 668, pr, pv, 1000, 1000, 1, 0, NULL, &pb);
    int inside = TestVertNear(0, 500, 500)->c[3], corner = TestVertNear(0, 300, 300)->c[3], edgeA = TestVertNear(0, 500 + 175, 500)->c[3];
    float far = 0;
    for (int i = 0; g_vbN && i < g_vb[0].count; i++) {
        const ViewVert *q = g_vv + g_vb[0].first + i;
        float d = sqrtf((q->x - 500) * (q->x - 500) + (q->y - 500) * (q->y - 500));
        if (q->c[3] && d > far) far = d;
    }
    ok &= inside == 255 && corner == 0 && edgeA > 64 && edgeA < 192 && far < 200 && g_vbN == 1;
    Check(ok, "a painting's part of the window: a mesh over where it, the window and its clip meet, each corner's texel straight from its place, soft inside the clip's edges; nothing at no alpha");

    /* the smaller copies: each texel the average of four, the picture's edges carried on to its square's */
    Fresh("glide-mips");
    g_parchK = 1; g_parchTW = 1024;
    g_parchPx = (unsigned char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 1024 * 1024 * 4);
    for (int yy = 0; yy < 668; yy++) for (int xx = 0; xx < 1002; xx++) { unsigned char *q = g_parchPx + 4 * (yy * 1024 + xx); q[0] = (unsigned char)(xx & 255); q[1] = (unsigned char)(yy & 255); q[2] = 40; q[3] = 255; }
    ok = ParchMips() && g_mipLevels == 10 && g_mipPx;
    const unsigned char *l1 = g_mipPx, *l2 = g_mipPx + 512 * 512 * 4;
    ok &= l1[4 * 3 + 0] == (6 + 7 + 6 + 7 + 2) / 4 && l1[4 * (512 * 5) + 1] == (10 + 10 + 11 + 11 + 2) / 4 && l1[2] == 40;
    ok &= g_parchPx[4 * (100 * 1024 + 1020)] == (1001 & 255) && g_parchPx[4 * (1000 * 1024 + 5) + 1] == (667 & 255);   /* (edges carried) */
    ok &= l2[0] == (l1[0] + l1[4] + l1[4 * 512] + l1[4 * 513] + 2) / 4;
    const unsigned char *last = g_mipPx;
    for (int w = 512; w > 1; w /= 2) last += (size_t)w * w * 4;
    ok &= last[3] == 255;
    ParchFree();
    ok &= !g_parchPx && !g_mipPx && !g_mipLevels;
    Check(ok, "the paintings' smaller copies: each level a box of four of the one above, down to one texel, the picture's last column and row carried on to its square's edges first (so they do not darken), all of it freed after");

    /* C's colour match: a finer painting's own detail kept, its colours brought to the coarser one's under it */
    g_parchK = 1; g_parchTW = 1024;
    g_parchPx = (unsigned char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 1024 * 1024 * 4);
    for (int yy = 0; yy < 668; yy++) for (int xx = 0; xx < 1002; xx++) {
        unsigned char *q = g_parchPx + 4 * (yy * 1024 + xx);
        int det = ((xx / 2 + yy / 2) & 1) ? 20 : -20;                    /* (its detail: a fine checker) */
        q[0] = (unsigned char)(60 + det); q[1] = (unsigned char)(160 + det); q[2] = (unsigned char)(60 + det); q[3] = 255;   /* (green) */
    }
    ParchLow L;
    L.ok = 1; L.w = 16; L.h = 10; L.z = 0;
    ParchRect lr = { 0, 0, 16, 10 }, fr = { 4, 4, 8, 6 };
    L.r = lr;
    for (int i = 0; i < 16 * 10; i++) { L.c[3 * i] = 200; L.c[3 * i + 1] = 150; L.c[3 * i + 2] = 80; }   /* (the coarser one: tan) */
    ok = ParchMatch(&L, fr);
    double sum[3] = { 0, 0, 0 }, con = 0;
    for (int yy = 100; yy < 568; yy++) for (int xx = 100; xx < 902; xx++) {
        const unsigned char *q = g_parchPx + 4 * (yy * 1024 + xx);
        for (int ch = 0; ch < 3; ch++) sum[ch] += q[ch];
        con += abs((int)q[0] - (int)g_parchPx[4 * (yy * 1024 + xx + 2)]);
    }
    double n = 468.0 * 802.0;
    ok &= Near(sum[0] / n, 200, 2) && Near(sum[1] / n, 150, 2) && Near(sum[2] / n, 80, 2) && Near(con / n, 40, 2);
    for (int yy = 0; yy < 668; yy++) for (int xx = 0; xx < 1002; xx++) {   /* a coarser one's colours kept, softened: tan, then blue */
        unsigned char *q = g_parchPx + 4 * (yy * 1024 + xx);
        q[0] = xx < 501 ? 200 : 40; q[1] = xx < 501 ? 150 : 60; q[2] = xx < 501 ? 80 : 200;
    }
    static ParchLow L2;
    ParchRect l2r = { 0, 0, 62.625f, 41.75f };
    ParchLowKeep(&L2, l2r, -2);
    float b62 = L2.c[3 * (40 * L2.w + 62) + 2], b58 = L2.c[3 * (40 * L2.w + 58) + 2], b66 = L2.c[3 * (40 * L2.w + 66) + 2];
    ok &= L2.ok && L2.w == PARCH_LP && L2.h == PARCH_LP * 668 / 1002 && L2.z == -2 && b62 > 95 && b62 < 185 && b58 < 95 && b66 > 185;
    ParchLow none = L;
    none.ok = 0;
    ok &= !ParchMatch(&none, fr);
    ParchFree();
    ok &= !ParchMatch(&L, fr);
    Check(ok, "the fade C's copy: a finer painting's colours brought to the coarser one's under it (green to tan here), its own detail kept whole (the fine checker as strong); the coarser one's colours kept softened over a few cells; nothing without the coarser one's colours or a painting");

    /* the work, a painting a frame, the first needed first: the world's (once a session), this screen's arrival's
     * continent's, its zone's; the arrival taken at once */
    Fresh("glide-work");
    GameDir();
    TestGlideState();
    {
        static const char wstr[] = "\0Azeroth\0Elwynn\0";
        unsigned wv[2 * 8] = { 14, 0, 0, 1, TestF(16000), TestF(-19200), TestF(7466), TestF(-16000),
                               30, 0, 12, 9, TestF(1535), TestF(-1935), TestF(-7939), TestF(-10254) };
        static unsigned char wma[256];
        unsigned wmn = TestWdbc(wma, 2, 8, wv, wstr, sizeof wstr);
        ParchTables(wma, wmn, NULL, 0);
    }
    static char tn[36][64];
    const char *tnames[36], *tdata[36];
    unsigned tlen[36];
    static const char *const folder[3] = { "World", "Azeroth", "Elwynn" };
    for (int i = 0; i < 36; i++) {
        snprintf(tn[i], sizeof tn[i], "Interface\\WorldMap\\%s\\%s%d.blp", folder[i / 12], folder[i / 12], i % 12 + 1);
        tnames[i] = tn[i];
        tdata[i] = (const char *)TestPalBlp(256, 256, (unsigned)(30 + 60 * (i / 12)) << 8 | 0x402010, 0, &tlen[i]);
    }
    MakeMpq("Data/patch-2.MPQ", 12, tnames, tdata, tlen);             /* (the world's, the continent's and the zone's tiles) */
    MakeMpq("Data/patch-3.MPQ", 12, tnames + 12, tdata + 12, tlen + 12);
    MakeMpq("Data/interface.MPQ", 12, tnames + 24, tdata + 24, tlen + 24);
    g_upN = 0; g_upFail = 0;
    g_destMap = 0; g_destWX = -9464; g_destWY = 62; g_viewScreen = 2;
    ParchWork(90);                                                      /* (the arrival known already: the world's alone, its frame) */
    int alone = g_upN == 1 && g_upOrder[0] == VT_PWORLD && g_parchForScreen == -1;
    g_ptry.world = 0; g_worldUp = 0; g_upN = 0; g_destMap = -1;
    ParchWork(100);                                                     /* (the world's, before any arrival) */
    ok = alone && g_upN == 1 && g_upOrder[0] == VT_PWORLD && g_worldUp && g_worldAt == 100 && g_worldTW == 1024 && !g_parchPx && LogSays("the world painted");
    ParchWork(116);                                                     /* (no arrival yet: nothing) */
    ok &= g_upN == 1 && g_parchForScreen == -1;
    g_destMap = 0; g_destWX = -9464; g_destWY = 62; g_viewScreen = 3;
    ParchWork(132);
    ok &= g_upN == 2 && g_upOrder[1] == VT_PARCH2 && g_parchForScreen == 3 && g_parchAt == 132 && g_parchWant == 1 && g_parchWantCont == 0 &&
          g_parchCont == 0 && g_contAt == 132;
    ok &= g_linesCont.row == 0 && g_linesCont.m;                       /* (its lines kept, for its zone's outline) */
    ParchWork(148);
    ok &= g_upN == 3 && g_upOrder[2] == VT_PARCH && g_parchZone == 1 && g_zoneAt == 148 && !g_parchPx && g_zoneCut && g_cutBoxed &&
          LogSays("Elwynn cut to its own area as Azeroth's painting draws it");   /* (flat paintings: its outline leaks, kept to its box) */
    ParchWork(164);                                                     /* (all up: nothing more) */
    ok &= g_upN == 3;
    g_viewScreen = 4;                                                   /* (the next screen, the same place: taken anew, nothing made) */
    ParchWork(500);
    ok &= g_upN == 3 && g_parchForScreen == 4 && g_parchAt == 500;
    Check(ok, "the parchment's work: a painting a frame, the first needed first: the world's once a session (before any arrival), then the arrival's continent's, then its zone's; the arrival taken for the glide as soon as it is known, anew each screen; nothing made again that is up");
    g_destWX = -500; g_destWY = 500; g_viewScreen = 5; g_parchCont = -1;   /* (at sea, its continent's not up) */
    g_upFail = 1;
    ParchWork(600);
    ok = g_upN == 4 && g_parchWant == 0 && g_parchCont == -1 && LogSays("OpenGL would not take the continent's painting");
    ParchWork(616);                                                     /* (tried: not again for this arrival) */
    ok &= g_upN == 4;
    g_upFail = 0;
    Check(ok, "the parchment's work: an arrival at sea wants only its continent's painting; one OpenGL will not take is not tried again for that arrival");
    TestGlideState();
    g_parchFade = PARCH_FADE_C; g_upN = 0;
    g_destMap = 0; g_destWX = -9464; g_destWY = 62; g_viewScreen = 6;
    ParchWork(700); ParchWork(716); ParchWork(732);
    ok = g_upN == 5 && g_upOrder[0] == VT_PWORLD && g_upOrder[1] == VT_PARCH2 && g_upOrder[2] == VT_PCONTM && g_upOrder[3] == VT_PARCH &&
         g_upOrder[4] == VT_PZONEM && g_contMatch == 0 && g_zoneMatch == 1 && g_lpWorld.ok && LogSays("its copy in the world's colours");
    g_worldFrameOk = 0; g_ptry.world = 0; g_worldUp = 0; g_upN = 0;      /* (no world frame: no world's painting, the continent's the opening) */
    ParchWork(800);
    ok &= g_upN == 0 && !g_worldUp && LogSays("is not as read");
    Check(ok, "the parchment's work with the fade C: each finer painting and then its copy in the colours under it (the world's); without the client's world frame, no world's painting, and the log says so");
    for (int i = 0; i < 36; i++) free((void *)tdata[i]);
    TestGlideState();

    /* asked for: the continents' places, the client's frame (as read or not) and the fade's switch */
    Fresh("glide-read");
    GameDir();
    Put("Data/CinderLoad/parchment.txt", "");
    memcpy(shim_module + (PARCH_FRAME_VA - 0x400000u), kParchFrame, sizeof kParchFrame);
    unsigned wc2[2 * 13] = { 1, 0, 23, 47, 15, 61, TestF(14.5f), TestF(-7), TestF(0.75f), 0, 0, 0, 0,
                             2, 1, 23, 48, 9, 52, TestF(-19), TestF(-0.322f), TestF(0.75f), 0, 0, 0, 0 };
    wn = TestWdbc(wcd, 2, 13, wc2, "\0", 1);
    static const char *dn[1] = { "DBFilesClient\\WorldMapContinent.dbc" };
    const char *dd[1] = { (const char *)wcd };
    unsigned dl[1] = { wn };
    MakeMpq("Data/dbc.MPQ", 1, dn, dd, dl);
    ParchRead();
    ok = g_wmc[0].ok && g_wmc[1].ok && g_worldFrameOk && g_parchFade == PARCH_FADE_A && LogSays("2 of the 2 continents placed") && LogSays("its fade A");
    Put("Data/CinderLoad/parchment-fade.txt", "  b\r\n");
    ParchRead();
    ok &= g_parchFade == PARCH_FADE_B;
    Put("Data/CinderLoad/parchment-fade.txt", "C");
    ParchRead();
    ok &= g_parchFade == PARCH_FADE_C;
    Put("Data/CinderLoad/parchment-fade.txt", "d");
    ParchRead();
    ok &= g_parchFade == PARCH_FADE_D && LogSays("its fade D");
    Put("Data/CinderLoad/parchment-fade.txt", "x");
    ParchRead();
    ok &= g_parchFade == PARCH_FADE_A;
    shim_module[PARCH_FRAME_VA - 0x400000u + 19] = 0x43;                /* (another client's number) */
    ParchRead();
    ok &= !g_worldFrameOk && LogSays("is NOT as read");
    g_parchAsked = 0; g_vbarAsked = 0;
    Check(ok, "the glide asked for: WorldMapContinent.dbc read, the client's world map frame checked byte for byte (another client's: no world opening), the fade from parchment-fade.txt's first letter (A, B, C or D; anything else or none A), all logged");

    /* whole loads through the DLL's own steps: one straight glide landing as the bar is full, never back, no jump; a
     * first login (his machine's), a very short load (no floor, a quick later screen) and a very long one (the game far
     * behind its plan) */
    float dev, jump;
    TestGlideLoad(5524, 5524, 2619, HOLD_MS, 1280, 540);
    ok = TestGlideGood(1280, 540, &dev, &jump) && dev < 0.01f && jump < 0.1f;   /* (the hold's exact end a little sooner than
                                                                                 * the plan's guess: the bar's pace a little quicker) */
    float dev1 = dev, jump1 = jump;
    TestGlideLoad(300, 609, 1025, HOLD_MS, 1280, 540);                /* (a quick later screen, the floor holding it) */
    int shortOk = TestGlideGood(1280, 540, &dev, &jump) && dev < 0.01f && jump < 0.15f;
    float jumpS = jump;
    TestGlideLoad(20000, 5524, 2619, HOLD_MS, 1280, 540);
    int longOk = TestGlideGood(1280, 540, &dev, &jump) && dev < 0.01f && jump < 0.05f && g_glN > 1200;
    printf("  the glide: off its line by at most %.5f px; its largest change of speed a frame, of its fastest: %.3f (a first login), %.3f (short), %.3f (long)\n",
           dev1, jump1, jumpS, jump);
    Check(ok && shortOk && longOk, "whole loads through the DLL's own steps (a first login, a very short load, a very long one): the character's spot on one straight line to the middle of the window, never back, the zoom never back out, no sudden change of speed from one frame to the next, landed on the frame the bar is full");
    /* the even bar's pace (the skeptic's find, 11 Oct: the hold moving the end out stepped it 18% in a frame): at most
     * EVEN_PACE_K of itself a ms, on his first login and on a long load waiting near the end (never stopping dead) */
    float worstPace[2] = { 0, 0 };
    for (int run = 0; run < 2; run++) {
        TestGlideLoad(run ? 20000 : 5524, 5524, 2619, HOLD_MS, 1280, 540);
        for (int i = 2; i < g_glN; i++) {
            if (!(g_glPace[i - 1] > 0) || g_glB[i] >= 1) continue;
            float d = (g_glPace[i - 1] - g_glPace[i]) / g_glPace[i - 1];
            if (d > worstPace[run]) worstPace[run] = d;
        }
    }
    printf("  the even bar: its largest slowing a frame %.4f of its pace (a first login), %.4f (a long load, waiting near the end)\n", worstPace[0], worstPace[1]);
    Check(worstPace[0] <= EVEN_PACE_K * 16 * 1.001f && worstPace[1] <= EVEN_PACE_K * 16 * 1.001f,
          "the even bar's pace slows at most EVEN_PACE_K of itself a ms (a moved end eases it, never a step); waiting near the end it slows that way too, never stopping dead");
    /* the arrival written anew mid-screen (the skeptic's find, 11 Oct: the glide went back to the opening, 5 times back
     * out, with a frame of it): on from the view as it was, the zoom never back out, no jump, landed on the new spot as
     * the bar is full */
    g_glMoveAt = 0.5f; g_glMoveDx = 300;
    TestGlideLoad(5524, 5524, 2619, HOLD_MS, 1280, 540);
    g_glMoveAt = 2;
    int reOk = g_glN > 20, landAt = -1, fullAt = -1, moveAt = -1;
    float lnMost = 0, lnJump = 0, cMost = 0, cJump = 0;
    for (int i = 1; i < g_glN; i++) {
        float st = logf(g_glS[i] / g_glS[i - 1]), cs = sqrtf((g_glVX[i] - g_glVX[i - 1]) * (g_glVX[i] - g_glVX[i - 1]) + (g_glVY[i] - g_glVY[i - 1]) * (g_glVY[i] - g_glVY[i - 1])) * g_glS[i];
        if (st < -1e-6f) reOk = 0;                                       /* (never back out) */
        if (st > lnMost) lnMost = st;
        if (cs > cMost) cMost = cs;
        if (i > 1) { float st0 = logf(g_glS[i - 1] / g_glS[i - 2]); if (fabsf(st - st0) > lnJump) lnJump = fabsf(st - st0); }
        if (moveAt < 0 && g_glB[i] >= 0.5f) moveAt = i;
        if (landAt < 0 && g_glT[i] >= 1) landAt = i;
        if (fullAt < 0 && g_glB[i] >= 1) fullAt = i;
    }
    for (int i = moveAt > 2 ? moveAt - 1 : 2; i < g_glN && i < moveAt + 4; i++) {
        float c1 = sqrtf((g_glVX[i] - g_glVX[i - 1]) * (g_glVX[i] - g_glVX[i - 1]) + (g_glVY[i] - g_glVY[i - 1]) * (g_glVY[i] - g_glVY[i - 1])) * g_glS[i];
        float c0 = sqrtf((g_glVX[i - 1] - g_glVX[i - 2]) * (g_glVX[i - 1] - g_glVX[i - 2]) + (g_glVY[i - 1] - g_glVY[i - 2]) * (g_glVY[i - 1] - g_glVY[i - 2])) * g_glS[i - 1];
        if (fabsf(c1 - c0) > cJump) cJump = fabsf(c1 - c0);
    }
    reOk &= landAt >= 0 && landAt == fullAt && Near(g_glX[g_glN - 1], 640, 1e-3) && Near(g_glY[g_glN - 1], 270, 1e-3) && lnJump < 0.1f * lnMost &&
            cJump < 0.5f * cMost && moveAt > 0;
    int zUp = -1, cUp = -1;                                             /* (its paintings stay up through it: never a frame of them gone) */
    for (int i = 0; i < g_glN; i++) {
        if (zUp < 0 && g_glZA[i] == 255) zUp = i;
        if (cUp < 0 && g_glCA[i] == 255) cUp = i;
        if ((zUp >= 0 && g_glZA[i] < 250) || (cUp >= 0 && g_glCA[i] < 250)) reOk = 0;
    }
    reOk &= zUp >= 0 && zUp < moveAt && cUp >= 0 && cUp < moveAt;
    printf("  an arrival changed at half the bar: its zoom's largest change of speed a frame %.3f of its fastest, its middle's %.3f, never back out\n",
           lnMost > 0 ? lnJump / lnMost : 0, cMost > 0 ? cJump / cMost : 0);
    Check(reOk, "the arrival written anew mid-screen: the glide goes on from the view as it was (never back to the opening, the zoom never back out, no jump in its speed), landing on the new spot as the bar is full");
    g_glMoveAt = 1.0f; g_glMoveDx = 300;                               /* (written anew after it landed: afresh from there, on the clock) */
    TestGlideLoad(5524, 5524, 2619, HOLD_MS, 1280, 540);
    g_glMoveAt = 2;
    int after = g_glN > 20 && Near(g_glX[g_glN - 1], 640, 1e-3) && Near(g_glY[g_glN - 1], 270, 1e-3) && g_glide.clock;
    for (int i = 1; i < g_glN; i++) after &= g_glS[i] == g_glS[i] && g_glVX[i] == g_glVX[i] && g_glS[i] >= g_glS[i - 1] * (1 - 1e-6f);
    Check(after, "the arrival written anew after the glide landed: a fresh glide from there on the clock, landed on the new spot before the screen goes");
    g_glTakeAt = 0.7f;                                                  /* (taken at 70% of the bar, its time left still long: on the bar from there) */
    TestGlideLoad(5524, 5524, 2619, HOLD_MS, 1280, 540);
    g_glTakeAt = 0;
    int fb = -1, fbLand = -1, fbFull = -1;
    float fbMost = 0;
    for (int i = 1; i < g_glN; i++) {
        float st = logf(g_glS[i] / g_glS[i - 1]);
        if (st > fbMost) fbMost = st;
        if (fb < 0 && g_glT[i] > 0) fb = i;
        if (fbLand < 0 && g_glT[i] >= 1) fbLand = i;
        if (fbFull < 0 && g_glB[i] >= 1) fbFull = i;
    }
    Check(fb > 0 && !g_glide.clock && Near(g_glS[fb - 1], 540.0 / (PARCH_WORLD_FH * PARCH_OPEN_TALL), 1e-3) && fbLand == fbFull && fbMost < logf(1.1f),
          "an arrival taken at 70% of the bar: from the opening there, on the rest of the bar, no jump, landed as the bar is full");
    /* an arrival taken late (the skeptic's find: its start capped at 95% of the bar jumped it 2.8 times in a frame): from
     * the opening, on the clock over PARCH_LATE_MS, no jump, landed before the screen goes */
    g_glTakeAt = 0.96f;
    TestGlideLoad(5524, 5524, 2619, HOLD_MS, 1280, 540);
    g_glTakeAt = 0;
    int first = -1, lateLand = -1;
    float lateMost = 0;
    for (int i = 1; i < g_glN; i++) {
        float st = logf(g_glS[i] / g_glS[i - 1]);
        if (st > lateMost) lateMost = st;
        if (first < 0 && g_glT[i] > 0) first = i;
        if (lateLand < 0 && g_glT[i] >= 1) lateLand = i;
    }
    double sOpen = 540.0 / (PARCH_WORLD_FH * PARCH_OPEN_TALL);
    int lateOk = first > 0 && Near(g_glS[first - 1], sOpen, 1e-3) && lateLand > first && (lateLand - first) * 16 <= g_glide.dur + 32 &&
                 (lateLand - first) * 16 >= g_glide.dur - 32 && g_glide.clock && g_glide.dur <= PARCH_LATE_MS && g_glide.dur > 300 &&
                 lateLand < g_glN - PARCH_LATE_SPARE / 16 + 2 && lateMost < logf(1.25f);
    printf("  an arrival taken at 96%% of the bar: glided in %d ms, its largest zoom a frame x%.3f\n", (lateLand - first + 1) * 16, expf(lateMost));
    Check(lateOk, "an arrival taken late (the bar nearly full): from the opening, on the clock (PARCH_LATE_MS, or less to land PARCH_LATE_SPARE before the screen goes), no jump (never a quarter more zoom in one frame), landed before the screen goes");
    TestGlideState();
    /* C's own colours: once the finest is up, over PARCH_COLOUR more zoom, never starting past 0.95 (so they are whole,
     * evenly, by the end, never in one frame); the end at the zone's zoom whether the window is wide or narrow */
    float k0, k1;
    ParchColourSpan(0.4f, 1 / 3.0, &k0, &k1);
    ok = Near(k0, 0.4, 1e-6) && Near(k1, 0.4 + log(PARCH_COLOUR) / 3, 1e-5);
    ParchColourSpan(1.0f, 1 / 3.0, &k0, &k1);
    ok &= Near(k0, 0.95, 1e-6) && k1 == 1 && ParchMix(0.975f, k0, k1) > 0.3f && ParchMix(0.975f, k0, k1) < 0.7f;
    ParchRect tall = { 10, 10, 13, 12 };                                /* (a zone 3 x 2 tiles, in a window narrower than it) */
    ParchView pe = ParchEnd(1000, 1400, tall, PARCH_ZOOM, 11, 11), pw = ParchEnd(3000, 1000, tall, PARCH_ZOOM, 11, 11);
    ok &= Near(pe.s, PARCH_ZOOM * 1400 / 2.0, 1e-3) && Near(pw.s, PARCH_ZOOM * 3000 / 3.0, 1e-3);
    Check(ok, "C's own colours come up once the finest painting is, never starting past 0.95 of the glide (whole, evenly, by its end); the glide's end has the zone the zoom times wider than the window, or taller when the window is narrower than it");
    TestGlideState();
}

/* no1600x1200's two display hooks made to take turns (his "fix the no1600x1200 crash", 10 Oct 2026): each hook's jump
 * pointed at CinderLoad's door, which lets one caller through at a time and goes on to the hook; only a jump out of the
 * function's own module (or MinHook's hot-patch form) is taken for a hook */
static unsigned char g_fakeFn[64], g_fakeHook[16];
static int g_hookCalls, g_hookHeld;
static BOOL WINAPI TestHookSettings(const char *dev, DWORD mode, void *dm)
{
    (void)dev; (void)dm;
    g_hookCalls++; g_hookHeld = g_modeLock.held;
    return mode == 7;
}
static void TestFakeJump(unsigned char *at, const unsigned char *to)
{
    at[0] = 0xE9;
    unsigned rel = (unsigned)(to - (at + 5));
    memcpy(at + 1, &rel, 4);
}
static void TestModeTurn(void)
{
    void *next = NULL;
    memset(g_fakeFn, 0xCC, sizeof g_fakeFn);
    TestFakeJump(g_fakeFn + 16, g_fakeHook);                          /* a hook: a jump at its first byte, out of its module */
    int r = ModeTurnPatch(g_fakeFn + 16, g_fakeFn, g_fakeFn + sizeof g_fakeFn, (void *)ModeSettingsTurn, &next);
    unsigned char *now = g_fakeFn + 16 + 5 + (int)Rd32(g_fakeFn + 17);
    int ok = r == 1 && next == (void *)g_fakeHook && now == (unsigned char *)(void *)ModeSettingsTurn && g_fakeFn[16] == 0xE9;
    memset(g_fakeFn, 0xCC, sizeof g_fakeFn);                          /* MinHook's hot-patch form: EB F9, the jump before it */
    TestFakeJump(g_fakeFn + 11, g_fakeHook);
    g_fakeFn[16] = 0xEB; g_fakeFn[17] = 0xF9;
    next = NULL;
    r = ModeTurnPatch(g_fakeFn + 16, g_fakeFn, g_fakeFn + sizeof g_fakeFn, (void *)ModeSettingsTurn, &next);
    now = g_fakeFn + 11 + 5 + (int)Rd32(g_fakeFn + 12);
    ok &= r == 2 && next == (void *)g_fakeHook && now == (unsigned char *)(void *)ModeSettingsTurn && g_fakeFn[16] == 0xEB;
    memset(g_fakeFn, 0xCC, sizeof g_fakeFn);                          /* not hooked: its own first instruction */
    g_fakeFn[16] = 0x8B; g_fakeFn[17] = 0xFF;
    next = NULL;
    ok &= ModeTurnPatch(g_fakeFn + 16, g_fakeFn, g_fakeFn + sizeof g_fakeFn, (void *)ModeSettingsTurn, &next) == 0 && !next && g_fakeFn[16] == 0x8B;
    TestFakeJump(g_fakeFn + 16, g_fakeFn + 40);                       /* a jump within its own module: not a hook */
    ok &= ModeTurnPatch(g_fakeFn + 16, g_fakeFn, g_fakeFn + sizeof g_fakeFn, (void *)ModeSettingsTurn, &next) == 0 && !next &&
          g_fakeFn + 16 + 5 + (int)Rd32(g_fakeFn + 17) == g_fakeFn + 40;
    ok &= ModeTurnPatch(NULL, g_fakeFn, g_fakeFn + sizeof g_fakeFn, (void *)ModeSettingsTurn, &next) == 0;
    Check(ok, "no1600x1200's hooks: a hook's jump (or MinHook's hot-patch form) sent to CinderLoad's door, the door going on to the hook; a function not hooked, or a jump within its own module, left alone");
    InitializeCriticalSection(&g_modeLock);
    g_edsNext = TestHookSettings; g_hookCalls = 0; g_hookHeld = 0;
    BOOL got = ModeSettingsTurn("\\\\.\\DISPLAY1", 7, NULL);
    Check(got && g_hookCalls == 1 && g_hookHeld && !g_modeLock.held && g_modeTurns >= 1,
          "no1600x1200's hooks: a call through the door reaches the hook with the lock held (one caller at a time), its answer back, the lock let go");
}

/* The explored areas (the parchment's discovery, its proof; his "word, start on the explored-areas read", 10 Oct 2026):
 * the client's own rule for a piece found, the active player found in the object manager's list as the client lays it
 * out (built here in low memory, where its 32-bit pointers fit), its bits read, kept per character with a zone-by-zone
 * account */
#include <sys/mman.h>
static unsigned TestLow(void *p) { return (unsigned)(ULONG_PTR)p; }
static void TestPut32(unsigned char *at, unsigned v) { memcpy(at, &v, 4); }
static void TestExplore(void)
{
    /* AreaTable: id, bit (field 3), field 10; three areas, one always shown, one past the 2048 */
    static unsigned av[4 * 25];
    memset(av, 0, sizeof av);
    unsigned ids[4] = { 9, 87, 120, 4000 }, bitv[4] = { 125, 548, 2050, 7 }, lvl[4] = { 1, 1, 1, 0xFFFFFFFFu };
    for (int i = 0; i < 4; i++) { av[i * 25] = ids[i]; av[i * 25 + 3] = bitv[i]; av[i * 25 + 10] = lvl[i]; }
    static unsigned char atb[1024];
    unsigned an = TestWdbc(atb, 4, 25, av, "\0", 1);
    ParchAreas(atb, an);
    int ok = g_atN == 4 && g_atSorted && AreaRow(87) == 1 && AreaRow(5) == -1 && g_atBit[0] == 125 && g_atLvl[3] < 0;
    unsigned char bits[EXPLORED_BYTES];
    memset(bits, 0, sizeof bits);
    bits[125 >> 3] |= 1 << (125 & 7);                                     /* Northshire found, Goldshire not */
    ParchPiece pn = { 30, 1, 1, 0, 0, { 9, 0, 0, 0 }, "NORTHSHIREVALLEY" }, pg = { 30, 1, 1, 0, 0, { 87, 0, 0, 0 }, "GOLDSHIRE" };
    ParchPiece pw = { 30, 1, 1, 0, 0, { 87, 120, 0, 0 }, "FORESTSEDGE" }, pa = { 30, 1, 1, 0, 0, { 4000, 0, 0, 0 }, "ALWAYS" };
    ParchPiece pz = { 30, 1, 1, 0, 0, { 0, 0, 0, 0 }, "NONE" }, pu = { 30, 1, 1, 0, 0, { 77, 0, 0, 0 }, "UNKNOWN" };
    ok &= PieceFound(&pn, bits) && !PieceFound(&pg, bits) && PieceFound(&pw, bits) && PieceFound(&pa, bits) && !PieceFound(&pz, bits) && !PieceFound(&pu, bits);
    bits[548 >> 3] |= 1 << (548 & 7);
    ok &= PieceFound(&pg, bits);
    av[25] = 3;                                                          /* (out of order: still found, by a search through all) */
    an = TestWdbc(atb, 4, 25, av, "\0", 1);
    ParchAreas(atb, an);
    ok &= !g_atSorted && AreaRow(3) == 1 && AreaRow(87) == -1;
    Check(ok, "explored: the client's own rule for a piece found: any of its areas' bits set, or an area past the 2048 bits, or one whose field 10 is below 0; an area with no row, none; AreaTable read sorted or not");
    av[25] = 87;                                                         /* (the table as it was, for what follows) */
    an = TestWdbc(atb, 4, 25, av, "\0", 1);
    ParchAreas(atb, an);

    /* the object manager as the client lays it out: the active player's GUID at +0xC0, the list's link offset at +0xA4 and
     * its first object at +0xAC; each object's next at link + 4 (odd at the end), its fields at +8 (its GUID first), a
     * player's pointer to its player fields at +0xE68, the explored bits at +0xE6C there */
    unsigned char *low = mmap(NULL, 1 << 16, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (low == MAP_FAILED) { Check(0, "explored: low memory for the test's object manager"); return; }
    memset(low, 0, 1 << 16);
    unsigned char *mgr = low, *o1 = low + 0x400, *o2 = low + 0x2000, *f1 = low + 0x8000, *f2 = low + 0x8100, *pf = low + 0x9000;
    TestPut32(mgr + OBJ_MGR_LINK, 0x38); TestPut32(mgr + OBJ_MGR_FIRST, TestLow(o1));
    TestPut32(mgr + OBJ_MGR_GUID, 0x1234); TestPut32(mgr + OBJ_MGR_GUID + 4, 0x50000);
    TestPut32(o1 + OBJ_FIELDS, TestLow(f1)); TestPut32(o1 + 0x38 + 4, TestLow(o2));
    TestPut32(f1, 0x1234); TestPut32(f1 + 4, 0x60000);                   /* (another object: its low word the player's) */
    TestPut32(o1 + PLAYER_FIELDS, TestLow(low + 0xC000));                /* (and other bits, should it be taken for it) */
    memset(low + 0xC000 + EXPLORED_OFF, 0xFF, EXPLORED_BYTES);
    TestPut32(o2 + OBJ_FIELDS, TestLow(f2)); TestPut32(o2 + 0x38 + 4, TestLow(mgr + OBJ_MGR_FIRST - 4) | 1);   /* (the last) */
    TestPut32(f2, 0x1234); TestPut32(f2 + 4, 0x50000);
    TestPut32(o2 + PLAYER_FIELDS, TestLow(pf));
    memcpy(pf + EXPLORED_OFF, bits, EXPLORED_BYTES);
    TestPut32(shim_module + (OBJ_MGR_VA - 0x400000), TestLow(mgr));
    unsigned char got[EXPLORED_BYTES];
    unsigned lo = 0, hi = 0;
    ok = ExploreRead(got, &lo, &hi) && lo == 0x1234 && hi == 0x50000 && !memcmp(got, bits, EXPLORED_BYTES);
    TestPut32(mgr + OBJ_MGR_GUID, 0x9999);                               /* no object is the player */
    ok &= !ExploreRead(got, &lo, &hi);
    TestPut32(mgr + OBJ_MGR_GUID, 0); TestPut32(mgr + OBJ_MGR_GUID + 4, 0);   /* no player yet */
    ok &= !ExploreRead(got, &lo, &hi);
    TestPut32(mgr + OBJ_MGR_GUID, 0x1234); TestPut32(mgr + OBJ_MGR_GUID + 4, 0x50000);
    TestPut32(o2 + PLAYER_FIELDS, 0);                                    /* its player fields not there */
    ok &= !ExploreRead(got, &lo, &hi);
    TestPut32(o2 + PLAYER_FIELDS, TestLow(pf));
    TestPut32(shim_module + (OBJ_MGR_VA - 0x400000), 0);                 /* no object manager */
    ok &= !ExploreRead(got, &lo, &hi);
    TestPut32(shim_module + (OBJ_MGR_VA - 0x400000), TestLow(mgr));
    Check(ok, "explored: the active player found in the object manager's list as the client lays it out, its explored bits read; none with no manager, no player yet, no object of its GUID or no player fields");

    /* kept per character, with its zone-by-zone account, when they change; only with the parchment asked for */
    Fresh("explored");
    GameDir();
    TestPut32(shim_module + (OBJ_MGR_VA - 0x400000), TestLow(mgr));     /* (Fresh clears the client's image) */
    static const char wstr[] = "\0Elwynn\0";
    unsigned wv[8] = { 30, 0, 12, 1, TestF(1535), TestF(-1935), TestF(-7939), TestF(-10254) };
    static unsigned char wma[256], wmo[512];
    unsigned wn = TestWdbc(wma, 1, 8, wv, wstr, sizeof wstr);
    static const char ostr[] = "\0NORTHSHIREVALLEY\0GOLDSHIRE\0FORESTSEDGE\0";
    unsigned ov[3 * 17] = { 0 };
    unsigned r0[13] = { 1, 30, 9, 0, 0, 0, 0, 0, 1, 256, 256, 0, 0 }, r1[13] = { 2, 30, 87, 0, 0, 0, 0, 0, 18, 256, 256, 0, 0 },
             r2[13] = { 3, 30, 120, 0, 0, 0, 0, 0, 28, 256, 256, 0, 0 };   /* (an area past the 2048: always found) */
    memcpy(ov, r0, sizeof r0); memcpy(ov + 17, r1, sizeof r1); memcpy(ov + 34, r2, sizeof r2);
    unsigned on = TestWdbc(wmo, 3, 17, ov, ostr, sizeof ostr);
    ParchTables(wma, wn, wmo, on);
    bits[548 >> 3] &= (unsigned char)~(1 << (548 & 7));                 /* (Goldshire not found again) */
    memcpy(pf + EXPLORED_OFF, bits, EXPLORED_BYTES);
    g_exGuid[0] = g_exGuid[1] = 0;
    /* once a screen, as it begins (his rule, 10 Oct: no rescanning): not asked for, nothing */
    g_parchAsked = 0; g_parchTablesOk = 1;
    LONG seq0 = g_scrSeq;
    ExploreAtLoad();
    ok = g_scrSeq == seq0 && !Get("Data/CinderLoad/explored-0005000000001234.bin");
    /* a teleport or a hearth: the player is there, read now and kept */
    g_parchAsked = 1;
    ExploreAtLoad();
    ok &= g_scrSeq == seq0 + 1 && g_scrKnown == 1 && !memcmp(g_scrBits, bits, EXPLORED_BYTES) && g_scrGuid[0] == 0x1234 && g_scrGuid[1] == 0x50000;
    ok &= Get("Data/CinderLoad/explored-0005000000001234.bin") != NULL;   /* (Get's one buffer: the .bin first) */
    const char *t = Get("Data/CinderLoad/explored-0005000000001234.txt");
    ok &= t && strstr(t, "Elwynn (Eastern Kingdoms): 2 of 3 found: NORTHSHIREVALLEY, FORESTSEDGE; not yet: GOLDSHIRE") &&
          LogSays("explored: character 0005000000001234 has found 1 of 2048 areas, 2 of the world map's 3 pieces") &&
          LogSays("read now (the player is here)");
    Put("Data/CinderLoad/explored-0005000000001234.txt", "y");
    ExploreAtLoad();                                                     /* (the next screen, nothing found since: not written again) */
    ok &= g_scrSeq == seq0 + 2 && !strcmp(Get("Data/CinderLoad/explored-0005000000001234.txt"), "y");
    pf[EXPLORED_OFF + (548 >> 3)] |= 1 << (548 & 7);                    /* Goldshire found, then a hearth */
    ExploreAtLoad();
    t = Get("Data/CinderLoad/explored-0005000000001234.txt");
    ok &= t && strstr(t, "Elwynn (Eastern Kingdoms): 3 of 3 found: NORTHSHIREVALLEY, GOLDSHIRE, FORESTSEDGE\r\n") && !strstr(t, "not yet");
    Check(ok, "explored: once a screen, as it begins (no rescanning: his rule): read now when the player is there, kept per character in Data/CinderLoad/explored-<guid>.bin with a zone-by-zone account when it has changed; nothing without the parchment");
    /* a login: no player yet, the picked character's file instead (its GUID the first eight bytes of its list entry);
     * no file, not known (the whole zone painted) */
    TestPut32(shim_module + (OBJ_MGR_VA - 0x400000), 0);
    unsigned entry = 0xB50000;                                           /* (a list of one, in the client's image) */
    TestPut32(shim_module + (CHAR_COUNT_VA - 0x400000), 1); TestPut32(shim_module + (CHAR_LIST_VA - 0x400000), entry);
    TestPut32(shim_module + (CHAR_PICKED_VA - 0x400000), 0);
    TestPut32(shim_module + (entry - 0x400000), 0x1234); TestPut32(shim_module + (entry - 0x400000) + 4, 0x50000);
    int sure = g_classSure;
    g_classSure = 1;
    ExploreAtLoad();
    ok = g_scrKnown == 2 && g_scrBits[548 >> 3] & (1 << (548 & 7)) && LogSays("from its file (a login");
    TestPut32(shim_module + (entry - 0x400000), 0x4321);                 /* (a character with no file yet) */
    ExploreAtLoad();
    ok &= g_scrKnown == 0 && g_scrBits[125 >> 3] == 0 && LogSays("not known yet (no player, no file), the whole zone painted");
    ok &= g_exPending == 1;                                              /* (not read live: once more as the world shows) */
    TestPut32(shim_module + (OBJ_MGR_VA - 0x400000), TestLow(mgr));     /* the world shows: the player is there */
    TestPut32(mgr + OBJ_MGR_GUID, 0x4321); TestPut32(f2, 0x4321);
    ExploreAfterLoad();
    ok &= Get("Data/CinderLoad/explored-0005000000004321.bin") != NULL;
    TestPut32(mgr + OBJ_MGR_GUID, 0x1234); TestPut32(f2, 0x1234);
    g_classSure = sure;
    Check(ok, "explored: at a login, before the player is there, the picked character's file (its GUID from the character list); no file, not known, the whole zone painted, and read once as the world shows, so the next login has it");
    g_parchAsked = 0; g_parchTablesOk = 0;
    munmap(low, 1 << 16);
}

/* ---- the style: the map and the bar, each the player's (his decision, 10 Oct 2026), and the world map's switches ---- */
#define STYLE_REL "Data/CinderLoad/style.txt"
static void TestStyle(void)
{
    int ok, map, bar;
    map = bar = -1;
    ok = StyleParse("map parchment\r\nbar retro\r\n", &map, &bar) == 2 && map == 1 && bar == 1;
    map = bar = -1;
    ok &= StyleParse("MAP Satellite\tbar NEO", &map, &bar) == 2 && map == 0 && bar == 0;
    map = bar = 7;
    ok &= StyleParse("# map: parchment or satellite, bar: retro or neo\nbar neo\n", &map, &bar) == 1 && map == 7 && bar == 0;
    map = bar = 7;
    ok &= StyleParse("?", &map, &bar) == 0 && StyleParse("parchments retroneo neon sat", &map, &bar) == 0 &&
          StyleParse(NULL, &map, &bar) == 0 && map == 7 && bar == 7;
    ok &= StyleParse("map=satellite,bar=neo fault", &map, &bar) == 2 && map == 0 && bar == 0;
    char v[32];
    StyleValue(v, 1, 0);
    ok &= !strcmp(v, "parchment neo");
    StyleValue(v, 0, 1);
    ok &= !strcmp(v, "satellite retro");
    Check(ok, "the style's words: parchment or satellite, retro or neo, in any case, whole words only, a note after '#' passed over; the CVar's value \"parchment neo\"");

    Fresh("style-none"); GameDir();
    StyleLoad();
    ok = g_parchAsked == 1 && g_vbarAsked == 1 && LogSays("style: the map parchment, the bar retro, from the defaults");
    Fresh("style-old-parchment"); GameDir();
    Put("Data/CinderLoad/parchment.txt", "");
    StyleLoad();
    ok &= g_parchAsked == 1 && g_vbarAsked == 1 && LogSays("from the old switch files");
    Fresh("style-old-vanilla-bar"); GameDir();
    Put("Data/CinderLoad/bar-vanilla.txt", "");
    StyleLoad();
    ok &= g_parchAsked == 0 && g_vbarAsked == 1;
    Fresh("style-file"); GameDir();
    Put("Data/CinderLoad/parchment.txt", ""); Put("Data/CinderLoad/bar-vanilla.txt", "");
    Put(STYLE_REL, "map satellite\r\nbar neo\r\n");
    StyleLoad();
    ok &= g_parchAsked == 0 && g_vbarAsked == 0 && LogSays("from Data\\CinderLoad\\style.txt (chosen in game)");
    Fresh("style-half"); GameDir();
    Put(STYLE_REL, "bar neo\n");
    StyleLoad();
    ok &= g_parchAsked == 1 && g_vbarAsked == 0;
    Fresh("style-garbage"); GameDir();
    Put(STYLE_REL, "\x01\x02 zzz satellit");
    StyleLoad();
    ok &= g_parchAsked == 1 && g_vbarAsked == 1;
    Check(ok, "the style at start: Data/CinderLoad/style.txt, ahead of all; without it the old switch files (parchment.txt: the parchment and vanilla's bar; bar-vanilla.txt: vanilla's bar on the living map); neither: the parchment and retro; a word missing, its default");

    Fresh("style-set"); GameDir();
    StyleLoad();
    const char *t;
    ok = StyleSet(0, 1, "a test") == 1 && g_parchAsked == 0 && g_vbarAsked == 1 && (t = Get(STYLE_REL)) &&
         strstr(t, "\r\nmap satellite\r\nbar retro\r\n") && t[0] == '#' && Gone("Data/CinderLoad/style.tmp") &&
         LogSays("style: the map satellite, the bar retro (a test), from the next loading screen; kept in Data\\CinderLoad\\style.txt");
    Put(STYLE_REL, "map satellite\nbar retro\n# untouched\n");
    ok &= StyleSet(0, 1, "the same") == 0 && Is(STYLE_REL, "map satellite\nbar retro\n# untouched\n");
    ok &= StyleSet(1, 0, "another") == 1;
    StyleLoad();
    ok &= g_parchAsked == 1 && g_vbarAsked == 0;
    {
        char p[700];
        snprintf(p, sizeof p, "%sData/CinderLoad", shim_gamedir);
        chmod(p, 0555);
        int changed = StyleSet(0, 0, "unwritable");
        chmod(p, 0755);
        ok &= changed && g_parchAsked == 0 && g_vbarAsked == 0 && LogSays("could NOT be written, so it holds only until the game closes");
    }
    Check(ok, "a choice made in game: kept in style.txt (written beside it, then moved over it), read back the same at the next start; the same choice again writes nothing; a file that cannot be written: it holds until the game closes, and the log says so");

    /* each screen as the view opens it: the choice, as far as it can be */
    int pzN = g_pzN;
    for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
    g_pzN = 4; g_viewTex[VT_VBORDER] = g_viewTex[VT_VGLASS] = 1;
    g_parchAsked = 1; g_vbarAsked = 1;
    StylePick();
    ok = g_parchOn == 1 && g_barVanilla == 1 && LogSays("  the view: the parchment (each screen") && LogSays("vanilla's own bar (retro");
    g_parchAsked = 0; g_vbarAsked = 0;
    StylePick();
    ok &= !g_parchOn && !g_barVanilla && LogSays("  the view: the living map (satellite), the blade for the bar (neo)");
    g_parchAsked = 1; g_pzN = 0;
    StylePick();
    ok &= !g_parchOn && LogSays("gave no zones: the living map");
    g_pzN = 4; g_vbarAsked = 1; g_viewTex[VT_VGLASS] = 0;
    StylePick();
    ok &= g_parchOn && !g_barVanilla && LogSays("did not reach OpenGL: the blade");
    Check(ok, "each screen as the view opens: the parchment when chosen and the client's tables were read, else the living map; vanilla's bar when chosen and its pictures are on OpenGL, else the blade; logged as it changes");

    /* the four, drawn: each map with each bar (neo: the blade in its frame, the logo on it; retro: vanilla's frame and
     * glass); the first frame of a parchment screen with its bar empty */
    static const char frameTxt[] = "sheet 1024 512\nleft 0 0 422 313\nrail 426 0 554 313\ncentre 558 0 598 313\nright 602 0 1024 313\n"
                                   "band 223 313\nchannel 236 301\nends 0 422\nlips 0 422\n";
    FrameParse(frameTxt, sizeof frameTxt - 1);
    { const char *lg = "logo 1024 512 4 4 1020 484\n"; LogoParse(lg, (unsigned)strlen(lg)); }
    static const float uv[8] = { 0.2f, 0.7f, 0.6f, 0.7f, 0.2f, 0.3f, 0.6f, 0.3f };
    memcpy(g_bgUV, uv, sizeof uv);
    ok = g_frame.ok && g_logo.ok;
    int all = 1;
    for (int c = 0; c < 4; c++) {
        map = c >> 1; bar = c & 1;
        for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
        g_viewTex[VT_MAP] = g_viewTex[VT_FRAME] = g_viewTex[VT_LOGO] = g_viewTex[VT_SPARK] = 1;
        g_viewTex[VT_VBORDER] = g_viewTex[VT_VGLASS] = 1;
        g_ovOn = 0; g_frameOn = 1; g_logoOn = 1; g_class = 4; g_vbFullAt = 0; g_sbFullAt = 0;
        g_pzN = 4; g_parchZone = -1; g_parchCont = -1;
        g_parchAsked = map; g_vbarAsked = bar;
        StylePick();
        int n[VT_COUNT + 1] = { 0 };
        ViewBuild(1920, 1080, 0.6f, 5000);
        for (int b = 0; b < g_vbN; b++) n[g_vb[b].tex >= 0 ? g_vb[b].tex : VT_COUNT]++;
        int one = map ? !n[VT_MAP] && n[VT_COUNT] : n[VT_MAP] == 1;
        one &= bar ? n[VT_VBORDER] && n[VT_VGLASS] && !n[VT_FRAME] && !n[VT_LOGO] : n[VT_FRAME] && n[VT_LOGO] && !n[VT_VBORDER] && !n[VT_VGLASS];
        for (int i = 0; i <= VT_COUNT; i++) n[i] = 0;
        ViewFirstList(1920, 1080, 5000);
        for (int b = 0; b < g_vbN; b++) n[g_vb[b].tex >= 0 ? g_vb[b].tex : VT_COUNT]++;
        one &= !map ? g_vbN == 0 : bar ? n[VT_VBORDER] && !n[VT_FRAME] : n[VT_FRAME] && !n[VT_VBORDER];
        if (!one) printf("  (the %s map with the %s bar is not as it should be)\n", StyleMapName(map), StyleBarName(bar));
        all &= one;
    }
    Check(ok && all, "the four, drawn: the parchment or the living map, each with vanilla's bar (its frame and glass) or the blade (its frame, the logo on it); a parchment screen's first frame: the dark paper and its own bar, empty");
    g_frame.ok = 0; g_logo.ok = 0; g_frameOn = 0; g_logoOn = 0; g_parchOn = 0; g_barVanilla = 0; g_class = 0;
    g_parchAsked = 0; g_vbarAsked = 0; g_pzN = pzN;
    for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
}

/* The fakes of the client's three calls, over a CVar in low memory (its value a 32-bit pointer, as the client's) */
static unsigned char *g_fkLow, *g_fkCv;
static char *g_fkVal;
static int g_fkUp, g_fkRuns, g_fkProbes, g_fkSetups, g_fkIgnore, g_fkRegisters, g_fkRegFail, g_fkMarked, g_fkHide;
static char g_fkHead[96], g_fkDef[64];
static int g_fkArgs[6];
static void FkSet(const char *v) { snprintf(g_fkVal, 64, "%s", v); }
static void FakeExec(const char *code, const char *source)
{
    g_fkRuns++;
    if (strcmp(source, "CinderLoad")) return;
    if (!strcmp(code, kLuaProbe)) { g_fkProbes++; if (!g_fkUp && g_fkCv) FkSet("?"); return; }
    g_fkSetups++;
    snprintf(g_fkHead, sizeof g_fkHead, "%.60s", code);
    g_fkMarked = Get("Data/CinderLoad/ui-setup.tmp") != NULL;
    if (g_fkIgnore) return;
    g_fkUp = 1;
    char m[16] = "", b[16] = "";
    if (sscanf(code, "local CL_MAP, CL_BAR = \"%15[a-z]\", \"%15[a-z]\"", m, b) == 2) { char v[40]; snprintf(v, sizeof v, "%s %s", m, b); FkSet(v); }
}
static void *FakeLookup(const char *name) { return !strcmp(name, CV_NAME) && g_fkCv && !g_fkHide ? g_fkCv : NULL; }
static void *FakeRegister(const char *name, const char *help, int flags, const char *def, void *cb, int cat, int unreg, void *arg)
{
    g_fkRegisters++;
    g_fkArgs[0] = help != NULL; g_fkArgs[1] = flags; g_fkArgs[2] = cb != NULL; g_fkArgs[3] = cat; g_fkArgs[4] = unreg; g_fkArgs[5] = arg != NULL;
    snprintf(g_fkDef, sizeof g_fkDef, "%s", def);
    if (g_fkRegFail || strcmp(name, CV_NAME)) return NULL;
    g_fkCv = g_fkLow + 0x100;
    TestPut32(g_fkCv + CV_VALUE, TestLow(g_fkVal));
    FkSet(def);
    return g_fkCv;
}
static void LuaPlant(void)
{
    for (int i = 0; i < LUA_CODE_COUNT; i++) memcpy(shim_module + (kLuaCode[i].va - 0x400000u), kLuaCode[i].bytes, (size_t)kLuaCode[i].n);
}
static void SwitchesQuiet(void);
static DWORD g_tk = 5000;                                /* the switches' clock in these tests */
static void Tick(void) { g_tk += LUA_TICK_MS; LuaTick(g_tk); } /* a tick a probe may come on */
static void TestSwitches(void)
{
    int ok;
    const char *what = "";
    /* every place the switches use, checked byte for byte first */
    Fresh("switches-code"); GameDir();
    LuaPlant();
    ok = LuaCodeOk(&what);
    for (int i = 0; i < LUA_CODE_COUNT; i++) {
        unsigned char *at = shim_module + (kLuaCode[i].va - 0x400000u) + kLuaCode[i].n / 2;
        *at ^= 0x40;
        ok &= !LuaCodeOk(&what) && what == kLuaCode[i].what;
        *at ^= 0x40;
    }
    ok &= LuaCodeOk(&what);
    Check(ok, "the world map's switches: each of the client's places they use checked byte for byte before any is used (any one off: off)");

    ok = LuaWanted() && g_luaExec == (LuaExecFn)(void *)ClientAt(LUA_EXEC_VA) && g_cvLookup == (CvLookupFn)(void *)ClientAt(CV_LOOKUP_VA) &&
         g_cvRegister == (CvRegisterFn)(void *)ClientAt(CV_REGISTER_VA);
    Put("Data/CinderLoad/ui-setup.tmp", "x");
    ok &= !LuaWanted() && Gone("Data/CinderLoad/ui-setup.tmp") && Get("Data/CinderLoad/ui-off.txt") &&
          LogSays("the last session ended while they were being made");
    ok &= !LuaWanted() && LogSays("Data\\CinderLoad\\ui-off.txt is there");
    Fresh("switches-unwritable"); GameDir();                           /* (the mark found, and ui-off.txt cannot be written: off still) */
    LuaPlant();
    Put("Data/CinderLoad/ui-setup.tmp", "x");
    {
        char p[700];
        snprintf(p, sizeof p, "%sData/CinderLoad", shim_gamedir);
        chmod(p, 0555);
        ok &= !LuaWanted() && Gone("Data/CinderLoad/ui-off.txt");
        chmod(p, 0755);
    }
    Fresh("switches-other-exe"); GameDir();
    LuaPlant();
    shim_module[LUA_EXEC_VA - 0x400000u] = 0xC3;
    ok &= !LuaWanted() && LogSays("the world map's switches: off, as WoW.exe is not as read (FrameScript_Execute");
    Check(ok, "the switches at start: on with the client as read; off with another WoW.exe, or when the last session ended while they were being made (ui-off.txt then keeps them off until it is deleted)");

    /* the game's thread at a safe moment: a player in the world, no loading screen, the UI's Lua idle */
    unsigned char *low = mmap(NULL, 1 << 16, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (low == MAP_FAILED) { Check(0, "low memory for the switches' test"); return; }
    memset(low, 0, 1 << 16);
    unsigned char *mgr = low + 0x1000, *L = low + 0x2000;
    g_fkLow = low + 0x3000; g_fkVal = (char *)(low + 0x4000);
    Fresh("switches"); GameDir();
    LuaPlant();
    TestPut32(shim_module + FIRE_LAYER_RVA, 0);
    TestPut32(shim_module + (OBJ_MGR_VA - 0x400000), TestLow(mgr));
    TestPut32(mgr + OBJ_MGR_GUID, 0x1234);
    TestPut32(shim_module + (LUA_STATE_VA - 0x400000), TestLow(L));
    TestPut32(L + LUA_L_CI, 0x5000); TestPut32(L + LUA_L_BASE_CI, 0x5000);
    ok = LuaSafeNow();
    TestPut32(shim_module + FIRE_LAYER_RVA, 7);    ok &= !LuaSafeNow(); TestPut32(shim_module + FIRE_LAYER_RVA, 0);
    TestPut32(mgr + OBJ_MGR_GUID, 0);              ok &= !LuaSafeNow(); TestPut32(mgr + OBJ_MGR_GUID + 4, 9); ok &= LuaSafeNow();
    TestPut32(mgr + OBJ_MGR_GUID + 4, 0); TestPut32(mgr + OBJ_MGR_GUID, 0x1234);
    TestPut32(shim_module + (OBJ_MGR_VA - 0x400000), 0); ok &= !LuaSafeNow(); TestPut32(shim_module + (OBJ_MGR_VA - 0x400000), TestLow(mgr));
    TestPut32(shim_module + (LUA_STATE_VA - 0x400000), 0); ok &= !LuaSafeNow(); TestPut32(shim_module + (LUA_STATE_VA - 0x400000), TestLow(L));
    L[LUA_L_NCCALLS] = 1;                          ok &= !LuaSafeNow(); L[LUA_L_NCCALLS] = 0;
    L[LUA_L_NCCALLS + 1] = 1;                      ok &= !LuaSafeNow(); L[LUA_L_NCCALLS + 1] = 0;
    TestPut32(L + LUA_L_CI, 0x5010);               ok &= !LuaSafeNow(); TestPut32(L + LUA_L_CI, 0x5000);
    ok &= LuaSafeNow();
    Check(ok, "the switches run only at a safe moment of the game's thread: a player in the world, no loading screen, the UI's Lua there and running nothing (no C call into it, its call stack at its base)");

    /* the ticks, through fakes of the client's calls */
    const char *t;
    StyleLoad();
    ok = LuaWanted();
    g_luaExec = FakeExec; g_cvLookup = FakeLookup; g_cvRegister = FakeRegister;
    g_luaOn = 1; g_luaBroken = 0; g_luaBusy = 0; g_luaFails = 0; g_luaSetups = 0; g_luaSynced = 0;
    g_fkCv = NULL; g_fkUp = 0; g_fkRuns = g_fkProbes = g_fkSetups = g_fkIgnore = g_fkRegisters = g_fkRegFail = 0;
    Tick();
    ok &= g_fkRegisters == 1 && !strcmp(g_fkDef, "parchment retro") && !g_fkArgs[0] && !g_fkArgs[1] && !g_fkArgs[2] && g_fkArgs[3] == 9 &&
          !g_fkArgs[4] && !g_fkArgs[5] && g_fkProbes == 1 && g_fkSetups == 1 && g_fkMarked && Gone("Data/CinderLoad/ui-setup.tmp") &&
          !strncmp(g_fkHead, "local CL_MAP, CL_BAR = \"parchment\", \"retro\"\n-- CinderLoad", 57) && !strcmp(g_fkVal, "parchment retro") &&
          g_luaSynced && LogSays("their CVar CinderLoadStyle made") && LogSays("made (the first: at a login or a /reload), set to parchment retro");
    Check(ok, "a login: the CVar made as RegisterCVar makes one (no help, no flags, the choice its default, no callback, category 9, registered), the probe finds no switches, they are made from the choice held (the setup's mark there while it runs, gone after)");
    Tick();
    ok = g_fkProbes == 2 && g_fkSetups == 1 && g_fkRegisters == 1;
    FkSet("satellite neo");                                           /* the player's clicks */
    Tick();
    ok &= g_parchAsked == 0 && g_vbarAsked == 0 && (t = Get(STYLE_REL)) && strstr(t, "map satellite\r\nbar neo") &&
          LogSays("style: the map satellite, the bar neo (chosen on the world map or with /cinderload)") && g_fkSetups == 1;
    g_fkUp = 0;                                                        /* a /reload */
    Tick();
    ok &= g_fkSetups == 2 && !strncmp(g_fkHead, "local CL_MAP, CL_BAR = \"satellite\", \"neo\"\n", 42) && !strcmp(g_fkVal, "satellite neo") &&
          LogSays("made (again: at a login or a /reload), set to satellite neo");
    FkSet("parchment neo");                                           /* a click, then at once a teleport */
    StyleAtLoad();
    ok &= g_parchAsked == 1 && g_vbarAsked == 0 && LogSays("(chosen just before this loading screen)");
    FkSet("satellite");                                               /* (one word: not the switches' own value) */
    Tick();
    ok &= g_parchAsked == 1 && g_vbarAsked == 0;
    FkSet("satellite retro");                                         /* a click, then a teleport: the game's first frame of it */
    g_dispNew = 1;
    GameStep(0.0f, 777000);
    ok &= g_parchAsked == 0 && g_vbarAsked == 1;
    FkSet("parchment neo");
    GameStep(0.05f, 777100);                                          /* (the same screen: only as one begins) */
    ok &= g_parchAsked == 0 && g_vbarAsked == 1;
    StyleAtLoad();
    Check(ok, "each tick: a choice made on the world map or with /cinderload taken and kept in style.txt; after a /reload made again from it; one made just before a loading screen, taken as it begins (the game's first frame of it); a value not the switches' own never");

    int runs = g_fkRuns;
    TestPut32(shim_module + FIRE_LAYER_RVA, 3);
    Tick();
    ok = g_fkRuns == runs;
    TestPut32(shim_module + FIRE_LAYER_RVA, 0);
    g_luaBusy = 1; Tick(); g_luaBusy = 0;
    ok &= g_fkRuns == runs;
    L[LUA_L_NCCALLS] = 2; Tick(); L[LUA_L_NCCALLS] = 0;
    ok &= g_fkRuns == runs;
    g_luaOn = 0; Tick(); g_luaOn = 1;
    ok &= g_fkRuns == runs;
    Check(ok, "no tick in a loading screen, inside another tick, with the UI's Lua running, or with the switches off");

    /* a new session: a CVar already there (Config.wtf's, say) never counts before the switches have set it */
    g_luaSynced = 0; g_fkUp = 0;
    FkSet("satellite retro");
    StyleAtLoad();
    ok = g_parchAsked == 1 && g_vbarAsked == 0;
    Tick();
    ok &= g_parchAsked == 1 && g_vbarAsked == 0 && !strcmp(g_fkVal, "parchment neo") && g_luaSynced;
    Check(ok, "a CVar there before the switches set it (Config.wtf's) never counts: style.txt's choice stands, and the switches set the CVar to it");

    /* their fault, a chunk that does not run, a CVar the client will not make: off for the session */
    FkSet("parchment neo fault");
    Tick();
    runs = g_fkRuns;
    g_fkUp = 0;
    Tick();
    ok = g_luaBroken && g_fkRuns == runs && LogSays("off for this session, as they met a Lua error");
    g_luaBroken = 0; g_fkUp = 0; g_fkIgnore = 1;
    FkSet("parchment neo");
    Tick();
    ok &= !g_luaBroken && g_luaFails == 1 && LogSays("made, but no word back from them (1 time)");
    Tick();
    ok &= g_luaBroken && LogSays("as they did not run (twice made, no word back)");
    FkSet("satellite neo");                                           /* off: not even a screen's start asks the client */
    StyleAtLoad();
    ok &= g_parchAsked == 1 && g_vbarAsked == 0;
    g_luaBroken = 0; g_luaFails = 0; g_fkUp = 0;                       /* once with no word back, then a good one: counted again */
    Tick();
    g_fkIgnore = 0; g_fkUp = 0;
    Tick();
    g_fkIgnore = 1; g_fkUp = 0;
    Tick();
    ok &= !g_luaBroken && g_luaFails == 1;
    g_luaBroken = 0; g_luaFails = 0; g_fkIgnore = 0; g_fkCv = NULL; g_fkRegFail = 1;
    Tick();
    ok &= g_luaBroken && LogSays("the client would not make its CVar");
    g_luaBroken = 0; g_fkCv = NULL; g_fkRegFail = 0; g_fkHide = 1;      /* made, but the client's lookup does not find it */
    Tick();
    ok &= g_luaBroken;
    g_fkHide = 0;
    Check(ok, "off for the session (logged; the world map and style.txt as ever): when the switches report a Lua error, when they are made twice and never run, when the client will not make their CVar");

    char small[64];
    Check(LuaChunk(small, sizeof small, 1, 1) == 0, "the switches' chunk: never written past its buffer");
    SwitchesQuiet();
    munmap(low, 1 << 16);
}

/* A file of these bytes, in the case's folder. */
static void PutN(const char *rel, const void *data, size_t n)
{
    char p[700];
    snprintf(p, sizeof p, "%s%s", shim_gamedir, rel);
    FILE *f = fopen(p, "wb");
    if (f) { fwrite(data, 1, n, f); fclose(f); }
}
/* The game in the world at a safe moment for the switches (a player, no loading screen, the UI's Lua idle), over low
 * memory: low + 0x1000 the object manager, + 0x2000 the lua_State, + 0x3000 the fake CVar, + 0x4000 its value. */
static void SwitchesWorld(unsigned char *low)
{
    memset(low, 0, 1 << 16);
    unsigned char *mgr = low + 0x1000, *L = low + 0x2000;
    g_fkLow = low + 0x3000; g_fkVal = (char *)(low + 0x4000);
    LuaPlant();
    TestPut32(shim_module + FIRE_LAYER_RVA, 0);
    TestPut32(shim_module + (OBJ_MGR_VA - 0x400000), TestLow(mgr));
    TestPut32(mgr + OBJ_MGR_GUID, 0x1234);
    TestPut32(shim_module + (LUA_STATE_VA - 0x400000), TestLow(L));
    TestPut32(L + LUA_L_CI, 0x5000); TestPut32(L + LUA_L_BASE_CI, 0x5000);
    g_luaExec = FakeExec; g_cvLookup = FakeLookup; g_cvRegister = FakeRegister;
    g_luaOn = 1; g_luaBroken = 0; g_luaBusy = 0; g_luaFails = 0; g_luaSetups = 0; g_luaSynced = 0; g_luaProbeAt = 0; g_luaCv = NULL;
    g_luaPosted = 0; g_luaPostedAt = 0; g_luaMsg = 0;
    g_fkCv = NULL; g_fkUp = 0; g_fkRuns = g_fkProbes = g_fkSetups = g_fkIgnore = g_fkRegisters = g_fkRegFail = g_fkHide = 0;
    shim_game = (HWND)(long)0x1234; shim_game_alive = 1; shim_game_tid = 77; shim_hook = NULL;
    shim_hooks = shim_unhooks = shim_hook_fail = shim_posted = shim_post_fail = 0; shim_posted_msg = 0;
}
static void SwitchesQuiet(void)
{
    g_luaOn = 0; g_luaBroken = 0; g_luaFails = 0; g_luaSynced = 0; g_luaProbeAt = 0; g_luaCv = NULL; g_luaPosted = 0; g_luaMsg = 0;
    g_luaExec = NULL; g_cvLookup = NULL; g_cvRegister = NULL;
    shim_game = NULL; shim_game_alive = 0; shim_hook = NULL;
    g_parchAsked = 0; g_vbarAsked = 0;
}
static int g_vsStarts, g_vsFail;
static int FakeViewThreadStart(const unsigned char *base) { (void)base; g_vsStarts++; return !g_vsFail; }
/* The style's file as Notepad and others save it; the CVar's value only as the switches write it; the black first frames
 * only for a parchment that will be drawn. */
static void TestStyleFiles(void)
{
    int ok = 1, map, bar, fault;
    /* style.txt in UTF-16 (Notepad's "Unicode", little or big endian, with its mark or without) and UTF-8 with its mark */
    static const char txt[] = "map satellite\r\nbar neo\r\n";
    for (int kind = 0; kind < 6; kind++) {
        unsigned char b[200];
        size_t n = 0;
        if (kind == 0) { b[n++] = 0xFF; b[n++] = 0xFE; }
        if (kind == 1) { b[n++] = 0xFE; b[n++] = 0xFF; }
        if (kind == 4) { b[n++] = 0xEF; b[n++] = 0xBB; b[n++] = 0xBF; }
        for (const char *c = txt; *c; c++) {
            if (kind == 0 || kind == 2) { b[n++] = (unsigned char)*c; b[n++] = 0; }
            else if (kind == 1 || kind == 3) { b[n++] = 0; b[n++] = (unsigned char)*c; }
            else b[n++] = (unsigned char)*c;
        }
        Fresh("style-unicode"); GameDir();
        PutN(STYLE_REL, b, n);
        g_parchAsked = g_vbarAsked = 7;
        StyleLoad();
        int one = g_parchAsked == 0 && g_vbarAsked == 0;
        if (!one) printf("  (style.txt in form %d not read)\n", kind);
        ok &= one;
    }
    {   /* a character past ASCII inside a word breaks it, even one whose low byte is a letter (UTF-16 "bar n", U+0165,
         * "o": 0x65 is 'e'): not a word of ours, its default */
        static const unsigned char u[] = { 0xFF, 0xFE, 'b', 0, 'a', 0, 'r', 0, ' ', 0, 'n', 0, 0x65, 0x01, 'o', 0, '\n', 0,
                                           'm', 0, 'a', 0, 'p', 0, ' ', 0, 's', 0, 'a', 0, 't', 0, 'e', 0, 'l', 0, 'l', 0, 'i', 0, 't', 0, 'e', 0 };
        Fresh("style-unicode-odd"); GameDir();
        PutN(STYLE_REL, u, sizeof u);
        StyleLoad();
        ok &= g_parchAsked == 0 && g_vbarAsked == 1;
        static const unsigned char odd[] = { 'm', 'a', 'p', ' ', 's', 'a', 't', 'e', 'l', 'l', 'i', 't', 'e' };   /* (no newline, plain) */
        PutN(STYLE_REL, odd, sizeof odd);
        StyleLoad();
        ok &= g_parchAsked == 0 && g_vbarAsked == 1;
    }
    Check(ok, "style.txt read whether saved as ASCII, UTF-8 with its mark, or UTF-16 little or big endian with or without its mark (Notepad's \"Unicode\"); a character past ASCII breaks a word");

    /* the CVar: only exactly what the switches write */
    ok = StyleCvarParse("satellite neo", &map, &bar, &fault) && map == 0 && bar == 0 && !fault;
    ok &= StyleCvarParse("parchment retro", &map, &bar, &fault) && map == 1 && bar == 1 && !fault;
    ok &= StyleCvarParse("parchment neo fault", &map, &bar, &fault) && map == 1 && bar == 0 && fault;
    static const char *const kNot[] = { "?", "", "parchment", "parchment retro ", "parchment  retro", "Parchment retro", "parchment retro faulty",
                                        "parchment retro f", "parchment retro fault x", "parchment retrofault", "neo satellite",
                                        "satellite neo retro", "satellites neo", "map=satellite,bar=neo" };
    for (size_t i = 0; i < sizeof kNot / sizeof kNot[0]; i++) {
        map = bar = fault = 7;
        int no = !StyleCvarParse(kNot[i], &map, &bar, &fault) && map == 7 && bar == 7 && fault == 7;
        if (!no) printf("  (the CVar's \"%s\" taken)\n", kNot[i]);
        ok &= no;
    }
    Check(ok, "the CVar heard only as the switches write it: \"<map> <bar>\", or with \" fault\" after; anything else (a word more or less, another case, the probe's \"?\", a run-on \"faulty\") is not theirs");

    /* the black first frames: only for a parchment the view will draw */
    {
        int pzN = g_pzN, asked = g_parchAsked;
        LONG live = g_viewLive, drew = g_viewDrew;
        g_viewLive = 1; g_viewDrew = 1;
        g_parchAsked = 1; g_pzN = 0;
        ok = !ParchWillDraw(1000);
        g_pzN = 3;
        ok &= ParchWillDraw(1000);
        g_parchAsked = 0;
        ok &= !ParchWillDraw(1000);
        g_parchAsked = 1; g_viewLive = 0;
        ok &= !ParchWillDraw(1000);
        g_pzN = pzN; g_parchAsked = asked; g_viewLive = live; g_viewDrew = drew;
        Check(ok, "the game's first frames black under a parchment screen only when the parchment can be drawn (the client's tables gave zones) and the view draws: chosen but not drawable, the living map as the view will draw it");
    }

}
/* The view's start and each screen's opening (outside the 32-bit part now, so these reach them); the switches' message
 * pump, hook and exit. */
static void TestSwitchesPump(void)
{
    int ok;
    /* the view's start: the gate, then the style from style.txt, the view's thread, then the switches */
    Fresh("view-start"); GameDir();
    LuaPlant();
    int wine = shim_wine, threads = shim_threads;
    shim_wine = 1;                                                       /* (the gate: on under Wine with no files) */
    g_map.ok = 1; g_driftOn = 1;
    Put(STYLE_REL, "map satellite\r\nbar neo\r\n");
    g_parchAsked = g_vbarAsked = 1; g_luaOn = 0; g_vsStarts = 0; g_vsFail = 0; shim_threads = 0;
    ok = ViewStartFrom(shim_module, FakeViewThreadStart) && g_vsStarts == 1 && g_parchAsked == 0 && g_vbarAsked == 0 && g_viewOn &&
         g_viewLive && g_luaOn && shim_threads == 1 && LogSays("style: the map satellite, the bar neo, from Data\\CinderLoad\\style.txt") &&
         LogSays("view on:");
    g_viewOn = 0; g_viewLive = 0; g_luaOn = 0;
    Fresh("view-start-no-thread"); GameDir();
    LuaPlant();
    g_map.ok = 1; g_driftOn = 1;                                         /* (Fresh takes them away) */
    g_vsFail = 1; shim_threads = 0;
    ok &= !ViewStartFrom(shim_module, FakeViewThreadStart) && !g_viewOn && !g_viewLive && !g_luaOn && shim_threads == 0 &&
          LogSays("view off: its thread could not be started");
    g_vsFail = 0;
    Fresh("view-start-off"); GameDir();
    LuaPlant();
    g_map.ok = 1; g_driftOn = 1;
    Put("Data/CinderLoad/view-off.txt", "");
    Put(STYLE_REL, "map satellite\r\n");
    g_parchAsked = 1; g_vsStarts = 0;
    ok &= !ViewStartFrom(shim_module, FakeViewThreadStart) && g_vsStarts == 0 && g_parchAsked == 1 && !g_luaOn && LogSays("view off:");
    Fresh("view-start-no-map"); GameDir();
    LuaPlant();
    g_map.ok = 0; g_driftOn = 1;
    ok &= !ViewStartFrom(shim_module, FakeViewThreadStart) && g_vsStarts == 0 && !g_luaOn;
    g_map.ok = 0; g_driftOn = 0; shim_wine = wine; shim_threads = threads;
    Check(ok, "the view's start: past its gate, the style from style.txt, the view's thread, then the world map's switches (their thread); no thread for the view, no switches; the view off, neither");

    /* each screen as the view opens it: the choice of this moment, and the first frame's list for it */
    {
        int pzN = g_pzN;
        for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
        g_viewTex[VT_VBORDER] = g_viewTex[VT_VGLASS] = 1;
        g_pzN = 4; g_parchZone = -1; g_parchCont = -1; g_class = 4; g_vbFullAt = 0;
        g_parchAsked = 1; g_vbarAsked = 1;
        ViewOpenFirst(1920, 1080, 5000);
        ok = g_parchOn && g_barVanilla && g_vbN > 0;
        g_parchAsked = 0;
        ViewOpenFirst(1920, 1080, 5000);
        ok &= !g_parchOn && g_vbN == 0;
        g_parchAsked = 1; g_vbarAsked = 0;
        ViewOpenFirst(1920, 1080, 5000);
        ok &= g_parchOn && !g_barVanilla;
        g_parchOn = 0; g_barVanilla = 0; g_class = 0; g_parchAsked = 0; g_vbarAsked = 0; g_pzN = pzN;
        for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
        Check(ok, "each screen as the view opens it: the choice of that moment (a parchment screen's first frame its dark paper and bar, the living map's none of its own), changed from one screen to the next");
    }

    /* the pump: our thread's message, the game's thread's hook */
    unsigned char *low = mmap(NULL, 1 << 16, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (low == MAP_FAILED) { Check(0, "low memory for the switches' pump"); return; }
    Fresh("switches-pump"); GameDir();
    SwitchesWorld(low);
    StyleLoad();
    LuaPump pump = { NULL, NULL, 0, 0 };
    ok = LuaPumpStep(&pump, 10000) && g_luaMsg == 0xC0DE && shim_hooks == 1 && shim_hook == LuaHook && shim_hook_tid == 77 &&
         shim_posted == 1 && shim_posted_msg == g_luaMsg && g_luaPosted;
    ok &= LuaPumpStep(&pump, 10250) && shim_posted == 1;                 /* (on its way: not sent again) */
    MSG msg = { shim_game, g_luaMsg, 0, 0, 0, { 0, 0 } };
    shim_ticks = 10300;
    shim_hook(HC_ACTION, PM_NOREMOVE, (LPARAM)&msg);                     /* the game looks at its queue: no tick */
    ok &= g_fkRuns == 0 && g_luaPosted && msg.message == g_luaMsg;
    shim_hook(1, PM_REMOVE, (LPARAM)&msg);                               /* (not HC_ACTION: never) */
    ok &= g_fkRuns == 0 && g_luaPosted;
    shim_hook(HC_ACTION, PM_REMOVE, (LPARAM)&msg);                       /* taken: the tick, made WM_NULL for the game's window */
    ok &= g_fkSetups == 1 && g_luaSynced && msg.message == WM_NULL && !g_luaPosted;
    ok &= LuaPumpStep(&pump, 10500) && shim_posted == 2;
    Check(ok, "the switches' pump: our thread registers its message, hooks the game's own thread and sends it one message at a time; the game's thread ticks only on taking it off its queue (not on a look, not on another hook code), and never shows it to the game's window; then the next is sent");

    /* a choice made in game kept within a poll (no probe needed), the probe still once a second */
    FkSet("satellite neo");
    int probes = g_fkProbes;
    msg.message = g_luaMsg; shim_ticks = 10550;
    shim_hook(HC_ACTION, PM_REMOVE, (LPARAM)&msg);
    const char *t = Get(STYLE_REL);
    ok = g_fkProbes == probes && t && strstr(t, "map satellite\r\nbar neo") && g_parchAsked == 0 && g_vbarAsked == 0;
    msg.message = g_luaMsg; shim_ticks = 10300 + LUA_TICK_MS;
    shim_hook(HC_ACTION, PM_REMOVE, (LPARAM)&msg);
    ok &= g_fkProbes == probes + 1;
    Check(ok, "a choice made on the world map kept in style.txt on the next message, a quarter of a second at most, not the next probe's second");

    /* in a loading screen nothing is sent; a message never taken is sent again after 5 s; a window gone is found again */
    TestPut32(shim_module + FIRE_LAYER_RVA, 3);
    int posted = shim_posted;
    ok = LuaPumpStep(&pump, 11000) && shim_posted == posted;
    TestPut32(shim_module + FIRE_LAYER_RVA, 0);
    ok &= LuaPumpStep(&pump, 11250) && shim_posted == posted + 1;
    ok &= LuaPumpStep(&pump, 11250 + LUA_POST_STALE_MS - 1) && shim_posted == posted + 1;
    ok &= LuaPumpStep(&pump, 11250 + LUA_POST_STALE_MS) && shim_posted == posted + 2 && LogSays("our message was not taken in 5 s");
    shim_game_alive = 0;                                                 /* the window gone with ours queued */
    ok &= LuaPumpStep(&pump, 17000) && !g_luaPosted;
    shim_game = (HWND)(long)0x5678; shim_game_alive = 1; shim_game_tid = 78;   /* a new one, on another thread */
    ok &= LuaPumpStep(&pump, 17250) && shim_unhooks == 1 && shim_hooks == 2 && shim_hook_tid == 78 && g_luaPosted &&
          shim_posted == posted + 3;
    Check(ok, "no message during a loading screen; one not taken in 5 s (a long freeze, its window gone) sent again, never stuck; a new game window found and its thread hooked");

    /* the game quitting: its WM_QUIT taken, a choice made since the last message kept */
    FkSet("parchment retro");
    MSG quit = { shim_game, WM_QUIT, 0, 0, 0, { 0, 0 } };
    shim_hook(HC_ACTION, PM_NOREMOVE, (LPARAM)&quit);
    ok = g_parchAsked == 0;
    shim_hook(HC_ACTION, PM_REMOVE, (LPARAM)&quit);
    t = Get(STYLE_REL);
    ok &= g_parchAsked == 1 && g_vbarAsked == 1 && t && strstr(t, "map parchment\r\nbar retro") && LogSays("(chosen just before the game closed)");
    g_luaBroken = 1;                                                     /* (off for the session: not even as it quits) */
    FkSet("satellite neo");
    shim_hook(HC_ACTION, PM_REMOVE, (LPARAM)&quit);
    ok &= g_parchAsked == 1 && (t = Get(STYLE_REL)) && strstr(t, "map parchment\r\nbar retro");
    g_luaBroken = 0;
    FkSet("parchment retro");
    Check(ok, "the game quitting (its WM_QUIT off the queue): a choice made in its last moments kept in style.txt; with the switches off for the session, nothing taken");

    /* a choice made just before a loading screen is the style of that screen, its explored areas read for it */
    g_parchAsked = 0; g_parchTablesOk = 1;
    StyleSet(0, 1, "a test");
    FkSet("parchment retro");
    LONG seq = g_scrSeq;
    g_dispNew = 1;
    GameStep(0.0f, 900000);
    ok = g_parchAsked == 1 && g_scrSeq == seq + 1;
    g_parchTablesOk = 0;
    Check(ok, "the parchment chosen a moment before a loading screen: taken as it begins, before its explored areas are read, so they are read for that very screen");

    /* off for good: the hook out, the thread ends; a thread that will not take a hook: off */
    g_luaBroken = 1;
    ok = !LuaPumpStep(&pump, 18000) && shim_unhooks == 2 && shim_hook == NULL;
    g_luaBroken = 0; shim_hook_fail = 1;
    LuaPump pump2 = { NULL, NULL, 0, 0 };
    ok &= !LuaPumpStep(&pump2, 18250) && g_luaBroken && LogSays("the game's thread would not take a message hook");
    shim_hook_fail = 0;
    Check(ok, "the switches off for good: the hook taken out and their thread ended; a game thread that will not take the hook: off, logged");
    SwitchesQuiet();
    munmap(low, 1 << 16);
}

static const char *g_luaBin;                             /* (TestSwitchesLua's finds) */
static char g_testsDir[1024], g_fxDir[800];
/* The switches' Lua (dll/switches.lua) as the DLL sends it, run by Lua 5.1 against a stubbed 1.12 UI with 5.0's limits
 * (tests/test_lua.lua); and switches_lua.h the same text as switches.lua. */
static void TestSwitchesLua(void)
{
    char here[1024], p[1200];
    snprintf(here, sizeof here, "%s", __FILE__);
    char *slash = strrchr(here, '/');
    if (slash) *slash = 0; else snprintf(here, sizeof here, ".");
    snprintf(p, sizeof p, "%s/../dll/switches.lua", here);
    FILE *f = fopen(p, "rb");
    static char src[65536];
    size_t n = f ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f) fclose(f);
    src[n] = 0;
    Check(n > 0 && !strcmp(src, kSwitchesLua), "dll/switches_lua.h holds dll/switches.lua as it is (tools/lua_to_c.py)");
    char dir[700];
    snprintf(dir, sizeof dir, "%s/lua", g_root);
    mkdir(dir, 0755);
    static char chunk[sizeof kSwitchesLua + 128];
    const struct { const char *name; int map, bar; } kOut[2] = { { "setup.lua", 1, 1 }, { "setup-satellite-neo.lua", 0, 0 } };
    for (int i = 0; i < 2; i++) {
        snprintf(p, sizeof p, "%s/%s", dir, kOut[i].name);
        f = fopen(p, "wb");
        int len = LuaChunk(chunk, (int)sizeof chunk, kOut[i].map, kOut[i].bar);
        if (f) { fwrite(chunk, 1, (size_t)len, f); fclose(f); }
    }
    snprintf(p, sizeof p, "%s/probe.lua", dir);
    f = fopen(p, "wb");
    if (f) { fputs(kLuaProbe, f); fclose(f); }
    const char *lua = !system("lua5.1 -v >/dev/null 2>&1") ? "lua5.1" : !system("luajit -v >/dev/null 2>&1") ? "luajit" : NULL;
    if (!lua) { printf("  (no lua5.1 or luajit here: the switches' Lua not run)\n"); return; }
    g_luaBin = lua;
    snprintf(g_testsDir, sizeof g_testsDir, "%s", here);
    const char *layout = getenv("CINDERLOAD_LAYOUT"), *widths = getenv("CINDERLOAD_WIDTHS");
    char fx[800] = "";
    if (g_client) {                                      /* the client's own drop-down and world map code, out of its archives */
        static const char *const kFx[2] = { "UIDropDownMenu.lua", "WorldMapFrame.lua" };
        Fresh("framexml"); GameDir();
        int got = LinkClient(g_client);
        snprintf(fx, sizeof fx, "%s/framexml", dir);
        mkdir(fx, 0755);
        for (int i = 0; got && i < 2; i++) {
            char in[200], outp[1000];
            unsigned char *data = NULL;
            unsigned len = 0;
            const char *from = "";
            snprintf(in, sizeof in, "Interface\\FrameXML\\%s", kFx[i]);
            got = ClientPicture(in, "interface.MPQ", &data, &len, &from);
            snprintf(outp, sizeof outp, "%s/%s", fx, kFx[i]);
            FILE *o = got ? fopen(outp, "wb") : NULL;
            if (o) { fwrite(data, 1, len, o); fclose(o); printf("  lua: the client's %s, from its %s\n", kFx[i], from); }
            got = o != NULL;
            if (data) HeapFree(GetProcessHeap(), 0, data);
        }
        Check(got, "a real client: its own UIDropDownMenu.lua and WorldMapFrame.lua read out of its archives for the switches' Lua");
        if (!got) fx[0] = 0;
        snprintf(g_fxDir, sizeof g_fxDir, "%s", fx);
    }
    char cmd[4000];
    snprintf(cmd, sizeof cmd, "%s %s/test_lua.lua '%s' '%s%s' '%s%s' '%s%s' 2>&1", lua, here, dir, layout ? "layout=" : "", layout ? layout : "",
             widths ? "widths=" : "", widths ? widths : "", fx[0] ? "framexml=" : "", fx);
    FILE *r = popen(cmd, "r");
    char line[1200];
    int seen = 0, done = 0;
    while (r && fgets(line, sizeof line, r)) {
        line[strcspn(line, "\n")] = 0;
        if (!strncmp(line, "ok ", 3)) { Check(1, line + 3); seen++; }
        else if (!strncmp(line, "FAIL ", 5)) { Check(0, line + 5); seen++; }
        else if (!strncmp(line, "done ", 5)) done = 1;
        else printf("  lua: %s\n", line);
    }
    int st = r ? pclose(r) : -1;
    Check(r && done && st == 0 && seen >= 60, "the switches' Lua ran whole under the stubbed UI");
}

/* ---- the whole path, run (the skeptic's finding, 11 Oct: it had never run anywhere): the DLL's start, its thread's
 * message, the game's thread taking it, the CVar, the switches' chunk run in the game's UI, a player's pick on the world
 * map's drop-down, style.txt, a loading screen drawn as picked, a /reload, the game quitting, the next session. The game's
 * UI is tests/wow_ui.lua's (the client's own drop-down and world map code with a client folder), in a Lua of its own
 * (tests/lua_bridge.lua, over a pipe); the client's three calls are the bridge (FrameScript_Execute) and the fake CVar. */
static FILE *g_brTo, *g_brFrom;
static pid_t g_brPid;
static char g_brInfo[8][400];
static int g_brInfoN;
static int BridgeOpen(void)
{
    int to[2], from[2];
    char script[1100], fx[900];
    snprintf(script, sizeof script, "%s/lua_bridge.lua", g_testsDir);
    snprintf(fx, sizeof fx, "framexml=%s", g_fxDir);
    if (!g_luaBin || pipe(to) || pipe(from)) return 0;
    fflush(stdout);
    g_brPid = fork();
    if (g_brPid < 0) return 0;
    if (!g_brPid) {
        dup2(to[0], 0); dup2(from[1], 1);
        close(to[0]); close(to[1]); close(from[0]); close(from[1]);
        execlp(g_luaBin, g_luaBin, script, fx, (char *)NULL);
        _exit(127);
    }
    close(to[0]); close(from[1]);
    g_brTo = fdopen(to[1], "w"); g_brFrom = fdopen(from[0], "r");
    return g_brTo && g_brFrom;
}
static void BridgeClose(void)
{
    if (g_brTo) fclose(g_brTo);
    if (g_brFrom) fclose(g_brFrom);
    g_brTo = g_brFrom = NULL;
    if (g_brPid > 0) waitpid(g_brPid, NULL, 0);
    g_brPid = 0;
}
/* One request: the CVar as the client holds it, the command (and data); the UI's answer taken (its info lines, the CVar). */
static int BridgeSend(const char *cmd, const char *data, size_t n)
{
    g_brInfoN = 0;
    if (!g_brTo) return 0;
    if (g_fkCv) fprintf(g_brTo, "cvar %s\n", g_fkVal); else fputs("nocvar\n", g_brTo);
    fprintf(g_brTo, "%s\n", cmd);
    if (data) fwrite(data, 1, n, g_brTo);
    fflush(g_brTo);
    char line[1200];
    while (fgets(line, sizeof line, g_brFrom)) {
        line[strcspn(line, "\n")] = 0;
        if (!strcmp(line, "end")) return 1;
        if (!strncmp(line, "cvar ", 5)) { if (g_fkCv) FkSet(line + 5); }
        else if (!strncmp(line, "info ", 5) && g_brInfoN < 8) snprintf(g_brInfo[g_brInfoN++], sizeof g_brInfo[0], "%s", line + 5);
    }
    return 0;
}
static int Bridge(const char *cmd) { return BridgeSend(cmd, NULL, 0); }
static int BridgeSaid(const char *s) { for (int i = 0; i < g_brInfoN; i++) if (strstr(g_brInfo[i], s)) return 1; return 0; }
static void BridgeExec(const char *code, const char *source)          /* FrameScript_Execute, in the stubbed UI */
{
    g_fkRuns++;
    if (strcmp(source, "CinderLoad")) return;
    if (!strcmp(code, kLuaProbe)) g_fkProbes++; else g_fkSetups++;
    char cmd[40];
    snprintf(cmd, sizeof cmd, "exec %u", (unsigned)strlen(code));
    BridgeSend(cmd, code, strlen(code));
}
static LuaPump g_pathPump;
/* Our thread's poll at now, and the game's thread taking our message off its queue if one was sent. */
static void Pump(DWORD now)
{
    LuaPumpStep(&g_pathPump, now);
    if (!g_luaPosted || !shim_hook) return;
    MSG msg = { shim_game, g_luaMsg, 0, 0, 0, { 0, 0 } };
    shim_ticks = now;
    shim_hook(HC_ACTION, PM_REMOVE, (LPARAM)&msg);
}
static void TestSwitchesPath(void)
{
    if (!g_luaBin) { printf("  (no lua5.1 or luajit here: the whole path not run)\n"); return; }
    signal(SIGPIPE, SIG_IGN);
    unsigned char *low = mmap(NULL, 1 << 16, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (low == MAP_FAILED) { Check(0, "low memory for the whole path"); return; }
    int wine = shim_wine, pzN = g_pzN, ok;
    const char *t;
    Fresh("switches-path"); GameDir();
    SwitchesWorld(low);
    shim_wine = 1; g_map.ok = 1; g_driftOn = 1;
    for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
    g_viewTex[VT_VBORDER] = g_viewTex[VT_VGLASS] = 1;
    g_pzN = 4; g_parchZone = -1; g_parchCont = -1; g_class = 4;
    memset(&g_pathPump, 0, sizeof g_pathPump);
    /* the game starts (DllMain's ViewStart): the style (none chosen yet), the view, the switches' thread */
    ok = BridgeOpen() && ViewStartFrom(shim_module, FakeViewThreadStart) && g_luaOn && g_parchAsked == 1 && g_vbarAsked == 1;
    g_luaExec = BridgeExec; g_cvLookup = FakeLookup; g_cvRegister = FakeRegister;
    /* a login: the first message taken makes the CVar, finds no switches, makes them in the game's UI */
    DWORD now = 20000;
    Pump(now);
    ok &= g_fkCv && g_luaSynced && g_fkSetups == 1 && !strcmp(g_fkVal, "parchment retro");
    Bridge("openmap");
    Bridge("text map");
    ok &= BridgeSaid("Parchment shown");
    Check(ok, "the whole path, a login: the DLL starts (the style the defaults), its thread's message is taken on the game's thread, which makes the CVar and the switches in the game's UI: the world map's drop-down says Parchment");

    /* Satellite picked on the world map: the CVar says so; the next message (a quarter second) keeps it; the next screen */
    Bridge("pick map satellite");
    ok = BridgeSaid("picked") && !strcmp(g_fkVal, "satellite retro") && g_parchAsked == 1;
    now += LUA_POLL_MS;
    Pump(now);
    ok &= g_parchAsked == 0 && (t = Get(STYLE_REL)) && strstr(t, "map satellite\r\nbar retro");
    g_dispNew = 1;
    GameStep(0.0f, now + 10);
    ViewOpenFirst(1920, 1080, now + 20);
    ok &= !g_parchOn && g_barVanilla && g_vbN == 0;
    Check(ok, "the whole path: Satellite picked from the drop-down, the CVar says so, the game's next message keeps it in style.txt, and the next loading screen is the living map");

    /* the parchment picked a moment before a loading screen, no message between: that screen the parchment */
    Bridge("pick map parchment");
    g_dispNew = 1;
    GameStep(0.0f, now + 100);
    ViewOpenFirst(1920, 1080, now + 110);
    ok = g_parchAsked == 1 && g_parchOn && g_vbN > 0 && (t = Get(STYLE_REL)) && strstr(t, "map parchment");
    Check(ok, "the whole path: the parchment picked just before a loading screen is that very screen's (taken as it begins), and kept");

    /* /cinderload bar in the chat */
    Bridge("slash bar");
    ok = BridgeSaid("loading bar Neo") && !strcmp(g_fkVal, "parchment neo");
    now += LUA_POLL_MS;
    Pump(now);
    ok &= g_vbarAsked == 0 && (t = Get(STYLE_REL)) && strstr(t, "bar neo");
    g_dispNew = 1;
    GameStep(0.0f, now + 10);
    ViewOpenFirst(1920, 1080, now + 20);
    ok &= g_parchOn && !g_barVanilla;
    Check(ok, "the whole path: /cinderload bar in the chat, kept the same way; the next screen the parchment with the blade");

    /* a /reload: the UI made anew; the probe finds no switches; made again from the choice held, showing it */
    Bridge("fresh");
    int setups = g_fkSetups;
    now += LUA_TICK_MS;
    Pump(now);
    Bridge("openmap");
    Bridge("text bar");
    ok = g_fkSetups == setups + 1 && !strcmp(g_fkVal, "parchment neo") && BridgeSaid("Neo shown");
    Bridge("minimize");
    Bridge("text map");
    ok &= BridgeSaid("hidden");
    Bridge("maximize");
    Bridge("text map");
    ok &= BridgeSaid("Parchment shown");
    Check(ok, "the whole path: a /reload, the switches made again from what the DLL holds (the drop-downs say it); the map minimised, they hide with its own; maximised, back");

    /* Retro picked in the game's last moment, then it quits: kept (its WM_QUIT) */
    Bridge("pick bar retro");
    MSG quit = { shim_game, WM_QUIT, 0, 0, 0, { 0, 0 } };
    shim_hook(HC_ACTION, PM_REMOVE, (LPARAM)&quit);
    ok = (t = Get(STYLE_REL)) && strstr(t, "map parchment\r\nbar retro");
    Bridge("errors");
    ok &= BridgeSaid("0 ");
    BridgeClose();
    Check(ok, "the whole path: Retro picked in the game's last moment, kept as it quits; no Lua error in its UI all along");

    /* the next session: style.txt at start; a CVar left in Config.wtf (an old choice) never counts; the switches made from
     * style.txt's, the drop-downs saying it */
    SwitchesWorld(low);
    memset(&g_pathPump, 0, sizeof g_pathPump);
    g_fkCv = g_fkLow + 0x100;
    TestPut32(g_fkCv + CV_VALUE, TestLow(g_fkVal));
    FkSet("satellite neo");
    g_parchAsked = g_vbarAsked = 7;
    ok = BridgeOpen() && ViewStartFrom(shim_module, FakeViewThreadStart) && g_parchAsked == 1 && g_vbarAsked == 1;
    g_luaExec = BridgeExec; g_cvLookup = FakeLookup; g_cvRegister = FakeRegister;
    now += 60000;
    Pump(now);
    Bridge("openmap");
    Bridge("text map");
    ok &= g_parchAsked == 1 && g_vbarAsked == 1 && !strcmp(g_fkVal, "parchment retro") && BridgeSaid("Parchment shown");
    Bridge("text bar");
    ok &= BridgeSaid("Retro shown");
    BridgeClose();
    Check(ok, "the whole path, the next session: the style from style.txt at start, the switches made from it (an old value of the CVar in Config.wtf never counts), the drop-downs saying it");

    g_viewOn = 0; g_viewLive = 0; shim_wine = wine; g_pzN = pzN; g_map.ok = 0; g_driftOn = 0;
    g_parchOn = 0; g_barVanilla = 0; g_class = 0;
    for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
    SwitchesQuiet();
    munmap(low, 1 << 16);
}

/* A real client's WoW.exe: the places the switches use, and the check that keeps a DLL's own functions out of its Lua */
static void LuaRealClient(const char *client)
{
    char exe[1024];
    snprintf(exe, sizeof exe, "%s/WoW.exe", client);
    FILE *x = fopen(exe, "rb");
    int match = x != NULL;
    for (int i = 0; x && match && i < LUA_CODE_COUNT; i++) {
        unsigned char got[64];
        long off = ExeOffset(x, kLuaCode[i].va - 0x400000u);
        match &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, (size_t)kLuaCode[i].n, x) == (size_t)kLuaCode[i].n &&
                 !memcmp(got, kLuaCode[i].bytes, (size_t)kLuaCode[i].n);
        if (!match) printf("FAIL: VA 0x%X (%s) differs in %s\n", kLuaCode[i].va, kLuaCode[i].what, exe);
    }
    /* luaD_precall hands each C function's address to 0x42A320 before it calls it ("Invalid function pointer: %p" when it
     * is outside the client's code): why the switches speak through a CVar */
    static const unsigned char kPrecall[12] = { 0x8B, 0xCF, 0xE8, 0x7A, 0x41, 0xD3, 0xFF, 0x8B, 0xCE, 0xFF, 0xD7, 0xC1 };
    static const unsigned char kRange[8] = { 0x3B, 0xF0, 0x72, 0x04, 0x3B, 0xF1, 0x72, 0x1D };
    unsigned char got[32];
    long off = x ? ExeOffset(x, 0x6F619F - 0x400000u) : -1;
    int checked = off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, 12, x) == 12 && !memcmp(got, kPrecall, 12);
    off = x ? ExeOffset(x, 0x42A34C - 0x400000u) : -1;
    checked &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, 8, x) == 8 && !memcmp(got, kRange, 8);
    off = x ? ExeOffset(x, 0x833124 - 0x400000u) : -1;
    checked &= off >= 0 && !fseek(x, off, SEEK_SET) && fread(got, 1, 28, x) == 28 && !memcmp(got, "Invalid function pointer: %p", 28);
    if (x) fclose(x);
    Check(match, "a real client: every place the world map's switches use holds the bytes they were made for, in its WoW.exe");
    Check(checked, "a real client: its Lua checks each C function's address against its own code before calling it (so the switches speak through a CVar, never a function of the DLL's)");
}

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: test_switch <scratch dir>\n"); return 2; }
    g_root = argv[1];
    g_client = argc > 2 ? argv[2] : NULL;
    g_todForce = 720;                                    /* noon (the plain day) unless a test says otherwise */

    Fresh("nothing");
    Start();
    Check(Gone(ACTIVE) && !On(), "no archives: nothing put in place, switch off");

    Fresh("first");
    Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(Is(ACTIVE, "AAAA") && Is(MARK, "21x9 4") && On(), "5120x2160: the ultrawide set put in place, switch on");
    memcpy(shim_module + PATCH_RVA, kStock, sizeof kStock);
    Start();
    Check(Is(ACTIVE, "AAAA") && On() && LogSays("are in place"), "next start: already in place, switch on again");
    Put("WTF/Config.wtf", RES("1920x1080"));
    memcpy(shim_module + PATCH_RVA, kStock, sizeof kStock);
    Start();
    Check(Gone(ACTIVE) && Gone(MARK) && !On(), "changed to 1920x1080 with no 16x9 set: ours removed, switch off");

    Fresh("foreign");
    Put(UW, "AAAA"); Put(ACTIVE, "FOREIGN"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(Is(ACTIVE, "FOREIGN") && !On() && LogSays("not ours"), "someone else's patch-~.MPQ: left alone, switch off");

    Fresh("stale-marker");
    Put(UW, "AAAA"); Put(ACTIVE, "FOREIGNXX"); Put(MARK, "21x9 4"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(Is(ACTIVE, "FOREIGNXX") && !On(), "a file that replaced ours (other size than recorded): left alone");

    /* an earlier one of ours whose ownership record is gone (deleted by hand or by a launcher's repair; the adversarial
     * pass, 8 Oct night): known by what is in it (CinderLoad\bars.txt since 0.1.5), or by its exact size for the five
     * releases before; replaced with the right pack where this screen has one, never removed where it has none */
    {
        static const char *const nm[] = { "CinderLoad\\bars.txt", "Interface\\Glues\\LoadingScreens\\LoadScreenDeadmines.blp" };
        static const char *const dt[] = { "default arcane\r\n", "BLP2old" };
        static const unsigned ln[] = { 16, 7 };
        CryptInit();                                    /* (the test archives' names hashed as the game hashes them) */
        Fresh("lost-marker");
        Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
        MakeMpq(ACTIVE, 2, nm, dt, ln);
        Start();
        int lm = Is(ACTIVE, "AAAA") && Is(MARK, "21x9 4") && On() && LogSays("ownership record is gone");
        Fresh("lost-marker-no-shape");
        Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("1024x768"));
        MakeMpq(ACTIVE, 2, nm, dt, ln);
        const char *before = Get(ACTIVE); char keep[64]; memcpy(keep, before, sizeof keep);
        Start();
        lm &= Get(ACTIVE) && !memcmp(Get(ACTIVE), keep, sizeof keep) && !On();   /* no shape here: left alone */
        Fresh("lost-marker-0.1.0");                     /* a 0.1.0 pack: the game's own names only, its exact size */
        Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
        MakeMpq(ACTIVE, 1, nm + 1, dt + 1, ln + 1);
        { char p[700]; snprintf(p, sizeof p, "%s%s", shim_gamedir, ACTIVE); lm &= !truncate(p, 56937150); }
        Start();
        lm &= Is(ACTIVE, "AAAA") && On();
        Fresh("lost-marker-other-size");                /* the same, one byte off every release: someone else's */
        Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
        MakeMpq(ACTIVE, 1, nm + 1, dt + 1, ln + 1);
        { char p[700]; snprintf(p, sizeof p, "%s%s", shim_gamedir, ACTIVE); lm &= !truncate(p, 56937151); }
        Start();
        lm &= !Is(ACTIVE, "AAAA") && !On() && LogSays("not ours");
        Check(lm, "an earlier pack of ours with its record gone: known by its bars.txt or a release's exact size, replaced where this screen has a shape, left alone where it has none; one byte off: someone else's");
    }

    Fresh("update");
    Put(UW, "BBBBBB"); Put(ACTIVE, "AAAA"); Put(MARK, "21x9 4"); Put("WTF/Config.wtf", RES("3440x1440"));
    Start();
    Check(Is(ACTIVE, "BBBBBB") && Is(MARK, "21x9 6") && On(), "a newer pack: ours replaced by the new one");

    Fresh("monitor");
    Put(UW, "AAAA");
    Start();
    Check(Is(ACTIVE, "AAAA") && On() && LogSays("the monitor"), "no Config.wtf: the monitor's 3440x1440 picks 21x9");

    Fresh("other-exe");
    Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
    shim_module[PATCH_RVA + 1] = 0x11;
    Start();
    Check(Gone(ACTIVE) && shim_module[PATCH_RVA] == 0xE8 && shim_module[PATCH_RVA + 1] == 0x11 &&
          LogSays("not the client this was made for"), "another WoW.exe: nothing put in place, its code never changed");

    Fresh("already-wide");
    Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
    memcpy(shim_module + PATCH_RVA, kWide, sizeof kWide);
    Start();
    Check(On() && LogSays("already on"), "an exe already changed for wide screens: left as it is");

    Fresh("two-shapes");
    Put(UW, "AAAA"); Put(HD, "HHHHH"); Put("WTF/Config.wtf", RES("2560x1440"));
    Start();
    Check(Is(ACTIVE, "HHHHH") && Is(MARK, "16x9 5") && On(), "2560x1440 with both sets: the 16x9 one");
    Put("WTF/Config.wtf", RES("3440x1440"));
    Start();
    Check(Is(ACTIVE, "AAAA") && Is(MARK, "21x9 4") && On(), "then 3440x1440: swapped to the 21x9 one");

    Fresh("ours-by-content");
    Put(UW, "AAAA"); Put(HD, "HHHHH"); Put(ACTIVE, "HHHHH"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(Is(ACTIVE, "AAAA") && On(), "ours with no marker (same bytes as one of our sets): replaced");

    Fresh("four-three");
    Put(UW, "AAAA"); Put(ACTIVE, "AAAA"); Put(MARK, "21x9 4"); Put("WTF/Config.wtf", RES("1024x768"));
    Start();
    Check(Gone(ACTIVE) && !On(), "4:3: no set for it, ours removed, the game's own screens as it shows them");

    Fresh("bar");
    Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(On() && BarWide() && LogSays("bar on"), "our set in place: the bar spans the whole width, twice as tall");
    Start();
    Check(BarWide() && LogSays("bar already full width"), "next start: the bar already full width, left as it is");

    Fresh("bar-no-set");
    Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(!On() && BarStock(), "no set for this screen: the bar keeps the game's own size");

    Fresh("bar-foreign");
    Put(UW, "AAAA"); Put(ACTIVE, "FOREIGN"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(!On() && BarStock(), "someone else's patch-~.MPQ: the bar keeps the game's own size too");

    Fresh("bar-other-exe");
    Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
    shim_module[BAR_BORDER_RVA + 8] = 0x11;
    Start();
    Check(!On() && Gone(ACTIVE) && shim_module[BAR_BORDER_RVA + 8] == 0x11 && Same(shim_module + BAR_FILL_RVA, kFillStock, 16) &&
          LogSays("not the client this was made for"), "a bar size table with other numbers: nothing put in place, never changed");

    Fresh("from-0.1.2");
    Put(UW, "AAAA"); Put(ACTIVE, "AAAA"); Put(OLDMARK, "21x9 4"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(On() && Is(MARK, "21x9 4") && Gone(OLDMARK), "0.1.2's record in Data/CinderLoad: moved beside the archive");

    Fresh("from-0.1.2-update");
    Put(UW, "BBBBBB"); Put(ACTIVE, "AAAA"); Put(OLDMARK, "21x9 4"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(On() && Is(ACTIVE, "BBBBBB") && Is(MARK, "21x9 6") && Gone(OLDMARK), "0.1.2's record still makes ours known: updated");

    Fresh("packs-removed");
    Put(ACTIVE, "AAAA"); Put(MARK, "21x9 4"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    Check(!On() && Gone(ACTIVE) && Gone(MARK), "packs removed, DLL still there: ours taken out, the game's own");

    Fresh("no-temp-left");
    Put(UW, "AAAA"); Put("WTF/Config.wtf", RES("5120x2160"));
    Start();
    {
        char t[64];
        snprintf(t, sizeof t, "Data/CinderLoad/pending-%lu.tmp", (unsigned long)getpid());
        Check(On() && Is(ACTIVE, "AAAA") && Gone(t), "put in place through its own temporary file, none left behind");
    }

    Fresh("copy-fails");
    Put(UW, "AAAA"); Put(HD, "HHHHH"); Put(ACTIVE, "HHHHH"); Put(MARK, "16x9 5"); Put("WTF/Config.wtf", RES("5120x2160"));
    {
        char p[700];
        snprintf(p, sizeof p, "%s%s", shim_gamedir, UW);
        chmod(p, 0);
        Start();
        chmod(p, 0644);
    }
    Check(!On() && Gone(ACTIVE) && Gone(MARK) && LogSays("could not put"),
          "the copy fails: the old shape's screens taken out too, never left squeezed");

    FireTests();
    FireBarTests();
    LiveTests();
    ViewTests();
    TestStyle();
    TestStyleFiles();
    TestSwitches();
    TestSwitchesPump();
    TestSwitchesLua();
    TestSwitchesPath();
    if (argc > 2) {
        FireBarRealClient(argv[2]);
        FireRealClient(argv[2]);
        LuaRealClient(argv[2]);
    }

    TestShips();
    TestWakes();
    TestSky();
    TestClouds();
    TestTimeOfDay();
    TestViewDefault();
    TestVanillaBar();
    TestParchment();
    TestZoneRule();
    TestCut();
    TestLand();
    TestWords();
    TestGlide();
    TestModeTurn();
    TestExplore();
    if (fails) printf("%d of %d checks FAILED\n", fails, checks); else printf("all %d checks passed\n", checks);
    return fails != 0;
}
