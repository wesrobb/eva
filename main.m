#include <Cocoa/Cocoa.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct gg_pixel_t {
    uint8_t r,g,b,a;
} gg_pixel_t;

typedef struct gg_app_ctx_t {
    int32_t window_width, window_height;
    int32_t framebuffer_width, framebuffer_height;
    gg_pixel_t *framebuffer;
    float scale_x, scale_y; // framebuffer / window
    const char *window_title;
    bool quit_requested;
    bool quit_ordered;
    bool valid;
} gg_app_ctx_t;

typedef void(frame_fn_t)(gg_pixel_t* framebuffer, int32_t framebuffer_width, int32_t framebuffer_height);
typedef void(init_fn_t)(void);
typedef void(cleanup_fn_t)(void);

static void gg_app_start(init_fn_t init_fn, cleanup_fn_t cleanup_fn, frame_fn_t *frame_fn);

@interface gg_app_delegate_t : NSObject<NSApplicationDelegate>
@end
@interface gg_app_window_delegate_t : NSObject<NSWindowDelegate>
@end
@interface gg_app_view_t : NSView
{
    NSTrackingArea* trackingArea;
}
- (void)drawRect:(NSRect)bounds;
@end

static gg_app_delegate_t *_app_delegate;
static gg_app_ctx_t _app_ctx;
static NSWindow* _app_window;
static gg_app_window_delegate_t* _app_window_delegate;
static gg_app_view_t* _app_view;

int main(int argc, char **argv) {
    printf("Hello, AppKit\n");

    _app_ctx.window_title = "Hello, AppKit!";

    [NSApplication sharedApplication];
    NSApp.activationPolicy = NSApplicationActivationPolicyRegular;
    _app_delegate = [[gg_app_delegate_t alloc] init];
    NSApp.delegate = _app_delegate;
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];

    return 0;
}

static void gg_app_update_dimensions(void) {
    NSRect window_bounds = _app_window.contentView.bounds;
    NSRect backing_bounds = [_app_window convertRectToBacking:window_bounds];
    
    _app_ctx.window_width = window_bounds.size.width;
    _app_ctx.window_height = window_bounds.size.height;
    
    _app_ctx.scale_x = backing_bounds.size.width / window_bounds.size.width;
    _app_ctx.scale_y = backing_bounds.size.height / window_bounds.size.height;
    
    _app_ctx.framebuffer_width = (int32_t)(_app_ctx.window_width * _app_ctx.scale_x);
    _app_ctx.framebuffer_height = (int32_t)(_app_ctx.window_height * _app_ctx.scale_y);
    if (_app_ctx.framebuffer)
    {
        free(_app_ctx.framebuffer);
    }
    _app_ctx.framebuffer = calloc(_app_ctx.framebuffer_width * _app_ctx.framebuffer_height, sizeof(gg_pixel_t));
    
    printf("Window Width %d\n", _app_ctx.window_width);
    printf("Window Height %d\n", _app_ctx.window_height);
    printf("FrameBuffer Width %d\n", _app_ctx.framebuffer_width);
    printf("FrameBuffer Height %d\n", _app_ctx.framebuffer_height);
    printf("Backing Scale Factor %.1f\n", _app_ctx.scale_x);
}

@implementation gg_app_delegate_t
- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
    const NSUInteger style =
        NSWindowStyleMaskTitled |
        NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable |
        NSWindowStyleMaskResizable;

    NSRect screen_rect = NSScreen.mainScreen.frame;
    NSRect window_rect = NSMakeRect(0, 0, screen_rect.size.width * 0.8, screen_rect.size.height * 0.8);
    _app_window = [[NSWindow alloc]
        initWithContentRect:window_rect
        styleMask:style
        backing:NSBackingStoreBuffered
        defer:NO];
    
    _app_window.title = [NSString stringWithUTF8String:_app_ctx.window_title];
    _app_window.acceptsMouseMovedEvents = YES;
    _app_window.restorable = YES;
    
    gg_app_update_dimensions();

    _app_window_delegate = [[gg_app_window_delegate_t alloc] init];
    _app_window.delegate = _app_window_delegate;

    //CGRect viewRect = CGRectMake(0, 0, _app_ctx.framebuffer_width, _app_ctx.framebuffer_height);
    _app_view = [[gg_app_view_t alloc] init];
    [_app_view setWantsLayer:YES];
    [_app_view updateTrackingAreas];
    _app_window.contentView = _app_view;
    [_app_window  makeFirstResponder:_app_view];
    [_app_window center];
    [_app_window makeKeyAndOrderFront:nil];
    [_app_view setNeedsDisplay:YES];
    _app_ctx.valid = true;
}
@end

@implementation gg_app_window_delegate_t
- (BOOL)windowShouldClose:(id)sender {
    /* only give user-code a chance to intervene when sapp_quit() wasn't already called */
    if (!_app_ctx.quit_ordered) {
        /* if window should be closed and event handling is enabled, give user code
           a chance to intervene via sapp_cancel_quit()
        */
        _app_ctx.quit_requested = true;
        //_sapp_macos_app_event(SAPP_EVENTTYPE_QUIT_REQUESTED);
        /* user code hasn't intervened, quit the app */
        if (_app_ctx.quit_requested) {
            _app_ctx.quit_ordered = true;
        }
    }
    if (_app_ctx.quit_ordered) {
        //_sapp_call_cleanup();
        return YES;
    }
    else {
        return NO;
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)windowDidResize:(NSNotification*)notification {
    puts("Window resized");
    gg_app_update_dimensions();
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
    puts("Window minimized");
    gg_app_update_dimensions();
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
    puts("Window maximized");
    gg_app_update_dimensions();
}
@end

@implementation gg_app_view_t
- (void)drawRect:(NSRect)bound {
    // Draw here
    printf("Drawing bounds are: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n", bound.origin.x, bound.origin.y, bound.size.width, bound.size.height);
    printf("Drawing in framebuffer %d x %d", _app_ctx.framebuffer_width, _app_ctx.framebuffer_height);
    // Get current context
    CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] CGContext];

    // Colorspace RGB
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    for (int j = 0; j < _app_ctx.framebuffer_height / 2; j++) {
        for (int i = 0; i < _app_ctx.framebuffer_width / 2; i++) {
            gg_pixel_t red = {
                .r = 255,
                .g = 0,
                .b = 0,
                .a = 255
            };
            _app_ctx.framebuffer[i + j*_app_ctx.framebuffer_width] = red;
        }
    }

    // Provider
    uint32_t size = _app_ctx.framebuffer_width * _app_ctx.framebuffer_height * sizeof(gg_pixel_t);
    CGDataProviderRef provider = CGDataProviderCreateWithData(nil, _app_ctx.framebuffer, size, nil);

    // CGImage
    CGImageRef image = CGImageCreate(_app_ctx.framebuffer_width,
                                     _app_ctx.framebuffer_height,
                                     8,
                                     32,
                                     4*_app_ctx.framebuffer_width,
                                     colorSpace,
                                     kCGBitmapByteOrder32Little,
                                     provider,
                                     nil, //No decode
                                     NO,  //No interpolation
                                     kCGRenderingIntentDefault); // Default rendering

    // Draw
    CGContextDrawImage(context, self.bounds, image);

    // Once everything is written on screen we can release everything
    CGImageRelease(image);
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
}
- (void)viewDidChangeBackingProperties {
    puts("Change backing properties");
    gg_app_update_dimensions();
}
- (BOOL)isOpaque {
    return YES;
}
- (BOOL)canBecomeKey {
    return YES;
}
- (BOOL)acceptsFirstResponder {
    return YES;
}
- (void)updateTrackingAreas {
    if (trackingArea != nil) {
        [self removeTrackingArea:trackingArea];
        trackingArea = nil;
    }
    const NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited |
                                          NSTrackingActiveInKeyWindow |
                                          NSTrackingEnabledDuringMouseDrag |
                                          NSTrackingCursorUpdate |
                                          NSTrackingInVisibleRect |
                                          NSTrackingAssumeInside;
    trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds] options:options owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
    [super updateTrackingAreas];
}
- (void)mouseEntered:(NSEvent*)event {
}
- (void)mouseExited:(NSEvent*)event {
}
- (void)mouseDown:(NSEvent*)event {
}
- (void)mouseUp:(NSEvent*)event {
}
- (void)rightMouseDown:(NSEvent*)event {
}
- (void)rightMouseUp:(NSEvent*)event {
}
- (void)otherMouseDown:(NSEvent*)event {
}
- (void)otherMouseUp:(NSEvent*)event {
}
- (void)mouseMoved:(NSEvent*)event {
}
- (void)mouseDragged:(NSEvent*)event {
}
- (void)rightMouseDragged:(NSEvent*)event {
}
- (void)scrollWheel:(NSEvent*)event {
}
- (void)keyDown:(NSEvent*)event {
}
- (void)keyUp:(NSEvent*)event {
}
- (void)flagsChanged:(NSEvent*)event {
}
- (void)cursorUpdate:(NSEvent*)event {
}
@end
