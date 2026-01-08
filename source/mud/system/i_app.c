//
// i_app.c - Sokol application entry point and rendering
//

#include "doom/d_event.h"
#include "doom/d_main.h"
#include "render/v_video.h"
#include "script/script_main.h"
#include "system/i_config.h"
#include "system/i_input.h"

#include "sokol_app.h"
#include "sokol_args.h"
// clang-format off
#include "sokol_gfx.h"
#include "system/doom_shaders.h"
// clang-format on
#include "sokol_glue.h"
#include "sokol_log.h"
#include "ui/ui_main.h"
#include <stdio.h>

#include "mud_profiling.h"

//
// Global state
//

typedef struct
{
    struct
    {
        sg_buffer vbuf;

        int gfx_vscreenwidth;
        int gfx_vscreenheight;

        int gfx_rscreenwidth;
        int gfx_rscreenheight;

        struct
        {
            sg_image img;
            sg_view tex_view;
        } pal;

        struct
        {
            sg_image img;
            sg_view tex_view;
        } hud;

        struct
        {
            sg_image img;
            sg_view tex_view;
        } level;

        struct
        {
            sg_image img;
            sg_view tex_view;
            sg_view att_view;
        } rgba;

        sg_sampler smp_palettize;  // Sampler for the palettization pass
        sg_sampler smp_upscale;    // Sampler for the upscale pass
        sg_pipeline offscreen_pip; // Offscreen pipeline
        sg_pipeline display_pip;   // Display pipeline
    } gfx;

    sg_pass_action pass_action;

    struct
    {
        uint32_t mouse_button_state;
    } inp;
} app_state_t;

static app_state_t state;
static TracyCZoneCtx tracy_zone = {0};

extern int r_upscaledwidth;
extern int r_upscaledheight;

//
// init
//
// Initialize application, graphics, and game
//
static void init(void)
{

    TracyCSetThreadName("Main Thread");

    Script_Init();
    
    memset(&state, 0, sizeof(state));

    // Initialize Doom engine
    int doomretro_main(void);
    doomretro_main();

    // Initialize Sokol GFX
    sg_setup(&(sg_desc){
    .environment = sglue_environment(),
    .logger.func = slog_func,
    });

    UI_Init();

    // Create vertex buffer for fullscreen triangle
    const float verts[] = { 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f };
    state.gfx.vbuf      = sg_make_buffer(&(sg_buffer_desc){
         .data = SG_RANGE(verts),
    });

    // Create dynamic image and texture view for the color palette
    state.gfx.pal.img = sg_make_image(&(sg_image_desc){
    .width               = 256,
    .height              = 1,
    .pixel_format        = SG_PIXELFORMAT_RGBA8,
    .usage.stream_update = true,
    });

    state.gfx.pal.tex_view = sg_make_view(&(sg_view_desc){
    .texture.image = state.gfx.pal.img,
    });

    // Create sampler with nearest filtering for the palettization pass
    state.gfx.smp_palettize = sg_make_sampler(&(sg_sampler_desc){
    .min_filter = SG_FILTER_NEAREST,
    .mag_filter = SG_FILTER_NEAREST,
    .wrap_u     = SG_WRAP_CLAMP_TO_EDGE,
    .wrap_v     = SG_WRAP_CLAMP_TO_EDGE,
    });

    // Create sampler with nearest filtering for the upscaling pass
    state.gfx.smp_upscale = sg_make_sampler(&(sg_sampler_desc){
    .min_filter = SG_FILTER_NEAREST,
    .mag_filter = SG_FILTER_NEAREST,
    .wrap_u     = SG_WRAP_CLAMP_TO_EDGE,
    .wrap_v     = SG_WRAP_CLAMP_TO_EDGE,
    });

    // Create pipeline for offscreen render pass (color palette lookup)
    state.gfx.offscreen_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader    = sg_make_shader(offscreen_shader_desc(sg_query_backend())),
        .layout    = { .attrs[0].format = SG_VERTEXFORMAT_FLOAT2 },
        .cull_mode = SG_CULLMODE_NONE,
        .depth = {
            .write_enabled = false,
            .compare       = SG_COMPAREFUNC_ALWAYS,
            .pixel_format  = SG_PIXELFORMAT_NONE,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
    });

    // Create pipeline to upscale offscreen framebuffer to display
    state.gfx.display_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader    = sg_make_shader(display_shader_desc(sg_query_backend())),
        .layout    = { .attrs[0].format = SG_VERTEXFORMAT_FLOAT2 },
        .cull_mode = SG_CULLMODE_NONE,
        .depth = {
            .write_enabled = false,
            .compare       = SG_COMPAREFUNC_ALWAYS,
        },
    });

    // Initialize pass action
    state.pass_action =
    (sg_pass_action){ .colors[0] = { .load_action = SG_LOADACTION_CLEAR,
                      .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } } };
    
}

static void update_render_textures()
{
    if(state.gfx.gfx_vscreenwidth != video.screen_width || state.gfx.gfx_vscreenheight != video.screen_height)
    {
        // Validate dimensions before creating textures
        if(video.screen_width <= 0 || video.screen_height <= 0)
        {
            return;  // Don't recreate with invalid dimensions
        }

        if(state.gfx.hud.img.id)
        {
            sg_destroy_view(state.gfx.hud.tex_view);
            sg_destroy_image(state.gfx.hud.img);
            state.gfx.hud.img.id = 0;
            state.gfx.hud.tex_view.id = 0;
        }

        // Create dynamic image and texture view for hud
        state.gfx.hud.img = sg_make_image(&(sg_image_desc){
        .width               = video.screen_width,
        .height              = video.screen_height,
        .pixel_format        = SG_PIXELFORMAT_R8,
        .usage.stream_update = true,
        });

        state.gfx.hud.tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = state.gfx.hud.img,
        });

        state.gfx.gfx_vscreenwidth  = video.screen_width;
        state.gfx.gfx_vscreenheight = video.screen_height;
    }

    if(state.gfx.gfx_rscreenwidth != render.screen_width || state.gfx.gfx_rscreenheight != render.screen_height)
    {
        // Validate dimensions before creating textures
        if(render.screen_width <= 0 || render.screen_height <= 0 ||
           r_upscaledwidth <= 0 || r_upscaledheight <= 0)
        {
            return;  // Don't recreate with invalid dimensions
        }

        if(state.gfx.rgba.img.id)
        {
            // Destroy RGBA resources
            sg_destroy_view(state.gfx.rgba.att_view);
            sg_destroy_view(state.gfx.rgba.tex_view);
            sg_destroy_image(state.gfx.rgba.img);
            state.gfx.rgba.img.id = 0;  // Clear ID after destruction
            state.gfx.rgba.tex_view.id = 0;
            state.gfx.rgba.att_view.id = 0;
        }

        if(state.gfx.level.img.id)
        {
            // Destroy level resources
            sg_destroy_view(state.gfx.level.tex_view);
            sg_destroy_image(state.gfx.level.img);
            state.gfx.level.img.id = 0;  // Clear ID after destruction
            state.gfx.level.tex_view.id = 0;
        }

        // Create dynamic image and texture view for level
        state.gfx.level.img = sg_make_image(&(sg_image_desc){
        .width               = render.screen_width,
        .height              = render.screen_height,
        .pixel_format        = SG_PIXELFORMAT_R8,
        .usage.stream_update = true,
        });

        state.gfx.level.tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = state.gfx.level.img,
        });

        // Create RGBA8 image, texture view and color-attachment view
        // for palette-expanded image (source for upscaling)
        state.gfx.rgba.img      = sg_make_image(&(sg_image_desc){
             .usage.color_attachment = true,
             .width                  = r_upscaledwidth * render.screen_width,
             .height                 = r_upscaledheight * render.screen_height,
             .pixel_format           = SG_PIXELFORMAT_RGBA8,
        });
        state.gfx.rgba.tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = state.gfx.rgba.img,
        });

        state.gfx.rgba.att_view = sg_make_view(&(sg_view_desc){
        .color_attachment.image = state.gfx.rgba.img,
        });

        state.gfx.gfx_rscreenwidth  = render.screen_width;
        state.gfx.gfx_rscreenheight = render.screen_height;
    }

    // Update hud texture
    sg_update_image(state.gfx.hud.img,
    &(sg_image_data){ .mip_levels[0] = {
                      .ptr  = v_screens[0],
                      .size = video.screen_width * video.screen_height,
                      } });

    // Update level texture
    sg_update_image(state.gfx.level.img,
    &(sg_image_data){ .mip_levels[0] = {
                      .ptr  = r_screens[0],
                      .size = render.screen_width * render.screen_height,
                      } });
}

extern SDL_Rect src_rect;
extern SDL_Rect dest_rect;

//
// frame
//
// Main render loop - called every frame
//
static void frame(void)
{
    TracyCZoneN(tracy_zone, "sokol frame", 1);

    void I_ReadController(void);
    I_ReadController();

    // Update game logic
    void D_DoomTick(void);
    D_DoomTick();

    update_render_textures();

    // Skip rendering if GPU resources aren't valid
    if(!state.gfx.rgba.img.id || !state.gfx.level.img.id || !state.gfx.hud.img.id)
    {
        sg_commit();
        TracyCZoneEnd(tracy_zone);
        TracyCFrameMark
        return;
    }

    // update palette
    sg_update_image(state.gfx.pal.img,
    &(sg_image_data){
    .mip_levels[0] = { .ptr = screencolors, .size = 256 * sizeof(uint32_t) } });

    // Offscreen render pass to perform color palette lookup
    sg_begin_pass(&(sg_pass){
    .action      = { .colors[0] = { .load_action = SG_LOADACTION_DONTCARE } },
    .attachments = { .colors[0] = state.gfx.rgba.att_view },
    });

    sg_apply_pipeline(state.gfx.offscreen_pip);

    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.gfx.vbuf,
        .views = {
            [VIEW_pix_img] = state.gfx.level.tex_view,
            [VIEW_pal_img] = state.gfx.pal.tex_view,
        },
        .samplers[SMP_smp] = state.gfx.smp_palettize,
    });

    sg_draw(0, 3, 1);

    // sg_apply_pipeline(state.gfx.offscreen_pip);

    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.gfx.vbuf,
        .views = {
            [VIEW_pix_img] = state.gfx.hud.tex_view,
            [VIEW_pal_img] = state.gfx.pal.tex_view,
        },
        .samplers[SMP_smp] = state.gfx.smp_palettize,
    });

    sg_draw(0, 3, 1);

    sg_end_pass();

    // Render resulting texture to display framebuffer with upscaling
    sg_begin_pass(&(sg_pass){ .action = { .colors[0] = { .load_action = SG_LOADACTION_CLEAR,
                                          .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } } },
    .swapchain = sglue_swapchain() });

    float x = dest_rect.x;
    float y = dest_rect.y;
    float w = dest_rect.w;
    float h = dest_rect.h;

    float scalar = ((float)video.display_height) / ((float)dest_rect.h);
    w *= scalar;
    h *= scalar;

    if(!vid_widescreen)
    {
        x = video.display_width / 2 - w / 2;
    }

    x = BETWEEN(0, x, video.display_width - x);
    y = BETWEEN(0, y, video.display_height - y);

    w = BETWEEN(0, w, video.display_width);
    h = BETWEEN(0, h, video.display_height);

    sg_apply_viewport(x, y, w, h, true);

    sg_apply_pipeline(state.gfx.display_pip);
    sg_apply_bindings(&(sg_bindings){
    .vertex_buffers[0]    = state.gfx.vbuf,
    .views[VIEW_rgba_img] = state.gfx.rgba.tex_view,
    .samplers[SMP_smp]    = state.gfx.smp_upscale,
    });

    sg_draw(0, 3, 1);
    sg_end_pass();

    UI_Frame();

    sg_commit();

    TracyCZoneEnd(tracy_zone);

    TracyCFrameMark
}

//
// input
//
// Handle input events from Sokol
//
static void input(const sapp_event* ev)
{
    I_InputQueueEvent(ev);
    UI_HandleEvent(ev);
}

//
// cleanup
//
// Shutdown and cleanup all resources
//
static void cleanup(void)
{
    // Destroy pipelines
    sg_destroy_pipeline(state.gfx.display_pip);
    sg_destroy_pipeline(state.gfx.offscreen_pip);

    // Destroy samplers
    sg_destroy_sampler(state.gfx.smp_upscale);
    sg_destroy_sampler(state.gfx.smp_palettize);

    // Destroy RGBA resources
    sg_destroy_view(state.gfx.rgba.att_view);
    sg_destroy_view(state.gfx.rgba.tex_view);
    sg_destroy_image(state.gfx.rgba.img);

    // Destroy palette resources
    sg_destroy_view(state.gfx.pal.tex_view);
    sg_destroy_image(state.gfx.pal.img);

    // Destroy pixel resources
    sg_destroy_view(state.gfx.hud.tex_view);
    sg_destroy_image(state.gfx.hud.img);

    // Destroy pixel resources
    sg_destroy_view(state.gfx.level.tex_view);
    sg_destroy_image(state.gfx.level.img);

    // Destroy vertex buffer
    sg_destroy_buffer(state.gfx.vbuf);

    UI_Shutdown();

    // Shutdown Sokol GFX
    sg_shutdown();

    Script_Shutdown();
}

//
// sokol_main
//
// Application entry point
//
sapp_desc sokol_main(int argc, char* argv[])
{
    sargs_desc init_args;
	memset(&init_args, 0, sizeof(sargs_desc));
	init_args.argc = argc;
	init_args.argv = argv;
	sargs_setup(&init_args);

    return (sapp_desc){ .init_cb = init,
        .frame_cb                = frame,
        .cleanup_cb              = cleanup,
        .event_cb                = input,
        .width                   = V_DEFAULT_DISPLAY_WIDTH,
        .height                  = V_DEFAULT_DISPLAY_HEIGHT,
        .swap_interval           = 1,
        .window_title            = "mud",
        .icon.sokol_default      = true,
        .logger.func             = slog_func,
        .win32_console_create    = true,
        .win32_console_utf8      = true };
}