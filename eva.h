#pragma once

/**
 * Eva is an library for creating cross-platform event driven applications.
 * It provides a framebuffer for an application to render into.
 * All windows created by eva are high-dpi when possible.
 *
 * Eva takes snippets of inspiration and code from the following libraries:
 *     - https://github.com/floooh/sokol/blob/master/sokol_app.h
 *     - https://github.com/emoon/minifb 
 *     - https://www.glfw.org
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct eva_pixel {
    uint8_t b, g, r, a;
} eva_pixel;

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
 * @brief Identifiers for individual keyboard keys.
 *
 * Taken from GLFW.
 *
 * @see @ref eva_key_fn
 *
 * @ingroup input
 */
typedef enum eva_key {
    /* The unknown key */
    EVA_KEY_UNKNOWN           = -1,

    /* Printable keys */
    EVA_KEY_SPACE             = 32,
    EVA_KEY_APOSTROPHE        = 39,  /* ' */
    EVA_KEY_COMMA             = 44,  /* , */
    EVA_KEY_MINUS             = 45,  /* - */
    EVA_KEY_PERIOD            = 46,  /* . */
    EVA_KEY_SLASH             = 47,  /* / */
    EVA_KEY_0                 = 48,
    EVA_KEY_1                 = 49,
    EVA_KEY_2                 = 50,
    EVA_KEY_3                 = 51,
    EVA_KEY_4                 = 52,
    EVA_KEY_5                 = 53,
    EVA_KEY_6                 = 54,
    EVA_KEY_7                 = 55,
    EVA_KEY_8                 = 56,
    EVA_KEY_9                 = 57,
    EVA_KEY_SEMICOLON         = 59,  /* ; */
    EVA_KEY_EQUAL             = 61,  /* = */
    EVA_KEY_A                 = 65,
    EVA_KEY_B                 = 66,
    EVA_KEY_C                 = 67,
    EVA_KEY_D                 = 68,
    EVA_KEY_E                 = 69,
    EVA_KEY_F                 = 70,
    EVA_KEY_G                 = 71,
    EVA_KEY_H                 = 72,
    EVA_KEY_I                 = 73,
    EVA_KEY_J                 = 74,
    EVA_KEY_K                 = 75,
    EVA_KEY_L                 = 76,
    EVA_KEY_M                 = 77,
    EVA_KEY_N                 = 78,
    EVA_KEY_O                 = 79,
    EVA_KEY_P                 = 80,
    EVA_KEY_Q                 = 81,
    EVA_KEY_R                 = 82,
    EVA_KEY_S                 = 83,
    EVA_KEY_T                 = 84,
    EVA_KEY_U                 = 85,
    EVA_KEY_V                 = 86,
    EVA_KEY_W                 = 87,
    EVA_KEY_X                 = 88,
    EVA_KEY_Y                 = 89,
    EVA_KEY_Z                 = 90,
    EVA_KEY_LEFT_BRACKET      = 91,  /* [ */
    EVA_KEY_BACKSLASH         = 92,  /* \ */
    EVA_KEY_RIGHT_BRACKET     = 93,  /* ] */
    EVA_KEY_GRAVE_ACCENT      = 96,  /* ` */
    EVA_KEY_WORLD_1           = 161, /* non-US #1 */
    EVA_KEY_WORLD_2           = 162, /* non-US #2 */

    /* Function keys */
    EVA_KEY_ESCAPE            = 256,
    EVA_KEY_ENTER             = 257,
    EVA_KEY_TAB               = 258,
    EVA_KEY_BACKSPACE         = 259,
    EVA_KEY_INSERT            = 260,
    EVA_KEY_DELETE            = 261,
    EVA_KEY_RIGHT             = 262,
    EVA_KEY_LEFT              = 263,
    EVA_KEY_DOWN              = 264,
    EVA_KEY_UP                = 265,
    EVA_KEY_PAGE_UP           = 266,
    EVA_KEY_PAGE_DOWN         = 267,
    EVA_KEY_HOME              = 268,
    EVA_KEY_END               = 269,
    EVA_KEY_CAPS_LOCK         = 280,
    EVA_KEY_SCROLL_LOCK       = 281,
    EVA_KEY_NUM_LOCK          = 282,
    EVA_KEY_PRINT_SCREEN      = 283,
    EVA_KEY_PAUSE             = 284,
    EVA_KEY_F1                = 290,
    EVA_KEY_F2                = 291,
    EVA_KEY_F3                = 292,
    EVA_KEY_F4                = 293,
    EVA_KEY_F5                = 294,
    EVA_KEY_F6                = 295,
    EVA_KEY_F7                = 296,
    EVA_KEY_F8                = 297,
    EVA_KEY_F9                = 298,
    EVA_KEY_F10               = 299,
    EVA_KEY_F11               = 300,
    EVA_KEY_F12               = 301,
    EVA_KEY_F13               = 302,
    EVA_KEY_F14               = 303,
    EVA_KEY_F15               = 304,
    EVA_KEY_F16               = 305,
    EVA_KEY_F17               = 306,
    EVA_KEY_F18               = 307,
    EVA_KEY_F19               = 308,
    EVA_KEY_F20               = 309,
    EVA_KEY_F21               = 310,
    EVA_KEY_F22               = 311,
    EVA_KEY_F23               = 312,
    EVA_KEY_F24               = 313,
    EVA_KEY_F25               = 314,
    EVA_KEY_KP_0              = 320,
    EVA_KEY_KP_1              = 321,
    EVA_KEY_KP_2              = 322,
    EVA_KEY_KP_3              = 323,
    EVA_KEY_KP_4              = 324,
    EVA_KEY_KP_5              = 325,
    EVA_KEY_KP_6              = 326,
    EVA_KEY_KP_7              = 327,
    EVA_KEY_KP_8              = 328,
    EVA_KEY_KP_9              = 329,
    EVA_KEY_KP_DECIMAL        = 330,
    EVA_KEY_KP_DIVIDE         = 331,
    EVA_KEY_KP_MULTIPLY       = 332,
    EVA_KEY_KP_SUBTRACT       = 333,
    EVA_KEY_KP_ADD            = 334,
    EVA_KEY_KP_ENTER          = 335,
    EVA_KEY_KP_EQUAL          = 336,
    EVA_KEY_LEFT_SHIFT        = 340,
    EVA_KEY_LEFT_CONTROL      = 341,
    EVA_KEY_LEFT_ALT          = 342,
    EVA_KEY_LEFT_SUPER        = 343,
    EVA_KEY_RIGHT_SHIFT       = 344,
    EVA_KEY_RIGHT_CONTROL     = 345,
    EVA_KEY_RIGHT_ALT         = 346,
    EVA_KEY_RIGHT_SUPER       = 347,
    EVA_KEY_MENU              = 348,
    EVA_KEY_LAST              = EVA_KEY_MENU
} eva_key;

/**
 * @brief Flags for modifier keys.
 *
 * Taken from GLFW.
 *
 * @see @ref eva_key_fn
 *
 * @ingroup input
 */
typedef enum eva_mod_flags {
    EVA_MOD_SHIFT     = 0x0001,
    EVA_MOD_CONTROL   = 0x0002,
    EVA_MOD_ALT       = 0x0004,
    EVA_MOD_SUPER     = 0x0008,
    EVA_MOD_CAPS_LOCK = 0x0010,
    EVA_MOD_NUM_LOCK  = 0x0020,
} eva_mod_flags;

/**
 * @brief Identifiers for mouse actions.
 *
 * @see @ref eva_mouse_btn_fn
 *
 * @ingroup input
 */
typedef enum eva_input_action {
    EVA_INPUT_PRESSED,
    EVA_INPUT_RELEASED
} eva_input_action;

/**
 * @brief The function pointer type for the initialization callback.
 *
 * This is the function pointer type for the initialization callback. It has
 * the following signature:
 * @code
 * void init(void);
 * @endcode
 *
 * See @ref eva_set_init_fn
 *
 * @ingroup initalization
 */
typedef void(*eva_init_fn)(void);

/**
 * @brief The function pointer type for the cleanup callback.
 *
 * This is the function pointer type for the cleanup callback. It has
 * the following signature:
 * @code
 * void cleanup(void);
 * @endcode
 *
 * See @ref eva_set_cleanup_fn
 *
 * @ingroup shutdown
 */
typedef void(*eva_cleanup_fn)(void);

/**
 * @brief The function pointer type for the cancel quit callback.
 *
 * This is the function pointer type for the cancel quit callback. It has
 * the following signature:
 * @code
 * bool cancel_quit(void);
 * @endcode
 *
 * If set this function will be called when the application wants to close. It
 * provides an opportunity to cancel the quit sequence. Typically this would be
 * used to provide the user to save unsaved work or show a quit confirmation
 * dialog.
 *
 * @return True to continue the quit sequence, false otherwise to cancel the
 * quit sequence.
 *
 * @see @ref eva_set_cancel_quit_fn
 *
 * @ingroup shutdown
 */
typedef bool(*eva_cancel_quit_fn)(void);

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
 *
 * see @ref eva_request_frame
 *
 * @ingroup drawing
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
 * @brief The function pointer type for mouse dragged event callbacks.
 *
 * This is the function pointer type for mouse dragged event callbacks. It has
 * the following signature:
 * @code
 * void mouse_dragged(int32_t mouse_x, int32_t mouse_y, eva_mouse_btn btn);
 * @endcode
 *
 * @param[in] x The mouse's new x position relative to the left of the window's
 * content area.
 * @param[in] y The mouse's new y position relative to the top of the window's
 * content area.
 * @param[in] btn The [button](/ref eva_mouse_btn) that was held during the
 * dragging movement.
 *
 * @see @ref eva_set_mouse_dragged_fn
 *
 * @ingroup input
 */
typedef void(*eva_mouse_dragged_fn)(int32_t x, int32_t y, eva_mouse_btn);

/**
 * @brief The function pointer type for mouse button event callbacks.
 *
 * This is the function pointer type for mouse button event callbacks. It has
 * the following signature:
 * @code
 * void mouse_button(int32_t x, int32_t y, 
 *                   eva_mouse_btn btn, eva_input_action action);
 * @endcode
 *
 * @param[in] x The mouse's x position relative to the left of the window's
 * content area at the time of the button action.
 * @param[in] y The mouse's y position relative to the top of the window's
 * content area at the time of the button action.
 * @param[in] btn The [button](/ref eva_mouse_btn) that triggered the action.
 * @param[in] action The [action](/ref eva_input_action) that occurred.
 *
 * @see @ref eva_set_mouse_moved_fn
 *
 * @ingroup input
 */
typedef void(*eva_mouse_btn_fn)(int32_t x, int32_t y, 
                                eva_mouse_btn btn, eva_input_action action);

/**
 * @brief The function pointer type for scroll event callbacks.
 *
 * This is the function pointer type for scroll event callbacks. It has
 * the following signature:
 * @code
 * void scroll(double delta_x, double delta_y);
 * @endcode
 *
 * @param[in] delta_x The scroll offset on the x-axis.
 * @param[in] delta_y The scroll offset on the y-axis.
 *
 * @see @ref eva_set_scroll_fn
 *
 * @ingroup input
 */
typedef void(*eva_scroll_fn)(double delta_x, double delta_y);

/**
 * @brief The function pointer type for physical key press/release event
 * callbacks.
 *
 * This is the function pointer type for physical key press/release event 
 * callbacks. It has the following signature:
 * @code
 * void key_action(eva_key key, eva_input_action action);
 * @endcode
 *
 * @param[in] key The [key](/ref eva_key) that triggered the action.
 * @param[in] action The [action](/ref eva_input_action) that occurred.
 * @param[in] mod The [modifier keys](/ref eva_mod_flags) that were active at
 * the time the key action occurred.
 *
 * @see @ref eva_set_key_fn
 *
 * @ingroup input
 */
typedef void(*eva_key_fn)(eva_key key, eva_input_action action,
                          eva_mod_flags mod);

/**
 * @brief The function pointer type for the unicode text input event callback.
 *
 * This is the function pointer type for the unicode text input event callback. 
 * It has the following signature:
 * @code
 * void text_input(const char* utf8_text, uint32_t len, eva_mod_flags mods);
 * @endcode
 *
 * @param[in] utf8_text The UTF8 encoded text that was input via key-presses or
 * via paste.
 * @param[in] len The length of the text in bytes.
 * @param[in] mod The [modifier keys](/ref eva_mod_flags) that were active at
 * the time the text input occurred.
 *
 * @see @ref eva_set_text_input_fn
 *
 * @ingroup input
 */
typedef void(*eva_text_input_fn)(const char *utf8_text, uint32_t len,
                                 eva_mod_flags mod);

/**
 * @brief The function pointer type for the window resize callback.
 *
 * This is the function pointer type for the window resize callback.  It has the
 * following signature:
 * @code
 * void window_resize(uint32_t framebuffer_width, uint32_t framebuffer_height);
 * @endcode
 *
 * @param[in] framebuffer_width The new width of the framebuffer.
 * @param[in] framebuffer_height The new height of the framebuffer.
 *
 * @see @ref eva_set_window_resize_fn
 *
 * @ingroup window
 */
typedef void(*eva_window_resize_fn)(uint32_t framebuffer_width, 
                                    uint32_t framebuffer_height);

/**
 * Start the application. This will create a window with high-dpi support
 * if possible. The provided event function is resposible for populating
 * the eva framebuffer and then requesting a draw with
 * eva_request_frame().
 */
void eva_run(const char     *window_title,
             eva_frame_fn    frame_fn,
             eva_fail_fn     fail_fn);

/**
 * @brief Request that a frame be drawn.
 *
 * This will trigger the [frame callback](@ref eva_frame_fn) once the current
 * event has finished being processed. 
 *
 * Only one call to the (frame callback)[@ref eva_frame_fn] will actually take
 * place and only one frame will actually be drawn no matter how many times 
 * this method is called in the current event handler. 
 *
 * @ingroup draw
 */
void eva_request_frame(void);

// TODO: Formalizae the idea of content/client area vs window area
uint32_t eva_get_window_width(void);
uint32_t eva_get_window_height(void);

/**
 * Returns the framebuffer struct that should be drawn into before requesting
 * it be drawn to the screen with a call to eva_request_frame().
 */
eva_framebuffer eva_get_framebuffer(void);

/** 
 * @brief Set a function to be called during application initialization.
 *
 * This function is called once during application startup and should be used
 * to prepare the application state before the first [frame](@ref eva_frame_fn)
 * is rendered.
 * 
 * @see @ref eva_init_fn
 *
 * @ingroup initialization
 */
void eva_set_init_fn(eva_init_fn init_fn);

/** 
 * @brief Set a function to be called during application shutdown.
 *
 * This function is called once it is confirmed that the application will be
 * shutdown. 
 * 
 * @see @ref eva_cleanup_fn
 *
 * @ingroup shutdown
 */
void eva_set_cleanup_fn(eva_cleanup_fn cleanup_fn);

/** 
 * @brief Set a function to be called when an application quit is requested.
 *
 * @see @ref eva_cancel_quit_fn
 *
 * @ingroup shutdown
 */
void eva_set_cancel_quit_fn(eva_cancel_quit_fn cancel_quit_fn);

/** 
 * @brief Sets a function to be called when the mouse is moved.
 *
 * See @ref eva_mouse_moved_fn
 *
 * @ingroup input
 */
void eva_set_mouse_moved_fn(eva_mouse_moved_fn mouse_moved_fn);

/** 
 * @brief Sets a function to be called scrolling takes place.
 *
 * See @ref eva_scroll_fn
 *
 * @ingroup input
 */
void eva_set_scroll_fn(eva_scroll_fn scroll_fn);

/** 
 * @brief Sets a function to be called when the mouse is dragged.
 *
 * See @ref eva_mouse_dragged_fn
 *
 * @ingroup input
 */
void eva_set_mouse_dragged_fn(eva_mouse_dragged_fn mouse_dragged_fn);

/** 
 * @brief Sets a function to be called when a mouse button is pressed/released.
 *
 * See @ref eva_mouse_btn_fn
 *
 * @ingroup input
 */
void eva_set_mouse_btn_fn(eva_mouse_btn_fn mouse_btn_fn);

/** 
 * @brief Sets a function to be called when a key is pressed/released.
 *
 * This should be used when responding to specific key press events. See 
 * [eva_set_text_input_fn](@ref eva_set_text_input_fn) for handling text input
 * events.
 *
 * See @ref eva_key_fn
 *
 * @ingroup input
 */
void eva_set_key_fn(eva_key_fn key_fn);

/** 
 * @brief Sets a function to be called when text is input via key presses or 
 * pasting.
 *
 * See @ref eva_text_input_fn
 *
 * @ingroup input
 */
void eva_set_text_input_fn(eva_text_input_fn text_input_fn);

/** 
 * @brief Sets a function to be called when the window is resized.
 *
 * Typically an application would simply request a new frame so that the
 * application can be drawn at the new size.
 *
 * See @ref eva_window_resize_fn
 *
 * @ingroup window
 */
void eva_set_window_resize_fn(eva_window_resize_fn window_resize_fn);

/**
 * Initialize the time subsystem.
 */
void eva_time_init(void);
uint64_t eva_time_now(void);
uint64_t eva_time_since(uint64_t start);

float eva_time_ms(uint64_t t);
float eva_time_elapsed_ms(uint64_t start, uint64_t end);
float eva_time_since_ms(uint64_t start);
