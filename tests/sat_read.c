/* The satellite's detail archive read with CinderLoad's own MPQ reader (shared/mpq.c), as the DLL will read it: every
 * file named in a list, compared byte for byte with the file it was packed from, and the time each read took.
 *   gcc -std=c99 -O2 -Wall -Ishared -o sat_read tests/sat_read.c shared/mpq.c shared/inflate.c
 *   ./sat_read <CinderSat.MPQ> <list>          list: one "<name in the archive>\t<file on disk>" a line
 * Prints one line: files read and matching, the slowest and the mean read in milliseconds. Exit 0 only when every file
 * read and matched. tools/make_sat.py builds and runs it on every archive it writes. */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mpq.h"

static const char *g_arc;
static void *Alloc(void *ctx, unsigned n) { (void)ctx; return malloc(n ? n : 1); }
static void Release(void *ctx, void *p) { (void)ctx; free(p); }
static void *Open(void *ctx, const char *name) { (void)ctx; (void)name; return fopen(g_arc, "rb"); }
static unsigned ReadAt(void *ctx, void *fh, unsigned long long off, void *buf, unsigned len)
{
    (void)ctx;
    if (fseek((FILE *)fh, (long)off, SEEK_SET)) return 0;
    return (unsigned)fread(buf, 1, len, (FILE *)fh);
}
static unsigned long long Size(void *ctx, void *fh)
{
    long here, end;
    (void)ctx;
    here = ftell((FILE *)fh); fseek((FILE *)fh, 0, SEEK_END); end = ftell((FILE *)fh); fseek((FILE *)fh, here, SEEK_SET);
    return (unsigned long long)end;
}
static void Close(void *ctx, void *fh) { (void)ctx; fclose((FILE *)fh); }

static double Now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }

int main(int argc, char **argv)
{
    mpq_io io = { Alloc, Release, Open, ReadAt, Size, Close, 0 };
    char line[2048];
    int files = 0, good = 0;
    double worst = 0, total = 0;
    FILE *lst;
    if (argc != 3) { fprintf(stderr, "usage: sat_read <archive> <list>\n"); return 2; }
    g_arc = argv[1];
    if (!(lst = fopen(argv[2], "r"))) { perror(argv[2]); return 2; }
    while (fgets(line, sizeof line, lst)) {
        char *tab = strchr(line, '\t'), *nl;
        unsigned char *got = 0, *want;
        unsigned n = 0;
        long wn;
        FILE *f;
        double t0, dt;
        int r;
        if (!tab) continue;
        *tab = 0;
        if ((nl = strpbrk(tab + 1, "\r\n"))) *nl = 0;
        files++;
        t0 = Now();
        r = mpq_read(&io, "CinderSat.MPQ", line, &got, &n);
        dt = Now() - t0;
        total += dt;
        if (dt > worst) worst = dt;
        if (r != MPQ_OK) { printf("FAIL %s: %s\n", line, mpq_error_text(r)); continue; }
        if (!(f = fopen(tab + 1, "rb"))) { printf("FAIL %s: no %s\n", line, tab + 1); free(got); continue; }
        fseek(f, 0, SEEK_END); wn = ftell(f); fseek(f, 0, SEEK_SET);
        want = malloc(wn ? (size_t)wn : 1);
        if (fread(want, 1, (size_t)wn, f) != (size_t)wn) wn = -1;
        fclose(f);
        if (wn != (long)n || memcmp(want, got, n)) printf("FAIL %s: %u bytes read, the file has %ld or differs\n", line, n, wn);
        else good++;
        free(want); free(got);
    }
    fclose(lst);
    printf("shared/mpq.c read %d of %d files, all matching: %s; slowest %.2f ms, mean %.2f ms\n", good, files,
           good == files && files ? "yes" : "NO", worst, files ? total / files : 0.0);
    return good == files && files ? 0 : 1;
}
