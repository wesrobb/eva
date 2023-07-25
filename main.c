#include "eva.h"

#include <stdio.h>

typedef struct rectangle {
    int x, y, w, h;
} rectangle;

static rectangle rect;

void clear(const eva_framebuffer *fb)
{
    eva_pixel gray = { .r = 20, .g = 20, .b = 20, .a = 255 };
    for (int j = 0; j < fb->h; j++) {
        for (int i = 0; i < fb->w; i++) {
            fb->pixels[i + j * fb->w] = gray;
        }
    }
}

void draw_rect(const eva_framebuffer *fb)
{
    eva_pixel red = { .r = 255, .g = 0, .b = 0, .a = 255 };
    for (int j = rect.y; j < rect.y + rect.h; j++) {
        for (int i = rect.x; i < rect.x + rect.w; i++) {
            fb->pixels[i + j * fb->w] = red;
        }
    }
}

void frame(const eva_framebuffer *fb)
{
    clear(fb);
    draw_rect(fb);
}

void init()
{
    puts("Init");
    rect.x = 10;
    rect.y = 10;
    rect.w = 100;
    rect.h = 100;
}

void cleanup()
{
    puts("Cleaning up");
}

void fail(int error_code, const char *error_message)
{
    printf("Error %d: %s\n", error_code, error_message);
}

#ifdef EVA_WINDOWS
#include <Windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
int main()
#endif
{
    eva_run("Hello, eva!", frame, fail);
    return 0;
}
