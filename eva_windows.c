#include "eva.h"

#include <Windows.h>

#include <assert.h>
#include <stdio.h>

#pragma comment(lib, "User32.lib")

static LRESULT CALLBACK wnd_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void update_window();
static void handle_paint();
static void handle_close();
static void handle_resize();
static void try_frame();
static bool utf8_to_utf16(const char* src, wchar_t* dst, int dst_num_bytes);
static bool utf16_to_utf8(const wchar_t* src, char* dst, int dst_num_bytes);

typedef struct eva_ctx {
    int32_t     window_width, window_height;
    eva_framebuffer framebuffer;
    const char *window_title;
    bool        quit_requested;
    bool        quit_ordered;

    eva_init_fn          init_fn;
    eva_frame_fn         frame_fn;
    eva_cleanup_fn       cleanup_fn;
    eva_fail_fn          fail_fn;
    eva_cancel_quit_fn   cancel_quit_fn;
    eva_mouse_moved_fn   mouse_moved_fn;
    eva_mouse_dragged_fn mouse_dragged_fn;
    eva_mouse_btn_fn     mouse_btn_fn;
    eva_scroll_fn        scroll_fn;
    eva_key_fn           key_fn;
    eva_text_input_fn    text_input_fn;
    eva_window_resize_fn window_resize_fn;

    LARGE_INTEGER ticks_per_sec;
    HWND hwnd;
    bool window_shown;
    bool resizing;
    bool frame_requested;
} eva_ctx;

static eva_ctx _ctx;

void eva_run(const char    *window_title,
             eva_frame_fn   frame_fn,
             eva_fail_fn    fail_fn)
{
    assert(window_title);
    assert(frame_fn);
    assert(fail_fn);

    eva_time_init();

    _ctx.window_title = window_title;
    _ctx.frame_fn     = frame_fn;
    _ctx.fail_fn      = fail_fn;

   if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
       // TODO: Comment make a common set of error codes.
       _ctx.fail_fn(GetLastError(), "Failed to set DPI");
   }

    WNDCLASSW wndclassw;
    memset(&wndclassw, 0, sizeof(wndclassw));
    wndclassw.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclassw.lpfnWndProc = (WNDPROC) wnd_proc;
    wndclassw.hInstance = GetModuleHandleW(NULL);
    wndclassw.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclassw.hIcon = LoadIcon(NULL, IDI_WINLOGO);
    wndclassw.lpszClassName = L"eva";
    RegisterClassW(&wndclassw);

    DWORD style = WS_CLIPSIBLINGS | 
                  WS_CLIPCHILDREN |
                  WS_CAPTION      |
                  WS_SYSMENU      |
                  WS_MINIMIZEBOX  |
                  WS_MAXIMIZEBOX  |
                  WS_SIZEBOX;
    DWORD ex_style = WS_EX_APPWINDOW |
                     WS_EX_WINDOWEDGE;

    wchar_t window_title_utf16[256];
    utf8_to_utf16(window_title, window_title_utf16, 256);
    
    _ctx.hwnd = CreateWindowExW(ex_style,      
                                L"eva", 
                                window_title_utf16,
                                style,         
                                CW_USEDEFAULT, 
                                CW_USEDEFAULT, 
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                NULL,
                                NULL,
                                GetModuleHandleW(NULL),
                                NULL);
    update_window();
    if (_ctx.init_fn) {
        _ctx.init_fn();
    }

    // Let the application full it's framebuffer before showing the window.
    _ctx.frame_fn(&_ctx.framebuffer);

    ShowWindow(_ctx.hwnd, SW_SHOW);
    _ctx.window_shown = true;

    bool done = false;
    while (!(done || _ctx.quit_ordered)) {
        MSG msg;
        GetMessageW(&msg, NULL, 0, 0);

        if (WM_QUIT == msg.message) {
            done = true;
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (_ctx.quit_requested) {
            PostMessage(_ctx.hwnd, WM_CLOSE, 0, 0);
        }
    }
    _ctx.cleanup_fn();

    DestroyWindow(_ctx.hwnd);
    UnregisterClassW(L"eva", GetModuleHandleW(NULL));
}

void eva_request_frame()
{
    _ctx.frame_requested = true;
}

uint32_t eva_get_window_width()
{
    return _ctx.window_width;
}

uint32_t eva_get_window_height()
{
    return _ctx.window_height;
}

eva_framebuffer eva_get_framebuffer()
{
    return _ctx.framebuffer;
}

void eva_set_init_fn(eva_init_fn init_fn)
{
    _ctx.init_fn = init_fn;
}

void eva_set_cleanup_fn(eva_cleanup_fn cleanup_fn)
{
    _ctx.cleanup_fn = cleanup_fn;
}

void eva_set_cancel_quit_fn(eva_cancel_quit_fn cancel_quit_fn)
{
    _ctx.cancel_quit_fn = cancel_quit_fn;
}

void eva_set_mouse_moved_fn(eva_mouse_moved_fn mouse_moved_fn)
{
    _ctx.mouse_moved_fn = mouse_moved_fn;
}

void eva_set_mouse_dragged_fn(eva_mouse_dragged_fn mouse_dragged_fn)
{
    _ctx.mouse_dragged_fn = mouse_dragged_fn;
}

void eva_set_mouse_btn_fn(eva_mouse_btn_fn mouse_btn_fn)
{
    _ctx.mouse_btn_fn = mouse_btn_fn;
}

void eva_set_scroll_fn(eva_scroll_fn scroll_fn)
{
    _ctx.scroll_fn = scroll_fn;
}

void eva_set_key_fn(eva_key_fn key_fn)
{
    _ctx.key_fn = key_fn;
}

void eva_set_text_input_fn(eva_text_input_fn text_input_fn)
{
    _ctx.text_input_fn = text_input_fn;
}

void eva_set_window_resize_fn(eva_window_resize_fn window_resize_fn)
{
    _ctx.window_resize_fn = window_resize_fn;
}

void eva_time_init()
{
    QueryPerformanceFrequency(&_ctx.ticks_per_sec);
}

uint64_t eva_time_now()
{
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    return qpc.QuadPart;
}

uint64_t eva_time_since(uint64_t start)
{
    return eva_time_now() - start;
}

float eva_time_ms(uint64_t t)
{
    float ticks1000 = t * 1000.0f;
    float ms = ticks1000 / _ctx.ticks_per_sec.QuadPart;
    return ms;
}

float eva_time_elapsed_ms(uint64_t start, uint64_t end)
{
    float ms = eva_time_ms(end - start);
    return ms;
}

float eva_time_since_ms(uint64_t start)
{
    uint64_t now = eva_time_now();
    float ms = eva_time_elapsed_ms(start, now);
    return ms;
}

static LRESULT CALLBACK wnd_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (_ctx.window_shown)
    {
        switch (uMsg) {
            case WM_CLOSE:
                handle_close();
                break;
            case WM_DPICHANGED:
                puts("WM_DPICHANGED");
                update_window();
                try_frame();
                // Redraw?
                break;
            case WM_PAINT:
                handle_paint();
                break;
            case WM_SIZE:
                handle_resize();
                try_frame();
                break;
            case WM_MOUSEMOVE:
                if (_ctx.mouse_moved_fn) {
                    POINTS mouse_pos = MAKEPOINTS(lParam);
                    _ctx.mouse_moved_fn(mouse_pos.x, mouse_pos.y);
                    try_frame();
                }
                break;
            case WM_LBUTTONDOWN:
                if (_ctx.mouse_btn_fn) {
                    POINTS mouse_pos = MAKEPOINTS(lParam);
                    _ctx.mouse_btn_fn(mouse_pos.x, mouse_pos.y,
                                      EVA_MOUSE_BTN_LEFT, EVA_INPUT_PRESSED);
                    try_frame();
                }
                break;
            case WM_LBUTTONUP:
                if (_ctx.mouse_btn_fn) {
                    POINTS mouse_pos = MAKEPOINTS(lParam);
                    _ctx.mouse_btn_fn(mouse_pos.x, mouse_pos.y,
                                      EVA_MOUSE_BTN_LEFT, EVA_INPUT_RELEASED);
                    try_frame();
                }
                break;
            case WM_RBUTTONDOWN:
                if (_ctx.mouse_btn_fn) {
                    POINTS mouse_pos = MAKEPOINTS(lParam);
                    _ctx.mouse_btn_fn(mouse_pos.x, mouse_pos.y,
                                      EVA_MOUSE_BTN_RIGHT, EVA_INPUT_PRESSED);
                    try_frame();
                }
                break;
            case WM_RBUTTONUP:
                if (_ctx.mouse_btn_fn) {
                    POINTS mouse_pos = MAKEPOINTS(lParam);
                    _ctx.mouse_btn_fn(mouse_pos.x, mouse_pos.y,
                                      EVA_MOUSE_BTN_RIGHT, EVA_INPUT_RELEASED);
                    try_frame();
                }
                break;
            case WM_MBUTTONDOWN:
                if (_ctx.mouse_btn_fn) {
                    POINTS mouse_pos = MAKEPOINTS(lParam);
                    _ctx.mouse_btn_fn(mouse_pos.x, mouse_pos.y,
                                      EVA_MOUSE_BTN_MIDDLE, EVA_INPUT_PRESSED);
                    try_frame();
                }
                break;
            case WM_MBUTTONUP:
                if (_ctx.mouse_btn_fn) {
                    POINTS mouse_pos = MAKEPOINTS(lParam);
                    _ctx.mouse_btn_fn(mouse_pos.x, mouse_pos.y,
                                      EVA_MOUSE_BTN_MIDDLE, EVA_INPUT_RELEASED);
                    try_frame();
                }
                break;
            default:
                break;

        };
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static void update_window()
{
    INT dpi = GetDpiForWindow(_ctx.hwnd);
    _ctx.framebuffer.scale_x = (float)dpi / USER_DEFAULT_SCREEN_DPI;
    // Always the same value on windows.
    _ctx.framebuffer.scale_y = _ctx.framebuffer.scale_x; 

    // TODO: Window width/height is not really what we want. We actually
    // want content area.
    RECT rect;
    if (GetWindowRect(_ctx.hwnd, &rect)) {
        _ctx.window_width  = rect.right  - rect.left;
        _ctx.window_height = rect.bottom - rect.top;
    }

    if (GetClientRect(_ctx.hwnd, &rect)) {
        _ctx.framebuffer.w  = rect.right  - rect.left;
        _ctx.framebuffer.h = rect.bottom - rect.top;
    }

    uint32_t capacity = _ctx.framebuffer.pitch * _ctx.framebuffer.max_height;
    if (capacity == 0 ||
        _ctx.framebuffer.w > _ctx.framebuffer.pitch ||
        _ctx.framebuffer.h > _ctx.framebuffer.max_height) {

        if (_ctx.framebuffer.pixels) {
            free(_ctx.framebuffer.pixels);
        }

        // Make the framebuffer large enough to hold pixels for the entire
        // screen. This makes it unnecessary to reallocate the framebuffer
        // when the window is resized. It should only need to be resized when
        // moving to a higher resolution monitor.
        HMONITOR monitor = MonitorFromWindow(_ctx.hwnd, MONITOR_DEFAULTTOPRIMARY);

        // Get the monitors size in pixels.
        MONITORINFO mi = {
            .cbSize = sizeof(mi)
        };
        GetMonitorInfoW(monitor, &mi);

        // According to win32 docs these may be negative. The docs don't explain
        // how or when unfortunately.
        uint32_t monitor_w = max(0, mi.rcMonitor.right - mi.rcMonitor.left);
        uint32_t monitor_h = max(0, mi.rcMonitor.bottom - mi.rcMonitor.top);

        _ctx.framebuffer.pitch = max(_ctx.framebuffer.w, monitor_w);
        _ctx.framebuffer.max_height = max(_ctx.framebuffer.h, monitor_h);

        int32_t size = _ctx.framebuffer.pitch * _ctx.framebuffer.max_height;
        _ctx.framebuffer.pixels = calloc((size_t)size, sizeof(eva_pixel));
    }

    printf("window %d x %d\n", _ctx.window_width, _ctx.window_height);
    printf("framebuffer %d x %d\n", _ctx.framebuffer.w, _ctx.framebuffer.h);
    printf("framebuffer max %d x %d\n", _ctx.framebuffer.pitch, _ctx.framebuffer.max_height);
    printf("scale %.1f x %.1f\n", _ctx.framebuffer.scale_x, _ctx.framebuffer.scale_y);
}

static void handle_paint()
{
    //uint64_t start = eva_time_now();

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = _ctx.framebuffer.pitch;
    bmi.bmiHeader.biHeight = -(int32_t)_ctx.framebuffer.max_height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // Get a paint DC for current window.
    // Paint DC contains the right scaling to match
    // the monitor DPI where the window is located.
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(_ctx.hwnd, &ps);

    // Draw the framebuffer to screen
    SetDIBitsToDevice(
            hdc,
            0,                   // x dest
            0,                   // y dest
            _ctx.framebuffer.w, // width
            _ctx.framebuffer.h, // height
            0,                   // x src
            0,                    // y src
            0,                                // scanline 0
            _ctx.framebuffer.h, // n scanlines
            _ctx.framebuffer.pixels,          // buffer
            &bmi,                             // buffer info
            DIB_RGB_COLORS                    // raw colors
            );

    EndPaint(_ctx.hwnd, &ps);

    //printf("handle_paint - %.1f ms\n", eva_time_since_ms(start));
}

static void handle_close()
{
    // only give user-code a chance to intervene when eva_quit() wasn't already
    // called
    if (!_ctx.quit_ordered) {
        // if window should be closed and event handling is enabled, give user
        // code a chance to intervene via eva_cancel_quit()
        if (_ctx.cancel_quit_fn) {
            _ctx.quit_requested = !_ctx.cancel_quit_fn();
        }
        else {
            _ctx.quit_requested = true;
        }

        // user code hasn't intervened, quit the app
        if (_ctx.quit_requested) {
            _ctx.quit_ordered = true;
        }
    }
    if (_ctx.quit_ordered) {
        PostQuitMessage(0);
    }
}

static void handle_resize()
{
    update_window();
    if (_ctx.window_resize_fn) {
        _ctx.window_resize_fn(_ctx.framebuffer.w, _ctx.framebuffer.h);
    }
}

static void try_frame()
{
    if (_ctx.frame_requested) {
        if (_ctx.frame_fn) {
            _ctx.frame_fn(&_ctx.framebuffer);
        }

        InvalidateRect(_ctx.hwnd, NULL, FALSE);
        UpdateWindow(_ctx.hwnd); // Force WM_PAINT immediately
    }
}

static bool utf8_to_utf16(const char* src, wchar_t* dst, int dst_num_bytes)
{
    assert(src && dst && (dst_num_bytes > 1));
    memset(dst, 0, dst_num_bytes);

    int dst_chars = dst_num_bytes / sizeof(wchar_t);
    int dst_needed = MultiByteToWideChar(CP_UTF8, 0, src, -1, 0, 0);
    if ((dst_needed > 0) && (dst_needed < dst_chars)) {
        MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dst_chars);
        return true;
    }

    return false;
}

static bool utf16_to_utf8(const wchar_t* src, char* dst, int dst_num_bytes)
{
    assert(src && dst && (dst_num_bytes > 1));
    memset(dst, 0, dst_num_bytes);

    int32_t size = WideCharToMultiByte(
            CP_UTF8,      // Going from UTF16 -> UTF8
            0,            // dwFlags must be 0 when using CP_UTF8
            src,          // UTF16 input data
            -1,           // Input len. -1 = use null terminator for len
            dst,          // Output buffer
            dst_num_bytes,// Output buffer capacity 
            NULL, NULL);  // Always NULL for CP_UTF8

    return size != 0;
}

