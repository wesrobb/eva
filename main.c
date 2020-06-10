#include "eva.h"

#include <stdio.h>

static eva_rect rect;

void clear()
{
    eva_pixel *framebuffer = eva_get_framebuffer();
    int32_t framebuffer_width = eva_get_framebuffer_width();
    int32_t framebuffer_height = eva_get_framebuffer_height();

    eva_pixel gray = { .r = 20, .g = 20, .b = 20, .a = 255 };
    for (int j = 0; j < framebuffer_height; j++) {
        for (int i = 0; i < framebuffer_width; i++) {
            framebuffer[i + j * framebuffer_width] = gray;
        }
    }
}

void draw_rect()
{
    eva_pixel *framebuffer = eva_get_framebuffer();
    int32_t framebuffer_width = eva_get_framebuffer_width();

    eva_pixel red = { .r = 255, .g = 0, .b = 0, .a = 255 };
    for (int j = rect.y; j < rect.y + rect.h; j++) {
        for (int i = rect.x; i < rect.x + rect.w; i++) {
            framebuffer[i + j * framebuffer_width] = red;
        }
    }
}

void event(eva_event *e)
{
    int32_t delta_x = 0;
    bool full_frame = false;
    switch (e->type) {
    case EVA_EVENTTYPE_WINDOW:
        puts("Received eva window event");
        break;
    case EVA_EVENTTYPE_MOUSE:
        return;
        break;
    case EVA_EVENTTYPE_KB:
        delta_x += 10;
        puts("Received eva keyboard event");
        break;
    case EVA_EVENTTYPE_QUITREQUESTED:
        puts("Received eva quit requested");
        break;
    case EVA_EVENTTYPE_REDRAWFRAME:
        puts("Full frame requested");
        full_frame = true;
        break;
    }
    puts("drawing");
    rect.x += delta_x;
    clear();
    draw_rect();

    eva_request_frame();
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

int main()
{
    eva_run("Hello, eva!", init, event, cleanup, fail);
    return 0;
}
