/* CinderLoad.dll: loading screens at the shape of the screen, for the 32-bit 1.12.1 client.
 *
 * The loading screens come in one archive per screen shape, Data\CinderLoad\LoadingScreens-<shape>.MPQ (the game
 * reads no archive in that folder). As the game starts, before it opens its own archives, this DLL reads the
 * resolution the game saved (SET gxResolution in WTF\Config.wtf; the monitor's size when there is none yet), picks
 * the nearest shape (shape.h) and copies that archive to Data\patch-~.MPQ, which the game then loads over its own
 * loading screens. A resolution changed in the game counts from the next start.
 *
 * Why patch-~: the client loads Data\patch-?.MPQ (one character, the pattern in WoW.exe) in sorted order and a later
 * archive wins. Every letter after M (the Project Reforged loading screens) is taken by a launcher pack, Ultra HD at U
 * among them, and '~' sorts after every letter and digit whichever way the names are compared, so ours load last and
 * no other pack is ever mistaken for this one.
 *
 * The stock client keeps every loading screen at 4:3. At RVA 0x6AC4 it calls a function whose result it then
 * compares (FCOM) to decide that; putting FLD1 (1.0) and four NOPs in place of the call lets the loading screen
 * fill the screen. That five-byte change is the community's widescreen edit of WoW.exe, published with its stock
 * and changed bytes in VanillaWideLoadScreens (github.com/Seraphic8x2244/VanillaWideLoadScreens); this file is
 * written from those published bytes, not from its code. It is made only when our loading screens for this shape
 * are in place, since the game's own square ones would be stretched, and only when the eleven bytes there (the
 * call and the compare after it) are exactly the stock ones. WoW.exe on disk is never touched.
 *
 * A Data\patch-~.MPQ that is not ours (not the size Data\CinderLoad\installed.txt recorded when we put it there,
 * and none of our archives) is left alone, and the switch stays off. Each start adds a line or two to Logs\CinderLoad.log.
 *
 * While a loading screen shows, the bar burns with a sound: the game's own fire loops, read from the player's client
 * archives, panned after the fill's front (fire.h, and "the fire sound" below). It is played through the client's
 * own fmod.dll from a thread of ours that only reads the client's memory; nothing in the client is changed for it.
 *
 * The bar's fire takes the colour of the picture's (orange, fel green, arcane, aqua: "the fire bar" below). The game
 * loads the bar's two textures once per screen; one call there is sent through this DLL, which names the textures
 * of that screen's fire family in our pack instead. Only with our loading screens in place, and only when every place
 * it relies on holds the bytes it was made for; otherwise the bar is the pack's orange one, as before.
 *
 * Built like IndoorRain.dll, with no C runtime (build.sh), and with IndoorRain's MPQ reader (mpq.c, inflate.c).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "shape.h"
#include "fire.h"
#include "mpq.h"

#define DLL_NAME  "CinderLoad"
#define PATCH_RVA 0x6AC4

static const unsigned char kStock[11] = { 0xE8, 0xA7, 0x42, 0x01, 0x00,           /* call (the 4:3 decision)     */
                                          0xD8, 0x15, 0xD8, 0xF9, 0x7F, 0x00 };   /* fcom dword [0x7FF9D8] after */
static const unsigned char kWide[5]   = { 0xD9, 0xE8, 0x90, 0x90, 0x90 };         /* fld1, then four nops        */

static char g_dir[MAX_PATH];             /* the game's folder, ending in a backslash */

static int Same(const unsigned char *a, const unsigned char *b, int n)
{
    for (int i = 0; i < n; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

static int Join(char *out, const char *a, const char *b)
{
    if (lstrlenA(a) + lstrlenA(b) >= MAX_PATH) return 0;
    lstrcpyA(out, a);
    lstrcatA(out, b);
    return 1;
}

static int GameDir(void)
{
    DWORD n = GetModuleFileNameA(NULL, g_dir, MAX_PATH);
    if (!n || n >= MAX_PATH) { g_dir[0] = 0; return 0; }
    char *dir = g_dir;
    for (char *p = g_dir; *p; p++)
        if (*p == '\\' || *p == '/') dir = p + 1;
    *dir = 0;
    return 1;
}

/* One line, appended to Logs\CinderLoad.log next to WoW.exe. */
static void Log(const char *msg)
{
    char path[MAX_PATH];
    if (!g_dir[0] || !Join(path, g_dir, "Logs\\" DLL_NAME ".log")) return;
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    char line[400];
    int len = wsprintfA(line, "%04d-%02d-%02d %02d:%02d:%02d %s\r\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                        t.wSecond, msg);
    DWORD wrote;
    WriteFile(h, line, (DWORD)len, &wrote, NULL);
    CloseHandle(h);
}

static int Exists(const char *path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static int VariantPath(char *out, int shape)
{
    return Join(out, g_dir, "Data\\CinderLoad\\LoadingScreens-") && lstrlenA(out) + 16 < MAX_PATH &&
           lstrcatA(out, kShapes[shape].name) && lstrcatA(out, ".MPQ");
}

/* A small text file, NUL-terminated, on the process heap (HeapFree it); NULL when missing or over limit bytes. */
static char *ReadText(const char *path, DWORD limit)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD size = GetFileSize(f, NULL), got = 0;
    char *buf = (size == INVALID_FILE_SIZE || size > limit) ? NULL : HeapAlloc(GetProcessHeap(), 0, size + 1);
    if (buf && !ReadFile(f, buf, size, &got, NULL)) got = 0;
    CloseHandle(f);
    if (buf) buf[got] = 0;
    return buf;
}

/* 1 when the two files hold the same bytes. */
static int SameContent(const char *a, const char *b)
{
    HANDLE fa = CreateFileA(a, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE fb = CreateFileA(b, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    int same = 0;
    if (fa != INVALID_HANDLE_VALUE && fb != INVALID_HANDLE_VALUE) {
        DWORD sa = GetFileSize(fa, NULL), sb = GetFileSize(fb, NULL);
        enum { CHUNK = 65536 };
        unsigned char *ba = HeapAlloc(GetProcessHeap(), 0, CHUNK), *bb = HeapAlloc(GetProcessHeap(), 0, CHUNK);
        if (sa == sb && sa != INVALID_FILE_SIZE && ba && bb) {
            same = 1;
            for (DWORD left = sa; same && left;) {
                DWORD want = left < CHUNK ? left : CHUNK, ga = 0, gb = 0;
                if (!ReadFile(fa, ba, want, &ga, NULL) || !ReadFile(fb, bb, want, &gb, NULL) || ga != want || gb != want ||
                    !Same(ba, bb, (int)want))
                    same = 0;
                left -= want;
            }
        }
        if (ba) HeapFree(GetProcessHeap(), 0, ba);
        if (bb) HeapFree(GetProcessHeap(), 0, bb);
    }
    if (fa != INVALID_HANDLE_VALUE) CloseHandle(fa);
    if (fb != INVALID_HANDLE_VALUE) CloseHandle(fb);
    return same;
}

static DWORD FileSize(const char *path)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return INVALID_FILE_SIZE;
    DWORD size = GetFileSize(f, NULL);
    CloseHandle(f);
    return size;
}

/* The ownership record says which shape we put in place and how big that file was: "21x9 98765432". It lives beside
 * the archive, as Data\patch-~.cinderload, so it stays with the file it describes even if Data\CinderLoad is removed
 * (0.1.2 and earlier kept it at Data\CinderLoad\installed.txt, still read once and then moved). It counts only while
 * Data\patch-~.MPQ is still that size, so a file that replaced ours is never taken for ours. */
static int MarkerMatches(const char *marker, const char *dst)
{
    char *text = ReadText(marker, 256);
    if (!text) return 0;
    const char *p = text, *num = NULL;
    for (; *p; p++)
        if (*p == ' ') num = p + 1;
    DWORD want = 0;
    int digits = 0;
    for (p = num; p && *p >= '0' && *p <= '9' && digits < 10; p++, digits++) want = want * 10 + (DWORD)(*p - '0');
    HeapFree(GetProcessHeap(), 0, text);
    return digits > 0 && want == FileSize(dst);
}

static void WriteText(const char *path, const char *text)
{
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD wrote;
    WriteFile(f, text, (DWORD)lstrlenA(text), &wrote, NULL);
    CloseHandle(f);
}

static int ClientIsOurs(void);

/* Takes our archive out: only once it is really gone is the ownership record dropped, so a file that could not be
 * deleted (another client has it open) is still known as ours next time. */
static void TakeOut(const char *dst, const char *marker, const char *oldMarker)
{
    if (!Exists(dst) || DeleteFileA(dst) || !Exists(dst)) {
        DeleteFileA(marker);
        DeleteFileA(oldMarker);
    }
}

/* Puts the loading screens for this screen's shape in place as Data\patch-~.MPQ. 1 when ours are in place, so the
 * switch can go on. */
static int g_scrW = 1920, g_scrH = 1080;        /* the screen, as Config.wtf (or the monitor) says */
static int ChooseScreens(void)
{
    char dst[MAX_PATH], marker[MAX_PATH], oldMarker[MAX_PATH], cfg[MAX_PATH], src[MAX_PATH], tmp[MAX_PATH], m[320];
    char pending[48];
    wsprintfA(pending, "Data\\CinderLoad\\pending-%lu.tmp", GetCurrentProcessId());
    if (!Join(dst, g_dir, "Data\\patch-~.MPQ") || !Join(marker, g_dir, "Data\\patch-~.cinderload") ||
        !Join(oldMarker, g_dir, "Data\\CinderLoad\\installed.txt") || !Join(cfg, g_dir, "WTF\\Config.wtf") ||
        !Join(tmp, g_dir, pending))
        return 0;

    int avail[SHAPE_COUNT], any = 0;
    for (int i = 0; i < SHAPE_COUNT; i++) {
        avail[i] = VariantPath(src, i) && Exists(src);
        any |= avail[i];
    }
    int w = 0, h = 0;
    const char *from = "Config.wtf";
    char *text = ReadText(cfg, 1 << 20);
    if (!text || !ParseResolution(text, &w, &h)) {
        w = GetSystemMetrics(SM_CXSCREEN);
        h = GetSystemMetrics(SM_CYSCREEN);
        from = "the monitor";
    }
    if (text) HeapFree(GetProcessHeap(), 0, text);
    if (w > 0 && h > 0) { g_scrW = w; g_scrH = h; }
    int pick = any ? ChooseShape(w, h, avail) : -1;
    int client = ClientIsOurs();
    if (!client) pick = -1;              /* never put widened screens in place where the switch cannot go on */

    int haveDst = Exists(dst);
    if (pick >= 0 && haveDst && VariantPath(src, pick) && SameContent(src, dst)) {
        if (!MarkerMatches(marker, dst)) {             /* moved beside the archive from 0.1.2's place, or rewritten */
            char note[64];
            wsprintfA(note, "%s %lu", kShapes[pick].name, FileSize(dst));
            WriteText(marker, note);
        }
        DeleteFileA(oldMarker);
        wsprintfA(m, "screen %dx%d (from %s): the %s loading screens are in place", w, h, from, kShapes[pick].name);
        Log(m);
        return 1;
    }
    int ours = !haveDst || MarkerMatches(marker, dst) || MarkerMatches(oldMarker, dst);
    for (int i = 0; !ours && i < SHAPE_COUNT; i++)
        if (avail[i] && VariantPath(src, i) && SameContent(src, dst)) ours = 1;
    if (!ours) {
        wsprintfA(m, "screen %dx%d (from %s): Data\\patch-~.MPQ is not ours, so it is left alone and the switch stays off",
                  w, h, from);
        Log(m);
        return 0;
    }
    if (pick < 0) {
        TakeOut(dst, marker, oldMarker);
        if (!client)
            wsprintfA(m, "screen %dx%d (from %s): this WoW.exe is not the client this was made for, so the game's own "
                      "loading screens, as it shows them", w, h, from);
        else
            wsprintfA(m, "screen %dx%d (from %s): no loading screens for this shape here, so the game's own, as it "
                      "shows them", w, h, from);
        Log(m);
        return 0;
    }
    /* Copied beside the archive first, then moved over it in one step: a copy cut short (the game killed while it
     * starts, a power cut) never leaves a broken Data\patch-~.MPQ, and a second client starting at the same moment
     * copies to its own file. */
    if (!VariantPath(src, pick) || !CopyFileA(src, tmp, FALSE) ||
        !MoveFileExA(tmp, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DWORD err = GetLastError();
        DeleteFileA(tmp);
        TakeOut(dst, marker, oldMarker);
        wsprintfA(m, "screen %dx%d (from %s): could not put the %s loading screens in place (error %lu); the game's own",
                  w, h, from, kShapes[pick].name, err);
        Log(m);
        return 0;
    }
    char note[64];
    wsprintfA(note, "%s %lu", kShapes[pick].name, FileSize(dst));
    WriteText(marker, note);
    DeleteFileA(oldMarker);
    wsprintfA(m, "screen %dx%d (from %s): put the %s loading screens in place", w, h, from, kShapes[pick].name);
    Log(m);
    return 1;
}

/* The loading bar. The client draws it from a table of {texture, is fill, cx, cy, w, h} in screen fractions measured
 * from the bottom (VA 0x7FFD34, two entries of 0x18 bytes): the fill at 0.5, 0.075, 0.525 x 0.025 and the border
 * (the charred log in our packs) at 0.5, 0.075, 0.6 x 0.05; the fill is drawn from its left edge to progress * w.
 * With our set in place the bar spans the whole width at 0.09 of the height, flush with the bottom of the screen; the
 * fill runs wall to wall too, 0.052 high, the log's solid band, where
 * the cracks the fire shows through run (our packs carry a border drawn for exactly these proportions). */
#define BAR_FILL_RVA   0x3FFD3C   /* cx, cy, w, h of Loading-BarFill */
#define BAR_BORDER_RVA 0x3FFD54   /* cx, cy, w, h of Loading-BarBorder */
static const unsigned char kFillStock[16]   = { 0x00, 0x00, 0x00, 0x3F, 0x9A, 0x99, 0x99, 0x3D,   /* 0.5, 0.075 */
                                                0x66, 0x66, 0x06, 0x3F, 0xCD, 0xCC, 0xCC, 0x3C }; /* 0.525, 0.025 */
static const unsigned char kBorderStock[16] = { 0x00, 0x00, 0x00, 0x3F, 0x9A, 0x99, 0x99, 0x3D,   /* 0.5, 0.075 */
                                                0x9A, 0x99, 0x19, 0x3F, 0xCD, 0xCC, 0x4C, 0x3D }; /* 0.6, 0.05 */
static const unsigned char kFillWide[16]    = { 0x00, 0x00, 0x00, 0x3F, 0xEC, 0x51, 0x38, 0x3D,   /* 0.5, 0.045 */
                                                0x00, 0x00, 0x80, 0x3F, 0xF4, 0xFD, 0x54, 0x3D }; /* 1.0, 0.052 */
static const unsigned char kBorderWide[16]  = { 0x00, 0x00, 0x00, 0x3F, 0xEC, 0x51, 0x38, 0x3D,   /* 0.5, 0.045 */
                                                0x00, 0x00, 0x80, 0x3F, 0xEC, 0x51, 0xB8, 0x3D }; /* 1.0, 0.09 */
/* A pack whose fire is revealed rather than stretched (see RevealBar) gets a fill the size of the log itself, so the
 * cracks can burn from its top edge to its bottom one. */
#define kFillReveal kBorderWide

/* Read only, before anything is put in place: is this the client CinderLoad was made for? Both places it changes must
 * hold the stock bytes (or ours, from an earlier start in this process). */
static int Readable(const unsigned char *at, int n)
{
    MEMORY_BASIC_INFORMATION mbi;
    return VirtualQuery(at, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
           (const unsigned char *)mbi.BaseAddress + mbi.RegionSize >= at + n;
}
static int ClientIsOurs(void)
{
    const unsigned char *base = (const unsigned char *)GetModuleHandleA(NULL);
    const unsigned char *code = base + PATCH_RVA, *fill = base + BAR_FILL_RVA, *border = base + BAR_BORDER_RVA;
    if (!Readable(code, (int)sizeof kStock) || !Readable(fill, (int)(border + 16 - fill))) return 0;
    int codeOk = Same(code, kStock, sizeof kStock) ||
                 (Same(code, kWide, sizeof kWide) && Same(code + 5, kStock + 5, (int)sizeof kStock - 5));
    int barOk = (Same(fill, kFillStock, 16) && Same(border, kBorderStock, 16)) ||
                ((Same(fill, kFillWide, 16) || Same(fill, kFillReveal, 16)) && Same(border, kBorderWide, 16));
    return codeOk && barOk;
}

static void WideBar(int reveal)
{
    unsigned char *base = (unsigned char *)GetModuleHandleA(NULL);
    unsigned char *fill = base + BAR_FILL_RVA, *border = base + BAR_BORDER_RVA;
    const unsigned char *want = reveal ? kFillReveal : kFillWide;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(fill, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) ||
        (unsigned char *)mbi.BaseAddress + mbi.RegionSize < border + 16) {
        Log("bar left as it is: its size table could not be read");
        return;
    }
    if (Same(fill, want, 16) && Same(border, kBorderWide, 16)) {
        Log("bar already full width: nothing to do");
        return;
    }
    int ours = (Same(fill, kFillWide, 16) || Same(fill, kFillReveal, 16)) && Same(border, kBorderWide, 16);
    if (!ours && (!Same(fill, kFillStock, 16) || !Same(border, kBorderStock, 16))) {
        Log("bar left as it is: its size table holds other numbers, not the client this was made for");
        return;
    }
    DWORD old;
    if (!VirtualProtect(fill, (SIZE_T)(border + 16 - fill), PAGE_READWRITE, &old)) {
        Log("bar left as it is: its size table could not be made writable");
        return;
    }
    for (int i = 0; i < 16; i++) {
        fill[i] = want[i];
        border[i] = kBorderWide[i];
    }
    VirtualProtect(fill, (SIZE_T)(border + 16 - fill), old, &old);
    Log(Same(fill, want, 16) && Same(border, kBorderWide, 16)
            ? (reveal ? "bar on: the whole width, flush with the bottom, its fire as tall as the log (its size table, in "
                        "memory only)"
                      : "bar on: the whole width, flush with the bottom (its size table, in memory only)")
            : "bar left as it is: the numbers read back differ");
}

static int Apply(void)
{
    unsigned char *at = (unsigned char *)GetModuleHandleA(NULL) + PATCH_RVA;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(at, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) ||
        (unsigned char *)mbi.BaseAddress + mbi.RegionSize < at + sizeof kStock) {
        Log("switch left off: the code there could not be read");
        return 0;
    }
    if (Same(at, kWide, sizeof kWide) && Same(at + 5, kStock + 5, (int)sizeof kStock - 5)) {
        Log("switch already on: nothing to do");
        return 1;
    }
    if (!Same(at, kStock, sizeof kStock)) {
        char m[160];
        wsprintfA(m, "switch left off: other bytes at RVA 0x%X (%02X %02X %02X %02X %02X), not the client this was made for",
                  PATCH_RVA, at[0], at[1], at[2], at[3], at[4]);
        Log(m);
        return 0;
    }
    DWORD old;
    if (!VirtualProtect(at, sizeof kWide, PAGE_EXECUTE_READWRITE, &old)) {
        Log("switch left off: the code could not be made writable");
        return 0;
    }
    for (int i = 0; i < (int)sizeof kWide; i++)
        at[i] = kWide[i];
    VirtualProtect(at, sizeof kWide, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, sizeof kWide);
    int on = Same(at, kWide, sizeof kWide);
    Log(on ? "switch on: loading screens fill the screen (5 bytes at RVA 0x6AC4, in memory only)"
           : "switch left off: the bytes read back differ");
    return on;
}

/* The switch failed after our screens went in (the code could not be made writable): take them out again, before the
 * game opens its archives, so it never shows widened screens squeezed into 4:3. */
static void TakeOutAfterAll(void)
{
    char dst[MAX_PATH], marker[MAX_PATH], oldMarker[MAX_PATH];
    if (!Join(dst, g_dir, "Data\\patch-~.MPQ") || !Join(marker, g_dir, "Data\\patch-~.cinderload") ||
        !Join(oldMarker, g_dir, "Data\\CinderLoad\\installed.txt"))
        return;
    if (MarkerMatches(marker, dst)) TakeOut(dst, marker, oldMarker);
    Log("switch off, so our loading screens were taken out again: the game's own");
}

/* ---- the fire sound ----
 * Read from WoW.exe (1.12.1, build 5875) on 2 Oct 2026:
 * - The loading screen is a layer the client keeps at VA 0x882BE0: made when a loading screen begins (RVA 0x6800),
 *   released and set to 0 when it ends (RVA 0x7E80), and the client's own "is a loading screen up?" (RVA 0x7E70) is
 *   that pointer being non-zero. The sound plays while it is, and never otherwise.
 * - The bar's fill is the float at VA 0x882BE4, worked out each frame by RVA 0x6920 from the three loading stages and
 *   held to 0..1; the bar is drawn from it (RVA 0x7217: fill width = it * the fill's width). RVA 0x6920 first stores
 *   0 there and then the sum, so a read from another thread can catch the 0: FireFill keeps the largest.
 * - The client's sound engine is up while the byte at VA 0xCF55E8 is non-zero: set after FSOUND_Init (RVA 0x3A4797),
 *   tested before FSOUND_Close is called (RVA 0x3A4A59, the call at RVA 0x3A4A89), cleared after it (RVA 0x3A4AB6).
 * Each of those places must hold exactly these bytes when CinderLoad starts, or there is no sound at all and the log
 * says which one differed; the loading screens and the bar work the same either way.
 *
 * A thread of ours reads those three numbers every 20 ms; nothing in the client is hooked or changed for the sound.
 * While a loading screen shows it plays one stereo stream through the client's fmod.dll (FMOD 3.75), a stream FMOD
 * asks us to fill (FSOUND_Stream_Create): every sample is mixed here (fire.h), from loops built once from the client's
 * own sound files, so the equal-power pan does not depend on how FMOD pans. The thread calls FMOD only to start the
 * stream as a loading screen comes up and to close it once it has faded out after the screen has gone, and only while
 * the client's sound engine is up: with the sound off in the game, or the engine closed, it makes no FMOD call at all.
 * FMOD's own master volume (the game's Master Volume) applies to it as to every sound the game plays. */
#define FIRE_LAYER_RVA    0x482BE0   /* DWORD: the loading screen's layer, 0 when none shows */
#define FIRE_PROGRESS_RVA 0x482BE4   /* float: the bar's fill, 0..1 */
#define FIRE_SOUNDUP_RVA  0x8F55E8   /* BYTE: non-zero while the client's sound engine is up */
#define FIRE_POLL_MS      20
#define FIRE_FADE_WAIT_MS 1500       /* after a screen has gone: close the stream once silent, or after this at most */
#define FIRE_BLOCK_FRAMES 4096       /* what FMOD asks us to fill at a time: 93 ms at 44.1 kHz */
#define FIRE_RATE_MAX     48000      /* a faster mixer gets the sound at this rate, which FMOD then takes up */
#define FIRE_SCREEN_LOGS  10         /* one line per loading screen, for the first this many of a session */

static const struct { DWORD rva; int n; unsigned char bytes[26]; const char *what; } kFireCode[] = {
    { 0x007E70, 14, { 0x8B, 0x0D, 0xE0, 0x2B, 0x88, 0x00, 0x33, 0xC0, 0x85, 0xC9, 0x0F, 0x95, 0xC0, 0xC3 },
      "the loading screen check" },                           /* mov ecx,[882BE0]; xor eax,eax; test; setne al; ret */
    { 0x007E80, 25, { 0x8B, 0x0D, 0xE0, 0x2B, 0x88, 0x00, 0x57, 0x33, 0xFF, 0x3B, 0xCF, 0x74, 0x05, 0xE8, 0x3E, 0x30,
                      0x01, 0x00, 0x56, 0x89, 0x3D, 0xE0, 0x2B, 0x88, 0x00 },
      "the loading screen's end" },                           /* ... mov [882BE0],edi (0) */
    { 0x006920, 16, { 0xD9, 0x05, 0x7C, 0xFD, 0x7F, 0x00, 0xC7, 0x05, 0xE4, 0x2B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00 },
      "the bar's progress" },                                 /* fld [7FFD7C]; mov dword [882BE4],0 */
    { 0x007217, 6, { 0xD9, 0x05, 0xE4, 0x2B, 0x88, 0x00 }, "the bar's drawing" },   /* fld dword [882BE4] */
    { 0x3A4797, 6, { 0x88, 0x1D, 0xE8, 0x55, 0xCF, 0x00 }, "the sound engine's start" },   /* mov [CF55E8],bl */
    { 0x3A4A59, 7, { 0xA0, 0xE8, 0x55, 0xCF, 0x00, 0x84, 0xC0 }, "the sound engine's close" },   /* mov al,[CF55E8]; test */
    { 0x3A4A82, 12, { 0x6A, 0xFD, 0xE8, 0xBB, 0x90, 0x05, 0x00, 0xE8, 0xB0, 0x90, 0x05, 0x00 },
      "the sound engine's close" },                           /* push -3; call; call FSOUND_Close */
    { 0x3A4AB6, 7, { 0xC6, 0x05, 0xE8, 0x55, 0xCF, 0x00, 0x00 }, "the sound engine's close" },   /* mov byte [CF55E8],0 */
};
#define FIRE_CODE_COUNT ((int)(sizeof kFireCode / sizeof kFireCode[0]))

/* -1 when every place holds its bytes, else the index of the first that does not. */
static int FireCodeDiffers(const unsigned char *base)
{
    for (int i = 0; i < FIRE_CODE_COUNT; i++)
        if (!Readable(base + kFireCode[i].rva, kFireCode[i].n) || !Same(base + kFireCode[i].rva, kFireCode[i].bytes, kFireCode[i].n))
            return i;
    return -1;
}

/* FMOD 3.75 (the client's fmod.dll), by decorated name; its callbacks are __stdcall like its calls (the client's own
 * file callbacks for FMOD all return with ret N). */
#define FSOUND_FREE       (-1)
#define FSOUND_STEREOPAN  (-1)
#define FSOUND_16BITS     0x00000010u
#define FSOUND_STEREO     0x00000040u
#define FSOUND_SIGNED     0x00000100u
#define FSOUND_2D         0x00002000u
#define FIRE_PRIORITY     255         /* a game sound must not take the channel while the screen shows */
typedef signed char (WINAPI *fn_StreamCallback)(void *stream, void *buff, int len, void *user);
static struct {
    void *(WINAPI *Stream_Create)(fn_StreamCallback cb, int lenbytes, unsigned int mode, int rate, void *user);
    void *(WINAPI *Stream_GetSample)(void *stream);
    signed char (WINAPI *Sample_SetDefaults)(void *sample, int freq, int vol, int pan, int pri);
    int (WINAPI *Stream_PlayEx)(int channel, void *stream, void *dsp, signed char paused);
    signed char (WINAPI *SetPriority)(int channel, int pri);
    signed char (WINAPI *SetReserved)(int channel, signed char reserved);
    signed char (WINAPI *SetPan)(int channel, int pan);
    signed char (WINAPI *SetVolume)(int channel, int vol);
    signed char (WINAPI *SetPaused)(int channel, signed char paused);
    signed char (WINAPI *StopSound)(int channel);
    signed char (WINAPI *Stream_Close)(void *stream);
    int (WINAPI *GetOutputRate)(void);
    float (WINAPI *GetVersion)(void);
    int (WINAPI *GetError)(void);
} F;

static void *Proc(HMODULE h, const char *name) { return (void *)GetProcAddress(h, name); }
static int FireResolve(void)
{
    HMODULE h = GetModuleHandleA("fmod.dll");
    if (!h) return 0;
    F.Stream_Create = Proc(h, "_FSOUND_Stream_Create@20");
    F.Stream_GetSample = Proc(h, "_FSOUND_Stream_GetSample@4");
    F.Sample_SetDefaults = Proc(h, "_FSOUND_Sample_SetDefaults@20");
    F.Stream_PlayEx = Proc(h, "_FSOUND_Stream_PlayEx@16");
    F.SetPriority = Proc(h, "_FSOUND_SetPriority@8");
    F.SetReserved = Proc(h, "_FSOUND_SetReserved@8");
    F.SetPan = Proc(h, "_FSOUND_SetPan@8");
    F.SetVolume = Proc(h, "_FSOUND_SetVolume@8");
    F.SetPaused = Proc(h, "_FSOUND_SetPaused@8");
    F.StopSound = Proc(h, "_FSOUND_StopSound@4");
    F.Stream_Close = Proc(h, "_FSOUND_Stream_Close@4");
    F.GetOutputRate = Proc(h, "_FSOUND_GetOutputRate@0");
    F.GetVersion = Proc(h, "_FSOUND_GetVersion@0");
    F.GetError = Proc(h, "_FSOUND_GetError@0");
    return F.Stream_Create && F.Stream_PlayEx && F.SetPan && F.SetVolume && F.SetPaused && F.StopSound &&
           F.Stream_Close && F.GetVersion;
}

static struct {
    const unsigned char *base;
    FireMix mix;                       /* the callback's alone while a stream is open; the thread's otherwise */
    volatile LONG gate, fillBits, quiet;
    FireFill fill;
    void *stream;
    int channel, playing, built, screens, complaints;
    DWORD since, gone;
    float reached;
    int steps;                         /* times the bar moved this screen, as seen every FIRE_POLL_MS: the zoom idea's
                                          question is how often the game redraws a loading screen, which it does
                                          when the bar moves, so this is a lower bound on that */
} g_fire;

static signed char WINAPI FireCallback(void *stream, void *buff, int len, void *user)
{
    (void)stream; (void)user;
    union { LONG bits; float f; } fill;
    fill.bits = g_fire.fillBits;
    int frames = len > 0 ? len / 4 : 0;
    int on = FireRender(&g_fire.mix, (int)g_fire.gate, fill.f, (short *)buff, frames);
    for (int i = frames * 4; i < len; i++) ((unsigned char *)buff)[i] = 0;
    InterlockedExchange(&g_fire.quiet, !on);
    return 1;
}
static void FirePublish(float fill)
{
    union { LONG bits; float f; } v;
    v.f = fill;
    InterlockedExchange(&g_fire.fillBits, v.bits);
}

/* The client's archives, read with plain file calls on this thread (mpq.c), never through the client's own. */
static void *IoAlloc(void *ctx, unsigned size) { (void)ctx; return HeapAlloc(GetProcessHeap(), 0, size ? size : 1); }
static void IoRelease(void *ctx, void *p) { (void)ctx; if (p) HeapFree(GetProcessHeap(), 0, p); }
static void *IoOpen(void *ctx, const char *archive)
{
    (void)ctx;
    char path[MAX_PATH];
    if (!Join(path, g_dir, "Data\\") || lstrlenA(path) + lstrlenA(archive) >= MAX_PATH) return NULL;
    lstrcatA(path, archive);
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return h == INVALID_HANDLE_VALUE ? NULL : (void *)h;
}
static unsigned IoReadAt(void *ctx, void *fh, unsigned long long off, void *buf, unsigned len)
{
    (void)ctx;
    unsigned done = 0;
    while (done < len) {
        OVERLAPPED ov;
        DWORD got = 0;
        unsigned long long at = off + done;
        ov.Internal = 0; ov.InternalHigh = 0; ov.Offset = (DWORD)at; ov.OffsetHigh = (DWORD)(at >> 32); ov.hEvent = NULL;
        if (!ReadFile((HANDLE)fh, (unsigned char *)buf + done, len - done, &got, &ov) || !got) break;
        done += got;
    }
    return done;
}
static unsigned long long IoSize(void *ctx, void *fh)
{
    (void)ctx;
    DWORD hi = 0, lo = GetFileSize((HANDLE)fh, &hi);
    return lo == INVALID_FILE_SIZE ? 0 : ((unsigned long long)hi << 32) | lo;
}
static void IoClose(void *ctx, void *fh) { (void)ctx; CloseHandle((HANDLE)fh); }
static const mpq_io kIo = { IoAlloc, IoRelease, IoOpen, IoReadAt, IoSize, IoClose, NULL };

/* The archives the loops are read from, the one the client lets win first: the six are the original files in the
 * client's sound.MPQ, and UndeadFireLarge the newer one in its patch.MPQ, as in the mock. A pack in a patch-?.MPQ that
 * changes a campfire's sound changes the campfire, not this fire: the sound stays the one that was approved. */
static const char *const kFireArchives[] = { "patch.MPQ", "sound.MPQ" };
#define FIRE_ARCHIVES ((int)(sizeof kFireArchives / sizeof kFireArchives[0]))

static void FireFree(void)
{
    for (int i = 0; i < FIRE_LOOPS; i++) {
        if (g_fire.mix.loop[i]) HeapFree(GetProcessHeap(), 0, g_fire.mix.loop[i]);
        g_fire.mix.loop[i] = NULL;
        g_fire.mix.len[i] = 0;
    }
}

/* The six loops at the mixer's rate, from the client's own files. Only while no stream is open. 1 when at least one
 * could be made (a missing one is logged and the rest play), 0 when none. */
static int FireBuild(int rate)
{
    char m[400];
    int made = 0;
    FireFree();
    FireSetRate(&g_fire.mix, rate);
    for (int i = 0; i < FIRE_LOOPS; i++) {
        const char *file = kFireLoops[i].path, *why = "in none of the client's archives";
        for (const char *p = kFireLoops[i].path; *p; p++)
            if (*p == '\\') file = p + 1;
        for (int a = 0; !g_fire.mix.loop[i] && a < FIRE_ARCHIVES; a++) {
            const char *name = kFireArchives[a];
            unsigned char *wav = NULL;
            unsigned len = 0, frames = 0;
            int r = mpq_read(&kIo, name, kFireLoops[i].path, &wav, &len), srate = 0, channels = 0;
            const unsigned char *data = NULL;
            if (r == MPQ_E_NOT_FOUND || r == MPQ_E_NO_ARCHIVE || r == MPQ_E_EMPTY) continue;
            if (r != MPQ_OK) {
                wsprintfA(m, "fire sound: %s in %s could not be read (%s); trying older copies", file, name, mpq_error_text(r));
                Log(m);
                continue;
            }
            float *src = NULL, *out = NULL;
            unsigned period = 0;
            if (!FireWavInfo(wav, len, &srate, &channels, &data, &frames)) why = "not a 16-bit PCM sound";
            else if (!(src = HeapAlloc(GetProcessHeap(), 0, frames * sizeof(float))) ||
                     !(out = HeapAlloc(GetProcessHeap(), 0, (FireResampledLength(frames, srate, rate) + 1) * sizeof(float))))
                why = "out of memory";
            else {
                FireWavRead(data, channels, frames, src);
                period = FireMakeLoop(src, frames, srate, rate, kFireLoops[i].offset * (rate / 100) / (FIRE_MOCK_RATE / 100), out);
                if (!period) why = "too short to loop";
            }
            if (src) HeapFree(GetProcessHeap(), 0, src);
            HeapFree(GetProcessHeap(), 0, wav);
            if (period) {
                g_fire.mix.loop[i] = out;
                g_fire.mix.len[i] = period;
                made++;
            } else {
                if (out) HeapFree(GetProcessHeap(), 0, out);
                wsprintfA(m, "fire sound: %s in %s is %s; trying older copies", file, name, why);
                Log(m);
                why = "in no archive as a sound it can use";
            }
        }
        if (!g_fire.mix.loop[i]) {
            wsprintfA(m, "fire sound: %s is %s, so the fire plays without it", file, why);
            Log(m);
        }
    }
    if (made) wsprintfA(m, "fire sound: %d of %d loops made from the client's own files, at %d Hz", made, FIRE_LOOPS, rate);
    else wsprintfA(m, "fire sound off: none of its %d loops could be made from the client's files", FIRE_LOOPS);
    Log(m);
    return made > 0;
}

static int FireRate(void)
{
    int rate = F.GetOutputRate ? F.GetOutputRate() : 0;
    if (rate < 8000 || rate > 192000) rate = 44100;
    return rate > FIRE_RATE_MAX ? FIRE_RATE_MAX : rate;
}

/* A loading screen has come up: one stream, started paused, then let go. 1 when it plays. */
static int FireOpen(float fill)
{
    char m[160];
    int rate = FireRate();
    if (g_fire.built && rate != g_fire.mix.rate) g_fire.built = 0;      /* the engine was restarted at another rate */
    if (!g_fire.built) g_fire.built = FireBuild(rate) ? 1 : -1;
    if (g_fire.built < 0) return 0;
    FireReset(&g_fire.mix);
    FireFillStart(&g_fire.fill, fill);
    FirePublish(g_fire.fill.fill);
    InterlockedExchange(&g_fire.gate, 1);
    InterlockedExchange(&g_fire.quiet, 0);
    void *st = F.Stream_Create(FireCallback, FIRE_BLOCK_FRAMES * 4, FSOUND_16BITS | FSOUND_SIGNED | FSOUND_STEREO | FSOUND_2D,
                               g_fire.mix.rate, NULL);
    if (!st) {
        if (g_fire.complaints++ >= 3) return 0;
        wsprintfA(m, "fire sound: the sound engine would not make the stream (FMOD error %d); trying at the next screen",
                  F.GetError ? F.GetError() : -1);
        Log(m);
        return 0;
    }
    void *sample = F.Stream_GetSample ? F.Stream_GetSample(st) : NULL;
    if (sample && F.Sample_SetDefaults) F.Sample_SetDefaults(sample, -1, -1, -1, FIRE_PRIORITY);
    int ch = F.Stream_PlayEx(FSOUND_FREE, st, NULL, 1);
    if (ch < 0) {
        F.Stream_Close(st);
        if (g_fire.complaints++ < 3) Log("fire sound: no free sound channel; trying at the next screen");
        return 0;
    }
    if (F.SetPriority) F.SetPriority(ch, FIRE_PRIORITY);
    if (F.SetReserved) F.SetReserved(ch, 1);
    F.SetPan(ch, FSOUND_STEREOPAN);             /* our own left and right, at full level: the pan is mixed in */
    F.SetVolume(ch, 255);
    F.SetPaused(ch, 0);
    g_fire.stream = st;
    g_fire.channel = ch;
    return 1;
}
static void FireClose(void)
{
    if (g_fire.channel >= 0) {
        if (F.SetReserved) F.SetReserved(g_fire.channel, 0);
        F.StopSound(g_fire.channel);
    }
    F.Stream_Close(g_fire.stream);
    g_fire.stream = NULL;
    g_fire.channel = -1;
}

#define BAR_SHOWN_MAX 96               /* the longest is 60: ", its fire the pack's orange, its wallpaper CinderWall64.blp" */
static void BarShownSince(char *out, int cap);
static void DriftScreenGone(void);
static void OverlayStart(void);
static void BadgeLog(char *m);           /* the marks' (below): where a map screen's class came from, into the log */
static volatile LONG g_screenWatch;      /* 1 once the fire's worker watches for screens ending */
static volatile LONG g_dispNew = 1;      /* the bar as shown: the next frame begins a new screen */
static volatile LONG g_frmCount, g_frmGap, g_frmGapFrom, g_frmGapTo, g_frmDips, g_frmShown;   /* this screen's, for the log */
#define HITCH_KEEP 6
static volatile LONG g_hitchN, g_hitchMs[HITCH_KEEP], g_hitchPct[HITCH_KEEP];   /* gaps between frames over HITCH_MS */
static volatile LONG g_stallN, g_stallMs[HITCH_KEEP], g_stallPct[HITCH_KEEP];   /* the bar still over STALL_MS, frames on */
static void HitchLog(char *m);           /* the bar as shown's (below): this screen's hitches and stalls, into the log */

/* One look at the client, every FIRE_POLL_MS. */
static void FireStep(DWORD now)
{
    const unsigned char *base = g_fire.base;
    int screen = *(volatile const unsigned int *)(base + FIRE_LAYER_RVA) != 0;
    float read = *(volatile const float *)(base + FIRE_PROGRESS_RVA);
    int up = *(volatile const unsigned char *)(base + FIRE_SOUNDUP_RVA) != 0;
    char m[220];
    if (!g_screenWatch) InterlockedExchange(&g_screenWatch, 1);   /* screens ending are watched for from now on */

    if (!up) {                                   /* the engine is closed: it took our stream with it, so no call */
        if (g_fire.stream) Log("fire sound: the game closed its sound engine; the stream went with it");
        g_fire.stream = NULL;
        g_fire.channel = -1;
        g_fire.playing = 0;
        return;
    }
    if (!g_fire.built && !screen)                /* made once the engine is up, before the first loading screen */
        g_fire.built = FireBuild(FireRate()) ? 1 : -1;
    if (screen && !g_fire.playing) {             /* a loading screen has come up, or come back while fading */
        if (!g_fire.stream && !FireOpen(read)) {
            g_fire.playing = -1;                 /* not this screen */
            return;
        }
        if (g_fire.stream) {
            FireFillStart(&g_fire.fill, read);
            FirePublish(g_fire.fill.fill);
            InterlockedExchange(&g_fire.gate, 1);
        }
        g_fire.playing = 1;
        g_fire.since = now;
        g_fire.reached = g_fire.fill.fill;
        g_fire.steps = 0;
        return;
    }
    if (screen && g_fire.playing > 0) {
        float fill = FireFillRead(&g_fire.fill, read);
        FirePublish(fill);
        if (fill > g_fire.reached) { g_fire.reached = fill; g_fire.steps++; }
        return;
    }
    if (!screen && g_fire.playing) {             /* the screen has gone: fade out */
        if (g_fire.playing > 0 && g_fire.screens < FIRE_SCREEN_LOGS) {
            g_fire.screens++;
            DWORD ms = now - g_fire.since;
            char bar[BAR_SHOWN_MAX];
            BarShownSince(bar, (int)sizeof bar);
            wsprintfA(m, "loading screen: the fire burned for %lu.%lu s, the bar reached %d%% in %d steps%s", ms / 1000,
                      ms % 1000 / 100, (int)(g_fire.reached * 100 + 0.5f), g_fire.steps, bar);
            Log(m);
            BadgeLog(m);
            if (g_frmCount > 0) {
                LONG gap = g_frmGap;
                wsprintfA(m, "  its frames: %ld drawn, the longest pause %ld.%ld s (the bar at %ld%%, then %ld%%), %ld "
                          "dips, the bar as shown last at %ld%%", (long)g_frmCount, (long)(gap / 1000),
                          (long)(gap % 1000 / 100), (long)g_frmGapFrom, (long)g_frmGapTo, (long)g_frmDips,
                          (long)g_frmShown);
                Log(m);
                HitchLog(m);
            }
        }
        InterlockedExchange(&g_fire.gate, 0);
        g_fire.playing = 0;
        g_fire.gone = now;
        DriftScreenGone();
        return;
    }
    if (g_fire.stream && (g_fire.quiet || now - g_fire.gone > FIRE_FADE_WAIT_MS)) FireClose();
}

static DWORD WINAPI FireThread(LPVOID arg)
{
    (void)arg;
    for (int tries = 0; !FireResolve(); tries++) {
        if (tries == 30) {
            Log("fire sound off: the game's fmod.dll is not there or lacks the calls it needs");
            return 0;
        }
        Sleep(1000);
    }
    float v = F.GetVersion();
    if (v < 3.7f || v >= 3.8f) {                 /* the stream callback's form is FMOD 3.7's */
        char m[120];
        wsprintfA(m, "fire sound off: the game's fmod.dll is version %d.%02d, not the 3.7x this was made for",
                  (int)v, (int)(v * 100 + 0.5f) % 100);
        Log(m);
        return 0;
    }
    g_fire.channel = -1;
    for (;;) {
        FireStep(GetTickCount());
        Sleep(FIRE_POLL_MS);
    }
}

/* At start, read only: the sound's thread is started when every place it relies on holds the bytes it was made for. */
static void FireStart(void)
{
    const unsigned char *base = (const unsigned char *)GetModuleHandleA(NULL);
    int bad = FireCodeDiffers(base);
    char m[300];
    if (bad >= 0) {
        int len = wsprintfA(m, "fire sound off: other bytes at RVA 0x%lX (%s) than in the client it was made for, so no "
                            "sound; there:", (unsigned long)kFireCode[bad].rva, kFireCode[bad].what);
        if (Readable(base + kFireCode[bad].rva, kFireCode[bad].n))
            for (int i = 0; i < kFireCode[bad].n; i++)
                len += wsprintfA(m + len, " %02X", base[kFireCode[bad].rva + (DWORD)i]);
        Log(m);
        return;
    }
    if (!Readable(base + FIRE_LAYER_RVA, 8) || !Readable(base + FIRE_SOUNDUP_RVA, 1)) {
        Log("fire sound off: the loading screen's numbers could not be read");
        return;
    }
    g_fire.base = base;
    g_fire.channel = -1;
    HANDLE t = CreateThread(NULL, 0, FireThread, NULL, 0, NULL);
    if (!t) {
        Log("fire sound off: its thread could not be started");
        return;
    }
    CloseHandle(t);
    Log("fire sound on: the bar will burn while loading screens show");
}

/* ---- the fire bar ----
 * Each loading screen's bar burns in the colour of its picture's fire. Beside the orange Loading-BarFill.blp and
 * Loading-BarBorder.blp the game loads for every screen, our pack carries Loading-BarFill-<family>.blp and
 * Loading-BarBorder-<family>.blp for each fire family the owner's art kit found (fel, arcane, aqua, ...), and
 * CinderLoad\bars.txt: one line per screen, "<the screen file's base name> <family>", and "default <family>" for the
 * screens it does not list (make_pack.py writes it).
 *
 * Read from WoW.exe (1.12.1, build 5875) on 2 Oct 2026:
 * - The DWORD at VA 0x82F00C is the map the loading screen is for, stored just before a screen begins (RVA 0x67E3, and
 *   RVA 0x72D3 on the other way in); -1 when there is none, and the game then shows Interface\Glues\loading.
 * - The game picks the picture at RVA 0x6E20, on the screen's first frame: Map.dbc's record for that map, its
 *   LoadingScreenID (+0x98), LoadingScreens.dbc's record for that, and its file name (+8); anything missing on the way
 *   is the default screen. BarScreen walks the same tables the same way to the same name. (A third way into begin,
 *   RVA 0x7DE0, stores no map but -1, so the screen is for the map stored last; the picture is then chosen from that
 *   same number, and so is the bar. One difference: the game also shows the default screen when the picture's file
 *   is in none of its archives, which BarScreen does not ask; every screen our pack carries is there.)
 * - The bar's two textures are loaded inside the screen's begin (RVA 0x6800), once per screen, and released at its end
 *   (RVA 0x7E80). For each entry of the bar's table (VA 0x7FFD34) with no texture yet, ecx = the entry's path and a call
 *   from RVA 0x6891 to the texture loader at VA 0x449D90, which takes edx and three stack arguments too and pops them.
 * The map is stored microseconds before the textures are loaded, on the game's own thread: a thread of ours cannot get
 * in between. So that one call is the one change: it goes to BarStub, which asks BarPath which texture to load and
 * goes on to the loader with the stack and every register as the game left them, but ecx. Every place above must hold
 * exactly these bytes, or the call is left alone and the log says which differed; the screens, the bar's size and the
 * fire sound work the same either way. The client's tables are only read, on the thread the game reads them on.
 */
#define BAR_CALL_RVA        0x006891   /* the call to the texture loader: E8 FA 34 04 00 */
#define BAR_LOAD_RVA        0x006863   /* the loop around it */
#define BAR_FILL_PATH_RVA   0x42F0A0   /* "Interface\Glues\LoadingBar\Loading-BarFill" */
#define BAR_BORDER_PATH_RVA 0x42F070   /* "Interface\Glues\LoadingBar\Loading-BarBorder" */
#define BAR_MAP_VA          0x82F00C   /* int: the loading screen's map, -1 for none */
#define BAR_MAPS_VA         0xC0DAA8   /* Map.dbc's records by id (an array of pointers) */
#define BAR_MAPS_MAX_VA     0xC0DAAC   /* its largest id */
#define BAR_SCREENS_VA      0xC0DB0C   /* LoadingScreens.dbc's records by id */
#define BAR_SCREENS_MAX_VA  0xC0DB10   /* its largest id */
#define BAR_MAP_SCREEN      0x98       /* in a Map record: its LoadingScreenID */
#define BAR_SCREEN_FILE     8          /* in a LoadingScreens record: its file name */
#define BAR_FAMILIES        8          /* fire families one pack may carry */
#define BAR_FAMILY_MAX      15         /* letters and digits in a family's name */
#define BAR_SCREENS         256        /* screens bars.txt may name (the client has 45) */
#define BAR_NAME_MAX        47         /* characters in a screen's base name */
#define BAR_LINE_MAX        200        /* a longer line in bars.txt is skipped */
#define BAR_TEXT_MAX        65536      /* a bigger bars.txt is refused */
#define BAR_PATH_MAX        260        /* characters of a screen's file name read from the client */

static const char kBarFillPath[]   = "Interface\\Glues\\LoadingBar\\Loading-BarFill";
static const char kBarBorderPath[] = "Interface\\Glues\\LoadingBar\\Loading-BarBorder";
static const unsigned char kBarMapSet[] = { 0x8B, 0x55, 0xEC, 0x89, 0x15, 0x0C, 0xF0, 0x82, 0x00,   /* mov [82F00C],edx */
                                            0xE8, 0x0F, 0x00, 0x00, 0x00 };                     /* call begin (6800) */
static const unsigned char kBarMapSet2[] = { 0xA3, 0x0C, 0xF0, 0x82, 0x00 };                     /* mov [82F00C],eax */
static const unsigned char kBarPick[] = { 0xA1, 0x0C, 0xF0, 0x82, 0x00, 0x8D };                  /* mov eax,[82F00C] */
static const unsigned char kBarPickWalk[] = {
    0x7C, 0x72, 0x3B, 0x05, 0xAC, 0xDA, 0xC0, 0x00, 0x7F, 0x6A,             /* id < 0 or > [C0DAAC]: the default */
    0x8B, 0x15, 0xA8, 0xDA, 0xC0, 0x00, 0x8B, 0x04, 0x82, 0x85, 0xC0, 0x74, 0x5D,   /* map = [C0DAA8][id], or none */
    0x8B, 0x80, 0x98, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x7C, 0x53,             /* ls = map->LoadingScreenID, not < 0 */
    0x3B, 0x05, 0x10, 0xDB, 0xC0, 0x00, 0x7F, 0x4B,                         /* nor > [C0DB10] */
    0x8B, 0x0D, 0x0C, 0xDB, 0xC0, 0x00, 0x8B, 0x34, 0x81, 0x85, 0xF6, 0x74, 0x3E,   /* rec = [C0DB0C][ls], or none */
    0x8B, 0x56, 0x08, 0x52, 0xE8, 0x1F };                                   /* push rec->file name; call */
static const unsigned char kBarLoad[] = {
    0xBE, 0x34, 0xFD, 0x7F, 0x00, 0x83, 0x3F, 0x00, 0x75, 0x2B,             /* esi = the bar's table; a texture yet? */
    0x6A, 0x00, 0x6A, 0x01, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x01,
    0x8D, 0x4D, 0xFC, 0xE8, 0xFB, 0x40, 0x18, 0x00, 0x8B, 0x10,
    0x8B, 0x0E,                                                             /* ecx = the entry's path */
    0x6A, 0x00, 0x6A, 0x01, 0x52, 0x8D, 0x55, 0xD8,                         /* three arguments; edx = a local */
    0xE8, 0xFA, 0x34, 0x04, 0x00,                                           /* call the loader (449D90): ours */
    0x89 };
#define BAR_REL_AT (BAR_CALL_RVA + 1 - BAR_LOAD_RVA)   /* where in kBarLoad the call's rel32 sits */

static const struct { DWORD rva; int n; const unsigned char *bytes; const char *what; } kBarCode[] = {
    { 0x0067E3, (int)sizeof kBarMapSet, kBarMapSet, "the loading screen's map" },
    { 0x0072D3, (int)sizeof kBarMapSet2, kBarMapSet2, "the loading screen's map" },
    { 0x006E2C, (int)sizeof kBarPick, kBarPick, "the loading screen's picture" },
    { 0x006E52, (int)sizeof kBarPickWalk, kBarPickWalk, "the loading screen's picture" },
    { BAR_LOAD_RVA, (int)sizeof kBarLoad, kBarLoad, "the bar's textures being loaded" },
    { BAR_FILL_PATH_RVA, (int)sizeof kBarFillPath, (const unsigned char *)kBarFillPath, "the bar's fill texture" },
    { BAR_BORDER_PATH_RVA, (int)sizeof kBarBorderPath, (const unsigned char *)kBarBorderPath, "the bar's border texture" },
};
#define BAR_CODE_COUNT ((int)(sizeof kBarCode / sizeof kBarCode[0]))

/* What bars.txt says, with each family's two texture paths in the DLL's own memory, for BarPath to hand the game. */
typedef struct {
    int families, screens, fallback, skipped;          /* fallback: the "default" line's family, or -1 */
    struct { char name[BAR_FAMILY_MAX + 1]; int ok; char fill[80], border[80]; } family[BAR_FAMILIES];
    struct { char name[BAR_NAME_MAX + 1]; int family; } screen[BAR_SCREENS];
} BarTable;
static BarTable g_bars;
static volatile LONG g_barsOn;                         /* 1 once g_bars is complete and the call is ours */
static volatile LONG g_barShown;                       /* BarPath's last fill: 0 none since the worker looked, -1 the
                                                          pack's orange, else the family's index + 1 */

/* Called from the asm stub below, so neither may be static, nor dropped as unused. */
const char *WINAPI BarPath(const char *path) __attribute__((used));
__attribute__((used)) void *g_barLoader;               /* the game's texture loader, VA 0x449D90 */

#if defined(__i386__)
/* The game's call lands here with ecx = the path, edx = the loader's own argument and the loader's three arguments on
 * the stack above the return address. eax and edx are kept around the call to BarPath (stdcall: it pops its one
 * argument and keeps ebx, esi, edi and ebp, as the game's own code expects of a call), ecx becomes the path it
 * returns, and the jump reaches the loader with the stack exactly as the game set it: the loader returns straight to
 * the game, popping its three arguments itself. A naked function rather than asm at the top of the file: the PE
 * assembler has no way to switch to .text and back, and the compiler then places it in .text itself. */
__attribute__((naked, used)) static void BarStub(void)
{
    __asm__("pushl %eax\n\t"
            "pushl %edx\n\t"
            "pushl %ecx\n\t"
            "call _BarPath@4\n\t"
            "movl %eax, %ecx\n\t"
            "popl %edx\n\t"
            "popl %eax\n\t"
            "jmp *_g_barLoader\n\t");
}
#else
static void BarStub(void) {}                           /* the tests' stand-in, never called: its address is all they use */
#endif

/* A client address as a pointer. The client's code above holds absolute addresses, so with every place verified the
 * client sits at its own base and its addresses are real ones. The tests define this to read a fake image instead. */
#ifndef ClientAt
#define ClientAt(va) ((const unsigned char *)(ULONG_PTR)(va))
#endif
static int ClientWord(unsigned va, unsigned *out)
{
    const unsigned char *p = ClientAt(va);
    if (!p || !Readable(p, 4)) return 0;
    *out = *(volatile const unsigned int *)p;
    return 1;
}
/* A NUL-terminated string of the client's into out: its length, -1 when it cannot be read, -2 when it is longer than
 * size - 1. Checked page by page, so a string at the end of its block is read no further than its NUL. */
static int ClientText(unsigned va, char *out, int size)
{
    const unsigned char *p = ClientAt(va);
    if (!p) return -1;
    for (int i = 0; i < size; i++) {
        if ((i == 0 || ((ULONG_PTR)(p + i) & 0xFFF) == 0) && !Readable(p + i, 1)) return -1;
        if (!(out[i] = (char)p[i])) return i;
    }
    return -2;
}

static int BarSame(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
/* A path the game names a texture by, against one of its own: case does not matter, nor which slash. */
static int BarSamePath(const char *a, const char *b)
{
    for (;; a++, b++) {
        char x = *a == '/' ? '\\' : Lower(*a), y = *b == '/' ? '\\' : Lower(*b);
        if (x != y) return 0;
        if (!x) return 1;
    }
}

/* A screen's file as bars.txt names it: after the last \ or /, before the last dot, in lower case ("Interface\Glues\
 * LoadingScreens\LoadScreenDeadmines.blp" is "loadscreendeadmines"). 0 when that is empty or over BAR_NAME_MAX. */
static int BarBaseName(const char *s, int len, char *out)
{
    int from = 0, to = len;
    for (int i = 0; i < len; i++)
        if (s[i] == '\\' || s[i] == '/') from = i + 1;
    for (int i = len - 1; i >= from; i--)
        if (s[i] == '.') { to = i; break; }
    if (to - from < 1 || to - from > BAR_NAME_MAX) return 0;
    for (int i = from; i < to; i++) out[i - from] = Lower(s[i]);
    out[to - from] = 0;
    return 1;
}

/* One line of bars.txt, without its line end. Blank lines and comments (#) are passed over; a line that is not two
 * words, a screen's name and a family's (letters and digits), is skipped and counted. A later line for the same
 * screen wins. */
static void BarLine(BarTable *t, const char *s, int n)
{
    const char *word[3];
    int len[3], words = 0;
    for (int i = 0; i < n && words < 3;) {
        while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
        if (i >= n || s[i] == '#') break;
        word[words] = s + i;
        while (i < n && s[i] != ' ' && s[i] != '\t') {
            if ((unsigned char)s[i] < 0x20 || s[i] == 0x7F) { t->skipped++; return; }
            i++;
        }
        len[words] = (int)(s + i - word[words]);
        words++;
    }
    if (!words) return;
    char name[BAR_NAME_MAX + 1], family[BAR_FAMILY_MAX + 1];
    if (words != 2 || !BarBaseName(word[0], len[0], name) || len[1] > BAR_FAMILY_MAX) { t->skipped++; return; }
    for (int i = 0; i < len[1]; i++) {
        family[i] = Lower(word[1][i]);
        if (!((family[i] >= 'a' && family[i] <= 'z') || (family[i] >= '0' && family[i] <= '9'))) { t->skipped++; return; }
    }
    family[len[1]] = 0;
    int f = 0;
    while (f < t->families && !BarSame(t->family[f].name, family)) f++;
    if (f == t->families) {
        if (f == BAR_FAMILIES) { t->skipped++; return; }
        lstrcpyA(t->family[f].name, family);
        t->families++;
    }
    if (BarSame(name, "default")) { t->fallback = f; return; }
    int k = 0;
    while (k < t->screens && !BarSame(t->screen[k].name, name)) k++;
    if (k == t->screens) {
        if (k == BAR_SCREENS) { t->skipped++; return; }
        lstrcpyA(t->screen[k].name, name);
        t->screens++;
    }
    t->screen[k].family = f;
}
/* bars.txt, len bytes (not NUL-terminated), into t. LF or CRLF; a line over BAR_LINE_MAX is skipped. */
static void BarParse(BarTable *t, const char *text, unsigned len)
{
    unsigned char *z = (unsigned char *)t;
    for (unsigned i = 0; i < sizeof *t; i++) z[i] = 0;
    t->fallback = -1;
    for (unsigned at = 0; at < len;) {
        unsigned end = at;
        while (end < len && text[end] != '\n') end++;
        int n = (int)(end - at);
        if (n > BAR_LINE_MAX) t->skipped++;
        else BarLine(t, text + at, n > 0 && text[at + (unsigned)n - 1] == '\r' ? n - 1 : n);
        at = end + 1;
    }
}

/* The loading screen's picture as the game picks it (RVA 0x6E20), into name: its file's base name, "loading" for the
 * default screen. 0 when the file's name is longer than any bars.txt can list. */
static int BarScreen(char *name)
{
    unsigned id, idMax, maps, map, ls, lsMax, recs, rec, file;
    lstrcpyA(name, "loading");
    if (!ClientWord(BAR_MAP_VA, &id) || (int)id < 0 || !ClientWord(BAR_MAPS_MAX_VA, &idMax) || (int)id > (int)idMax ||
        !ClientWord(BAR_MAPS_VA, &maps) || !ClientWord(maps + id * 4, &map) || !map ||
        !ClientWord(map + BAR_MAP_SCREEN, &ls) || (int)ls < 0 || !ClientWord(BAR_SCREENS_MAX_VA, &lsMax) ||
        (int)ls > (int)lsMax || !ClientWord(BAR_SCREENS_VA, &recs) || !ClientWord(recs + ls * 4, &rec) || !rec ||
        !ClientWord(rec + BAR_SCREEN_FILE, &file))
        return 1;
    char path[BAR_PATH_MAX + 1];
    int n = ClientText(file, path, (int)sizeof path);
    if (n == -1) return 1;                             /* no name to be read there: as the default screen */
    return n >= 0 && BarBaseName(path, n, name);
}

/* The family whose textures this screen's bar gets, or -1 for the pack's orange ones. */
static int BarFamily(const BarTable *t)
{
    char name[BAR_NAME_MAX + 1];
    int f = t->fallback;
    if (BarScreen(name))
        for (int k = 0; k < t->screens; k++)
            if (BarSame(t->screen[k].name, name)) { f = t->screen[k].family; break; }
    return f >= 0 && t->family[f].ok ? f : -1;
}

/* ---- a random wallpaper on the continents (3 Oct 2026) ----
 * Loading into Eastern Kingdoms or Kalimdor, the screen shows a wallpaper drawn at random from the pack's
 * Interface\Glues\LoadingScreens\CinderWall01.blp, 02, ...: credited art the artists' owners cleared for it. Every other
 * map (dungeons, raids, battlegrounds) keeps the screen it always had. The game loads the bar's fill first, as a screen
 * begins (RVA 0x6891), and its picture a moment later, on the screen's first frame (RVA 0x6EBA, the same loader): so
 * the draw happens at the fill, the bar takes the wallpaper's fire, and the picture's load is then handed that same
 * wallpaper. Both calls reach BarPath through BarStub. */
/* ---- the world map zoom (3 Oct 2026, his design) ----
 * Loading into Eastern Kingdoms or Kalimdor, the picture is the pack's world map (CinderMap.blp: both continents in the
 * Great Sea, drawn for this screen's shape), and the view glides from the whole world down onto where the player
 * arrives as the bar fills (DriftStep). CinderLoad\map.txt says where each continent lies on the picture:
 *   map <0|1> <au> <bu> <av> <bv>   u = au + bu * y, v = av + bv * x, for a world position x, y in yards; the four
 *                                   as the hex of their float bits (no C runtime here to read decimals)
 *   end <yards>                     how tall a stretch of the world the view ends on
 *   pace <seconds>                  the zoom is never quicker than this, all the way in (his call, 4 Oct: a
 *                                   two-second hearth flew at him); 7 when not given
 * Only when both the picture and the table are in the pack, and the picture can move (drift.txt, the reveal). */
#define MAP_PATH        "Interface\\Glues\\LoadingScreens\\CinderMap.blp"
#define MAP_DEST_MAP_VA 0x88262C   /* int: the map SMSG_NEW_WORLD / LOGIN_VERIFY_WORLD put the player on (VA 0x401B00) */
#define MAP_DEST_X_VA   0x88268C   /* float: x there, in yards */
#define MAP_DEST_Y_VA   0x882690   /* float: y there */
#define MAP_TAU         0.15f      /* seconds: how smoothly the view follows, so a late position glides in */
#define MAP_BAR_TAU     0.15f      /* seconds: how closely the zoom follows the bar (a quick load is over in a second) */
static struct { int ok; struct { int ok; float au, bu, av, bv; } m[2]; float endYards, pace; } g_map;
static volatile LONG g_mapShown;  /* the screen showing has the world map */
static float g_viewU = 0.5f, g_viewV = 0.5f, g_viewH = 0.5f;     /* the world map's view, eased */
static volatile LONG g_driftResets;                             /* a fallback reset within a screen (the log says) */

static int MapSpace(char c) { return c == ' ' || c == '\t'; }
/* A hex number of up to 8 digits at text[*at], spaces before it skipped. */
static int MapHex(const char *s, int n, int *at, unsigned *out)
{
    while (*at < n && MapSpace(s[*at])) (*at)++;
    unsigned v = 0;
    int d = 0;
    for (; *at < n && d < 9; (*at)++, d++) {
        char c = s[*at];
        unsigned x = c >= '0' && c <= '9' ? (unsigned)(c - '0') : c >= 'a' && c <= 'f' ? (unsigned)(c - 'a' + 10)
                   : c >= 'A' && c <= 'F' ? (unsigned)(c - 'A' + 10) : 16u;
        if (x == 16u) break;
        v = v << 4 | x;
    }
    if (d < 1 || d > 8) return 0;
    *out = v;
    return 1;
}
static float MapBits(unsigned u) { union { unsigned u; float f; } c; c.u = u; return c.f; }
static int MapFinite(float f) { return f > -1e30f && f < 1e30f; }
static void MapParse(const char *s, unsigned len)
{
    unsigned char *z = (unsigned char *)&g_map;
    for (unsigned i = 0; i < sizeof g_map; i++) z[i] = 0;
    for (unsigned at = 0; at < len;) {
        unsigned end = at;
        while (end < len && s[end] != '\n') end++;
        const char *l = s + at;
        int n = (int)(end - at), i = 4;
        unsigned v[4], id;
        if (n > 4 && l[0] == 'm' && l[1] == 'a' && l[2] == 'p' && MapSpace(l[3]) && MapHex(l, n, &i, &id) && id <= 1 &&
            MapHex(l, n, &i, &v[0]) && MapHex(l, n, &i, &v[1]) && MapHex(l, n, &i, &v[2]) && MapHex(l, n, &i, &v[3])) {
            float au = MapBits(v[0]), bu = MapBits(v[1]), av = MapBits(v[2]), bv = MapBits(v[3]);
            if (MapFinite(au) && MapFinite(bu) && MapFinite(av) && MapFinite(bv) && bu != 0 && bv != 0) {
                g_map.m[id].au = au; g_map.m[id].bu = bu; g_map.m[id].av = av; g_map.m[id].bv = bv;
                g_map.m[id].ok = 1;
            }
        } else if (n > 4 && ((l[0] == 'e' && l[1] == 'n' && l[2] == 'd' && MapSpace(l[3])) ||
                             (n > 5 && l[0] == 'p' && l[1] == 'a' && l[2] == 'c' && l[3] == 'e' && MapSpace(l[4])))) {
            int y = 0, d = 0, pace = l[0] == 'p';
            i = pace ? 5 : 4;
            while (i < n && MapSpace(l[i])) i++;
            for (; i < n && l[i] >= '0' && l[i] <= '9' && d < 6; i++, d++) y = y * 10 + (l[i] - '0');
            if (d && pace) g_map.pace = (float)y;
            else if (d) g_map.endYards = (float)y;
        }
        at = end + 1;
    }
    if (!(g_map.pace >= 1 && g_map.pace <= 60)) g_map.pace = 7;
    g_map.ok = (g_map.m[0].ok || g_map.m[1].ok) && g_map.endYards >= 100;
}

#define WALL_MAX 64
static struct { int count; int family[WALL_MAX]; char path[WALL_MAX][56]; } g_walls;
static volatile LONG g_wallPick;                       /* the wallpaper drawn for the screen now showing, + 1; 0 none */
static int g_wallLast = -1, g_barFamily = -1;          /* the game's thread only */
static unsigned g_wallSeed = 1;

static int BarIsOverworld(const char *name)
{
    return BarSame(name, "loadscreeneasternkingdom") || BarSame(name, "loadscreenkalimdor");
}
static int BarFamilyOf(const BarTable *t, const char *name)
{
    int f = t->fallback;
    for (int k = 0; k < t->screens; k++)
        if (BarSame(t->screen[k].name, name)) { f = t->screen[k].family; break; }
    return f >= 0 && t->family[f].ok ? f : -1;
}
/* A different wallpaper from last time when there is more than one. */
static int WallDraw(void)
{
    g_wallSeed = g_wallSeed * 1664525u + 1013904223u;
    int r = (int)((g_wallSeed >> 16) % (unsigned)g_walls.count);
    if (g_walls.count > 1 && r == g_wallLast) r = (r + 1) % g_walls.count;
    g_wallLast = r;
    return r;
}

/* On the game's thread, inside its loop over the bar's table, and as a screen's picture is loaded: the texture to load
 * in place of path. Anything it does not recognise, and any screen whose family is not in the pack, gets path back as
 * it came. Reads only; no allocation, no file, no log. */
const char *WINAPI BarPath(const char *path)
{
    if (!g_barsOn || !path) return path;
    int border = BarSamePath(path, kBarBorderPath), fill = !border && BarSamePath(path, kBarFillPath);
    if (!border && !fill) {                            /* the screen's picture: a continent's gets the map or a wallpaper */
        LONG w = g_wallPick, m = g_mapShown;
        char name[BAR_NAME_MAX + 1];
        if ((m || (w > 0 && w <= g_walls.count)) && BarBaseName(path, lstrlenA(path), name) && BarIsOverworld(name))
            return m ? MAP_PATH : g_walls.path[w - 1];
        return path;
    }
    int f;
    if (fill) {                                        /* the fill comes first: the screen's choices are made here */
        char name[BAR_NAME_MAX + 1];
        int over = (g_map.ok || g_walls.count) && BarScreen(name) && BarIsOverworld(name);
        if (over && g_map.ok) {                        /* the world map, its fire from bars.txt's "cindermap" line */
            InterlockedExchange(&g_wallPick, 0);
            InterlockedExchange(&g_mapShown, 1);
            f = BarFamilyOf(&g_bars, "cindermap");
        } else if (over) {
            int r = WallDraw();
            InterlockedExchange(&g_mapShown, 0);
            InterlockedExchange(&g_wallPick, r + 1);
            f = g_walls.family[r];
        } else {
            InterlockedExchange(&g_mapShown, 0);
            InterlockedExchange(&g_wallPick, 0);
            f = BarFamily(&g_bars);
        }
        g_barFamily = f;
        InterlockedExchange(&g_barShown, f >= 0 ? f + 1 : -1);
    } else
        f = g_barFamily;                               /* the border burns as its fill does */
    if (f < 0) return path;
    return border ? g_bars.family[f].border : g_bars.family[f].fill;
}

/* For the worker's line about a screen that has gone: which bar it had, if the bar was ours to choose. */
/* Appends s to out, never past cap bytes (the terminator included). */
static void BarCat(char *out, int cap, const char *s)
{
    int n = lstrlenA(out);
    while (*s && n < cap - 1) out[n++] = *s++;
    out[n] = 0;
}
/* Fixed 3 Oct 2026: it wrote into a 48-byte buffer with no limit, and a screen with a wallpaper overran it (by 2 bytes
 * with the arcane fire, 13 with the pack's orange). */
static void BarShownSince(char *out, int cap)
{
    LONG shown = InterlockedExchange(&g_barShown, 0);
    out[0] = 0;
    if (g_barsOn && shown > 0 && shown <= g_bars.families) {
        BarCat(out, cap, ", its fire ");
        BarCat(out, cap, g_bars.family[shown - 1].name);
    } else if (g_barsOn && shown < 0)
        BarCat(out, cap, ", its fire the pack's orange");
    if (g_barsOn && g_mapShown) {
        char z[64];
        float h = g_viewH;
        int x10 = h > 0.001f ? (int)(5.0f / h + 0.5f) : 0;            /* 0.5 / h, in tenths */
        wsprintfA(z, ", the world map, ending %d.%dx in", x10 / 10, x10 % 10);
        BarCat(out, cap, z);
        if (g_driftResets) { wsprintfA(z, ", %ld resets mid-screen", (long)g_driftResets); BarCat(out, cap, z); }
    }
    LONG w = g_wallPick;
    if (g_barsOn && !g_mapShown && w > 0 && w <= g_walls.count) {
        const char *n = g_walls.path[w - 1];
        for (const char *q = n; *q; q++)
            if (*q == '\\') n = q + 1;
        BarCat(out, cap, ", its wallpaper ");
        BarCat(out, cap, n);
    }
}

/* The rel32 that sends the call at RVA 0x6891 to BarStub. */
static unsigned BarRel(const unsigned char *base)
{
    return (unsigned)((ULONG_PTR)BarStub - (ULONG_PTR)(base + BAR_CALL_RVA + 5));
}
static unsigned Rd32(const unsigned char *p) { return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24; }

/* Place i holds its bytes; the call's rel32 may already be ours, from an earlier start in this process. */
static int BarPlaceHolds(const unsigned char *base, int i)
{
    const unsigned char *at = base + kBarCode[i].rva, *want = kBarCode[i].bytes;
    int n = kBarCode[i].n;
    if (!Readable(at, n)) return 0;
    if (Same(at, want, n)) return 1;
    return kBarCode[i].rva == BAR_LOAD_RVA && Same(at, want, BAR_REL_AT) &&
           Same(at + BAR_REL_AT + 4, want + BAR_REL_AT + 4, n - BAR_REL_AT - 4) && Rd32(at + BAR_REL_AT) == BarRel(base);
}

/* Both of a family's textures are in our archive: read whole, so a damaged one is found here and not by the game. */
static int BarHasTexture(const char *path)
{
    char name[100];
    if (lstrlenA(path) + 5 > (int)sizeof name) return 0;
    lstrcpyA(name, path);
    lstrcatA(name, ".blp");
    unsigned char *data = NULL;
    unsigned len = 0;
    int r = mpq_read(&kIo, "patch-~.MPQ", name, &data, &len);
    if (data) HeapFree(GetProcessHeap(), 0, data);
    return r == MPQ_OK;
}

/* bars.txt is ours, but a file in the player's folder all the same: nothing bigger is even read in. */
static void *IoAllocSmall(void *ctx, unsigned size) { return size > 4 * BAR_TEXT_MAX ? NULL : IoAlloc(ctx, size); }
static const mpq_io kIoSmall = { IoAllocSmall, IoRelease, IoOpen, IoReadAt, IoSize, IoClose, NULL };

/* The world map's table and picture, out of Data\patch-~.MPQ. */
static void MapLoad(void)
{
    unsigned char *text = NULL;
    unsigned len = 0;
    g_map.ok = 0;
    int r = mpq_read(&kIoSmall, "patch-~.MPQ", "CinderLoad\\map.txt", &text, &len);
    if (r != MPQ_OK || !text) {
        if (text) HeapFree(GetProcessHeap(), 0, text);
        return;
    }
    MapParse((const char *)text, len);
    HeapFree(GetProcessHeap(), 0, text);
    if (!g_map.ok) { Log("zoom off: CinderLoad\\map.txt names no continent it can use, or no end"); return; }
    if (!BarHasTexture("Interface\\Glues\\LoadingScreens\\CinderMap")) {
        g_map.ok = 0;
        Log("zoom off: the pack has map.txt but no CinderMap.blp");
        return;
    }
    Log("zoom: the world map is in the pack; on Eastern Kingdoms and Kalimdor screens the view glides down onto where "
        "you arrive");
}

/* The screen's picture is loaded by the same loader as the bar's textures (RVA 0x6EBA, ecx = the picture's path from
 * its LoadingScreens record): that call goes to BarStub too, once the fire bar is on and there are wallpapers. */
#define WALL_CALL_RVA 0x006EBA                         /* call 0x449D90: E8 D1 2E 04 00 */
#define WALL_LOAD_RVA 0x006EAF
static const unsigned char kWallLoad[] = { 0x8B, 0x4E, 0x08, 0x6A, 0x00, 0x6A, 0x00, 0x50, 0x8D, 0x55, 0xE8,   /* mov ecx,[esi+8] */
                                           0xE8, 0xD1, 0x2E, 0x04, 0x00, 0xA3, 0x04, 0x2E, 0x88, 0x00 };    /* call; mov [882E04],eax */
#define WALL_REL_AT (WALL_CALL_RVA + 1 - WALL_LOAD_RVA)
static unsigned WallRel(const unsigned char *base)
{
    return (unsigned)((ULONG_PTR)BarStub - (ULONG_PTR)(base + WALL_CALL_RVA + 5));
}
static int WallCall(unsigned char *base)
{
    unsigned char *at = base + WALL_LOAD_RVA, *call = base + WALL_CALL_RVA;
    int n = (int)sizeof kWallLoad;
    if (!Readable(at, n)) { Log("wallpapers off: the picture's load could not be read"); return 0; }
    if (Same(at, kWallLoad, WALL_REL_AT) && Same(at + WALL_REL_AT + 4, kWallLoad + WALL_REL_AT + 4, n - WALL_REL_AT - 4) &&
        Rd32(call + 1) == WallRel(base)) {
        Log("wallpapers already on: nothing to do");
        return 1;
    }
    if (!Same(at, kWallLoad, n) || (unsigned char *)g_barLoader != call + 5 + (int)Rd32(call + 1)) {
        char m[200];
        int len = wsprintfA(m, "wallpapers off: other bytes at RVA 0x%lX (the picture's load) than in the client it was "
                            "made for, so the game's own screens; there:", (unsigned long)WALL_LOAD_RVA);
        for (int k = 0; k < n; k++) len += wsprintfA(m + len, " %02X", at[k]);
        Log(m);
        return 0;
    }
    unsigned rel = WallRel(base);
    DWORD old;
    if (!VirtualProtect(call + 1, 4, PAGE_EXECUTE_READWRITE, &old)) { Log("wallpapers off: the code could not be made writable"); return 0; }
    for (int i = 0; i < 4; i++)
        call[1 + i] = (unsigned char)(rel >> (8 * i));
    VirtualProtect(call + 1, 4, old, &old);
    FlushInstructionCache(GetCurrentProcess(), call, 5);
    if (Rd32(call + 1) != rel) { Log("wallpapers off: the bytes read back differ"); return 0; }
    Log("wallpapers on: the picture's load goes through CinderLoad (one call at RVA 0x6EBA, in memory only)");
    return 1;
}

/* At start, after our screens and the switch are in place: reads bars.txt and the families out of Data\patch-~.MPQ,
 * then sends the one call to BarStub. 1 when the bar's fire follows the screens. */
static int FireBar(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleA(NULL);
    char m[400];
    for (int i = 0; i < BAR_CODE_COUNT; i++) {
        if (BarPlaceHolds(base, i)) continue;
        int len = wsprintfA(m, "fire bar off: other bytes at RVA 0x%lX (%s) than in the client it was made for, so "
                            "every bar is the pack's orange; there:", (unsigned long)kBarCode[i].rva, kBarCode[i].what);
        if (Readable(base + kBarCode[i].rva, kBarCode[i].n))
            for (int k = 0; k < kBarCode[i].n; k++)
                len += wsprintfA(m + len, " %02X", base[kBarCode[i].rva + (DWORD)k]);
        else
            lstrcpyA(m + len, " (unreadable)");
        Log(m);
        return 0;
    }
    unsigned char *at = base + BAR_CALL_RVA;
    if (Rd32(at + 1) == BarRel(base)) {
        Log("fire bar already on: nothing to do");
        return 1;
    }
    InterlockedExchange(&g_barsOn, 0);

    unsigned char *text = NULL;
    unsigned len = 0;
    int r = mpq_read(&kIoSmall, "patch-~.MPQ", "CinderLoad\\bars.txt", &text, &len);
    if (r != MPQ_OK || len > BAR_TEXT_MAX) {
        if (text) HeapFree(GetProcessHeap(), 0, text);
        if (r == MPQ_E_NOT_FOUND)
            Log("fire bar off: Data\\patch-~.MPQ holds no CinderLoad\\bars.txt, so every bar is the pack's orange");
        else if (r == MPQ_OK || r == MPQ_E_NOMEM)
            Log("fire bar off: CinderLoad\\bars.txt is over 64 KB, so every bar is the pack's orange");
        else {
            wsprintfA(m, "fire bar off: CinderLoad\\bars.txt could not be read from Data\\patch-~.MPQ (%s), so every "
                      "bar is the pack's orange", mpq_error_text(r));
            Log(m);
        }
        return 0;
    }
    BarParse(&g_bars, (const char *)text, len);
    HeapFree(GetProcessHeap(), 0, text);

    int have = 0, miss = 0;
    char haveList[BAR_FAMILIES * (BAR_FAMILY_MAX + 2) + 1], missList[BAR_FAMILIES * (BAR_FAMILY_MAX + 2) + 1];
    haveList[0] = missList[0] = 0;
    for (int f = 0; f < g_bars.families; f++) {
        lstrcpyA(g_bars.family[f].fill, kBarFillPath);
        lstrcatA(g_bars.family[f].fill, "-");
        lstrcatA(g_bars.family[f].fill, g_bars.family[f].name);
        lstrcpyA(g_bars.family[f].border, kBarBorderPath);
        lstrcatA(g_bars.family[f].border, "-");
        lstrcatA(g_bars.family[f].border, g_bars.family[f].name);
        g_bars.family[f].ok = BarHasTexture(g_bars.family[f].fill) && BarHasTexture(g_bars.family[f].border);
        char *list = g_bars.family[f].ok ? haveList : missList;
        int *count = g_bars.family[f].ok ? &have : &miss;
        if ((*count)++) lstrcatA(list, ", ");
        lstrcatA(list, g_bars.family[f].name);
    }
    wsprintfA(m, "fire bar: bars.txt names %d screens, %s, %d lines skipped", g_bars.screens,
              g_bars.fallback < 0 ? "no default (the screens it does not list keep the pack's orange)"
                                  : "and a default for the rest", g_bars.skipped);
    Log(m);
    wsprintfA(m, "fire bar: families in the pack: %s; named but not in it (their screens keep the pack's orange): %s",
              have ? haveList : "none", miss ? missList : "none");
    Log(m);
    if (!have) {
        Log("fire bar off: no family bars.txt names has both its textures in the pack");
        return 0;
    }

    /* The wallpapers: CinderWall01.blp, 02, ... while they are there, each with its fire from bars.txt. */
    g_walls.count = 0;
    for (int i = 1; i <= WALL_MAX; i++) {
        char path[56], name[24];
        wsprintfA(name, "cinderwall%02d", i);
        wsprintfA(path, "Interface\\Glues\\LoadingScreens\\CinderWall%02d", i);
        if (!BarHasTexture(path)) break;
        lstrcatA(path, ".blp");
        lstrcpyA(g_walls.path[g_walls.count], path);
        g_walls.family[g_walls.count] = BarFamilyOf(&g_bars, name);
        g_walls.count++;
    }
    g_wallSeed = GetTickCount() | 1u;
    if (g_walls.count) {
        wsprintfA(m, "wallpapers: %d in the pack, one drawn at random for each Eastern Kingdoms and Kalimdor screen",
                  g_walls.count);
        Log(m);
    }

    /* Everything BarPath reads is in place before the call can reach it. */
    g_barLoader = at + 5 + (int)Rd32(at + 1);          /* where the game's call went: the loader, VA 0x449D90 */
    InterlockedExchange(&g_barsOn, 1);
    unsigned rel = BarRel(base);
    DWORD old;
    if (!VirtualProtect(at + 1, 4, PAGE_EXECUTE_READWRITE, &old)) {
        InterlockedExchange(&g_barsOn, 0);
        Log("fire bar off: the code could not be made writable");
        return 0;
    }
    for (int i = 0; i < 4; i++)
        at[1 + i] = (unsigned char)(rel >> (8 * i));
    VirtualProtect(at + 1, 4, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
    if (Rd32(at + 1) != rel) {
        InterlockedExchange(&g_barsOn, 0);           /* BarPath then hands every path back as it came */
        Log("fire bar off: the bytes read back differ");
        return 0;
    }
    Log("fire bar on: each loading screen's bar burns in its picture's fire (one call at RVA 0x6891, in memory only)");
    MapLoad();
    if ((g_walls.count || g_map.ok) && !WallCall(base)) {   /* the picture cannot be swapped: show neither */
        g_walls.count = 0;
        g_map.ok = 0;
    }
    OverlayStart();
    return 1;
}

/* ---- the fire revealed, not stretched ----
 * The game draws the bar's fill from its left edge to progress * width with the whole texture on it, so a fill is
 * stretched as the bar grows and can only be a plain fade: any detail would slide. The 3 Oct look: ahead of the fire
 * the log must be matte black and behind it every crack must burn, as in the mock. So a pack can carry a fill that is
 * a painting of the burning log, made to sit exactly under the log's cracks, and ask for it to be revealed: the fill
 * then shows its texture from 0 to progress, fixed in place under the log, and the log itself is plain char with open
 * cracks over black.
 *
 * Read from WoW.exe (1.12.1 build 5875) on 3 Oct 2026, the bar's draw loop (RVA 0x7150): esi walks the bar's table at
 * VA 0x7FFD34, whose first entry is the fill (RVA 0x719D); for the fill the right edge is progress * width (RVA 0x7215);
 * then every entry pushes the texture coordinates at VA 0x882B8C, the whole texture, and calls the vertex setup at
 * VA 0x58A2A0 (RVA 0x7229 .. 0x7262). That one call goes to RevealStub, which for the fill entry alone puts our own
 * coordinates in place of 0x882B8C's, u from 0 to the progress, and goes on to the same function. Only when the pack
 * holds CinderLoad\reveal.txt, so a pack made for stretching keeps being stretched. */
#define REVEAL_CALL_RVA 0x00725E   /* call 0x58A2A0 (vertex setup): E8 3D 30 18 00 */
#define REVEAL_DRAW_CALL_RVA 0x007288   /* call 0x58A2E0 (draw the indexed strip): E8 53 30 18 00 */
#define REVEAL_TABLE_VA 0x7FFD34   /* the bar's table; its first entry is the fill */
static const unsigned char kRevealTable[] = { 0xBE, 0x34, 0xFD, 0x7F, 0x00 };                     /* mov esi,7FFD34 */
static const unsigned char kRevealFill[] = { 0x74, 0x12, 0xD9, 0x05, 0xE4, 0x2B, 0x88, 0x00, 0xD8, 0x4E, 0x10 };
static const unsigned char kRevealDraw[] = {                                                     /* RVA 0x7229 */
    0x53, 0x53, 0x6A, 0x08, 0x68, 0x8C, 0x2B, 0x88, 0x00, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x6A, 0x0C, 0x8D, 0x55,
    0xCC, 0xB9, 0x04, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xD4, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xE0, 0x00, 0x00, 0x00,
    0x00, 0xC7, 0x45, 0xEC, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xF8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x3D, 0x30, 0x18,
    0x00 };
static const unsigned char kRevealStrip[] = {                                                    /* RVA 0x727C */
    0xBA, 0x04, 0x00, 0x00, 0x00, 0x68, 0x10, 0xF0, 0x82, 0x00, 0x8B, 0xCA, 0xE8, 0x53, 0x30, 0x18, 0x00 };
static const unsigned char kRevealIndices[] = { 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00 };  /* VA 0x82F010 */
#define REVEAL_DRAW_RVA 0x007229
#define REVEAL_STRIP_RVA 0x00727C
#define REVEAL_REL_AT (REVEAL_CALL_RVA + 1 - REVEAL_DRAW_RVA)          /* where in kRevealDraw the call's rel32 sits */
#define REVEAL_STRIP_REL_AT (REVEAL_DRAW_CALL_RVA + 1 - REVEAL_STRIP_RVA)
static const struct { DWORD rva; int n; const unsigned char *bytes; const char *what; } kRevealCode[] = {
    { 0x00719D, (int)sizeof kRevealTable, kRevealTable, "the bar's draw loop over its table" },
    { 0x007215, (int)sizeof kRevealFill, kRevealFill, "the fill's width from the progress" },
    { REVEAL_DRAW_RVA, (int)sizeof kRevealDraw, kRevealDraw, "the bar's texture coordinates and vertex setup" },
    { REVEAL_STRIP_RVA, (int)sizeof kRevealStrip, kRevealStrip, "the bar's draw, a strip of 4 corners" },
    { 0x42F010, (int)sizeof kRevealIndices, kRevealIndices, "the strip's corner order 0 1 2 3" },
};
#define REVEAL_CODE_COUNT ((int)(sizeof kRevealCode / sizeof kRevealCode[0]))

/* The fill, drawn as a strip of slices instead of the game's one quad (3 Oct: "make it appear like it's happening
 * in real time"). The pack's revealed fill is flowing lava, seen only through the log's open cracks; each slice gets its
 * own brightness from how far it lies behind the front: white-hot at the tip (where it fades in over the last
 * REVEAL_FEATHER), brightest just behind, so the dimmer valleys catch after the main channel, then cooling to embers,
 * with a small flicker. The lava's texture drifts a little every frame. All of it is worked out on the game's thread
 * by RevealFill just before the game reads it, from the front's place and the clock. */
#define REVEAL_FEATHER  0.04f      /* the front fades in over this much of the bar's width (his look of 3 Oct) */
#define FILL_COLUMNS    26         /* the strip's columns, so up to 52 corners */
#define FILL_FLOW       0.006f     /* the lava drifts this much of its width a second */
static const float kFillBehind[FILL_COLUMNS - 1] = {   /* where the columns sit behind the front, in screen widths */
    0.0f, 0.012f, 0.025f, 0.04f, 0.055f, 0.075f, 0.1f, 0.13f, 0.17f, 0.22f, 0.28f, 0.35f, 0.43f, 0.52f, 0.62f, 0.73f,
    0.85f, 0.98f, 1.12f, 1.27f, 1.43f, 1.6f, 1.78f, 1.97f, 2.2f };
__attribute__((used)) float g_fillVerts[3 * 2 * FILL_COLUMNS];
__attribute__((used)) float g_fillUV[2 * 2 * FILL_COLUMNS];
__attribute__((used)) unsigned g_fillColor[2 * FILL_COLUMNS];   /* grey levels, alpha in the top byte */
__attribute__((used)) unsigned short g_fillIndex[2 * FILL_COLUMNS];
__attribute__((used)) int g_fillCount = 4;                       /* corners in the strip just made */
__attribute__((used)) void *g_drawSetup;              /* the game's vertex setup, VA 0x58A2A0 */
__attribute__((used)) void *g_drawStrip;              /* the game's indexed draw, VA 0x58A2E0 */
void WINAPI RevealFill(const float *v) __attribute__((used));

/* ---- the picture moves: the first step of the zoom (3 Oct 2026) ----
 * Read from WoW.exe (1.12.1 build 5875) on 3 Oct 2026: the loading screen's background (VA 0x406CF0) pushes its
 * texture coordinates by address, `push 0x882B8C` at RVA 0x6DBC: the whole picture, set once at start (VA 0x406280:
 * u,v = 0,1  1,1  0,0  1,0 for its corners 0,0  1,0  0,1  1,1) and shared with the bar. That one push is pointed at
 * g_bgUV instead, and DriftStep, on the game's thread every frame the bar is drawn, eases them: the picture drifts in
 * toward its centre as the bar fills. Only when the pack holds CinderLoad\drift.txt, and only with the reveal on (its
 * stub is what runs DriftStep). */
#define BG_DRAW_RVA 0x006DB6
#define BG_PUSH_RVA 0x006DBC
static const unsigned char kBgDraw[] = {                                                         /* RVA 0x6DB6 */
    0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x08, 0x68, 0x8C, 0x2B, 0x88, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00,
    0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x0C, 0xBA, 0xC0, 0x2D, 0x88, 0x00, 0xB9, 0x04, 0x00, 0x00, 0x00, 0xE8, 0xC2, 0x34,
    0x18, 0x00 };
#define BG_PTR_AT (BG_PUSH_RVA + 1 - BG_DRAW_RVA)       /* where in kBgDraw the pushed address sits */
#define DRIFT_ZOOM 0.12f      /* the picture is this much closer when the bar is full */
#define DRIFT_TAU  0.6f       /* seconds: how smoothly the drift follows the bar */
__attribute__((used)) float g_bgUV[8] = { 0, 1, 1, 1, 0, 0, 1, 0 };
static volatile LONG g_driftOn;
static float g_drift;
static DWORD g_driftAt;

/* ln x for x > 0, near enough for a view (no C runtime): x = m * 2^k, ln m by its series in (m - 1) / (m + 1). */
static float MapLn(float x)
{
    union { float f; unsigned u; } c;
    c.f = x;
    int k = (int)((c.u >> 23) & 0xFF) - 127;
    c.u = (c.u & 0x007FFFFFu) | 0x3F800000u;                        /* m in [1, 2) */
    float t = (c.f - 1) / (c.f + 1), t2 = t * t;
    return (float)k * 0.69314718f + 2 * t * (1 + t2 * (1.0f / 3 + t2 * (1.0f / 5 + t2 * (1.0f / 7 + t2 / 9))));
}

/* Where the player arrives (4 Oct 2026, his find: a hunter in Westfall, a paladin in Karazhan and a druid in Stormwind
 * all showed at Alterac, the Eastern Kingdoms' 0, 0: at a login the game has not written the arrival yet, and its
 * nothing was taken for a place). Each frame, the first of:
 *   the arrival the game writes (NEW_WORLD, LOGIN_VERIFY_WORLD), once it has changed since this screen began; before
 *     that it is the last screen's, or nothing;
 *   at a login, the character list's own place for the one picked (its map +0x38, x +0x44, y +0x48, filled by the
 *     character list's parser, VA 0x5B4389..), while the list is there (the game clears it during the load);
 *   from half the bar on, the game's arrival as it stands (a teleport to where you already were writes the same place)
 * and never 0, 0. Its world place is kept too, for the glow. */
#define CHAR_COUNT_VA   0xB42140
#define CHAR_LIST_VA    0xB42144
#define CHAR_PICKED_VA  0x83856C
#define CHAR_ENTERED_VA 0xB41DF4
#define CHAR_SIZE       0x120
#define CHAR_CLASS      0x101
#define CHAR_MAP        0x38
#define CHAR_X          0x44
#define CHAR_Y          0x48
static unsigned g_destSnap[3];                                  /* the game's arrival as this screen began */
static int g_destMap = -1;
static float g_destWX, g_destWY;                                /* the arrival in world yards, once known */
static int g_classSure;                                         /* the character list's code is all there (below) */

/* The game's arrival as it stands, into g_destSnap: called as a screen begins. */
static void ArrivalSnap(void)
{
    if (!ClientWord(MAP_DEST_MAP_VA, &g_destSnap[0]) || !ClientWord(MAP_DEST_X_VA, &g_destSnap[1]) ||
        !ClientWord(MAP_DEST_Y_VA, &g_destSnap[2]))
        g_destSnap[0] = g_destSnap[1] = g_destSnap[2] = 0;
}

/* World x, y on screen map id, on the picture: 1 with its u, v and the view's half size there. */
static int MapPlace(unsigned id, float x, float y, float *du, float *dv, float *hEnd)
{
    if (!(x > -20000 && x < 20000 && y > -20000 && y < 20000) || (x == 0 && y == 0)) return 0;
    float u = g_map.m[id].au + g_map.m[id].bu * y, v = g_map.m[id].av + g_map.m[id].bv * x;
    float bv = g_map.m[id].bv < 0 ? -g_map.m[id].bv : g_map.m[id].bv, h = 0.5f * g_map.endYards * bv;
    if (!(u > 0 && u < 1 && v > 0 && v < 1 && h > 0.001f && h < 0.45f)) return 0;
    *du = u; *dv = v; *hEnd = h;
    g_destMap = (int)id; g_destWX = x; g_destWY = y;
    return 1;
}

/* The picked character's list entry (a client address), or 0 while there is no list. */
static unsigned CharEntry(void)
{
    unsigned count, list, picked;
    if (!g_classSure || !ClientWord(CHAR_COUNT_VA, &count) || !ClientWord(CHAR_LIST_VA, &list) ||
        !ClientWord(CHAR_PICKED_VA, &picked) || !list || !count || count > 64 || picked >= count)
        return 0;
    return list + picked * CHAR_SIZE;
}

/* Where the player arrives, on the world map: 1 with its u, v and the view's half size there; 0 while it is not
 * known (see above). p: the bar so far. */
static int MapDest(float *du, float *dv, float *hEnd, float p)
{
    unsigned id, dm, xb, yb, e;
    if (!ClientWord(BAR_MAP_VA, &id) || id > 1 || !g_map.m[id].ok) return 0;
    int game = ClientWord(MAP_DEST_MAP_VA, &dm) && ClientWord(MAP_DEST_X_VA, &xb) && ClientWord(MAP_DEST_Y_VA, &yb);
    int fresh = game && (dm != g_destSnap[0] || xb != g_destSnap[1] || yb != g_destSnap[2]);
    if (fresh && dm == id && MapPlace(id, MapBits(xb), MapBits(yb), du, dv, hEnd)) return 1;
    if ((e = CharEntry()) != 0) {                                    /* a login: the list's own place */
        const unsigned char *m = ClientAt(e + CHAR_MAP), *x = ClientAt(e + CHAR_X), *y = ClientAt(e + CHAR_Y);
        if (m && x && y && Readable(m, 4) && Readable(x, 4) && Readable(y, 4) && *(volatile const unsigned *)m == id &&
            MapPlace(id, MapBits(*(volatile const unsigned *)x), MapBits(*(volatile const unsigned *)y), du, dv, hEnd))
            return 1;
    }
    return game && p >= 0.5f && dm == id && MapPlace(id, MapBits(xb), MapBits(yb), du, dv, hEnd);
}

static float g_driftMax;                                        /* the bar's highest this screen */
static volatile LONG g_driftNew = 1;                            /* the next frame begins a new screen */

/* ---- the bar as shown (4 Oct 2026, his ask: "moves forward at a steady rate") ----
 * The game moves its progress in lumps: a 3.5 s load from the Hinterlands to Stormwind made 18 steps, ran to about
 * two thirds, stopped drawing for a second or more while it loaded, then leapt. The fill and the zoom now follow the
 * bar as shown instead: it runs at the game's own pace of late (its progress a second, averaged over DISP_RATE_TAU),
 * closes any gap within about DISP_CATCH, changes speed only gently (DISP_EASE), never covers more than DISP_FASTEST
 * of the bar in a second, may coast DISP_LEAD past the game through a stall, and never goes back within a screen (the
 * game's dip to nothing at the very end is not shown). A second in which the game draws nothing still shows nothing,
 * but no jump follows it: a frame's step counts at most a tenth of a second.
 * Also kept, for the log: how many frames, the longest pause between two, the bar either side of it, and dips. */
#define DISP_RATE_TAU 1.0f
#define DISP_CATCH    0.5f
#define DISP_EASE     0.2f
#define DISP_FASTEST  1.5f
#define DISP_LEAD     0.04f
#define DISP_GAP_MAX  0.75f   /* seconds: a pause between frames counts this much at most. His three logins (4 Oct) all
                               * paused at 0, 41, 44, 50 and 54% for 0.2 to 0.6 s (the game drew nothing: its interface
                               * loading, the same for every character), then froze 1 to 2.6 s at 100%. Counting a pause
                               * whole, the bar comes out of it where its pace would have taken it, instead of lagging and
                               * hurrying after (3 times its average speed, and ending at 91%); a freeze past this counts
                               * this much only. */
static float g_disp, g_dispV, g_dispRate, g_dispMax, g_fillFull;
static DWORD g_dispAt, g_dispStill;                            /* ms of frames on, the game's bar unmoved, so far */
#define HITCH_MS 120              /* a gap between frames this long or more is a hitch (4 Oct: he saw stutters at 20, 30,
                                   * 50, 55, 60, 75 and 80%; the log said only where the longest pause was) */
#define STALL_MS 300              /* the game's bar unmoved this long while frames still come is a stall */

/* Keeps the HITCH_KEEP longest of a screen's hitches or stalls: ms long, at the bar's pct. */
static void HitchKeep(volatile LONG *n, volatile LONG *ms, volatile LONG *pct, LONG len, LONG at)
{
    int least = 0;
    for (int i = 1; i < HITCH_KEEP; i++) if (ms[i] < ms[least]) least = i;
    InterlockedExchange(n, *n + 1);
    if (len > ms[least]) { InterlockedExchange(&ms[least], len); InterlockedExchange(&pct[least], at); }
}

/* The game's thread only: the bar as shown for the game's p (0..1) at now. */
static float DisplayStep(float p, DWORD now)
{
    DWORD gap = g_dispAt ? now - g_dispAt : 0;
    int fallback = !g_screenWatch && g_dispAt && gap > 1000 && p < 0.1f && g_dispMax > 0.5f;
    if (InterlockedExchange(&g_dispNew, 0) || !g_dispAt || fallback) {
        g_disp = g_dispMax = p;
        g_dispV = g_dispRate = 0;
        InterlockedExchange(&g_frmCount, 0); InterlockedExchange(&g_frmGap, 0); InterlockedExchange(&g_frmDips, 0);
        InterlockedExchange(&g_hitchN, 0); InterlockedExchange(&g_stallN, 0);
        for (int i = 0; i < HITCH_KEEP; i++) {
            InterlockedExchange(&g_hitchMs[i], 0); InterlockedExchange(&g_stallMs[i], 0);
        }
        g_dispStill = 0;
        gap = 0;
    }
    InterlockedExchange(&g_frmCount, g_frmCount + 1);                 /* the game thread is these counts' only writer */
    if (gap > (DWORD)g_frmGap) {
        InterlockedExchange(&g_frmGap, (LONG)gap);
        InterlockedExchange(&g_frmGapFrom, (LONG)(g_dispMax * 100 + 0.5f));
        InterlockedExchange(&g_frmGapTo, (LONG)(p * 100 + 0.5f));
    }
    if (p < g_dispMax - 0.2f) InterlockedExchange(&g_frmDips, g_frmDips + 1);
    if (gap >= HITCH_MS) HitchKeep(&g_hitchN, g_hitchMs, g_hitchPct, (LONG)gap, (LONG)(g_dispMax * 100 + 0.5f));
    if (gap < HITCH_MS) g_dispStill += gap;                           /* a hitch is not a stall: only drawn time counts */
    if (p > g_dispMax) {                                              /* the game's bar moves: was it still a while? */
        if (g_dispStill >= STALL_MS)
            HitchKeep(&g_stallN, g_stallMs, g_stallPct, (LONG)g_dispStill, (LONG)(g_dispMax * 100 + 0.5f));
        g_dispStill = 0;
    }
    g_dispAt = now;
    float dt = (float)gap / 1000.0f;
    if (dt > DISP_GAP_MAX) dt = DISP_GAP_MAX;
    float rise = p > g_dispMax ? p - g_dispMax : 0;
    if (p > g_dispMax) g_dispMax = p;
    if (dt <= 0) return g_disp;
    /* through the time since the last frame at the pace it had: a pause is crossed as if the bar had moved through it,
     * never at the catching-up pace the leap after it would ask for (that was his "freezes, then jumps up a ways") */
    float d = g_disp + g_dispV * dt, cap = g_dispMax + DISP_LEAD < 1 ? g_dispMax + DISP_LEAD : 1;
    if (d > cap) d = cap;
    if (d > g_disp) g_disp = d;
    /* then the pace for what comes: the game's of late, closing the gap, changing gently */
    float k = (float)(1.0 - FireExp(-(double)dt / DISP_RATE_TAU));
    g_dispRate += (rise / dt - g_dispRate) * k;
    float want = FireClamp(g_dispRate + (g_dispMax - g_disp) / DISP_CATCH, 0, DISP_FASTEST);
    g_dispV += (want - g_dispV) * (float)(1.0 - FireExp(-(double)dt / DISP_EASE));
    if (g_disp >= cap && g_dispV > 0) g_dispV = 0;                    /* held at the lead: it waits for the game */
    if (g_dispMax >= 0.999f) g_disp = 1;                              /* the load is done: the bar reads full, as the game's
                                                                       * freeze at its end holds this frame a second or two */
    InterlockedExchange(&g_frmShown, (LONG)(g_disp * 100 + 0.5f));
    return g_disp;
}

/* The worker, as a screen ends: its hitches and stalls, longest kept, in the bar's order (m: 220 bytes of room). */
static void HitchLog(char *m)
{
    static const char *const kind[2] = { "hitches (the game drew nothing)", "stalls (frames on, its bar still)" };
    for (int k = 0; k < 2; k++) {
        volatile LONG *ms = k ? g_stallMs : g_hitchMs, *pct = k ? g_stallPct : g_hitchPct;
        LONG n = k ? g_stallN : g_hitchN;
        if (!n) continue;
        int idx[HITCH_KEEP], c = 0;
        for (int i = 0; i < HITCH_KEEP; i++) if (ms[i] > 0) idx[c++] = i;
        for (int i = 1; i < c; i++)                                       /* in the bar's order */
            for (int j = i; j > 0 && pct[idx[j]] < pct[idx[j - 1]]; j--) { int t = idx[j]; idx[j] = idx[j - 1]; idx[j - 1] = t; }
        int len = wsprintfA(m, "  its %s: %ld;", kind[k], (long)n);
        for (int i = 0; i < c && len < 180; i++)
            len += wsprintfA(m + len, " %ld.%02ld s at %ld%%%s", (long)(ms[idx[i]] / 1000), (long)(ms[idx[i]] % 1000 / 10),
                             (long)pct[idx[i]], i + 1 < c ? "," : "");
        Log(m);
    }
}
static int g_destOk;                                            /* this screen's arrival, once known, is kept */
static DWORD g_destSince;                                       /* when it became known (the ripple fades in from it) */
static float g_destU, g_destV, g_destH;

/* The fire's worker, when a screen has gone (the loading screen's layer cleared): the next screen starts from the
 * whole world. Written between screens, when no loading screen is drawn, so no frame shows it. */
static void DriftScreenGone(void)
{
    static const float whole[8] = { 0, 1, 1, 1, 0, 0, 1, 0 };
    for (int i = 0; i < 8; i++) g_bgUV[i] = whole[i];
    InterlockedExchange(&g_driftNew, 1);
    InterlockedExchange(&g_dispNew, 1);
}

/* The game's thread only. p: the bar, 0..1. A new screen (no frame for a second, or another map) starts again; within
 * a screen the bar only goes up (4 Oct 2026: it dips for a frame at the very end, and the view used to jump back out
 * to the whole world). With the world map: from the whole world, the view zooms evenly (in steps of the same ratio)
 * onto where the player arrives, its middle following the zoom, the approach done by about 90% of the bar so the end
 * of it rests on them. Otherwise: the picture drifts in. */
static void DriftStep(float p, DWORD now)
{
    /* A new screen: the worker says so (it watches the loading screen's layer), or, only if there is no worker, the
     * bar has started again from empty after a second without a frame. Never a pause alone: the game freezes for a
     * moment at the very end of a load, and resetting there showed the first frame again (his find, 4 Oct); and
     * never with a worker (4 Oct, later: a load to Stormwind paused and dipped mid-screen, and the view went back
     * out to the whole world). */
    int fallback = !g_screenWatch && g_driftAt && now - g_driftAt > 1000 && p < 0.1f && g_driftMax > 0.5f;
    if (InterlockedExchange(&g_driftNew, 0) || !g_driftAt || fallback) {
        if (fallback) InterlockedExchange(&g_driftResets, g_driftResets + 1);   /* the game thread is its only writer */
        else InterlockedExchange(&g_driftResets, 0);
        g_drift = 0;
        g_driftMax = 0;
        g_viewU = g_viewV = g_viewH = 0.5f;
        g_destOk = 0;
        g_destMap = -1;
        ArrivalSnap();
    }
    if (p > g_driftMax) g_driftMax = p;
    p = g_driftMax;
    float dt = g_driftAt ? (float)(now - g_driftAt) / 1000.0f : 0;
    g_driftAt = now;
    if (dt > DISP_GAP_MAX) dt = DISP_GAP_MAX;                       /* as the bar: a pause counts whole, up to a limit */
    int zoom = g_mapShown && g_map.ok;
    float step = (p - g_drift) * (float)(1.0 - FireExp(-(double)dt / (zoom ? MAP_BAR_TAU : DRIFT_TAU)));
    if (zoom && step > dt / g_map.pace) step = dt / g_map.pace;     /* the speed limit: never quicker than pace */
    g_drift += step;
    float e = zoom ? 1 - (1 - g_drift) * (1 - g_drift)              /* the zoom: done early, then resting on them */
                   : g_drift * g_drift * (3 - 2 * g_drift);         /* the drift: eased in and out */
    float cu = 0.5f, cv = 0.5f, h;
    if (zoom) {
        float du, dv, hEnd;
        h = 0.5f;
        if (MapDest(&du, &dv, &hEnd, p)) {                         /* once known, kept for the screen */
            if (!g_destOk) g_destSince = now;
            g_destOk = 1; g_destU = du; g_destV = dv; g_destH = hEnd;
        } else if (g_destOk) {
            du = g_destU; dv = g_destV; hEnd = g_destH;
        }
        if (g_destOk) {
            h = 0.5f * (float)FireExp((double)(e * MapLn(hEnd / 0.5f)));
            float k = (0.5f - h) / (0.5f - hEnd);                   /* how far the zoom has come */
            cu = 0.5f + (du - 0.5f) * k;
            cv = 0.5f + (dv - 0.5f) * k;
        }
        float a = (float)(1.0 - FireExp(-(double)dt / MAP_TAU));
        g_viewU += (cu - g_viewU) * a;
        g_viewV += (cv - g_viewV) * a;
        g_viewH += (h - g_viewH) * a;
        cu = g_viewU; cv = g_viewV; h = g_viewH;
        if (cu < h) cu = h;
        if (cu > 1 - h) cu = 1 - h;
        if (cv < h) cv = h;
        if (cv > 1 - h) cv = 1 - h;
    } else
        h = 0.5f / (1.0f + DRIFT_ZOOM * e);
    float u0 = cu - h, u1 = cu + h, vt = cv - h, vb = cv + h;       /* v grows downward on the picture */
    g_bgUV[0] = u0; g_bgUV[1] = vb; g_bgUV[2] = u1; g_bgUV[3] = vb;
    g_bgUV[4] = u0; g_bgUV[5] = vt; g_bgUV[6] = u1; g_bgUV[7] = vt;
}

/* ---- the overlay: over the world map, under the bar (4 Oct 2026) ----
 * Read from WoW.exe: each frame draws the picture, then the bar, whose pass turns blending on (render state 7 = 2, RVA
 * 0x71AC). RevealStub already runs at the bar's fill entry, before the fill's own vertex setup; there the overlay draws
 * its own quads with the game's own calls, all __fastcall (the callee cleans): 0x58A2A0 vertex setup, 0x44ACF0 and
 * 0x589E80 bind a texture, 0x58A2E0 draw a strip, 0x58A340 after a draw. Its picture is loaded once, the way the
 * screen's picture is: 0x58A980 for the flags, then the loader the fire bar already found (VA 0x449D90). With the
 * world map only: first the sea, then the names and icons over it. */
static const struct { DWORD rva; unsigned target; const char *what; } kOverlayCalls[] = {
    { 0x006DD9, 0x58A2A0, "the vertex setup" }, { 0x006DEB, 0x44ACF0, "a texture's device copy" },
    { 0x006DF7, 0x589E80, "binding it" }, { 0x006E08, 0x58A2E0, "the strip's draw" },
    { 0x006E0D, 0x58A340, "after a draw" }, { 0x006EA8, 0x58A980, "a texture's flags" } };
#define OVERLAY_CALLS ((int)(sizeof kOverlayCalls / sizeof kOverlayCalls[0]))
static int g_ovOn;
#define LABELS_PATH "Interface\\Glues\\LoadingScreens\\CinderLabels.blp"

/* Every call the overlay makes, where WoW.exe calls it: 1 when all six are there. */
static int OverlayCallsHold(const unsigned char *base)
{
    for (int i = 0; i < OVERLAY_CALLS; i++) {
        const unsigned char *at = base + kOverlayCalls[i].rva;
        if (!Readable(at, 5) || at[0] != 0xE8 ||
            (ULONG_PTR)(at + 5 + (int)Rd32(at + 1)) != (ULONG_PTR)base + (kOverlayCalls[i].target - 0x400000u))
            return 0;
    }
    return 1;
}

/* Quad q of a set of strips LBL_BATCH quads long: its corners (bottom left, bottom right, top left, top right) into
 * pos / uv / col. A strip's first quad has 4 corner slots, every later one 6: the last corner of the quad before and
 * its own first again, two empty triangles joining them. */
#define LBL_BATCH   120                        /* quads to a strip: 6 corners each less 2, so 718 */
static void QuadPut(float *pos, float *uv, unsigned *col, int q, const float *x, const float *y, const float *tu,
                    const float *tv, unsigned c)
{
    int inBatch = q % LBL_BATCH, base = (q / LBL_BATCH) * (LBL_BATCH * 6 - 2), at = base + (inBatch ? inBatch * 6 - 2 : 0);
    int order[6], m = 0;
    if (inBatch) { order[m++] = -1; order[m++] = 0; }    /* the last corner again, then this quad's first again */
    for (int k = 0; k < 4; k++) order[m++] = k;
    for (int j = 0; j < m; j++) {
        int k = order[j], slot = at + j;
        if (k < 0) {                                      /* repeat the previous quad's last corner */
            for (int e = 0; e < 3; e++) pos[slot * 3 + e] = pos[(slot - 1) * 3 + e];
            uv[slot * 2] = uv[(slot - 1) * 2]; uv[slot * 2 + 1] = uv[(slot - 1) * 2 + 1];
            col[slot] = col[slot - 1];
            continue;
        }
        pos[slot * 3] = x[k]; pos[slot * 3 + 1] = y[k]; pos[slot * 3 + 2] = 0;
        uv[slot * 2] = tu[k]; uv[slot * 2 + 1] = tv[k];
        col[slot] = c;
    }
}

static void SwirlAt(float *x, float *v, float asp);             /* the marks' (below): the swells bend round the Maelstrom */
static int LblInt(const char *s, int n, int *at, int *out);       /* the labels' (below): a decimal from a table line */

/* ---- the sea (4 Oct 2026, his pick: glints, swells and depth; the depth is painted into the world map itself) ----
 * CinderSea.blp (DXT5, 256 x 128): a glint on its left half; on its right a white square over a deep-sea one, so the
 * colours are the sheet's own (the corners only ever say how much, never what colour). CinderLoad\sea.bin: where the
 * open sea is, kept back from every coast: 'CSEA', its width and height (16 bits each, at most 256), then a byte a
 * cell, 0 land to 255 open sea, over the picture's u, v. Both only while the view is far out: they fade as it closes in.
 *   the swells: a grid over the screen drawn twice, white where two broad slow waves crest and deep blue where they
 *               fall, in the world's own place, so the bands roll across the sea and stay put on it as the view moves
 *   the glints: sparkles that come and go on the swells' crests, each at a fresh place every time it comes back */
#define SEA_PATH    "Interface\\Glues\\LoadingScreens\\CinderSea.blp"
#define SEA_MAX     256
#define SEA_FAR     0.30f      /* the view's half height: the sea at its fullest from here out */
#define SEA_NEAR    0.14f      /* and gone by here */
#define SWELL_ROWS  48
#define SWELL_COLS  176        /* at most: the screen's shape sets how many */
#define SWELL_LIGHT 0.07f      /* the most a crest lightens the sea */
#define SWELL_DARK  0.16f      /* and a trough darkens it */
#define SEA_SQUARE_U 0.75f     /* the middle of the sheet's two squares: white above, black below */
#define SEA_WHITE_V  0.25f
#define SEA_DEEP_V   0.75f
#define BAR_BLACK_H  (0.09f + 2.0f / 2160)  /* the log's own height, as make_pack.py blacks it in on every other screen */
#define GLINTS      40
#define GLINT_PX    36.0f      /* a glint at its brightest, on a 2160-tall screen */
#define GLINT_LIFE  1.1f       /* seconds a glint shows */
#define GLINT_CREST 0.25f      /* a glint only sparks where the swell is at least this high: on the crests */
static unsigned char g_seaMask[SEA_MAX * SEA_MAX];
static int g_seaW, g_seaH, g_seaOn, g_seaTried, g_swCols, g_glQuads;
static void *g_seaTex;
static struct { float u, v; int cycle, set, on; } g_glint[GLINTS];
__attribute__((used)) float g_swPos[(SWELL_ROWS + 1) * (SWELL_COLS + 1) * 3];
__attribute__((used)) float g_swUV[2][(SWELL_ROWS + 1) * (SWELL_COLS + 1) * 2];        /* the crests', the troughs' */
__attribute__((used)) unsigned g_swCol[2][(SWELL_ROWS + 1) * (SWELL_COLS + 1)];
__attribute__((used)) unsigned short g_swIndex[(SWELL_COLS + 1) * 2];
__attribute__((used)) float g_glPos[GLINTS * 6 * 3];
__attribute__((used)) float g_glUV[GLINTS * 6 * 2];
__attribute__((used)) unsigned g_glCol[GLINTS * 6];

/* sea.bin, checked whole: 1 when it is used. */
static int SeaParse(const unsigned char *d, unsigned len)
{
    g_seaW = g_seaH = 0;
    if (len < 8 || d[0] != 'C' || d[1] != 'S' || d[2] != 'E' || d[3] != 'A') return 0;
    int w = d[4] | d[5] << 8, h = d[6] | d[7] << 8;
    if (w < 2 || h < 2 || w > SEA_MAX || h > SEA_MAX || len != 8u + (unsigned)(w * h)) return 0;
    for (int i = 0; i < w * h; i++) g_seaMask[i] = d[8 + i];
    g_seaW = w; g_seaH = h;
    return 1;
}

/* How open the sea is at picture u, v: 0..1, between the cells' middles; 0 off the picture. */
static float SeaAt(float u, float v)
{
    if (!g_seaW || !(u >= 0 && u <= 1 && v >= 0 && v <= 1)) return 0;
    float x = FireClamp(u * g_seaW - 0.5f, 0, (float)(g_seaW - 1)), y = FireClamp(v * g_seaH - 0.5f, 0, (float)(g_seaH - 1));
    int x0 = (int)x, y0 = (int)y, x1 = x0 + 1 < g_seaW ? x0 + 1 : x0, y1 = y0 + 1 < g_seaH ? y0 + 1 : y0;
    float fx = x - x0, fy = y - y0;
    const unsigned char *a = g_seaMask + y0 * g_seaW, *b = g_seaMask + y1 * g_seaW;
    float top = a[x0] + (a[x1] - a[x0]) * fx, bot = b[x0] + (b[x1] - b[x0]) * fx;
    return (top + (bot - top) * fy) / 255.0f;
}

/* The sea's strength for a view h high (half height): full far out, gone close in, eased between. */
static float SeaFade(float h)
{
    float k = FireClamp((h - SEA_NEAR) / (SEA_FAR - SEA_NEAR), 0, 1);
    return k * k * (3 - 2 * k);
}

/* sin for any x (FireSin is good only to pi/2). */
static float SeaSin(float x)
{
    const float pi = 3.14159265f;
    x -= 2 * pi * (float)(int)(x / (2 * pi));
    if (x < 0) x += 2 * pi;
    float s = 1;
    if (x > pi) { x -= pi; s = -1; }
    if (x > pi / 2) x = pi - x;
    return s * FireSin(x);
}

/* The swell, -1..1, at x across and v down the picture, both in heights of the world (x = u times the screen's shape),
 * t seconds: two broad waves running nearly the same way (about a sixth and an eighth of the world apart), rolling
 * slowly down the world, both bent by the same slow wander, so the sea shows long soft bands, never a grid. */
static float SwellAt(float x, float v, float t)
{
    float wv = 2.2f * SeaSin(x * 4.5f + v * 2.0f + t * 0.05f) + 1.1f * SeaSin(-x * 7.0f + v * 5.0f + 0.7f);
    float a = SeaSin(x * 9.0f + v * 40.0f - t * 0.45f + wv);
    float b = SeaSin(-x * 5.0f + v * 53.0f - t * 0.6f + 0.8f * wv + 0.9f);
    return 0.6f * a + 0.4f * b;
}

/* This frame's swells, from the view (g_bgUV): rows of corners bottom to top into g_swPos, each corner's alpha for the
 * crests' pass (the sheet's white) and the troughs' (its deep blue) into g_swCol[0] and [1] with their g_swUV, and the
 * strip that joins two rows into g_swIndex (each row's corner above, then below, for the background's own winding).
 * 0 when there is nothing to draw (no sea, or the view too close in). */
static int SwellBuild(float t)
{
    float u0 = g_bgUV[0], u1 = g_bgUV[2], vt = g_bgUV[5], vb = g_bgUV[1];
    float fade = SeaFade((u1 - u0) * 0.5f), asp = (float)g_scrW / (float)g_scrH;
    if (!g_seaW || fade <= 0.01f) return 0;
    int cols = (int)(SWELL_ROWS * asp + 0.5f);
    if (cols < 8) cols = 8;
    if (cols > SWELL_COLS) cols = SWELL_COLS;
    g_swCols = cols;
    for (int j = 0; j <= SWELL_ROWS; j++) {
        float y = (float)j / SWELL_ROWS, v = vb + (vt - vb) * y;
        for (int i = 0; i <= cols; i++) {
            float x = (float)i / cols, u = u0 + (u1 - u0) * x;
            int at = j * (cols + 1) + i;
            g_swPos[at * 3] = x; g_swPos[at * 3 + 1] = y; g_swPos[at * 3 + 2] = 0;
            g_swUV[0][at * 2] = g_swUV[1][at * 2] = SEA_SQUARE_U;
            g_swUV[0][at * 2 + 1] = SEA_WHITE_V; g_swUV[1][at * 2 + 1] = SEA_DEEP_V;
            float m = SeaAt(u, v) * fade, sx = u * asp, sv = v;
            SwirlAt(&sx, &sv, asp);
            float s = m > 0.004f ? SwellAt(sx, sv, t) : 0;
            g_swCol[0][at] = (unsigned)((s > 0 ? s : 0) * m * SWELL_LIGHT * 255.0f + 0.5f) << 24 | 0xFFFFFFu;
            g_swCol[1][at] = (unsigned)((s < 0 ? -s : 0) * m * SWELL_DARK * 255.0f + 0.5f) << 24 | 0xFFFFFFu;
        }
    }
    for (int i = 0; i <= cols; i++) { g_swIndex[2 * i] = (unsigned short)(cols + 1 + i); g_swIndex[2 * i + 1] = (unsigned short)i; }
    return 1;
}

static unsigned SeaHash(unsigned x)
{
    x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16;
    return x;
}
static float SeaRand(unsigned a, unsigned b) { return (float)(SeaHash(a * 0x9E3779B9u ^ SeaHash(b + 0x632BE5ABu)) >> 8) / 16777216.0f; }

/* This frame's glints, from the view and the clock, into g_glPos / g_glUV / g_glCol as one strip (g_glQuads). Each of
 * GLINTS comes back every 2.4 to 4 s; each time, at a fresh place in view, kept only on open sea on a swell's crest,
 * and held there (in the world) for the GLINT_LIFE it shows, swelling and fading, at a steady size on screen. */
static void GlintsBuild(float t)
{
    float u0 = g_bgUV[0], u1 = g_bgUV[2], vt = g_bgUV[5], vb = g_bgUV[1];
    float fade = SeaFade((u1 - u0) * 0.5f), k = (float)g_scrH / 2160.0f;
    g_glQuads = 0;
    if (!g_seaW || fade <= 0.01f) return;
    for (int g = 0; g < GLINTS; g++) {
        float period = 2.4f + 1.6f * SeaRand((unsigned)g, 1), tt = t + period * SeaRand((unsigned)g, 2);
        int n = (int)(tt / period);
        float age = tt - n * period;
        if (!g_glint[g].set || g_glint[g].cycle != n) {
            unsigned id = (unsigned)g * 7919u + (unsigned)n;
            float u = u0 + (u1 - u0) * SeaRand(id, 3), v = vt + (vb - vt) * SeaRand(id, 4);
            g_glint[g].u = u; g_glint[g].v = v; g_glint[g].cycle = n; g_glint[g].set = 1;
            g_glint[g].on = SeaAt(u, v) > 0.6f && SwellAt(u * (float)g_scrW / (float)g_scrH, v, t) > GLINT_CREST;
        }
        if (!g_glint[g].on || age >= GLINT_LIFE) continue;
        float e = SeaSin(3.14159265f * age / GLINT_LIFE);
        e *= e;
        float a = e * fade * 0.9f;
        if (a < 0.01f) continue;
        float sx = (g_glint[g].u - u0) / (u1 - u0), sy = 1 - (g_glint[g].v - vt) / (vb - vt);
        float px = GLINT_PX * k * (0.55f + 0.45f * e), sw = px / (float)g_scrW, sh = px / (float)g_scrH;
        if (sx + sw / 2 < 0 || sx - sw / 2 > 1 || sy + sh / 2 < 0 || sy - sh / 2 > 1) continue;
        float x[4] = { sx - sw / 2, sx + sw / 2, sx - sw / 2, sx + sw / 2 }, y[4] = { sy - sh / 2, sy - sh / 2, sy + sh / 2, sy + sh / 2 };
        static const float tu[4] = { 0, 0.5f, 0, 0.5f }, tv[4] = { 1, 1, 0, 0 };
        QuadPut(g_glPos, g_glUV, g_glCol, g_glQuads++, x, y, tu, tv, (unsigned)(a * 255.0f + 0.5f) << 24 | 0xFFFFFFu);
    }
}

/* ---- the marks (4 Oct 2026, his yeses): the Maelstrom, and the ripple where you arrive ----
 * CinderMarks.blp (DXT5, MARKS_SHEET square) and CinderLoad\\marks.txt: `s <id> <x0> <y0> <x1> <y1>`, the sprites'
 * rectangles (ids: 0 the ripple's ring, 1 the Maelstrom, 2 the badge's ring, 3 the arrival's glow, 4 the badge's
 * shadow, 16 + class id the class icons), and `maelstrom <u> <v> <r>` (float bits in hex: its middle on the picture, its
 * radius in heights of the world). Colours are the sheet's own; a mark's corners only say how much of it shows.
 *   the Maelstrom: a whirlpool in the middle of the Great Sea, in the world's own place and size, turning once in
 *                  MAEL_TURN s the way that draws the eye inward, the swells within SWIRL_REACH of its radius bent
 *                  into its swirl; far out only, fading with the sea
 *   the ripple: RIPPLE_RINGS rings opening slowly from where you arrive, each over RIPPLE_S, at a steady size on screen,
 *               appearing over RIPPLE_IN once the arrival is known; far out only, fading with the sea */
#define MARKS_PATH    "Interface\\Glues\\LoadingScreens\\CinderMarks.blp"
#define MARKS_SHEET   512.0f
#define MARKS_SPRITES 32
#define MARKS_FRAME   16       /* marks a pass at most */
#define MAEL_TURN     36.0f
#define SWIRL_REACH   2.6f
#define SWIRL_TWIST   2.2f     /* radians, at its middle */
#define RIPPLE_RINGS  3
#define RIPPLE_S      3.0f
#define RIPPLE_PX     110.0f   /* a ring at its widest, its radius on a 2160-tall screen */
#define RIPPLE_IN     0.5f
static struct { unsigned short x0, y0, x1, y1; unsigned char ok; } g_mark[MARKS_SPRITES];
static struct { int ok; float u, v, r; } g_mael;
static int g_marksOn, g_marksTried, g_mkQuads;
static void *g_marksTex;
__attribute__((used)) float g_mkPos[MARKS_FRAME * 6 * 3];
__attribute__((used)) float g_mkUV[MARKS_FRAME * 6 * 2];
__attribute__((used)) unsigned g_mkCol[MARKS_FRAME * 6];

static void MarksParse(const char *s, unsigned len)
{
    for (int i = 0; i < MARKS_SPRITES; i++) g_mark[i].ok = 0;
    g_mael.ok = 0;
    for (unsigned at = 0; at < len;) {
        unsigned end = at;
        while (end < len && s[end] != '\n') end++;
        const char *l = s + at;
        int n = (int)(end - at), i = 2, v[5], k = 0;
        unsigned b[3];
        if (n > 2 && l[0] == 's' && MapSpace(l[1])) {
            while (k < 5 && LblInt(l, n, &i, &v[k])) k++;
            if (k == 5 && v[0] >= 0 && v[0] < MARKS_SPRITES && v[1] >= 0 && v[2] >= 0 && v[3] > v[1] && v[4] > v[2] &&
                v[3] <= (int)MARKS_SHEET && v[4] <= (int)MARKS_SHEET) {
                g_mark[v[0]].x0 = (unsigned short)v[1]; g_mark[v[0]].y0 = (unsigned short)v[2];
                g_mark[v[0]].x1 = (unsigned short)v[3]; g_mark[v[0]].y1 = (unsigned short)v[4];
                g_mark[v[0]].ok = 1;
            }
        } else if (n > 10 && l[0] == 'm' && l[1] == 'a' && l[2] == 'e' && l[3] == 'l' && l[4] == 's' && l[5] == 't' &&
                   l[6] == 'r' && l[7] == 'o' && l[8] == 'm' && MapSpace(l[9])) {
            i = 9;
            if (MapHex(l, n, &i, &b[0]) && MapHex(l, n, &i, &b[1]) && MapHex(l, n, &i, &b[2])) {
                float u = MapBits(b[0]), vv = MapBits(b[1]), r = MapBits(b[2]);
                if (u > 0 && u < 1 && vv > 0 && vv < 1 && r > 0.001f && r < 0.3f) {
                    g_mael.u = u; g_mael.v = vv; g_mael.r = r; g_mael.ok = 1;
                }
            }
        }
        at = end + 1;
    }
}

static float SeaCos(float x) { return SeaSin(x + 1.5707963f); }

/* Mark id as a quad into g_mk*: its middle at screen sx, sy, half as tall as hs (in screen heights; square on screen),
 * turned ang radians (anticlockwise on screen), alpha shown. Left out off screen, when faint, or when the pass is full. */
static void MarkPut(int id, float sx, float sy, float hs, float ang, float alpha)
{
    if (id < 0 || id >= MARKS_SPRITES || !g_mark[id].ok || alpha <= 0.004f || g_mkQuads >= MARKS_FRAME) return;
    float k = (float)g_scrH / (float)g_scrW, c = SeaCos(ang), sn = SeaSin(ang), reach = hs * 1.42f;
    if (sx + reach * k < 0 || sx - reach * k > 1 || sy + reach < 0 || sy - reach > 1) return;
    static const float cx[4] = { -1, 1, -1, 1 }, cy[4] = { -1, -1, 1, 1 };
    float x[4], y[4];
    for (int i = 0; i < 4; i++) {
        x[i] = sx + (cx[i] * c - cy[i] * sn) * hs * k;
        y[i] = sy + (cx[i] * sn + cy[i] * c) * hs;
    }
    float tu0 = g_mark[id].x0 / MARKS_SHEET, tu1 = g_mark[id].x1 / MARKS_SHEET;
    float tv0 = g_mark[id].y0 / MARKS_SHEET, tv1 = g_mark[id].y1 / MARKS_SHEET;
    float tu[4] = { tu0, tu1, tu0, tu1 }, tv[4] = { tv1, tv1, tv0, tv0 };
    QuadPut(g_mkPos, g_mkUV, g_mkCol, g_mkQuads++, x, y, tu, tv, (unsigned)(FireClamp(alpha, 0, 1) * 255.0f + 0.5f) << 24 | 0xFFFFFFu);
}

/* Where picture u, v shows on screen, for the view this frame. */
static void ViewToScreen(float u, float v, float *sx, float *sy)
{
    float u0 = g_bgUV[0], u1 = g_bgUV[2], vt = g_bgUV[5], vb = g_bgUV[1];
    *sx = (u - u0) / (u1 - u0);
    *sy = 1 - (v - vt) / (vb - vt);
}

/* This frame's Maelstrom: one turned quad in g_mk* (g_mkQuads 0 or 1). */
static void MaelstromBuild(float t)
{
    g_mkQuads = 0;
    float fade = SeaFade((g_bgUV[2] - g_bgUV[0]) * 0.5f);
    if (!g_mael.ok || fade <= 0.01f) return;
    float sx, sy, turns = t / MAEL_TURN;
    ViewToScreen(g_mael.u, g_mael.v, &sx, &sy);
    turns -= (float)(int)turns;
    MarkPut(1, sx, sy, g_mael.r / (g_bgUV[1] - g_bgUV[5]), 6.2831853f * turns, fade);
}

/* The swells near the Maelstrom are drawn from a place turned about its middle, the more the nearer: x across and v
 * down the picture in heights of the world, as SwellAt takes them. */
static void SwirlAt(float *x, float *v, float asp)
{
    if (!g_mael.ok) return;
    float cx = g_mael.u * asp, cv = g_mael.v, dx = *x - cx, dv = *v - cv, reach = g_mael.r * SWIRL_REACH;
    float d2 = (dx * dx + dv * dv) / (reach * reach);
    if (d2 >= 1) return;
    float w = (1 - d2) * (1 - d2), a = -SWIRL_TWIST * w, c = SeaCos(a), s = SeaSin(a);   /* with its arms: clockwise inward */
    *x = cx + dx * c - dv * s;
    *v = cv + dx * s + dv * c;
}

/* This frame's ripple: up to RIPPLE_RINGS rings in g_mk*, opening from the arrival once it is known. */
static void RippleBuild(float t, DWORD now)
{
    g_mkQuads = 0;
    float fade = SeaFade((g_bgUV[2] - g_bgUV[0]) * 0.5f);
    if (!g_destOk || fade <= 0.01f) return;
    float sx, sy, appear = FireClamp((float)(now - g_destSince) / (RIPPLE_IN * 1000.0f), 0, 1);
    ViewToScreen(g_destU, g_destV, &sx, &sy);
    for (int k = 0; k < RIPPLE_RINGS; k++) {
        float ph = t / RIPPLE_S + (float)k / RIPPLE_RINGS;
        ph -= (float)(int)ph;
        float px = RIPPLE_PX * (0.12f + 0.88f * ph), a = (1 - ph) * (float)FireExp(0.6 * MapLn(1 - ph + 1e-6f));
        a *= FireClamp(ph / 0.08f, 0, 1) * fade * appear;
        MarkPut(0, sx, sy, px / 2160.0f, 0, a);                       /* the ring's size steady on any screen height */
    }
}

/* ---- the badge: your class where you arrive (his yes, 4 Oct 2026: a class icon in a gold ring, a dark outline, a drop
 * shadow, no blinking) ----
 * The class, read from WoW.exe 1.12.1 (4 Oct 2026): the character select screen's list is a count at VA 0xB42140 and an
 * array at [0xB42144], 0x120 bytes a character (GetCharacterInfo, VA 0x4732A0: name +0x08, race +0x100, class +0x101,
 * level +0x108); the one picked is [0x83856C] (SelectCharacter, VA 0x472740 stores it); entering the world (VA 0x46B500)
 * keeps that character's entry at [0xB41DF4]. The list is read whenever it is there and the class kept for the session,
 * so a screen after the list has gone (if the game clears it in the world) still shows the last one read; the log says
 * where each screen's came from, and what the entered character's own entry says, to learn which holds. */
#define BADGE_PX        36.0f   /* the gold ring's half size on a 2160-tall screen (bigger reads better) */
static volatile LONG g_class, g_classFrom, g_classEntered;   /* the class (0 none); 1 the list, 2 kept; the entry's own */
static const struct { DWORD rva; int n; unsigned char bytes[8]; const char *what; } kClassCode[] = {
    { 0x0732E2, 6, { 0x3B, 0x05, 0x40, 0x21, 0xB4, 0x00 }, "GetCharacterInfo's count" },
    { 0x0732EE, 6, { 0x8B, 0x15, 0x44, 0x21, 0xB4, 0x00 }, "GetCharacterInfo's list" },
    { 0x0732F6, 6, { 0x8D, 0x3C, 0xC0, 0xC1, 0xE7, 0x05 }, "GetCharacterInfo's 0x120 a character" },
    { 0x073342, 7, { 0x0F, 0xB6, 0x87, 0x01, 0x01, 0x00, 0x00 }, "GetCharacterInfo's class at +0x101" },
    { 0x072754, 5, { 0xA3, 0x6C, 0x85, 0x83, 0x00 }, "SelectCharacter's pick" },
    { 0x06B539, 5, { 0xA3, 0xF4, 0x1D, 0xB4, 0x00 }, "entering the world's character" },
    { 0x1B4435, 6, { 0x8D, 0x46, 0x38, 0x50, 0x8B, 0xCF }, "the character list's map at +0x38" },
    { 0x1B4440, 6, { 0x8D, 0x4E, 0x44, 0x51, 0x8B, 0xCF }, "its x at +0x44" },
    { 0x1B444B, 6, { 0x8D, 0x56, 0x48, 0x52, 0x8B, 0xCF }, "its y at +0x48" } };
#define CLASS_CODE ((int)(sizeof kClassCode / sizeof kClassCode[0]))

/* 1 when every place the class's addresses were read from holds the bytes it did in the client this was made for. */
static int ClassCodeHolds(const unsigned char *base)
{
    for (int i = 0; i < CLASS_CODE; i++)
        if (!Readable(base + kClassCode[i].rva, kClassCode[i].n) || !Same(base + kClassCode[i].rva, kClassCode[i].bytes, kClassCode[i].n))
            return 0;
    return 1;
}

static int ClassOk(unsigned c) { return c == 1 || c == 2 || c == 3 || c == 4 || c == 5 || c == 7 || c == 8 || c == 9 || c == 11; }

/* The class byte of a character list entry at client address entry, or 0. */
static unsigned EntryClass(unsigned entry)
{
    const unsigned char *p = ClientAt(entry + CHAR_CLASS);
    if (!entry || !p || !Readable(p, 1)) return 0;
    unsigned c = *(volatile const unsigned char *)p;
    return ClassOk(c) ? c : 0;
}

/* The game's thread, each frame of a map screen: the class as the list says, else the one kept. */
static void ClassStep(void)
{
    unsigned count, list, picked, entered, c = 0;
    if (!g_classSure) return;
    if (ClientWord(CHAR_COUNT_VA, &count) && ClientWord(CHAR_LIST_VA, &list) && ClientWord(CHAR_PICKED_VA, &picked) &&
        list && count > 0 && count <= 64 && picked < count)
        c = EntryClass(list + picked * CHAR_SIZE);
    if (c) { InterlockedExchange(&g_class, (LONG)c); InterlockedExchange(&g_classFrom, 1); }
    else if (g_class) InterlockedExchange(&g_classFrom, 2);
    InterlockedExchange(&g_classEntered, ClientWord(CHAR_ENTERED_VA, &entered) ? (LONG)EntryClass(entered) : 0);
}

/* The worker, as a map screen ends: where its class came from (m: 220 bytes of room). */
static void BadgeLog(char *m)
{
    static const char *const kClass[12] = { "none", "warrior", "paladin", "hunter", "rogue", "priest", "?", "shaman", "mage",
                                            "warlock", "?", "druid" };
    if (!g_marksOn || !g_mapShown) return;
    LONG c = g_class, e = g_classEntered;
    wsprintfA(m, "  its badge: %s%s; the entered character's own entry says %s", kClass[c >= 0 && c < 12 ? c : 0],
              !c ? " (no class read yet)" : (g_classFrom == 1 ? ", from the character list" :
                                             ", kept from an earlier screen (the list is not there now)"),
              kClass[e >= 0 && e < 12 ? e : 0]);
    Log(m);
}

/* This frame's badge at the arrival, once known: its shadow, the class's icon, the gold ring; a steady size on screen,
 * appearing over RIPPLE_IN. Without a class, the ring alone. */
static void BadgeBuild(DWORD now)
{
    g_mkQuads = 0;
    if (!g_destOk || !g_mark[2].ok) return;
    float sx, sy, hs = BADGE_PX / 2160.0f, a = FireClamp((float)(now - g_destSince) / (RIPPLE_IN * 1000.0f), 0, 1);
    ViewToScreen(g_destU, g_destV, &sx, &sy);
    MarkPut(4, sx, sy, hs * 1.15f, 0, a);
    if (g_class) MarkPut(16 + (int)g_class, sx, sy, hs * 0.81f, 0, a);
    MarkPut(2, sx, sy, hs, 0, a);
}

/* ---- the arrival's glow (his yes, 4 Oct 2026: the subzone you arrive in lights up at the landing) ----
 * From the world map's own explored-area overlays (the pieces of a zone's map that light up as you explore it), made by
 * the art kit's glow.py: CinderGlow.blp (DXT5, GLOW_SHEET square), CinderLoad\\glow.txt (`s <id> <x0> <y0> <x1> <y1>`,
 * a glow's rectangle on the sheet; `g <sprite> <u0> <v0> <u1> <v1>`, a subzone's glow and where it lies on the picture)
 * and CinderLoad\\glow.bin ('CGLW', the number of maps, then each: map id, cell yards, the grid's corner x0, y0 in world
 * yards, its columns along y and rows along x, then a 16-bit cell each: 0 none, else the subzone's g line + 1). Once the
 * arrival is known, its subzone glows, coming up as the view closes in (from GLOW_FROM to GLOW_FULL); where no subzone
 * is (a city's middle, open ground), the marks' round glow, GLOW_YARDS across, in its place. */
#define GLOW_PATH     "Interface\\Glues\\LoadingScreens\\CinderGlow.blp"
#define GLOW_SHEET    2048.0f
#define GLOW_SPRITES  1024
#define GLOW_CELLS    65536
#define GLOW_FROM     0.16f
#define GLOW_FULL     0.08f
#define GLOW_YARDS    450.0f
static struct { unsigned short x0, y0, x1, y1; } g_glowSpr[GLOW_SPRITES];
static struct { float u0, v0, u1, v1; unsigned short spr; } g_glowItem[GLOW_SPRITES];
static struct { int ok, cols, rows; float cell, x0, y0; unsigned at; } g_glowMap[2];
static unsigned short g_glowCells[GLOW_CELLS];
static int g_glowSprites, g_glowItems, g_glowOn, g_glowTried, g_glowPick = -1, g_glowQuads;
static DWORD g_glowFor;                                         /* the arrival the pick was made for (g_destSince) */
static void *g_glowTex;
__attribute__((used)) float g_glowPos[4 * 3];
__attribute__((used)) float g_glowUV[4 * 2];
__attribute__((used)) unsigned g_glowCol[4];

static void GlowParse(const char *s, unsigned len)
{
    g_glowSprites = g_glowItems = 0;
    for (unsigned at = 0; at < len;) {
        unsigned end = at;
        while (end < len && s[end] != '\n') end++;
        const char *l = s + at;
        int n = (int)(end - at), i = 2, v[5], k = 0;
        unsigned b[4];
        if (n > 2 && l[0] == 's' && MapSpace(l[1])) {
            while (k < 5 && LblInt(l, n, &i, &v[k])) k++;
            if (k == 5 && v[0] == g_glowSprites && v[0] < GLOW_SPRITES && v[1] >= 0 && v[2] >= 0 && v[3] > v[1] &&
                v[4] > v[2] && v[3] <= (int)GLOW_SHEET && v[4] <= (int)GLOW_SHEET) {
                g_glowSpr[v[0]].x0 = (unsigned short)v[1]; g_glowSpr[v[0]].y0 = (unsigned short)v[2];
                g_glowSpr[v[0]].x1 = (unsigned short)v[3]; g_glowSpr[v[0]].y1 = (unsigned short)v[4];
                g_glowSprites++;
            }
        } else if (n > 2 && l[0] == 'g' && MapSpace(l[1]) && g_glowItems < GLOW_SPRITES && LblInt(l, n, &i, &v[0]) &&
                   MapHex(l, n, &i, &b[0]) && MapHex(l, n, &i, &b[1]) && MapHex(l, n, &i, &b[2]) && MapHex(l, n, &i, &b[3])) {
            float u0 = MapBits(b[0]), v0 = MapBits(b[1]), u1 = MapBits(b[2]), v1 = MapBits(b[3]);
            if (v[0] >= 0 && v[0] < g_glowSprites && MapFinite(u0) && MapFinite(v0) && MapFinite(u1) && MapFinite(v1) &&
                u1 > u0 && v1 > v0 && u0 > -1 && u1 < 2 && v0 > -1 && v1 < 2) {
                g_glowItem[g_glowItems].u0 = u0; g_glowItem[g_glowItems].v0 = v0;
                g_glowItem[g_glowItems].u1 = u1; g_glowItem[g_glowItems].v1 = v1;
                g_glowItem[g_glowItems].spr = (unsigned short)v[0];
                g_glowItems++;
            } else
                g_glowItems = GLOW_SPRITES + 1;                     /* a bad g line: its numbering is lost, so none */
        }
        at = end + 1;
    }
    if (g_glowItems > GLOW_SPRITES) g_glowItems = 0;
}

static float GlowF(const unsigned char *p) { union { unsigned u; float f; } c; c.u = (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24; return c.f; }

/* glow.bin, checked whole: 1 when its grids are used. */
static int GlowGridParse(const unsigned char *d, unsigned len)
{
    g_glowMap[0].ok = g_glowMap[1].ok = 0;
    if (len < 6 || d[0] != 'C' || d[1] != 'G' || d[2] != 'L' || d[3] != 'W') return 0;
    unsigned maps = d[4] | d[5] << 8, at = 6, cells = 0;
    for (unsigned k = 0; k < maps; k++) {
        if (len - at < 18) return g_glowMap[0].ok = g_glowMap[1].ok = 0;
        unsigned m = d[at] | d[at + 1] << 8, cols = d[at + 14] | d[at + 15] << 8, rows = d[at + 16] | d[at + 17] << 8;
        float cell = GlowF(d + at + 2), x0 = GlowF(d + at + 6), y0 = GlowF(d + at + 10);
        at += 18;
        if (m > 1 || !cols || !rows || !(cell > 1 && cell < 10000) || !MapFinite(x0) || !MapFinite(y0) ||
            cols * rows > GLOW_CELLS - cells || len - at < cols * rows * 2)
            return g_glowMap[0].ok = g_glowMap[1].ok = 0;
        g_glowMap[m].cols = (int)cols; g_glowMap[m].rows = (int)rows; g_glowMap[m].cell = cell;
        g_glowMap[m].x0 = x0; g_glowMap[m].y0 = y0; g_glowMap[m].at = cells; g_glowMap[m].ok = 1;
        for (unsigned i = 0; i < cols * rows; i++) g_glowCells[cells + i] = (unsigned short)(d[at + 2 * i] | d[at + 2 * i + 1] << 8);
        cells += cols * rows;
        at += cols * rows * 2;
    }
    return at == len && (g_glowMap[0].ok || g_glowMap[1].ok);
}

/* The subzone at world x, y on map m: its g line's index, or -1. */
static int GlowFind(int m, float x, float y)
{
    if (m < 0 || m > 1 || !g_glowMap[m].ok) return -1;
    float fi = (y - g_glowMap[m].y0) / g_glowMap[m].cell, fj = (x - g_glowMap[m].x0) / g_glowMap[m].cell;
    if (!(fi >= 0 && fj >= 0 && fi < g_glowMap[m].cols && fj < g_glowMap[m].rows)) return -1;
    unsigned c = g_glowCells[g_glowMap[m].at + (unsigned)(int)fj * (unsigned)g_glowMap[m].cols + (unsigned)(int)fi];
    return c && (int)c <= g_glowItems ? (int)c - 1 : -1;
}

/* This frame's glow: the arrival's subzone (one quad in g_glow*, g_glowQuads 1) or the round glow in its place (into the
 * marks' g_mk*, drawn with their sheet); how strongly, for how close the view has come. */
static void GlowBuild(DWORD now)
{
    g_glowQuads = 0;
    g_mkQuads = 0;
    if (!g_destOk) return;
    if (g_glowFor != g_destSince || g_glowPick < -1) {             /* this screen's arrival: which subzone, once */
        g_glowFor = g_destSince;
        g_glowPick = GlowFind(g_destMap, g_destWX, g_destWY);
    }
    float h = (g_bgUV[2] - g_bgUV[0]) * 0.5f, k = FireClamp((GLOW_FROM - h) / (GLOW_FROM - GLOW_FULL), 0, 1);
    float a = k * k * (3 - 2 * k) * FireClamp((float)(now - g_destSince) / (RIPPLE_IN * 1000.0f), 0, 1);
    if (a <= 0.004f) return;
    if (g_glowPick >= 0) {
        float u0 = g_bgUV[0], u1 = g_bgUV[2], vt = g_bgUV[5], vb = g_bgUV[1];
        int it = g_glowPick, sp = g_glowItem[it].spr;
        float x0 = (g_glowItem[it].u0 - u0) / (u1 - u0), x1 = (g_glowItem[it].u1 - u0) / (u1 - u0);
        float yt = 1 - (g_glowItem[it].v0 - vt) / (vb - vt), yb2 = 1 - (g_glowItem[it].v1 - vt) / (vb - vt);
        float x[4] = { x0, x1, x0, x1 }, y[4] = { yb2, yb2, yt, yt };
        float tu0 = g_glowSpr[sp].x0 / GLOW_SHEET, tu1 = g_glowSpr[sp].x1 / GLOW_SHEET;
        float tv0 = g_glowSpr[sp].y0 / GLOW_SHEET, tv1 = g_glowSpr[sp].y1 / GLOW_SHEET;
        float tu[4] = { tu0, tu1, tu0, tu1 }, tv[4] = { tv1, tv1, tv0, tv0 };
        QuadPut(g_glowPos, g_glowUV, g_glowCol, 0, x, y, tu, tv, (unsigned)(a * 255.0f + 0.5f) << 24 | 0xFFFFFFu);
        g_glowQuads = 1;
    } else if (g_mark[3].ok) {
        float sx, sy, bv = g_destMap >= 0 && g_destMap <= 1 && g_map.m[g_destMap].ok ? g_map.m[g_destMap].bv : 0;
        if (bv < 0) bv = -bv;
        if (bv <= 0) return;
        ViewToScreen(g_destU, g_destV, &sx, &sy);
        MarkPut(3, sx, sy, GLOW_YARDS * 0.5f * bv / (g_bgUV[1] - g_bgUV[5]), 0, a);
    }
}

/* ---- the log's black (4 Oct 2026, his find: "a log that's being lit on fire should be solid black not transparent"):
 * every other screen's picture is painted black where the log lies (make_pack.py --reveal), so the log's open cracks
 * show black ahead of the fire; the world map is one picture that zooms, so it cannot be, and the map showed through.
 * On the map's screens the overlay ends with that same black band, over everything it drew and under the bar. */
__attribute__((used)) float g_blackPos[4 * 3];
__attribute__((used)) float g_blackUV[4 * 2];
__attribute__((used)) unsigned g_blackCol[4];
static void BarBlackBuild(void)
{
    static const float x[4] = { 0, 1, 0, 1 }, y[4] = { 0, 0, BAR_BLACK_H, BAR_BLACK_H };
    static const float tu[4] = { SEA_SQUARE_U, SEA_SQUARE_U, SEA_SQUARE_U, SEA_SQUARE_U };
    static const float tv[4] = { SEA_DEEP_V, SEA_DEEP_V, SEA_DEEP_V, SEA_DEEP_V };
    QuadPut(g_blackPos, g_blackUV, g_blackCol, 0, x, y, tu, tv, 0xFFFFFFFFu);
}

/* ---- the names and icons (4 Oct 2026): CinderLabels.blp (DXT5, one sheet of every sprite) and CinderLoad\labels.txt:
 *   s <id> <x0> <y0> <x1> <y1>              a sprite's rectangle on the sheet (4096 x 2048)
 *   i <sprite> <u> <v> <hmax> <hmin> <dy>    an item at picture u, v (float bits in hex), shown while the view's half
 *                                           height h is between hmin and hmax thousandths, dy pixels below its place
 * Sprites are sized for a 2160-tall screen; each frame the items the view's h allows are drawn at a steady size,
 * fading in over the top fifth of their range and out over the bottom fifth, batched into strips. */
#define LBL_SHEET_W 4096.0f
#define LBL_SHEET_H 2048.0f
#define LBL_SPRITES 1024
#define LBL_ITEMS   2048
#define LBL_FRAME   480                        /* quads a frame at most */
static struct { unsigned short x0, y0, x1, y1; } g_lblSpr[LBL_SPRITES];
static struct { float u, v; unsigned short spr, hmax, hmin; short dy; } g_lblItem[LBL_ITEMS];
static int g_lblSprites, g_lblItems, g_lblOn, g_lblTried, g_lblQuads;
static void *g_lblTex;
__attribute__((used)) float g_lblPos[LBL_FRAME * 6 * 3];
__attribute__((used)) float g_lblUV[LBL_FRAME * 6 * 2];
__attribute__((used)) unsigned g_lblCol[LBL_FRAME * 6];
__attribute__((used)) unsigned short g_lblIndex[LBL_BATCH * 6];

static int LblInt(const char *s, int n, int *at, int *out)
{
    while (*at < n && MapSpace(s[*at])) (*at)++;
    int neg = *at < n && s[*at] == '-';
    if (neg) (*at)++;
    int v = 0, d = 0;
    for (; *at < n && s[*at] >= '0' && s[*at] <= '9' && d < 7; (*at)++, d++) v = v * 10 + (s[*at] - '0');
    if (!d) return 0;
    *out = neg ? -v : v;
    return 1;
}
static void LabelsParse(const char *s, unsigned len)
{
    g_lblSprites = g_lblItems = 0;
    for (unsigned at = 0; at < len;) {
        unsigned end = at;
        while (end < len && s[end] != '\n') end++;
        const char *l = s + at;
        int n = (int)(end - at), i = 2, a, b, c, d, e;
        unsigned hu, hv;
        if (n > 2 && l[0] == 's' && MapSpace(l[1]) && LblInt(l, n, &i, &a) && LblInt(l, n, &i, &b) &&
            LblInt(l, n, &i, &c) && LblInt(l, n, &i, &d) && LblInt(l, n, &i, &e) && a == g_lblSprites &&
            a < LBL_SPRITES && b >= 0 && c >= 0 && d > b && e > c && d <= 4096 && e <= 2048) {
            g_lblSpr[a].x0 = (unsigned short)b; g_lblSpr[a].y0 = (unsigned short)c;
            g_lblSpr[a].x1 = (unsigned short)d; g_lblSpr[a].y1 = (unsigned short)e;
            g_lblSprites++;
        } else if (n > 2 && l[0] == 'i' && MapSpace(l[1]) && g_lblItems < LBL_ITEMS && LblInt(l, n, &i, &a) &&
                   MapHex(l, n, &i, &hu) && MapHex(l, n, &i, &hv) && LblInt(l, n, &i, &b) && LblInt(l, n, &i, &c) &&
                   LblInt(l, n, &i, &d) && a >= 0 && a < g_lblSprites && b > c && c >= 0 && b <= 1000) {
            float u = MapBits(hu), v = MapBits(hv);
            if (MapFinite(u) && MapFinite(v)) {
                g_lblItem[g_lblItems].u = u; g_lblItem[g_lblItems].v = v;
                g_lblItem[g_lblItems].spr = (unsigned short)a;
                g_lblItem[g_lblItems].hmax = (unsigned short)b; g_lblItem[g_lblItems].hmin = (unsigned short)c;
                g_lblItem[g_lblItems].dy = (short)d;
                g_lblItems++;
            }
        }
        at = end + 1;
    }
}

/* This frame's quads, from the view (g_bgUV): into g_lblPos / g_lblUV / g_lblCol as strips of LBL_BATCH quads, each
 * after the first joined by two repeated corners (degenerate triangles). g_lblQuads: how many. */
static void LabelsBuild(void)
{
    float u0 = g_bgUV[0], u1 = g_bgUV[2], vt = g_bgUV[5], vb = g_bgUV[1];
    float hk = (u1 - u0) * 500.0f, k = (float)g_scrH / 2160.0f;
    int q = 0;
    for (int it = 0; it < g_lblItems && q < LBL_FRAME; it++) {
        float hi = g_lblItem[it].hmax, lo = g_lblItem[it].hmin;
        if (!(hk <= hi && hk >= lo)) continue;
        float a = 1, span = hi - lo;
        if (hi < 500 && hk > hi - span * 0.2f) a = (hi - hk) / (span * 0.2f);    /* fading in, coming closer */
        if (lo > 0 && hk < lo + span * 0.2f) { float b = (hk - lo) / (span * 0.2f); if (b < a) a = b; }
        if (a <= 0.01f) continue;
        float sx = (g_lblItem[it].u - u0) / (u1 - u0), sy = 1 - (g_lblItem[it].v - vt) / (vb - vt);
        int s = g_lblItem[it].spr;
        float sw = (g_lblSpr[s].x1 - g_lblSpr[s].x0) * k / (float)g_scrW, sh = (g_lblSpr[s].y1 - g_lblSpr[s].y0) * k / (float)g_scrH;
        sy -= g_lblItem[it].dy * k / (float)g_scrH;
        if (sx + sw / 2 < 0 || sx - sw / 2 > 1 || sy + sh / 2 < 0 || sy - sh / 2 > 1) continue;
        float x[4] = { sx - sw / 2, sx + sw / 2, sx - sw / 2, sx + sw / 2 }, y[4] = { sy - sh / 2, sy - sh / 2, sy + sh / 2, sy + sh / 2 };
        float tu0 = g_lblSpr[s].x0 / LBL_SHEET_W, tu1 = g_lblSpr[s].x1 / LBL_SHEET_W;
        float tv0 = g_lblSpr[s].y0 / LBL_SHEET_H, tv1 = g_lblSpr[s].y1 / LBL_SHEET_H;
        float tu[4] = { tu0, tu1, tu0, tu1 }, tv[4] = { tv1, tv1, tv0, tv0 };
        QuadPut(g_lblPos, g_lblUV, g_lblCol, q++, x, y, tu, tv, (unsigned)(FireClamp(a, 0, 1) * 255.0f + 0.5f) << 24 | 0xFFFFFFu);
    }
    g_lblQuads = q;
}

/* ---- the RavenCraft logo (4 Oct 2026, his ask: "so that people know that it's Ravencraft") ----
 * CinderLogo.blp (DXT5) and CinderLoad\logo.txt: `logo <sheet w> <sheet h> <x0> <y0> <x1> <y1>`, the logo's rectangle
 * on its sheet. His picks: the old wide raven over RAVENCRAFT, without its 1.18.1 plaque, fixed at the top centre of
 * the map's screens at a steady size the whole load, over everything else. LOGO_H of the screen's height, narrowed to
 * at most LOGO_W of its width (on a narrow screen it would reach the continents' titles), LOGO_TOP down from the top. */
#define LOGO_PATH "Interface\\Glues\\LoadingScreens\\CinderLogo.blp"
#define LOGO_H    0.12f
#define LOGO_W    0.13f
#define LOGO_TOP  0.02f
static struct { int ok; float tu0, tv0, tu1, tv1, aspect; } g_logo;   /* aspect: the logo's width over height, in pixels */
static int g_logoOn, g_logoTried;
static void *g_logoTex;
__attribute__((used)) float g_logoPos[4 * 3];
__attribute__((used)) float g_logoUV[4 * 2];
__attribute__((used)) unsigned g_logoCol[4];

static void LogoParse(const char *s, unsigned len)
{
    g_logo.ok = 0;
    for (unsigned at = 0; at < len;) {
        unsigned end = at;
        while (end < len && s[end] != '\n') end++;
        const char *l = s + at;
        int n = (int)(end - at), i = 4, v[6], k = 0;
        if (n > 5 && l[0] == 'l' && l[1] == 'o' && l[2] == 'g' && l[3] == 'o' && MapSpace(l[4]))
            while (k < 6 && LblInt(l, n, &i, &v[k])) k++;
        if (k == 6 && v[0] > 0 && v[1] > 0 && v[0] <= 4096 && v[1] <= 4096 && v[2] >= 0 && v[3] >= 0 && v[4] > v[2] &&
            v[5] > v[3] && v[4] <= v[0] && v[5] <= v[1]) {
            g_logo.tu0 = (float)v[2] / v[0]; g_logo.tu1 = (float)v[4] / v[0];
            g_logo.tv0 = (float)v[3] / v[1]; g_logo.tv1 = (float)v[5] / v[1];
            g_logo.aspect = (float)(v[4] - v[2]) / (float)(v[5] - v[3]);
            g_logo.ok = 1;
            return;
        }
        at = end + 1;
    }
}

/* The logo's one quad for this screen: top centre, LOGO_H tall unless that would make it wider than LOGO_W. */
static void LogoBuild(void)
{
    float h = LOGO_H, w = h * g_logo.aspect * (float)g_scrH / (float)g_scrW;
    if (w > LOGO_W) { h *= LOGO_W / w; w = LOGO_W; }
    float top = 1 - LOGO_TOP, x[4] = { 0.5f - w / 2, 0.5f + w / 2, 0.5f - w / 2, 0.5f + w / 2 };
    float y[4] = { top - h, top - h, top, top }, tu[4] = { g_logo.tu0, g_logo.tu1, g_logo.tu0, g_logo.tu1 };
    float tv[4] = { g_logo.tv1, g_logo.tv1, g_logo.tv0, g_logo.tv0 };
    QuadPut(g_logoPos, g_logoUV, g_logoCol, 0, x, y, tu, tv, 0xFFFFFFFFu);
}

#if defined(__i386__)
typedef void (__attribute__((fastcall)) *OvSetup)(int, const float *, int, int, int, const unsigned *, int, int, int,
                                                  const float *, int, int, int);
typedef void *(__attribute__((fastcall)) *OvDevice)(void *, int, int);
typedef void (__attribute__((fastcall)) *OvBind)(int, void *);
typedef void (__attribute__((fastcall)) *OvDraw)(int, int, const void *);
typedef void (__attribute__((fastcall)) *OvAfter)(void);
typedef void *(__attribute__((fastcall)) *OvFlags)(void *, int, int, int, int, int, int, int, int, int);
typedef void *(__attribute__((fastcall)) *OvLoad)(const char *, void *, unsigned, int, int);
#define OV_FN(type, va) ((type)(void *)((ULONG_PTR)GetModuleHandleA(NULL) + ((va) - 0x400000u)))

/* A picture of the overlay's, loaded once the way the screen's picture is (0x58A980 flags, then the loader). */
static void *OverlayLoad(const char *path)
{
    unsigned slot[4] = { 0 }, status[8] = { 0 };
    void *f = OV_FN(OvFlags, 0x58A980)(slot, 0, 1, 0, 0, 0, 0, 0, 1, 0);
    unsigned flags = f ? *(unsigned *)f : 0;
    return ((OvLoad)g_barLoader)(path, status, flags, 0, 0);
}

/* Quads put by QuadPut, drawn as strips of LBL_BATCH with the picture dev. */
static void OverlayStrips(void *dev, const float *pos, const float *uv, const unsigned *col, int quads)
{
    for (int q = 0; q < quads; q += LBL_BATCH) {
        int n = quads - q < LBL_BATCH ? quads - q : LBL_BATCH, first = (q / LBL_BATCH) * (LBL_BATCH * 6 - 2);
        int corners = n * 6 - 2;
        OV_FN(OvSetup, 0x58A2A0)(corners, pos + first * 3, 12, 0, 0, col + first, 4, 0, 0, uv + first * 2, 8, 0, 0);
        OV_FN(OvBind, 0x589E80)(0x17, dev);
        OV_FN(OvDraw, 0x58A2E0)(4, corners, g_lblIndex);
        OV_FN(OvAfter, 0x58A340)();
    }
}

/* From RevealStub, on the game's thread, before the fill's own setup: the overlay's quads, the sea under the names. */
void WINAPI OverlayDraw(void) __attribute__((used));
void WINAPI OverlayDraw(void)
{
    if (!g_ovOn || !g_mapShown) return;
    DWORD now = GetTickCount();
    float t = (float)(now % 3600000u) / 1000.0f;
    if (g_seaOn && !g_seaTex && !g_seaTried) { g_seaTried = 1; g_seaTex = OverlayLoad(SEA_PATH); }
    if (g_marksOn && !g_marksTex && !g_marksTried) { g_marksTried = 1; g_marksTex = OverlayLoad(MARKS_PATH); }
    if (g_seaTex) {
        if (SwellBuild(t)) {
            void *dev = OV_FN(OvDevice, 0x44ACF0)(g_seaTex, 1, 0);
            int row = g_swCols + 1;
            for (int k = 0; k < 2; k++)                            /* the crests, then the troughs */
                for (int j = 0; j < SWELL_ROWS; j++) {             /* a strip from each row up to the next */
                    OV_FN(OvSetup, 0x58A2A0)(row * 2, g_swPos + j * row * 3, 12, 0, 0, g_swCol[k] + j * row, 4, 0, 0,
                                             g_swUV[k] + j * row * 2, 8, 0, 0);
                    OV_FN(OvBind, 0x589E80)(0x17, dev);
                    OV_FN(OvDraw, 0x58A2E0)(4, row * 2, g_swIndex);
                    OV_FN(OvAfter, 0x58A340)();
                }
        }
    }
    if (g_marksTex) {                                              /* the Maelstrom, on the sea and under its glints */
        MaelstromBuild(t);
        if (g_mkQuads) OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_marksTex, 1, 0), g_mkPos, g_mkUV, g_mkCol, g_mkQuads);
    }
    if (g_seaTex) {
        GlintsBuild(t);
        if (g_glQuads) OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_seaTex, 1, 0), g_glPos, g_glUV, g_glCol, g_glQuads);
    }
    if (g_marksTex) {                                              /* the ripple, under the names */
        RippleBuild(t, now);
        if (g_mkQuads) OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_marksTex, 1, 0), g_mkPos, g_mkUV, g_mkCol, g_mkQuads);
    }
    if (g_glowOn && !g_glowTex && !g_glowTried) { g_glowTried = 1; g_glowTex = OverlayLoad(GLOW_PATH); }
    if (g_glowTex) {                                               /* the arrival's subzone lights up, under the names */
        GlowBuild(now);
        if (g_glowQuads) OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_glowTex, 1, 0), g_glowPos, g_glowUV, g_glowCol, 1);
        if (g_mkQuads && g_marksTex)
            OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_marksTex, 1, 0), g_mkPos, g_mkUV, g_mkCol, g_mkQuads);
    }
    if (g_lblOn && !g_lblTex && !g_lblTried) { g_lblTried = 1; g_lblTex = OverlayLoad(LABELS_PATH); }
    if (g_lblTex) {
        LabelsBuild();
        OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_lblTex, 1, 0), g_lblPos, g_lblUV, g_lblCol, g_lblQuads);
    }
    if (g_marksTex) {                                              /* the badge: over the names, never hidden by one */
        ClassStep();
        BadgeBuild(now);
        if (g_mkQuads) OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_marksTex, 1, 0), g_mkPos, g_mkUV, g_mkCol, g_mkQuads);
    }
    if (g_logoOn && !g_logoTex && !g_logoTried) { g_logoTried = 1; g_logoTex = OverlayLoad(LOGO_PATH); }
    if (g_logoTex) {                                               /* over everything else */
        LogoBuild();
        OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_logoTex, 1, 0), g_logoPos, g_logoUV, g_logoCol, 1);
    }
    if (g_seaTex) {                                                /* last: the log's black, under the bar */
        BarBlackBuild();
        OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_seaTex, 1, 0), g_blackPos, g_blackUV, g_blackCol, 1);
    }
}
#endif

/* At start, with the world map on: the overlay, when its calls are where they should be and its picture is in the pack. */
static void OverlayStart(void)
{
    g_ovOn = g_seaOn = g_lblOn = g_logoOn = g_marksOn = g_glowOn = 0;
    if (!g_map.ok || !g_barLoader) return;
    if (BarHasTexture("Interface\\Glues\\LoadingScreens\\CinderSea")) {
        unsigned char *data = NULL;
        unsigned len = 0;
        if (mpq_read(&kIoSmall, "patch-~.MPQ", "CinderLoad\\sea.bin", &data, &len) == MPQ_OK && data)
            g_seaOn = SeaParse(data, len);
        if (data) HeapFree(GetProcessHeap(), 0, data);
    }
    if (BarHasTexture("Interface\\Glues\\LoadingScreens\\CinderLabels")) {
        unsigned char *text = NULL;
        unsigned len = 0;
        if (mpq_read(&kIoSmall, "patch-~.MPQ", "CinderLoad\\labels.txt", &text, &len) == MPQ_OK && text)
            LabelsParse((const char *)text, len);
        if (text) HeapFree(GetProcessHeap(), 0, text);
        g_lblOn = g_lblItems > 0;
    }
    if (BarHasTexture("Interface\\Glues\\LoadingScreens\\CinderLogo")) {
        unsigned char *text = NULL;
        unsigned len = 0;
        if (mpq_read(&kIoSmall, "patch-~.MPQ", "CinderLoad\\logo.txt", &text, &len) == MPQ_OK && text)
            LogoParse((const char *)text, len);
        if (text) HeapFree(GetProcessHeap(), 0, text);
        g_logoOn = g_logo.ok;
    }
    if (BarHasTexture("Interface\\Glues\\LoadingScreens\\CinderMarks")) {
        unsigned char *text = NULL;
        unsigned len = 0;
        if (mpq_read(&kIoSmall, "patch-~.MPQ", "CinderLoad\\marks.txt", &text, &len) == MPQ_OK && text)
            MarksParse((const char *)text, len);
        if (text) HeapFree(GetProcessHeap(), 0, text);
        g_marksOn = g_mark[0].ok || (g_mark[1].ok && g_mael.ok);
        g_classSure = g_marksOn && g_mark[2].ok && ClassCodeHolds((const unsigned char *)GetModuleHandleA(NULL));
        if (g_marksOn && g_mark[2].ok && !g_classSure)
            Log("badge: the character list is not where it is in the client this was made for, so the badge is its ring alone");
    }
    if (BarHasTexture("Interface\\Glues\\LoadingScreens\\CinderGlow")) {
        unsigned char *text = NULL, *grid = NULL;
        unsigned len = 0, glen = 0;
        if (mpq_read(&kIoSmall, "patch-~.MPQ", "CinderLoad\\glow.txt", &text, &len) == MPQ_OK && text)
            GlowParse((const char *)text, len);
        int gridOk = mpq_read(&kIoSmall, "patch-~.MPQ", "CinderLoad\\glow.bin", &grid, &glen) == MPQ_OK && grid &&
                     GlowGridParse(grid, glen);
        if (text) HeapFree(GetProcessHeap(), 0, text);
        if (grid) HeapFree(GetProcessHeap(), 0, grid);
        g_glowOn = g_glowItems > 0 && gridOk;
        g_glowPick = -2;                                           /* nothing picked yet */
    }
    if (!g_seaOn && !g_lblOn && !g_logoOn && !g_marksOn && !g_glowOn) return;
    if (!OverlayCallsHold((const unsigned char *)GetModuleHandleA(NULL))) {
        g_seaOn = g_lblOn = g_logoOn = g_marksOn = g_glowOn = 0;
        Log("overlay off: the calls it draws with are not where they are in the client it was made for");
        return;
    }
    for (int i = 0; i < LBL_BATCH * 6; i++) g_lblIndex[i] = (unsigned short)i;
    g_ovOn = 1;
    char m[300];
    wsprintfA(m, "overlay on:%s%s%s%s%s%s%s%s%s, drawn over the world map and under the bar (in memory only)",
              g_seaOn ? " swells and glints on the open sea" : "", g_seaOn && g_marksOn ? "," : "",
              g_marksOn ? (g_mael.ok && g_mark[1].ok ? " the Maelstrom, the ripple and the badge" : " the ripple and the badge") : "",
              (g_seaOn || g_marksOn) && g_glowOn ? "," : "", g_glowOn ? " the arrival's glow" : "",
              (g_seaOn || g_marksOn || g_glowOn) && g_lblOn ? "," : "", g_lblOn ? " names and icons" : "",
              (g_seaOn || g_marksOn || g_glowOn || g_lblOn) && g_logoOn ? "," : "", g_logoOn ? " the RavenCraft logo at the top" : "");
    Log(m);
}

static float FillHeat(float d, float t, int k)
{
    float b;
    if (d < 0.04f) b = 0.82f + 0.18f * (d / 0.04f);                 /* the tip: hot, rising to the brightest */
    else if (d < 0.09f) b = 1.0f;
    else b = 0.30f + 0.70f * (float)FireExp(-(double)(d - 0.09f) / 0.22);   /* cooling to embers */
    float w = FireSin((float)((t * 2.3f + k * 1.7f) - (int)((t * 2.3f + k * 1.7f) / 6.2831853f) * 6.2831853f) * 0.25f);
    return FireClamp(b * (0.94f + 0.06f * w), 0, 1);
}
/* v: the game's four corners for the fill, x y z each: left bottom, front bottom, left top, front top. The front
 * drawn is the bar as shown (DisplayStep), not the game's. */
void WINAPI RevealFill(const float *v)
{
    unsigned bits = 0;
    float p = 0;
    if (ClientWord(0x882BE4, &bits)) { union { unsigned u; float f; } c; c.u = bits; p = c.f; }
    if (!(p >= 0)) p = 0;
    if (p > 1) p = 1;
    DWORD now = GetTickCount();
    float shown = DisplayStep(p, now);
    if (g_driftOn) DriftStep(shown, now);
    float L = v[0], B = v[1], z = v[2], T = v[7];
    if (p > 0.02f && v[3] > L) g_fillFull = (v[3] - L) / p;         /* the bar's whole width, in the same units */
    float full = g_fillFull > 0 ? g_fillFull : 1.0f, R = L + shown * full;   /* the front: the bar as shown */
    float t = (float)(GetTickCount() % 3600000u) / 1000.0f;
    float flow = t * FILL_FLOW;
    flow -= (int)(flow / 0.5f) * 0.5f;                               /* the lava repeats every half of its texture */
    float xs[FILL_COLUMNS];
    int n = 0;
    xs[n++] = L;                                                    /* columns from the left edge to the front */
    for (int k = FILL_COLUMNS - 2; k >= 0; k--) {
        float x = R - kFillBehind[k] * (full > 0 ? full : 1);
        if (x > xs[n - 1] + 1e-6f) xs[n++] = x;
    }
    if (xs[n - 1] < R - 1e-6f && n < FILL_COLUMNS) xs[n++] = R;
    for (int k = 0; k < n; k++) {
        float *top = g_fillVerts + 6 * k, *bottom = top + 3;
        top[0] = xs[k]; top[1] = T; top[2] = z;
        bottom[0] = xs[k]; bottom[1] = B; bottom[2] = z;
        float frac = full > 0 ? (xs[k] - L) / full : 0;            /* where along the whole bar, 0..1 */
        float u = flow + 0.5f * frac;
        g_fillUV[4 * k] = u; g_fillUV[4 * k + 1] = 0;
        g_fillUV[4 * k + 2] = u; g_fillUV[4 * k + 3] = 1;
        float d = full > 0 ? (R - xs[k]) / full : 0;                /* how far behind the front */
        float a = d >= REVEAL_FEATHER ? 1.0f : d / REVEAL_FEATHER;
        unsigned grey = (unsigned)(FillHeat(d, t, k) * 255.0f + 0.5f), alpha = (unsigned)(a * 255.0f + 0.5f);
        g_fillColor[2 * k] = g_fillColor[2 * k + 1] = alpha << 24 | grey << 16 | grey << 8 | grey;
    }
    for (int k = 0; k < 2 * n; k++) g_fillIndex[k] = (unsigned short)k;
    g_fillCount = 2 * n;
}

#if defined(__i386__)
/* The vertex setup: ecx = corners, edx = their positions, and eleven arguments above the return address (at entry:
 * colours at [esp+16], their stride at [esp+20], texture coordinates at [esp+32]). For the fill entry (esi at the
 * table's start) RevealFill makes the six corners, and ecx, edx and those three arguments become ours. */
__attribute__((naked, used)) static void RevealStub(void)
{
    __asm__("cmpl $0x7FFD34, %esi\n\t"
            "jne 1f\n\t"
            "pushl %eax\n\t"
            "pushl %ecx\n\t"
            "pushl %edx\n\t"
            "pushl %edx\n\t"
            "call _RevealFill@4\n\t"
            "call _OverlayDraw@0\n\t"
            "popl %edx\n\t"
            "popl %ecx\n\t"
            "popl %eax\n\t"
            "movl _g_fillCount, %ecx\n\t"
            "movl $_g_fillVerts, %edx\n\t"
            "movl $_g_fillColor, 16(%esp)\n\t"
            "movl $4, 20(%esp)\n\t"
            "movl $_g_fillUV, 32(%esp)\n"
            "1:\n\t"
            "jmp *_g_drawSetup\n\t");
}
/* The draw: ecx = the kind (a strip), edx = how many corners, [esp+4] = their order. For the fill, our six. */
__attribute__((naked, used)) static void RevealStripStub(void)
{
    __asm__("cmpl $0x7FFD34, %esi\n\t"
            "jne 1f\n\t"
            "movl _g_fillCount, %edx\n\t"
            "movl $_g_fillIndex, 4(%esp)\n"
            "1:\n\t"
            "jmp *_g_drawStrip\n\t");
}
#else
static void RevealStub(void) {}                        /* the tests' stand-ins, never called: their addresses are all */
static void RevealStripStub(void) {}                   /* the tests use */
#endif

static unsigned RevealRel(const unsigned char *base)
{
    return (unsigned)((ULONG_PTR)RevealStub - (ULONG_PTR)(base + REVEAL_CALL_RVA + 5));
}
static unsigned RevealStripRel(const unsigned char *base)
{
    return (unsigned)((ULONG_PTR)RevealStripStub - (ULONG_PTR)(base + REVEAL_DRAW_CALL_RVA + 5));
}
static int RevealPlaceHolds(const unsigned char *base, int i)
{
    const unsigned char *at = base + kRevealCode[i].rva, *want = kRevealCode[i].bytes;
    int n = kRevealCode[i].n, relAt;
    unsigned ours;
    if (!Readable(at, n)) return 0;
    if (Same(at, want, n)) return 1;
    if (kRevealCode[i].rva == REVEAL_DRAW_RVA) { relAt = REVEAL_REL_AT; ours = RevealRel(base); }
    else if (kRevealCode[i].rva == REVEAL_STRIP_RVA) { relAt = REVEAL_STRIP_REL_AT; ours = RevealStripRel(base); }
    else return 0;
    return Same(at, want, relAt) && Same(at + relAt + 4, want + relAt + 4, n - relAt - 4) && Rd32(at + relAt) == ours;
}
/* One call's rel32 to a stub of ours: 1 when it reads back. */
static int RevealSend(unsigned char *at, unsigned rel)
{
    DWORD old;
    if (!VirtualProtect(at + 1, 4, PAGE_EXECUTE_READWRITE, &old)) return 0;
    for (int i = 0; i < 4; i++)
        at[1 + i] = (unsigned char)(rel >> (8 * i));
    VirtualProtect(at + 1, 4, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
    return Rd32(at + 1) == rel;
}

/* The pack asks for its picture to drift: CinderLoad\drift.txt is in Data\patch-~.MPQ. */
static int DriftWanted(void)
{
    unsigned char *text = NULL;
    unsigned len = 0;
    int r = mpq_read(&kIoSmall, "patch-~.MPQ", "CinderLoad\\drift.txt", &text, &len);
    if (text) HeapFree(GetProcessHeap(), 0, text);
    return r == MPQ_OK;
}

/* Points the background's one push at g_bgUV. 1 when the picture can move. */
static int BgDrift(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleA(NULL);
    unsigned char *at = base + BG_DRAW_RVA, *ptr = base + BG_PUSH_RVA + 1;
    unsigned want = (unsigned)(ULONG_PTR)g_bgUV;
    int n = (int)sizeof kBgDraw;
    if (!Readable(at, n)) { Log("picture drift off: the background's draw could not be read"); return 0; }
    if (Same(at, kBgDraw, BG_PTR_AT) && Same(at + BG_PTR_AT + 4, kBgDraw + BG_PTR_AT + 4, n - BG_PTR_AT - 4) &&
        Rd32(ptr) == want) {
        InterlockedExchange(&g_driftOn, 1);
        Log("picture drift already on: nothing to do");
        return 1;
    }
    if (!Same(at, kBgDraw, n)) {
        char m[300];
        int len = wsprintfA(m, "picture drift off: other bytes at RVA 0x%lX (the background's texture coordinates) than in "
                            "the client it was made for; there:", (unsigned long)BG_DRAW_RVA);
        for (int k = 0; k < 16; k++) len += wsprintfA(m + len, " %02X", at[k]);
        Log(m);
        return 0;
    }
    DWORD old;
    if (!VirtualProtect(ptr, 4, PAGE_EXECUTE_READWRITE, &old)) { Log("picture drift off: the code could not be made writable"); return 0; }
    for (int i = 0; i < 4; i++)
        ptr[i] = (unsigned char)(want >> (8 * i));
    VirtualProtect(ptr, 4, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, (SIZE_T)n);
    if (Rd32(ptr) != want) { Log("picture drift off: the bytes read back differ"); return 0; }
    InterlockedExchange(&g_driftOn, 1);
    Log("picture drift on: each loading screen's picture eases in as the bar fills (one address at RVA 0x6DBD, in memory only)");
    return 1;
}

/* The pack asks for its fire to be revealed: CinderLoad\reveal.txt is in Data\patch-~.MPQ. */
static int RevealWanted(void)
{
    unsigned char *text = NULL;
    unsigned len = 0;
    int r = mpq_read(&kIoSmall, "patch-~.MPQ", "CinderLoad\\reveal.txt", &text, &len);
    if (text) HeapFree(GetProcessHeap(), 0, text);
    return r == MPQ_OK;
}

/* Sends the one call to RevealStub. 1 when the fill is revealed. */
static int RevealBar(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleA(NULL);
    char m[400];
    for (int i = 0; i < REVEAL_CODE_COUNT; i++) {
        if (RevealPlaceHolds(base, i)) continue;
        int len = wsprintfA(m, "fire reveal off: other bytes at RVA 0x%lX (%s) than in the client it was made for, so "
                            "the fire is stretched as before; there:", (unsigned long)kRevealCode[i].rva,
                            kRevealCode[i].what);
        if (Readable(base + kRevealCode[i].rva, kRevealCode[i].n))
            for (int k = 0; k < kRevealCode[i].n && k < 16; k++)
                len += wsprintfA(m + len, " %02X", base[kRevealCode[i].rva + (DWORD)k]);
        Log(m);
        return 0;
    }
    unsigned char *at = base + REVEAL_CALL_RVA, *strip = base + REVEAL_DRAW_CALL_RVA;
    if (Rd32(at + 1) == RevealRel(base) && Rd32(strip + 1) == RevealStripRel(base)) {
        Log("fire reveal already on: nothing to do");
        return 1;
    }
    g_drawSetup = at + 5 + (int)Rd32(at + 1);           /* where the game's calls went: VA 0x58A2A0 and 0x58A2E0 */
    g_drawStrip = strip + 5 + (int)Rd32(strip + 1);
    unsigned rel = RevealRel(base), stripRel = RevealStripRel(base);
    unsigned char *oldSetup = (unsigned char *)g_drawSetup;
    /* The setup first: with only it changed, the fill's six corners are drawn as the game's four (the left part,
     * short of the front), harmless. The other way round, six would be drawn from four and read past them. */
    if (!RevealSend(at, rel)) {
        Log("fire reveal off: the vertex setup could not be changed");
        return 0;
    }
    if (!RevealSend(strip, stripRel)) {
        RevealSend(at, (unsigned)(oldSetup - (at + 5)));  /* back to the game's own setup */
        Log("fire reveal off: the draw call could not be changed, so the setup was put back");
        return 0;
    }
    Log("fire reveal on: the fire is uncovered as the bar fills, not stretched, its front soft (two calls, RVA 0x725E and 0x7288, in memory only)");
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        if (GameDir() && ChooseScreens()) {
            if (Apply()) {
                int reveal = RevealWanted() && RevealBar();
                WideBar(reveal);                            /* revealed first: only then may the fill be as tall as the log */
                if (reveal && DriftWanted()) BgDrift();     /* the reveal's stub moves the picture */
                FireBar();
            } else {
                TakeOutAfterAll();
            }
        }
        if (g_dir[0]) FireStart();
    }
    return TRUE;
}

BOOL WINAPI DllMainCRTStartup(HINSTANCE inst, DWORD reason, LPVOID reserved) { return DllMain(inst, reason, reserved); }
