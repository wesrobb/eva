#include "eva.h"

#include <stdbool.h>

#import <Cocoa/Cocoa.h>


@interface eva_app_delegate : NSObject <NSApplicationDelegate>
@end
@interface eva_window_delegate : NSObject <NSWindowDelegate>
@end
@interface eva_view : NSView {
    NSTrackingArea *trackingArea;
}
- (void)drawRect:(NSRect)dirtyRect;
@end

void init_mouse_event(eva_event *e, eva_mouse_event_type type);

typedef struct eva_ctx {
    int32_t     window_width, window_height;
    int32_t     framebuffer_width, framebuffer_height;
    eva_pixel  *framebuffer;
    float       scale_x, scale_y; // framebuffer / window
    const char *window_title;
    bool        quit_requested;
    bool        quit_ordered;

    eva_init_fn    *init_fn;
    eva_event_fn   *event_fn;
    eva_cleanup_fn *cleanup_fn;
    eva_fail_fn    *fail_fn;
} eva_ctx;

static eva_ctx _ctx;

static eva_app_delegate    *_app_delegate;
static NSWindow            *_app_window;
static eva_window_delegate *_app_window_delegate;
static eva_view            *_app_view;

void eva_run(const char     *window_title,
             eva_init_fn    *init_fn,
             eva_event_fn   *event_fn,
             eva_cleanup_fn *cleanup_fn,
             eva_fail_fn    *fail_fn)
{
    // Usually time init would go first but macos's
    // timers require no initialization.

    _ctx.window_title = window_title;
    _ctx.init_fn      = init_fn;
    _ctx.event_fn     = event_fn;
    _ctx.cleanup_fn   = cleanup_fn;
    _ctx.fail_fn      = fail_fn;

    [NSApplication sharedApplication];
    NSApp.activationPolicy = NSApplicationActivationPolicyRegular;
    _app_delegate          = [[eva_app_delegate alloc] init];
    NSApp.delegate         = _app_delegate;
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
}

void eva_cancel_quit()
{
    _ctx.quit_requested = false;
    _ctx.quit_ordered   = false;
}

void eva_request_frame(const eva_rect *dirty_rect)
{
    if (dirty_rect) {
        if (!eva_rect_empty(dirty_rect)) {

            printf("request_frame x=%d, y=%d, w=%d, h=%d\n",
                   dirty_rect->x,
                   dirty_rect->y,
                   dirty_rect->w,
                   dirty_rect->h);
            NSRect r = NSMakeRect(dirty_rect->x, dirty_rect->y,
                                  dirty_rect->w, dirty_rect->h);
            [_app_view setNeedsDisplayInRect:r];
        }
    }
    else {
        puts("request_full_frame");
        [_app_view setNeedsDisplay:YES];
    }
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

static void update_window(void)
{
    NSRect content_bounds  = _app_window.contentView.bounds;
    NSRect backing_bounds = [_app_window convertRectToBacking:content_bounds];

    _ctx.window_width  = (int32_t)content_bounds.size.width;
    _ctx.window_height = (int32_t)content_bounds.size.height;

    _ctx.scale_x = (float)(backing_bounds.size.width / content_bounds.size.width);
    _ctx.scale_y = (float)(backing_bounds.size.height / content_bounds.size.height);

    _ctx.framebuffer_width  = (int32_t)(_ctx.window_width * _ctx.scale_x);
    _ctx.framebuffer_height = (int32_t)(_ctx.window_height * _ctx.scale_y);
    if (_ctx.framebuffer) {
        free(_ctx.framebuffer);
    }

    int32_t size = _ctx.framebuffer_width * _ctx.framebuffer_height;
    _ctx.framebuffer = calloc((size_t)size, sizeof(eva_pixel));
}

@implementation eva_app_delegate
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Setup window
    const NSUInteger style =
        NSWindowStyleMaskTitled |
        NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable |
        NSWindowStyleMaskResizable;

    NSRect screen_rect = NSScreen.mainScreen.frame;
    NSRect window_rect = NSMakeRect(0, 0,
                                    screen_rect.size.width * 0.8,
                                    screen_rect.size.height * 0.8);

    _app_window = [[NSWindow alloc] initWithContentRect:window_rect
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];

    _app_window.title = [NSString stringWithUTF8String:_ctx.window_title];
    _app_window.acceptsMouseMovedEvents = YES;
    _app_window.restorable              = YES;

    update_window();
    _ctx.init_fn();

    // Setup window delegate
    _app_window_delegate = [[eva_window_delegate alloc] init];
    _app_window.delegate = _app_window_delegate;

    // Setup view
    _app_view = [[eva_view alloc] init];
    _app_view.wantsLayer = YES;
    [_app_view updateTrackingAreas];

    // Assign view to window
    _app_window.contentView = _app_view;
    [_app_window makeFirstResponder:_app_view];
    [_app_window center];
    [_app_window makeKeyAndOrderFront:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}
@end

@implementation eva_window_delegate
- (BOOL)windowShouldClose:(id)sender
{
    // only give user-code a chance to intervene when eva_quit() wasn't already
    // called
    if (!_ctx.quit_ordered) {
        // if window should be closed and event handling is enabled, give user
        // code a chance to intervene via eva_cancel_quit()
        _ctx.quit_requested = true;

        eva_event quit_event = { .type = EVA_EVENTTYPE_QUITREQUESTED };
        _ctx.event_fn(&quit_event);
        // user code hasn't intervened, quit the app
        if (_ctx.quit_requested) {
            _ctx.quit_ordered = true;
        }
    }
    if (_ctx.quit_ordered) {
        _ctx.cleanup_fn();
        return YES;
    } else {
        return NO;
    }
}

- (void)windowDidResize:(NSNotification *)notification
{
    update_window();

    eva_event event = {
        .type                      = EVA_EVENTTYPE_WINDOW,
        .window.window_width       = _ctx.window_width,
        .window.window_height      = _ctx.window_height,
        .window.framebuffer_width  = _ctx.framebuffer_width,
        .window.framebuffer_height = _ctx.framebuffer_height,
        .window.scale_x            = _ctx.scale_x,
        .window.scale_y            = _ctx.scale_y,
    };
    _ctx.event_fn(&event);
}

- (void)windowDidMiniaturize:(NSNotification *)notification
{
    update_window();
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    update_window();
}
@end

@implementation eva_view
- (void)drawRect:(NSRect)dirtyRect
{
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];

    eva_rect dirty_rect = {
        .x = (int32_t)(dirtyRect.origin.x    * _ctx.scale_x),
        .y = (int32_t)(dirtyRect.origin.y    * _ctx.scale_y),
        .w = (int32_t)(dirtyRect.size.width  * _ctx.scale_x),
        .h = (int32_t)(dirtyRect.size.height * _ctx.scale_y),
    };

    printf("drawRect x=%d, y=%d, w=%d, h=%d\n",
           dirty_rect.x,
           dirty_rect.y,
           dirty_rect.w,
           dirty_rect.h);

    int32_t size = _ctx.framebuffer_width * 
                   _ctx.framebuffer_height *
                   (int32_t)sizeof(eva_pixel);
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        nil, _ctx.framebuffer, (uint32_t)size, nil
    );

    CGImageRef image =
        CGImageCreate((size_t)_ctx.framebuffer_width,
                      (size_t)_ctx.framebuffer_height,
                      8,
                      32,
                      sizeof(eva_pixel) * (size_t)_ctx.framebuffer_width,
                      _app_window.screen.colorSpace.CGColorSpace,
                      kCGBitmapByteOrder32Big,
                      provider,
                      nil,                        // No decode
                      NO,                         // No interpolation
                      kCGRenderingIntentDefault); // Default rendering

    CGRect cg_rect = CGRectMake(dirty_rect.x,
                                dirty_rect.y,
                                dirty_rect.w,
                                dirty_rect.h);
    CGImageRef subImage = CGImageCreateWithImageInRect(image, cg_rect);

    CGContextDrawImage(context, dirtyRect, subImage);

    CGImageRelease(subImage);
    CGImageRelease(image);
    CGDataProviderRelease(provider);
}
- (void)viewDidChangeBackingProperties
{
    update_window();

    eva_event event = {
        .type = EVA_EVENTTYPE_REDRAWFRAME,
    };
    _ctx.event_fn(&event);
}
- (BOOL)isOpaque
{
    return YES;
}
- (BOOL)canBecomeKey
{
    return YES;
}
- (BOOL)acceptsFirstResponder
{
    return YES;
}
- (void)updateTrackingAreas
{
    if (trackingArea != nil) {
        [self removeTrackingArea:trackingArea];
        trackingArea = nil;
    }
    const NSTrackingAreaOptions options =
        NSTrackingMouseEnteredAndExited  |
        NSTrackingActiveInKeyWindow      |
        NSTrackingEnabledDuringMouseDrag |
        NSTrackingCursorUpdate           |
        NSTrackingInVisibleRect          |
        NSTrackingAssumeInside;

    trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                options:options
                                                  owner:self
                                               userInfo:nil];
    [self addTrackingArea:trackingArea];
    [super updateTrackingAreas];
}
- (void)mouseEntered:(NSEvent *)event
{
    eva_event e;
    init_mouse_event(&e, EVA_MOUSE_EVENTTYPE_MOUSE_ENTERED);
    _ctx.event_fn(&e);
}
- (void)mouseExited:(NSEvent *)event
{
    eva_event e;
    init_mouse_event(&e, EVA_MOUSE_EVENTTYPE_MOUSE_EXITED);
    _ctx.event_fn(&e);
}
- (void)mouseDown:(NSEvent *)event
{
    eva_event e;
    init_mouse_event(&e, EVA_MOUSE_EVENTTYPE_MOUSE_PRESSED);
    e.mouse.left_btn_pressed = true;
    _ctx.event_fn(&e);
}
- (void)mouseUp:(NSEvent *)event
{
    eva_event e;
    init_mouse_event(&e, EVA_MOUSE_EVENTTYPE_MOUSE_RELEASED);
    e.mouse.left_btn_released = true;
    _ctx.event_fn(&e);
}
- (void)rightMouseDown:(NSEvent *)event
{
    eva_event e;
    init_mouse_event(&e, EVA_MOUSE_EVENTTYPE_MOUSE_PRESSED);
    e.mouse.right_btn_pressed = true;
    _ctx.event_fn(&e);
}
- (void)rightMouseUp:(NSEvent *)event
{
    eva_event e;
    init_mouse_event(&e, EVA_MOUSE_EVENTTYPE_MOUSE_RELEASED);
    e.mouse.right_btn_released = true;
    _ctx.event_fn(&e);
}
- (void)otherMouseDown:(NSEvent *)event
{
    eva_event e;
    init_mouse_event(&e, EVA_MOUSE_EVENTTYPE_MOUSE_PRESSED);
    e.mouse.middle_btn_pressed = true;
    _ctx.event_fn(&e);
}
- (void)otherMouseUp:(NSEvent *)event
{
    eva_event e;
    init_mouse_event(&e, EVA_MOUSE_EVENTTYPE_MOUSE_RELEASED);
    e.mouse.middle_btn_released = true;
    _ctx.event_fn(&e);
}
- (void)mouseMoved:(NSEvent *)event
{
    eva_event e;
    init_mouse_event(&e, EVA_MOUSE_EVENTTYPE_MOUSE_MOVED);
    _ctx.event_fn(&e);
}
- (void)mouseDragged:(NSEvent *)event
{
    eva_event e;
    init_mouse_event(&e, EVA_MOUSE_EVENTTYPE_MOUSE_MOVED);
    _ctx.event_fn(&e);
}
- (void)rightMouseDragged:(NSEvent *)event
{
    eva_event e;
    init_mouse_event(&e, EVA_MOUSE_EVENTTYPE_MOUSE_MOVED);
    _ctx.event_fn(&e);
}
- (void)scrollWheel:(NSEvent *)event
{
}
- (void)keyDown:(NSEvent *)event
{
    NSString *characters = event.charactersIgnoringModifiers;

    eva_event e = {
        .type = EVA_EVENTTYPE_KB,
        .kb.type = EVA_KB_EVENTTYPE_KEYDOWN,
        .kb.utf8_codepoint = characters.UTF8String,
    };

    _ctx.event_fn(&e);
}
- (void)keyUp:(NSEvent *)event
{
}
- (void)flagsChanged:(NSEvent *)event
{
}
- (void)cursorUpdate:(NSEvent *)event
{
}
@end

// time

#include <time.h>

void eva_time_init()
{
    // No-op on macos.
}

uint64_t eva_time_now()
{
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

uint64_t eva_time_since(uint64_t start)
{
    return eva_time_now() - start;
}

float eva_time_ms(uint64_t t)
{
    return t / 1000000.0f;
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

void init_mouse_event(eva_event *e, eva_mouse_event_type type)
{
    NSPoint mouse_position = _app_window.mouseLocationOutsideOfEventStream;

    memset(e, 0, sizeof(*e));
    e->type          = EVA_EVENTTYPE_MOUSE;
    e->mouse.type    = type;
    e->mouse.mouse_x = (int32_t)mouse_position.x;
    e->mouse.mouse_y = (int32_t)mouse_position.y;
}
