#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Eva is an library for creating cross-platform event driven applications.
 * It provides a frame buffer for an application to render to and offers
 * dirty rect drawing optimizations to improve drawing performance.
 * All windows created by eva are high-dpi by default.
 *
 * This library is based heavily on Andre Weissflog's sokol-app.h:
 * https://github.com/floooh/sokol/blob/master/sokol_app.h
 */

typedef enum eva_event_type {
    EVA_EVENTTYPE_WINDOW,
    EVA_EVENTTYPE_MOUSE,
    EVA_EVENTTYPE_KB,
    EVA_EVENTTYPE_REDRAWFRAME,
    EVA_EVENTTYPE_QUITREQUESTED,
} eva_event_type;

typedef enum eva_mouse_event_type {
    EVA_MOUSE_EVENTTYPE_MOUSE_PRESSED,
    EVA_MOUSE_EVENTTYPE_MOUSE_RELEASED,
    EVA_MOUSE_EVENTTYPE_MOUSE_MOVED,
    EVA_MOUSE_EVENTTYPE_MOUSE_ENTERED,
    EVA_MOUSE_EVENTTYPE_MOUSE_EXITED,
} eva_mouse_event_type;

typedef enum eva_kb_event_type {
    EVA_KB_EVENTTYPE_KEYDOWN,
} eva_kb_event_type;

typedef struct eva_window_event {
    int32_t window_width;
    int32_t window_height;

    int32_t framebuffer_width;
    int32_t framebuffer_height;

    float scale_x;
    float scale_y;
} eva_window_event;

typedef struct eva_mouse_event {
    eva_mouse_event_type type;

    int32_t mouse_x;
    int32_t mouse_y;

    bool left_btn_pressed;
    bool left_btn_released;

    bool middle_btn_pressed;
    bool middle_btn_released;

    bool right_btn_pressed;
    bool right_btn_released;
} eva_mouse_event;

typedef struct eva_kb_event {
    eva_kb_event_type type;

    /**
     * Null-terminated array of UTF-8 encoded
     * characters. 
     *
     * A single character is not sufficient since
     * utf8 codepoints > 128 are larger than a
     * single byte.
     */
    const char* utf8_codepoint;
} eva_kb_event;

typedef struct eva_event {
    eva_event_type type;
    union {
        eva_mouse_event  mouse;
        eva_window_event window;
        eva_kb_event     kb;
    };
} eva_event;

typedef struct eva_pixel {
    uint8_t r, g, b, a;
} eva_pixel;

typedef struct eva_rect {
    int32_t x, y, w, h;
} eva_rect;

typedef void(eva_init_fn)(void);
typedef void(eva_cleanup_fn)(void);
typedef void(eva_event_fn)(eva_event *event);
typedef void(eva_fail_fn)(int32_t error_code, const char *error_string);

/**
 * Start the application. This will create a window with high-dpi support
 * if possible. The provided event function is resposible for populating
 * the eva framebuffer and then requesting a draw with
 * eva_request_frame(dirty_rect).
 */
void eva_run(const char     *window_title,
             eva_init_fn    *init_fn,
             eva_event_fn   *event_fn,
             eva_cleanup_fn *cleanup_fn,
             eva_fail_fn    *fail_fn);
/**
 * Use to cancel a pending quit on a EVA_EVENTTYPE_QUITREQUESTED event.
 */
void eva_cancel_quit();

/**
 * Request that a frame be drawn using the current contents
 * of eva_get_framebuffer(). Typically an application would
 * update the framebuffer on an event and then request that it
 * get drawn by calling this method.
 */
void eva_request_frame();

int32_t eva_get_window_width();
int32_t eva_get_window_height();

/**
 * Returns a pointer to the framebuffer that should be filled
 * for drawing.
 */
eva_pixel *eva_get_framebuffer();
int32_t eva_get_framebuffer_width();
int32_t eva_get_framebuffer_height();

float eva_get_framebuffer_scale_x();
float eva_get_framebuffer_scale_y();

void eva_time_init();
uint64_t eva_time_now();
uint64_t eva_time_since(uint64_t start);

float eva_time_ms(uint64_t t);
float eva_time_elapsed_ms(uint64_t start, uint64_t end);
float eva_time_since_ms(uint64_t start);

eva_rect eva_rect_union(const eva_rect *a, const eva_rect *b);
bool eva_rect_empty(const eva_rect *a);
