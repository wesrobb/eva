#pragma once

/**
 * Eva is an library for creating cross-platform event driven applications.
 * It provides a framebuffer for an application to render into.
 * All windows created by eva are high-dpi by default.
 *
 * Eva take snippets of inspiration and code from the following libraries:
 *     - https://github.com/floooh/sokol/blob/master/sokol_app.h
 *     - https://github.com/emoon/minifb 
 *     - https://www.glfw.org
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum eva_event_type {
    EVA_EVENTTYPE_WINDOW,
    EVA_EVENTTYPE_KB,
    EVA_EVENTTYPE_REDRAWFRAME,
    EVA_EVENTTYPE_QUITREQUESTED,
} eva_event_type;

typedef enum eva_kb_event_type {
    EVA_KB_EVENTTYPE_KEYDOWN,
} eva_kb_event_type;

typedef struct eva_window_event {
    uint32_t window_width;
    uint32_t window_height;

    uint32_t framebuffer_width;
    uint32_t framebuffer_height;

    float scale_x;
    float scale_y;
} eva_window_event;

typedef struct eva_kb_event {
    eva_kb_event_type type;

    /**
     * @brief Null-terminated array of utf8 encoded characters. 
     *
     * A single character is not sufficient since utf8 codepoints > 128 are 
     * larger than a single byte.
     */
    const char* utf8_codepoint;
} eva_kb_event;

typedef struct eva_event {
    eva_event_type type;
    union {
        eva_window_event window;
        eva_kb_event     kb;
    };
} eva_event;

typedef struct eva_pixel {
    uint8_t b, g, r, a;
} eva_pixel;

typedef struct eva_rect {
    int32_t x, y, w, h;
} eva_rect;

typedef struct eva_framebuffer {
    uint32_t w, h;

    uint32_t pitch;       // Max width
    uint32_t max_height;  // Max height
    
    /**
     * @brief The dpi scale for high-dpi displays.
     *
     * e.g. On a typical retina display the window reports a resolution of
     * 1440x900 but that actual framebuffer resolution is 2880x1800. In this
     * case the scale will be x=2.0f and y=2.0f.
     */
    float scale_x, scale_y; 

    eva_pixel *pixels;
} eva_framebuffer;

/**
 * @brief Identifiers for individual mouse buttons.
 *
 * @see @ref eva_mouse_btn_fn
 *
 * @ingroup input
 */
typedef enum eva_mouse_btn {
    EVA_MOUSE_BTN_LEFT,
    EVA_MOUSE_BTN_RIGHT,
    EVA_MOUSE_BTN_MIDDLE,
} eva_mouse_btn;

/**
 * @brief Identifiers for mouse actions.
 *
 * @see @ref eva_mouse_btn_fn
 *
 * @ingroup input
 */
typedef enum eva_mouse_action {
    EVA_MOUSE_PRESSED,
    EVA_MOUSE_RELEASED
} eva_mouse_action;

typedef void(*eva_init_fn)(void);
typedef void(*eva_cleanup_fn)(void);
typedef void(*eva_event_fn)(eva_event *event);
typedef void(*eva_fail_fn)(int32_t error_code, const char *error_string);

/**
 * @brief The function pointer type for frame callbacks.
 *
 * This is the function pointer type for frame callbacks. It has the following
 * signature:
 * @code
 * void frame(const eva_framebuffer* fb);
 * @endcode
 *
 * This function is called when it is time to draw to the screen. This happens
 * when either a window event happens (e.g. window is resized or moved between
 * monitors) or an event is triggered in the application (e.g. 
 * [mouse click](@ref eva_mouse_btn_fn)) and the application requested a frame
 * be drawn via a call to [eva_request_frame](@ref eva_request_frame).
 */
typedef void(*eva_frame_fn)(const eva_framebuffer* fb);

/**
 * @brief The function pointer type for mouse moved event callbacks.
 *
 * This is the function pointer type for mouse moved event callbacks. It has
 * the following signature:
 * @code
 * void mouse_moved(int32_t mouse_x, int32_t mouse_y);
 * @endcode
 *
 * @param[in] x The mouse's new x position relative to the left of the window's
 * content area.
 * @param[in] y The mouse's new y position relative to the top of the window's
 * content area.
 *
 * @see @ref eva_set_mouse_moved_fn
 *
 * @ingroup input
 */
typedef void(*eva_mouse_moved_fn)(int32_t x, int32_t y);

/**
 * @brief The function pointer type for mouse button event callbacks.
 *
 * This is the function pointer type for mouse button event callbacks. It has
 * the following signature:
 * @code
 * void mouse_button(int32_t x, int32_y, 
 *                   eva_mouse_btn btn, eva_mouse_action action);
 * @endcode
 *
 * @param[in] x The mouse's x position relative to the left of the window's
 * content area at the time of the button action.
 * @param[in] y The mouse's y position relative to the top of the window's
 * content area at the time of the button action.
 * @param[in] btn The [button](/ref eva_mouse_btn) that triggered the action.
 * @param[in] action The [action](/ref eva_mouse_action) that occurred.
 *
 * @see @ref eva_set_mouse_moved_fn
 *
 * @ingroup input
 */
typedef void(*eva_mouse_btn_fn)(int32_t x, int32_t y, 
                                eva_mouse_btn btn, eva_mouse_action action);

/**
 * Start the application. This will create a window with high-dpi support
 * if possible. The provided event function is resposible for populating
 * the eva framebuffer and then requesting a draw with
 * eva_request_frame().
 */
void eva_run(const char     *window_title,
             eva_init_fn    init_fn,
             eva_event_fn   event_fn,
             eva_frame_fn   frame_fn,
             eva_cleanup_fn cleanup_fn,
             eva_fail_fn    fail_fn);
/**
 * Use to cancel a pending quit on a EVA_EVENTTYPE_QUITREQUESTED event.
 */
void eva_cancel_quit(void);

/**
 * @brief Request that a frame be drawn.
 *
 * This will trigger the [frame callback](@ref eva_frame_fn) once the current
 * event has finished being processed. 
 *
 * Only one call to the (frame callback)[@ref eva_frame_fn] will actually take
 * place and only one frame will actually be drawn no matter how many times 
 * this method is called in the current event handler. 
 */
void eva_request_frame(void);

uint32_t eva_get_window_width(void);
uint32_t eva_get_window_height(void);

/**
 * Returns the framebuffer struct that should be drawn into before requesting
 * it be drawn to the screen with a call to eva_request_frame().
 */
eva_framebuffer eva_get_framebuffer(void);

/** 
 * @brief Sets a function to be called when the mouse is moved.
 *
 * See @ref eva_mouse_moved_fn
 *
 * @ingroup input
 */
void eva_set_mouse_moved_fn(eva_mouse_moved_fn mouse_moved_fn);

/** 
 * @brief Sets a function to be called when a mouse button is pressed/released.
 *
 * See @ref eva_mouse_btn_fn
 *
 * @ingroup input
 */
void eva_set_mouse_btn_fn(eva_mouse_btn_fn mouse_btn_fn);

eva_rect eva_rect_union(const eva_rect *a, const eva_rect *b);
bool eva_rect_empty(const eva_rect *a);

/**
 * Initialize the timer subsystem.
 */
void eva_time_init(void);
uint64_t eva_time_now(void);
uint64_t eva_time_since(uint64_t start);

float eva_time_ms(uint64_t t);
float eva_time_elapsed_ms(uint64_t start, uint64_t end);
float eva_time_since_ms(uint64_t start);
