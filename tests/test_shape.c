/* Native tests of CinderLoad's shape choice (dll/shape.h): gcc -std=c99 -Wall -o /tmp/t test_shape.c && /tmp/t */
#include <stdio.h>
#include "../dll/shape.h"

static int fails, checks;
static void Check(int cond, const char *what) { checks++; if (!cond) { printf("FAIL: %s\n", what); fails++; } }

static const char *Pick(int w, int h)
{
    static const int all[SHAPE_COUNT] = { 1, 1, 1, 1 };
    int i = ChooseShape(w, h, all);
    return i < 0 ? "none" : kShapes[i].name;
}

static int Is(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

int main(void)
{
    int w = 0, h = 0;
    Check(ParseResolution("SET gxResolution \"5120x2160\"\r\nSET gxWindow \"1\"\r\n", &w, &h) && w == 5120 && h == 2160,
          "his own Config.wtf line");
    w = h = 0;
    Check(ParseResolution("SET gxColorBits \"24\"\nset gxresolution \"3440x1440\"\n", &w, &h) && w == 3440 && h == 1440,
          "lower case, not on the first line");
    w = h = 0;
    Check(ParseResolution("SET gxResolution \"1920x1080\"\nSET gxResolution \"2560x1440\"\n", &w, &h) && w == 2560,
          "the last line wins, as the game reads it");
    Check(!ParseResolution("SET gxRefresh \"165\"\n", &w, &h), "no resolution line");
    Check(!ParseResolution("SET gxResolution \"5120x\"\n", &w, &h), "a broken value");
    Check(!ParseResolution("SET gxResolution \"10x10\"\n", &w, &h), "an impossible size");
    Check(!ParseResolution("# SET gxResolution \"1920x1080\"\n", &w, &h), "not at the start of a line");
    Check(!ParseResolution("", &w, &h), "an empty file");
    w = h = 0;
    Check(ParseResolution("\xEF\xBB\xBFSET gxResolution \"2560x1080\"\r\n", &w, &h) && w == 2560 && h == 1080,
          "the only line behind the file's UTF-8 mark");
    w = h = 0;
    Check(ParseResolution("SET gxWindow \"1\"\n  \tSET gxResolution \"3440x1440\"\n", &w, &h) && w == 3440 && h == 1440,
          "spaces and a tab before the SET");
    w = h = 0;
    Check(ParseResolution("SET gxResolution \"1920x1080\"\n   ", &w, &h) && w == 1920, "spaces at the very end: no read past it");
    Check(!ParseResolution("\xEF", &w, &h) && !ParseResolution("\xEF\xBB", &w, &h) && !ParseResolution("  ", &w, &h),
          "a mark cut short, blanks only: nothing, and no read past the end");
    Check(!ParseResolution("x\xEF\xBB\xBFSET gxResolution \"2560x1080\"\n", &w, &h), "a mark anywhere but the start: not one");

    Check(Is(Pick(5120, 2160), "21x9"), "5K2K is 21x9");
    Check(Is(Pick(3440, 1440), "21x9"), "Alienware 3440x1440 is 21x9");
    Check(Is(Pick(3840, 1600), "21x9"), "3840x1600 is 21x9");
    Check(Is(Pick(2560, 1080), "21x9"), "2560x1080 is 21x9");
    Check(Is(Pick(1920, 1080), "16x9"), "1080p is 16x9");
    Check(Is(Pick(2560, 1440), "16x9"), "1440p is 16x9");
    Check(Is(Pick(3840, 2160), "16x9"), "4K is 16x9");
    Check(Is(Pick(1920, 1200), "16x10"), "1920x1200 is 16x10");
    Check(Is(Pick(2560, 1600), "16x10"), "2560x1600 is 16x10");
    Check(Is(Pick(5120, 1440), "32x9"), "Samsung G9 5120x1440 is 32x9");
    Check(Is(Pick(1024, 768), "none"), "4:3 gets the game's own");
    Check(Is(Pick(1280, 1024), "none"), "5:4 gets the game's own");
    Check(Is(Pick(0, 0), "none"), "no size");

    int only21[SHAPE_COUNT] = { 0, 0, 1, 0 };
    Check(ChooseShape(1920, 1080, only21) == -1, "16:9 with only the ultrawide archive: none, not a stretched one");
    Check(ChooseShape(3440, 1440, only21) == 2, "ultrawide with only the ultrawide archive");

    /* the game on OpenGL: -d3d first, then -opengl, then Config.wtf's last SET gxApi */
    Check(!GameUsesOpenGL("\"C:\\WoW\\WoW.exe\"", NULL) && !GameUsesOpenGL("", "SET gxApi \"D3D\"\n"), "Direct3D by default");
    Check(GameUsesOpenGL("\"C:\\WoW\\WoW.exe\" -opengl", NULL) && GameUsesOpenGL("WoW.exe -OpenGL -console", NULL),
          "-opengl on the command line, any case, among other flags");
    Check(!GameUsesOpenGL("WoW.exe -opengl -d3d", "SET gxApi \"OpenGL\"\n"), "-d3d beats both");
    Check(!GameUsesOpenGL("WoW.exe -openglx", NULL) && !GameUsesOpenGL("C:\\x-opengl\\WoW.exe", NULL),
          "a flag is a whole word: not -openglx, not a folder named x-opengl");
    Check(GameUsesOpenGL("WoW.exe", "SET gxResolution \"1920x1080\"\n  set gxapi \"opengl\"\r\n"),
          "Config.wtf's gxApi, indented, any case");
    Check(!GameUsesOpenGL("WoW.exe", "SET gxApi \"OpenGL\"\nSET gxApi \"D3D\"\n") &&
          GameUsesOpenGL("WoW.exe", "\xEF\xBB\xBFSET gxApi \"OpenGL\"\n"), "the last gxApi line counts; behind a UTF-8 mark too");
    Check(!GameUsesOpenGL("WoW.exe", "SET gxApi \"OpenGLx\"\n"), "a value only starting OpenGL: not OpenGL");

    /* the game in a window (slice 5): -fullscreen, then -windowed, then Config.wtf's last SET gxWindow, then the client's
     * own default (the client writes only values that differ from its defaults; RavenCraft's registers "1") */
    Check(GameIsWindowed("\"C:\\WoW\\WoW.exe\"", NULL, 1) && GameIsWindowed("WoW.exe", "SET gxResolution \"1920x1080\"\n", 1),
          "no -windowed and no gxWindow line: the client's default, a window in RavenCraft's");
    Check(!GameIsWindowed("WoW.exe", NULL, 0) && !GameIsWindowed("WoW.exe", "SET gxResolution \"1920x1080\"\n", 0),
          "no line and a client whose default is the whole screen: the whole screen");
    Check(GameIsWindowed("WoW.exe", "SET gxWindow \"1\"\n", 0) && !GameIsWindowed("WoW.exe", "SET gxWindow \"0\"\n", 1),
          "gxWindow 1 a window, 0 the whole screen, whatever the default");
    Check(GameIsWindowed("WoW.exe -windowed", "SET gxWindow \"0\"\n", 0) && GameIsWindowed("WoW.exe -console -Windowed", NULL, 0),
          "-windowed beats Config.wtf and the default; any case, among other flags");
    Check(!GameIsWindowed("WoW.exe -fullscreen", "SET gxWindow \"1\"\n", 1) && !GameIsWindowed("WoW.exe -FullScreen -windowed", NULL, 1),
          "-fullscreen beats Config.wtf, the default and -windowed");
    Check(GameIsWindowed("WoW.exe -fullscreenx", NULL, 1) && GameIsWindowed("C:\\x-fullscreen\\WoW.exe", NULL, 1) &&
          !GameIsWindowed("WoW.exe -windowedx", NULL, 0) && !GameIsWindowed("C:\\x-windowed\\WoW.exe", NULL, 0),
          "the flags are whole words");
    Check(!GameIsWindowed("WoW.exe", "SET gxWindow \"1\"\nSET gxWindow \"0\"\n", 1) &&
          GameIsWindowed("WoW.exe", "SET gxWindow \"0\"\r\n  set gxwindow \"1\"\r\n", 0),
          "the last gxWindow line counts; indented, any case, CRLF");
    Check(!GameIsWindowed("WoW.exe", "\xEF\xBB\xBFSET gxWindow \"0\"\n", 1), "gxWindow behind a UTF-8 mark");
    Check(!GameIsWindowed("WoW.exe", "SET gxWindow \"\"\n", 1), "an empty value: the whole screen, the safe reading");
    Check(!GameIsWindowed("WoW.exe", "SET gxWindowX \"1\"\nSET gxMaximize \"1\"\n# SET gxWindow \"1\"\n", 0) &&
          GameIsWindowed("WoW.exe", "SET gxWindowX \"0\"\nSET gxMaximize \"0\"\n# SET gxWindow \"0\"\n", 1),
          "another setting, gxMaximize, a line not starting SET: no gxWindow line, the default");
    if (fails) printf("%d of %d checks FAILED\n", fails, checks); else printf("all %d checks passed\n", checks);
    return fails != 0;
}
