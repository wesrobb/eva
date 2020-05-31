#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum eva_event_type {
    EVA_EVENTTYPE_WINDOW,
    EVA_EVENTTYPE_MOUSE,
    EVA_EVENTTYPE_KEYBOARD,
    EVA_EVENTTYPE_QUITREQUESTED
} eva_event_type;

typedef struct eva_window_event {
    int32_t window_width, window_height;
    int32_t framebuffer_width, framebuffer_height;
    float   scale_x, scale_y;
} eva_window_event;

typedef struct eva_mouse_event {
    int32_t x, y;
    bool    left_button_pressed, left_button_released;
    bool    middle_button_pressed, middle_button_released;
    bool    right_button_pressed, right_button_released;
} eva_mouse_event;

typedef struct eva_event {
    eva_event_type type;
    union {
        eva_mouse_event  mouse;
        eva_window_event window;
    };
} eva_event;

typedef struct eva_pixel {
    uint8_t r, g, b, a;
} eva_pixel;

typedef void(eva_cleanup_fn)(void);
typedef void(eva_event_fn)(eva_event *event);
typedef void(eva_frame_fn)(eva_pixel *framebuffer,
                           int32_t    framebuffer_width,
                           int32_t    framebuffer_height);
typedef void(eva_fail_fn)(int32_t error_code, const char *error_string);

void eva_run(const char *    window_title,
             eva_frame_fn *  frame_fn,
             eva_event_fn *  event_fn,
             eva_cleanup_fn *cleanup_fn,
             eva_fail_fn *   fail_fn);

void eva_cancel_quit();