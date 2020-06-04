#include "eva.h"

#include <stdio.h>

void draw_test_rect()
{
    eva_pixel *framebuffer = eva_get_framebuffer();
    int32_t framebuffer_width = eva_get_framebuffer_width();
    int32_t framebuffer_height = eva_get_framebuffer_height();

    for (int j = 0; j < framebuffer_height / 2; j++) {
        for (int i = 0; i < framebuffer_width / 2; i++) {
            eva_pixel red = { .r = 255, .g = 0, .b = 0, .a = 255 };
            framebuffer[i + j * framebuffer_width] = red;
        }
    }
}

void event(eva_event *e)
{
    switch (e->type) {
    case EVA_EVENTTYPE_WINDOW:
        puts("Received eva window event");
        break;
    case EVA_EVENTTYPE_MOUSE: {
        int32_t x = e->mouse.x;
        int32_t y = e->mouse.y;
        printf("Mouse pos %d, %d\n", x, y);
    } break;
    case EVA_EVENTTYPE_KEYBOARD:
        puts("Received eva keyboard event");
        break;
    case EVA_EVENTTYPE_QUITREQUESTED:
        puts("Received eva quit requested");
        break;
    case EVA_EVENTTYPE_FULLFRAME:
        puts("Full frame requested");
        break;
    }

    draw_test_rect();
}

void init()
{
    puts("Init");
};

void cleanup()
{
    puts("Cleaning up");
};

void fail(int error_code, const char *error_message)
{
    printf("Error %d: %s\n", error_code, error_message);
}

int main(int argv, const char **argc)
{
    eva_run("Hello, eva!", init, event, cleanup, fail);
    return 0;
}
