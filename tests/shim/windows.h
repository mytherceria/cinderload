/* A stand-in for <windows.h> with just what cinderload.c calls, on POSIX files, so the DLL's own code runs in
 * tests/test_switch.c. Paths with backslashes are turned into slashes; the game folder and the module's code are
 * set by the test. */
#ifndef SHIM_WINDOWS_H
#define SHIM_WINDOWS_H
#define _DEFAULT_SOURCE                     /* pread, symlink, M_PI under -std=c99 */
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef unsigned long DWORD;
typedef int BOOL;
typedef int LONG;                           /* 32 bits, as on Windows */
typedef void *HANDLE, *HINSTANCE, *LPVOID, *HMODULE;
typedef const void *LPCVOID;
typedef char *LPSTR;
typedef struct { unsigned short wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
typedef struct { void *BaseAddress, *AllocationBase; DWORD AllocationProtect; size_t RegionSize; DWORD State, Protect, Type; } MEMORY_BASIC_INFORMATION;
#define WINAPI
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define DLL_PROCESS_ATTACH 1
#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
#define INVALID_FILE_SIZE ((DWORD)0xFFFFFFFF)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define FILE_APPEND_DATA 4
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define CREATE_ALWAYS 2
#define MEM_COMMIT 0x1000
#define PAGE_NOACCESS 1
#define PAGE_GUARD 0x100
#define PAGE_EXECUTE_READ 0x20
#define PAGE_READWRITE 0x04
#define PAGE_EXECUTE_READWRITE 0x40
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1

#define SHIM_MODULE_SIZE 0x900000
extern char shim_gamedir[];                          /* "/tmp/x/" */
extern unsigned char shim_module[SHIM_MODULE_SIZE];  /* WoW.exe's image, as far as the sound engine's flag */
extern int shim_threads;                             /* threads CreateThread was asked for (none is run) */
extern DWORD shim_ticks;                             /* what GetTickCount says */
extern int shim_screen_w, shim_screen_h;

static void ShimPath(char *out, const char *in) { strcpy(out, in); for (char *p = out; *p; p++) if (*p == '\\') *p = '/'; }
static HANDLE ToHandle(int fd) { return fd < 0 ? INVALID_HANDLE_VALUE : (HANDLE)(long)(fd + 1); }
static int Fd(HANDLE h) { return (int)(long)h - 1; }

static DWORD GetModuleFileNameA(HMODULE m, char *out, DWORD n) { (void)m; snprintf(out, n, "%sWoW.exe", shim_gamedir); return (DWORD)strlen(out); }
static HMODULE GetModuleHandleA(const char *n) { (void)n; return (HMODULE)shim_module; }
static HANDLE CreateFileA(const char *path, DWORD access, DWORD share, void *sa, DWORD disp, DWORD flags, HANDLE t)
{
    (void)share; (void)sa; (void)flags; (void)t;
    char p[1024]; ShimPath(p, path);
    int o = access == GENERIC_READ ? O_RDONLY : (access == FILE_APPEND_DATA ? O_WRONLY | O_APPEND : O_WRONLY);
    if (disp == OPEN_ALWAYS) o |= O_CREAT;
    if (disp == CREATE_ALWAYS) o |= O_CREAT | O_TRUNC;
    return ToHandle(open(p, o, 0644));
}
static BOOL CloseHandle(HANDLE h) { return (long)h == 1000 ? TRUE : close(Fd(h)) == 0; }   /* 1000: a thread */
static DWORD GetFileSize(HANDLE h, DWORD *hi) { if (hi) *hi = 0; struct stat st; return fstat(Fd(h), &st) ? INVALID_FILE_SIZE : (DWORD)st.st_size; }
typedef struct { DWORD Internal, InternalHigh, Offset, OffsetHigh; HANDLE hEvent; } OVERLAPPED;
static BOOL ReadFile(HANDLE h, void *buf, DWORD n, DWORD *got, OVERLAPPED *ov)
{
    long r = ov ? pread(Fd(h), buf, n, (off_t)((unsigned long long)ov->OffsetHigh << 32 | ov->Offset)) : read(Fd(h), buf, n);
    *got = r < 0 ? 0 : (DWORD)r; return r >= 0;
}
static BOOL WriteFile(HANDLE h, const void *buf, DWORD n, DWORD *put, void *ov) { (void)ov; long r = write(Fd(h), buf, n); *put = r < 0 ? 0 : (DWORD)r; return r >= 0; }
static DWORD GetFileAttributesA(const char *path) { char p[1024]; ShimPath(p, path); struct stat st; return stat(p, &st) ? INVALID_FILE_ATTRIBUTES : (S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL); }
static BOOL DeleteFileA(const char *path) { char p[1024]; ShimPath(p, path); return unlink(p) == 0; }
static BOOL CopyFileA(const char *from, const char *to, BOOL failIfExists)
{
    (void)failIfExists; char a[1024], b[1024]; ShimPath(a, from); ShimPath(b, to);
    FILE *in = fopen(a, "rb"), *out = in ? fopen(b, "wb") : NULL; char buf[4096]; size_t n;
    if (!in || !out) { if (in) fclose(in); return FALSE; }
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
    fclose(in); fclose(out); return TRUE;
}
static DWORD GetLastError(void) { return 0; }
#define MOVEFILE_REPLACE_EXISTING 0x1
#define MOVEFILE_WRITE_THROUGH 0x8
static BOOL MoveFileExA(const char *from, const char *to, DWORD flags) { (void)flags; char a[1024], b[1024]; ShimPath(a, from); ShimPath(b, to); return rename(a, b) == 0; }
static DWORD GetCurrentProcessId(void) { return (DWORD)getpid(); }
static void GetLocalTime(SYSTEMTIME *t) { memset(t, 0, sizeof *t); }
static SYSTEMTIME shim_utc;                                   /* UTC as GetSystemTime gives it (the tests set it) */
static void GetSystemTime(SYSTEMTIME *t) { *t = shim_utc; }
static HANDLE GetProcessHeap(void) { return (HANDLE)1; }
#define HEAP_ZERO_MEMORY 0x00000008
static void *HeapAlloc(HANDLE h, DWORD f, size_t n) { (void)h; return (f & HEAP_ZERO_MEMORY) ? calloc(1, n) : malloc(n); }
static BOOL HeapFree(HANDLE h, DWORD f, void *p) { (void)h; (void)f; free(p); return TRUE; }
static int lstrlenA(const char *s) { return (int)strlen(s); }
static char *lstrcpyA(char *d, const char *s) { return strcpy(d, s); }
static int lstrcmpiA(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y || !x) return x - y;
    }
}
static char *lstrcatA(char *d, const char *s) { return strcat(d, s); }
static int wsprintfA(char *out, const char *fmt, ...) { va_list a; va_start(a, fmt); int n = vsprintf(out, fmt, a); va_end(a); return n; }
static int GetSystemMetrics(int i) { return i == SM_CXSCREEN ? shim_screen_w : shim_screen_h; }
extern char shim_cmdline[512];                       /* the game's command line, as GetCommandLineA gives it */
static char *GetCommandLineA(void) { return shim_cmdline; }
extern int shim_vq;                                  /* VirtualQuery's calls, so a test can see one was not made */
static size_t VirtualQuery(const void *at, MEMORY_BASIC_INFORMATION *m, size_t n)
{
    (void)at; shim_vq++; memset(m, 0, n); m->BaseAddress = shim_module; m->RegionSize = SHIM_MODULE_SIZE;
    m->State = MEM_COMMIT; m->Protect = PAGE_EXECUTE_READ; return n;
}
typedef size_t SIZE_T;
typedef unsigned long ULONG_PTR;                     /* as wide as a pointer, as on Windows */
static BOOL VirtualProtect(void *at, size_t n, DWORD prot, DWORD *old) { (void)at; (void)n; (void)prot; *old = PAGE_EXECUTE_READ; return TRUE; }
static BOOL FlushInstructionCache(HANDLE p, const void *at, size_t n) { (void)p; (void)at; (void)n; return TRUE; }
static HANDLE GetCurrentProcess(void) { return (HANDLE)1; }
static BOOL DisableThreadLibraryCalls(HINSTANCE h) { (void)h; return TRUE; }
typedef void (*FARPROC)(void);
extern int shim_wine;                                /* 1: ntdll exports wine_get_version, as under Wine */
static FARPROC GetProcAddress(HMODULE h, const char *n)
{ (void)h; return shim_wine && n && !strcmp(n, "wine_get_version") ? (FARPROC)(size_t)1 : NULL; }
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);
static HANDLE CreateThread(void *sa, size_t st, LPTHREAD_START_ROUTINE f, LPVOID arg, DWORD fl, DWORD *id)
{ (void)sa; (void)st; (void)f; (void)arg; (void)fl; (void)id; shim_threads++; return (HANDLE)(long)1000; }
static void Sleep(DWORD ms) { (void)ms; }
static DWORD GetTickCount(void) { return shim_ticks; }
static LONG InterlockedExchange(volatile LONG *at, LONG v) { LONG old = *at; *at = v; return old; }
static LONG InterlockedIncrement(volatile LONG *p) { return ++*p; }
/* The game's window and its thread's message hook, for the world map's switches (LuaPumpStep, LuaHook): one window,
 * shim_game, alive while shim_game_alive; a hook is recorded, never called by the shim (the test pumps it). */
typedef void *HWND, *HHOOK;
typedef unsigned UINT;
typedef unsigned long WPARAM;
typedef long LPARAM, LRESULT;
#define CALLBACK
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; struct { LONG x, y; } pt; } MSG;
#define WM_NULL 0x0000
#define WM_QUIT 0x0012
#define PM_NOREMOVE 0
#define PM_REMOVE 1
#define HC_ACTION 0
#define WH_GETMESSAGE 3
typedef LRESULT (*HOOKPROC)(int, WPARAM, LPARAM);
extern HWND shim_game;                               /* the game's window (NULL: not found yet) */
extern int shim_game_alive;                          /* IsWindow says so */
extern DWORD shim_game_tid;                          /* its thread */
extern HOOKPROC shim_hook;                           /* the message hook put in (NULL: none) */
extern DWORD shim_hook_tid;
extern int shim_hooks, shim_unhooks, shim_hook_fail; /* hooks put in and taken out; 1: SetWindowsHookExA fails */
extern int shim_posted, shim_post_fail;              /* messages posted to the game's window; 1: PostMessageA fails */
extern UINT shim_posted_msg;
static UINT RegisterWindowMessageA(const char *n) { (void)n; return 0xC0DE; }
static BOOL IsWindow(HWND h) { return h && h == shim_game && shim_game_alive; }
static DWORD GetWindowThreadProcessId(HWND h, DWORD *pid) { (void)h; if (pid) *pid = 0; return shim_game_tid; }
static HHOOK SetWindowsHookExA(int id, HOOKPROC f, HINSTANCE m, DWORD tid)
{ (void)m; if (shim_hook_fail || id != WH_GETMESSAGE) return NULL; shim_hook = f; shim_hook_tid = tid; shim_hooks++; return (HHOOK)(long)(0x500 + shim_hooks); }
static BOOL UnhookWindowsHookEx(HHOOK h) { (void)h; shim_unhooks++; shim_hook = NULL; return TRUE; }
static LRESULT CallNextHookEx(HHOOK h, int c, WPARAM w, LPARAM l) { (void)h; (void)c; (void)w; (void)l; return 0; }
static BOOL PostMessageA(HWND h, UINT m, WPARAM w, LPARAM l)
{ (void)w; (void)l; if (shim_post_fail || !IsWindow(h)) return FALSE; shim_posted++; shim_posted_msg = m; return TRUE; }
typedef struct { int held; } CRITICAL_SECTION;       /* one thread here: counts, so a test can see it is let go */
static void InitializeCriticalSection(CRITICAL_SECTION *c) { c->held = 0; }
static void EnterCriticalSection(CRITICAL_SECTION *c) { c->held++; }
static void LeaveCriticalSection(CRITICAL_SECTION *c) { c->held--; }
#endif
