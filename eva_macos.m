#include "eva.h"

#include <stdbool.h>

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

#define eva_shader(inc, src)    @inc#src

NSString *_shader_src = eva_shader(
    "#include <metal_stdlib>\n",
    using namespace metal;

    struct vert_out {
        float4 pos [[position]];
        float2 tex_coord;
    };

    struct vert_in {
        float4 pos [[position]];
    };

    vertex vert_out
    vert_shader(unsigned int vert_id[[vertex_id]], const device vert_in *pos [[ buffer(0) ]]) {
        vert_out out;

        out.pos = pos[vert_id].pos;

        out.tex_coord.x = (float) (vert_id / 2);
        out.tex_coord.y = 1.0 - (float) (vert_id % 2);

        return out;
    }

    fragment float4
    frag_shader(vert_out input [[stage_in]], texture2d<half> framebuffer [[ texture(0) ]]) {
        constexpr sampler tex_sampler(mag_filter::nearest, min_filter::nearest);

        // Sample the framebuffer to obtain a color
        const half4 sample = framebuffer.sample(tex_sampler, input.tex_coord);

        return float4(sample);
    };
);

@interface eva_app_delegate : NSObject <NSApplicationDelegate>
@end
@interface eva_window_delegate : NSObject <NSWindowDelegate>
@end
@interface eva_view : MTKView {
    NSTrackingArea *trackingArea;
}
@end
@interface eva_view_delegate : NSViewController<MTKViewDelegate>
@end

void init_mouse_event(eva_event *e, eva_mouse_event_type type);

#define EVA_MAX_MTL_BUFFERS 2
typedef struct eva_ctx {
    uint32_t window_width, window_height;
    uint32_t framebuffer_width, framebuffer_height;
    float    scale_x, scale_y; // framebuffer / window

    eva_pixel  *framebuffer;
    const char *window_title;
    bool        quit_requested;
    bool        quit_ordered;

    eva_init_fn    *init_fn;
    eva_event_fn   *event_fn;
    eva_cleanup_fn *cleanup_fn;
    eva_fail_fn    *fail_fn;

    id<MTLLibrary>      mtl_library;
    id<MTLDevice>       mtl_device;
    id<MTLCommandQueue> mtl_cmd_queue;
    id<MTLRenderPipelineState>  mtl_pipe_state;
    id<MTLTexture>      mtl_textures[EVA_MAX_MTL_BUFFERS];
    int8_t mtl_texture_index;

    dispatch_semaphore_t semaphore; // Used for syncing with CPU/GPU
} eva_ctx;

typedef struct eva_vertex {
    float x, y, z, w;
} eva_vertex;

static eva_vertex _vertices[4] = {
    {-1.0, -1.0, 0, 1},
    {-1.0,  1.0, 0, 1},
    { 1.0, -1.0, 0, 1},
    { 1.0,  1.0, 0, 1},
};

static eva_ctx _ctx;

static eva_app_delegate    *_app_delegate;
static NSWindow            *_app_window;
static eva_window_delegate *_app_window_delegate;
static eva_view            *_app_view;

static bool create_shaders()
{
    NSError *error = 0x0;
    _ctx.mtl_library = [_ctx.mtl_device newLibraryWithSource:_shader_src
                                                     options:[[MTLCompileOptions alloc] init]
                                                       error:&error
    ];
    if (!error || _ctx.mtl_library) {
        id<MTLFunction> vertex_shader_func   = 
            [_ctx.mtl_library newFunctionWithName:@"vert_shader"];
        if (vertex_shader_func) {
            id<MTLFunction> fragment_shader_func = 
                [_ctx.mtl_library newFunctionWithName:@"frag_shader"];
            if (fragment_shader_func) {
                // Create a reusable pipeline state
                MTLRenderPipelineDescriptor *pipe_desc = [[MTLRenderPipelineDescriptor alloc] init];
                pipe_desc.label = @"eva_mtl_pipeline";
                pipe_desc.vertexFunction = vertex_shader_func;
                pipe_desc.fragmentFunction = fragment_shader_func;
                pipe_desc.colorAttachments[0].pixelFormat =
                    MTLPixelFormatBGRA8Unorm;

                _ctx.mtl_pipe_state = [_ctx.mtl_device newRenderPipelineStateWithDescriptor:pipe_desc error:&error];
                if (_ctx.mtl_pipe_state) {
                    return true;
                }
                else {
                    _ctx.fail_fn(0, "Failed to metal pipeline state"); // TODO: Error codes
                    return false;
                }
            }
            else {
                _ctx.fail_fn(0, "Failed to get frag_shader function");
                return false;
            }
        }
        else {
            _ctx.fail_fn(0, "Failed to get vert_shader function");
            return false;
        }
    } else {
        _ctx.fail_fn(0, "Failed to create metal shader library");
        return false;
    }
}

static void init_textures() {
    MTLTextureDescriptor *texture_desc
        = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:_ctx.framebuffer_width
                                                           height:_ctx.framebuffer_height
                                                        mipmapped:false];

    // Create the texture from the device by using the descriptor
    for (size_t i = 0; i < EVA_MAX_MTL_BUFFERS; ++i) {
        _ctx.mtl_textures[i] = [_ctx.mtl_device newTextureWithDescriptor:texture_desc];
    }
}

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

void eva_request_frame()
{
    [_app_view setNeedsDisplay:YES];
}

uint32_t eva_get_window_width()
{
    return _ctx.window_width;
}

uint32_t eva_get_window_height()
{
    return _ctx.window_height;
}

eva_pixel *eva_get_framebuffer()
{
    return _ctx.framebuffer;
}

uint32_t eva_get_framebuffer_width()
{
    return _ctx.framebuffer_width;
}

uint32_t eva_get_framebuffer_height()
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

    _ctx.window_width  = (uint32_t)content_bounds.size.width;
    _ctx.window_height = (uint32_t)content_bounds.size.height;

    _ctx.scale_x = (float)(backing_bounds.size.width / content_bounds.size.width);
    _ctx.scale_y = (float)(backing_bounds.size.height / content_bounds.size.height);

    _ctx.framebuffer_width  = (uint32_t)(_ctx.window_width * _ctx.scale_x);
    _ctx.framebuffer_height = (uint32_t)(_ctx.window_height * _ctx.scale_y);
    if (_ctx.framebuffer) {
        free(_ctx.framebuffer);
    }

    uint32_t size = _ctx.framebuffer_width * _ctx.framebuffer_height;
    _ctx.framebuffer = calloc((size_t)size, sizeof(eva_pixel));

   // if (_ctx.mtl_texture != nil) {
   //     // TODO: Does this release the attached texture_desc created below?
   //     [_ctx.mtl_texture release];
   // }

   // MTLTextureDescriptor *texture_desc = [[MTLTextureDescriptor alloc] init];
   // texture_desc.pixelFormat = MTLPixelFormatBGRA8Unorm;
   // texture_desc.width = _ctx.framebuffer_width;
   // texture_desc.height = _ctx.framebuffer_height;
   // _ctx.mtl_texture = [_app_view.device newTextureWithDescriptor:texture_desc];
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

    // Setup metal
    _ctx.mtl_device = MTLCreateSystemDefaultDevice();
    _ctx.mtl_cmd_queue = [_ctx.mtl_device newCommandQueue];
    _ctx.semaphore = dispatch_semaphore_create(EVA_MAX_MTL_BUFFERS);
    create_shaders();
    init_textures();

    // Setup view
    _app_view = [[eva_view alloc] init];
    _app_view.device = _ctx.mtl_device;
    _app_view.paused = YES;
    _app_view.enableSetNeedsDisplay = YES;
    [_app_view updateTrackingAreas];
    eva_view_delegate *viewController = [[eva_view_delegate alloc] init];
    _app_view.delegate = viewController;

    // Assign view to window
    _app_window.contentView = _app_view;
    [_app_window makeFirstResponder:_app_view];
    [_app_window center];
    [_app_window makeKeyAndOrderFront:nil];

    [NSApp finishLaunching];
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
- (BOOL)isFlipped
{
    return NO;
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
@implementation eva_view_delegate
- (void) drawInMTKView:(nonnull MTKView *) view {
    uint64_t start = eva_time_now();

    // Most of this code is from minifb: https://github.com/emoon/minifb

    // Wait to ensure only MaxBuffersInFlight number of frames are getting proccessed
    // by any stage in the Metal pipeline (App, Metal, Drivers, GPU, etc)
    dispatch_semaphore_wait(_ctx.semaphore, DISPATCH_TIME_FOREVER);

    _ctx.mtl_texture_index = (_ctx.mtl_texture_index + 1) % EVA_MAX_MTL_BUFFERS;

    // Create a new command buffer for each render pass to the current drawable
    id<MTLCommandBuffer> cmd_buf = [_ctx.mtl_cmd_queue commandBuffer];
    cmd_buf.label = @"eva_command_buffer";

    // Add completion hander which signals semaphore when Metal and the GPU has fully
    // finished processing the commands we're encoding this frame.  This indicates when the
    // dynamic buffers filled with our vertices, that we're writing to this frame, will no longer
    // be needed by Metal and the GPU, meaning we can overwrite the buffer contents without
    // corrupting the rendering.
    __block dispatch_semaphore_t block_sema = _ctx.semaphore;
    [cmd_buf addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        (void)buffer;
        puts("command buffer complete");
        dispatch_semaphore_signal(block_sema);
    }];

    // Copy the bytes from our data object into the texture
    MTLRegion region = {
        { 0, 0, 0 },
        { _ctx.framebuffer_width, _ctx.framebuffer_height, 1 }
    };
    uint32_t bytes_per_row = _ctx.framebuffer_width * sizeof(eva_pixel);
    id<MTLTexture> texture = _ctx.mtl_textures[_ctx.mtl_texture_index];
    [texture replaceRegion:region 
               mipmapLevel:0 
                 withBytes:_ctx.framebuffer 
               bytesPerRow:bytes_per_row];

    // Delay getting the currentRenderPassDescriptor until absolutely needed. This avoids
    // holding onto the drawable and blocking the display pipeline any longer than necessary
    MTLRenderPassDescriptor* pass_desc = view.currentRenderPassDescriptor;
    if (pass_desc != nil) {
        pass_desc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 1.0, 0.0, 1.0);

        // Create a render command encoder so we can render into something
        id<MTLRenderCommandEncoder> render_enc = [cmd_buf renderCommandEncoderWithDescriptor:pass_desc];
        render_enc.label = @"eva_command_encoder";

        // Set render command encoder state
        [render_enc setRenderPipelineState:_ctx.mtl_pipe_state];
        [render_enc setVertexBytes:_vertices length:sizeof(_vertices) atIndex:0];

        [render_enc setFragmentTexture:texture atIndex:0];

        // Draw the vertices of our quads
        [render_enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

        // We're done encoding commands
        [render_enc endEncoding];

        // Schedule a present once the framebuffer is complete using the current drawable
        [cmd_buf presentDrawable:view.currentDrawable];
    }

    // Finalize rendering here & push the command buffer to the GPU
    [cmd_buf commit];

    printf("drawRect %.1fms\n", eva_time_since_ms(start));
}
- (void) mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
	(void)view;
	(void)size;
    // resize
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
