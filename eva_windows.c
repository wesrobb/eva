#include "eva.h"

#include <Windows.h>

#include <assert.h>

#pragma comment(lib, "User32.lib")

static bool utf8_to_utf16(const char* src, wchar_t* dst, int dst_num_bytes);
static bool utf16_to_utf8(const wchar_t* src, char* dst, int dst_num_bytes);
LRESULT CALLBACK wnd_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

typedef struct eva_ctx {
    int32_t     window_width, window_height;
    int32_t     framebuffer_width, framebuffer_height;
    eva_pixel  *framebuffer;
    float       scale_x, scale_y; // framebuffer / window
    const char *window_title;
    bool        quit_requested;
    bool        quit_ordered;

    LARGE_INTEGER ticks_per_sec;

    eva_init_fn    *init_fn;
    eva_event_fn   *event_fn;
    eva_cleanup_fn *cleanup_fn;
    eva_fail_fn    *fail_fn;
} eva_ctx;

static eva_ctx _ctx;

void eva_run(const char     *window_title,
             eva_init_fn    *init_fn,
             eva_event_fn   *event_fn,
             eva_cleanup_fn *cleanup_fn,
             eva_fail_fn    *fail_fn)
{
    _ctx.window_title = window_title;
    _ctx.init_fn      = init_fn;
    _ctx.event_fn     = event_fn;
    _ctx.cleanup_fn   = cleanup_fn;
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
    
    HWND hwnd = CreateWindowExW(ex_style,      
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
    ShowWindow(hwnd, SW_SHOW);

    bool done = false;
    while (!(done || _ctx.quit_ordered)) {
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0)) {
            if (WM_QUIT == msg.message) {
                done = true;
                continue;
            }
            else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        /* check for window resized, this cannot happen in WM_SIZE as it explodes memory usage */
       // if (_sapp_win32_update_dimensions()) {
       //     #if defined(SOKOL_D3D11)
       //     _sapp_d3d11_resize_default_render_target();
       //     #endif
       //     _sapp_win32_app_event(SAPP_EVENTTYPE_RESIZED);
       // }
        if (_ctx.quit_requested) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
    }
    _ctx.cleanup_fn();

    DestroyWindow(hwnd);
    UnregisterClassW(L"eva", GetModuleHandleW(NULL));
}

void eva_cancel_quit()
{
    _ctx.quit_requested = false;
    _ctx.quit_ordered   = false;
}

void eva_request_frame(const eva_rect* dirty_rect)
{
    (void)dirty_rect;
}

int32_t eva_get_window_width()
{
    return _ctx.window_width;
}

int32_t eva_get_window_height()
{
    return _ctx.window_height;
}

eva_pixel *eva_get_framebuffer()
{
    return _ctx.framebuffer;
}

int32_t eva_get_framebuffer_width()
{
    return _ctx.framebuffer_width;
}

int32_t eva_get_framebuffer_height()
{
    return _ctx.framebuffer_height;
}

float eva_get_framebuffer_scale_x()
{
    return _ctx.scale_x;
}

float eva_get_framebuffer_scale_y()
{
    return _ctx.scale_y;
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
    // Convert to ms before dividing to avoid loss of precision.
    float ms = t * 1000.0f;
    return ms / _ctx.ticks_per_sec.QuadPart;
}

float eva_time_elapsed_ms(uint64_t start, uint64_t end)
{
    return eva_time_ms(end - start);
}

float eva_time_since_ms(uint64_t start)
{
    return eva_time_elapsed_ms(start, eva_time_now());
}

#define eva_min(a, b) (((a) < (b)) ? (a) : (b))
#define eva_max(a, b) (((a) > (b)) ? (a) : (b))

eva_rect eva_rect_union(const eva_rect *a, const eva_rect *b)
{
    eva_rect result;

    result.x = eva_min(a->x, b->x);
    result.y = eva_min(a->y, b->y);
    result.w = eva_max(a->x + a->w, b->x + b->w) - result.x;
    result.h = eva_max(a->y + a->h, b->y + b->h) - result.y;

    return result;
}
#undef eva_min
#undef eva_max

bool eva_rect_empty(const eva_rect *a)
{
    return a->x == 0 &&
           a->y == 0 &&
           a->w == 0 &&
           a->h == 0;
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

LRESULT CALLBACK wnd_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_CLOSE:
            // only give user-code a chance to intervene when eva_quit() wasn't already
            // called
            if (!_ctx.quit_ordered) {
                // if window should be closed and event handling is enabled, give user
                // code a chance to intervene via eva_cancel_quit()
                _ctx.quit_requested = true;

                eva_event quit_event = { 
                    .type = EVA_EVENTTYPE_QUITREQUESTED
                };
                _ctx.event_fn(&quit_event);
                // user code hasn't intervened, quit the app
                if (_ctx.quit_requested) {
                    _ctx.quit_ordered = true;
                }
            }
            if (_ctx.quit_ordered) {
                PostQuitMessage(0);
            }
            return 0;
        default:
            break;
            
    };

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
