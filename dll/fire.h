/* The loading screen's sound, the part of CinderLoad.dll kept free of Windows and FMOD calls so it can be tested on its
 * own (tests/test_switch.c). Included once, by cinderload.c.
 *
 * 7 Oct 2026: the crackling fire that went with the fire bar is gone (his call: "clear out ... the old cinderload
 * crackling fire sound"), and with it the druid's own; the blade's sound is one of several recipes he auditions on real
 * loads (cinderload.c, kSounds). Each recipe is a few of the game's own sounds, read out of the player's client at run
 * time (none is shipped), each resampled to the mixer's rate and looped with a 0.12 s crossfade, then:
 *   a bed, there from the start and rising with the fill; a swell, silent until its start and whole by its end; motion,
 *   as loud as the bar is moving; sparks, one-shots fired now and then, more often as the bar fills (the blade is
 *   unstable); up to three stingers, each played once as the fill passes its mark (0: as the screen begins).
 * A sound can be stretched (played slower, so lower) or sped up. The mix is centred, or follows the bar's front from
 * left to right by as much as the recipe's sweep (1: the old fire's 80/20, equal power). It fades in over 0.4 s as a
 * loading screen comes up and out over 0.4 s when it goes; the front glides after the bar with a 0.3 s time constant.
 */
#ifndef CINDERLOAD_FIRE_H
#define CINDERLOAD_FIRE_H

#define FIRE_PAN_FROM   0.295f   /* equal-power pan at an empty bar: 80% of the loudness left */
#define FIRE_PAN_TO     0.705f   /* at a full bar: 80% right */
#define FIRE_GLIDE_S    0.30f    /* the pan follows the fill with this time constant (the mock's) */
#define FIRE_MOVE_S     0.25f    /* the crackle follows the front's speed with this one (the mock's) */
#define FIRE_MOVE_CAP   0.60f    /* speeds over this many bars a second count as this */
#define FIRE_MOVE_FULL  0.25f    /* at this speed the crackle is at 1 */
#define FIRE_MOVE_MAX   1.20f    /* and never more than this */
#define FIRE_FADE_IN_S  0.40f
#define FIRE_FADE_OUT_S 0.40f
#define FIRE_XFADE_S    0.12f    /* each loop's seam */
#define FIRE_MAX_FRAMES (1 << 21) /* a source over this many frames is not one of these loops */

/* ---- a little arithmetic, with no C runtime ---- */
#define FIRE_HALF_PI 1.57079632679489662f
static float FireClamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static double FireExp(double x)                    /* e^x: halved until small, a series, squared back */
{
    int k = 0;
    while ((x > 0.5 || x < -0.5) && k < 60) { x *= 0.5; k++; }
    double term = 1, sum = 1;
    for (int i = 1; i < 14; i++) { term *= x / i; sum += term; }
    while (k-- > 0) sum *= sum;
    return sum;
}
static float FireSin(float x)                       /* for 0 <= x <= pi/2, to about 1e-7 */
{
    float x2 = x * x;
    return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110 * (1 - x2 / 156))))));
}
static float FireCos(float x)
{
    float x2 = x * x;
    return 1 - x2 / 2 * (1 - x2 / 12 * (1 - x2 / 30 * (1 - x2 / 56 * (1 - x2 / 90 * (1 - x2 / 132 * (1 - x2 / 182))))));
}

/* Where the old fire's pan sat for a fill of 0..1: 0.295 + 0.41 * fill, equal power (left = cos(pan * pi/2), right =
 * sin(pan * pi/2)); a recipe's sweep takes as much of it as it asks for. */
static float FirePan(float fill) { return FIRE_PAN_FROM + (FIRE_PAN_TO - FIRE_PAN_FROM) * FireClamp(fill, 0, 1); }

/* The motion's level for a smoothed speed of the front, in bars a second. */
static float FireCrackleLevel(float speed) { return FireClamp(speed / FIRE_MOVE_FULL, 0, FIRE_MOVE_MAX); }

/* ---- the source files ---- */

/* A RIFF WAVE of 16-bit PCM, mono or stereo (stereo averaged to mono, as the mock did), into floats of -1..1 on buf,
 * which holds room for max frames. The frame count, or 0 for anything else (and *rate set). Bounds are checked
 * against len at every step: a damaged file is refused, never read past. */
static unsigned FireRd16(const unsigned char *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }
static unsigned FireRd32(const unsigned char *p) { return FireRd16(p) | FireRd16(p + 2) << 16; }
static int FireWavInfo(const unsigned char *wav, unsigned len, int *rate, int *channels, const unsigned char **data,
                       unsigned *frames)
{
    if (len < 12 || FireRd32(wav) != 0x46464952u || FireRd32(wav + 8) != 0x45564157u) return 0;   /* "RIFF" "WAVE" */
    int haveFmt = 0;
    for (unsigned at = 12; at + 8 <= len;) {
        unsigned id = FireRd32(wav + at), size = FireRd32(wav + at + 4);
        if (size > len - at - 8) return 0;
        const unsigned char *body = wav + at + 8;
        if (id == 0x20746D66u) {                                                                    /* "fmt " */
            if (size < 16 || FireRd16(body) != 1 || FireRd16(body + 14) != 16) return 0;            /* PCM, 16 bit */
            *channels = (int)FireRd16(body + 2);
            *rate = (int)FireRd32(body + 4);
            if ((*channels != 1 && *channels != 2) || *rate < 8000 || *rate > 192000) return 0;
            haveFmt = 1;
        } else if (id == 0x61746164u) {                                                             /* "data" */
            if (!haveFmt) return 0;
            *data = body;
            *frames = size / (2u * (unsigned)*channels);
            return *frames > 1 && *frames <= FIRE_MAX_FRAMES;
        }
        at += 8 + size + (size & 1);
    }
    return 0;
}
static void FireWavRead(const unsigned char *data, int channels, unsigned frames, float *out)
{
    for (unsigned i = 0; i < frames; i++) {
        const unsigned char *s = data + (size_t)i * 2 * (unsigned)channels;
        float v = (float)(short)FireRd16(s);
        if (channels == 2) v = (v + (float)(short)FireRd16(s + 2)) * 0.5f;
        out[i] = v / 32768.0f;
    }
}

/* How many frames a source of n frames at rate `from` becomes at rate `to`: floor(n * to / from), as the mock's
 * resampler counts them. */
static unsigned FireResampledLength(unsigned n, int from, int to) { return (unsigned)((double)n * to / from); }

/* One loop, made as the mock made it: the source resampled to `to` by straight lines between its frames (np.interp, the
 * last frame held past the end), turned to start `start` frames in, faded in and out over xf frames at that seam, and
 * laid end to end every n - xf frames, each copy's fade-out over the next one's fade-in. One period of that, n - xf
 * frames, is written to out (room for FireResampledLength frames). Returns the period, or 0 when the loop is too
 * short to cross-fade. */
static unsigned FireMakeLoop(const float *src, unsigned frames, int from, int to, int start, float *out)
{
    if (from <= 0 || to <= 0 || frames < 2) return 0;
    unsigned n = FireResampledLength(frames, from, to), xf = (unsigned)to * 12u / 100u;     /* FIRE_XFADE_S, exactly */
    if (n < 2 * xf + 2) return 0;
    /* resampled, into out: frame j sits at j * from / to source frames, stepped with whole numbers so it never drifts */
    unsigned idx = 0, rem = 0;
    for (unsigned j = 0; j < n; j++) {
        float a = src[idx];
        out[j] = idx + 1 < frames ? a + (src[idx + 1] - a) * ((float)rem / (float)to) : src[frames - 1];
        rem += (unsigned)from;
        while (rem >= (unsigned)to) { rem -= (unsigned)to; idx++; }
        if (idx >= frames) idx = frames - 1;
    }
    /* turned by start: a seam there instead of at the file's own ends (np.roll) */
    unsigned s = (unsigned)start % n;
    if (s) {                                                    /* rotate in place, three reversals */
        for (unsigned a = 0, b = s - 1; a < b; a++, b--) { float t = out[a]; out[a] = out[b]; out[b] = t; }
        for (unsigned a = s, b = n - 1; a < b; a++, b--) { float t = out[a]; out[a] = out[b]; out[b] = t; }
        for (unsigned a = 0, b = n - 1; a < b; a++, b--) { float t = out[a]; out[a] = out[b]; out[b] = t; }
    }
    /* the seam: frame i of the head (rising ramp) plus frame n - xf + i of the tail (falling ramp), as the mock's
     * np.linspace(0, 1, xf) ramps */
    for (unsigned i = 0; i < xf; i++) {
        float up = xf > 1 ? (float)i / (float)(xf - 1) : 1;
        out[i] = out[i] * up + out[n - xf + i] * (1 - up);
    }
    return n - xf;
}

/* ---- the mixer: what every recipe shares (the glide after the bar, its speed, the fades) ---- */
typedef struct {
    int rate;                                  /* frames a second, the stream's */
    float kGlide, kMove, stepIn, stepOut;      /* per frame */
    float front, last, speed, env;             /* the gliding fill, its last value, its smoothed speed, the fade */
} FireMix;

/* Coefficients for a rate, and the mixer set back to silence at the start of every loop. */
static void FireSetRate(FireMix *m, int rate)
{
    m->rate = rate;
    m->kGlide = (float)(1 - FireExp(-1.0 / (rate * (double)FIRE_GLIDE_S)));
    m->kMove = (float)(1 - FireExp(-1.0 / (rate * (double)FIRE_MOVE_S)));
    m->stepIn = 1.0f / (FIRE_FADE_IN_S * rate);
    m->stepOut = 1.0f / (FIRE_FADE_OUT_S * rate);
}
static void FireReset(FireMix *m) { m->front = m->last = m->speed = m->env = 0; }

/* ---- a recipe ----
 * Its lines (the built-in recipes in cinderload.c are written this way; a bad line is skipped):
 *   level <x>                                        the mix's scale
 *   sweep <x>                                        0 centred .. 1 the old fire's full left-to-right sweep
 *   loop <bed|swell|motion> <gain> <from> <to> [x<speed>] <path>
 *       bed: there from the start at 0.55 of its gain, rising to all of it as the fill goes from <from> to <to>;
 *       swell: silent until <from>, all of it by <to> (an S curve between); motion: as loud as the bar is moving
 *   spark <gain> <from> <to> [x<speed>] <path>       a one-shot fired every 2.4 s or so at <from>, every half second by
 *                                                    <to>, each wait a little longer or shorter than the last
 *   stinger <gain> <at> [x<speed>] <path>            played once, whole, as the fill passes <at> (0: as it begins)
 * x<speed>: 0.8 plays the sound at 80% speed (longer and lower), 1.25 faster; 0.25 to 4. Paths are the client's own. */
#define SND_LOOPS 6
#define SND_SPARKS 2
#define SND_STINGS 3
#define SND_MAX_S 30             /* a loop's source is cut to this many seconds (a 77 s ambience is 13 MB at 44.1 kHz) */
#define SND_PATH  120
#define SND_SPARK_SLOW 2.4f      /* seconds between sparks at a recipe's <from> */
#define SND_SPARK_FAST 0.5f      /* and by its <to> */
enum { SND_BED, SND_SWELL, SND_MOTION };
typedef struct { int kind; float gain, from, to, speed; char path[SND_PATH]; } SndLayer;
typedef struct {
    int n, sparks, stings;                          /* loops, sparks and stingers listed */
    float level, sweep;
    SndLayer l[SND_LOOPS], spark[SND_SPARKS], sting[SND_STINGS];   /* a stinger's <at> is its from */
} SndSpec;
typedef struct {
    int n;                                          /* loops made (0: nothing to play) */
    float *loop[SND_LOOPS], *spark[SND_SPARKS], *sting[SND_STINGS];
    unsigned len[SND_LOOPS], pos[SND_LOOPS], sparkLen[SND_SPARKS], sparkPos[SND_SPARKS], stingLen[SND_STINGS],
             stingPos[SND_STINGS], seed;
    int sparkOn[SND_SPARKS], stingState[SND_STINGS];   /* a spark sounding; a stinger 0 waiting, 1 playing, 2 played */
    float sparkWait[SND_SPARKS];                    /* seconds to the next spark */
} SndSet;

/* A decimal number ("0.55", "1", ".9") at s[*at] after spaces: 1 with it in *out. */
static int SndNum(const char *s, int n, int *at, float *out)
{
    while (*at < n && (s[*at] == ' ' || s[*at] == '\t')) (*at)++;
    float v = 0, scale = 0;
    int digits = 0;
    for (; *at < n; (*at)++) {
        char c = s[*at];
        if (c >= '0' && c <= '9') { if (scale) { v += (c - '0') * scale; scale *= 0.1f; } else v = v * 10 + (c - '0'); digits++; }
        else if (c == '.' && !scale) scale = 0.1f;
        else break;
    }
    if (!digits || digits > 9) return 0;
    *out = v;
    return 1;
}
/* A word at s[*at] after spaces, compared with w (lower case); 1 and past it when it is that word. */
static int SndWord(const char *s, int n, int *at, const char *w)
{
    int i = *at;
    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    int k = 0;
    while (w[k] && i + k < n && (s[i + k] | 0x20) == w[k]) k++;
    if (w[k] || (i + k < n && s[i + k] != ' ' && s[i + k] != '\t')) return 0;
    *at = i + k;
    return 1;
}
/* The rest of the line, spaces before and after cut, into out (SND_PATH): 1 when it is a path that fits. */
static int SndRest(const char *s, int n, int at, char *out)
{
    while (at < n && (s[at] == ' ' || s[at] == '\t')) at++;
    while (n > at && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) n--;
    if (n - at < 5 || n - at >= SND_PATH) return 0;
    for (int i = at; i < n; i++) { if ((unsigned char)s[i] < 0x20) return 0; out[i - at] = s[i]; }
    out[n - at] = 0;
    return 1;
}
/* An optional x<speed> at s[*at] after spaces (default 1): 0 when one is there but out of 0.25 to 4. */
static int SndSpeed(const char *s, int n, int *at, float *speed)
{
    int i = *at;
    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    *speed = 1;
    if (i + 1 >= n || (s[i] | 0x20) != 'x' || !((s[i + 1] >= '0' && s[i + 1] <= '9') || s[i + 1] == '.')) return 1;
    i++;
    if (!SndNum(s, n, &i, speed) || *speed < 0.25f || *speed > 4) return 0;
    *at = i;
    return 1;
}
/* A recipe's text into sp: 1 when it has a level and at least one loop. Bad lines are skipped. */
static int SndParse(SndSpec *sp, const char *t, unsigned len)
{
    unsigned char *z = (unsigned char *)sp;
    for (unsigned i = 0; i < sizeof *sp; i++) z[i] = 0;
    for (unsigned at = 0; at < len;) {
        unsigned end = at;
        while (end < len && t[end] != '\n') end++;
        const char *l = t + at;
        int n = (int)(end - at), i = 0;
        float a, b, c, x;
        if (SndWord(l, n, &i, "level") && SndNum(l, n, &i, &a) && a > 0 && a < 100) sp->level = a;
        else if ((i = 0, SndWord(l, n, &i, "sweep")) && SndNum(l, n, &i, &a) && a <= 1) sp->sweep = a;
        else if ((i = 0, SndWord(l, n, &i, "loop")) && sp->n < SND_LOOPS) {
            int kind = SndWord(l, n, &i, "bed") ? SND_BED : SndWord(l, n, &i, "swell") ? SND_SWELL :
                       SndWord(l, n, &i, "motion") ? SND_MOTION : -1;
            SndLayer *y = &sp->l[sp->n];
            if (kind >= 0 && SndNum(l, n, &i, &a) && SndNum(l, n, &i, &b) && SndNum(l, n, &i, &c) && a <= 4 && b <= 1 && c <= 1 &&
                SndSpeed(l, n, &i, &x) && SndRest(l, n, i, y->path)) {
                y->kind = kind; y->gain = a; y->from = b; y->to = c; y->speed = x;
                sp->n++;
            }
        } else if ((i = 0, SndWord(l, n, &i, "spark")) && sp->sparks < SND_SPARKS) {
            SndLayer *y = &sp->spark[sp->sparks];
            if (SndNum(l, n, &i, &a) && SndNum(l, n, &i, &b) && SndNum(l, n, &i, &c) && a <= 4 && b <= 1 && c <= 1 &&
                SndSpeed(l, n, &i, &x) && SndRest(l, n, i, y->path)) {
                y->gain = a; y->from = b; y->to = c; y->speed = x;
                sp->sparks++;
            }
        } else if ((i = 0, SndWord(l, n, &i, "stinger")) && sp->stings < SND_STINGS) {
            SndLayer *y = &sp->sting[sp->stings];
            if (SndNum(l, n, &i, &a) && SndNum(l, n, &i, &b) && a <= 4 && b <= 1 && SndSpeed(l, n, &i, &x) &&
                SndRest(l, n, i, y->path)) {
                y->gain = a; y->from = b; y->speed = x;
                sp->stings++;
            }
        }
        at = end + 1;
    }
    return sp->level > 0 && sp->n > 0;
}

/* The stinger at the mixer's rate, whole (no seam): the same straight-line resampling as the loops'. Returns frames. */
static unsigned SndOneShot(const float *src, unsigned frames, int from, int to, float *out)
{
    if (from <= 0 || to <= 0 || frames < 2) return 0;
    unsigned n = FireResampledLength(frames, from, to), idx = 0, rem = 0;
    for (unsigned j = 0; j < n; j++) {
        float a = src[idx];
        out[j] = idx + 1 < frames ? a + (src[idx + 1] - a) * ((float)rem / (float)to) : src[frames - 1];
        rem += (unsigned)from;
        while (rem >= (unsigned)to) { rem -= (unsigned)to; idx++; }
        if (idx >= frames) idx = frames - 1;
    }
    return n;
}
static float SndSmooth(float e0, float e1, float x)
{
    float u = FireClamp((x - e0) / (e1 - e0 > 1e-6f ? e1 - e0 : 1e-6f), 0, 1);
    return u * u * (3 - 2 * u);
}
static void SndReset(SndSet *s)
{
    for (int i = 0; i < SND_LOOPS; i++) s->pos[i] = 0;
    for (int i = 0; i < SND_SPARKS; i++) { s->sparkOn[i] = 0; s->sparkPos[i] = 0; s->sparkWait[i] = 0.3f + 0.4f * i; }
    for (int i = 0; i < SND_STINGS; i++) { s->stingPos[i] = 0; s->stingState[i] = 0; }
    s->seed = 0x2545F491u;
}
/* 0 to 1, the next of the sparks' own sequence (each wait a little different, the same every time for a test). */
static float SndRand(SndSet *s) { s->seed = s->seed * 1664525u + 1013904223u; return (float)(s->seed >> 8) / 16777216.0f; }
/* A gentle limit: as it was up to 0.8, then bending ever closer to 1 and never reaching it, so a stinger landing on a
 * loud bed rounds off instead of clipping. */
static float SndLimit(float x)
{
    float a = x < 0 ? -x : x;
    if (a > 0.8f) { float d = (a - 0.8f) / 0.2f; a = 0.8f + 0.2f * d / (1 + d); }
    return x < 0 ? -a : a;
}
/* frames of 16-bit stereo into out from a recipe's sounds. gate: a loading screen is showing; fill: how far its bar is,
 * 0..1. Returns 1 while there is still something to hear (the fade not yet at zero). */
static int SndRender(FireMix *m, SndSet *s, const SndSpec *sp, int gate, float fill, short *out, int frames)
{
    fill = FireClamp(fill, 0, 1);
    float perFrame = 1.0f / (float)(m->rate > 0 ? m->rate : 44100);
    for (int f = 0; f < frames; f++) {
        if (gate && m->env <= 0) { m->front = m->last = fill; m->speed = 0; }   /* a new screen: start at its fill */
        m->env = FireClamp(m->env + (gate ? m->stepIn : -m->stepOut), 0, 1);
        m->front += (fill - m->front) * m->kGlide;
        float speed = (m->front - m->last) * (float)m->rate;
        m->last = m->front;
        m->speed += ((speed < FIRE_MOVE_CAP ? speed : FIRE_MOVE_CAP) - m->speed) * m->kMove;
        float sum = 0;
        for (int i = 0; i < sp->n && i < SND_LOOPS; i++) {
            if (!s->loop[i]) continue;
            float v = s->loop[i][s->pos[i]] * sp->l[i].gain, lvl;
            if (++s->pos[i] >= s->len[i]) s->pos[i] = 0;
            if (sp->l[i].kind == SND_BED) lvl = 0.55f + 0.45f * SndSmooth(sp->l[i].from, sp->l[i].to, fill);
            else if (sp->l[i].kind == SND_SWELL) lvl = SndSmooth(sp->l[i].from, sp->l[i].to, fill);
            else lvl = FireCrackleLevel(m->speed);
            sum += v * lvl;
        }
        for (int i = 0; i < sp->sparks && i < SND_SPARKS; i++) {        /* now and then, more often as the bar fills */
            if (!s->spark[i]) continue;
            if (gate && !s->sparkOn[i] && (s->sparkWait[i] -= perFrame) <= 0) {
                float every = SND_SPARK_SLOW + (SND_SPARK_FAST - SND_SPARK_SLOW) * SndSmooth(sp->spark[i].from, sp->spark[i].to, fill);
                s->sparkOn[i] = 1; s->sparkPos[i] = 0;
                s->sparkWait[i] = every * (0.6f + 0.8f * SndRand(s));
            }
            if (s->sparkOn[i]) {
                sum += s->spark[i][s->sparkPos[i]] * sp->spark[i].gain;
                if (++s->sparkPos[i] >= s->sparkLen[i]) s->sparkOn[i] = 0;
            }
        }
        for (int i = 0; i < sp->stings && i < SND_STINGS; i++) {        /* each once, as the fill passes its mark */
            if (!s->sting[i]) continue;
            if (gate && s->stingState[i] == 0 && fill >= sp->sting[i].from) { s->stingState[i] = 1; s->stingPos[i] = 0; }
            if (s->stingState[i] == 1) {
                sum += s->sting[i][s->stingPos[i]] * sp->sting[i].gain;
                if (++s->stingPos[i] >= s->stingLen[i]) s->stingState[i] = 2;
            }
        }
        float mono = sum * sp->level * m->env, l = 0.70710678f, r = 0.70710678f;
        if (sp->sweep > 0) {                                            /* following the front, by as much as the sweep */
            float a = (0.5f + (FirePan(m->front) - 0.5f) * sp->sweep) * FIRE_HALF_PI;
            l = FireCos(a); r = FireSin(a);
        }
        out[2 * f] = (short)(SndLimit(mono * l) * 32767.0f);
        out[2 * f + 1] = (short)(SndLimit(mono * r) * 32767.0f);
    }
    return m->env > 0 || gate;
}

/* ---- what the worker makes of the client's numbers ---- */

/* The bar's fill as the worker passes it on: the client clears the number to 0 and then works it out again every
 * frame, so a read can catch the 0 (or a part sum) in between. Within one loading screen the fill only grows, so the
 * largest seen is the fill; a fall that lasts FIRE_FALL_POLLS reads running (20 ms apart, never a passing 0) is the
 * client starting a new screen in the same window, and is taken. */
#define FIRE_FALL_POLLS 3
typedef struct { float fill; int falls; } FireFill;
static void FireFillStart(FireFill *s, float read) { s->fill = (read >= 0 && read <= 1) ? read : (read > 1 ? 1 : 0); s->falls = 0; }
static float FireFillRead(FireFill *s, float read)
{
    if (!(read >= 0)) read = 0;                     /* NaN as 0 */
    if (read > 1) read = 1;
    if (read >= s->fill) { s->fill = read; s->falls = 0; }
    else if (read < s->fill - 0.25f && ++s->falls >= FIRE_FALL_POLLS) { s->fill = read; s->falls = 0; }
    else if (read >= s->fill - 0.25f) s->falls = 0;
    return s->fill;
}

#endif
