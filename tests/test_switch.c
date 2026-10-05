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
#include "../dll/cinderload.c"
static const float kWholeUV[8] = { 0, 1, 1, 1, 0, 0, 1, 0 };   /* the whole picture: corners 0,0 1,0 0,1 1,1 */
#include <stdio.h>
#include <sys/stat.h>

char shim_gamedir[512];
unsigned char shim_module[SHIM_MODULE_SIZE];
int shim_threads;
DWORD shim_ticks;
int shim_vq;
int shim_screen_w = 3440, shim_screen_h = 1440;

static int fails, checks;
static void Check(int cond, const char *what) { checks++; if (!cond) { printf("FAIL: %s\n", what); fails++; } }

static const char *g_root;
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

/* A loop that is a constant: what a mixer test hears is then only the levels, the pan and the fade. */
static float g_dc[64];
static void DcMix(FireMix *m, int rate, int which, float value)
{
    memset(m, 0, sizeof *m);
    FireSetRate(m, rate);
    for (int i = 0; i < 64; i++) g_dc[i] = value;
    m->loop[which] = g_dc;
    m->len[which] = 64;
}
static short g_pcm[2 * 48000];
/* frames rendered at one gate and fill; the last frame's left and right */
static void Run(FireMix *m, int gate, float fill, int frames, int *l, int *r)
{
    while (frames > 0) {
        int n = frames < 48000 ? frames : 48000;
        FireRender(m, gate, fill, g_pcm, n);
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
    float l, r;
    int il, ir;

    /* the pan curve: equal power, 80/20 by loudness at both ends */
    Check(Near(FirePan(0), 0.295, 1e-6) && Near(FirePan(1), 0.705, 1e-6) && Near(FirePan(0.5f), 0.5, 1e-6) &&
          Near(FirePan(-1), 0.295, 1e-6) && Near(FirePan(2), 0.705, 1e-6), "pan: 0.295 at an empty bar, 0.705 at a full one");
    int power = 1;
    for (int i = 0; i <= 20; i++) {
        FireGains(i / 20.0f, &l, &r);
        power &= Near(l * l + r * r, 1, 1e-5);
    }
    Check(power, "pan: equal power, left^2 + right^2 = 1 all the way along");
    FireGains(0, &l, &r);
    int ends = Near(l * l, 0.80, 0.002);
    FireGains(1, &l, &r);
    ends &= Near(r * r, 0.80, 0.002);
    FireGains(0.5f, &l, &r);
    Check(ends && Near(l, r, 1e-6), "pan: 80% of the loudness left at the start, 80% right at the end, even halfway");
    double worst = 0;
    for (int i = 0; i <= 1000; i++) {
        double x = i * (M_PI / 2) / 1000;
        worst = fmax(worst, fmax(fabs(FireSin((float)x) - sin(x)), fabs(FireCos((float)x) - cos(x))));
    }
    Check(worst < 1e-6 && Near(FireExp(-0.001), exp(-0.001), 1e-15) && Near(FireExp(-3), exp(-3), 1e-12),
          "the arithmetic without a C runtime: sin, cos and exp as the library's");

    /* the glide: a jump in the bar moves the sound over about a third of a second, never at once */
    FireMix m;
    DcMix(&m, 1000, 0, 0.1f);
    Run(&m, 1, 0, 10, NULL, NULL);
    Run(&m, 1, 1, 1, NULL, NULL);
    float first = m.front;
    Run(&m, 1, 1, 299, NULL, NULL);
    float at300 = m.front;
    Run(&m, 1, 1, 50, NULL, NULL);
    Check(first < 0.005f && Near(at300, 1 - exp(-1), 0.01) && Near(m.front, 1 - exp(-0.35 / 0.3), 0.01),
          "glide: one frame after a jump the front has hardly moved; 63% at 0.3 s, 69% at 0.35 s");
    DcMix(&m, 1000, 0, 0.1f);
    Run(&m, 1, 0, 1000, &il, &ir);
    double startRight = (double)ir * ir / ((double)il * il + (double)ir * ir);
    Run(&m, 1, 1, 3000, &il, &ir);
    double endRight = (double)ir * ir / ((double)il * il + (double)ir * ir);
    Check(Near(startRight, 0.2, 0.002) && Near(endRight, 0.8, 0.002), "glide: the sound itself goes from 20% right to 80% right");
    DcMix(&m, 1000, 0, 0.1f);
    Run(&m, 1, 0.7f, 1, NULL, NULL);
    Check(Near(m.front, 0.7, 1e-6), "glide: a screen starts where its bar is, with no sweep across");

    /* the crackle: loud while the fill moves, soft while it stalls */
    Check(Near(FireBedLevel(0), 0.85, 1e-6) && Near(FireBedLevel(1 / 3.0f), 1.15, 1e-5) && Near(FireBedLevel(1), 1.15, 1e-6),
          "bed: 0.85 at an empty bar, 1.15 from a third on");
    DcMix(&m, 1000, 4, 0.1f);                               /* UndeadFireLarge alone: the crackle */
    Run(&m, 1, 0, 500, NULL, NULL);
    for (int step = 0; step < 100; step++) Run(&m, 1, step * 0.005f, 20, NULL, NULL);   /* 0.25 a second, in 20 ms steps */
    float moving = FireCrackleLevel(m.speed);
    Run(&m, 1, 0.5f, 1, &il, &ir);
    double loudMoving = sqrt((double)il * il + (double)ir * ir);
    Run(&m, 1, 0.5f, 2000, &il, &ir);
    float stalled = FireCrackleLevel(m.speed);
    double loudStalled = sqrt((double)il * il + (double)ir * ir);
    Check(Near(moving, 1, 0.1) && stalled < 0.02f && loudMoving > 20 * loudStalled,
          "crackle: at a quarter of the bar a second it is at 1; two seconds of stall and it has all but gone");
    DcMix(&m, 1000, 4, 0.1f);
    Run(&m, 1, 0, 100, NULL, NULL);
    float peak = 0;
    for (int i = 0; i < 2000; i++) {
        Run(&m, 1, 1, 1, NULL, NULL);
        if (m.speed > peak) peak = m.speed;
    }
    Check(peak <= FIRE_MOVE_CAP + 1e-6f && FireCrackleLevel(10) == FIRE_MOVE_MAX, "crackle: a leap of the whole bar is held to its cap");

    /* the fades */
    DcMix(&m, 1000, 0, 0.1f);
    Run(&m, 1, 0.5f, 200, NULL, NULL);
    float half = m.env;
    Run(&m, 1, 0.5f, 200, NULL, NULL);
    float full = m.env;
    Run(&m, 0, 0.5f, 200, NULL, NULL);
    float out = m.env;
    int going = FireRender(&m, 0, 0.5f, g_pcm, 199);
    int quiet = !FireRender(&m, 0, 0.5f, g_pcm, 10) && g_pcm[18] == 0 && g_pcm[19] == 0;
    Check(Near(half, 0.5, 0.01) && Near(full, 1, 1e-6) && Near(out, 0.5, 0.01) && going && quiet,
          "fades: in over 0.4 s as a screen comes up, out over 0.4 s when it goes, then silence");

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

    /* a class's own sound (his rule, 4 Oct): its table read carefully; a bed rising with the fill, a swell silent until
     * its start and whole by its end, the stinger once as the fill passes its mark, centred, the fade the fire's */
    {
        static const char tab[] = "# druid\nlevel 0.8632\nloop bed 0.950 0.000 1.000 Sound\\Ambience\\ZoneAmbience\\EnchangedForestDay.wav\n"
            "loop swell 0.550 0.150 0.600 Sound\\Doodad\\MoonWellLightLoop.wav\r\nloop swell 0.700 0.450 0.950 Sound\\Creature\\Whisp\\WispLoop01.wav\n"
            "loop motion 0.350 0.000 0.000 Sound\\Spells\\Missile\\NAtureMissileLoop.wav\nloop hum 1 0 1 Sound\\x.wav\n"
            "loop bed 9 0 1 Sound\\y.wav\nstinger 0.900 0.970 Sound\\Spells\\Tranquility.wav\n";
        static SndSpec sp;
        int ok = SndParse(&sp, tab, sizeof tab - 1) && sp.n == 4 && sp.sting && Near(sp.level, 0.8632, 1e-4) &&
                 sp.l[0].kind == SND_BED && sp.l[1].kind == SND_SWELL && sp.l[3].kind == SND_MOTION && Near(sp.l[1].from, 0.15, 1e-6) &&
                 Near(sp.l[2].to, 0.95, 1e-6) && !strcmp(sp.l[1].path, "Sound\\Doodad\\MoonWellLightLoop.wav") &&
                 !strcmp(sp.stingPath, "Sound\\Spells\\Tranquility.wav") && Near(sp.stingAt, 0.97, 1e-6);
        ok &= !SndParse(&sp, "loop bed 1 0 1 Sound\\a.wav\n", 27);                      /* no level: none */
        Check(ok, "class sound: its table read (CRLF, comments; an unknown kind and a gain over 4 skipped); no level, no sound");

        static float one[100], sting[50];
        for (int i = 0; i < 100; i++) one[i] = 1;
        for (int i = 0; i < 50; i++) sting[i] = 1;
        static SndSet st;
        memset(&st, 0, sizeof st);
        SndParse(&sp, tab, sizeof tab - 1);
        for (int i = 0; i < 4; i++) { st.loop[i] = one; st.len[i] = 100; }
        st.n = 4; st.sting = sting; st.stingLen = 50;
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
        /* at 0: the bed at 0.55 of 0.95, nothing else; at 0.1 (once still again): the bed a little up, the swells silent */
        float want0 = 0.95f * 0.55f * 0.8632f * 0.70710678f;
        float bed10 = 0.55f + 0.45f * SndSmooth(0, 1, 0.1f), want10 = 0.95f * bed10 * 0.8632f * 0.70710678f;
        ok = centred && Near(at0, want0, 2e-4) && Near(at10, want10, 2e-3) && st.stingState == 0;
        SndRender(&mx, &st, &sp, 1, 0.97f, pcm, 1);                                 /* the mark: the stinger starts */
        int playing = st.stingState == 1;
        SndRender(&mx, &st, &sp, 1, 0.98f, pcm, 60);
        int played = st.stingState == 2;
        SndRender(&mx, &st, &sp, 1, 0.99f, pcm, 60);
        ok &= playing && played && st.stingState == 2;                              /* once */
        SndReset(&st);
        ok &= st.stingState == 0 && st.pos[0] == 0;
        Check(ok, "class sound: the bed at 0.55 rising, the swells silent before their start, the stinger once at its mark, centred");
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
    Check(shim_threads == 1 && LogSays("fire sound on") && On() && BarWide(), "the client's bytes all there: the fire's thread starts");
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
    DcMix(&g_fire.mix, 8000, 0, 0.1f);
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
    Check(g_fire.gate == 0 && g_fire.stream && !g_calls[0] && LogSays("the fire burned for") && LogSays("the bar reached 60%"),
          "the screen goes: the fire fades out, and the log says how long it burned");
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
    g_fire.base = shim_module;
    g_fire.channel = -1;
    Client(0x1234, 0.1f, 1);
    FireStep(shim_ticks += 20);
    Client(0x1234, 0.2f, 1);
    FireStep(shim_ticks += 20);
    Check(!g_calls[0] && g_fire.built == -1 && LogSays("none of its 6 loops"),
          "the client's fire sounds are not to be had: logged once, no FMOD call, the screens as ever");
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
    Check(On() && BarWide() && shim_threads == 1 && LogSays("fire sound on"), "the screens, the bar's size and the fire sound as ever");
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
                 LogSays("fire sound on");
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

    /* the garland (his druid bar, 4 Oct): "style <family> garland" in bars.txt; with its backing in the pack, the class
     * that takes that family gets the tall bar (the banner's mirror), its fill laid once and never cooling, the
     * backing and the wisps; any other class the log's size again */
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
        int gl = fd >= 0 && ff >= 0 && g_bars.family[fd].garland && !g_bars.family[ff].garland && g_barSizeOurs &&
                 g_bars.skipped == 1 && LogSays("fire bar: druid grows as a garland") && LogSays("(its backing in the pack; 6 wisp sprites; 4 kinds of flower opening in 8 frames)");
        Check(gl, "garland: bars.txt's style line read (a style other than garland skipped), its backing found, its wisps' table read (bad lines left out)");

        unsigned char *fill = shim_module + BAR_FILL_RVA, *border = shim_module + BAR_BORDER_RVA;
        Poke(CHAR_COUNT_VA, 2); Poke(CHAR_LIST_VA, 0x8A0000); Poke(CHAR_PICKED_VA, 0);
        shim_module[0x8A0000 - 0x400000 + 0 * 0x120 + 0x101] = 11;       /* a druid, a hunter */
        shim_module[0x8A0000 - 0x400000 + 1 * 0x120 + 0x101] = 3;
        const char *p = ForMap(36);
        gl = !strcmp(p, TEX "Loading-BarFill-druid") && g_barGarland == fd + 1 && Same(fill, kBarTall, 16) && Same(border, kBarTall, 16);
        /* its fill: laid once across the bar (u = how far along), full brightness behind the soft front */
        float vf[12] = { 0, 0, 0, 0.5f, 0, 0, 0, 0.18f, 0, 0.5f, 0.18f, 0 };   /* the game's front at its 50% */
        { float h = 0.5f; unsigned u; memcpy(&u, &h, 4); Poke(0x882BE4, u); }
        g_dispNew = 1;
        for (int i = 0; i < 40; i++) { shim_ticks += 20; RevealFill(vf); }
        int flat = g_fillCount >= 4;
        for (int k = 0; k + 1 < g_fillCount / 2; k++) {
            float x = g_fillVerts[6 * k];
            flat &= fabsf(g_fillUV[4 * k] - x) < 1e-4f && (g_fillColor[2 * k] & 0xFFFFFFu) == 0xFFFFFFu;
        }
        gl &= flat;
        Poke(CHAR_PICKED_VA, 1);                                           /* the hunter: fel, a fire, the log's size */
        p = ForMap(36);
        gl &= !strcmp(p, TEX "Loading-BarFill-fel") && g_barGarland == 0 && Same(fill, kBorderWide, 16) && Same(border, kBorderWide, 16);
        Check(gl, "garland: the druid's bar tall (the banner's mirror), its fill laid once and never cooling; the hunter's fire the log's size again");

        /* the arch's curve: full height at the edges, a third of it at the centre, the banner's own */
        int cv = fabsf(GarlandThick(0) - ARCH_H) < 1e-3f && fabsf(GarlandThick(1) - ARCH_H) < 1e-3f &&
                 fabsf(GarlandThick(0.5f) - ARCH_H / 3) < 1e-3f && GarlandThick(0.25f) > GarlandThick(0.4f);
        /* the wisps: spawned from just behind the moving front, inside the arch; the front's own wisp, trail and aura */
        g_scrW = 5120; g_scrH = 2160;
        memset(g_wisp, 0, sizeof g_wisp); g_wispFront = -1; g_wispCredit = 0;
        float fr = 0.3f;
        for (int i = 0; i < 30; i++) { fr += 0.004f; WispsBuild(fd, fr, i * 0.033f, 0.033f); }
        int live = 0, inside = 1;
        for (int i = 0; i < WISP_MAX; i++) if (g_wisp[i].live) {
            live++;
            inside &= g_wisp[i].x <= fr + 1e-4f && g_wisp[i].x >= 0 && g_wisp[i].y >= 0 && g_wisp[i].y <= GarlandThick(g_wisp[i].x) + 0.06f;
        }
        cv &= live >= 8 && inside && g_wsQuads <= live + 3 + WISP_ZIPS * WISP_ZIP_TAIL && g_wsQuads >= 3 + WISP_ZIPS * WISP_ZIP_TAIL;   /* a twinkle at its dimmest draws nothing */
        /* the zipping ones stay near the front and inside the arch */
        for (int q = live; q < g_wsQuads; q++) {
            int c0 = q ? q * 6 : 0;
            float zx = (g_wsPos[c0 * 3] + g_wsPos[(c0 + 1) * 3]) / 2, zy = (g_wsPos[c0 * 3 + 1] + g_wsPos[(c0 + 2) * 3 + 1]) / 2;
            cv &= fabsf(zx - fr) < 0.05f && zy > -0.02f && zy < GarlandThick(fr) + 0.04f;
        }
        WispsBuild(fd, 1.0f, 2.0f, 0.033f);                                 /* the bar full: no front's own three */
        int after = 0;
        for (int i = 0; i < WISP_MAX; i++) after += g_wisp[i].live;
        cv &= g_wsQuads <= after;
        /* his ask: sparkles keep coming across the whole grown bar, full or not */
        memset(g_wisp, 0, sizeof g_wisp);
        for (int i = 0; i < 60; i++) WispsBuild(fd, 1.0f, 3.0f + i * 0.033f, 0.033f);
        int left = 0, right = 0;
        for (int i = 0; i < WISP_MAX; i++) if (g_wisp[i].live) { if (g_wisp[i].x < 0.5f) left++; else right++; }
        cv &= left >= 3 && right >= 3;
        Check(cv, "garland: the arch the banner's curve; wisps rising from just behind the front, inside it, the front's own three on top; none new once full");
        /* the blooms (his v2 note): the same places every screen, inside the arch; each opens once the front reaches it,
         * bud to full bloom over BLOOM_OPEN_S, growing as it opens; one the front leapt past is open at once; a new
         * screen closes them all; a sheet that does not fit makes none */
        {
            BloomsBuild(fd, 0.0f, 100000, 1);
            int bm = g_bmQuads == 0 && g_bloomTab[fd].cell == 128 && g_bloomTab[fd].types == 4 && g_bloomTab[fd].frames == 8;
            int in = 1, kinds[4] = { 0 };
            for (int i = 0; i < BLOOM_MAX; i++) {
                in &= g_bloom[i].y > 0 && g_bloom[i].y < GarlandThick(g_bloom[i].x) && g_bloom[i].x > 0 && g_bloom[i].x < 1;
                float th = GarlandThick(g_bloom[i].x);                  /* his v5 note: 10-15% lower than v4's places */
                in &= g_bloom[i].y <= th * 0.8f - 0.10f * (th + 85.0f / 2160.0f) + 1e-6f && g_bloom[i].y >= th * 0.1f - 1e-6f;
                if (g_bloom[i].type >= 0 && g_bloom[i].type < 4) kinds[g_bloom[i].type]++;
            }
            bm &= in && kinds[0] > kinds[1] && kinds[1] && kinds[2] && kinds[3];          /* orange most, every kind used */
            BloomsBuild(fd, 0.30f, 100000, 1);                                            /* the front at 30%: a leap */
            int open = 0;
            for (int i = 0; i < BLOOM_MAX; i++) open += g_bloom[i].on;
            bm &= open > 10 && open < 25;
            for (int i = 0; i < BLOOM_MAX; i++) bm &= g_bloom[i].on == (g_bloom[i].x <= 0.30f);
            for (int i = 0; i < BLOOM_MAX; i++)                                          /* far behind: open already */
                if (g_bloom[i].on) bm &= g_bloom[i].x < 0.05f ? g_bloom[i].t0 == 100000 - 1000
                                                              : (g_bloom[i].x > 0.29f ? g_bloom[i].t0 > 100000 - 100 : 1);
            BloomsBuild(fd, 0.32f, 100050, 1);                                            /* just reached: a bud, small */
            int j = -1;
            for (int i = 0; i < BLOOM_MAX && j < 0; i++) if (g_bloom[i].x > 0.30f && g_bloom[i].x <= 0.32f) j = i;
            if (j >= 0) {
                BloomsBuild(fd, 0.32f, g_bloom[j].t0, 1);
                int q = 0;
                for (int i = 0; i < j; i++) q += g_bloom[i].on;                           /* bloom j's quad */
                float u0 = g_bmUV[(q ? q * 6 - 2 + 2 : 0) * 2];
                float h0 = g_bmPos[((q ? q * 6 : 0) + 2) * 3 + 1] - g_bmPos[(q ? q * 6 : 0) * 3 + 1];
                BloomsBuild(fd, 0.32f, g_bloom[j].t0 + 1000, 1);                          /* a second on: fully open */
                float u1 = g_bmUV[(q ? q * 6 - 2 + 2 : 0) * 2];
                float h1 = g_bmPos[((q ? q * 6 : 0) + 2) * 3 + 1] - g_bmPos[(q ? q * 6 : 0) * 3 + 1];
                bm &= u0 == 0 && fabsf(u1 - 7 * 128 / 1024.0f) < 1e-6f && h1 > h0 * 1.7f;
            } else bm = 0;
            BloomsBuild(fd, 0.0f, 101000, 2);                                              /* a new screen */
            int still = 0;
            for (int i = 0; i < BLOOM_MAX; i++) still += g_bloom[i].on;
            bm &= still == 0;
            BloomsParse(fd, "cell 256\ntypes 5\nframes 8\n", 26);                         /* 5 x 256 > 1024: none */
            bm &= g_bloomTab[fd].types == 0;
            Check(bm, "garland: blooms inside the arch, mostly orange; open once the front reaches them, bud to bloom and growing; a leap opens them at once; a new screen closes them; a sheet too big none");
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
    if (x) fclose(x);
    Check(match, "a real client: all eight places hold the bytes the fire bar and the drift expect, in its WoW.exe");
}

/* The mock's timeline (render_frames.py KEYS) through the sound built from a real client's files, polled as the game
 * would be: the fill changes every 20 ms. Written as 16-bit stereo for listening; a few checks on the way. */
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
    char from[1024], to[1024];
    const char *arch[] = { "patch.MPQ", "sound.MPQ" };
    for (int i = 0; i < 2; i++) {
        snprintf(from, sizeof from, "%s/Data/%s", client, arch[i]);
        snprintf(to, sizeof to, "%sData/%s", shim_gamedir, arch[i]);
        if (symlink(from, to)) { printf("FAIL: could not link %s\n", from); fails++; return; }
    }
    memset(&g_fire, 0, sizeof g_fire);
    int built = FireBuild(44100), all = 1;
    for (int i = 0; i < FIRE_LOOPS; i++) all &= g_fire.mix.loop[i] != NULL;
    Check(built && all && LogSays("6 of 6 loops"), "a real client: all six loops made from its own files");
    if (!all) return;
    FireReset(&g_fire.mix);
    int total = (int)(12.5 * 44100), pollFrames = 882, done = 0;
    static short pcm[2 * 12 * 44100 + 2 * 44100];
    short peak = 0;
    for (int poll = 0; done < total; poll++) {
        float t = poll * 0.02f, fill = 0;
        for (int k = 0; k + 1 < 10; k++)
            if (t >= keys[k][0] && t <= keys[k + 1][0]) fill = keys[k][1] + (keys[k + 1][1] - keys[k][1]) * (t - keys[k][0]) / (keys[k + 1][0] - keys[k][0]);
        int n = total - done < pollFrames ? total - done : pollFrames;
        FireRender(&g_fire.mix, t < 11.6f, fill, pcm + 2 * done, n);
        done += n;
    }
    for (int i = 0; i < 2 * total; i++) if (abs(pcm[i]) > peak) peak = (short)abs(pcm[i]);
    Check(peak > 8000 && peak < 32767, "a real client: the timeline is loud as the mock and never clips");
    char path[1024];
    snprintf(path, sizeof path, "%s/fire-timeline.wav", g_root);
    static unsigned char out[sizeof pcm + 64];
    size_t len = Wav(out, 1, 2, 16, 44100, pcm, total);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(out, 1, len, f); fclose(f); printf("wrote %s (peak %d)\n", path, peak); }

    /* the druid's own sound (his rule, 4 Oct), from the same client's files, over the same timeline: it builds */
    static const char tab[] = "level 0.8632\nloop bed 0.950 0.000 1.000 Sound\\Ambience\\ZoneAmbience\\EnchangedForestDay.wav\n"
        "loop swell 0.550 0.150 0.600 Sound\\Doodad\\MoonWellLightLoop.wav\nloop swell 0.700 0.450 0.950 Sound\\Creature\\Whisp\\WispLoop01.wav\n"
        "loop motion 0.350 0.000 0.000 Sound\\Spells\\Missile\\NAtureMissileLoop.wav\nstinger 0.900 0.970 Sound\\Spells\\Tranquility.wav\n";
    memset(g_sndHas, 0, sizeof g_sndHas);
    g_sndHas[0] = SndParse(&g_sndSpec[0], tab, sizeof tab - 1);
    SndBuildAll(44100);
    int built4 = g_snd[0].n == 4 && g_snd[0].sting && LogSays("4 of 4 loops, and its stinger");
    Check(built4, "a real client: the druid's four loops and its stinger made from its own files");
    if (!built4) return;
    FireReset(&g_fire.mix);
    SndReset(&g_snd[0]);
    done = 0;
    for (int poll = 0; done < total; poll++) {
        float t = poll * 0.02f, fill = 0;
        for (int k = 0; k + 1 < 10; k++)
            if (t >= keys[k][0] && t <= keys[k + 1][0]) fill = keys[k][1] + (keys[k + 1][1] - keys[k][1]) * (t - keys[k][0]) / (keys[k + 1][0] - keys[k][0]);
        int n = total - done < pollFrames ? total - done : pollFrames;
        SndRender(&g_fire.mix, &g_snd[0], &g_sndSpec[0], t < 11.6f, fill, pcm + 2 * done, n);
        done += n;
    }
    double early = 0, late = 0;
    int centred = 1;
    peak = 0;
    for (int i = 1 * 44100; i < 3 * 44100; i++) early += (double)pcm[2 * i] * pcm[2 * i];
    for (int i = 9 * 44100; i < 11 * 44100; i++) late += (double)pcm[2 * i] * pcm[2 * i];
    for (int i = 0; i < total; i++) { centred &= pcm[2 * i] == pcm[2 * i + 1]; if (abs(pcm[2 * i]) > peak) peak = (short)abs(pcm[2 * i]); }
    Check(late > early * 20 && centred && peak < 32767 && g_snd[0].stingState >= 1,
          "a real client: the druid's sound builds (its last seconds far louder than its first), centred, never clips, Tranquility begun");
    printf("druid: seconds 9-11 are %.1f times seconds 1-3 in power\n", early > 0 ? late / early : 0);
    snprintf(path, sizeof path, "%s/druid-timeline.wav", g_root);
    len = Wav(out, 1, 2, 16, 44100, pcm, total);
    f = fopen(path, "wb");
    if (f) { fwrite(out, 1, len, f); fclose(f); printf("wrote %s (peak %d)\n", path, peak); }
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
    if (reveal & 8192) { names[n] = "Interface\\Glues\\LoadingScreens\\CinderArch-druid.blp"; data[n++] = "BLP2arch"; }
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

    /* the class banner (his design, 4 Oct): one per class the pack carries, the full width, ARCH_H tall at the top */
    {
        Fresh("banner");
        PackLive(1 | 4 | 8 | 4096 | 8192, 0);
        Start();
        char path[80];
        int bn = g_archOn && g_archHas[11] && !g_archHas[3] && !g_archHas[2] && LogSays("banner: the class banners in the pack: druid") &&
                 ArchPath(11, path) && !strcmp(path, "Interface\\Glues\\LoadingScreens\\CinderArch-druid") && !ArchPath(6, path) &&
                 !ArchPath(0, path) && !ArchPath(12, path);
        ArchBuild();
        bn &= g_archPos[0] == 0 && g_archPos[1] == 1 - ARCH_H && g_archPos[3] == 1 && g_archPos[3 * 3 + 1] == 1 &&
              fabsf(g_archUV[1] - 311.0f / 512.0f) < 1e-6f && g_archUV[3 * 2 + 1] == 0 && g_archCol[0] == 0xFFFFFFFFu;
        Check(bn, "the class banner: found per class in the pack (druid here), its path, one quad the full width and ARCH_H tall at the top");
        Fresh("banner-none");
        PackLive(1 | 4 | 8 | 4096, 0);
        Start();
        Check(!g_archOn && !LogSays("banner:"), "no banner in the pack: none drawn, nothing said");
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
        g_scrW = 5120; g_scrH = 2160;                                 /* his 21:9: LOGO_H tall, its own shape */
        LogoBuild();
        float w = g_logoPos[3] - g_logoPos[0], h = g_logoPos[7] - g_logoPos[1];
        lg &= fabsf(h - LOGO_H) < 1e-5f && fabsf(w * 5120 / (h * 2160) - asp) < 1e-3f &&
              fabsf((g_logoPos[0] + g_logoPos[3]) / 2 - 0.5f) < 1e-6f && fabsf(g_logoPos[7] - (1 - LOGO_TOP)) < 1e-6f &&
              g_logoUV[0] == 4.0f / 1024 && g_logoUV[2] == 1020.0f / 1024 && g_logoUV[1] == 484.0f / 512 &&
              g_logoUV[5] == 4.0f / 512 && g_logoCol[0] == 0xFFFFFFFFu;
        g_scrW = 2560; g_scrH = 1600;                                 /* 16:10: narrowed to LOGO_W, still its own shape */
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
        g_scrW = 5120; g_scrH = 2160;
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

        /* the swells bend round it, the more the nearer, and not at all beyond its reach */
        static const unsigned char allSea[8 + 16] = { 'C', 'S', 'E', 'A', 4, 0, 4, 0, 255, 255, 255, 255, 255, 255, 255, 255,
                                                      255, 255, 255, 255, 255, 255, 255, 255 };
        SeaParse(allSea, sizeof allSea);
        g_scrW = 3840; g_scrH = 2160;
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
        ClassStep();
        bd &= g_class == 8 && g_classFrom == 1 && g_classEntered == 8;
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
        g_scrW = 3840; g_scrH = 2160;
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
              (kClassCode[3].bytes[3] | kClassCode[3].bytes[4] << 8) == CHAR_CLASS && 9 * (1 << kClassCode[2].bytes[5]) == CHAR_SIZE;
        Check(bd, "the badge: the class from the list (a bad byte never taken, kept when the list goes); shadow, icon, ring at the arrival; the ring alone without; off when the code moved");
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
        Poke(e + 0x38, 0); PutFloat(e + 0x44, -5000); PutFloat(e + 0x48, 10000);
        int ar = g_classSure;
        g_drift = 0; g_driftAt = 0; g_zoomScreens = 0; g_driftNew = 1;
        DWORD at = 900000;
        DriftStep(0, at);
        ar &= g_destOk && fabsf(g_destU - 0.3f) < 1e-4f && fabsf(g_destV - 0.6f) < 1e-4f && g_destMap == 0 &&
              g_destWX == -5000 && g_destWY == 10000;                    /* a login: the list's place, at once */
        /* a teleport: no list; the old place (the last screen's) is not taken, the new one is the moment it lands */
        Poke(CHAR_COUNT_VA, 0);
        PutFloat(MAP_DEST_X_VA, -5000); PutFloat(MAP_DEST_Y_VA, 10000);  /* where the last screen arrived */
        g_driftNew = 1;
        at += 20; DriftStep(0.1f, at);
        ar &= !g_destOk;
        at += 20; DriftStep(0.2f, at);
        ar &= !g_destOk;
        PutFloat(MAP_DEST_X_VA, -2500); PutFloat(MAP_DEST_Y_VA, 5000);   /* the game writes the new place */
        at += 20; DriftStep(0.2f, at);
        ar &= g_destOk && fabsf(g_destU - 0.4f) < 1e-4f && fabsf(g_destV - 0.55f) < 1e-4f;
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
        Check(ar, "the arrival: a login's from the character list at once; a teleport's only once the game writes it (or from half the bar); never 0, 0");
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
        g_scrW = 3840; g_scrH = 2160;
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
        g_scrW = 3840; g_scrH = 2160;
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
        g_scrW = 3840; g_scrH = 2160;
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
            g_scrW = 3840; g_scrH = 2160; g_mael.ok = 0;
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
        g_scrW = 3840; g_scrH = 2160; g_mael.ok = 0;
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
        g_scrW = 3840; g_scrH = 2160;                                    /* its own screen, whatever ran before */
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
    g_scrW = 3840; g_scrH = 2160;
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
    /* its frame: the world map over the whole window at the zoom's own corners, the plain bar along the bottom */
    {
        static const float uv[8] = { 0.2f, 0.7f, 0.6f, 0.7f, 0.2f, 0.3f, 0.6f, 0.3f };
        memcpy(g_bgUV, uv, sizeof uv);
        g_ovOn = 0; g_barFamNow = -1; g_barGarland = 0; g_barTallNow = 0;
        for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
        ViewBuild(1000, 500, 0.25f, 0);
        const ViewVert *v = g_vv;
        int ok = g_vbN == 1 && g_vb[0].tex == VT_MAP && g_vb[0].first == 0 && g_vb[0].count == 4 && v[0].x == 0 &&
                 v[0].y == 0 && v[0].u == 0.2f && v[0].v == 0.3f && v[2].x == 1000 && v[2].y == 500 && v[2].u == 0.6f &&
                 v[2].v == 0.7f && v[1].x == 1000 && v[1].y == 0 && v[0].c[0] == 255 && v[0].c[3] == 255;   /* no bar yet */
        /* slice 3: the bar, the pack's own family here, the log's size (kBorderWide: the bottom 9%): its fill to the bar
         * as shown, as the game's own strip, then the log over the whole width */
        g_viewTex[ViewBarSlot(-1, 0)] = g_viewTex[ViewBarSlot(-1, 1)] = 1;
        ViewBuild(1000, 500, 0.25f, 0);
        int nf = FillStrip(0, 0, 0.09f, 0, 1, 0.25f, 0, 0, g_vfVerts, g_vfUV, g_vfColor);
        ok &= g_vbN == 3 && g_vb[1].tex == ViewBarSlot(-1, 0) && g_vb[1].count == 4 * (nf / 2 - 1) && g_vb[2].tex == ViewBarSlot(-1, 1) &&
              g_vb[2].count == 4;
        const ViewVert *fq = g_vv + g_vb[1].first, *lq = fq + g_vb[1].count - 4, *bq = g_vv + g_vb[2].first;
        ok &= fq[0].x == 0 && fabsf(fq[0].y - 455) < 1e-3f && fq[0].v == 0 && fabsf(fq[3].y - 500) < 1e-3f && fq[3].v == 1 &&
              fq[3].x == fq[0].x && fq[2].x == fq[1].x && fq[1].x > fq[0].x && fabsf(fq[2].y - 500) < 1e-3f &&  /* round it */
              fabsf(lq[1].x - 250) < 1e-3f && lq[1].c[3] == 0 && fq[0].c[3] == 255 &&      /* to the front, faded in at it */
              bq[0].x == 0 && fabsf(bq[0].y - 455) < 1e-3f && bq[2].x == 1000 && bq[2].y == 500 && bq[0].u == 0 && bq[0].v == 0 &&
              bq[2].u == 1 && bq[2].v == 1;
        g_barTallNow = 1;                                                /* a garland's: the bottom 22% */
        ViewBuild(1000, 500, 1.0f, 0);
        bq = g_vv + g_vb[g_vbN - 1].first;
        ok &= fabsf(bq[0].y - 390) < 1e-3f && fabsf(g_vv[g_vb[1].first + g_vb[1].count - 4 + 1].x - 1000) < 1e-3f;
        g_barTallNow = 0;
        for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        Check(ok, "the view's frame: the world map over the whole window at the zoom's corners, top left to bottom right; slice 3's bar, the game's own fill strip to the bar as shown and the log over the whole width, at the log's size or a garland's");
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
        ok &= ViewStep(p, t, 1000, 500) && g_viewSteps == 1 && g_stepLock.held == 0 && g_vbN >= 1 && g_vb[0].tex == VT_MAP;
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
         * one of them a step closer, the bar never more than DISP_LEAD past the game's */
        DWORD stall = t;
        int moving = 1, steps = 0;
        float lastH = g_viewH, lastU = g_viewU, most = 0;
        while (t - stall < 600) {
            t += 7;
            ViewStep(p, t, 1000, 500);
            if (t - stall <= 300) moving &= g_viewH < lastH && g_viewU != lastU;
            lastH = g_viewH; lastU = g_viewU;
            if (g_viewShown > most) most = g_viewShown;
            steps++;
        }
        ok &= moving && steps > 80 && most <= p + DISP_LEAD + 1e-5f && most > p;
        t += 7;
        GameStep(0.55f, t);                                              /* the game's next frame: its own hitch, logged */
        ok &= g_hitchN == 1 && g_hitchMs[0] + g_hitchMs[1] + g_hitchMs[2] + g_hitchMs[3] + g_hitchMs[4] + g_hitchMs[5] >= 600 &&
              g_viewSteps == 1;
        /* the hold, with the view on: its numbers set under the lock, never held through its frames */
        g_holdMs = 5000; g_map.ok = 1; g_lnMs[1] = 950; g_lnFreeze[1] = 800; g_lnN[1] = 3;
        t += 7; ViewStep(1.0f, t, 1000, 500);                            /* (the plan's on: timed) */
        PutFloat(HOLD_BAR_VA, 1.0f);
        g_holdFrame = TestHoldFrame; g_holdOn = 1; g_testFrames = 0; shim_ticks = t;
        HoldFrame();
        ok &= g_testFrames > 1 && g_stepLock.held == 0 && g_settleTo != 0;
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
        g_mapShown = 1; g_scrW = 1000; g_scrH = 500;
        memcpy(g_bgUV, kWholeUV, sizeof kWholeUV);
        for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
        g_viewTex[VT_MAP] = g_viewTex[VT_MARKS] = g_viewTex[VT_SEA] = g_viewTex[VT_LOGO] = 1;
        ViewBuild(1000, 500, 0.5f, 100000);
        int order[VIEW_BATCHES], n = 0, logo = -1;
        for (int i = 0; i < g_vbN; i++) { order[n++] = g_vb[i].tex; if (g_vb[i].tex == VT_LOGO) logo = i; }
        ok &= n >= 4 && order[0] == VT_MAP && order[1] == VT_MARKS && order[n - 1] == VT_SEA && order[n - 2] == VT_LOGO;
        for (int i = 2; i < n - 2; i++) ok &= order[i] == VT_SEA || order[i] == VT_MARKS;   /* glints, ripple, badge */
        const ViewVert *blk = g_vv + g_vb[n - 1].first;                  /* last, the log's black band under the bar */
        ok &= g_vb[n - 1].count == 4 && blk[0].x == 0 && blk[0].y == 500 && blk[2].x == 1000 &&
              fabsf(blk[2].y - (1 - BAR_BLACK_H) * 500) < 1e-3f;
        g_barGarland = 1; g_bars.families = 1;                           /* a garland: no black band */
        ViewBuild(1000, 500, 0.5f, 100010);
        ok &= g_vb[g_vbN - 1].tex == VT_LOGO;
        g_barGarland = 0; g_bars.families = 0;
        if (logo >= 0) {
            const ViewVert *w = g_vv + g_vb[logo].first;
            ok &= g_vb[logo].count == 4 && fabsf(w[0].x - g_logoPos[0] * 1000) < 1e-3f &&
                  fabsf(w[0].y - (1 - g_logoPos[1]) * 500) < 1e-3f && fabsf(w[2].x - g_logoPos[9] * 1000) < 1e-3f &&
                  fabsf(w[2].y - (1 - g_logoPos[10]) * 500) < 1e-3f && w[2].u == g_logoUV[6] && w[2].v == g_logoUV[7];
        } else ok = 0;
        g_viewTex[VT_LOGO] = 0;
        ViewBuild(1000, 500, 0.5f, 100020);
        for (int i = 0; i < g_vbN; i++) ok &= g_vb[i].tex != VT_LOGO;
        g_mapShown = 0; g_ovOn = 0;
        for (int i = 0; i < VT_COUNT; i++) g_viewTex[i] = 0;
        Check(ok, "slice 2: the view's layers in the game's order (the map, the Maelstrom, the sea's glints and marks, the logo, the log's black band; none under a garland), the logo where the game puts it; a layer whose sheet OpenGL lacks is left out");
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

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: test_switch <scratch dir>\n"); return 2; }
    g_root = argv[1];

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
    if (argc > 2) {
        FireBarRealClient(argv[2]);
        FireRealClient(argv[2]);
    }

    if (fails) printf("%d of %d checks FAILED\n", fails, checks); else printf("all %d checks passed\n", checks);
    return fails != 0;
}
