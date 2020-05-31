#include "eva.h"

#include <stdio.h>

void frame(eva_pixel *framebuffer,
           int32_t    framebuffer_width,
           int32_t    framebuffer_height)
{
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
    case EVA_EVENTTYPE_MOUSE:
        puts("Received eva mouse event");
        break;
    case EVA_EVENTTYPE_KEYBOARD:
        puts("Received eva keyboard event");
        break;
    case EVA_EVENTTYPE_QUITREQUESTED:
        puts("Received eva quit requested");
        break;
    }
}

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
    eva_run("Hello, eva!", frame, event, cleanup, fail);
    return 0;
}
