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
 * While a loading screen shows it plays a sound made of the game's own, read from the player's client archives: the
 * recipe Data\CinderLoad\sound.txt names (fire.h, kSounds, and "the sound" below). It is played through the client's
 * own fmod.dll from a thread of ours that only reads the client's memory; nothing in the client is changed for it.
 *
 * The bar's fire takes the colour of the character's class where the pack has one for it, else the picture's (orange,
 * fel green, arcane, aqua: "the fire bar" below). The game loads the bar's two textures once per screen; one call there
 * is sent through this DLL, which names the textures of that screen's fire family in our pack instead. Only with our loading screens in place, and only when every place
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
/* A garland's bar (4 Oct 2026): the top banner's mirror, flush with the bottom; fill the same. GARLAND_H tall, not
 * ARCH_H: his standing rule for every class's bottom bar (4 Oct, ~18:35), its leaves "finish naturally and stick out
 * past" the arch line, so the bar has headroom above the arch (the arch itself is ARCH_H at the edges, at the bottom). */
#define GARLAND_H 0.22f
static const unsigned char kBarTall[16]     = { 0x00, 0x00, 0x00, 0x3F, 0xAE, 0x47, 0xE1, 0x3D,   /* 0.5, 0.11 */
                                                0x00, 0x00, 0x80, 0x3F, 0xAE, 0x47, 0x61, 0x3E }; /* 1.0, 0.22 */
static int g_barSizeOurs, g_barTallNow;               /* the size table is ours (revealed); it holds the tall size */

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
        g_barSizeOurs = reveal;
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
    g_barSizeOurs = reveal && Same(fill, want, 16) && Same(border, kBorderWide, 16);
    Log(Same(fill, want, 16) && Same(border, kBorderWide, 16)
            ? (reveal ? "bar on: the whole width, flush with the bottom, its fire as tall as the log (its size table, in "
                        "memory only)"
                      : "bar on: the whole width, flush with the bottom (its size table, in memory only)")
            : "bar left as it is: the numbers read back differ");
}

/* The bar's size for this screen (BarPath, on the game's thread, as it begins): a garland's tall, else the log's. */
static void BarSizeSet(int tall)
{
    if (!g_barSizeOurs || tall == g_barTallNow) return;
    unsigned char *base = (unsigned char *)GetModuleHandleA(NULL);
    unsigned char *fill = base + BAR_FILL_RVA, *border = base + BAR_BORDER_RVA;
    const unsigned char *want = tall ? kBarTall : kBorderWide;
    DWORD old;
    if (!VirtualProtect(fill, (SIZE_T)(border + 16 - fill), PAGE_READWRITE, &old)) return;
    for (int i = 0; i < 16; i++) { fill[i] = want[i]; border[i] = want[i]; }
    VirtualProtect(fill, (SIZE_T)(border + 16 - fill), old, &old);
    g_barTallNow = tall;
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

/* ---- the sound (the fire's until 7 Oct 2026; its names kept) ----
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
 * asks us to fill (FSOUND_Stream_Create): every sample is mixed here (fire.h), from the screen's recipe, built from the
 * client's own sound files as it comes up (again only when it changes), so the pan does not depend on how FMOD pans. The thread calls FMOD only to start the
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
    DWORD since, gone, upAt;           /* upAt: when a screen came up with the class not yet known (0: none waiting) */
    float reached;
    int steps;                         /* times the bar moved this screen, as seen every FIRE_POLL_MS: the zoom idea's
                                          question is how often the game redraws a loading screen, which it does
                                          when the bar moves, so this is a lower bound on that */
} g_fire;

/* ---- the sound's recipes (7 Oct 2026, his ask: rig up every candidate so he can audition them on real loads and pick)
 * Each is a few of the game's own sounds (fire.h, "a recipe"). Data\CinderLoad\sound.txt says which plays: its first
 * word, one of
 *   hum       the blade's: the troll force field's 100 Hz hum, a buzz rising, sparks crackling faster as it fills
 *   hearth    the Hearthstone's own: its channel whir (the client's spell 8690), stretched and looped, the cast and the
 *             Astral Recall whoosh as the bar lands
 *   portal    an instance portal's hum, a crystal portal and Zul'Gurub's rising over it, the mage's Teleport at the end
 *   class     the character's class's own (below)
 *   cycle     the next of hum, hearth, portal, class at each loading screen (Data\CinderLoad\sound-cycle.txt counts)
 *   classes   the next of the nine classes' at each loading screen, whoever is playing
 *   warrior, paladin, hunter, rogue, priest, shaman, mage, warlock, druid: that class's, whoever is playing
 * No file, or a word it does not know: hum. Read as each screen comes up, so a change needs no restart. */
static const char *const kClassName[12];             /* (the classes' names and the character's class: below) */
static volatile LONG g_class;
#define SOUND_FILE  "Data\\CinderLoad\\sound.txt"
#define SOUND_CYCLE "Data\\CinderLoad\\sound-cycle.txt"
enum { SOUND_HUM, SOUND_HEARTH, SOUND_PORTAL, SOUND_CLASS0 };          /* SOUND_CLASS0 + class: that class's */
static const struct { const char *name, *text; } kSounds[] = {
    { "hum",
      "level 1.24\nsweep 0.6\n"
      "loop bed 1.0 0 0.6 Sound\\Doodad\\G_VooDooTrollForceFieldClosedLoop.wav\n"
      "loop swell 0.45 0.15 0.85 Sound\\Creature\\ArcaneGolem\\ArcaneGolemLoop.wav\n"
      "loop motion 0.3 0 0 Sound\\Doodad\\Sizzle.wav\n"
      "spark 0.5 0 1 Sound\\Doodad\\GnomeRoboArmStandFidget01Sparks.wav\n"
      "stinger 0.6 0 Sound\\Spells\\ShootWandLaunchLightning.wav\n" },
    { "hearth",
      "level 1.33\n"
      "loop bed 1.0 0 0.8 x0.8 Sound\\Spells\\PreCastNatureMagicHigh.wav\n"
      "loop swell 0.55 0.3 0.9 x0.8 Sound\\Spells\\PreCastNatureMagicMedium.wav\n"
      "stinger 0.6 0.9 Sound\\Spells\\Cast\\NatureCast.wav\n"
      "stinger 1.0 0.96 Sound\\Spells\\AstralRecall.wav\n" },
    { "portal",
      "level 1.02\n"
      "loop bed 1.0 0 0.7 Sound\\Doodad\\InstancePortalLoop.wav\n"
      "loop swell 0.6 0.3 0.95 Sound\\Creature\\CrystalPortal\\CrystalPortalLoop.wav\n"
      "loop swell 0.4 0.55 1 Sound\\Doodad\\ZulGurubPortalLoop.wav\n"
      "stinger 0.9 0.95 Sound\\Spells\\Teleport.wav\n" },
    { "none", "" },                                                      /* (class 0: no class's own) */
    { "warrior",
      "level 0.9\n"
      "loop bed 1.0 0 0.8 Sound\\Doodad\\TrollDrumLoop1.wav\n"
      "loop swell 0.5 0.4 0.95 Sound\\Spells\\WhirlwindLoop.wav\n"
      "stinger 0.7 0 Sound\\Spells\\BattleShout.wav\n"
      "stinger 0.9 0.95 Sound\\Spells\\Spell_WA_Revamp_Battleroar_01.wav\n" },
    { "paladin",
      "level 0.88\n"
      "loop bed 1.0 0 0.8 Sound\\Spells\\Missile\\HolyMissileLoop.wav\n"
      "loop swell 0.6 0.35 0.95 Sound\\Spells\\PreCastHolyMagicLow.wav\n"
      "stinger 0.6 0 Sound\\Spells\\SealofMight.wav\n"
      "stinger 0.9 0.95 Sound\\Spells\\DivineShield.wav\n" },
    { "hunter",
      "level 2.8\n"
      "loop bed 1.0 0 0.8 Sound\\Ambience\\ZoneAmbience\\JungleDay.wav\n"
      "loop swell 0.8 0.4 0.95 Sound\\Spells\\PreCastNatureMagicLow.wav\n"
      "stinger 0.3 0 Sound\\Spells\\AspectofThePack.wav\n"
      "stinger 0.35 0.95 Sound\\Spells\\AspectofTheHawk.wav\n" },
    { "rogue",
      "level 0.8\n"
      "loop bed 1.0 0 0.8 x0.75 Sound\\Spells\\Missile\\ShadowMissileLoop.wav\n"
      "loop swell 0.6 0.35 0.95 Sound\\Spells\\PreCastShadowMagicLow.wav\n"
      "stinger 0.7 0 Sound\\Spells\\Stealth.wav\n"
      "stinger 0.9 0.95 Sound\\Spells\\Vanish.wav\n" },
    { "priest",
      "level 1.55\n"
      "loop bed 1.0 0 0.8 Sound\\Spells\\PreCastHolyMagicMedium.wav\n"
      "loop swell 0.6 0.35 0.95 Sound\\Spells\\PreCastHolyMagicHigh.wav\n"
      "spark 0.45 0.1 1 Sound\\Spells\\PrayerofHealing.wav\n"
      "stinger 0.6 0 Sound\\Spells\\Renew.wav\n"
      "stinger 0.8 0.95 Sound\\Spells\\DirectDamage\\HolyImpactDDLow.wav\n" },
    { "none", "" },                                                      /* (class 6: none in 1.12) */
    { "shaman",
      "level 1.22\n"
      "loop bed 1.0 0 0.8 Sound\\Creature\\ElementalAir\\AirElementalLoop.wav\n"
      "loop swell 0.5 0.35 0.95 Sound\\Creature\\ElementalEarth\\ElementalEarthLoop.wav\n"
      "spark 0.45 0.2 1 Sound\\Spells\\LightningShieldImpact.wav\n"
      "stinger 0.9 0.95 Sound\\Spells\\CallLightning.wav\n" },
    { "mage",
      "level 1.4\n"
      "loop bed 1.0 0 0.8 Sound\\Spells\\PreCastFrostMagicLow.wav\n"
      "loop swell 0.55 0.35 0.95 Sound\\Spells\\PreCastMageLow.wav\n"
      "spark 0.4 0.2 1 Sound\\Spells\\ArcaneMissileImpact1a.wav\n"
      "stinger 0.9 0.95 Sound\\Spells\\ArcaneExplosion.wav\n" },
    { "warlock",
      "level 0.8\n"
      "loop bed 1.0 0 0.8 x0.8 Sound\\Spells\\Missile\\FireMissileLoop.wav\n"
      "loop swell 0.6 0.3 0.95 Sound\\Spells\\LifeDrainLoop.wav\n"
      "spark 0.5 0.1 1 x0.9 Sound\\Spells\\Immolate.wav\n"
      "stinger 0.6 0 Sound\\Spells\\DemonArmor.wav\n"
      "stinger 0.9 0.95 Sound\\spells\\SoulShatter_Cast.wav\n" },
    { "none", "" },                                                      /* (class 10: none in 1.12) */
    { "druid",
      "level 1.6\n"
      "loop bed 1.0 0 0.8 Sound\\Spells\\Missile\\NAtureMissileLoop.wav\n"
      "loop swell 0.6 0.35 0.95 Sound\\Doodad\\MoonWellLightLoop.wav\n"
      "stinger 0.35 0 Sound\\Spells\\Rejuvenation.wav\n"
      "stinger 0.5 0.95 Sound\\Spells\\StarFireImpact.wav\n" },
};
#define SOUNDS ((int)(sizeof kSounds / sizeof kSounds[0]))
static SndSpec g_sndSpec;                            /* the recipe built: its lines, its sounds, which it is */
static SndSet g_snd;
static int g_sndNow = -1, g_sndRate;
static unsigned g_sndBad;                            /* recipes that could not be made this session (a bit each) */
static unsigned g_sndCycle;                          /* the cycle's next, as sound-cycle.txt last said or this session moved it */
static volatile LONG g_barFamNow = -1;               /* BarPath's: the screen's fire family, -1 the pack's orange */

static signed char WINAPI FireCallback(void *stream, void *buff, int len, void *user)
{
    (void)stream; (void)user;
    union { LONG bits; float f; } fill;
    fill.bits = g_fire.fillBits;
    int frames = len > 0 ? len / 4 : 0, on = 0;
    if (g_snd.n) on = SndRender(&g_fire.mix, &g_snd, &g_sndSpec, (int)g_fire.gate, fill.f, (short *)buff, frames);
    else for (int i = 0; i < frames * 4; i++) ((unsigned char *)buff)[i] = 0;
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

static void SoundFree(void)
{
    float **all[3] = { g_snd.loop, g_snd.spark, g_snd.sting };
    int counts[3] = { SND_LOOPS, SND_SPARKS, SND_STINGS };
    for (int k = 0; k < 3; k++)
        for (int i = 0; i < counts[k]; i++) {
            if (all[k][i]) HeapFree(GetProcessHeap(), 0, all[k][i]);
            all[k][i] = NULL;
        }
    g_snd.n = 0;
    g_sndNow = -1;
}

/* One of a recipe's sounds at the mixer's rate, from the client's own files (the newest copy across its archives, in
 * the order the client lets them win; a zero-length stub is passed over), played at speed (0.8: slower and lower): a
 * loop (seamed) or a one-shot (whole). NULL, and why, when it cannot be had. */
static float *SndLoad(const char *path, int rate, float speed, int loop, unsigned *outLen, const char **why)
{
    static const char kOrder[] = "ZYXWVUTSRQPONMLKJIHGFEDCBA98765432";
    char arch[16];
    *why = "in none of the client's archives";
    for (int a = 0; a <= (int)sizeof kOrder - 1 + 1; a++) {
        if (a < (int)sizeof kOrder - 1) { lstrcpyA(arch, "patch-?.MPQ"); arch[6] = kOrder[a]; }
        else lstrcpyA(arch, a == (int)sizeof kOrder - 1 ? "patch.MPQ" : "sound.MPQ");
        unsigned char *wav = NULL;
        unsigned len = 0, frames = 0;
        int r = mpq_read(&kIo, arch, path, &wav, &len), srate = 0, channels = 0;
        const unsigned char *data = NULL;
        if (r == MPQ_E_NOT_FOUND || r == MPQ_E_NO_ARCHIVE || r == MPQ_E_EMPTY) continue;
        if (r != MPQ_OK) { *why = "unreadable"; continue; }
        float *src = NULL, *out = NULL;
        unsigned n = 0;
        int from = 0;
        if (!FireWavInfo(wav, len, &srate, &channels, &data, &frames)) *why = "not a 16-bit PCM sound";
        else {
            from = (int)(srate * speed + 0.5f);                        /* read as if it were made at this rate */
            if (frames > (unsigned)srate * SND_MAX_S) frames = (unsigned)srate * SND_MAX_S;
            if (!(src = HeapAlloc(GetProcessHeap(), 0, frames * sizeof(float))) ||
                !(out = HeapAlloc(GetProcessHeap(), 0, (FireResampledLength(frames, from, rate) + 1) * sizeof(float))))
                *why = "out of memory";
            else {
                FireWavRead(data, channels, frames, src);
                n = loop ? FireMakeLoop(src, frames, from, rate, 0, out) : SndOneShot(src, frames, from, rate, out);
                if (!n) *why = "too short";
            }
        }
        if (src) HeapFree(GetProcessHeap(), 0, src);
        HeapFree(GetProcessHeap(), 0, wav);
        if (n) { *outLen = n; return out; }
        if (out) HeapFree(GetProcessHeap(), 0, out);
        return NULL;                                   /* the newest copy decides */
    }
    return NULL;
}

/* Recipe k's sounds at the mixer's rate, from the client's own files, in place of the last's. Only while no stream is
 * open. 1 when at least one of its loops could be made (a missing sound is logged and the rest play). */
static int SoundBuild(int k, int rate)
{
    char m[300];
    DWORD t0 = GetTickCount();
    SoundFree();
    FireSetRate(&g_fire.mix, rate);
    g_sndRate = rate;
    if (k < 0 || k >= SOUNDS || !SndParse(&g_sndSpec, kSounds[k].text, (unsigned)lstrlenA(kSounds[k].text))) return 0;
    int made = 0, extra = 0, wanted = g_sndSpec.sparks + g_sndSpec.stings;
    const char *why = "";
    for (int i = 0; i < g_sndSpec.n; i++) {
        g_snd.loop[i] = SndLoad(g_sndSpec.l[i].path, rate, g_sndSpec.l[i].speed, 1, &g_snd.len[i], &why);
        if (g_snd.loop[i]) made++;
        else { wsprintfA(m, "sound: %s's %s is %s, so it plays without it", kSounds[k].name, g_sndSpec.l[i].path, why); Log(m); }
    }
    for (int i = 0; i < g_sndSpec.sparks + g_sndSpec.stings; i++) {
        int spark = i < g_sndSpec.sparks, j = spark ? i : i - g_sndSpec.sparks;
        SndLayer *y = spark ? &g_sndSpec.spark[j] : &g_sndSpec.sting[j];
        float **buf = spark ? &g_snd.spark[j] : &g_snd.sting[j];
        unsigned *n = spark ? &g_snd.sparkLen[j] : &g_snd.stingLen[j];
        *buf = SndLoad(y->path, rate, y->speed, 0, n, &why);
        if (*buf) extra++;
        else { wsprintfA(m, "sound: %s's %s is %s, so it plays without it", kSounds[k].name, y->path, why); Log(m); }
    }
    g_snd.n = made ? g_sndSpec.n : 0;
    SndReset(&g_snd);
    if (made) g_sndNow = k;
    else g_sndBad |= 1u << k;
    wsprintfA(m, "sound: %s made from the client's own files (%d of %d loops, %d of %d sparks and stingers) at %d Hz in %lu ms",
              kSounds[k].name, made, g_sndSpec.n, extra, wanted, rate, (unsigned long)(GetTickCount() - t0));
    if (!made) wsprintfA(m, "sound off for now: none of %s's loops could be made from the client's files", kSounds[k].name);
    Log(m);
    return made > 0;
}

/* Which recipe sound.txt says for a screen of class cls (0: not known): advance 1 as a screen comes up (the cycle moves
 * on and is kept), 0 to look ahead. *word gets what the file said, for the log. */
static int SoundPick(unsigned cls, int advance, const char **word)
{
    static const char *const kModes[] = { "hum", "hearth", "portal", "class", "cycle", "classes" };
    char path[MAX_PATH], *t = NULL;
    int mode = 0;
    *word = "hum";
    if (g_dir[0] && Join(path, g_dir, SOUND_FILE) && (t = ReadText(path, 256))) {
        int n = 0, i = 0, found = -1;
        while (t[n] && t[n] != '\r' && t[n] != '\n') n++;            /* its first line */
        for (int k = 0; k < 6 && found < 0; k++) if (SndWord(t, n, (i = 0, &i), kModes[k])) found = k;
        for (int c = 1; c < 12 && found < 0; c++)
            if (kClassName[c][0] != '?' && SndWord(t, n, (i = 0, &i), kClassName[c])) found = 100 + c;
        if (found >= 0) { mode = found; *word = found >= 100 ? kClassName[found - 100] : kModes[found]; }
        HeapFree(GetProcessHeap(), 0, t);
    }
    if (mode >= 100) return SOUND_CLASS0 + mode - 100;
    if (mode <= 2) return mode;
    unsigned at = g_sndCycle;
    int k;
    if (mode == 3) k = cls > 0 && cls < 12 && kClassName[cls][0] != '?' ? SOUND_CLASS0 + (int)cls : SOUND_HUM;
    else if (mode == 4) k = at % 4 == 3 ? (cls > 0 && cls < 12 && kClassName[cls][0] != '?' ? SOUND_CLASS0 + (int)cls : SOUND_HUM)
                                        : (int)(at % 4);
    else {
        static const unsigned char kNine[9] = { 1, 2, 3, 4, 5, 7, 8, 9, 11 };
        k = SOUND_CLASS0 + kNine[at % 9];
    }
    if (advance && mode >= 4) {
        char text[16];
        g_sndCycle = at + 1;
        wsprintfA(text, "%u\r\n", g_sndCycle);
        if (g_dir[0] && Join(path, g_dir, SOUND_CYCLE)) WriteText(path, text);
    }
    return k;
}
/* At start: where the cycle had got to (sound-cycle.txt), so each session goes on from the last. */
static void SoundCycleLoad(void)
{
    char path[MAX_PATH], *t;
    g_sndCycle = 0;
    if (!g_dir[0] || !Join(path, g_dir, SOUND_CYCLE) || !(t = ReadText(path, 64))) return;
    for (const char *p = t; *p >= '0' && *p <= '9' && g_sndCycle < 100000000u; p++) g_sndCycle = g_sndCycle * 10 + (unsigned)(*p - '0');
    HeapFree(GetProcessHeap(), 0, t);
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
    if (!g_snd.n) return 0;                                            /* (FireStep built the screen's recipe) */
    FireReset(&g_fire.mix);
    FireFillStart(&g_fire.fill, fill);
    FirePublish(g_fire.fill.fill);
    InterlockedExchange(&g_fire.gate, 1);
    InterlockedExchange(&g_fire.quiet, 0);
    void *st = F.Stream_Create(FireCallback, FIRE_BLOCK_FRAMES * 4, FSOUND_16BITS | FSOUND_SIGNED | FSOUND_STEREO | FSOUND_2D,
                               g_fire.mix.rate, NULL);
    if (!st) {
        if (g_fire.complaints++ >= 3) return 0;
        wsprintfA(m, "sound: the sound engine would not make the stream (FMOD error %d); trying at the next screen",
                  F.GetError ? F.GetError() : -1);
        Log(m);
        return 0;
    }
    void *sample = F.Stream_GetSample ? F.Stream_GetSample(st) : NULL;
    if (sample && F.Sample_SetDefaults) F.Sample_SetDefaults(sample, -1, -1, -1, FIRE_PRIORITY);
    int ch = F.Stream_PlayEx(FSOUND_FREE, st, NULL, 1);
    if (ch < 0) {
        F.Stream_Close(st);
        if (g_fire.complaints++ < 3) Log("sound: no free sound channel; trying at the next screen");
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

#define BAR_SHOWN_MAX 160              /* the longest about 115: its fire, the class, "the world map from halfway in ...", resets */
static void BarShownSince(char *out, int cap);
static void DriftScreenGone(void);
static void LearnScreen(char *m);
static void HoldSetup(void);
static void PresentSetup(void);
static void BlurRelease(void);
static volatile LONG g_destKnownPct = -1, g_destKnownWay;       /* the arrival: the bar when it became known, and how */
static void OverlayStart(void);
static void BadgeLog(char *m);           /* the marks' (below): where a map screen's class came from, into the log */
static volatile LONG g_screenWatch;      /* 1 once the fire's worker watches for screens ending */
static volatile LONG g_dispNew = 1;      /* the bar as shown: the next frame begins a new screen */
static volatile LONG g_viewOn, g_viewSteps, g_gameSeq;   /* the view (below): on; its thread moves the bar and the zoom
                                                          * now; the game's loading frames drawn */
static int g_settleView;                 /* the hold's glide in is the view's (HoldFrame): the push its last stretch, landing
                                          * with the world */
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
        if (g_fire.stream) Log("sound: the game closed its sound engine; the stream went with it");
        g_fire.stream = NULL;
        g_fire.channel = -1;
        g_fire.playing = 0;
        return;
    }
    if (!g_fire.built && !screen) {              /* the next screen's recipe made once the engine is up, before it comes */
        const char *word;
        g_fire.built = SoundBuild(SoundPick((unsigned)g_class, 0, &word), FireRate()) ? 1 : -1;
    }
    if (screen && !g_fire.playing) {             /* a loading screen has come up, or come back while fading */
        LONG cls = g_class;
        const char *word;
        if (cls <= 0 && SoundPick(1, 0, &word) != SoundPick(2, 0, &word) &&   /* the pick is the class's, and the class */
            (!g_fire.upAt || now - g_fire.upAt < 400)) {                     /* can come a moment after the screen */
            if (!g_fire.upAt) g_fire.upAt = now ? now : 1;
            return;
        }
        g_fire.upAt = 0;
        int k = SoundPick(cls > 0 ? (unsigned)cls : 0, 1, &word), rate = FireRate();
        if (k != g_sndNow || rate != g_sndRate) {
            if (g_fire.stream) FireClose();      /* never rebuilt under a stream that is playing it */
            if ((g_sndBad >> k & 1) || !SoundBuild(k, rate)) {
                g_fire.playing = -1;             /* silent this screen */
                return;
            }
        }
        if (g_fire.screens < FIRE_SCREEN_LOGS) {
            wsprintfA(m, "sound: %s for this screen (Data\\CinderLoad\\sound.txt: %s)", kSounds[k].name, word);
            Log(m);
        }
        SndReset(&g_snd);
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
            wsprintfA(m, "loading screen: its sound (%s) played for %lu.%lu s, the bar reached %d%% in %d steps%s",
                      g_sndNow >= 0 ? kSounds[g_sndNow].name : "none", ms / 1000, ms % 1000 / 100,
                      (int)(g_fire.reached * 100 + 0.5f), g_fire.steps, bar);
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
        LearnScreen(m);
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
            Log("sound off: the game's fmod.dll is not there or lacks the calls it needs");
            return 0;
        }
        Sleep(1000);
    }
    float v = F.GetVersion();
    if (v < 3.7f || v >= 3.8f) {                 /* the stream callback's form is FMOD 3.7's */
        char m[120];
        wsprintfA(m, "sound off: the game's fmod.dll is version %d.%02d, not the 3.7x this was made for",
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
        int len = wsprintfA(m, "sound off: other bytes at RVA 0x%lX (%s) than in the client it was made for, so no "
                            "sound; there:", (unsigned long)kFireCode[bad].rva, kFireCode[bad].what);
        if (Readable(base + kFireCode[bad].rva, kFireCode[bad].n))
            for (int i = 0; i < kFireCode[bad].n; i++)
                len += wsprintfA(m + len, " %02X", base[kFireCode[bad].rva + (DWORD)i]);
        Log(m);
        return;
    }
    if (!Readable(base + FIRE_LAYER_RVA, 8) || !Readable(base + FIRE_SOUNDUP_RVA, 1)) {
        Log("sound off: the loading screen's numbers could not be read");
        return;
    }
    g_fire.base = base;
    g_fire.channel = -1;
    HANDLE t = CreateThread(NULL, 0, FireThread, NULL, 0, NULL);
    if (!t) {
        Log("sound off: its thread could not be started");
        return;
    }
    CloseHandle(t);
    SoundCycleLoad();
    Log("sound on: each loading screen plays the recipe Data\\CinderLoad\\sound.txt names (the hum when it names none)");
}

/* ---- the fire bar ----
 * Each loading screen's bar burns in the colour of its picture's fire. Beside the orange Loading-BarFill.blp and
 * Loading-BarBorder.blp the game loads for every screen, our pack carries Loading-BarFill-<family>.blp and
 * Loading-BarBorder-<family>.blp for each fire family the owner's art kit found (fel, arcane, aqua, ...), and
 * CinderLoad\bars.txt: one line per screen, "<the screen file's base name> <family>", and "default <family>" for the
 * screens it does not list (make_pack.py writes it).
 * His call, 4 Oct 2026: the fire is the CLASS's colour. bars.txt names it as if a screen: "classdruid orange", "classhunter
 * fel", "classpaladin paladin", ... A class it names takes that fire on every screen; a class it does not name, or a
 * screen before any class is known (the character list's code not where it was, say), keeps the picture's.
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
#define BAR_FAMILIES        16         /* fire families one pack may carry: the nine classes', the pictures' own */
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
    struct {
        char name[BAR_FAMILY_MAX + 1]; int ok; char fill[80], border[80];
        int garland; char back[80], wisps[80], blooms[80];   /* a garland ("style <family> garland"): see "the garland" */
    } family[BAR_FAMILIES];
    struct { char name[BAR_NAME_MAX + 1]; int family; } screen[BAR_SCREENS];
} BarTable;
static BarTable g_bars;
static volatile LONG g_barsOn;                         /* 1 once g_bars is complete and the call is ours */
static volatile LONG g_barGarland;                     /* the screen showing has a garland: its family + 1, else 0 */
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
/* WoW.exe's own .data section, its globals (the bar, the arrival, the character list's head), found in its PE header at
 * start (ClientDataFind): mapped for the process's whole life, so a read inside it needs no VirtualQuery. The view's
 * thread reads these every frame, and under Wine VirtualQuery waits on the process's memory lock, which the game's own
 * loading holds (4 Oct 2026, his first look at the view: it paused about a tenth of a second inside each of the game's
 * add-on hitches). 0, 0 until found: every read is checked. */
static ULONG_PTR g_dataLo, g_dataHi;
static int ClientWord(unsigned va, unsigned *out)
{
    const unsigned char *p = ClientAt(va);
    if (!p || (!((ULONG_PTR)p >= g_dataLo && (ULONG_PTR)p + 4 <= g_dataHi) && !Readable(p, 4))) return 0;
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
    if (words == 3 && len[0] == 5 && Lower(word[0][0]) == 's' && Lower(word[0][1]) == 't' && Lower(word[0][2]) == 'y' &&
        Lower(word[0][3]) == 'l' && Lower(word[0][4]) == 'e' && len[2] == 7 && len[1] <= BAR_FAMILY_MAX) {
        static const char kGarland[] = "garland";        /* "style <family> garland": that family grows, not burns */
        char family[BAR_FAMILY_MAX + 1];
        for (int i = 0; i < 7; i++) if (Lower(word[2][i]) != kGarland[i]) { t->skipped++; return; }
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
        t->family[f].garland = 1;
        return;
    }
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
/* Every screen after the session's first (his rule, 4 Oct 2026: "start halfway to the spot the character is loading to
 * (from the initial maelstrom/dead center of the map starting position)"): the view starts LATER_START of the way in
 * (the zoom's steps) with its middle LATER_MIDDLE of the way from the world's middle to the arrival, set there at once
 * when the arrival is known from the start. */
#define LATER_START     0.5f
#define LATER_MIDDLE    0.5f
/* The screen's TIME (his rule, 4 Oct, later the same night: "build a five second minimum loading screen time even if
 * that leaves a character unprotected for a second or two ... so that it's not so jarring to the eye", the zoom going
 * "at whatever percentage it needs to in order to finish where it would normally end by the time that five seconds is
 * up"; and for every machine, "not everyone's computer will be as beefy as mine", learning "how long the initial cold
 * boot load times are and also from the intersession load times that are faster"). Each world map screen plans when
 * its bar and zoom finish: this machine's learned time to a full bar for its kind (the session's first, or a later
 * one), but never sooner than the floor (map.txt "hold <ms>", HOLD_MS when not given; 0 turns all this off) less the
 * game's own freeze at the end (learned too). The bar and the zoom run on that plan, never ahead of the game; when the
 * game is done first, HoldFrame holds the screen, drawing the game's own loading frame, until the plan is met.
 * LEARN_FILE keeps the lessons across sessions, the newest load counting half. */
#define HOLD_MS         5000       /* the floor: no loading screen shorter (his five seconds) */
#define HOLD_MAX_MS     6000       /* never hold the game longer than this, whatever the plan says */
#define HOLD_SETTLE_MS  600        /* the game done after the floor yet ahead of the plan: the rest of the way in this */
#define HOLD_TAIL_MS    200        /* held past the plan's end, so the view settles on the arrival */
#define FREEZE_GUESS_MS 1000       /* the game's freeze at the end of a load, before one is learned */
#define FIRST_GUESS_MS  7000       /* a session's first screen to a full bar, before one is learned */
#define LEARN_MIN_MS    100        /* a time outside these is no lesson */
#define LEARN_MAX_MS    120000
#define LEARN_FILE      "Data\\CinderLoad\\loadtimes.txt"
/* The character list's music (his ask, 4 Oct: "once you click a character, it should fade out that sound pretty
 * quickly and transition over to the sound of the character class loading screen ... that transition out will need to
 * be snappy"): as a loading screen begins, if the glue music still plays (its sound at [GLUE_MUSIC_VA]), CinderLoad
 * stops it the way the glue's own StopGlueMusic does (VA 0x46CF00: push 1.0, call 0x45B050), with map.txt's
 * "music <ms>" fade (MUSIC_FADE_MS when not given; 0 cuts it at once). */
#define MUSIC_FADE_MS   250
/* The ending (his ask, 4 Oct: "at the end of every class loading screen ... blur and fade out over about ... half a
 * second, and then the character appears"; his pick, the higher end: "a true downsample blur on the game's graphics
 * device ... it'll be worth it in the end", with the matching fade into the world). Every timed screen ends with
 * map.txt's "blur <ms> <dim %>" (BLUR_MS, BLUR_DIM when not given; blur 0 for none): once the view has landed, the
 * screen goes out of focus and darkens by that much over those ms (held that long when the game is done sooner, inside
 * the five seconds), the game's own freeze then holds that frame, and the world's first DISSOLVE_MS dissolve out of it.
 * The blur is the game's own frame, shrunk on its Direct3D 9 device a half at a time to 1/32 and grown back to a
 * quarter, all with linear filtering, laid over the frame. Drawn just before the device presents (PRESENT_VA). */
#define BLUR_MS         300        /* after the view lands; with BLUR_LEAD_MS, the last half second (his rule, 4 Oct 21:47:
                                    * "just have it be the last half a second, no matter how long it is") */
#define BLUR_DIM        35         /* percent of black at the end (100: to black) */
/* His notes on the first in game (4 Oct, 21:40): "it gets to as max zoom in as it's going to get and then just blurs
 * from there and instead it should be blurring a little bit earlier and the zoom should keep zooming in until it
 * completely fades away. And we should see Stormwind City on the map": the blur begins BLUR_LEAD_MS before the view
 * lands, and through it the view keeps closing in, BLUR_PUSH times closer by its end. */
#define BLUR_LEAD_MS    200
#define BLUR_PUSH       2.5f
#define DISSOLVE_MS     500
#define PRESENT_VA      0x5A19FD   /* the game's present: mov eax, [esi+0x38A8] (its IDirect3DDevice9), then Present */
#define GX_D3D_OFF      0x38A8
#define GX_SCENE_OFF    0x3A38     /* nonzero while the game is between BeginScene and EndScene */
#define GLUE_STOP_VA    0x45B050   /* stdcall (float seconds): stops the glue music's sound with that fade, clears it */
#define GLUE_MUSIC_VA   0xB06038   /* the glue music's sound while it plays, else 0 */
#define MAP_ENTS        96         /* dungeon, raid and battleground entrances map.txt may list */
/* map.txt's "ent <map id> <continent> <x> <y>" lines (hex; x and y float bits, world yards): where each instance's
 * entrance lies on the world map (4 Oct 2026, his rule: "no separate log in screens for being in a dungeon or raid,
 * everyone who uses CinderLoad will get the zoom in map loading screen", the class icon on the dungeon's icon). */
static struct {
    int ok; struct { int ok; float au, bu, av, bv; } m[2]; float endYards, pace;
    int ents; struct { unsigned id; int c; float x, y; } ent[MAP_ENTS];
} g_map;
static volatile LONG g_mapShown;  /* the screen showing has the world map */
static float g_viewU = 0.5f, g_viewV = 0.5f, g_viewH = 0.5f;     /* the world map's view, eased */
static volatile LONG g_driftResets;                             /* a fallback reset within a screen (the log says) */
static int g_zoomScreens;                                       /* the world map's screens begun this session */
static float g_zoomStart;                                       /* how far in this screen starts (0 the whole world) */
static volatile LONG g_lnMs[2], g_lnFreeze[2], g_lnN[2];        /* learned [first, later]: to a full bar, the end freeze */
static volatile LONG g_fullMs, g_endFreezeMs, g_screenLogin;    /* this screen: ms to a full bar (0 not yet), its freeze */
static volatile LONG g_holdMs = HOLD_MS;                        /* the floor (map.txt "hold"), 0 for none */
static volatile LONG g_holdN, g_holdLast;                       /* holds made, and the last one's ms (the log) */
static DWORD g_scrStartAt, g_planEndAt;                         /* this screen: its first frame, when its plan ends */
static float g_planU;                                           /* the plan's bar, 0..1 */
static int g_screenKind, g_holdDone, g_timed;                   /* later (1) or first (0); held already; on the plan */
static volatile LONG g_blurMs = BLUR_MS, g_blurDim = BLUR_DIM, g_blurOn;   /* map.txt's blur; the present's hook set */
static DWORD g_blurFrom, g_blurTo, g_dissolveAt;                /* this screen's blur; when the world's dissolve began */
static int g_blurUsed, g_loadFrame;                             /* the blur drew this screen; a loading frame is drawn */
static const unsigned char kPresent[20] = { 0x8B, 0x86, 0xA8, 0x38, 0x00, 0x00, 0x8B, 0x10, 0x6A, 0x00, 0x6A, 0x00, 0x6A,
                                            0x00, 0x6A, 0x00, 0x50, 0xFF, 0x52, 0x44 };   /* VA 0x5A19FD, as read */

/* The blur at now: 0 none .. 1 all, eased. */
static float BlurAmount(DWORD now)
{
    if (!g_blurTo || (LONG)(now - g_blurFrom) <= 0) return 0;
    float k = FireClamp((float)(LONG)(now - g_blurFrom) / (float)(LONG)(g_blurTo - g_blurFrom + 1), 0, 1);
    return k * k * (3 - 2 * k);
}

/* What the present draws over this frame (the game's thread): 0 nothing, 1 the blur by *amount, 2 the world's dissolve
 * by *amount, 3 nothing and the blur's textures go (the dissolve is over). */
static int BlurStep(int loading, DWORD now, float *amount)
{
    if (loading) {
        g_dissolveAt = 0;
        float k = BlurAmount(now);
        if (k <= 0) return 0;
        g_blurUsed = 1;
        *amount = k;
        return 1;
    }
    if (!g_blurUsed) return 0;
    if (!g_dissolveAt) g_dissolveAt = now ? now : 1;
    float k = FireClamp((float)(LONG)(now - g_dissolveAt) / (float)DISSOLVE_MS, 0, 1);
    if (k >= 1) {
        g_blurUsed = 0;
        g_dissolveAt = 0;
        return 3;
    }
    *amount = 1 - k * k * (3 - 2 * k);
    return 2;
}
static volatile LONG g_musicFadeMs = MUSIC_FADE_MS, g_musicOn, g_musicFaded;   /* map.txt's fade; set up; this screen */
static const unsigned char kGlueStop[18] = { 0x55, 0x8B, 0xEC, 0x8B, 0x0D, 0x38, 0x60, 0xB0, 0x00, 0x85, 0xC9, 0x74, 0x13,
                                             0x8B, 0x45, 0x08, 0x50, 0xE8 };    /* 0x45B050, as read from his WoW.exe */
static void (WINAPI *g_glueStop)(float);

/* As a world map screen begins, on the game's thread: the glue music, if it still plays, out with the pack's fade. */
static void MusicFade(void)
{
    unsigned h;
    InterlockedExchange(&g_musicFaded, 0);
    if (!g_musicOn || !g_glueStop || !ClientWord(GLUE_MUSIC_VA, &h) || !h) return;
    g_glueStop((float)g_musicFadeMs / 1000.0f);
    InterlockedExchange(&g_musicFaded, 1);
}

/* At start, with the world map: the glue music's stop, once its bytes are where they were read. */
static void MusicSetup(void)
{
    const unsigned char *at = (const unsigned char *)GetModuleHandleA(NULL) + (GLUE_STOP_VA - 0x400000u);
    InterlockedExchange(&g_musicOn, 0);
    if (g_musicFadeMs < 0) return;
    if (!Readable(at, (int)sizeof kGlueStop) || !Same(at, kGlueStop, (int)sizeof kGlueStop)) {
        Log("music: the glue music's stop is not where it was read (VA 0x45B050), so it plays on through the screen");
        return;
    }
    g_glueStop = (void (WINAPI *)(float))(ULONG_PTR)at;
    InterlockedExchange(&g_musicOn, 1);
    char m[120];
    wsprintfA(m, "music: the character list's music fades out over %ld ms as a loading screen begins", (long)g_musicFadeMs);
    Log(m);
}
static volatile LONG g_zoomHalf;                                /* this one starts halfway in (his ask, 4 Oct: the first
                                                                 * load after a cold boot is long, the rest a fraction) */
static volatile LONG g_screenMap;                               /* this screen zooms the world map */

/* A decimal of up to 7 digits at *q, spaces before it skipped: its value, or -1 when there is none. */
static LONG LearnNum(const char **q)
{
    LONG v = 0;
    int d = 0;
    while (**q == ' ') (*q)++;
    for (; **q >= '0' && **q <= '9' && d < 7; (*q)++, d++) v = v * 10 + (**q - '0');
    return d ? v : -1;
}

/* The lessons, from LEARN_FILE ("first|later <ms to a full bar> <end freeze ms> <count>" lines): at start. */
static void LearnLoad(void)
{
    char path[MAX_PATH], *t;
    for (int k = 0; k < 2; k++) {
        InterlockedExchange(&g_lnMs[k], 0); InterlockedExchange(&g_lnFreeze[k], 0); InterlockedExchange(&g_lnN[k], 0);
    }
    if (!g_dir[0] || !Join(path, g_dir, LEARN_FILE) || !(t = ReadText(path, 512))) return;
    for (const char *q = t; *q;) {
        int k = -1;
        if (q[0] == 'f' && q[1] == 'i' && q[2] == 'r' && q[3] == 's' && q[4] == 't' && q[5] == ' ') k = 0;
        else if (q[0] == 'l' && q[1] == 'a' && q[2] == 't' && q[3] == 'e' && q[4] == 'r' && q[5] == ' ') k = 1;
        if (k >= 0) {
            q += 6;
            LONG ms = LearnNum(&q), fz = LearnNum(&q), n = LearnNum(&q);
            if (ms >= LEARN_MIN_MS && ms <= LEARN_MAX_MS && fz >= 0 && fz <= LEARN_MAX_MS && n > 0) {
                InterlockedExchange(&g_lnMs[k], ms); InterlockedExchange(&g_lnFreeze[k], fz);
                InterlockedExchange(&g_lnN[k], n);
            }
        }
        while (*q && *q != '\n') q++;
        if (*q) q++;
    }
    HeapFree(GetProcessHeap(), 0, t);
}

/* The worker, as a screen ends (m: 200 bytes of room): a world map screen teaches its kind its time to a full bar and
 * the game's freeze after it; a later screen that is a login teaches nothing (a login is not a later load's length). */
static void LearnScreen(char *m)
{
    LONG v = InterlockedExchange(&g_fullMs, 0), fz = g_endFreezeMs;
    int k = g_screenKind ? 1 : 0;
    if (!g_screenMap || (k && g_screenLogin) || v < LEARN_MIN_MS || v > LEARN_MAX_MS) return;
    if (fz < 0 || fz > LEARN_MAX_MS) fz = 0;
    LONG n = g_lnN[k], ms = n > 0 ? (g_lnMs[k] + v) / 2 : v, f2 = n > 0 ? (g_lnFreeze[k] + fz) / 2 : fz;
    if (n < 9999) n++;
    InterlockedExchange(&g_lnMs[k], ms); InterlockedExchange(&g_lnFreeze[k], f2); InterlockedExchange(&g_lnN[k], n);
    char path[MAX_PATH], text[96];
    if (g_dir[0] && Join(path, g_dir, LEARN_FILE)) {
        wsprintfA(text, "first %ld %ld %ld\r\nlater %ld %ld %ld\r\n", (long)g_lnMs[0], (long)g_lnFreeze[0], (long)g_lnN[0],
                  (long)g_lnMs[1], (long)g_lnFreeze[1], (long)g_lnN[1]);
        WriteText(path, text);
    }
    wsprintfA(m, "  learned: this machine's %s loads fill the bar in %ld.%02ld s and then freeze %ld.%02ld s (%ld so far)",
              k ? "later" : "first", (long)(ms / 1000), (long)(ms % 1000 / 10), (long)(f2 / 1000), (long)(f2 % 1000 / 10),
              (long)n);
    Log(m);
}

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
    InterlockedExchange(&g_holdMs, HOLD_MS);
    InterlockedExchange(&g_musicFadeMs, MUSIC_FADE_MS);
    InterlockedExchange(&g_blurMs, BLUR_MS);
    InterlockedExchange(&g_blurDim, BLUR_DIM);
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
        } else if (n > 4 && l[0] == 'e' && l[1] == 'n' && l[2] == 't' && MapSpace(l[3]) && MapHex(l, n, &i, &id) && id > 1 &&
                   MapHex(l, n, &i, &v[0]) && v[0] <= 1 && MapHex(l, n, &i, &v[1]) && MapHex(l, n, &i, &v[2])) {
            float x = MapBits(v[1]), y = MapBits(v[2]);
            int k = 0;
            while (k < g_map.ents && g_map.ent[k].id != id) k++;               /* a later line for the same map wins */
            if (x > -20000 && x < 20000 && y > -20000 && y < 20000 && k < MAP_ENTS) {
                g_map.ent[k].id = id; g_map.ent[k].c = (int)v[0]; g_map.ent[k].x = x; g_map.ent[k].y = y;
                if (k == g_map.ents) g_map.ents++;
            }
        } else if (n > 4 && ((l[0] == 'e' && l[1] == 'n' && l[2] == 'd' && MapSpace(l[3])) ||
                             (n > 5 && l[0] == 'p' && l[1] == 'a' && l[2] == 'c' && l[3] == 'e' && MapSpace(l[4])) ||
                             (n > 5 && l[0] == 'h' && l[1] == 'o' && l[2] == 'l' && l[3] == 'd' && MapSpace(l[4])) ||
                             (n > 6 && l[0] == 'm' && l[1] == 'u' && l[2] == 's' && l[3] == 'i' && l[4] == 'c' &&
                              MapSpace(l[5])) ||
                             (n > 5 && l[0] == 'b' && l[1] == 'l' && l[2] == 'u' && l[3] == 'r' && MapSpace(l[4])))) {
            int y = 0, d = 0, pace = l[0] == 'p', hold = l[0] == 'h', music = l[0] == 'm', blur = l[0] == 'b';
            i = music ? 6 : pace || hold || blur ? 5 : 4;
            while (i < n && MapSpace(l[i])) i++;
            for (; i < n && l[i] >= '0' && l[i] <= '9' && d < 6; i++, d++) y = y * 10 + (l[i] - '0');
            if (d && pace) g_map.pace = (float)y;
            else if (d && hold) InterlockedExchange(&g_holdMs, y <= 20000 ? y : 20000);
            else if (d && music) InterlockedExchange(&g_musicFadeMs, y <= 5000 ? y : 5000);
            else if (d && blur) {
                int dim = 0, dd = 0;
                InterlockedExchange(&g_blurMs, y <= 2000 ? y : 2000);
                while (i < n && MapSpace(l[i])) i++;
                for (; i < n && l[i] >= '0' && l[i] <= '9' && dd < 3; i++, dd++) dim = dim * 10 + (l[i] - '0');
                if (dd) InterlockedExchange(&g_blurDim, dim <= 100 ? dim : 100);
            }
            else if (d) g_map.endYards = (float)y;
        }
        at = end + 1;
    }
    if (!(g_map.pace >= 1 && g_map.pace <= 60)) g_map.pace = 7;
    g_map.ok = (g_map.m[0].ok || g_map.m[1].ok) && g_map.endYards >= 100;
}
/* Instance id's entrance: 1 with its continent and world place, when map.txt lists one on a continent it can show. */
static int MapEntrance(unsigned id, unsigned *c, float *x, float *y)
{
    for (int k = 0; k < g_map.ents; k++)
        if (g_map.ent[k].id == id && g_map.m[g_map.ent[k].c].ok) {
            *c = (unsigned)g_map.ent[k].c; *x = g_map.ent[k].x; *y = g_map.ent[k].y;
            return 1;
        }
    return 0;
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
static const char *const kClassName[12] = { "none", "warrior", "paladin", "hunter", "rogue", "priest", "?", "shaman",
                                            "mage", "warlock", "?", "druid" };
static int g_barClassOk;                               /* the class can be read for the bar (FireBar) */
static volatile LONG g_barClassUsed;                   /* the class whose fire this screen's bar took, or 0 */
static unsigned BarClassNow(void);                     /* the character's class (the badge's, below), or 0 */

/* The family bars.txt names for class c ("class<name> <family>"): its index, -1 for the pack's orange (a family not in
 * the pack, orange itself among them), or -2 when it names none, so the picture's stands. */
static int BarClassFamily(const BarTable *t, unsigned c)
{
    char line[16];
    if (c >= 12 || kClassName[c][0] == '?' || c == 0) return -2;
    lstrcpyA(line, "class");
    lstrcatA(line, kClassName[c]);
    for (int k = 0; k < t->screens; k++)
        if (BarSame(t->screen[k].name, line)) {
            int f = t->screen[k].family;
            return f >= 0 && t->family[f].ok ? f : -1;
        }
    return -2;
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
        if (!BarBaseName(path, lstrlenA(path), name)) return path;
        if (m && (g_map.ents || BarIsOverworld(name))) return MAP_PATH;   /* with entrances: every screen's picture */
        if (!m && w > 0 && w <= g_walls.count && BarIsOverworld(name)) return g_walls.path[w - 1];
        return path;
    }
    int f;
    if (fill) {                                        /* the fill comes first: the screen's choices are made here */
        char name[BAR_NAME_MAX + 1];
        int named = BarScreen(name), over = (g_map.ok || g_walls.count) && named && BarIsOverworld(name);
        if (g_map.ok && (over || g_map.ents)) {        /* the world map (with entrances, on every screen), its fire from
                                                        * bars.txt's "cindermap" line */
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
        unsigned c = g_barClassOk ? BarClassNow() : 0;
        int cf = BarClassFamily(&g_bars, c);           /* the class's own fire, where bars.txt names one */
        if (cf != -2) f = cf;
        InterlockedExchange(&g_barClassUsed, cf != -2 ? (LONG)c : 0);
        int g = f >= 0 && g_bars.family[f].garland;
        InterlockedExchange(&g_barGarland, g ? f + 1 : 0);
        BarSizeSet(g);                                 /* a garland's bar is the banner's mirror, tall */
        InterlockedExchange(&g_barFamNow, f);
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
    LONG c = InterlockedExchange(&g_barClassUsed, 0);
    if (g_barsOn && shown && c > 0 && c < 12) {
        BarCat(out, cap, ", the ");
        BarCat(out, cap, kClassName[c]);
        BarCat(out, cap, "'s");
    }
    if (g_barsOn && g_mapShown) {
        char z[64];
        float h = g_viewH;
        int x10 = h > 0.001f ? (int)(5.0f / h + 0.5f) : 0;            /* 0.5 / h, in tenths */
        int z100 = (int)(g_zoomStart * 100 + 0.5f);
        if (g_zoomHalf) wsprintfA(z, ", the world map from %d%% in, ending %d.%dx in", z100, x10 / 10, x10 % 10);
        else wsprintfA(z, ", the world map, ending %d.%dx in", x10 / 10, x10 % 10);
        BarCat(out, cap, z);
        static const char *const way[6] = { "", "its entrance", "the game's, written during it", "the character list's",
                                            "the game's as it stood", "the game's, written before it" };
        LONG kp = g_destKnownPct, kw = g_destKnownWay;
        if (kp < 0) BarCat(out, cap, ", the arrival never known");
        else {
            wsprintfA(z, ", the arrival known at %ld%% (%s)", (long)kp, kw >= 1 && kw <= 5 ? way[kw] : "?");
            BarCat(out, cap, z);
        }
        if (g_driftResets) { wsprintfA(z, ", %ld resets mid-screen", (long)g_driftResets); BarCat(out, cap, z); }
        if (g_musicFaded) BarCat(out, cap, ", the character list's music faded out");
        LONG hl = g_holdLast;
        if (hl > 0) { wsprintfA(z, ", held %ld.%ld s for the floor", (long)(hl / 1000), (long)(hl % 1000 / 100)); BarCat(out, cap, z); }
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

/* At start: where WoW.exe's .data section lies (see ClientWord), from its own headers in memory; left 0, 0 when they are
 * not as a PE image's. */
static void ClientDataFind(void)
{
    const unsigned char *b = (const unsigned char *)GetModuleHandleA(NULL);
    g_dataLo = g_dataHi = 0;
    if (!b || !Readable(b, 0x40) || b[0] != 'M' || b[1] != 'Z') return;
    unsigned pe = Rd32(b + 0x3C);
    if (pe < 0x40 || pe > 0x1000 || !Readable(b + pe, 24) || Rd32(b + pe) != 0x4550u) return;   /* "PE\0\0" */
    unsigned n = b[pe + 6] | (unsigned)b[pe + 7] << 8, sh = pe + 24 + (b[pe + 20] | (unsigned)b[pe + 21] << 8);
    for (unsigned i = 0; i < n && i < 32 && Readable(b + sh + 40 * i, 40); i++) {
        const unsigned char *x = b + sh + 40 * i;
        unsigned size = Rd32(x + 8), rva = Rd32(x + 12);
        if (x[0] == '.' && x[1] == 'd' && x[2] == 'a' && x[3] == 't' && x[4] == 'a' && !x[5] && rva >= 0x1000 && size &&
            size < 0x4000000 && Readable(b + rva, 1) && Readable(b + rva + size - 1, 1)) {
            g_dataLo = (ULONG_PTR)(b + rva);
            g_dataHi = g_dataLo + size;
            return;
        }
    }
}

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
    LearnLoad();
    {
        char m[200];
        if (g_holdMs > 0)
            wsprintfA(m, "zoom: no loading screen under %ld.%ld s; learned here: first loads %ld.%02ld s (%ld), later %ld.%02ld s "
                      "(%ld)", (long)(g_holdMs / 1000), (long)(g_holdMs % 1000 / 100), (long)(g_lnMs[0] / 1000),
                      (long)(g_lnMs[0] % 1000 / 10), (long)g_lnN[0], (long)(g_lnMs[1] / 1000), (long)(g_lnMs[1] % 1000 / 10),
                      (long)g_lnN[1]);
        else
            lstrcpyA(m, "zoom: no floor (map.txt says hold 0): the bar and the zoom follow the game alone");
        Log(m);
    }
    HoldSetup();
    MusicSetup();
    PresentSetup();
    if (g_map.ents) {
        char m[200];
        wsprintfA(m, "zoom: the world map is in the pack, on every loading screen; the view glides down onto where you arrive, "
                  "or onto the entrance of the dungeon, raid or battleground (%d listed)", g_map.ents);
        Log(m);
    } else
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
/* ---- the garland (4 Oct 2026, his design for the druid: "flowers/plants/leaves that load across the bottom like the
 * Path of Cenarius", leaves and flowers growing and overlapping left to right, "wispy bluish nightelf wisp magic" at the
 * front, the bar the top banner's mirror: T tall at both edges, a third of it at the centre) ----
 * A fire family bars.txt marks "style <family> garland" is drawn as one: the bar ARCH_H tall instead of the log's
 * height (its size table set as each screen begins), its fill laid once across the bar, not flowing and not cooling,
 * Loading-BarBack-<family>.blp (the arch's shape, dark) under it in place of the black band, and the wisps of
 * Loading-BarWisps-<family>.blp (its sprites in CinderLoad\wisps-<family>.txt) rising from the growing front. Without
 * its backing in the pack a family is no garland (its tall pictures would show squeezed into the log). */
static int LblInt(const char *s, int n, int *at, int *out);       /* the labels' (below) */
#define WISP_SPRITES 16
#define WISP_KINDS   7          /* 0 the wisp, 1 its trail, 2 a second wisp, 3 a small one, 4 the aura, 5 motes, 6 sparkles */
static struct { int n; struct { int kind; float u0, v0, u1, v1, w, h; } s[WISP_SPRITES]; } g_wispTab[BAR_FAMILIES];

/* CinderLoad\wisps-<family>.txt: "s <kind> <x0> <y0> <x1> <y1>" on a 512 x 512 sheet, sizes as for a 2160-tall screen. */
static void WispsParse(int f, const char *t, unsigned len)
{
    g_wispTab[f].n = 0;
    for (unsigned at = 0; at < len && g_wispTab[f].n < WISP_SPRITES;) {
        unsigned end = at;
        while (end < len && t[end] != '\n') end++;
        const char *l = t + at;
        int n = (int)(end - at), i = 2, v[5], k = 0;
        if (n > 2 && l[0] == 's' && MapSpace(l[1]))
            while (k < 5 && LblInt(l, n, &i, &v[k])) k++;
        if (k == 5 && v[0] >= 0 && v[0] < WISP_KINDS && v[1] >= 0 && v[2] >= 0 && v[3] > v[1] && v[4] > v[2] && v[3] <= 512 &&
            v[4] <= 512) {
            int j = g_wispTab[f].n++;
            g_wispTab[f].s[j].kind = v[0];
            g_wispTab[f].s[j].u0 = v[1] / 512.0f; g_wispTab[f].s[j].v0 = v[2] / 512.0f;
            g_wispTab[f].s[j].u1 = v[3] / 512.0f; g_wispTab[f].s[j].v1 = v[4] / 512.0f;
            g_wispTab[f].s[j].w = (float)(v[3] - v[1]); g_wispTab[f].s[j].h = (float)(v[4] - v[2]);
        }
        at = end + 1;
    }
}

/* The blooms (his v2 note, 4 Oct: "the flowers should open up and bloom"): Loading-BarBlooms-<family>.blp, a sheet of
 * flowers opening, a row a kind and a column a frame from bud to full bloom, described by CinderLoad\blooms-<family>.txt
 * ("cell <px>", "types <n>", "frames <n>"); sheet 1024 x 1024. */
static struct { int cell, types, frames; } g_bloomTab[BAR_FAMILIES];
static void BloomsParse(int f, const char *t, unsigned len)
{
    int v[3] = { 0, 0, 0 };
    static const char *const key[3] = { "cell", "types", "frames" };
    for (unsigned at = 0; at < len;) {
        unsigned end = at;
        while (end < len && t[end] != '\n') end++;
        const char *l = t + at;
        int n = (int)(end - at);
        for (int k = 0; k < 3; k++) {
            int kl = lstrlenA(key[k]), i = kl, x = 0, j = 0;
            while (j < kl && j < n && l[j] == key[k][j]) j++;
            if (j == kl && n > kl && MapSpace(l[kl]) && LblInt(l, n, &i, &x)) v[k] = x;
        }
        at = end + 1;
    }
    g_bloomTab[f].cell = v[0]; g_bloomTab[f].types = v[1]; g_bloomTab[f].frames = v[2];
    if (!(v[0] >= 16 && v[0] <= 512 && v[1] >= 1 && v[2] >= 2 && v[1] * v[0] <= 1024 && v[2] * v[0] <= 1024))
        g_bloomTab[f].types = 0;                                        /* no blooms from a sheet that does not fit */
}

/* FireBar's: each garland family's backing and wisps found in the pack (m: 600 bytes of room for the log). */
static void GarlandSetup(char *m)
{
    for (int f = 0; f < g_bars.families; f++) {
        if (!g_bars.family[f].garland) continue;
        lstrcpyA(g_bars.family[f].back, "Interface\\Glues\\LoadingBar\\Loading-BarBack-");
        lstrcatA(g_bars.family[f].back, g_bars.family[f].name);
        lstrcpyA(g_bars.family[f].wisps, "Interface\\Glues\\LoadingBar\\Loading-BarWisps-");
        lstrcatA(g_bars.family[f].wisps, g_bars.family[f].name);
        if (!g_bars.family[f].ok || !BarHasTexture(g_bars.family[f].back)) {
            g_bars.family[f].garland = 0;
            wsprintfA(m, "fire bar: %s is marked a garland but its backing is not in the pack, so it is drawn as a fire",
                      g_bars.family[f].name);
            Log(m);
            continue;
        }
        g_wispTab[f].n = 0;
        if (BarHasTexture(g_bars.family[f].wisps)) {
            char table[64];
            unsigned char *text = NULL;
            unsigned len = 0;
            lstrcpyA(table, "CinderLoad\\wisps-");
            lstrcatA(table, g_bars.family[f].name);
            lstrcatA(table, ".txt");
            if (mpq_read(&kIoSmall, "patch-~.MPQ", table, &text, &len) == MPQ_OK && text && len < 8192)
                WispsParse(f, (const char *)text, len);
            if (text) HeapFree(GetProcessHeap(), 0, text);
        }
        g_bloomTab[f].types = 0;
        lstrcpyA(g_bars.family[f].blooms, "Interface\\Glues\\LoadingBar\\Loading-BarBlooms-");
        lstrcatA(g_bars.family[f].blooms, g_bars.family[f].name);
        if (BarHasTexture(g_bars.family[f].blooms)) {
            char table[64];
            unsigned char *text = NULL;
            unsigned len = 0;
            lstrcpyA(table, "CinderLoad\\blooms-");
            lstrcatA(table, g_bars.family[f].name);
            lstrcatA(table, ".txt");
            if (mpq_read(&kIoSmall, "patch-~.MPQ", table, &text, &len) == MPQ_OK && text && len < 4096)
                BloomsParse(f, (const char *)text, len);
            if (text) HeapFree(GetProcessHeap(), 0, text);
        }
        wsprintfA(m, "fire bar: %s grows as a garland, the bar the banner's mirror (its backing in the pack; %d wisp sprites; "
                  "%d kinds of flower opening in %d frames)", g_bars.family[f].name, g_wispTab[f].n, g_bloomTab[f].types,
                  g_bloomTab[f].types ? g_bloomTab[f].frames : 0);
        Log(m);
    }
}

static int ClassCodeHolds(const unsigned char *base);

static int FireBar(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleA(NULL);
    char m[600];
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
    GarlandSetup(m);
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

    /* The class's own fire: which classes bars.txt names, and whether the class can be read in this client. */
    char cls[12 * 9] = "";
    for (unsigned c = 1; c < 12; c++)
        if (BarClassFamily(&g_bars, c) != -2) {
            if (cls[0]) lstrcatA(cls, ", ");
            lstrcatA(cls, kClassName[c]);
        }
    g_barClassOk = cls[0] && ClassCodeHolds(base);
    if (g_barClassOk) {
        wsprintfA(m, "fire bar: each class burns its own fire where bars.txt names one (%s); the rest, the picture's", cls);
        Log(m);
    } else if (cls[0])
        Log("fire bar: bars.txt names the classes' fires, but the character list is not where it is in the client this "
            "was made for, so every bar takes its picture's fire");

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
static volatile LONG g_destEnd[3], g_destEndOk;                 /* the game's arrival as the last screen ended */
static int g_destWay;                                           /* how MapDest knew it (the screen's log line) */
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

/* The picked character's place in the character list (its map; x and y as float bits): 1 while the list holds it, read
 * there and kept. Only the game's thread reads the list: the game frees it during a login's load, and a read from the
 * view's thread could meet that memory as it goes, so the view's thread (g_viewSteps: it is the one stepping) has what
 * the game's last frame read (GameFacts keeps it each frame while the view is on). */
static volatile LONG g_charKept[4];                             /* 1 when kept, its map, x, y */
static int CharRead(unsigned *m, unsigned *x, unsigned *y)
{
    unsigned e = CharEntry();
    const unsigned char *pm = e ? ClientAt(e + CHAR_MAP) : 0, *px = e ? ClientAt(e + CHAR_X) : 0, *py = e ? ClientAt(e + CHAR_Y) : 0;
    int ok = pm && px && py && Readable(pm, 4) && Readable(px, 4) && Readable(py, 4);
    if (ok) {
        *m = *(volatile const unsigned *)pm; *x = *(volatile const unsigned *)px; *y = *(volatile const unsigned *)py;
        InterlockedExchange(&g_charKept[1], (LONG)*m); InterlockedExchange(&g_charKept[2], (LONG)*x);
        InterlockedExchange(&g_charKept[3], (LONG)*y);
    }
    InterlockedExchange(&g_charKept[0], ok);
    return ok;
}
static int CharPlace(unsigned *m, unsigned *x, unsigned *y)
{
    if (!g_viewSteps) return CharRead(m, x, y);
    if (!g_charKept[0]) return 0;
    *m = (unsigned)g_charKept[1]; *x = (unsigned)g_charKept[2]; *y = (unsigned)g_charKept[3];
    return 1;
}

/* Where the player arrives, on the world map: 1 with its u, v and the view's half size there; 0 while it is not
 * known (see above). p: the bar so far. */
static int MapDest(float *du, float *dv, float *hEnd, float p)
{
    unsigned id, dm, xb, yb, e, c, xb2, yb2;
    float ex, ey;
    if (!ClientWord(BAR_MAP_VA, &id)) return 0;
    g_destWay = 1;
    if (id > 1)                                      /* a dungeon, raid or battleground: its entrance, from the start */
        return MapEntrance(id, &c, &ex, &ey) && MapPlace(c, ex, ey, du, dv, hEnd);
    if (!g_map.m[id].ok) return 0;
    int game = ClientWord(MAP_DEST_MAP_VA, &dm) && ClientWord(MAP_DEST_X_VA, &xb) && ClientWord(MAP_DEST_Y_VA, &yb);
    int fresh = game && (dm != g_destSnap[0] || xb != g_destSnap[1] || yb != g_destSnap[2]);
    /* the game wrote it before this screen began: it differs from where the last screen left it (a later load's
     * NEW_WORLD; a login's stale place is still the last screen's, so it never counts this way) */
    int moved = game && g_destEndOk && (dm != (unsigned)g_destEnd[0] || xb != (unsigned)g_destEnd[1] ||
                                        yb != (unsigned)g_destEnd[2]);
    if ((fresh || moved) && dm == id && MapPlace(id, MapBits(xb), MapBits(yb), du, dv, hEnd)) {
        g_destWay = fresh ? 2 : 5;
        return 1;
    }
    if (CharPlace(&e, &xb2, &yb2) && e == id && MapPlace(id, MapBits(xb2), MapBits(yb2), du, dv, hEnd)) {
        g_destWay = 3;                                               /* a login: the list's own place */
        return 1;
    }
    g_destWay = 4;
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
static float g_fillShown;                                      /* the bar as shown on the game's last loading frame */
static DWORD g_dispAt, g_dispStill;                           /* ms of frames on, the game's bar unmoved, so far */
static DWORD g_gameAt;                                         /* the game's last loading frame (GameFacts) */
static float g_gameMax;                                        /* the game's bar, its highest this screen */
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

/* ---- whose thread moves the bar and the zoom (4 Oct 2026, option 1: the view, below) ----
 * Without the view, the game's thread does, in each loading frame it draws, as it always did (DisplayStep, DriftStep).
 * While the view shows, the view's own thread does, at its own frames, so a frame the game cannot draw never stops them;
 * the game's thread then only notes what its own frames alone can tell (GameFacts) and draws its frame, hidden under the
 * view, from the view's numbers. A screen always begins on the game's thread, at its first frame: the music's fade and
 * the blur's release are the game's own calls and its device's, and only that frame knows the bar is this screen's (the
 * view's thread waits for it, and never moves them while a new screen is due). g_stepLock keeps the two threads from
 * moving them at once, and is only taken while the view is on (g_viewOn, set once, at start). */
static float g_viewShown;                                       /* the bar as shown at the view's last frame */
static CRITICAL_SECTION g_stepLock;
static int StepLock(void) { if (!g_viewOn) return 0; EnterCriticalSection(&g_stepLock); return 1; }
static void StepUnlock(int held) { if (held) LeaveCriticalSection(&g_stepLock); }
/* now for a step of the bar and the zoom, never before the last one: each thread reads its clock before it takes the
 * lock, so the other may have stepped since with a later one (the view's own window opening takes a moment, too), and a
 * step back in time would read as a pause of 49 days: the bar and the zoom would leap DISP_GAP_MAX's worth at once. */
static DWORD StepNow(DWORD now)
{
    if (g_dispAt && (LONG)(now - g_dispAt) < 0) now = g_dispAt;
    if (g_driftAt && (LONG)(now - g_driftAt) < 0) now = g_driftAt;
    return now;
}

/* The game's thread, each loading frame it draws: a new screen's start, and what only its own frames can tell (how many,
 * the pauses between them and where, the time to a full bar, the freeze at its end). */
static void GameFacts(float p, DWORD now)
{
    DWORD gap = g_gameAt ? now - g_gameAt : 0;
    int fallback = !g_screenWatch && g_gameAt && gap > 1000 && p < 0.1f && g_gameMax > 0.5f;
    if (InterlockedExchange(&g_dispNew, 0) || !g_gameAt || fallback) {
        InterlockedExchange(&g_viewSteps, 0);                         /* (the view takes them back after this frame) */
        g_disp = g_dispMax = g_gameMax = p;
        g_dispV = g_dispRate = 0;
        g_dispAt = now;                                               /* the bar as shown starts here, without a step */
        InterlockedExchange(&g_frmCount, 0); InterlockedExchange(&g_frmGap, 0); InterlockedExchange(&g_frmDips, 0);
        InterlockedExchange(&g_hitchN, 0); InterlockedExchange(&g_stallN, 0);
        for (int i = 0; i < HITCH_KEEP; i++) {
            InterlockedExchange(&g_hitchMs[i], 0); InterlockedExchange(&g_stallMs[i], 0);
        }
        g_dispStill = 0;
        gap = 0;
        g_scrStartAt = now;
        InterlockedExchange(&g_fullMs, 0);
        InterlockedExchange(&g_endFreezeMs, 0);
        g_screenKind = g_zoomScreens > 0;                             /* (DriftStep counts this screen after) */
        g_holdDone = 0;
        g_settleView = 0;
        g_planU = 0;
        g_blurFrom = g_blurTo = 0;
        if (g_blurUsed) { g_blurUsed = 0; g_dissolveAt = 0; BlurRelease(); }
        InterlockedExchange(&g_holdLast, 0);
        MusicFade();                                                  /* the character list's music out, snappily */
        LONG full = g_lnN[g_screenKind] > 0 ? g_lnMs[g_screenKind] : (g_screenKind ? 0 : FIRST_GUESS_MS);
        LONG fz = g_lnN[g_screenKind] > 0 ? g_lnFreeze[g_screenKind] : FREEZE_GUESS_MS, floor = g_holdMs - fz;
        LONG plan = full > floor ? full : floor;
        g_planEndAt = now + (DWORD)(plan > 300 ? plan : 300);
    }
    InterlockedExchange(&g_gameSeq, g_gameSeq + 1);                   /* the game thread is these counts' only writer */
    if (g_viewOn) { unsigned m, x, y; CharRead(&m, &x, &y); }       /* the character list's place, for the view's */
    g_loadFrame = 1;                                                  /* this frame is a loading screen's (the present) */
    if (p >= 0.999f && !g_fullMs)                                     /* the game's bar full: the time it took */
        InterlockedExchange(&g_fullMs, now - g_scrStartAt > 0 ? (LONG)(now - g_scrStartAt) : 1);
    if (g_gameMax >= 0.999f && gap >= HITCH_MS && (LONG)gap > g_endFreezeMs)   /* the game's own freeze once it is done */
        InterlockedExchange(&g_endFreezeMs, (LONG)gap);
    InterlockedExchange(&g_frmCount, g_frmCount + 1);
    if (gap > (DWORD)g_frmGap) {
        InterlockedExchange(&g_frmGap, (LONG)gap);
        InterlockedExchange(&g_frmGapFrom, (LONG)(g_gameMax * 100 + 0.5f));
        InterlockedExchange(&g_frmGapTo, (LONG)(p * 100 + 0.5f));
    }
    if (p < g_gameMax - 0.2f) InterlockedExchange(&g_frmDips, g_frmDips + 1);
    if (gap >= HITCH_MS) HitchKeep(&g_hitchN, g_hitchMs, g_hitchPct, (LONG)gap, (LONG)(g_gameMax * 100 + 0.5f));
    if (gap < HITCH_MS) g_dispStill += gap;                           /* a hitch is not a stall: only drawn time counts */
    if (p > g_gameMax) {                                              /* the game's bar moves: was it still a while? */
        if (g_dispStill >= STALL_MS)
            HitchKeep(&g_stallN, g_stallMs, g_stallPct, (LONG)g_dispStill, (LONG)(g_gameMax * 100 + 0.5f));
        g_dispStill = 0;
        g_gameMax = p;
    }
    g_gameAt = now;
}

/* The thread that moves the bar (the game's, or the view's while it shows): the bar as shown for the game's p (0..1) at
 * now, a step on from the last. */
static float BarStep(float p, DWORD now)
{
    DWORD gap = g_dispAt ? now - g_dispAt : 0;
    /* the plan's bar: on to 1 by g_planEndAt, through any pause (time does not stop for one) */
    g_timed = g_holdMs > 0 && g_mapShown && g_map.ok;
    if (gap > 0) {
        LONG left = (LONG)(g_planEndAt - now);
        g_planU = left <= 0 ? 1 : g_planU + (1 - g_planU) * FireClamp((float)gap / (float)(left + (LONG)gap), 0, 1);
    }
    g_dispAt = now;
    float dt = (float)gap / 1000.0f;
    if (dt > DISP_GAP_MAX) dt = DISP_GAP_MAX;
    float rise = p > g_dispMax ? p - g_dispMax : 0;
    if (p > g_dispMax) g_dispMax = p;
    if (dt <= 0) return g_timed && g_planU < g_disp ? g_planU : g_disp;
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
    float shown = g_timed && g_planU < g_disp ? g_planU : g_disp;    /* on the plan: never ahead of it, nor of the game */
    InterlockedExchange(&g_frmShown, (LONG)(shown * 100 + 0.5f));
    return shown;
}

/* Its frame's facts and the bar as shown, as each loading frame did before the view: the tests' way in (GameStep, below,
 * is the game's). */
__attribute__((unused)) static float DisplayStep(float p, DWORD now)
{
    GameFacts(p, now);
    return BarStep(p, now);
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
static DWORD g_settleFrom, g_settleTo;                          /* the hold's glide in (see HoldFrame), 0 for none */
static float g_settleU, g_settleV, g_settleH;                   /* ... from the view as the game finished */
#define VIEW_WORLD_GAP_MS 400   /* the world shows this long after the screen goes (his loads, 4-5 Oct: 378 to 394 ms) */
#define SETTLE_NEAR     0.02f      /* of the zoom (in its even steps) or of the view's width: landed already */

/* The fire's worker, when a screen has gone (the loading screen's layer cleared): the next screen starts from the
 * whole world. Written between screens, when no loading screen is drawn, so no frame shows it. */
static void DriftScreenGone(void)
{
    static const float whole[8] = { 0, 1, 1, 1, 0, 0, 1, 0 };
    int held = StepLock();                                        /* (the view's thread may be finishing a step) */
    for (int i = 0; i < 8; i++) g_bgUV[i] = whole[i];
    unsigned dm, xb, yb;                                          /* where this screen left the game's arrival */
    if (ClientWord(MAP_DEST_MAP_VA, &dm) && ClientWord(MAP_DEST_X_VA, &xb) && ClientWord(MAP_DEST_Y_VA, &yb)) {
        InterlockedExchange(&g_destEnd[0], (LONG)dm); InterlockedExchange(&g_destEnd[1], (LONG)xb);
        InterlockedExchange(&g_destEnd[2], (LONG)yb); InterlockedExchange(&g_destEndOk, 1);
    } else
        InterlockedExchange(&g_destEndOk, 0);
    InterlockedExchange(&g_driftNew, 1);
    InterlockedExchange(&g_dispNew, 1);
    StepUnlock(held);
}

/* The thread that moves the zoom (see GameStep), a new screen always the game's. p: the bar, 0..1. A new screen (no
 * frame for a second, or another map) starts again; within a screen the bar only goes up (4 Oct 2026: it dips for a
 * frame at the very end, and the view used to jump back out to the whole world). With the world map: from the whole
 * world, the view zooms evenly (in steps of the same ratio) onto where the player arrives, its middle following the
 * zoom, the approach done by about 90% of the bar so the end of it rests on them. Otherwise: the picture drifts in. */
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
        g_settleTo = 0;
        InterlockedExchange(&g_destKnownPct, -1);
        ArrivalSnap();
        InterlockedExchange(&g_zoomHalf, g_zoomScreens > 0);
        InterlockedExchange(&g_screenLogin, CharEntry() != 0);
        InterlockedExchange(&g_screenMap, g_mapShown && g_map.ok);
        g_zoomStart = g_zoomScreens > 0 ? LATER_START : 0;          /* the session's first: the whole world */
        if (g_mapShown && g_map.ok) g_zoomScreens++;
    }
    if (p > g_driftMax) g_driftMax = p;
    p = g_driftMax;
    float dt = g_driftAt ? (float)(now - g_driftAt) / 1000.0f : 0;
    g_driftAt = now;
    if (dt > DISP_GAP_MAX) dt = DISP_GAP_MAX;                       /* as the bar: a pause counts whole, up to a limit */
    int zoom = g_mapShown && g_map.ok;
    float step = (p - g_drift) * (float)(1.0 - FireExp(-(double)dt / (zoom ? MAP_BAR_TAU : DRIFT_TAU)));
    if (zoom && !g_timed && step > dt / g_map.pace) step = dt / g_map.pace;   /* untimed: never quicker than pace */
    g_drift += step;
    float e = zoom ? 1 - (1 - g_drift) * (1 - g_drift)              /* the zoom: done early, then resting on them */
                   : g_drift * g_drift * (3 - 2 * g_drift);         /* the drift: eased in and out */
    float cu = 0.5f, cv = 0.5f, h;
    if (zoom) {
        float du, dv, hEnd;
        h = 0.5f;
        int snap = 0;
        if (MapDest(&du, &dv, &hEnd, p)) {                         /* once known, kept for the screen */
            if (!g_destOk) {
                g_destSince = now;
                InterlockedExchange(&g_destKnownPct, (LONG)(p * 100 + 0.5f));
                InterlockedExchange(&g_destKnownWay, g_destWay);
                snap = g_zoomHalf && g_drift < 0.05f;              /* a later screen, known from the start: begin there */
            }
            g_destOk = 1; g_destU = du; g_destV = dv; g_destH = hEnd;
        } else if (g_destOk) {
            du = g_destU; dv = g_destV; hEnd = g_destH;
        }
        if (g_destOk) {
            float z0 = g_zoomHalf ? g_zoomStart : 0;
            e = z0 + (1 - z0) * e;                                  /* after the first: from g_zoomStart in */
            h = 0.5f * (float)FireExp((double)(e * MapLn(hEnd / 0.5f)));
            float k = (0.5f - h) / (0.5f - hEnd);                   /* how far the zoom has come */
            if (g_zoomHalf) {                                       /* ... its middle as far: LATER_MIDDLE at halfway */
                float h0 = 0.5f * (float)FireExp((double)(z0 * MapLn(hEnd / 0.5f)));
                float k0 = (0.5f - h0) / (0.5f - hEnd);
                float m0 = LATER_MIDDLE + (1 - LATER_MIDDLE) * (z0 - LATER_START) / (1 - LATER_START);
                k = k0 < 0.999f ? m0 + (1 - m0) * FireClamp((k - k0) / (1 - k0), 0, 1) : 1;
            }
            cu = 0.5f + (du - 0.5f) * k;
            cv = 0.5f + (dv - 0.5f) * k;
            float pb = BlurAmount(now);                             /* through the blur, closer still */
            if (pb > 0) h *= (float)FireExp(-(double)pb * MapLn(BLUR_PUSH));
        }
        if (g_settleTo && g_destOk) {                               /* the hold's glide in: on the clock, not the bar */
            LONG span = (LONG)(g_settleTo - g_settleFrom);
            float t = span > 0 ? FireClamp((float)(LONG)(now - g_settleFrom) / (float)span, 0, 1) : 1;
            float s = t * t * (3 - 2 * t);
            float hEnd = g_settleView ? g_destH / BLUR_PUSH : g_destH;    /* the view's: the push its last stretch */
            if (hEnd > g_settleH) hEnd = g_settleH;                       /* (never back out) */
            float lh = (float)FireExp((double)((1 - s) * MapLn(g_settleH) + s * MapLn(hEnd)));
            float kc = g_settleH - hEnd > 1e-6f ? FireClamp((g_settleH - lh) / (g_settleH - hEnd), 0, 1) : 1;
            float pb = g_settleView ? 0 : BlurAmount(now);
            g_viewU = g_settleU + (g_destU - g_settleU) * kc;
            g_viewV = g_settleV + (g_destV - g_settleV) * kc;
            g_viewH = pb > 0 ? lh * (float)FireExp(-(double)pb * MapLn(BLUR_PUSH)) : lh;
        } else {
            float a = snap ? 1.0f : (float)(1.0 - FireExp(-(double)dt / MAP_TAU));
            g_viewU += (cu - g_viewU) * a;
            g_viewV += (cv - g_viewV) * a;
            g_viewH += (h - g_viewH) * a;
        }
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

/* The game's thread, each loading frame (RevealFill): its facts, then the bar as shown with the zoom moved on, unless
 * the view's thread is moving them: then the view's numbers, for the game's own frame under it. */
static float GameStep(float p, DWORD now)
{
    int held = StepLock();
    GameFacts(p, now);
    float shown;
    if (g_viewSteps)
        shown = g_viewShown;
    else {
        now = StepNow(now);
        shown = BarStep(p, now);
        if (g_driftOn) DriftStep(shown, now);
    }
    StepUnlock(held);
    return shown;
}

/* ---- the hold (his five-second floor, 4 Oct 2026; see HOLD_MS) ----
 * Read from his WoW.exe: the game draws a loading frame only as a phase reports progress. Three setters (VA 0x407F70,
 * 0x407FA0, 0x407FD0) store their phase and call 0x406900, one loading frame (0x406920 works out the bar into
 * [0x882BE4], 0x406A60 draws, 0x58A960 presents; no arguments, its registers the caller's to lose). Those three calls go
 * to HoldFrame: the frame as ever, then, the first time on a timed screen that the game's bar is full while the plan is
 * not yet met, the same frame again and again until it is (never past HOLD_MAX_MS), and back to the game, which then
 * builds the world. The game's own freeze comes after, on the landed frame. */
#define HOLD_FRAME_VA 0x406900
#define HOLD_BAR_VA   0x882BE4
#define HOLD_FRAMES   4000     /* frames in one hold at most, whatever the clock says */
static const unsigned kHoldCall[3] = { 0x407F8B, 0x407FBB, 0x407FEB };    /* each "call 0x406900" (E8 rel32) */
static const unsigned char kHoldFrame[9] = { 0xE8, 0x1B, 0x00, 0x00, 0x00, 0x6A, 0x00, 0x6A, 0x00 };   /* its start */
static void (*g_holdFrame)(void);
static volatile LONG g_holdOn;

/* The game's thread, in place of each setter's call to the loading frame. */
static void HoldFrame(void)
{
    g_holdFrame();
    unsigned bits;
    if (!g_holdOn || !g_timed || g_holdDone || !ClientWord(HOLD_BAR_VA, &bits) || !(MapBits(bits) >= 0.999f)) return;
    g_holdDone = 1;
    int held = StepLock();                                           /* the view's thread may be moving the view */
    DWORD now = GetTickCount(), start = now;
    LONG blur = g_blurOn && g_blurMs > 0 ? g_blurMs : 0;             /* the ending, inside the floor */
    LONG fz = g_lnN[g_screenKind] > 0 ? g_lnFreeze[g_screenKind] : FREEZE_GUESS_MS, floor = g_holdMs - fz - blur;
    DWORD floorAt = g_scrStartAt + (DWORD)(floor > 0 ? floor : 0), settleAt = now + HOLD_SETTLE_MS;
    DWORD planAt = (LONG)(g_planEndAt - settleAt) < 0 ? g_planEndAt : settleAt;   /* the plan, hurried to settle */
    DWORD endAt = (LONG)(floorAt - planAt) > 0 ? floorAt : planAt;                 /* but never before the floor */
    /* The view still out as the game finishes (his clip, 4 Oct 22:04: the game sat near half, then finished in a
     * rush; the zoom, never ahead of the game, was still over the zone, and the blur came on there and froze). It
     * gets its own glide in, never shorter than HOLD_SETTLE_MS, from where it is onto the arrival, landing at endAt
     * as the blur comes on; every frame after, the game's own freeze included, shows that landed view. */
    if (g_destOk) {
        float rem = g_viewH > g_destH && g_destH < 0.5f ? MapLn(g_viewH / g_destH) / MapLn(0.5f / g_destH) : 0;
        float du = g_viewU - g_destU, dv = g_viewV - g_destV, off = (du < 0 ? -du : du) + (dv < 0 ? -dv : dv);
        if ((rem > SETTLE_NEAR || off > SETTLE_NEAR * 2 * g_viewH) && (LONG)(now + HOLD_SETTLE_MS - endAt) > 0)
            endAt = now + HOLD_SETTLE_MS;
        g_settleFrom = now;
        g_settleTo = (LONG)(endAt - now) > 0 ? endAt : now + 1;
        g_settleU = g_viewU; g_settleV = g_viewV; g_settleH = g_viewH;
        g_settleView = 0;
    }
    int hold = 1;
    if ((LONG)(endAt - now) <= 0) {
        if (!blur) hold = 0;                                                      /* met already: no hold */
        else endAt = now;                                                         /* (but the ending still plays) */
    }
    DWORD stopAt = now;
    if (hold) {
        g_planEndAt = endAt;
        if (blur) {                                               /* from a little before it lands, never before now */
            g_blurFrom = (LONG)(endAt - BLUR_LEAD_MS - now) > 0 ? endAt - BLUR_LEAD_MS : now;
            g_blurTo = endAt + (DWORD)blur;
        }
        stopAt = endAt + (DWORD)(blur > HOLD_TAIL_MS ? blur : HOLD_TAIL_MS);
    }
    /* With the view (his clip, 5 Oct 00:22: "from 7-11s it rushes to get to stormwind fully zoomed in and then just
     * sits there"): the glide above landed by endAt and the push closer came in half a second, because the game's frames
     * were about to stop for its end freeze; the blur hid the rest. The view draws through that freeze, so its last
     * glide in, the push with it, lands as the world appears: the hold's end, this machine's freeze, and the world's
     * own moment after the screen goes (VIEW_WORLD_GAP_MS). */
    if (g_viewSteps && g_destOk) {
        g_settleTo = stopAt + (DWORD)(fz > 0 ? fz : 0) + VIEW_WORLD_GAP_MS;
        g_settleView = 1;
    }
    StepUnlock(held);                                             /* never held through the frames: the view goes on */
    if (!hold) return;
    for (int f = 0; f < HOLD_FRAMES && (LONG)(stopAt - now) > 0 && now - start < HOLD_MAX_MS; f++) {
        Sleep(4);
        g_holdFrame();
        now = GetTickCount();
    }
    InterlockedExchange(&g_holdLast, (LONG)(now - start));
    InterlockedExchange(&g_holdN, g_holdN + 1);
}

/* At start, with a timed world map: the three calls to HoldFrame, once every byte is where it was read. */
static void HoldSetup(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleA(NULL);
    const unsigned char *frame = base + (HOLD_FRAME_VA - 0x400000u);
    InterlockedExchange(&g_holdOn, 0);
    if (g_holdMs <= 0 || !g_map.ok) return;
    if (!Readable(frame, (int)sizeof kHoldFrame) || !Same(frame, kHoldFrame, (int)sizeof kHoldFrame)) {
        Log("hold off: the game's loading frame is not where it was read (VA 0x406900), so loads keep the game's own length");
        return;
    }
    unsigned rel[3];
    for (int i = 0; i < 3; i++) {
        unsigned char *at = base + (kHoldCall[i] - 0x400000u);
        unsigned game = (unsigned)((ULONG_PTR)frame - (ULONG_PTR)(at + 5)), ours = (unsigned)((ULONG_PTR)HoldFrame - (ULONG_PTR)(at + 5));
        if (!Readable(at, 5) || at[0] != 0xE8 || (Rd32(at + 1) != game && Rd32(at + 1) != ours)) {
            Log("hold off: a loading phase's call to its frame is not where it was read, so loads keep the game's own length");
            return;
        }
        rel[i] = ours;
    }
    g_holdFrame = (void (*)(void))(ULONG_PTR)frame;
    for (int i = 0; i < 3; i++) {
        unsigned char *at = base + (kHoldCall[i] - 0x400000u);
        DWORD old;
        if (!VirtualProtect(at + 1, 4, PAGE_EXECUTE_READWRITE, &old)) { Log("hold off: the code could not be made writable"); return; }
        for (int b = 0; b < 4; b++) at[1 + b] = (unsigned char)(rel[i] >> (8 * b));
        VirtualProtect(at + 1, 4, old, &old);
        FlushInstructionCache(GetCurrentProcess(), at, 5);
    }
    InterlockedExchange(&g_holdOn, 1);
    Log("hold on: a loading screen the game finishes early stays up to its plan (three calls at VA 0x407F8B, 0x407FBB, "
        "0x407FEB, in memory only)");
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
static void QuadPut4(float *pos, float *uv, unsigned *col, int q, const float *x, const float *y, const float *tu,
                     const float *tv, const unsigned *c)
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
        col[slot] = c[k];
    }
}
/* The same, one colour for all four corners. */
static void QuadPut(float *pos, float *uv, unsigned *col, int q, const float *x, const float *y, const float *tu,
                    const float *tv, unsigned c)
{
    const unsigned c4[4] = { c, c, c, c };
    QuadPut4(pos, uv, col, q, x, y, tu, tv, c4);
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
#define SEA_SQUARE_U 0.75f     /* the middle of the sheet's two squares: white above, black below */
#define SEA_WHITE_V  0.25f
#define SEA_DEEP_V   0.75f
#define BAR_BLACK_H  (0.09f + 2.0f / 2160)  /* the log's own height, as make_pack.py blacks it in on every other screen */
#define GLINTS      40
#define GLINT_PX    36.0f      /* a glint at its brightest, on a 2160-tall screen */
#define GLINT_LIFE  1.1f       /* seconds a glint shows */
#define GLINT_CREST 0.25f      /* a glint only sparks where the swell is at least this high: on the crests */
static unsigned char g_seaMask[SEA_MAX * SEA_MAX];
static int g_seaW, g_seaH, g_seaOn, g_seaTried, g_glQuads;
static void *g_seaTex;
static struct { float u, v; int cycle, set, on; } g_glint[GLINTS];
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
static float SeaCos(float x) { return SeaSin(x + 1.5707963f); }
static float MapFloor(float x) { float t = (float)(int)x; return t > x ? t - 1 : t; }   /* floor (no C runtime) */

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
 *   the ripple: RIPPLE_RINGS rings opening from where you arrive, each over RIPPLE_S, at a steady size on screen,
 *               appearing over RIPPLE_IN once the arrival is known, and kept all the way in (his ask, 4 Oct: "should
 *               keep going as it zooms in, to draw the eye, maybe even be more ostentatious because the class icon is
 *               pretty small on such a huge continent": wider, quicker, more rings, brighter, never faded out) */
#define MARKS_PATH    "Interface\\Glues\\LoadingScreens\\CinderMarks.blp"
#define MARKS_SHEET   512.0f
#define MARKS_SPRITES 32
#define MARKS_FRAME   16       /* marks a pass at most */
#define MAEL_TURN     36.0f
#define SWIRL_REACH   2.6f
#define SWIRL_TWIST   2.2f     /* radians, at its middle */
#define RIPPLE_RINGS  4
#define RIPPLE_S      2.4f
#define RIPPLE_PX     240.0f   /* a ring at its widest, its radius on a 2160-tall screen (110 before his ask) */
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
    if (!g_destOk) return;
    float sx, sy, appear = FireClamp((float)(now - g_destSince) / (RIPPLE_IN * 1000.0f), 0, 1);
    ViewToScreen(g_destU, g_destV, &sx, &sy);
    for (int k = 0; k < RIPPLE_RINGS; k++) {
        float ph = t / RIPPLE_S + (float)k / RIPPLE_RINGS;
        ph -= (float)(int)ph;
        float px = RIPPLE_PX * (0.12f + 0.88f * ph), a = (1 - ph) * (float)FireExp(0.2 * MapLn(1 - ph + 1e-6f));
        a *= FireClamp(ph / 0.08f, 0, 1) * appear;
        MarkPut(0, sx, sy, px / 2160.0f, 0, a);                       /* the ring's size steady on any screen height */
    }
}

/* ---- the waves, take two (4 Oct 2026, his word on the first: "gigantic waves, continent sized waves") ----
 * CinderWaves.blp (DXT5, 512 square): one tile of small wave crests that repeats on itself (the art kit's wavetile.py),
 * laid over the open sea at two sizes at a time, cross-fading as the view zooms, so the crests keep their size on screen
 * at any zoom: with f the octaves the view has closed in (log2 of the whole picture's height over the view's), the
 * tiles are WAVE_TILE * 2^-floor(f) and half that, weighted 1 - frac(f) and frac(f), between WAVE_TILE / 2 and twice it
 * of the screen's height. Turned WAVE_TURN off the screen's axes (so the repeat never lines up in columns), drifting
 * WAVE_DRIFT tiles a second, cut into cells about WAVE_CELL of the screen across, each corner as bright as the open sea
 * there allows (SeaAt), brighter on the broad swells (SwellAt) and in drifting patches of rougher water. */
#define WAVES_PATH "Interface\\Glues\\LoadingScreens\\CinderWaves.blp"
#define WAVE_TILE   0.35f
#define WAVE_TURN   0.2094395f     /* 12 degrees */
#define WAVE_DRIFT  0.05f
#define WAVE_CELL   0.05f
#define WAVE_ALPHA  0.6f
#define WAVE_QUADS  12288
static int g_wavesOn, g_wavesTried, g_wvQuads;
static void *g_wavesTex;
__attribute__((used)) float g_wvPos[WAVE_QUADS * 6 * 3];
__attribute__((used)) float g_wvUV[WAVE_QUADS * 6 * 2];
__attribute__((used)) unsigned g_wvCol[WAVE_QUADS * 6];

/* How bright the waves are at X across (heights of the world) and V down: the open sea, the swells, the rough patches. */
static float WaveLight(float X, float V, float t, float S0, float asp)
{
    float m = SeaAt(X / asp, V);
    if (m <= 0.004f) return 0;
    float sx = X, sv = V;
    SwirlAt(&sx, &sv, asp);                                        /* the swells curl into the Maelstrom */
    float sw = FireClamp(SwellAt(sx, sv, t), 0, 1);
    float patch = 0.5f + 0.25f * SeaSin(X / S0 * 4.37f + V / S0 * 1.9f + t * 0.21f) +
                  0.25f * SeaSin(-X / S0 * 2.71f + V / S0 * 3.3f + 1.7f - t * 0.17f);
    return m * (0.45f + 0.4f * sw) * (0.35f + 0.9f * patch);
}

/* This frame's waves, from the view (g_bgUV) and the clock, into g_wv* (g_wvQuads). */
static void WaveBuild(float t)
{
    g_wvQuads = 0;
    if (!g_seaW) return;
    float u0 = g_bgUV[0], u1 = g_bgUV[2], vt = g_bgUV[5], vb = g_bgUV[1], vh = vb - vt, asp = (float)g_scrW / (float)g_scrH;
    if (!(vh > 0.001f)) return;
    float f = vh < 1 ? MapLn(1 / vh) / 0.6931472f : 0;
    int n0 = (int)f;
    float frac = f - (float)n0, c = SeaCos(WAVE_TURN), sn = SeaSin(WAVE_TURN), S0 = WAVE_TILE / (float)(1 << n0);
    /* the view's corners on the lattice's own axes: a = X c - V s, b = X s + V c */
    float X0 = u0 * asp, X1 = u1 * asp, amin = 1e9f, amax = -1e9f, bmin = 1e9f, bmax = -1e9f;
    for (int k = 0; k < 4; k++) {
        float X = (k & 1) ? X1 : X0, V = (k & 2) ? vb : vt, a = X * c - V * sn, b = X * sn + V * c;
        if (a < amin) amin = a;
        if (a > amax) amax = a;
        if (b < bmin) bmin = b;
        if (b > bmax) bmax = b;
    }
    for (int layer = 0; layer < 2; layer++) {
        float w = layer ? frac : 1 - frac;
        if (w < 0.02f) continue;
        float S = WAVE_TILE / (float)(1 << (n0 + layer)), oa = t * WAVE_DRIFT * S, ob = t * WAVE_DRIFT * S * 0.35f;
        int sub = (int)(S / vh / WAVE_CELL) + 1;
        if (sub > 12) sub = 12;
        int i0 = (int)MapFloor((amin - oa) / S), i1 = (int)MapFloor((amax - oa) / S);
        int j0 = (int)MapFloor((bmin - ob) / S), j1 = (int)MapFloor((bmax - ob) / S);
        for (int i = i0; i <= i1; i++)
            for (int j = j0; j <= j1; j++)
                for (int p = 0; p < sub; p++)
                    for (int q = 0; q < sub; q++) {
                        if (g_wvQuads >= WAVE_QUADS) return;
                        float x[4], y[4], tu[4], tv[4];
                        unsigned col[4];
                        int lit = 0, seen = 0;
                        for (int k = 0; k < 4; k++) {               /* bottom left, bottom right, top left, top right */
                            float fp = (float)(p + (k & 1)) / sub, fq = (float)(q + ((k & 2) ? 0 : 1)) / sub;
                            float a = oa + ((float)i + fp) * S, b = ob + ((float)j + fq) * S;
                            float X = a * c + b * sn, V = -a * sn + b * c;
                            x[k] = (X / asp - u0) / (u1 - u0);
                            y[k] = 1 - (V - vt) / vh;
                            tu[k] = fp; tv[k] = fq;
                            float al = w * WAVE_ALPHA * WaveLight(X, V, t, S0, asp);
                            unsigned a8 = (unsigned)(FireClamp(al, 0, 1) * 255.0f + 0.5f);
                            col[k] = a8 << 24 | 0xFFFFFFu;
                            lit |= a8 > 0;
                            seen |= x[k] > -0.05f && x[k] < 1.05f && y[k] > -0.05f && y[k] < 1.05f;
                        }
                        if (lit && seen) QuadPut4(g_wvPos, g_wvUV, g_wvCol, g_wvQuads++, x, y, tu, tv, col);
                    }
    }
}

/* ---- the currents (4 Oct 2026, his idea and picks: the sea moves as Earth's does, heated at the equator, the water
 * carried north as it cools, corralled by the continents; the equator through Booty Bay) ----
 * CinderLoad\flow.bin (the art kit's flow.py): 'CFLW', its width and height (16 bits each), the equator's v and the speed
 * scale (floats), then two signed bytes a cell, the current across and down the picture: the trades and the westerlies,
 * turned along every coast into gyres, the Great Sea one great clockwise gyre with the Maelstrom at its heart, the
 * Maelstrom a drain. CinderFlow.blp (DXT5, 256 x 64): eight rows of 8 pixels, a soft line in each of his blues.
 * FLOW_DROPS drops ride it, born evenly on the open sea in view. Each leaves a TRAIL (his ask, 4 Oct: the streaks
 * "nowhere near long enough"; his zoom.earth and windy.com looks): the last FLOW_TRAIL_S x FLOW_TRAIL seconds of its way,
 * bright and wide at its head, fading and narrowing to its tail, every stretch of it in the shade of how fast the water
 * ran there: his blues, slowest to fastest, dark blue, sea green, aquamarine, turquoise, cerulean, light blue, each as
 * much of the moving sea as the next. Water crowds where it gathers (a gyre, a cape) but never into a clump: a crowded
 * patch ages its drops faster. A drop dies by fading: at a coast or in the Maelstrom's eye it stops and fades quickly, in
 * the cold north it ages sooner (the water sinking); reborn in view. */
#define FLOW_PATH    "Interface\\Glues\\LoadingScreens\\CinderFlow.blp"
#define FLOW_MAX     256
#define FLOW_DROPS   1000          /* each a trail now, so fewer than the 2400 dots of the morning of 4 Oct */
#define FLOW_TRAIL   12            /* the points a drop remembers behind it */
#define FLOW_TRAIL_S 0.2f          /* seconds between them: its trail is the last 2.4 s of its way */
#define FLOW_DEGREES 76.0f         /* the picture's height in degrees of latitude (as flow.py) */
#define FLOW_SPEED   0.035f        /* heights of the screen a second, at the field's usual speed */
#define FLOW_SLOWEST 0.6f          /* every drop moves at least this much of that, and at most FLOW_FASTEST times it: the */
#define FLOW_FASTEST 2.2f          /* still middles of the gyres show, and the fast currents run long without swamping */
#define FLOW_ALPHA   0.85f
#define FLOW_PX      2.5f          /* a trail's half width at its head on a 2160-tall screen; 0.4 of it at its tail */
#define FLOW_SHADES  6             /* his blues, slowest to fastest: rows 0..5 of the sheet */
#define FLOW_ROWS    8             /* rows of 8 pixels in the 64-tall sheet */
#define FLOW_GRID_X  24            /* the view in cells, to find a crowd */
#define FLOW_GRID_Y  12
#define FLOW_CROWD   4.0f          /* a cell holding this many times its share ages its drops three times as fast */
#define FLOW_DROP_Q  (FLOW_DROPS * FLOW_TRAIL)
static signed char g_flowCell[FLOW_MAX * FLOW_MAX * 2];
static int g_flowW, g_flowH, g_flowOn, g_flowTried, g_flQuads;
static float g_flowEq, g_flowScale, g_flowUsual;
static float g_flowCut[FLOW_SHADES - 1];   /* the speeds (against the usual) where one shade gives way to the next */
static void *g_flowTex;
static struct {
    float u, v, du, dv, age, life, since;
    int live, dying, n;                    /* dying: stopped at a coast or the eye, fading; n: points on its trail */
    float tu[FLOW_TRAIL], tv[FLOW_TRAIL];  /* its trail, newest first */
    unsigned char shade, ts[FLOW_TRAIL];   /* the shade where it is, and where each point was */
} g_drop[FLOW_DROPS];
static unsigned g_dropSeed;
static unsigned short g_flowCrowd[FLOW_GRID_X * FLOW_GRID_Y];
__attribute__((used)) float g_flPos[FLOW_DROP_Q * 6 * 3];
__attribute__((used)) float g_flUV[FLOW_DROP_Q * 6 * 2];
__attribute__((used)) unsigned g_flCol[FLOW_DROP_Q * 6];

static float GlowF(const unsigned char *p);          /* the glow's (below): a float from four bytes, low first */

/* The cuts between his blues: each shade as much of the moving sea as the next, from the field's own speeds (a
 * histogram in 1/32 steps of log2 of the speed against the usual, from 1/16 to 16 times it). */
static void FlowCuts(void)
{
    static unsigned hist[256];
    unsigned moving = 0;
    for (int b = 0; b < 256; b++) hist[b] = 0;
    for (int c = 0; c < g_flowW * g_flowH; c++) {
        float a = g_flowCell[c * 2], b = g_flowCell[c * 2 + 1], s2 = a * a + b * b;
        if (!(s2 > 0)) continue;
        float rel = (float)FireExp(0.5 * MapLn(s2)) * g_flowScale / g_flowUsual;
        int bin = (int)((MapLn(rel) / 0.69314718f + 4) * 32);
        hist[bin < 0 ? 0 : (bin > 255 ? 255 : bin)]++;
        moving++;
    }
    static const float kEven[FLOW_SHADES - 1] = { 0.6f, 0.85f, 1.1f, 1.4f, 1.9f };
    for (int k = 0; k < FLOW_SHADES - 1; k++) g_flowCut[k] = kEven[k];
    if (!moving) return;
    unsigned sum = 0;
    int k = 0;
    for (int b = 0; b < 256 && k < FLOW_SHADES - 1; b++) {
        sum += hist[b];
        while (k < FLOW_SHADES - 1 && (unsigned long long)sum * FLOW_SHADES >= (unsigned long long)moving * (unsigned)(k + 1))
            g_flowCut[k++] = (float)FireExp(((b + 1) / 32.0 - 4) * 0.69314718);
    }
}
/* The shade of water running at rel times the usual speed: 0 the slowest, FLOW_SHADES - 1 the fastest. */
static int FlowShade(float rel)
{
    int s = 0;
    while (s < FLOW_SHADES - 1 && rel >= g_flowCut[s]) s++;
    return s;
}

/* flow.bin, checked whole: 1 when it is used. */
static int FlowParse(const unsigned char *d, unsigned len)
{
    g_flowW = g_flowH = 0;
    if (len < 16 || d[0] != 'C' || d[1] != 'F' || d[2] != 'L' || d[3] != 'W') return 0;
    int w = d[4] | d[5] << 8, h = d[6] | d[7] << 8;
    float eq = GlowF(d + 8), scale = GlowF(d + 12);
    if (w < 2 || h < 2 || w > FLOW_MAX || h > FLOW_MAX || len != 16u + (unsigned)(w * h * 2) || !(eq > -2 && eq < 3) ||
        !(scale > 0 && scale < 100))
        return 0;
    long sum = 0;
    for (int i = 0; i < w * h * 2; i++) { g_flowCell[i] = (signed char)d[16 + i]; sum += g_flowCell[i] < 0 ? -g_flowCell[i] : g_flowCell[i]; }
    g_flowW = w; g_flowH = h; g_flowEq = eq; g_flowScale = scale;
    g_flowUsual = (float)sum / (float)(w * h * 2) * scale * 2.0f;     /* about the usual speed at sea (land counts 0) */
    if (!(g_flowUsual > 0)) g_flowW = g_flowH = 0;
    if (g_flowW) FlowCuts();
    return g_flowW > 0;
}

/* The current at picture u, v (across and down, in the field's units), between the cells' middles; 0 off the picture. */
static void FlowAt(float u, float v, float *cx, float *cv)
{
    *cx = *cv = 0;
    if (!g_flowW || !(u >= 0 && u <= 1 && v >= 0 && v <= 1)) return;
    float x = FireClamp(u * g_flowW - 0.5f, 0, (float)(g_flowW - 1)), y = FireClamp(v * g_flowH - 0.5f, 0, (float)(g_flowH - 1));
    int x0 = (int)x, y0 = (int)y, x1 = x0 + 1 < g_flowW ? x0 + 1 : x0, y1 = y0 + 1 < g_flowH ? y0 + 1 : y0;
    float fx = x - x0, fy = y - y0;
    for (int k = 0; k < 2; k++) {
        float a = g_flowCell[(y0 * g_flowW + x0) * 2 + k], b = g_flowCell[(y0 * g_flowW + x1) * 2 + k];
        float c = g_flowCell[(y1 * g_flowW + x0) * 2 + k], e = g_flowCell[(y1 * g_flowW + x1) * 2 + k];
        float top = a + (b - a) * fx, bot = c + (e - c) * fx, val = (top + (bot - top) * fy) * g_flowScale;
        if (k) *cv = val; else *cx = val;
    }
}

static float FlowLat(float v) { return (g_flowEq - v) * FLOW_DEGREES; }
static float DropRand(void) { g_dropSeed = SeaHash(g_dropSeed + 0x9E3779B9u); return (float)(g_dropSeed >> 8) / 16777216.0f; }

/* A drop reborn in view, anywhere on the open sea, evenly (his ask, 4 Oct: "spread out evenly around the oceans"): 1 when
 * placed, its trail empty. */
static int DropBirth(int i, float u0, float u1, float vt, float vb)
{
    for (int tries = 0; tries < 4; tries++) {
        float u = u0 + (u1 - u0) * DropRand(), v = vt + (vb - vt) * DropRand();
        if (SeaAt(u, v) < 0.5f) continue;
        g_drop[i].u = u; g_drop[i].v = v; g_drop[i].du = g_drop[i].dv = 0; g_drop[i].age = 0; g_drop[i].since = 0;
        g_drop[i].life = 6 + 6 * DropRand(); g_drop[i].live = 1; g_drop[i].dying = 0; g_drop[i].n = 0;
        return 1;
    }
    return 0;
}

/* One drop's trail as quads: from its head back along the points it passed, each stretch in the shade the water ran
 * there, the alpha and width falling from the head to nothing at FLOW_TRAIL x FLOW_TRAIL_S seconds back. */
static void DropDraw(int i, float u0, float u1, float vt, float vh, float k, float a0)
{
    float total = FLOW_TRAIL * FLOW_TRAIL_S, hs = FLOW_PX / 2160.0f;
    float pu = g_drop[i].u, pv = g_drop[i].v, pt = 0;
    for (int j = 0; j < g_drop[i].n && g_flQuads < FLOW_DROP_Q; j++) {
        float qu = g_drop[i].tu[j], qv = g_drop[i].tv[j], qt = g_drop[i].since + j * FLOW_TRAIL_S;
        float hx = (pu - u0) / (u1 - u0), hy = 1 - (pv - vt) / vh, tx = (qu - u0) / (u1 - u0), ty = 1 - (qv - vt) / vh;
        float sx = (hx - tx) / k, sy = hy - ty, len = (float)FireExp(0.5 * MapLn(sx * sx + sy * sy + 1e-14f));
        float fh = FireClamp(1 - pt / total, 0, 1), ft = FireClamp(1 - qt / total, 0, 1);
        pu = qu; pv = qv; pt = qt;
        if (len < 1e-6f || fh <= 0) continue;
        float dx = sx / len, dy = sy / len, wh = hs * (0.4f + 0.6f * fh), wt = hs * (0.4f + 0.6f * ft);
        float x[4] = { tx + dy * wt * k, hx + dy * wh * k, tx - dy * wt * k, hx - dy * wh * k };
        float y[4] = { ty - dx * wt, hy - dx * wh, ty + dx * wt, hy + dx * wh };
        int row = j ? g_drop[i].ts[j - 1] : g_drop[i].shade;
        float v0 = (row * 8 + 0.5f) / 64.0f, v1 = (row * 8 + 7.5f) / 64.0f;
        float tu4[4] = { 0, 1, 0, 1 }, tv4[4] = { v1, v1, v0, v0 };
        unsigned ah = (unsigned)(a0 * fh * 255.0f + 0.5f), at = (unsigned)(a0 * ft * 255.0f + 0.5f);
        unsigned c4[4] = { at << 24 | 0xFFFFFFu, ah << 24 | 0xFFFFFFu, at << 24 | 0xFFFFFFu, ah << 24 | 0xFFFFFFu };
        QuadPut4(g_flPos, g_flUV, g_flCol, g_flQuads++, x, y, tu4, tv4, c4);
    }
}

/* This frame's drops: each moved along the current (at FLOW_SPEED of the screen's height a second for the field's usual
 * speed, the same on screen at any zoom), its trail kept, aged (faster in a crowd and in the cold north), and drawn. */
static void FlowBuild(float t, float dt)
{
    g_flQuads = 0;
    if (!g_flowW || !g_seaW) return;
    float u0 = g_bgUV[0], u1 = g_bgUV[2], vt = g_bgUV[5], vb = g_bgUV[1], vh = vb - vt, asp = (float)g_scrW / (float)g_scrH;
    if (!(vh > 0.001f)) return;
    (void)t;
    if (dt > 0.25f) dt = 0.25f;
    float mx = g_mael.u, mv = g_mael.v, mr2 = g_mael.ok ? (0.3f * g_mael.r) * (0.3f * g_mael.r) : -1;
    float k = (float)g_scrH / (float)g_scrW;
    for (int c = 0; c < FLOW_GRID_X * FLOW_GRID_Y; c++) g_flowCrowd[c] = 0;
    int alive = 0;
    for (int i = 0; i < FLOW_DROPS; i++) {
        if (!g_drop[i].live && !DropBirth(i, u0, u1, vt, vb)) continue;
        if (!g_drop[i].dying) {
            float cx, cv;
            FlowAt(g_drop[i].u, g_drop[i].v, &cx, &cv);
            float sp = (float)FireExp(0.5 * MapLn(cx * cx + cv * cv + 1e-20f)), rel = sp / g_flowUsual;
            float want = FireClamp(rel, FLOW_SLOWEST, FLOW_FASTEST), step = sp > 1e-12f ? FLOW_SPEED * vh * dt * want / sp : 0;
            g_drop[i].du = cx * step / asp; g_drop[i].dv = cv * step;
            g_drop[i].u += g_drop[i].du; g_drop[i].v += g_drop[i].dv;
            g_drop[i].shade = (unsigned char)FlowShade(rel);
        }
        g_drop[i].since += dt;
        if (g_drop[i].since >= FLOW_TRAIL_S) {                      /* a new point on its trail, the oldest let go */
            for (int j = FLOW_TRAIL - 1; j > 0; j--) {
                g_drop[i].tu[j] = g_drop[i].tu[j - 1]; g_drop[i].tv[j] = g_drop[i].tv[j - 1]; g_drop[i].ts[j] = g_drop[i].ts[j - 1];
            }
            g_drop[i].tu[0] = g_drop[i].u; g_drop[i].tv[0] = g_drop[i].v; g_drop[i].ts[0] = g_drop[i].shade;
            if (g_drop[i].n < FLOW_TRAIL) g_drop[i].n++;
            g_drop[i].since -= FLOW_TRAIL_S;
            if (g_drop[i].since >= FLOW_TRAIL_S) g_drop[i].since = 0;
        }
        float lat = FlowLat(g_drop[i].v);
        g_drop[i].age += dt * (lat > 50 ? 1 + (lat - 50) / 5 : 1);      /* the cold north: it sinks sooner */
        if (g_drop[i].u < u0 - 0.05f * (u1 - u0) || g_drop[i].u > u1 + 0.05f * (u1 - u0) || g_drop[i].v < vt - 0.05f * vh ||
            g_drop[i].v > vb + 0.05f * vh) {
            g_drop[i].live = 0;                                         /* out of view: gone at once */
            continue;
        }
        float ex = (g_drop[i].u - mx) * asp, ev = g_drop[i].v - mv;
        if (!g_drop[i].dying && (SeaAt(g_drop[i].u, g_drop[i].v) < 0.15f || ex * ex + ev * ev < mr2)) {
            g_drop[i].dying = 1;                                        /* a coast or the eye: it stops and fades */
            if (g_drop[i].life > g_drop[i].age + 0.6f) g_drop[i].life = g_drop[i].age + 0.6f;
        }
        if (g_drop[i].age > g_drop[i].life) { g_drop[i].live = 0; continue; }
        int gx = (int)((g_drop[i].u - u0) / (u1 - u0) * FLOW_GRID_X), gy = (int)((g_drop[i].v - vt) / vh * FLOW_GRID_Y);
        gx = gx < 0 ? 0 : (gx >= FLOW_GRID_X ? FLOW_GRID_X - 1 : gx); gy = gy < 0 ? 0 : (gy >= FLOW_GRID_Y ? FLOW_GRID_Y - 1 : gy);
        g_flowCrowd[gy * FLOW_GRID_X + gx]++;
        alive++;
    }
    float share = (float)alive / (FLOW_GRID_X * FLOW_GRID_Y), cap = FLOW_CROWD * (share > 1 ? share : 1);
    for (int i = 0; i < FLOW_DROPS; i++) {
        if (!g_drop[i].live) continue;
        int gx = (int)((g_drop[i].u - u0) / (u1 - u0) * FLOW_GRID_X), gy = (int)((g_drop[i].v - vt) / vh * FLOW_GRID_Y);
        gx = gx < 0 ? 0 : (gx >= FLOW_GRID_X ? FLOW_GRID_X - 1 : gx); gy = gy < 0 ? 0 : (gy >= FLOW_GRID_Y ? FLOW_GRID_Y - 1 : gy);
        if (g_flowCrowd[gy * FLOW_GRID_X + gx] > cap) g_drop[i].age += 2 * dt;     /* a crowd thins: three times the age */
        float fade = g_drop[i].dying ? 0.6f : 1.0f;
        float env = FireClamp(g_drop[i].age / 0.6f, 0, 1) * FireClamp((g_drop[i].life - g_drop[i].age) / fade, 0, 1);
        float a = env * FLOW_ALPHA * FireClamp(SeaAt(g_drop[i].u, g_drop[i].v) * 1.5f, g_drop[i].dying ? 0.4f : 0, 1);
        if (a >= 0.01f) DropDraw(i, u0, u1, vt, vh, k, a);
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
    if (!g_classSure && !g_barClassOk) return;
    if (ClientWord(CHAR_COUNT_VA, &count) && ClientWord(CHAR_LIST_VA, &list) && ClientWord(CHAR_PICKED_VA, &picked) &&
        list && count > 0 && count <= 64 && picked < count)
        c = EntryClass(list + picked * CHAR_SIZE);
    if (c) { InterlockedExchange(&g_class, (LONG)c); InterlockedExchange(&g_classFrom, 1); }
    else if (g_class) InterlockedExchange(&g_classFrom, 2);
    InterlockedExchange(&g_classEntered, ClientWord(CHAR_ENTERED_VA, &entered) ? (LONG)EntryClass(entered) : 0);
}

/* The game's thread, as a screen's bar is chosen (BarPath): the class, read now if the list is there, else the one kept. */
static unsigned BarClassNow(void)
{
    ClassStep();
    LONG c = g_class;
    return c > 0 && c < 12 ? (unsigned)c : 0;
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

/* ---- the class banner (4 Oct 2026, his design: black wrought iron and stained glass across the top, full height at
 * both edges arching to a third of it at the centre, different for each class, the blue moon medallion holding the
 * RavenCraft logo the same on all) ----
 * Interface\Glues\LoadingScreens\CinderArch-<class>.blp (DXT5, ARCH_SHEET_W x ARCH_SHEET_H): the banner, the screen's
 * full width, its top ARCH_ROWS rows; drawn ARCH_H of the screen tall across the top of the map, over the names and the
 * badge and under the logo (which sits in its medallion). The class is the fire's (BarClassNow); a class whose banner
 * the pack lacks has none. */
#define ARCH_H        0.18f
#define ARCH_SHEET_W  4096
#define ARCH_SHEET_H  512
#define ARCH_ROWS     311          /* the banner's 5120 x 389 at 4096 wide */
static int g_archHas[12], g_archOn;
static void *g_archTex[12];
static int g_archTried[12];
__attribute__((used)) float g_archPos[4 * 3];
__attribute__((used)) float g_archUV[4 * 2];
__attribute__((used)) unsigned g_archCol[4];
/* The banner's path for class c into out (80 bytes), or 0 for no class. */
static int ArchPath(unsigned c, char *out)
{
    if (c == 0 || c >= 12 || kClassName[c][0] == '?') return 0;
    lstrcpyA(out, "Interface\\Glues\\LoadingScreens\\CinderArch-");
    lstrcatA(out, kClassName[c]);
    return 1;
}
/* The banner's one quad: the full width, ARCH_H tall, flush with the top. */
static void ArchBuild(void)
{
    float x[4] = { 0, 1, 0, 1 }, y[4] = { 1 - ARCH_H, 1 - ARCH_H, 1, 1 };
    float tv1 = (float)ARCH_ROWS / ARCH_SHEET_H, tu[4] = { 0, 1, 0, 1 }, tv[4] = { tv1, tv1, 0, 0 };
    QuadPut(g_archPos, g_archUV, g_archCol, 0, x, y, tu, tv, 0xFFFFFFFFu);
}

/* ---- the garland's backing and wisps (drawn by the overlay, under the bar) ----
 * The backing: Loading-BarBack-<family>.blp, the arch's dark shape, the full width and ARCH_H tall at the bottom.
 * The wisps: the wisp, its trail and a soft aura ride the front, bobbing; motes rise from just behind it, sparkles
 * twinkle there, small wisps linger over the grown leaves; all inside the arch, which is the banner's own curve. */
#define WISP_MAX 96
static struct { int live, spr; float x, y, vx, vy, age, life, size, phase; } g_wisp[WISP_MAX];
static unsigned g_wispSeed = 7;
static float g_wispCredit, g_wispFront = -1;
static int g_wsQuads;
__attribute__((used)) float g_wsPos[(WISP_MAX + 4) * 6 * 3];
__attribute__((used)) float g_wsUV[(WISP_MAX + 4) * 6 * 2];
__attribute__((used)) unsigned g_wsCol[(WISP_MAX + 4) * 6];
#define WISP_ZIPS      4        /* his v2 note (4 Oct): wisps "floating and zipping around whimsically around the forward
                                 * leading edge": small ones looping around the front, each with a fading tail */
#define WISP_ZIP_TAIL  5

static float WispRand(void) { g_wispSeed = SeaHash(g_wispSeed + 0x9E3779B9u); return (float)(g_wispSeed >> 8) / 16777216.0f; }
/* sin x for any x (FireSin is only for 0..pi/2) */
static float WispSin(float x)
{
    float tp = 6.2831853f;
    x -= (float)(int)(x / tp) * tp;
    if (x < 0) x += tp;
    if (x < 1.5707963f) return FireSin(x);
    if (x < 3.1415927f) return FireSin(3.1415927f - x);
    if (x < 4.712389f) return -FireSin(x - 3.1415927f);
    return -FireSin(tp - x);
}
/* The arch's thickness at x (0..1 of the width), in screen heights: the banner's curve (archlib.py), a circle's arc
 * through full height at both edges and a third of it at the centre, worked on a 5120 x 2160 screen. */
static float GarlandThick(float x)
{
    float W = 5120.0f, T = ARCH_H * 2160.0f, s = T - T / 3.0f, half = W / 2, R = (half * half + s * s) / (2 * s);
    float dx = FireClamp(x, 0, 1) * W - half, r2 = R * R - dx * dx;
    float root = r2 > 0 ? (float)FireExp(0.5 * MapLn(r2)) : 0;
    return FireClamp(T - (root - (R - s)), T / 3.0f - 1, T) / 2160.0f;
}
/* A sprite of the kind: the first (first) or one at random; -1 when the table has none. */
static int WispPick(int f, int kind, int first)
{
    int n = 0, at[WISP_SPRITES];
    for (int j = 0; j < g_wispTab[f].n; j++) if (g_wispTab[f].s[j].kind == kind) at[n++] = j;
    if (!n) return -1;
    return first ? at[0] : at[(int)(WispRand() * n) % n];
}
/* One sprite centred at cx, cy (screen fractions, y up), scale times its size on a 2160-tall screen, alpha a. */
static void WispQuad(int f, int j, float cx, float cy, float scale, float a)
{
    if (j < 0 || a < 0.01f || g_wsQuads >= WISP_MAX + 4) return;
    float k = (float)g_scrH / (float)g_scrW, w = g_wispTab[f].s[j].w * scale / 2160.0f * k, h = g_wispTab[f].s[j].h * scale / 2160.0f;
    float x[4] = { cx - w / 2, cx + w / 2, cx - w / 2, cx + w / 2 }, y[4] = { cy - h / 2, cy - h / 2, cy + h / 2, cy + h / 2 };
    float tu[4] = { g_wispTab[f].s[j].u0, g_wispTab[f].s[j].u1, g_wispTab[f].s[j].u0, g_wispTab[f].s[j].u1 };
    float tv[4] = { g_wispTab[f].s[j].v1, g_wispTab[f].s[j].v1, g_wispTab[f].s[j].v0, g_wispTab[f].s[j].v0 };
    QuadPut(g_wsPos, g_wsUV, g_wsCol, g_wsQuads++, x, y, tu, tv, (unsigned)(FireClamp(a, 0, 1) * 255.0f + 0.5f) << 24 | 0xFFFFFFu);
}
/* This frame's wisps for garland f, the front at `front` (the bar as shown, 0..1). */
static void WispsBuild(int f, float front, float t, float dt)
{
    g_wsQuads = 0;
    if (f < 0 || f >= BAR_FAMILIES || !g_wispTab[f].n) return;
    if (dt > 0.25f) dt = 0.25f;
    int moving = g_wispFront >= 0 && front > g_wispFront + 1e-5f, at = front > 0.002f && front < 0.998f;
    g_wispFront = front;
    if (at) g_wispCredit += dt * (moving ? 22.0f : 4.0f);
    /* his ask (4 Oct, ~19:45): the whitish-blue sparkles keep appearing ACROSS the grown bar after the big wisp has
     * passed, and on once it is full: a second, quieter spawner over all of it */
    static float ambient;
    if (front > 0.01f) ambient += dt * 14.0f * FireClamp(front, 0, 1);
    while (ambient >= 1) {
        ambient -= 1;
        int i = 0;
        while (i < WISP_MAX && g_wisp[i].live) i++;
        if (i == WISP_MAX) { ambient = 0; break; }
        int kind = WispRand() < 0.6f ? 6 : 5, spr = WispPick(f, kind, 0);
        if (spr < 0) continue;
        float x = WispRand() * FireClamp(front, 0, 1);
        g_wisp[i].live = 1; g_wisp[i].spr = spr; g_wisp[i].x = x; g_wisp[i].y = WispRand() * GarlandThick(x) * 1.15f;
        g_wisp[i].vx = 0; g_wisp[i].vy = kind == 5 ? 0.02f + 0.03f * WispRand() : 0;
        g_wisp[i].life = 0.8f + 0.8f * WispRand(); g_wisp[i].size = 0.6f + 0.5f * WispRand();
        g_wisp[i].phase = 6.2831853f * WispRand(); g_wisp[i].age = 0;
    }
    while (g_wispCredit >= 1) {                                    /* the new ones, from just behind the front */
        g_wispCredit -= 1;
        int i = 0;
        while (i < WISP_MAX && g_wisp[i].live) i++;
        if (i == WISP_MAX) { g_wispCredit = 0; break; }
        float r = WispRand();
        int kind = r < 0.55f ? 5 : (r < 0.8f ? 6 : 3), spr = WispPick(f, kind, 0);
        if (spr < 0) continue;
        float x = front - WispRand() * (kind == 3 ? 0.2f : 0.025f);
        g_wisp[i].live = 1; g_wisp[i].spr = spr; g_wisp[i].x = x; g_wisp[i].y = WispRand() * GarlandThick(x);
        g_wisp[i].vx = kind == 5 ? -0.004f - 0.006f * WispRand() : -0.002f;
        g_wisp[i].vy = kind == 5 ? 0.04f + 0.06f * WispRand() : (kind == 3 ? 0.008f : 0);
        g_wisp[i].life = kind == 6 ? 0.5f + 0.4f * WispRand() : (kind == 3 ? 2.0f + 1.5f * WispRand() : 1.4f + 1.2f * WispRand());
        g_wisp[i].size = kind == 6 ? 0.7f + 0.6f * WispRand() : 0.8f + 0.4f * WispRand();
        g_wisp[i].phase = 6.2831853f * WispRand();
        g_wisp[i].age = 0;
    }
    for (int i = 0; i < WISP_MAX; i++) {                           /* each on its way, fading in and out */
        if (!g_wisp[i].live) continue;
        g_wisp[i].age += dt;
        if (g_wisp[i].age >= g_wisp[i].life) { g_wisp[i].live = 0; continue; }
        g_wisp[i].x += g_wisp[i].vx * dt; g_wisp[i].y += g_wisp[i].vy * dt;
        float u = g_wisp[i].age / g_wisp[i].life, a = FireClamp(g_wisp[i].age / 0.15f, 0, 1) * FireClamp((1 - u) / 0.4f, 0, 1);
        if (g_wispTab[f].s[g_wisp[i].spr].kind == 6) a *= 0.5f + 0.5f * WispSin(t * 9.0f + g_wisp[i].phase);
        WispQuad(f, g_wisp[i].spr, g_wisp[i].x, g_wisp[i].y, g_wisp[i].size, a);
    }
    if (!at) return;
    float th = GarlandThick(front), bob = 0.12f * th * WispSin(t * 2.1f);   /* last, on top: the front's own */
    int zip = WispPick(f, 3, 1);
    for (int z = 0; zip >= 0 && z < WISP_ZIPS; z++) {               /* the zipping ones, oldest of each tail first */
        float ph = 1.618f * (float)z * 3.1415927f;
        for (int k = WISP_ZIP_TAIL - 1; k >= 0; k--) {
            float tt = t - 0.035f * (float)k;
            float zx = front + 0.014f * WispSin(1.7f * tt + ph) + 0.006f * WispSin(4.3f * tt + 2 * ph);
            float zy = th * (0.5f + 0.42f * WispSin(2.3f * tt + 3 * ph));
            WispQuad(f, zip, zx, zy, 0.7f * (1 - 0.12f * (float)k), 0.9f * (1 - (float)k / WISP_ZIP_TAIL));
        }
    }
    int aura = WispPick(f, 4, 1), wisp = WispPick(f, 0, 1), trail = WispPick(f, 1, 1);
    if (aura >= 0) WispQuad(f, aura, front, th * 0.5f, th * 2160.0f / g_wispTab[f].s[aura].h, 0.35f);
    if (trail >= 0) {
        float k = (float)g_scrH / (float)g_scrW, tw = g_wispTab[f].s[trail].w / 2160.0f * k * 0.8f;
        WispQuad(f, trail, front - tw * 0.32f, th * 0.5f + bob * 0.5f, 0.8f, 0.7f);    /* its orb at the front, its trail behind */
    }
    if (wisp >= 0) WispQuad(f, wisp, front, th * 0.55f + bob, 0.9f, 1.0f);
}
/* The blooms: BLOOM_MAX flowers set through the arch for the family (the same places every screen), each opening over
 * BLOOM_OPEN_S once the front reaches it, its frames bud to full bloom, growing as it opens; any the front passed in a
 * leap (more than a little behind it) are open at once. Drawn over the garland, under the wisps. */
#define BLOOM_MAX    64
#define BLOOM_OPEN_S 0.7f
#define BLOOM_LOWER  0.125f    /* his v5 note (4 Oct): "the leaves and flowers should be 10-15% lower across board": each
                                * bloom drops this share of the garland's height there (the arch plus the leaves' 85 px
                                * reach), as the art does (class-bar/druid/src/lower.py), never below a tenth of the arch */
static struct { float x, y, scale; int type, on; DWORD t0; } g_bloom[BLOOM_MAX];
static int g_bloomFam = -1, g_bloomScreen = -1, g_bmQuads;
static float g_bloomLast;
__attribute__((used)) float g_bmPos[BLOOM_MAX * 6 * 3];
__attribute__((used)) float g_bmUV[BLOOM_MAX * 6 * 2];
__attribute__((used)) unsigned g_bmCol[BLOOM_MAX * 6];

static void BloomsPlace(int f)
{
    unsigned seed = 0xB100Du + (unsigned)f * 7919u;
    for (int i = 0; i < BLOOM_MAX; i++) {
        float r[4];
        for (int k = 0; k < 4; k++) { seed = SeaHash(seed + 0x9E3779B9u); r[k] = (float)(seed >> 8) / 16777216.0f; }
        float x = ((float)i + 0.2f + 0.6f * r[0]) / BLOOM_MAX, th = GarlandThick(x);
        g_bloom[i].x = x;
        float y = th * (0.2f + 0.6f * r[1]) - BLOOM_LOWER * (th + 85.0f / 2160.0f);
        g_bloom[i].y = y > th * 0.1f ? y : th * 0.1f;
        g_bloom[i].type = g_bloomTab[f].types < 2 || r[2] < 0.4f ? 0 : 1 + (int)(r[3] * (g_bloomTab[f].types - 1)) % (g_bloomTab[f].types - 1);
        g_bloom[i].scale = (0.8f + 0.5f * r[3]) * FireClamp(th / ARCH_H * 1.6f, 0.55f, 1.15f);
        g_bloom[i].on = 0;
    }
}
/* This frame's blooms for garland f, the front at `front` (0..1), screen `screen` (a new one closes them all). */
static void BloomsBuild(int f, float front, DWORD now, int screen)
{
    g_bmQuads = 0;
    if (f < 0 || f >= BAR_FAMILIES || !g_bloomTab[f].types) return;
    if (g_bloomFam != f) { BloomsPlace(f); g_bloomFam = f; }
    if (screen != g_bloomScreen || front < g_bloomLast - 0.05f)
        for (int i = 0; i < BLOOM_MAX; i++) g_bloom[i].on = 0;
    g_bloomScreen = screen;
    g_bloomLast = front;
    float k = (float)g_scrH / (float)g_scrW, cell = (float)g_bloomTab[f].cell, frames = (float)g_bloomTab[f].frames;
    for (int i = 0; i < BLOOM_MAX; i++) {
        if (!g_bloom[i].on && front >= g_bloom[i].x) {
            g_bloom[i].on = 1;
            g_bloom[i].t0 = now - (DWORD)FireClamp((front - g_bloom[i].x) * 4000.0f, 0, 1000);
        }
        if (!g_bloom[i].on) continue;
        float u = FireClamp((float)(now - g_bloom[i].t0) / 1000.0f / BLOOM_OPEN_S, 0, 1);
        int fr = (int)(u * (frames - 1) + 0.5f);
        float sc = g_bloom[i].scale * (0.55f + 0.45f * u), h = cell * sc / 2160.0f, w = h * k;
        float cx = g_bloom[i].x, cy = g_bloom[i].y;
        float x[4] = { cx - w / 2, cx + w / 2, cx - w / 2, cx + w / 2 }, y[4] = { cy - h / 2, cy - h / 2, cy + h / 2, cy + h / 2 };
        float u0 = fr * cell / 1024.0f, u1 = (fr + 1) * cell / 1024.0f, v0 = g_bloom[i].type * cell / 1024.0f, v1 = v0 + cell / 1024.0f;
        float tu[4] = { u0, u1, u0, u1 }, tv[4] = { v1, v1, v0, v0 };
        QuadPut(g_bmPos, g_bmUV, g_bmCol, g_bmQuads++, x, y, tu, tv, (unsigned)(FireClamp(u * 3, 0, 1) * 255.0f + 0.5f) << 24 | 0xFFFFFFu);
    }
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
    void *tex = ((OvLoad)g_barLoader)(path, status, flags, 0, 0);
    static int logged;
    if (logged++ < 24) {                       /* 4 Oct: labels drawn as white boxes on a session's first screen */
        char m[260];
        const char *n = path;
        for (const char *q = path; *q; q++) if (*q == '\\') n = q + 1;
        wsprintfA(m, "  overlay load %s: handle %08lX, status %08lX %08lX %08lX, at %lu ms", n, (unsigned long)(ULONG_PTR)tex,
                  (unsigned long)status[0], (unsigned long)status[1], (unsigned long)status[2], (unsigned long)GetTickCount());
        Log(m);
    }
    return tex;
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

/* From RevealStub, on the game's thread, before the fill's own setup: the overlay's quads, the sea under the names.
 * While the view's thread moves the view, it builds and draws the overlay itself (ViewOverlayBuild), so this frame, hidden
 * under the view, only reads the class (the character list is the game's thread's alone) and builds nothing: the two
 * never build the same layer at once. Otherwise all of it, under g_stepLock, so the view cannot take over halfway. */
void WINAPI OverlayDraw(void) __attribute__((used));
static void OverlayDrawAll(void);
void WINAPI OverlayDraw(void)
{
    if (!g_ovOn || !g_mapShown) return;
    if (!g_viewSteps) {                                            /* (the view, once it moves them, keeps them) */
        int held = StepLock();
        if (!g_viewSteps) { OverlayDrawAll(); StepUnlock(held); return; }
        StepUnlock(held);
    }
    ClassStep();
}
static void OverlayDrawAll(void)
{
    DWORD now = GetTickCount();
    float t = (float)(now % 3600000u) / 1000.0f;
    static DWORD last;
    float dt = last ? (float)(now - last) / 1000.0f : 0;
    last = now;
    if (g_lblOn && !g_lblTex && !g_lblTried) { g_lblTried = 1; g_lblTex = OverlayLoad(LABELS_PATH); }   /* the biggest: first */
    if (g_seaOn && !g_seaTex && !g_seaTried) { g_seaTried = 1; g_seaTex = OverlayLoad(SEA_PATH); }
    if (g_marksOn && !g_marksTex && !g_marksTried) { g_marksTried = 1; g_marksTex = OverlayLoad(MARKS_PATH); }
    if (g_wavesOn && !g_wavesTex && !g_wavesTried) { g_wavesTried = 1; g_wavesTex = OverlayLoad(WAVES_PATH); }
    if (g_flowOn && !g_flowTex && !g_flowTried) { g_flowTried = 1; g_flowTex = OverlayLoad(FLOW_PATH); }
    if (g_wavesTex) {                                              /* the waves, first on the sea */
        WaveBuild(t);
        if (g_wvQuads) OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_wavesTex, 1, 0), g_wvPos, g_wvUV, g_wvCol, g_wvQuads);
    }
    if (g_marksTex) {                                              /* the Maelstrom, on the sea and under its glints */
        MaelstromBuild(t);
        if (g_mkQuads) OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_marksTex, 1, 0), g_mkPos, g_mkUV, g_mkCol, g_mkQuads);
    }
    if (g_flowTex) {                                               /* the currents, over the waves */
        FlowBuild(t, dt);
        if (g_flQuads) OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_flowTex, 1, 0), g_flPos, g_flUV, g_flCol, g_flQuads);
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
        void *dev = OV_FN(OvDevice, 0x44ACF0)(g_lblTex, 1, 0);
        static void *lastDev;
        static int devLogs;
        if (dev != lastDev && devLogs < 8) {           /* does its device copy change while the first screen shows? */
            char m[120];
            wsprintfA(m, "  labels' device copy %08lX at %lu ms", (unsigned long)(ULONG_PTR)dev, (unsigned long)now);
            Log(m);
            devLogs++;
            lastDev = dev;
        }
        OverlayStrips(dev, g_lblPos, g_lblUV, g_lblCol, g_lblQuads);
    }
    if (g_marksTex) {                                              /* the badge: over the names, never hidden by one */
        ClassStep();
        BadgeBuild(now);
        if (g_mkQuads) OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_marksTex, 1, 0), g_mkPos, g_mkUV, g_mkCol, g_mkQuads);
    }
    if (g_archOn) {                                                /* the class's banner, over the badge, under the logo */
        unsigned c = (unsigned)g_class;
        char path[80];
        if (c > 0 && c < 12 && g_archHas[c] && !g_archTex[c] && !g_archTried[c] && ArchPath(c, path)) {
            g_archTried[c] = 1;
            lstrcatA(path, ".blp");
            g_archTex[c] = OverlayLoad(path);
        }
        if (c > 0 && c < 12 && g_archTex[c]) {
            ArchBuild();
            OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_archTex[c], 1, 0), g_archPos, g_archUV, g_archCol, 1);
        }
    }
    if (g_logoOn && !g_logoTex && !g_logoTried) { g_logoTried = 1; g_logoTex = OverlayLoad(LOGO_PATH); }
    if (g_logoTex) {                                               /* over everything else */
        LogoBuild();
        OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_logoTex, 1, 0), g_logoPos, g_logoUV, g_logoCol, 1);
    }
    LONG garland = g_barGarland;
    if (garland > 0 && garland <= g_bars.families) {
        /* a garland draws nothing ahead of its front: the map shows there until it grows over it (his note, 4 Oct
         * evening: "There's a black bar. That should not be black. That should be the map"); its backing in the pack
         * still marks the family a garland */
    } else if (g_seaTex) {                                         /* last: the log's black, under the bar */
        BarBlackBuild();
        OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(g_seaTex, 1, 0), g_blackPos, g_blackUV, g_blackCol, 1);
    }
}
#endif

#if defined(__i386__)
/* From RevealStub at the border's entry, on the game's thread: over the garland's leaves and under the border, its
 * blooms opening and its wisps, at the fill's front as RevealFill drew it this frame (g_fillShown). */
void WINAPI OverlayAfterFill(void) __attribute__((used));
static void OverlayAfterFillAll(int f);
void WINAPI OverlayAfterFill(void)
{
    LONG garland = g_barGarland;
    if (!g_ovOn || !g_mapShown || garland <= 0 || garland > g_bars.families || g_viewSteps) return;   /* (the view draws
                                                                                         * the blade: SaberBuild) */
    int held = StepLock();
    if (!g_viewSteps) OverlayAfterFillAll((int)garland - 1);
    StepUnlock(held);
}
static void OverlayAfterFillAll(int f)
{
    static void *bloomsTex[BAR_FAMILIES], *wispsTex[BAR_FAMILIES];
    static int bloomsTried[BAR_FAMILIES], wispsTried[BAR_FAMILIES];
    static DWORD last;
    DWORD now = GetTickCount();
    float t = (float)(now % 3600000u) / 1000.0f, dt = last ? (float)(now - last) / 1000.0f : 0;
    last = now;
    char path[96];
    if (g_bloomTab[f].types && !bloomsTex[f] && !bloomsTried[f]) {
        bloomsTried[f] = 1;
        lstrcpyA(path, g_bars.family[f].blooms); lstrcatA(path, ".blp");
        bloomsTex[f] = OverlayLoad(path);
    }
    if (bloomsTex[f]) {
        BloomsBuild(f, g_fillShown, now, g_zoomScreens);
        if (g_bmQuads) OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(bloomsTex[f], 1, 0), g_bmPos, g_bmUV, g_bmCol, g_bmQuads);
    }
    if (g_wispTab[f].n && !wispsTex[f] && !wispsTried[f]) {
        wispsTried[f] = 1;
        lstrcpyA(path, g_bars.family[f].wisps); lstrcatA(path, ".blp");
        wispsTex[f] = OverlayLoad(path);
    }
    if (wispsTex[f]) {
        WispsBuild(f, g_fillShown, t, dt);
        if (g_wsQuads) OverlayStrips(OV_FN(OvDevice, 0x44ACF0)(wispsTex[f], 1, 0), g_wsPos, g_wsUV, g_wsCol, g_wsQuads);
    }
}
#endif

/* At start, with the world map on: the overlay, when its calls are where they should be and its picture is in the pack. */
static void OverlayStart(void)
{
    g_ovOn = g_seaOn = g_lblOn = g_logoOn = g_marksOn = g_glowOn = g_wavesOn = g_flowOn = 0;
    g_archOn = 0;
    if (!g_map.ok || !g_barLoader) return;
    {
        char path[80], list[12 * 9] = "";
        for (unsigned c = 1; c < 12; c++) {
            g_archHas[c] = ArchPath(c, path) && BarHasTexture(path);
            if (g_archHas[c]) { if (list[0]) lstrcatA(list, ", "); lstrcatA(list, kClassName[c]); g_archOn = 1; }
        }
        if (g_archOn) {
            char m[180];
            wsprintfA(m, "banner: the class banners in the pack: %s (the rest have none)", list);
            Log(m);
        }
    }
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
    g_wavesOn = g_seaOn && BarHasTexture("Interface\\Glues\\LoadingScreens\\CinderWaves");
    if (g_seaOn && BarHasTexture("Interface\\Glues\\LoadingScreens\\CinderFlow")) {
        unsigned char *data = NULL;
        unsigned len = 0;
        if (mpq_read(&kIoSmall, "patch-~.MPQ", "CinderLoad\\flow.bin", &data, &len) == MPQ_OK && data)
            g_flowOn = FlowParse(data, len);
        if (data) HeapFree(GetProcessHeap(), 0, data);
    }
    if (!g_seaOn && !g_lblOn && !g_logoOn && !g_marksOn && !g_glowOn) return;
    if (!OverlayCallsHold((const unsigned char *)GetModuleHandleA(NULL))) {
        g_seaOn = g_lblOn = g_logoOn = g_marksOn = g_glowOn = g_wavesOn = g_flowOn = 0;
        Log("overlay off: the calls it draws with are not where they are in the client it was made for");
        return;
    }
    for (int i = 0; i < LBL_BATCH * 6; i++) g_lblIndex[i] = (unsigned short)i;
    g_ovOn = 1;
    char m[300];
    wsprintfA(m, "overlay on:%s%s%s%s%s%s%s%s%s, drawn over the world map and under the bar (in memory only)",
              g_seaOn ? (g_wavesOn ? (g_flowOn ? " waves, currents and glints on the open sea" : " waves and glints on the open sea")
                                   : " glints on the open sea") : "", g_seaOn && g_marksOn ? "," : "",
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
/* The fill as a strip for a bar from L to L + full, B to T (bottom up), at z, its front the bar as shown (0..1), at t
 * seconds: columns from the left edge to the front, closer together near it, each a top and a bottom corner into verts
 * (x y z), uv and colours (grey levels, alpha on top). The fire flows and cools behind its front, which fades in over
 * REVEAL_FEATHER; a garland's leaves and flowers are laid once and never cool. Its corners (two a column). The game's
 * frame makes it (RevealFill); the view draws the blade instead (SaberBuild). */
static int FillStrip(float L, float B, float T, float z, float full, float shown, float t, int garland, float *verts,
                     float *uvs, unsigned *colors)
{
    float R = L + shown * full;                                     /* the front: the bar as shown */
    float flow = garland ? 0 : t * FILL_FLOW;
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
        float *top = verts + 6 * k, *bottom = top + 3;
        top[0] = xs[k]; top[1] = T; top[2] = z;
        bottom[0] = xs[k]; bottom[1] = B; bottom[2] = z;
        float frac = full > 0 ? (xs[k] - L) / full : 0;            /* where along the whole bar, 0..1 */
        float u = garland ? frac : flow + 0.5f * frac;
        uvs[4 * k] = u; uvs[4 * k + 1] = 0;
        uvs[4 * k + 2] = u; uvs[4 * k + 3] = 1;
        float d = full > 0 ? (R - xs[k]) / full : 0;                /* how far behind the front */
        float a = d >= REVEAL_FEATHER ? 1.0f : d / REVEAL_FEATHER;
        unsigned grey = garland ? 255u : (unsigned)(FillHeat(d, t, k) * 255.0f + 0.5f), alpha = (unsigned)(a * 255.0f + 0.5f);
        colors[2 * k] = colors[2 * k + 1] = alpha << 24 | grey << 16 | grey << 8 | grey;
    }
    return 2 * n;
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
    float shown = GameStep(p, GetTickCount());
    g_fillShown = FireClamp(shown, 0, 1);                           /* the garland's front too (OverlayAfterFill) */
    float L = v[0], B = v[1], z = v[2], T = v[7];
    if (p > 0.02f && v[3] > L) g_fillFull = (v[3] - L) / p;         /* the bar's whole width, in the same units */
    float full = g_fillFull > 0 ? g_fillFull : 1.0f;
    int n2 = FillStrip(L, B, T, z, full, shown, (float)(GetTickCount() % 3600000u) / 1000.0f, g_barGarland != 0, g_fillVerts,
                       g_fillUV, g_fillColor);
    for (int k = 0; k < n2; k++) g_fillIndex[k] = (unsigned short)k;
    g_fillCount = n2;
}

#if defined(__i386__)
/* The vertex setup: ecx = corners, edx = their positions, and eleven arguments above the return address (at entry:
 * colours at [esp+16], their stride at [esp+20], texture coordinates at [esp+32]). For the fill entry (esi at the
 * table's start) RevealFill makes the six corners, and ecx, edx and those three arguments become ours. */
__attribute__((naked, used)) static void RevealStub(void)
{
    __asm__("cmpl $0x7FFD34, %esi\n\t"
            "jne 2f\n\t"
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
            "movl $_g_fillUV, 32(%esp)\n\t"
            "jmp 1f\n"
            "2:\n\t"
            "cmpl $0x7FFD4C, %esi\n\t"          /* the border's entry: over the fill, under the border */
            "jne 1f\n\t"
            "pushl %eax\n\t"
            "pushl %ecx\n\t"
            "pushl %edx\n\t"
            "call _OverlayAfterFill@0\n\t"
            "popl %edx\n\t"
            "popl %ecx\n\t"
            "popl %eax\n"
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

/* ---- the view: CinderLoad's own window over the game while a world map screen shows (4 Oct 2026, option 1) ----
 * His ask (4 Oct): "I don't want the stutter of other add-ons and stuff loading to cause the map to stutter." The game
 * draws a loading frame only when a phase reports progress (0x406900, from the three setters at 0x407F70/A0/D0), so
 * while a big add-on file runs or the world finishes, nothing the game draws can move. The view is a renderer of our
 * own: for each world map screen it opens a borderless window over the game window's whole client area, owned by it (so
 * it stays above the game only, and goes when it is minimised; never topmost over his other windows), that never takes
 * the focus or a click, drawn with OpenGL (never Direct3D: see below) on its own thread at the screen's refresh. While it
 * shows, its thread moves the bar and the zoom (GameStep, above), so the game's stalls never stop them. The game's own
 * frame goes on being drawn under it, the fallback: any failure is logged once, the view stays off for the session, and
 * the game's frames are what shows, as before. As the screen goes, the view fades out over VIEW_FADE_MS on its window's
 * own alpha, the world coming up through it.
 * The proof (4 Oct, 22:28, his game): a band of it drew 1,328 frames in 9 s, the longest gap 110 ms, while the game's map
 * froze 1.74 s. Slice 1 (this): the world map, zoomed, over the whole window, and a plain bar along its bottom; the
 * overlays, the fire and the ending's blur follow. Its picture is the pack's own CinderMap.blp, read with our MPQ reader
 * as the game starts, given to OpenGL as the DXT blocks it is made of (GL_EXT_texture_compression_s3tc), else decoded
 * here from no wider than VIEW_DECODE_MAX. Only while Data\CinderLoad\view.txt exists: delete it to turn this off. */
#define VIEW_FADE_MS    500     /* the view fades out over this as the screen goes */
#define VIEW_DECODE_MAX 2048    /* a driver without DXT: the picture decoded from its first level no wider than this */
#define BLP_LEVELS      16
typedef struct { unsigned w, h; int dxt, alpha, levels; unsigned off[BLP_LEVELS], len[BLP_LEVELS]; } BlpInfo;
#define BLP_PALETTE 148                                         /* a palette picture's 256 colours, B G R x each */

/* A BLP2 of DXT1 or DXT5 blocks (the pack's pictures, as tools/blp.py writes them) or of palette indices with 0, 1, 4 or
 * 8 bits of alpha after them (dxt 0: the fire bars, as tools/blp_pal.py writes them), checked against its length: 1 with
 * where each level lies, from the whole picture down, as far as the file holds them whole. */
static int BlpParse(const unsigned char *d, unsigned n, BlpInfo *b)
{
    if (n < 1172 || d[0] != 'B' || d[1] != 'L' || d[2] != 'P' || d[3] != '2' || Rd32(d + 4) != 1) return 0;
    if (d[8] == 1 && (d[9] == 0 || d[9] == 1 || d[9] == 4 || d[9] == 8)) b->dxt = 0;   /* palette indices */
    else if (d[8] == 2 && d[10] == 0 && d[9] <= 1) b->dxt = 1;   /* alpha encoding 0: DXT1 (no alpha, or one bit) */
    else if (d[8] == 2 && d[10] == 7 && d[9] == 8) b->dxt = 5;   /* 7: DXT5 */
    else return 0;
    b->alpha = d[9];
    b->w = Rd32(d + 12); b->h = Rd32(d + 16);
    if (b->w < 1 || b->h < 1 || b->w > 8192 || b->h > 8192 || (b->dxt && (b->w < 4 || b->h < 4 || (b->w & 3) || (b->h & 3))))
        return 0;
    unsigned w = b->w, h = b->h, block = b->dxt == 1 ? 8 : 16;
    b->levels = 0;
    for (int i = 0; i < BLP_LEVELS; i++) {
        unsigned off = Rd32(d + 20 + 4 * i), len = Rd32(d + 84 + 4 * i);
        unsigned want = b->dxt ? ((w + 3) / 4) * ((h + 3) / 4) * block : w * h + (w * h * b->alpha + 7) / 8;
        if (!off || len < want || off > n || want > n - off) break;
        b->off[i] = off; b->len[i] = want; b->levels++;
        if (!d[11] || (w == 1 && h == 1)) break;                 /* no smaller copies, or the last one */
        w = w > 1 ? w / 2 : 1; h = h > 1 ? h / 2 : 1;
    }
    return b->levels > 0;
}

static void Dxt565(unsigned c, unsigned char *rgb)
{
    unsigned r = c >> 11 & 31, g = c >> 5 & 63, b = c & 31;
    rgb[0] = (unsigned char)(r << 3 | r >> 2); rgb[1] = (unsigned char)(g << 2 | g >> 4); rgb[2] = (unsigned char)(b << 3 | b >> 2);
}
/* One level of DXT1 or DXT5 blocks, w x h texels, into out as RGBA (4 bytes a texel): for a driver without DXT. */
static void DxtDecode(const unsigned char *src, unsigned w, unsigned h, int dxt, unsigned char *out)
{
    unsigned bw = (w + 3) / 4, bh = (h + 3) / 4, size = dxt == 1 ? 8 : 16;
    for (unsigned by = 0; by < bh; by++)
        for (unsigned bx = 0; bx < bw; bx++) {
            const unsigned char *k = src + (by * bw + bx) * size, *c = dxt == 1 ? k : k + 8;
            unsigned c0 = c[0] | c[1] << 8, c1 = c[2] | c[3] << 8, four = dxt == 5 || c0 > c1;
            unsigned char pal[4][4], a[8];
            Dxt565(c0, pal[0]); Dxt565(c1, pal[1]);
            for (int j = 0; j < 3; j++) {
                pal[2][j] = (unsigned char)(four ? (2 * pal[0][j] + pal[1][j]) / 3 : (pal[0][j] + pal[1][j]) / 2);
                pal[3][j] = (unsigned char)(four ? (pal[0][j] + 2 * pal[1][j]) / 3 : 0);
            }
            pal[0][3] = pal[1][3] = pal[2][3] = 255;
            pal[3][3] = four ? 255 : 0;                          /* DXT1's three colours: the fourth is see-through */
            unsigned lo = 0, hi = 0;                             /* DXT5's alpha: 3 bits a texel, 8 texels a half */
            if (dxt == 5) {
                a[0] = k[0]; a[1] = k[1];
                if (a[0] > a[1])
                    for (int i = 1; i < 7; i++) a[i + 1] = (unsigned char)(((7 - i) * a[0] + i * a[1]) / 7);
                else {
                    for (int i = 1; i < 5; i++) a[i + 1] = (unsigned char)(((5 - i) * a[0] + i * a[1]) / 5);
                    a[6] = 0; a[7] = 255;
                }
                lo = k[2] | k[3] << 8 | (unsigned)k[4] << 16; hi = k[5] | k[6] << 8 | (unsigned)k[7] << 16;
            }
            for (unsigned y = 0; y < 4; y++)
                for (unsigned x = 0; x < 4; x++) {
                    unsigned tx = bx * 4 + x, ty = by * 4 + y, i = 4 * y + x;
                    if (tx >= w || ty >= h) continue;
                    unsigned char *o = out + (ty * w + tx) * 4;
                    const unsigned char *p = pal[c[4 + y] >> (2 * x) & 3];
                    o[0] = p[0]; o[1] = p[1]; o[2] = p[2];
                    o[3] = dxt == 5 ? a[(i < 8 ? lo >> (3 * i) : hi >> (3 * (i - 8))) & 7] : p[3];
                }
        }
}

/* Level i of a palette picture (file d, as BlpParse read it), w x h texels, into out as RGBA (4 bytes a texel). */
static void BlpPalDecode(const unsigned char *d, const BlpInfo *b, int i, unsigned w, unsigned h, unsigned char *out)
{
    const unsigned char *ix = d + b->off[i], *al = ix + w * h;
    for (unsigned p = 0; p < w * h; p++) {
        const unsigned char *c = d + BLP_PALETTE + 4 * ix[p];
        unsigned a = b->alpha == 8 ? al[p] : b->alpha == 4 ? (al[p >> 1] >> ((p & 1) * 4) & 15) * 17u
                   : b->alpha == 1 ? ((al[p >> 3] >> (p & 7)) & 1) * 255u : 255u;
        out[4 * p] = c[2]; out[4 * p + 1] = c[1]; out[4 * p + 2] = c[0]; out[4 * p + 3] = (unsigned char)a;
    }
}

/* name is one of list's space-separated words (OpenGL's extensions). */
static int TextHas(const char *list, const char *name)
{
    int n = lstrlenA(name);
    for (const char *p = list; *p;) {
        while (*p == ' ') p++;
        const char *e = p;
        while (*e && *e != ' ') e++;
        int i = 0;
        if (e - p == n) while (i < n && p[i] == name[i]) i++;
        if (n && i == n) return 1;
        p = e;
    }
    return 0;
}

/* A frame of the view, as the quads it draws, in batches by picture (the renderer's seam: the tests read the list):
 * pixels, y down; each corner's place on its picture and its colour (RGBA). Made where the bar and the zoom move (under
 * g_stepLock), drawn after it; kept for the fade. Pictures by slot: the world map, the overlay's sheets, the blade's
 * soft spark (made here, not read); g_viewTex holds OpenGL's name for each (0 while it has none). A batch is drawn
 * over what is under it, or added to it (add: light, the blade's glow). */
enum { VT_MAP, VT_SEA, VT_MARKS, VT_WAVES, VT_FLOW, VT_GLOW, VT_LABELS, VT_LOGO, VT_SPARK, VT_COUNT };
static unsigned g_viewTex[VT_COUNT];
typedef struct { float x, y, u, v; unsigned char c[4]; } ViewVert;
#define SABER_COL_PX   6          /* the blade's columns: one every this many pixels across the window */
#define SABER_COLS_MAX 1024       /* and at the most this many */
#define SABER_SPARKS   14         /* sparks thrown back off its tip */
#define SABER_QUADS    (8 * SABER_COLS_MAX + 1 + 2 + SABER_SPARKS + 2 + 1 + 72)   /* (the last its housing's: HOUSE_QUADS) */
#define VIEW_QUADS_MAX (2 + WAVE_QUADS + FLOW_DROP_Q + LBL_FRAME + GLINTS + 6 * MARKS_FRAME + SABER_QUADS)
#define VIEW_BATCHES   32
static ViewVert g_vv[VIEW_QUADS_MAX * 4];
static struct { int tex, add, first, count; } g_vb[VIEW_BATCHES];   /* tex: a VT_ slot, or -1 for plain colour */
static int g_vbN, g_vvN, g_vbFull;
static void ViewListStart(void) { g_vbN = g_vvN = g_vbFull = 0; }

/* Room for one more quad of picture tex, added (add 1) or drawn over: its four corners, in the last batch when that is
 * the same picture's the same way; NULL once the list is full (that quad and every one after it left out, so nothing is
 * drawn out of its order). */
static ViewVert *ViewQuadMode(int tex, int add)
{
    if (g_vbFull || g_vvN + 4 > VIEW_QUADS_MAX * 4) return g_vbFull = 1, (ViewVert *)NULL;
    if (!g_vbN || g_vb[g_vbN - 1].tex != tex || g_vb[g_vbN - 1].add != add) {
        if (g_vbN >= VIEW_BATCHES) return g_vbFull = 1, (ViewVert *)NULL;
        g_vb[g_vbN].tex = tex; g_vb[g_vbN].add = add; g_vb[g_vbN].first = g_vvN; g_vb[g_vbN].count = 0;
        g_vbN++;
    }
    ViewVert *v = &g_vv[g_vvN];
    g_vvN += 4;
    g_vb[g_vbN - 1].count += 4;
    return v;
}
static ViewVert *ViewQuadNew(int tex) { return ViewQuadMode(tex, 0); }
static void ViewVertSet(ViewVert *v, float x, float y, float u, float t, DWORD argb)
{
    v->x = x; v->y = y; v->u = u; v->v = t;
    v->c[0] = (unsigned char)(argb >> 16); v->c[1] = (unsigned char)(argb >> 8); v->c[2] = (unsigned char)argb;
    v->c[3] = (unsigned char)(argb >> 24);
}
/* A rectangle, x0, y0 its top left (pixels), u0, v0 there on its picture. */
static void ViewRect(int tex, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, DWORD c)
{
    ViewVert *q = ViewQuadNew(tex);
    if (!q) return;
    ViewVertSet(q, x0, y0, u0, v0, c); ViewVertSet(q + 1, x1, y0, u1, v0, c);
    ViewVertSet(q + 2, x1, y1, u1, v1, c); ViewVertSet(q + 3, x0, y1, u0, v1, c);
}
/* quads of one of the overlay's sets (QuadPut's strips: screen fractions up from the bottom left; each quad's corners
 * bottom left, bottom right, top left, top right) into the list on picture tex, for a W x H window. */
static void ViewStrips(int tex, const float *pos, const float *uv, const unsigned *col, int quads, float W, float H)
{
    static const int order[4] = { 0, 1, 3, 2 };                 /* round the quad: bottom left, bottom right, top right */
    for (int q = 0; q < quads; q++) {
        int in = q % LBL_BATCH, s = (q / LBL_BATCH) * (LBL_BATCH * 6 - 2) + (in ? in * 6 : 0);
        ViewVert *v = ViewQuadNew(tex);
        if (!v) return;
        for (int k = 0; k < 4; k++) {
            int i = s + order[k];
            ViewVertSet(v + k, pos[i * 3] * W, (1 - pos[i * 3 + 1]) * H, uv[i * 2], uv[i * 2 + 1], col[i]);
        }
    }
}

/* The overlay's layers over the map (slice 2, 5 Oct 2026), in the game's own order (OverlayDraw), each made by its own
 * builder and copied into the list with its sheet. The view's thread, under g_stepLock: while it moves the view, the
 * game's frame builds none of them (OverlayDraw stands down and only reads the class, which only the game's thread
 * may). A layer whose sheet OpenGL does not have is left out. No class banner across the top, no black band along the
 * bottom (his call, 6 Oct 2026: the map gets the screen); the logo is the blade's, on it (below). now: the view's clock. */
static DWORD g_ovViewAt;
static void ViewOverlayBuild(float W, float H, DWORD now)
{
    if (!g_ovOn || !g_mapShown) return;
    float t = (float)(now % 3600000u) / 1000.0f, dt = g_ovViewAt ? (float)(LONG)(now - g_ovViewAt) / 1000.0f : 0;
    g_ovViewAt = now;
    if (dt < 0) dt = 0;
    if (dt > 0.25f) dt = 0.25f;                                  /* (a new screen, or a pause: no leap of the currents) */
    if (g_wavesOn && g_viewTex[VT_WAVES]) { WaveBuild(t); ViewStrips(VT_WAVES, g_wvPos, g_wvUV, g_wvCol, g_wvQuads, W, H); }
    if (g_marksOn && g_viewTex[VT_MARKS]) { MaelstromBuild(t); ViewStrips(VT_MARKS, g_mkPos, g_mkUV, g_mkCol, g_mkQuads, W, H); }
    if (g_flowOn && g_viewTex[VT_FLOW]) { FlowBuild(t, dt); ViewStrips(VT_FLOW, g_flPos, g_flUV, g_flCol, g_flQuads, W, H); }
    if (g_seaOn && g_viewTex[VT_SEA]) { GlintsBuild(t); ViewStrips(VT_SEA, g_glPos, g_glUV, g_glCol, g_glQuads, W, H); }
    if (g_marksOn && g_viewTex[VT_MARKS]) { RippleBuild(t, now); ViewStrips(VT_MARKS, g_mkPos, g_mkUV, g_mkCol, g_mkQuads, W, H); }
    if (g_glowOn && g_viewTex[VT_GLOW]) {                        /* the arrival's subzone, or the round glow in its place */
        GlowBuild(now);
        if (g_glowQuads) ViewStrips(VT_GLOW, g_glowPos, g_glowUV, g_glowCol, 1, W, H);
        if (g_mkQuads && g_marksOn && g_viewTex[VT_MARKS]) ViewStrips(VT_MARKS, g_mkPos, g_mkUV, g_mkCol, g_mkQuads, W, H);
    }
    if (g_lblOn && g_viewTex[VT_LABELS]) { LabelsBuild(); ViewStrips(VT_LABELS, g_lblPos, g_lblUV, g_lblCol, g_lblQuads, W, H); }
    if (g_marksOn && g_viewTex[VT_MARKS]) { BadgeBuild(now); ViewStrips(VT_MARKS, g_mkPos, g_mkUV, g_mkCol, g_mkQuads, W, H); }
}

/* ---- the blade (6 Oct 2026, his design: "one really nice bar") ----
 * One loading bar for every class, the same size for all, only its colour the class's: edge to edge along the bottom of
 * the screen, unstable like the Darksaber (his reference, The Mandalorian): a SOLID core of the class's light (his call,
 * 7 Oct: "solid fill not hollow"), brightest along its centre, whose edges crackle, a glow that flickers and throws
 * flares, a hot tip with sparks coming off it as it runs, and the way still to go a faint dark line. The RavenCraft logo sits on it at the centre, its halfway mark, and lights in the
 * class's colour as the tip reaches it. Drawn by the view only (the game's own frame under it keeps the pack's bar: the
 * fallback). Plain quads, a column every SABER_COL_PX pixels with their corners shared (the edges run on unbroken), and
 * the soft spark (VT_SPARK) for the tip, the sparks and the logo's light; the light is added, the core drawn over. Sizes
 * are of the window's height, so the blade is the same on every shape of screen. */
#define SABER_CY     0.05f         /* its centre line, up from the bottom */
#define SABER_CORE   0.012f        /* its core, top to bottom */
#define SABER_GLOW   0.024f        /* its glow beyond the core's edge, as it usually reaches (a flare goes further) */
#define SABER_LOGO_H 0.085f        /* the logo on it, tall */
#define SABER_TRACK  0x5A000000u   /* the way still to go: a faint dark line, a little thinner than the core */
/* The classes' colours, 0xRRGGBB: the game's own (Blizzard's), the shaman's the TBC blue RavenCraft uses (his call,
 * 5 Oct); a character whose class the badge does not know gets CinderLoad's ember. */
static const unsigned kClassColour[12] = { 0xFF8A2A, 0xC79C6E, 0xF58CBA, 0xABD473, 0xFFF569, 0xFFFFFF, 0xFF8A2A,
                                           0x0070DE, 0x69CCF0, 0x9482C9, 0xFF8A2A, 0xFF7D0A };
static DWORD SaberArgb(unsigned rgb, float a) { return (DWORD)(int)(FireClamp(a, 0, 1) * 255 + 0.5f) << 24 | (rgb & 0xFFFFFFu); }
static unsigned SaberMix(unsigned rgb, unsigned to, float k)            /* rgb k of the way to to */
{
    unsigned out = 0;
    for (int s = 0; s < 24; s += 8) {
        float a = (float)(rgb >> s & 255), b = (float)(to >> s & 255);
        out |= (unsigned)(int)(a + (b - a) * k + 0.5f) << s;
    }
    return out;
}
/* A light's colour from a class's: the same hue, richer (its saturation 45% of the way to whole) and as bright as it
 * goes. Blizzard's class colours are pale for text on dark panels; added as light they wash to white (a warlock's
 * purple came out lavender). White stays white. */
static unsigned SaberVivid(unsigned rgb)
{
    float c[3], hi = 0, lo = 255;
    for (int s = 0; s < 3; s++) {
        c[s] = (float)(rgb >> (16 - 8 * s) & 255);
        if (c[s] > hi) hi = c[s];
        if (c[s] < lo) lo = c[s];
    }
    if (hi <= 0) return 0xFFFFFF;
    float sat = (hi - lo) / hi, k = sat > 0 ? 1 + (1 - sat) * 0.45f / sat : 1, up = 255 / hi;
    unsigned out = 0;
    for (int s = 0; s < 3; s++) {
        float v = (hi - (hi - c[s]) * k) * up;
        out |= (unsigned)(int)(FireClamp(v, 0, 255) + 0.5f) << (16 - 8 * s);
    }
    return out;
}
/* Value noise over x and t, 0 to 1, smooth between whole steps of each; seed picks the field. */
static float SaberNoise(float x, float t, unsigned seed)
{
    float fx = MapFloor(x), ft = MapFloor(t), ax = x - fx, at = t - ft;
    ax = ax * ax * (3 - 2 * ax); at = at * at * (3 - 2 * at);
    unsigned ix = (unsigned)(int)fx + seed * 7919u, it = (unsigned)(int)ft;
    float a = SeaRand(ix, it), b = SeaRand(ix + 1, it), c = SeaRand(ix, it + 1), d = SeaRand(ix + 1, it + 1);
    float lo = a + (b - a) * ax, hi = c + (d - c) * ax;
    return lo + (hi - lo) * at;
}
/* The blade at column place k (columns from the left, fractions between), t seconds, above (side 0) or below (1), for a
 * window H tall, in pixels: how far its core reaches from the centre line (*core), how far its glow then reaches
 * (*glow), and how hot its edge burns (*hot, 0 to 1). Slow swelling, a fast edge that never sits still, a glow whose
 * reach flickers, and now and then a jagged flare. */
static void SaberEdge(float k, float t, int side, float H, float *core, float *glow, float *hot)
{
    unsigned s = (unsigned)side * 4;
    float hc = SABER_CORE / 2 * H, g = SABER_GLOW * H;
    float swell = SaberNoise(k * 0.03f, t * 0.8f, s + 1) - 0.5f;
    float jitter = SaberNoise(k * 0.45f, t * 26, s + 2) - 0.5f;
    float crackle = SaberNoise(k * 0.22f, t * 17, s + 3);
    float flare = SaberNoise(k * 0.18f, t * 11, s + 4) - 0.76f;
    flare = flare > 0 ? flare / 0.24f : 0;
    flare *= flare;
    *core = hc * (1 + 0.16f * swell) + hc * 0.32f * jitter;
    *glow = g * (0.55f + 0.5f * crackle + 0.8f * flare);
    *hot = FireClamp(0.55f + 0.45f * crackle + flare, 0, 1);
}
/* A quad round from its top left: corners x, y, each its own colour (a plain quad's u, v unused). */
static void SaberPut(int tex, int add, const float *x, const float *y, const DWORD *c, const float *uv)
{
    ViewVert *q = ViewQuadMode(tex, add);
    if (!q) return;
    static const float kWhole[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    if (!uv) uv = kWhole;
    for (int j = 0; j < 4; j++) ViewVertSet(q + j, x[j], y[j], uv[2 * j], uv[2 * j + 1], c[j]);
}
/* The soft spark, centred on x, y, rx by ry, colour argb. */
static void SaberGlowAt(float x, float y, float rx, float ry, DWORD argb)
{
    float xs[4] = { x - rx, x + rx, x + rx, x - rx }, ys[4] = { y - ry, y - ry, y + ry, y + ry };
    DWORD c[4] = { argb, argb, argb, argb };
    SaberPut(VT_SPARK, 1, xs, ys, c, NULL);
}
/* ---- the blade's housing (7 Oct 2026, his reference: a frosty glowing bar "with a metal looking border around it") ----
 * Steel, lit from above: a rail along the top of the blade and one along the bottom, edge to edge, each a dark outline,
 * a bright bevel, a body darkening downward and a dark lip at the channel the blade runs in; at each end a guard, taller
 * than the rails and tapering into them, like a hilt's; rivets along the rails and on the guards. Drawn before the
 * blade, so the blade's light falls on it. Sizes of the window's height, as the blade's. */
#define HOUSE_CHANNEL 0.0075f      /* the channel's half height: the core's and a little room */
#define HOUSE_RAIL    0.0062f      /* each rail's height */
#define HOUSE_GUARD   0.011f       /* how far a guard rises past the rails */
#define HOUSE_GUARD_W 0.018f       /* a guard's width at its full height (of the window's width), */
#define HOUSE_TAPER_W 0.022f       /* and how far it then tapers into the rails */
#define HOUSE_RIVETS  8            /* rivets along each rail */
#define HOUSE_QUADS   (1 + 2 * 4 + 2 * 2 * 2 * 2 + 2 * 2 * HOUSE_RIVETS + 2 * 4)
typedef char house_fits_the_list[HOUSE_QUADS <= 72 ? 1 : -1];
/* A quad of plain colour, its top two corners one colour and its bottom two another (x0, x1 its left and right; the top
 * and bottom edges each given at the left and the right, so a guard's slope is one quad). */
static void HouseBand(float x0, float x1, float ytl, float ytr, float ybl, float ybr, DWORD top, DWORD bottom)
{
    float x[4] = { x0, x1, x1, x0 }, y[4] = { ytl, ytr, ybr, ybl };
    DWORD c[4] = { top, top, bottom, bottom };
    SaberPut(-1, 0, x, y, c, NULL);
}
/* A rail from x0 to x1, its outer edge at yo and its channel edge at yi (above the blade: yo < yi; below: yo > yi). */
static void HouseRail(float x0, float x1, float yo, float yi, float px)
{
    float d = yi > yo ? 1.0f : -1.0f, h = (yi - yo) * d;
    float yb = yo + d * px * 1.5f, yh = yb + d * px * 2.0f, yl = yi - d * px * 2.0f;
    if (h < px * 6) return;
    if (d > 0) {                                                    /* the top rail: its bevel catches the light */
        HouseBand(x0, x1, yo, yo, yb, yb, 0xFF0D1014u, 0xFF0D1014u);
        HouseBand(x0, x1, yb, yb, yh, yh, 0xFFD8E2EAu, 0xFFA9B4BFu);
        HouseBand(x0, x1, yh, yh, yl, yl, 0xFF7D8894u, 0xFF3E4650u);
        HouseBand(x0, x1, yl, yl, yi, yi, 0xFF14181Du, 0xFF14181Du);
    } else {                                                        /* the bottom rail: in the blade's shadow above */
        HouseBand(x0, x1, yi, yi, yl, yl, 0xFF14181Du, 0xFF14181Du);
        HouseBand(x0, x1, yl, yl, yh, yh, 0xFF5A646Fu, 0xFF2D333Au);
        HouseBand(x0, x1, yh, yh, yb, yb, 0xFF9AA5B0u, 0xFF6E7884u);
        HouseBand(x0, x1, yb, yb, yo, yo, 0xFF0D1014u, 0xFF0D1014u);
    }
}
/* A quad of four corners each its own colour (round it from the top left). */
static void HousePoly(const float *x, const float *y, DWORD a, DWORD b, DWORD c, DWORD d)
{
    DWORD k[4] = { a, b, c, d };
    SaberPut(-1, 0, x, y, k, NULL);
}
/* A guard at one end, side -1 the left (from the window's edge inward) or 1 the right: wings swept up and down from the
 * rails to points past them, like a hilt's crossguard seen from the side, steel lit from above, a dark outline round it.
 * Its four corners at a time: the window's edge, the wing's point, where it meets the rail, the centre line. */
static void HouseGuard(float W, float H, float cy, int side, float px)
{
    float tall = (HOUSE_CHANNEL + HOUSE_RAIL + HOUSE_GUARD) * H, rail = (HOUSE_CHANNEL + HOUSE_RAIL) * H;
    float e = side < 0 ? 0 : W, s = side < 0 ? 1.0f : -1.0f, g = HOUSE_GUARD_W * W, tp = HOUSE_TAPER_W * W;
    for (int pass = 0; pass < 2; pass++) {                          /* the outline, then the steel inside it */
        float o = pass ? px * 1.5f : 0;
        float x0 = e + s * o, xp = e + s * g * 0.35f, xr = e + s * (g + tp - o * 2);
        for (int half = 0; half < 2; half++) {                      /* the upper wing, then the lower */
            float d = half ? 1.0f : -1.0f, yEdge = cy + d * (tall * 0.6f - o), yPt = cy + d * (tall - o * 1.6f);
            float yRail = cy + d * (rail - o);
            DWORD lit = pass == 0 ? 0xFF0D1014u : half ? 0xFF4B545Eu : 0xFFC4CED8u;
            DWORD dim = pass == 0 ? 0xFF0D1014u : half ? 0xFF22282Eu : 0xFF6A7480u;
            DWORD out = half ? dim : lit, in = half ? lit : dim;       /* (lit at the top of each wing) */
            float xa[4] = { x0, xp, xp, x0 }, ya[4] = { yEdge, yPt, cy, cy };
            float xb[4] = { xp, xr, xr, xp }, yb[4] = { yPt, yRail, cy, cy };
            if (side > 0) {                                         /* (round it from the top left either way) */
                float t0 = xa[0]; xa[0] = xa[1]; xa[1] = t0; t0 = xa[2]; xa[2] = xa[3]; xa[3] = t0;
                t0 = ya[0]; ya[0] = ya[1]; ya[1] = t0; t0 = ya[2]; ya[2] = ya[3]; ya[3] = t0;
                t0 = xb[0]; xb[0] = xb[1]; xb[1] = t0; t0 = xb[2]; xb[2] = xb[3]; xb[3] = t0;
                t0 = yb[0]; yb[0] = yb[1]; yb[1] = t0; t0 = yb[2]; yb[2] = yb[3]; yb[3] = t0;
            }
            if (half) {                                             /* the lower wing: its centre line on top */
                float t1;
                t1 = ya[0]; ya[0] = ya[3]; ya[3] = t1; t1 = ya[1]; ya[1] = ya[2]; ya[2] = t1;
                t1 = yb[0]; yb[0] = yb[3]; yb[3] = t1; t1 = yb[1]; yb[1] = yb[2]; yb[2] = t1;
            }
            DWORD ca = half ? in : out, cb = half ? out : in;           /* the outer edge's colour, the centre line's */
            HousePoly(xa, ya, half ? cb : ca, half ? cb : ca, half ? ca : cb, half ? ca : cb);
            HousePoly(xb, yb, half ? cb : ca, half ? cb : ca, half ? ca : cb, half ? ca : cb);
        }
    }
}
/* A rivet at x, y: a dark round head and the light's glint on it (the spark, drawn over, not added). */
static void HouseRivet(float x, float y, float r)
{
    float xs[4] = { x - r, x + r, x + r, x - r }, ys[4] = { y - r, y - r, y + r, y + r };
    DWORD c[4] = { 0xF0262C33u, 0xF0262C33u, 0xF0262C33u, 0xF0262C33u };
    SaberPut(VT_SPARK, 0, xs, ys, c, NULL);
    float g = r * 0.45f, gx = x - r * 0.3f, gy = y - r * 0.3f;
    float xg[4] = { gx - g, gx + g, gx + g, gx - g }, yg[4] = { gy - g, gy - g, gy + g, gy + g };
    DWORD w[4] = { 0xC8E8EEF3u, 0xC8E8EEF3u, 0xC8E8EEF3u, 0xC8E8EEF3u };
    SaberPut(VT_SPARK, 0, xg, yg, w, NULL);
}
/* The housing's channel, dark, under the blade (for a W x H window, on the blade's centre line cy). */
static void HouseChannel(float W, float H, float cy)
{
    float ch = HOUSE_CHANNEL * H;
    HouseBand(0, W, cy - ch, cy - ch, cy + ch, cy + ch, 0xE6050A0Fu, 0xE6050A0Fu);
}
/* Its steel, over the blade: the light spills out past the rails; a gem of the class's light (rgb) in each guard. */
static void HouseBuild(float W, float H, float cy, unsigned rgb)
{
    float px = H / 1440.0f, ch = HOUSE_CHANNEL * H, rh = HOUSE_RAIL * H;
    HouseRail(0, W, cy - ch - rh, cy - ch, px);
    HouseRail(0, W, cy + ch + rh, cy + ch, px);
    HouseGuard(W, H, cy, -1, px);
    HouseGuard(W, H, cy, 1, px);
    if (!g_viewTex[VT_SPARK]) return;
    float r = rh * 0.32f, span = W * (1 - 2 * (HOUSE_GUARD_W + HOUSE_TAPER_W));
    for (int i = 0; i < HOUSE_RIVETS; i++) {
        float x = W * (HOUSE_GUARD_W + HOUSE_TAPER_W) + span * (i + 0.5f) / HOUSE_RIVETS;
        if (x > W * 0.44f && x < W * 0.56f) continue;                  /* (under the logo) */
        HouseRivet(x, cy - ch - rh * 0.5f, r);
        HouseRivet(x, cy + ch + rh * 0.5f, r);
    }
    float gx = HOUSE_GUARD_W * W * 0.42f, gr = (ch + rh) * 0.9f;
    HouseRivet(gx, cy, gr);                                         /* the guards' gems, set dark (lit in HouseGems) */
    HouseRivet(W - gx, cy, gr);
    (void)rgb;
}
/* The gems' light, added with the blade's (after its tip, in the same batch). */
static void HouseGems(float W, float H, float cy, unsigned rgb)
{
    if (!g_viewTex[VT_SPARK]) return;
    float gx = HOUSE_GUARD_W * W * 0.42f, gr = (HOUSE_CHANNEL + HOUSE_RAIL) * H * 0.9f;
    for (int s = 0; s < 2; s++) {
        float x = s ? W - gx : gx;
        SaberGlowAt(x, cy, gr * 0.85f, gr * 0.85f, SaberArgb(rgb, 0.95f));
        SaberGlowAt(x, cy, gr * 0.35f, gr * 0.35f, SaberArgb(0xFFFFFF, 0.8f));
    }
}

static float g_sbCore[2][SABER_COLS_MAX + 1], g_sbGlow[2][SABER_COLS_MAX + 1], g_sbHot[2][SABER_COLS_MAX + 1];
static float g_sbX[SABER_COLS_MAX + 1];
/* The blade for a W x H window, its tip at the bar as shown, at the view's clock now. */
static void SaberBuild(float W, float H, float shown, DWORD now)
{
    LONG cl = g_class;
    unsigned base = kClassColour[cl > 0 && cl < 12 ? cl : 0], rgb = SaberVivid(base);
    unsigned hotRgb = SaberMix(rgb, 0xFFFFFF, 0.4f), midRgb = SaberMix(rgb, 0xFFFFFF, 0.45f);
    float t = (float)(now % 3600000u) / 1000.0f, cy = (1 - SABER_CY) * H, hc = SABER_CORE / 2 * H, g = SABER_GLOW * H;
    float front = FireClamp(shown, 0, 1) * W, line = H / 1440.0f, round = hc * 2.5f;
    int n = (int)(W / SABER_COL_PX);
    if (n < 16) n = 16;
    if (n > SABER_COLS_MAX) n = SABER_COLS_MAX;
    float cw = W / (float)n;
    int b = 0;                                                     /* the column edges reached, the last at the tip */
    if (front > 0)
        for (; b <= n; b++) {
            float x = b < n && b * cw < front ? b * cw : front;
            float tip = (front - x) / round;                         /* the tip rounded off: sqrt-ish, no C runtime */
            tip = shown >= 1 ? 1 : FireClamp(tip, 0, 1);
            tip = 1 - (1 - tip) * (1 - tip);
            g_sbX[b] = x;
            for (int s = 0; s < 2; s++) {
                SaberEdge(x / cw, t, s, H, &g_sbCore[s][b], &g_sbGlow[s][b], &g_sbHot[s][b]);
                g_sbCore[s][b] *= tip; g_sbGlow[s][b] *= tip;
            }
            if (x >= front) { b++; break; }
        }
    HouseChannel(W, H, cy);                                         /* the dark channel the blade runs in */
    /* the glow, added: a narrow bright one and a wide faint one beyond the core's edge, above and below */
    for (int k = 0; k + 1 < b; k++)
        for (int s = 0; s < 2; s++) {
            float d = s ? 1.0f : -1.0f, x[4] = { g_sbX[k], g_sbX[k + 1], g_sbX[k + 1], g_sbX[k] };
            for (int layer = 0; layer < 2; layer++) {
                float reach = layer ? 1.0f : 0.32f;
                float a0 = layer ? 0.3f : 0.5f + 0.4f * g_sbHot[s][k], a1 = layer ? 0.3f : 0.5f + 0.4f * g_sbHot[s][k + 1];
                float e0 = cy + d * g_sbCore[s][k], e1 = cy + d * g_sbCore[s][k + 1];
                float y[4] = { e0 + d * reach * g_sbGlow[s][k], e1 + d * reach * g_sbGlow[s][k + 1], e1, e0 };
                DWORD c[4] = { SaberArgb(rgb, 0), SaberArgb(rgb, 0), SaberArgb(rgb, a1), SaberArgb(rgb, a0) };
                SaberPut(-1, 1, x, y, c, NULL);
            }
        }
    /* the way still to go, then the core over the glow's inner edge: solid, the class's light at its edges, paler along
     * its centre line (two quads a column, the top half and the bottom) */
    if (front < W) {
        float x[4] = { front, W, W, front }, y[4] = { cy - hc * 0.55f, cy - hc * 0.55f, cy + hc * 0.55f, cy + hc * 0.55f };
        DWORD c[4] = { SABER_TRACK, SABER_TRACK, SABER_TRACK, SABER_TRACK };
        SaberPut(-1, 0, x, y, c, NULL);
    }
    DWORD edgeC = SaberArgb(rgb, 0.96f), midC = SaberArgb(midRgb, 0.96f);
    for (int k = 0; k + 1 < b; k++) {
        float x[4] = { g_sbX[k], g_sbX[k + 1], g_sbX[k + 1], g_sbX[k] };
        float yt[4] = { cy - g_sbCore[0][k], cy - g_sbCore[0][k + 1], cy, cy };
        float yb[4] = { cy, cy, cy + g_sbCore[1][k + 1], cy + g_sbCore[1][k] };
        DWORD ct[4] = { edgeC, edgeC, midC, midC }, cb[4] = { midC, midC, edgeC, edgeC };
        SaberPut(-1, 0, x, yt, ct, NULL);
        SaberPut(-1, 0, x, yb, cb, NULL);
    }
    /* the hot edge, added over the core's: a thin line of near white, brighter where it burns hotter */
    for (int k = 0; k + 1 < b; k++)
        for (int s = 0; s < 2; s++) {
            float d = s ? 1.0f : -1.0f, x[4] = { g_sbX[k], g_sbX[k + 1], g_sbX[k + 1], g_sbX[k] };
            float w0 = line * (1.4f + 1.2f * g_sbHot[s][k]), w1 = line * (1.4f + 1.2f * g_sbHot[s][k + 1]);
            float e0 = cy + d * g_sbCore[s][k], e1 = cy + d * g_sbCore[s][k + 1];
            float y[4] = { e0 + d * w0, e1 + d * w1, e1 - d * w1 * 0.5f, e0 - d * w0 * 0.5f };
            DWORD c[4] = { SaberArgb(hotRgb, 0.35f * g_sbHot[s][k]), SaberArgb(hotRgb, 0.35f * g_sbHot[s][k + 1]),
                           SaberArgb(hotRgb, g_sbHot[s][k + 1]), SaberArgb(hotRgb, g_sbHot[s][k]) };
            SaberPut(-1, 1, x, y, c, NULL);
        }
    HouseBuild(W, H, cy, rgb);                                      /* the steel over it, the light spilling past */
    /* the tip, flaring and throwing sparks back as it runs (none once the blade is whole) */
    if (g_viewTex[VT_SPARK] && front > 0 && shown < 1) {
        float flick = 0.75f + 0.25f * SaberNoise(0, t * 23, 9);
        SaberGlowAt(front, cy, g * 1.1f, g * 1.3f * flick, SaberArgb(rgb, 0.85f * flick));
        SaberGlowAt(front, cy, g * 0.45f, g * 0.45f, SaberArgb(hotRgb, 0.9f));
        for (int i = 0; i < SABER_SPARKS; i++) {
            float life = 0.28f + 0.3f * SeaRand((unsigned)i, 77), ph = (t + SeaRand((unsigned)i, 11) * 5) / life;
            float age = ph - MapFloor(ph);
            unsigned gen = (unsigned)i * 31u + (unsigned)(int)MapFloor(ph);
            float dist = age * g * (1.5f + 2.5f * SeaRand(gen, 7));
            float sx = front - (0.4f + 0.8f * SeaRand(gen, 3)) * dist;
            float sy = cy + (SeaRand(gen, 5) - 0.5f) * 1.3f * dist + age * age * g * 0.4f;
            float r = g * 0.16f * (1 - age) + line;
            SaberGlowAt(sx, sy, r, r, SaberArgb(hotRgb, 0.9f * (1 - age)));
        }
    }
    HouseGems(W, H, cy, rgb);
    /* the logo on it at the centre, its halfway mark: lit in the class's colour as the tip reaches it */
    if (g_logoOn && g_logo.ok && g_viewTex[VT_LOGO]) {
        float lh = SABER_LOGO_H * H, lw = lh * g_logo.aspect;
        if (lw > LOGO_W * W) { lh *= LOGO_W * W / lw; lw = LOGO_W * W; }
        float lx = W / 2, lit = FireClamp((shown - 0.47f) / 0.06f, 0, 1);
        if (lit > 0 && g_viewTex[VT_SPARK]) {
            float flick = 0.85f + 0.15f * SaberNoise(0, t * 9, 10);
            SaberGlowAt(lx, cy, lw * 0.85f, lh * 0.9f, SaberArgb(rgb, 0.55f * lit * flick));
            SaberGlowAt(lx, cy, lw * 0.5f, lh * 0.45f, SaberArgb(hotRgb, 0.35f * lit));
        }
        unsigned grey = 210 + (unsigned)(int)(45 * lit);
        ViewRect(VT_LOGO, lx - lw / 2, cy - lh / 2, lx + lw / 2, cy + lh / 2, g_logo.tu0, g_logo.tv0, g_logo.tu1, g_logo.tv1,
                 0xFF000000u | grey << 16 | grey << 8 | grey);
    }
}

/* W x H pixels at the view's clock now: the world map as the zoom has it (g_bgUV, the game's own corners: [4], [5] its
 * top left, [2], [3] its bottom right), the overlay over it, and the blade, its tip the bar as shown. */
static void ViewBuild(float W, float H, float shown, DWORD now)
{
    ViewListStart();
    ViewRect(VT_MAP, 0, 0, W, H, g_bgUV[4], g_bgUV[5], g_bgUV[2], g_bgUV[3], 0xFFFFFFFFu);
    ViewOverlayBuild(W, H, now);
    SaberBuild(W, H, shown, now);
}

/* The view's thread, a screen up and the view not open: 1 once the game's first frame of it has begun it (seen: the
 * game's frame count while no screen was up), with *ours 1 when it is the view's to draw (a world map screen). */
static int ViewReady(LONG seen, int *ours)
{
    int held = StepLock();
    int ready = g_gameSeq != seen && !g_dispNew && !g_driftNew;
    *ours = g_screenWatch && g_driftOn && g_screenMap;
    StepUnlock(held);
    return ready;
}
/* The view's thread, each of its frames, g_stepLock held: the bar and the zoom a step on for the game's bar p at now,
 * and the frame's quads for W x H; never while the game's thread is to begin a screen (0: nothing moved, the last quads
 * stand). */
static int ViewStepLocked(float p, DWORD now, float W, float H)
{
    if (!(p >= 0)) p = 0;
    if (p > 1) p = 1;
    if (g_dispNew || g_driftNew) return 0;
    InterlockedExchange(&g_viewSteps, 1);
    now = StepNow(now);
    float s = BarStep(p, now);
    if (g_driftOn) DriftStep(s, now);
    g_viewShown = s;
    ViewBuild(W, H, s, now);
    return 1;
}
/* The same, taking the lock itself: the tests' way in (the view's thread times its wait for the lock). */
__attribute__((unused)) static int ViewStep(float p, DWORD now, float W, float H)
{
    int held = StepLock(), ok = ViewStepLocked(p, now, W, H);
    StepUnlock(held);
    return ok;
}

/* The view's own clock (his yes, 5 Oct 2026: "hide the jump"). The card is sometimes busy with a burst of the game's
 * own work as it comes out of a hitch, the same stall the game has had since 2005 on every card, and the view's frame
 * waits about a tenth of a second behind it; on the wall clock its next frame then leaps ahead to where the zoom would
 * be. Its steps run on this clock instead: never more than two and a half of its usual frames in one go, so after a
 * pause the picture goes on from where it stopped, then up to a quarter faster until it has caught the wall clock up (a
 * tenth of a second in about a third of one), never past it, so it still lands when it should. Times in ms; the usual
 * frame kept in sixteenths of one. */
#define VIEW_CLOCK_MIN   20       /* a step is at most two and a half usual frames, and never held under this (ms) */
static struct { DWORD at, real, usual16; } g_vclk;
static void ViewClockStart(DWORD real) { g_vclk.at = g_vclk.real = real; g_vclk.usual16 = 0; }
static DWORD ViewClock(DWORD real)
{
    DWORD d = real - g_vclk.real, behind = real - g_vclk.at;
    DWORD cap = g_vclk.usual16 * 5 / 32;                                     /* 2.5 usual frames */
    if (cap < VIEW_CLOCK_MIN) cap = VIEW_CLOCK_MIN;
    g_vclk.real = real;
    if ((LONG)d < 0 || (LONG)behind < 0) return g_vclk.at;                 /* (a clock that went back: hold) */
    DWORD step = d < cap ? d : cap;
    if (d < cap) g_vclk.usual16 = g_vclk.usual16 ? g_vclk.usual16 + ((LONG)(d * 16) - (LONG)g_vclk.usual16) / 8 : d * 16;
    if (d < cap && behind > step) {                    /* behind, on a usual frame: catch up, a quarter faster */
        DWORD more = behind - step, most = (step + 1) / 4;
        step += more < most ? more : most;
    }
    g_vclk.at += step;
    return g_vclk.at;
}

/* The world's first frame (4 Oct 2026, his video: the loading screen's layer clears about 0.8 s before the game shows
 * the world, and a view that faded as it cleared showed the game's last loading frame, then the world all at once): the
 * game's presents that are not a loading screen's, counted by PrePresent. The view's thread, its screen gone at goneAt
 * with seen of them then: 1 when its fade may begin, once the game has shown the world, or with no way to see that (no
 * present hook), or after VIEW_WORLD_WAIT_MS. */
#define VIEW_WORLD_WAIT_MS 3000
static volatile LONG g_worldFrames;
static int ViewFadeDue(DWORD now, DWORD goneAt, LONG seen)
{
    return !g_blurOn || g_worldFrames != seen || (LONG)(now - goneAt) >= VIEW_WORLD_WAIT_MS;
}

#ifdef __i386__
#include <d3d9.h>
/* ITS DRAWING IS OPENGL, NOT DIRECT3D (4 Oct 2026, 22:23, his first launch of the proof: a critical error). Mods such as
 * comfyfog hook Direct3D 9 by patching the device's function table, and that table is shared by every device the
 * game's d3d9.dll makes, ours included: our device's calls ran through comfyfog's hooks, and the game's next frame
 * called a null pointer inside comfyfog. OpenGL (opengl32.dll, GDI's pixel formats) is a different library that no
 * Direct3D mod touches; it is loaded by name at run time, so the DLL's own imports stay kernel32 and user32. */
#define GL_COLOR_BUFFER_BIT_     0x4000
#define GL_QUADS_                0x0007
#define GL_PROJECTION_           0x1701
#define GL_MODELVIEW_            0x1700
#define GL_DEPTH_TEST_           0x0B71
#define GL_BLEND_                0x0BE2
#define GL_TEXTURE_2D_           0x0DE1
#define GL_EXTENSIONS_           0x1F03
#define GL_MAX_TEXTURE_SIZE_     0x0D33
#define GL_UNPACK_ALIGNMENT_     0x0CF5
#define GL_TEXTURE_MAG_FILTER_   0x2800
#define GL_TEXTURE_MIN_FILTER_   0x2801
#define GL_TEXTURE_WRAP_S_       0x2802
#define GL_TEXTURE_WRAP_T_       0x2803
#define GL_TEXTURE_MAX_LEVEL_    0x813D
#define GL_CLAMP_TO_EDGE_        0x812F
#define GL_LINEAR_               0x2601
#define GL_LINEAR_MIPMAP_LINEAR_ 0x2703
#define GL_RGBA_                 0x1908
#define GL_RGBA8_                0x8058
#define GL_UNSIGNED_BYTE_        0x1401
#define GL_DXT1_RGB_             0x83F0
#define GL_DXT1_RGBA_            0x83F1
#define GL_DXT5_RGBA_            0x83F3
#define GL_VERTEX_ARRAY_         0x8074
#define GL_COLOR_ARRAY_          0x8076
#define GL_TEXTURE_COORD_ARRAY_  0x8078
#define GL_FLOAT_                0x1406
#define GL_SRC_ALPHA_            0x0302
#define GL_ONE_MINUS_SRC_ALPHA_  0x0303
#define GL_ONE_                  1
static struct {
    HMODULE lib, gdi;
    HGLRC (WINAPI *CreateContext)(HDC);
    BOOL (WINAPI *MakeCurrent)(HDC, HGLRC);
    PROC (WINAPI *GetProc)(LPCSTR);
    void (WINAPI *Viewport)(int, int, int, int);
    void (WINAPI *ClearColor)(float, float, float, float);
    void (WINAPI *Clear)(unsigned);
    void (WINAPI *MatrixMode)(unsigned);
    void (WINAPI *LoadIdentity)(void);
    void (WINAPI *Ortho)(double, double, double, double, double, double);
    void (WINAPI *Enable)(unsigned);
    void (WINAPI *Disable)(unsigned);
    void (WINAPI *EnableClientState)(unsigned);
    void (WINAPI *VertexPointer)(int, unsigned, int, const void *);
    void (WINAPI *TexCoordPointer)(int, unsigned, int, const void *);
    void (WINAPI *ColorPointer)(int, unsigned, int, const void *);
    void (WINAPI *DrawArrays)(unsigned, int, int);
    void (WINAPI *BlendFunc)(unsigned, unsigned);
    void (WINAPI *GenTextures)(int, unsigned *);
    void (WINAPI *BindTexture)(unsigned, unsigned);
    void (WINAPI *TexParameteri)(unsigned, unsigned, int);
    void (WINAPI *TexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);
    void (WINAPI *PixelStorei)(unsigned, int);
    const unsigned char *(WINAPI *GetString)(unsigned);
    void (WINAPI *GetIntegerv)(unsigned, int *);
    unsigned (WINAPI *GetError)(void);
    void (WINAPI *Finish)(void);
    void (WINAPI *CompressedTexImage2D)(unsigned, int, unsigned, int, int, int, int, const void *);   /* 1.3: by name */
    int (WINAPI *ChoosePF)(HDC, const PIXELFORMATDESCRIPTOR *);
    BOOL (WINAPI *SetPF)(HDC, int, const PIXELFORMATDESCRIPTOR *);
    BOOL (WINAPI *Swap)(HDC);
} gl;
static HINSTANCE g_inst;
static struct {
    HWND win; HDC dc; HGLRC rc; int w, h, vsync, s3tc, maxTex;
    unsigned char *blp[VT_COUNT]; unsigned len[VT_COUNT];      /* each picture's file, read as the game starts, until
                                                                 * OpenGL has it */
    DWORD since, last, gap, frames;
} g_view;

/* opengl32.dll and gdi32.dll's few calls: 1 when every one is there. */
static int GlLoad(void)
{
    if (gl.Swap) return 1;
    gl.lib = LoadLibraryA("opengl32.dll");
    gl.gdi = LoadLibraryA("gdi32.dll");
    if (!gl.lib || !gl.gdi) return 0;
#define GLP(f, m, n) (*(FARPROC *)&gl.f = GetProcAddress(m, n))
    if (!GLP(CreateContext, gl.lib, "wglCreateContext") || !GLP(MakeCurrent, gl.lib, "wglMakeCurrent") ||
        !GLP(GetProc, gl.lib, "wglGetProcAddress") || !GLP(Viewport, gl.lib, "glViewport") ||
        !GLP(ClearColor, gl.lib, "glClearColor") || !GLP(Clear, gl.lib, "glClear") ||
        !GLP(MatrixMode, gl.lib, "glMatrixMode") || !GLP(LoadIdentity, gl.lib, "glLoadIdentity") ||
        !GLP(Ortho, gl.lib, "glOrtho") || !GLP(Enable, gl.lib, "glEnable") || !GLP(Disable, gl.lib, "glDisable") ||
        !GLP(EnableClientState, gl.lib, "glEnableClientState") || !GLP(VertexPointer, gl.lib, "glVertexPointer") ||
        !GLP(TexCoordPointer, gl.lib, "glTexCoordPointer") || !GLP(ColorPointer, gl.lib, "glColorPointer") ||
        !GLP(DrawArrays, gl.lib, "glDrawArrays") || !GLP(BlendFunc, gl.lib, "glBlendFunc") ||
        !GLP(GenTextures, gl.lib, "glGenTextures") || !GLP(BindTexture, gl.lib, "glBindTexture") ||
        !GLP(TexParameteri, gl.lib, "glTexParameteri") || !GLP(TexImage2D, gl.lib, "glTexImage2D") ||
        !GLP(PixelStorei, gl.lib, "glPixelStorei") || !GLP(GetString, gl.lib, "glGetString") ||
        !GLP(GetIntegerv, gl.lib, "glGetIntegerv") || !GLP(GetError, gl.lib, "glGetError") ||
        !GLP(Finish, gl.lib, "glFinish") ||
        !GLP(ChoosePF, gl.gdi, "ChoosePixelFormat") || !GLP(SetPF, gl.gdi, "SetPixelFormat") ||
        !GLP(Swap, gl.gdi, "SwapBuffers"))
        return gl.Swap = NULL, 0;
#undef GLP
    return 1;
}
/* A call past OpenGL 1.1, by name, once a context is current: NULL when the driver has none (some say so with 1, 2, 3
 * or -1). */
static PROC GlExt(const char *name)
{
    PROC f = gl.GetProc(name);
    ULONG_PTR v = (ULONG_PTR)f;
    return v > 3 && v != (ULONG_PTR)-1 ? f : NULL;
}

/* What this OpenGL takes, once its context is current: DXT blocks as they are (GL_EXT_texture_compression_s3tc, and
 * its call by name), and its largest picture. */
static void ViewGlCaps(void)
{
    const char *ext = (const char *)gl.GetString(GL_EXTENSIONS_);
    gl.GetIntegerv(GL_MAX_TEXTURE_SIZE_, &g_view.maxTex);
    if (ext && TextHas(ext, "GL_EXT_texture_compression_s3tc")) {
        *(PROC *)&gl.CompressedTexImage2D = GlExt("glCompressedTexImage2D");
        if (!gl.CompressedTexImage2D) *(PROC *)&gl.CompressedTexImage2D = GlExt("glCompressedTexImage2DARB");
        g_view.s3tc = gl.CompressedTexImage2D != NULL;
    }
}
/* Picture slot's file, given to OpenGL: 1 when it took it (g_viewTex[slot]); the file's memory goes either way. why:
 * what failed, for the log. */
static int ViewTexUp(int slot, const char **why)
{
    BlpInfo b;
    unsigned char *d = g_view.blp[slot];
    unsigned n = g_view.len[slot];
    g_view.blp[slot] = NULL;
    int ok = 0, max = g_view.maxTex, s3tc = g_view.s3tc, first = 0;
    unsigned char *rgba = NULL;
    if (!d || !BlpParse(d, n, &b)) { *why = "it is not a BLP2 of DXT1 or DXT5 blocks whole"; goto done; }
    if (!b.dxt) s3tc = 0;                                       /* a palette picture: decoded here, whatever the driver */
    while (first < b.levels - 1 && ((int)(b.w >> first) > max || (int)(b.h >> first) > max ||
                                    (!s3tc && (b.w >> first) > VIEW_DECODE_MAX && b.dxt)))
        first++;
    unsigned w0 = b.w >> first, h0 = b.h >> first;
    if (!w0 || !h0 || (int)w0 > max || (int)h0 > max) { *why = "its OpenGL takes no picture that large"; goto done; }
    rgba = s3tc ? NULL : (unsigned char *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)w0 * h0 * 4);
    if (!s3tc && !rgba) { *why = "no memory to decode it"; goto done; }
    while (gl.GetError()) {}
    gl.GenTextures(1, &g_viewTex[slot]);
    gl.BindTexture(GL_TEXTURE_2D_, g_viewTex[slot]);
    gl.PixelStorei(GL_UNPACK_ALIGNMENT_, 1);
    unsigned fmt = b.dxt == 5 ? GL_DXT5_RGBA_ : b.alpha ? GL_DXT1_RGBA_ : GL_DXT1_RGB_;
    for (int i = first; i < b.levels; i++) {
        unsigned w = b.w >> i ? b.w >> i : 1, h = b.h >> i ? b.h >> i : 1;
        if (s3tc)
            gl.CompressedTexImage2D(GL_TEXTURE_2D_, i - first, fmt, (int)w, (int)h, 0, (int)b.len[i], d + b.off[i]);
        else {
            if (b.dxt) DxtDecode(d + b.off[i], w, h, b.dxt, rgba);
            else BlpPalDecode(d, &b, i, w, h, rgba);
            gl.TexImage2D(GL_TEXTURE_2D_, i - first, GL_RGBA8_, (int)w, (int)h, 0, GL_RGBA_, GL_UNSIGNED_BYTE_, rgba);
        }
    }
    gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAX_LEVEL_, b.levels - 1 - first);
    gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MIN_FILTER_, b.levels - first > 1 ? GL_LINEAR_MIPMAP_LINEAR_ : GL_LINEAR_);
    gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAG_FILTER_, GL_LINEAR_);
    gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_S_, GL_CLAMP_TO_EDGE_);
    gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_T_, GL_CLAMP_TO_EDGE_);
    if (gl.GetError()) { g_viewTex[slot] = 0; *why = "its OpenGL would not take it"; goto done; }
    if (slot == VT_MAP) {
        char m[200];
        wsprintfA(m, "  the view's picture: CinderMap.blp, %u x %u in %d levels, %s%s", w0, h0, b.levels - first,
                  s3tc ? "its DXT blocks as they are" : "decoded here (its OpenGL has no DXT)",
                  g_view.vsync ? ", at the screen's refresh" : ", paced by the clock (no vsync)");
        Log(m);
    }
    ok = 1;
done:
    if (rgba) HeapFree(GetProcessHeap(), 0, rgba);
    if (d) HeapFree(GetProcessHeap(), 0, d);
    return ok;
}

/* The blade's soft spark (VT_SPARK), made here: white, its alpha falling smoothly from the centre to nothing at the
 * edge, SPARK_PX square. 0 when OpenGL would not take it (the blade then has no tip, sparks or light on the logo). */
#define SPARK_PX 64
static unsigned char g_sparkPx[SPARK_PX * SPARK_PX * 4];
static void SparkMake(unsigned char *px)
{
    for (int y = 0; y < SPARK_PX; y++)
        for (int x = 0; x < SPARK_PX; x++) {
            float dx = (x + 0.5f) / (SPARK_PX / 2) - 1, dy = (y + 0.5f) / (SPARK_PX / 2) - 1, a = 1 - dx * dx - dy * dy;
            a = a > 0 ? a * a * a : 0;
            unsigned char *p = px + 4 * (y * SPARK_PX + x);
            p[0] = p[1] = p[2] = 255;
            p[3] = (unsigned char)(int)(a * 255 + 0.5f);
        }
}
static int ViewSparkUp(void)
{
    SparkMake(g_sparkPx);
    while (gl.GetError()) {}
    gl.GenTextures(1, &g_viewTex[VT_SPARK]);
    gl.BindTexture(GL_TEXTURE_2D_, g_viewTex[VT_SPARK]);
    gl.PixelStorei(GL_UNPACK_ALIGNMENT_, 1);
    gl.TexImage2D(GL_TEXTURE_2D_, 0, GL_RGBA8_, SPARK_PX, SPARK_PX, 0, GL_RGBA_, GL_UNSIGNED_BYTE_, g_sparkPx);
    gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAX_LEVEL_, 0);
    gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MIN_FILTER_, GL_LINEAR_);
    gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAG_FILTER_, GL_LINEAR_);
    gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_S_, GL_CLAMP_TO_EDGE_);
    gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_T_, GL_CLAMP_TO_EDGE_);
    if (gl.GetError()) g_viewTex[VT_SPARK] = 0;
    return g_viewTex[VT_SPARK] != 0;
}

/* The window's own alpha (a layered window's): 255 whole. 0 when the system will not do it. */
static int ViewAlpha(BYTE a)
{
    LONG ex = GetWindowLongA(g_view.win, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED)) SetWindowLongA(g_view.win, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    return SetLayeredWindowAttributes(g_view.win, 0, a, LWA_ALPHA) != 0;
}
static void ViewOpaque(void)
{
    LONG ex = GetWindowLongA(g_view.win, GWL_EXSTYLE);
    if (ex & WS_EX_LAYERED) SetWindowLongA(g_view.win, GWL_EXSTYLE, ex & ~WS_EX_LAYERED);
}

static BOOL CALLBACK ViewFind(HWND h, LPARAM lp)
{
    DWORD pid = 0;
    char cls[64];
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || h == g_view.win || !IsWindowVisible(h)) return TRUE;
    if (!GetClassNameA(h, cls, (int)sizeof cls)) return TRUE;
    cls[13] = 0;                                         /* "GxWindowClassD3d", "GxWindowClassOpenGl": 13 letters */
    if (lstrcmpiA(cls, "GxWindowClass")) return TRUE;
    *(HWND *)lp = h;
    return FALSE;
}
static LRESULT CALLBACK ViewProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (m == WM_NCHITTEST) return HTTRANSPARENT;
    return DefWindowProcA(h, m, w, l);
}
/* Our window over the game window's whole client area, its OpenGL context and the world map's picture: 1 when all are
 * there and the window shows; -1 not for this screen (the game's window minimised, say); 0 not at all. why: what
 * failed, for the log. */
static int ViewOpen(const char **why)
{
    HWND game = NULL;
    EnumWindows(ViewFind, (LPARAM)&game);
    if (!game) { *why = "the game's window was not found"; return -1; }
    RECT r;
    POINT o = { 0, 0 };
    if (!GetClientRect(game, &r) || !ClientToScreen(game, &o)) { *why = "the game window's size could not be read"; return -1; }
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w < 64 || h < 64) { *why = "the game's window is minimised or too small"; return -1; }
    static int registered;
    if (!registered) {
        WNDCLASSEXA wc;
        unsigned char *z = (unsigned char *)&wc;
        for (unsigned i = 0; i < sizeof wc; i++) z[i] = 0;
        wc.cbSize = sizeof wc; wc.lpfnWndProc = ViewProc; wc.hInstance = g_inst; wc.lpszClassName = "CinderView";
        wc.style = CS_OWNDC;                             /* OpenGL's: one device context for the window's life */
        if (!RegisterClassExA(&wc)) { *why = "its window class could not be made"; return 0; }
        registered = 1;
    }
    if (!g_view.win)
        g_view.win = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT, "CinderView", "CinderLoad",
                                     WS_POPUP, o.x, o.y, w, h, game, NULL, g_inst, NULL);
    else
        SetWindowPos(g_view.win, NULL, o.x, o.y, w, h, SWP_NOACTIVATE | SWP_NOZORDER);
    if (!g_view.win) { *why = "its window could not be made"; return 0; }
    ViewOpaque();                                        /* whole again after the last screen's fade */
    if (!g_view.rc) {                                    /* once: the window's pixel format and an OpenGL context */
        if (!GlLoad()) { *why = "opengl32.dll or gdi32.dll is missing a call it needs"; return 0; }
        g_view.dc = GetDC(g_view.win);
        PIXELFORMATDESCRIPTOR pf;
        unsigned char *z = (unsigned char *)&pf;
        for (unsigned i = 0; i < sizeof pf; i++) z[i] = 0;
        pf.nSize = sizeof pf; pf.nVersion = 1; pf.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pf.iPixelType = PFD_TYPE_RGBA; pf.cColorBits = 32; pf.iLayerType = PFD_MAIN_PLANE;
        int n = g_view.dc ? gl.ChoosePF(g_view.dc, &pf) : 0;
        if (!n || !gl.SetPF(g_view.dc, n, &pf)) { *why = "its window would not take an OpenGL pixel format"; return 0; }
        g_view.rc = gl.CreateContext(g_view.dc);
        if (!g_view.rc || !gl.MakeCurrent(g_view.dc, g_view.rc)) { g_view.rc = NULL; *why = "its OpenGL context could not be made"; return 0; }
        BOOL (WINAPI *interval)(int) = NULL;                /* at the screen's refresh, as the game's own frames */
        *(PROC *)&interval = GlExt("wglSwapIntervalEXT");
        g_view.vsync = interval && interval(1);
    }
    if (!g_viewTex[VT_MAP]) {                            /* once: the pictures, read as the game started */
        ViewGlCaps();
        if (!ViewTexUp(VT_MAP, why)) return 0;
        int sheets = 0, given = 0;
        char m[200];
        for (int i = VT_MAP + 1; i < VT_COUNT; i++) {
            if (!g_view.blp[i]) continue;
            const char *w = "";
            sheets++;
            if (ViewTexUp(i, &w)) given++;
            else { wsprintfA(m, "  the view: an overlay's sheet (slot %d) was not given to OpenGL, as %s; that layer is left out", i, w); Log(m); }
        }
        wsprintfA(m, "  the view's overlay: %d of its %d sheets given to OpenGL", given, sheets);
        Log(m);
        Log(ViewSparkUp() ? "  the view's blade: one bar for every class, in its colour; its spark given to OpenGL"
                          : "  the view's blade: its spark was not given to OpenGL; the blade has no tip, sparks or light on the logo");
    }
    g_view.w = w; g_view.h = h;
    ShowWindow(g_view.win, SW_SHOWNOACTIVATE);
    return 1;
}
static DWORD ViewNow(void);
/* The list (g_vb, g_vv), drawn and shown: 0 when it could not be (the view then goes). drawn and gpu, when given: when
 * the drawing was handed over, and when the GPU had finished it (glFinish), before the swap (the slow frames' parts,
 * below). */
static int ViewFrame(DWORD *drawn, DWORD *gpu)
{
    gl.Viewport(0, 0, g_view.w, g_view.h);
    gl.ClearColor(0, 0, 0, 1);
    gl.Clear(GL_COLOR_BUFFER_BIT_);
    gl.MatrixMode(GL_PROJECTION_); gl.LoadIdentity(); gl.Ortho(0, g_view.w, g_view.h, 0, -1, 1);   /* pixels, y down */
    gl.MatrixMode(GL_MODELVIEW_); gl.LoadIdentity();
    gl.Disable(GL_DEPTH_TEST_);
    gl.Enable(GL_BLEND_);
    gl.BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    gl.EnableClientState(GL_VERTEX_ARRAY_); gl.EnableClientState(GL_TEXTURE_COORD_ARRAY_); gl.EnableClientState(GL_COLOR_ARRAY_);
    gl.VertexPointer(2, GL_FLOAT_, (int)sizeof(ViewVert), &g_vv[0].x);
    gl.TexCoordPointer(2, GL_FLOAT_, (int)sizeof(ViewVert), &g_vv[0].u);
    gl.ColorPointer(4, GL_UNSIGNED_BYTE_, (int)sizeof(ViewVert), g_vv[0].c);
    int added = 0;
    for (int i = 0; i < g_vbN; i++) {
        int t = g_vb[i].tex;
        if (t < 0) gl.Disable(GL_TEXTURE_2D_);
        else if (!g_viewTex[t]) continue;
        else { gl.Enable(GL_TEXTURE_2D_); gl.BindTexture(GL_TEXTURE_2D_, g_viewTex[t]); }
        if (g_vb[i].add != added) {                               /* light added (the blade's glow), or drawn over */
            added = g_vb[i].add;
            gl.BlendFunc(GL_SRC_ALPHA_, added ? GL_ONE_ : GL_ONE_MINUS_SRC_ALPHA_);
        }
        gl.DrawArrays(GL_QUADS_, g_vb[i].first, g_vb[i].count);
    }
    if (added) gl.BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    if (drawn) *drawn = ViewNow();
    if (gpu) { gl.Finish(); *gpu = ViewNow(); }
    return gl.Swap(g_view.dc) != 0;
}
/* The view's slow frames, per screen (4 Oct 2026: it paused about a tenth of a second inside the game's add-on hitches,
 * and the log said only its longest gap): frames over VIEW_SLOW_MS, how many of them came while the game was drawing
 * nothing (its last frame HITCH_MS or more before), the slowest one's parts and the game's quiet then, and the most each
 * part took. Parts: its messages, the wait for g_stepLock, the step, the drawing, the GPU finishing it (glFinish), the
 * swap. (His next load, 23:50: all 106 ms of the slowest were in the swap; this splits the GPU's share out of it.) */
#define VIEW_SLOW_MS 40
#define VIEW_PARTS   6
static struct { DWORD n, quiet, worst, worstQuiet, part[VIEW_PARTS], most[VIEW_PARTS]; } g_slow;
static void ViewSlowKeep(DWORD all, const DWORD *part, DWORD quiet)
{
    for (int i = 0; i < VIEW_PARTS; i++) if (part[i] > g_slow.most[i]) g_slow.most[i] = part[i];
    if (all >= VIEW_SLOW_MS) { g_slow.n++; if (quiet >= HITCH_MS) g_slow.quiet++; }
    if (all > g_slow.worst) {
        g_slow.worst = all;
        g_slow.worstQuiet = quiet;
        for (int i = 0; i < VIEW_PARTS; i++) g_slow.part[i] = part[i];
    }
}
/* GetTickCount, between its ticks too: on Windows it moves in steps of about 16 ms, too coarse for a view drawn 144 times
 * a second; QueryPerformanceCounter fills them in, never past the next tick, so it never runs ahead of the game's clock. */
static DWORD ViewNow(void)
{
    static DWORD tick, last, perMs;
    static LARGE_INTEGER at;
    LARGE_INTEGER q;
    DWORD t = GetTickCount(), now = t;
    if (!perMs) {
        LARGE_INTEGER f;
        perMs = QueryPerformanceFrequency(&f) && !f.HighPart && f.LowPart >= 1000 ? f.LowPart / 1000 : 0xFFFFFFFFu;
    }
    if (perMs != 0xFFFFFFFFu && QueryPerformanceCounter(&q)) {
        if (t != tick) { tick = t; at = q; }
        else { DWORD d = (q.LowPart - at.LowPart) / perMs; now = t + (d < 15 ? d : 15); }
    }
    if ((LONG)(now - last) < 0) now = last;
    last = now;
    return now;
}
/* The view gone for the session: the game's thread moves the bar and the zoom again from its next frame. */
static DWORD ViewOff(const char *why)
{
    char m[200];
    int held = StepLock();
    InterlockedExchange(&g_viewSteps, 0);
    StepUnlock(held);
    if (g_view.win) ShowWindow(g_view.win, SW_HIDE);
    for (int i = 0; i < VT_COUNT; i++)
        if (g_view.blp[i]) { HeapFree(GetProcessHeap(), 0, g_view.blp[i]); g_view.blp[i] = NULL; }
    wsprintfA(m, "view off for this session: %s", why);
    Log(m);
    return 0;
}
static DWORD WINAPI ViewThread(LPVOID arg)
{
    const unsigned char *base = (const unsigned char *)arg;
    char m[400];
    int r = mpq_read(&kIo, "patch-~.MPQ", MAP_PATH, &g_view.blp[VT_MAP], &g_view.len[VT_MAP]);
    if (r != MPQ_OK) {
        wsprintfA(m, "its picture could not be read from Data\\patch-~.MPQ (%s)", mpq_error_text(r));
        return ViewOff(m);
    }
    if (g_ovOn) {                                       /* the overlay's sheets (a layer whose sheet will not read is left out) */
        static const struct { int slot, *on; const char *path; } kSheets[] = {
            { VT_SEA, &g_seaOn, SEA_PATH }, { VT_MARKS, &g_marksOn, MARKS_PATH }, { VT_WAVES, &g_wavesOn, WAVES_PATH },
            { VT_FLOW, &g_flowOn, FLOW_PATH }, { VT_GLOW, &g_glowOn, GLOW_PATH }, { VT_LABELS, &g_lblOn, LABELS_PATH },
            { VT_LOGO, &g_logoOn, LOGO_PATH } };
        for (unsigned i = 0; i < sizeof kSheets / sizeof kSheets[0]; i++)
            if (*kSheets[i].on) mpq_read(&kIo, "patch-~.MPQ", kSheets[i].path, &g_view.blp[kSheets[i].slot], &g_view.len[kSheets[i].slot]);
    }
    int showing = 0, built = 0, skip = 0, skips = 0, fadeOk = 1;
    LONG seen = g_gameSeq, worldSeen = 0;
    DWORD fadeAt = 0, waitAt = 0;
    for (;;) {
        DWORD t0 = ViewNow();
        MSG msg;
        while (g_view.win && PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        int screen = *(volatile const unsigned int *)(base + FIRE_LAYER_RVA) != 0;
        DWORD now = ViewNow();
        if (!screen) { seen = g_gameSeq; skip = 0; }
        if ((fadeAt || waitAt) && screen) { fadeAt = waitAt = 0; ShowWindow(g_view.win, SW_HIDE); }   /* a new screen */
        if (waitAt) {                                       /* the screen has gone: its last frame, until the world shows */
            if (!ViewFadeDue(now, waitAt, worldSeen)) {
                if (!ViewFrame(NULL, NULL)) return ViewOff("its OpenGL frame could not be shown");
                if (!g_view.vsync) Sleep(4);
                continue;
            }
            wsprintfA(m, "  the view: the world showed %lu ms after the screen went%s", (unsigned long)(now - waitAt),
                      g_worldFrames != worldSeen ? "" : ", or no frame of it was seen and it faded anyway");
            Log(m);
            waitAt = 0;
            fadeAt = now ? now : 1;
        }
        if (fadeAt) {                                       /* then it fades out on that frame, and hides */
            LONG t = (LONG)(now - fadeAt);
            if (t < VIEW_FADE_MS && fadeOk) {
                fadeOk = ViewAlpha((BYTE)(255 - 255 * t / VIEW_FADE_MS));
                if (!fadeOk) Log("  the view's fade: the system would not set its window's alpha, so it just goes");
                if (fadeOk && ViewFrame(NULL, NULL)) { if (!g_view.vsync) Sleep(4); continue; }
            }
            ShowWindow(g_view.win, SW_HIDE);
            fadeAt = 0;
            continue;
        }
        if (screen && !showing && !skip) {
            int ours = 0;
            if (!ViewReady(seen, &ours)) { Sleep(1); continue; }
            if (!ours) { skip = 1; continue; }              /* not a world map screen: the game's own, as ever */
            const char *why = "";
            int open = ViewOpen(&why);
            if (open < 0) {                                 /* not this screen: the game's own, as ever */
                if (skips++ < 8) { wsprintfA(m, "  the view: not this screen, as %s", why); Log(m); }
                skip = 1;
                continue;
            }
            if (!open) return ViewOff(why);
            showing = 1; built = 0;
            g_view.since = g_view.last = now; g_view.gap = 0; g_view.frames = 0;
            for (unsigned i = 0; i < sizeof g_slow; i++) ((unsigned char *)&g_slow)[i] = 0;
            ViewClockStart(now);
            continue;                                       /* (the open took a moment: a fresh clock for the first) */
        }
        if (screen && showing) {
            float p = *(volatile const float *)(base + FIRE_PROGRESS_RVA);
            DWORD part[VIEW_PARTS], t1 = ViewNow();
            int held = StepLock();
            DWORD t2 = ViewNow();
            if (ViewStepLocked(p, ViewClock(t2), (float)g_view.w, (float)g_view.h)) built = 1;
            StepUnlock(held);
            DWORD t3 = ViewNow(), t4 = t3, t5 = t3, gameAt = g_gameAt;
            LONG quiet = (LONG)(t0 - gameAt);                /* the game's last loading frame, before this one began */
            if (!built) { Sleep(1); continue; }              /* (this screen's first quads not made yet) */
            if (!ViewFrame(&t4, &t5)) return ViewOff("its OpenGL frame could not be shown");
            if (!g_view.vsync) Sleep(4);
            DWORD t = ViewNow();
            part[0] = t1 - t0; part[1] = t2 - t1; part[2] = t3 - t2; part[3] = t4 - t3; part[4] = t5 - t4; part[5] = t - t5;
            ViewSlowKeep(t - t0, part, quiet > 0 ? (DWORD)quiet : 0);
            if (t - g_view.last > g_view.gap) g_view.gap = t - g_view.last;
            g_view.last = t;
            g_view.frames++;
            continue;
        }
        if (showing) {                                      /* the screen has gone */
            int held = StepLock();
            InterlockedExchange(&g_viewSteps, 0);
            StepUnlock(held);
            showing = 0;
            waitAt = now ? now : 1;                         /* it fades once the world shows (above) */
            worldSeen = g_worldFrames;
            DWORD ms = now - g_view.since;
            wsprintfA(m, "  the view: %lu frames of its own in %lu.%lu s (%lu a second), its longest gap %lu ms",
                      (unsigned long)g_view.frames, (unsigned long)(ms / 1000), (unsigned long)(ms % 1000 / 100),
                      ms ? (unsigned long)(g_view.frames * 1000 / ms) : 0ul, (unsigned long)g_view.gap);
            Log(m);
            const DWORD *a = g_slow.part, *b = g_slow.most;
            wsprintfA(m, "  its slow frames: %lu over %d ms, %lu of them while the game drew nothing; the slowest %lu ms "
                      "(messages %lu, lock %lu, step %lu, draw %lu, gpu %lu, swap %lu), the game's last frame %lu ms before it",
                      (unsigned long)g_slow.n, VIEW_SLOW_MS, (unsigned long)g_slow.quiet, (unsigned long)g_slow.worst,
                      (unsigned long)a[0], (unsigned long)a[1], (unsigned long)a[2], (unsigned long)a[3], (unsigned long)a[4],
                      (unsigned long)a[5], (unsigned long)g_slow.worstQuiet);
            Log(m);                                         /* (two lines: Log's own line holds 400 bytes) */
            wsprintfA(m, "  its parts' most: messages %lu, lock %lu, step %lu, draw %lu, gpu %lu, swap %lu ms",
                      (unsigned long)b[0], (unsigned long)b[1], (unsigned long)b[2], (unsigned long)b[3], (unsigned long)b[4],
                      (unsigned long)b[5]);
            Log(m);
        }
        Sleep(5);
    }
}
/* At start: the view's thread, only while Data\CinderLoad\view.txt exists and the pack's world map zoom is on. */
static void ViewStart(void)
{
    char flag[MAX_PATH];
    const unsigned char *base = (const unsigned char *)GetModuleHandleA(NULL);
    if (!Join(flag, g_dir, "Data\\CinderLoad\\view.txt") || GetFileAttributesA(flag) == INVALID_FILE_ATTRIBUTES) return;
    if (!Readable(base + FIRE_LAYER_RVA, 8)) { Log("view off: the loading screen's numbers could not be read"); return; }
    if (!g_map.ok || !g_driftOn) { Log("view off: it draws the world map's zoom, and that is not on"); return; }
    InitializeCriticalSection(&g_stepLock);
    InterlockedExchange(&g_viewOn, 1);                     /* before any loading frame: the game's thread locks from now */
    HANDLE t = CreateThread(NULL, 0, ViewThread, (LPVOID)base, 0, NULL);
    if (!t) { InterlockedExchange(&g_viewOn, 0); Log("view off: its thread could not be started"); return; }
    CloseHandle(t);
    Log("view on: on world map screens, its own window over the game, drawn with OpenGL on its own thread, which moves "
        "the bar and the zoom while it shows (Data\\CinderLoad\\view.txt; delete it to turn this off)");
}
#else
static void ViewStart(void) {}
#endif

/* ---- the ending's drawing, on the game's own device (see BLUR_MS) ----
 * The present's six bytes at VA 0x5A19FD go to PresentStub, which hands the game's device object (esi) to PrePresent and
 * then does what they did. PrePresent works out from BlurStep what goes over this frame, and draws it with the frame's
 * own state saved and put back around it (a state block, the render target too). Any failure turns the ending off for
 * the session; the game's frame is never left half-drawn. The blur's textures live only from the blur to the end of
 * the world's dissolve, so they are never there when the game resets its device. */
#define BLUR_LEVELS 8          /* 1/2, 1/4, 1/8, 1/16, 1/32 down; 1/16, 1/8, 1/4 back up */
#ifdef __i386__
__attribute__((used)) void *g_presentBack;            /* VA 0x5A1A03: the present, after the six bytes */
void WINAPI PrePresent(unsigned char *gx) __attribute__((used));
__attribute__((naked, used)) static void PresentStub(void)
{
    __asm__("pushal\n\t"
            "pushl %esi\n\t"
            "call _PrePresent@4\n\t"
            "popal\n\t"
            "movl 0x38a8(%esi), %eax\n\t"
            "jmp *_g_presentBack\n\t");
}
static IDirect3DTexture9 *g_blurTex[BLUR_LEVELS];
static IDirect3DStateBlock9 *g_blurState;
static UINT g_blurW, g_blurH;
static int g_blurDead;
typedef struct { float x, y, z, rhw, u, v; } BlurVert;
typedef struct { float x, y, z, rhw; DWORD c; } BlurDark;

static void BlurRelease(void)
{
    for (int i = 0; i < BLUR_LEVELS; i++)
        if (g_blurTex[i]) { IDirect3DTexture9_Release(g_blurTex[i]); g_blurTex[i] = NULL; }
    if (g_blurState) { IDirect3DStateBlock9_Release(g_blurState); g_blurState = NULL; }
}

/* The level i's size: halving down to 1/32 (i 0..4), then back up (5..7: 1/16, 1/8, 1/4). */
static void BlurSize(int i, UINT *w, UINT *h)
{
    int sh = i < 5 ? i + 1 : 9 - i;
    *w = g_blurW >> sh; *h = g_blurH >> sh;
    if (*w < 1) *w = 1;
    if (*h < 1) *h = 1;
}

/* The frame in the back buffer, blurred into the last level. 0 on any failure. */
static int BlurCapture(IDirect3DDevice9 *dev, IDirect3DSurface9 *bb)
{
    D3DSURFACE_DESC d;
    if (FAILED(IDirect3DSurface9_GetDesc(bb, &d))) return 0;
    if (d.Width != g_blurW || d.Height != g_blurH) { BlurRelease(); g_blurW = d.Width; g_blurH = d.Height; }
    for (int i = 0; i < BLUR_LEVELS; i++)
        if (!g_blurTex[i]) {
            UINT w, h;
            BlurSize(i, &w, &h);
            if (FAILED(IDirect3DDevice9_CreateTexture(dev, w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT,
                                                      &g_blurTex[i], NULL))) { g_blurTex[i] = NULL; return 0; }
        }
    IDirect3DSurface9 *from = bb;
    IDirect3DSurface9_AddRef(bb);
    int ok = 1;
    for (int i = 0; i < BLUR_LEVELS && ok; i++) {
        IDirect3DSurface9 *to = NULL;
        ok = SUCCEEDED(IDirect3DTexture9_GetSurfaceLevel(g_blurTex[i], 0, &to)) &&
             SUCCEEDED(IDirect3DDevice9_StretchRect(dev, from, NULL, to, NULL, D3DTEXF_LINEAR));
        IDirect3DSurface9_Release(from);
        from = to;
        if (!from) break;
    }
    if (from) IDirect3DSurface9_Release(from);
    return ok;
}

/* Over the whole frame: the blurred level at alpha a, then black at alpha dark. */
static void BlurDraw(IDirect3DDevice9 *dev, float a, float dark)
{
    float w = (float)g_blurW, h = (float)g_blurH;
    IDirect3DDevice9_SetVertexShader(dev, NULL);
    IDirect3DDevice9_SetPixelShader(dev, NULL);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, D3DZB_FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_BLENDOP, D3DBLENDOP_ADD);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xF);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    if (a > 0.003f) {
        IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)g_blurTex[BLUR_LEVELS - 1]);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_SRGBTEXTURE, FALSE);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXCOORDINDEX, 0);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_TEXTUREFACTOR, (DWORD)(FireClamp(a, 0, 1) * 255.0f + 0.5f) << 24);
        IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_TEX1);
        BlurVert v[4] = { { -0.5f, -0.5f, 0, 1, 0, 0 }, { w - 0.5f, -0.5f, 0, 1, 1, 0 },
                          { -0.5f, h - 0.5f, 0, 1, 0, 1 }, { w - 0.5f, h - 0.5f, 0, 1, 1, 1 } };
        IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof v[0]);
    }
    if (dark > 0.003f) {
        IDirect3DDevice9_SetTexture(dev, 0, NULL);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        DWORD c = (DWORD)(FireClamp(dark, 0, 1) * 255.0f + 0.5f) << 24;
        BlurDark v[4] = { { -0.5f, -0.5f, 0, 1, c }, { w - 0.5f, -0.5f, 0, 1, c }, { -0.5f, h - 0.5f, 0, 1, c },
                          { w - 0.5f, h - 0.5f, 0, 1, c } };
        IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof v[0]);
    }
}

/* From PresentStub, on the game's thread, just before the device presents the frame. */
void WINAPI PrePresent(unsigned char *gx)
{
    int loading = g_loadFrame;
    g_loadFrame = 0;
    if (!loading) InterlockedExchange(&g_worldFrames, g_worldFrames + 1);   /* (the view waits for one to fade) */
    if (!g_blurOn || g_blurDead || !gx) return;
    float amount = 0;
    int mode = BlurStep(loading, GetTickCount(), &amount);
    if (mode == 3) { BlurRelease(); return; }
    if (!mode || !Readable(gx + GX_D3D_OFF, 4) || !Readable(gx + GX_SCENE_OFF, 4) || *(volatile DWORD *)(gx + GX_SCENE_OFF))
        return;                                              /* (inside the game's scene: not this frame) */
    IDirect3DDevice9 *dev = *(IDirect3DDevice9 *volatile *)(gx + GX_D3D_OFF);
    IDirect3DSurface9 *bb = NULL, *rt = NULL;
    if (!dev || FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return;
    int ok = 1;
    if (mode == 1) ok = BlurCapture(dev, bb);                /* the loading frame, blurred afresh */
    else ok = g_blurTex[BLUR_LEVELS - 1] != NULL;            /* the world: the last blurred frame, dissolving */
    if (ok && !g_blurState) ok = SUCCEEDED(IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_ALL, &g_blurState));
    if (ok) ok = SUCCEEDED(IDirect3DStateBlock9_Capture(g_blurState)) &&
                 SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev, 0, &rt));
    if (ok && SUCCEEDED(IDirect3DDevice9_SetRenderTarget(dev, 0, bb)) && SUCCEEDED(IDirect3DDevice9_BeginScene(dev))) {
        D3DVIEWPORT9 vp = { 0, 0, g_blurW, g_blurH, 0.0f, 1.0f };
        IDirect3DDevice9_SetViewport(dev, &vp);
        float dim = (float)g_blurDim / 100.0f;
        BlurDraw(dev, amount, amount * dim);
        IDirect3DDevice9_EndScene(dev);
    } else if (ok)
        ok = 0;
    if (rt) { IDirect3DDevice9_SetRenderTarget(dev, 0, rt); IDirect3DSurface9_Release(rt); }
    if (g_blurState) IDirect3DStateBlock9_Apply(g_blurState);
    IDirect3DSurface9_Release(bb);
    if (!ok) {
        g_blurDead = 1;
        BlurRelease();
        Log("blur off: the device would not take it, so screens end as the game ends them");
    }
}
#else
static void PresentStub(void) {}                         /* the tests' stand-in: its address is all they use */
static void *g_presentBack;
static void BlurRelease(void) {}
#endif

/* At start, with a timed world map: the present's six bytes to PresentStub, once they and what follows are as read. */
static void PresentSetup(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleA(NULL), *at = base + (PRESENT_VA - 0x400000u);
    InterlockedExchange(&g_blurOn, 0);
    if (g_blurMs <= 0 || !g_holdOn) return;
    unsigned ours = (unsigned)((ULONG_PTR)PresentStub - (ULONG_PTR)(at + 5));
    int mine = Readable(at, (int)sizeof kPresent) && at[0] == 0xE9 && Rd32(at + 1) == ours && at[5] == 0x90 &&
               Same(at + 6, kPresent + 6, (int)sizeof kPresent - 6);
    if (!mine && (!Readable(at, (int)sizeof kPresent) || !Same(at, kPresent, (int)sizeof kPresent))) {
        Log("blur off: the game's present is not where it was read (VA 0x5A19FD), so screens end as the game ends them");
        return;
    }
    g_presentBack = at + 6;
    if (!mine) {
        DWORD old;
        if (!VirtualProtect(at, 6, PAGE_EXECUTE_READWRITE, &old)) { Log("blur off: the code could not be made writable"); return; }
        at[0] = 0xE9;
        for (int b = 0; b < 4; b++) at[1 + b] = (unsigned char)(ours >> (8 * b));
        at[5] = 0x90;
        VirtualProtect(at, 6, old, &old);
        FlushInstructionCache(GetCurrentProcess(), at, 6);
    }
    InterlockedExchange(&g_blurOn, 1);
    char m[160];
    wsprintfA(m, "blur on: each screen ends out of focus and %ld%% darker over %ld ms, dissolving into the world over %d ms "
              "(VA 0x5A19FD, in memory only)", (long)g_blurDim, (long)g_blurMs, DISSOLVE_MS);
    Log(m);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
#ifdef __i386__
        g_inst = inst;
#endif
        ClientDataFind();
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
        if (g_dir[0]) ViewStart();
    }
    return TRUE;
}

BOOL WINAPI DllMainCRTStartup(HINSTANCE inst, DWORD reason, LPVOID reserved) { return DllMain(inst, reason, reserved); }
