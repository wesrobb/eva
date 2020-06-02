#import <Cocoa/Cocoa.h>

#include "eva.h"

@interface eva_app_delegate : NSObject <NSApplicationDelegate>
@end
@interface eva_window_delegate : NSObject <NSWindowDelegate>
@end
@interface eva_view : NSView {
    NSTrackingArea *trackingArea;
}
- (void)drawRect:(NSRect)bounds;
@end
@interface eva_layer_delegate : NSObject <CALayerDelegate>
@end


typedef struct eva_ctx {
    int32_t     window_width, window_height;
    int32_t     framebuffer_width, framebuffer_height;
    eva_pixel * framebuffer;
    float       scale_x, scale_y; // framebuffer / window
    const char *window_title;
    bool        quit_requested;
    bool        quit_ordered;

    eva_init_fn *   init_fn;
    eva_frame_fn *  frame_fn;
    eva_event_fn *  event_fn;
    eva_cleanup_fn *cleanup_fn;
    eva_fail_fn *   fail_fn;
} eva_ctx;

// Global variables
static eva_ctx              _ctx;
static eva_app_delegate *   _app_delegate;
static NSWindow *           _app_window;
static eva_window_delegate *_app_window_delegate;
static eva_view *           _app_view;

void eva_run(const char *    window_title,
             eva_init_fn *   init_fn,
             eva_frame_fn *  frame_fn,
             eva_event_fn *  event_fn,
             eva_cleanup_fn *cleanup_fn,
             eva_fail_fn *   fail_fn)
{
    // Usually time init would go first but macos's
    // timers require no initialization.

    _ctx.window_title = window_title;
    _ctx.init_fn      = init_fn;
    _ctx.frame_fn     = frame_fn;
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

void eva_request_frame()
{
    [_app_view setNeedsDisplay:YES];
}

int32_t eva_get_window_width()
{
    return _ctx.window_width;
}

int32_t eva_get_window_height()
{
    return _ctx.window_height;
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

static void eva_update_window(void)
{
    NSRect window_bounds  = _app_window.contentView.bounds;
    NSRect backing_bounds = [_app_window convertRectToBacking:window_bounds];

    _ctx.window_width  = (int32_t)window_bounds.size.width;
    _ctx.window_height = (int32_t)window_bounds.size.height;

    _ctx.scale_x =
        (float)(backing_bounds.size.width / window_bounds.size.width);
    _ctx.scale_y =
        (float)(backing_bounds.size.height / window_bounds.size.height);

    _ctx.framebuffer_width  = (int32_t)(_ctx.window_width * _ctx.scale_x);
    _ctx.framebuffer_height = (int32_t)(_ctx.window_height * _ctx.scale_y);
    if (_ctx.framebuffer) {
        free(_ctx.framebuffer);
    }

    int32_t size     = _ctx.framebuffer_width * _ctx.framebuffer_height;
    _ctx.framebuffer = calloc((size_t)size, sizeof(eva_pixel));
}

@implementation eva_app_delegate
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Setup window
    const NSUInteger style =
        NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

    NSRect screen_rect = NSScreen.mainScreen.frame;
    NSRect window_rect = NSMakeRect(
        0, 0, screen_rect.size.width * 0.8, screen_rect.size.height * 0.8);
    _app_window = [[NSWindow alloc] initWithContentRect:window_rect
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];

    _app_window.title = [NSString stringWithUTF8String:_ctx.window_title];
    _app_window.acceptsMouseMovedEvents = YES;
    _app_window.restorable              = YES;

    eva_update_window();
    _ctx.init_fn();

    // Setup window delegate
    _app_window_delegate = [[eva_window_delegate alloc] init];
    _app_window.delegate = _app_window_delegate;

    // Setup view
    _app_view = [[eva_view alloc] init];
    _app_view.wantsLayer = YES;
    [_app_view updateTrackingAreas];
    _app_view.layer.delegate = [[eva_layer_delegate alloc] init];

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
    // only give user-code a chance to intervene when sapp_quit() wasn't already
    // called
    if (!_ctx.quit_ordered) {
        // if window should be closed and event handling is enabled, give user
        // code a chance to intervene via eva_cancel_quit()
        _ctx.quit_requested = true;

        eva_event quit_event = { .type = EVA_EVENTTYPE_QUITREQUESTED };
        _ctx.event_fn(&quit_event);
        /* user code hasn't intervened, quit the app */
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
    eva_update_window();

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
    eva_update_window();
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    eva_update_window();
}
@end

@implementation eva_view
- (void)drawRect:(NSRect)bound
{
}
- (void)viewDidChangeBackingProperties
{
    eva_update_window();
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
        NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow |
        NSTrackingEnabledDuringMouseDrag | NSTrackingCursorUpdate |
        NSTrackingInVisibleRect | NSTrackingAssumeInside;
    trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                options:options
                                                  owner:self
                                               userInfo:nil];
    [self addTrackingArea:trackingArea];
    [super updateTrackingAreas];
}
- (void)mouseEntered:(NSEvent *)event
{
}
- (void)mouseExited:(NSEvent *)event
{
}
- (void)mouseDown:(NSEvent *)event
{
    NSPoint mouse_position = _app_window.mouseLocationOutsideOfEventStream;

    eva_event e = { .type       = EVA_EVENTTYPE_MOUSE,
                    .mouse.type = EVA_MOUSE_EVENTTYPE_MOUSE_PRESSED,
                    .mouse.x    = (int32_t)mouse_position.x,
                    .mouse.y    = (int32_t)mouse_position.y,
                    .mouse.left_button_pressed = true };
    _ctx.event_fn(&e);
}
- (void)mouseUp:(NSEvent *)event
{
    NSPoint mouse_position = _app_window.mouseLocationOutsideOfEventStream;

    eva_event e = { .type       = EVA_EVENTTYPE_MOUSE,
                    .mouse.type = EVA_MOUSE_EVENTTYPE_MOUSE_RELEASED,
                    .mouse.x    = (int32_t)mouse_position.x,
                    .mouse.y    = (int32_t)mouse_position.y,
                    .mouse.left_button_released = true };
    _ctx.event_fn(&e);
}
- (void)rightMouseDown:(NSEvent *)event
{
    NSPoint mouse_position = _app_window.mouseLocationOutsideOfEventStream;

    eva_event e = { .type       = EVA_EVENTTYPE_MOUSE,
                    .mouse.type = EVA_MOUSE_EVENTTYPE_MOUSE_RELEASED,
                    .mouse.x    = (int32_t)mouse_position.x,
                    .mouse.y    = (int32_t)mouse_position.y,
                    .mouse.right_button_pressed = true };
    _ctx.event_fn(&e);
}
- (void)rightMouseUp:(NSEvent *)event
{
    NSPoint mouse_position = _app_window.mouseLocationOutsideOfEventStream;

    eva_event e = { .type       = EVA_EVENTTYPE_MOUSE,
                    .mouse.type = EVA_MOUSE_EVENTTYPE_MOUSE_RELEASED,
                    .mouse.x    = (int32_t)mouse_position.x,
                    .mouse.y    = (int32_t)mouse_position.y,
                    .mouse.right_button_released = true };
    _ctx.event_fn(&e);
}
- (void)otherMouseDown:(NSEvent *)event
{
    NSPoint mouse_position = _app_window.mouseLocationOutsideOfEventStream;

    eva_event e = { .type       = EVA_EVENTTYPE_MOUSE,
                    .mouse.type = EVA_MOUSE_EVENTTYPE_MOUSE_RELEASED,
                    .mouse.x    = (int32_t)mouse_position.x,
                    .mouse.y    = (int32_t)mouse_position.y,
                    .mouse.middle_button_pressed = true };
    _ctx.event_fn(&e);
}
- (void)otherMouseUp:(NSEvent *)event
{
    NSPoint mouse_position = _app_window.mouseLocationOutsideOfEventStream;

    eva_event e = { .type       = EVA_EVENTTYPE_MOUSE,
                    .mouse.type = EVA_MOUSE_EVENTTYPE_MOUSE_RELEASED,
                    .mouse.x    = (int32_t)mouse_position.x,
                    .mouse.y    = (int32_t)mouse_position.y,
                    .mouse.middle_button_released = true };
    _ctx.event_fn(&e);
}
- (void)mouseMoved:(NSEvent *)event
{
    NSPoint mouse_position = _app_window.mouseLocationOutsideOfEventStream;

    eva_event e = { .type       = EVA_EVENTTYPE_MOUSE,
                    .mouse.type = EVA_MOUSE_EVENTTYPE_MOUSE_MOVED,
                    .mouse.x    = (int32_t)mouse_position.x,
                    .mouse.y    = (int32_t)mouse_position.y };
    _ctx.event_fn(&e);
}
- (void)mouseDragged:(NSEvent *)event
{
    NSPoint mouse_position = _app_window.mouseLocationOutsideOfEventStream;

    eva_event e = { .type       = EVA_EVENTTYPE_MOUSE,
                    .mouse.type = EVA_MOUSE_EVENTTYPE_MOUSE_MOVED,
                    .mouse.x    = (int32_t)mouse_position.x,
                    .mouse.y    = (int32_t)mouse_position.y };
    _ctx.event_fn(&e);
}
- (void)rightMouseDragged:(NSEvent *)event
{
    NSPoint mouse_position = _app_window.mouseLocationOutsideOfEventStream;

    eva_event e = { .type       = EVA_EVENTTYPE_MOUSE,
                    .mouse.type = EVA_MOUSE_EVENTTYPE_MOUSE_MOVED,
                    .mouse.x    = (int32_t)mouse_position.x,
                    .mouse.y    = (int32_t)mouse_position.y };
    _ctx.event_fn(&e);
}
- (void)scrollWheel:(NSEvent *)event
{
}
- (void)keyDown:(NSEvent *)event
{
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

@implementation eva_layer_delegate
- (void)displayLayer:(CALayer *)layer 
{
    uint64_t start = eva_time_now();

    printf("DrawRect Init %.2f\n", eva_time_since_ms(start));
    start = eva_time_now();

    eva_rect dirty_rect;
    _ctx.frame_fn(_ctx.framebuffer,
                  _ctx.framebuffer_width,
                  _ctx.framebuffer_height,
                  _ctx.scale_x,
                  _ctx.scale_y,
                  &dirty_rect);

    printf("DrawRect Frame %.2f\n", eva_time_since_ms(start));
    start = eva_time_now();


    @autoreleasepool {
        NSBitmapImageRep *rep = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes: (uint8_t**)&_ctx.framebuffer 
                                  pixelsWide: _ctx.framebuffer_width
                                  pixelsHigh: _ctx.framebuffer_height
                                  bitsPerSample: 8
                                  samplesPerPixel: 4
                                  hasAlpha: YES
                                  isPlanar: NO
                                  colorSpaceName: NSDeviceRGBColorSpace
                                  bytesPerRow: (uint32_t)_ctx.framebuffer_width * sizeof(eva_pixel)
                                  bitsPerPixel: sizeof(eva_pixel) * 8] autorelease];

        NSSize imageSize = NSMakeSize(_ctx.framebuffer_width, _ctx.framebuffer_height);
        NSImage *image = [[[NSImage alloc] initWithSize: imageSize] autorelease];
        [image addRepresentation: rep];
        layer.contents = image;
    }

    printf("DrawRect NS %.2f\n", eva_time_since_ms(start));
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

double eva_time_ms(uint64_t t)
{
    return t / 1000000.0f;
}

double eva_time_elapsed_ms(uint64_t start, uint64_t end)
{
    return eva_time_ms(end - start);
}

double eva_time_since_ms(uint64_t start)
{
    return eva_time_elapsed_ms(start, eva_time_now());
}
