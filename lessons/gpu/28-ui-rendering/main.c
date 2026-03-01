/*
 * GPU Lesson 28 -- UI Rendering
 *
 * Renders the forge-gpu immediate-mode UI system (forge_ui_ctx.h,
 * forge_ui_window.h) using the SDL GPU API.  All widgets -- labels,
 * buttons, checkboxes, sliders, and a text input -- are batched into a
 * single vertex/index buffer and drawn with one DrawIndexedPrimitives
 * call through a font-atlas texture with alpha blending.
 *
 * Pipeline overview:
 *   1. The UI context generates ForgeUiVertex arrays + Uint32 index
 *      arrays each frame in screen-space pixel coordinates.
 *   2. A single transfer buffer uploads both arrays to GPU buffers.
 *   3. An orthographic projection (push uniform) maps pixel coords to
 *      NDC, rebuilt every frame from the window size.
 *   4. The fragment shader samples the R8_UNORM atlas for glyph
 *      coverage and multiplies by the per-vertex color.
 *   5. Alpha blending (SRC_ALPHA / ONE_MINUS_SRC_ALPHA) composites
 *      anti-aliased text and translucent panel backgrounds.
 *
 * Controls:
 *   Mouse           -- interact with UI widgets
 *   Keyboard        -- type into the text input field
 *   Escape          -- quit
 *
 * SPDX-License-Identifier: Zlib
 */
#define SDL_MAIN_USE_CALLBACKS 1

#include "ui/forge_ui.h"
#include "ui/forge_ui_ctx.h"
#include "ui/forge_ui_window.h"
#include "math/forge_math.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h> /* offsetof */

/* ── Frame capture (compile-time option) ─────────────────────────────── */
#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Compiled shader bytecodes ───────────────────────────────────────── */

#include "shaders/compiled/ui_vert_spirv.h"
#include "shaders/compiled/ui_vert_dxil.h"
#include "shaders/compiled/ui_frag_spirv.h"
#include "shaders/compiled/ui_frag_dxil.h"

/* ── Constants ───────────────────────────────────────────────────────── */

#define WINDOW_TITLE "Forge GPU \xe2\x80\x94 28 UI Rendering"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Clear color — matches lesson 01 (near-black). */
#define CLEAR_R 0.02f
#define CLEAR_G 0.02f
#define CLEAR_B 0.03f
#define CLEAR_A 1.0f

/* Font asset path (relative to executable, same as all UI lessons). */
#define FONT_PATH "assets/fonts/liberation_mono/LiberationMono-Regular.ttf"

/* Atlas build parameters. */
#define ATLAS_PIXEL_HEIGHT 32.0f  /* glyph rasterization height in pixels */
#define ATLAS_PADDING      1      /* pixel padding between packed glyphs  */
#define ASCII_START        32     /* first printable ASCII codepoint      */
#define ASCII_END          126    /* last printable ASCII codepoint       */
#define ASCII_COUNT        (ASCII_END - ASCII_START + 1)  /* 95 glyphs  */

/* Shader resource counts. */
#define VS_NUM_SAMPLERS        0
#define VS_NUM_UNIFORM_BUFFERS 1  /* orthographic projection matrix */
#define FS_NUM_SAMPLERS        1  /* atlas texture + sampler        */
#define FS_NUM_UNIFORM_BUFFERS 0

/* Vertex attribute count: position(float2), uv(float2),
 * color_rg(float2), color_ba(float2). */
#define NUM_VERTEX_ATTRIBUTES  4

/* Initial GPU buffer capacities.  Sized to handle a typical UI panel
 * (around 1000 quads = 4000 vertices + 6000 indices) without needing
 * a resize on the first frame. */
#define INITIAL_VERTEX_CAPACITY 4096   /* vertices */
#define INITIAL_INDEX_CAPACITY  6144   /* indices  */

/* Demo window position and size. */
#define DEMO_WIN_X       50.0f
#define DEMO_WIN_Y       50.0f
#define DEMO_WIN_W       320.0f
#define DEMO_WIN_H       400.0f
#define DEMO_WIN_Z_ORDER 0

/* Demo widget layout sizes (pixel heights passed to layout_next). */
#define LABEL_HEIGHT        26.0f
#define BUTTON_HEIGHT       36.0f
#define CHECKBOX_HEIGHT     30.0f
#define SLIDER_HEIGHT       30.0f
#define TEXT_INPUT_HEIGHT    32.0f

/* Demo slider range. */
#define SLIDER_MIN 0.0f
#define SLIDER_MAX 1.0f

/* Demo initial values. */
#define SLIDER_INITIAL_VALUE   0.5f
#define CLICK_COUNT_INITIAL    0

/* Text input backing buffer size. */
#define TEXT_INPUT_BUF_SIZE 128

/* Label colors — title uses accent cyan (#4fc3f7) for emphasis,
 * info uses theme dim text (#8888aa) for secondary content. */
#define TITLE_LABEL_R  0.310f
#define TITLE_LABEL_G  0.765f
#define TITLE_LABEL_B  0.969f
#define TITLE_LABEL_A  1.00f
#define INFO_LABEL_R   0.533f
#define INFO_LABEL_G   0.533f
#define INFO_LABEL_B   0.667f
#define INFO_LABEL_A   1.00f

/* Cursor blink timing (in milliseconds). */
#define CURSOR_BLINK_INTERVAL_MS 530  /* half-period: on for 530ms, off for 530ms */

/* Label formatting buffer size. */
#define LABEL_BUF_SIZE 64

/* ── Uniform structure (matches ui.vert.hlsl cbuffer) ────────────────── */

typedef struct UiUniforms {
    mat4 projection;   /* 64 bytes: orthographic pixel-to-NDC mapping */
} UiUniforms;

/* ── Application state ───────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;   /* main application window handle            */
    SDL_GPUDevice *device;   /* GPU device used for all rendering         */

    /* ---- GPU pipeline ------------------------------------------------ */
    SDL_GPUGraphicsPipeline *pipeline;   /* alpha-blended 2D UI pipeline */

    /* ---- Font atlas texture ------------------------------------------ */
    SDL_GPUTexture *atlas_texture;       /* R8_UNORM single-channel alpha */
    SDL_GPUSampler *atlas_sampler;       /* linear filter, clamp-to-edge  */

    /* ---- Dynamic geometry (re-uploaded every frame) ------------------ */
    SDL_GPUBuffer *vertex_buffer;        /* ForgeUiVertex data            */
    Uint32         vertex_buffer_size;   /* current allocation in bytes   */
    SDL_GPUBuffer *index_buffer;         /* Uint32 index data             */
    Uint32         index_buffer_size;    /* current allocation in bytes   */

    /* ---- CPU-side UI state ------------------------------------------- */
    ForgeUiFont          font;           /* parsed TTF font data          */
    ForgeUiFontAtlas     atlas;          /* rasterized glyph atlas (CPU)  */
    ForgeUiContext       ui_ctx;         /* immediate-mode UI context     */
    ForgeUiWindowContext ui_wctx;        /* draggable window context      */

    /* ---- Demo widget state (persists across frames) ------------------ */
    ForgeUiWindowState   demo_window;    /* position, scroll, z-order     */
    float                slider_value;   /* slider demo value [0..1]      */
    bool                 checkbox_value; /* checkbox demo toggle           */
    ForgeUiTextInputState text_input;    /* text input buffer + cursor    */
    char                 text_buf[TEXT_INPUT_BUF_SIZE]; /* backing buffer */
    int                  click_count;    /* button click counter          */

    /* ---- Per-frame keyboard state (consumed by ctx_set_keyboard) ----- */
    char        frame_text_buf[64];      /* stable buffer for text typed this frame */
    bool        frame_key_backspace; /* backspace pressed this frame       */
    bool        frame_key_delete;    /* delete pressed this frame          */
    bool        frame_key_left;      /* left arrow pressed this frame      */
    bool        frame_key_right;     /* right arrow pressed this frame     */
    bool        frame_key_home;      /* home key pressed this frame        */
    bool        frame_key_end;       /* end key pressed this frame         */
    bool        frame_key_escape;    /* escape pressed this frame          */
    float       frame_scroll_delta;      /* mouse wheel accumulator       */

    /* ---- Swapchain format (queried at init) -------------------------- */
    SDL_GPUTextureFormat swapchain_format; /* pixel format of the swapchain  */

#ifdef FORGE_CAPTURE
    ForgeCapture capture;  /* screenshot/GIF capture state (optional)       */
#endif
} app_state;

/* ── Helper: next power of two ───────────────────────────────────────── */

/* Returns the smallest power of two >= value.  Used for buffer growth
 * so that repeated resizes amortize to O(1) per element. */
static Uint32 next_power_of_two(Uint32 value)
{
    if (value == 0) return 1;
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value + 1;
}

/* ── Helper: build orthographic projection ───────────────────────────── */

/* Orthographic projection mapping screen-space pixels to clip space.
 *
 * Origin at top-left, x-right, y-down (matching ForgeUiVertex convention).
 * Maps:
 *   x [0..width]   -> [-1..+1]
 *   y [0..height]  -> [+1..-1]  (y is flipped: top = +1, bottom = -1)
 *   z [0..1]       -> [0..1]    (unused, no depth buffer)
 *
 * Column-major storage (matches forge_math mat4 layout and HLSL):
 *   col0: (2/W,  0,   0, 0)
 *   col1: ( 0, -2/H,  0, 0)
 *   col2: ( 0,   0,   1, 0)
 *   col3: (-1,   1,   0, 1)
 */
static mat4 ui_ortho_projection(float width, float height)
{
    mat4 m = mat4_identity();
    m.m[0]  =  2.0f / width;   /* col0 row0: scale x to [-1,+1]       */
    m.m[5]  = -2.0f / height;  /* col1 row1: scale y, flip for y-down  */
    m.m[12] = -1.0f;           /* col3 row0: translate x so 0 -> -1    */
    m.m[13] =  1.0f;           /* col3 row1: translate y so 0 -> +1    */
    /* m[10] = 1, m[15] = 1 already set by mat4_identity */
    return m;
}

/* ── Helper: create shader from embedded bytecode ────────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code, size_t spirv_size,
    const Uint8 *dxil_code,  size_t dxil_size,
    Uint32 num_samplers,
    Uint32 num_uniform_buffers)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage              = stage;
    info.entrypoint         = "main";
    info.num_samplers       = num_samplers;
    info.num_uniform_buffers = num_uniform_buffers;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format    = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code      = spirv_code;
        info.code_size = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format    = SDL_GPU_SHADERFORMAT_DXIL;
        info.code      = dxil_code;
        info.code_size = dxil_size;
    } else {
        SDL_Log("create_shader: no supported shader format available");
        return NULL;
    }

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("SDL_CreateGPUShader failed: %s", SDL_GetError());
    }
    return shader;
}

/* ── SDL_AppInit ─────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* ── SDL + GPU device + window ──────────────────────────────── */

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, true, NULL);
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Request SDR_LINEAR for correct gamma handling. */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s",
                    SDL_GetError());
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    SDL_GPUTextureFormat swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    /* ── Allocate app state ─────────────────────────────────────── */

    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app_state");
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window           = window;
    state->device           = device;
    state->swapchain_format = swapchain_format;

    /* Assign *appstate immediately after allocation (CodeRabbit C-04)
     * so that SDL_AppQuit can clean up even if init fails below. */
    *appstate = state;

#ifdef FORGE_CAPTURE
    forge_capture_parse_args(&state->capture, argc, argv);
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_init(&state->capture, device, window)) {
            SDL_Log("Failed to initialise capture");
            goto init_fail;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    /* ── Load font + build atlas ────────────────────────────────── */

    const char *base = SDL_GetBasePath();
    if (!base) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        goto init_fail;
    }

    char font_path[512];
    SDL_snprintf(font_path, sizeof(font_path), "%s%s", base, FONT_PATH);

    if (!forge_ui_ttf_load(font_path, &state->font)) {
        SDL_Log("forge_ui_ttf_load failed for '%s'", font_path);
        goto init_fail;
    }

    /* Build the printable ASCII codepoint range [32..126]. */
    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    if (!forge_ui_atlas_build(&state->font, ATLAS_PIXEL_HEIGHT,
                              codepoints, ASCII_COUNT, ATLAS_PADDING,
                              &state->atlas)) {
        SDL_Log("forge_ui_atlas_build failed");
        goto init_fail;
    }

    /* ── Upload atlas to GPU texture ────────────────────────────── */
    {
        /* Create GPU texture -- R8_UNORM because the atlas is single-channel
         * alpha coverage data (one byte per pixel, no color information). */
        SDL_GPUTextureCreateInfo tex_info;
        SDL_zero(tex_info);
        tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
        tex_info.format               = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tex_info.width                = (Uint32)state->atlas.width;
        tex_info.height               = (Uint32)state->atlas.height;
        tex_info.layer_count_or_depth = 1;
        tex_info.num_levels           = 1;

        state->atlas_texture = SDL_CreateGPUTexture(device, &tex_info);
        if (!state->atlas_texture) {
            SDL_Log("SDL_CreateGPUTexture (atlas) failed: %s",
                    SDL_GetError());
            goto init_fail;
        }

        /* One byte per pixel for R8_UNORM. */
        Uint32 atlas_bytes =
            (Uint32)state->atlas.width * (Uint32)state->atlas.height;

        /* Create transfer buffer for the one-time atlas upload. */
        SDL_GPUTransferBufferCreateInfo xfer_info;
        SDL_zero(xfer_info);
        xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        xfer_info.size  = atlas_bytes;

        SDL_GPUTransferBuffer *xfer =
            SDL_CreateGPUTransferBuffer(device, &xfer_info);
        if (!xfer) {
            SDL_Log("SDL_CreateGPUTransferBuffer (atlas) failed: %s",
                    SDL_GetError());
            goto init_fail;
        }

        /* Map, copy pixel data, unmap. */
        void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
        if (!mapped) {
            SDL_Log("SDL_MapGPUTransferBuffer (atlas) failed: %s",
                    SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(device, xfer);
            goto init_fail;
        }
        SDL_memcpy(mapped, state->atlas.pixels, atlas_bytes);
        SDL_UnmapGPUTransferBuffer(device, xfer);

        /* Upload via copy pass. */
        SDL_GPUCommandBuffer *upload_cmd =
            SDL_AcquireGPUCommandBuffer(device);
        if (!upload_cmd) {
            SDL_Log("SDL_AcquireGPUCommandBuffer (atlas upload) failed: %s",
                    SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(device, xfer);
            goto init_fail;
        }

        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(upload_cmd);

        SDL_GPUTextureTransferInfo tex_src;
        SDL_zero(tex_src);
        tex_src.transfer_buffer = xfer;
        tex_src.pixels_per_row  = (Uint32)state->atlas.width;
        tex_src.rows_per_layer  = (Uint32)state->atlas.height;

        SDL_GPUTextureRegion tex_dst;
        SDL_zero(tex_dst);
        tex_dst.texture = state->atlas_texture;
        tex_dst.w       = (Uint32)state->atlas.width;
        tex_dst.h       = (Uint32)state->atlas.height;
        tex_dst.d       = 1;

        SDL_UploadToGPUTexture(copy, &tex_src, &tex_dst, false);
        SDL_EndGPUCopyPass(copy);

        if (!SDL_SubmitGPUCommandBuffer(upload_cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer (atlas upload) failed: %s",
                    SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(device, xfer);
            goto init_fail;
        }

        SDL_ReleaseGPUTransferBuffer(device, xfer);
    }

    /* ── Create atlas sampler ───────────────────────────────────── */
    {
        /* Linear filtering for smooth text edges at sub-pixel positions.
         * Clamp-to-edge prevents sampling outside the atlas boundary,
         * which would bleed neighboring glyphs into each other. */
        SDL_GPUSamplerCreateInfo samp_info;
        SDL_zero(samp_info);
        samp_info.min_filter     = SDL_GPU_FILTER_LINEAR;
        samp_info.mag_filter     = SDL_GPU_FILTER_LINEAR;
        samp_info.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        state->atlas_sampler = SDL_CreateGPUSampler(device, &samp_info);
        if (!state->atlas_sampler) {
            SDL_Log("SDL_CreateGPUSampler failed: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Create shaders ─────────────────────────────────────────── */

    SDL_GPUShader *vert_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_VERTEX,
        ui_vert_spirv, sizeof(ui_vert_spirv),
        ui_vert_dxil,  sizeof(ui_vert_dxil),
        VS_NUM_SAMPLERS, VS_NUM_UNIFORM_BUFFERS);
    if (!vert_shader) goto init_fail;

    SDL_GPUShader *frag_shader = create_shader(
        device, SDL_GPU_SHADERSTAGE_FRAGMENT,
        ui_frag_spirv, sizeof(ui_frag_spirv),
        ui_frag_dxil,  sizeof(ui_frag_dxil),
        FS_NUM_SAMPLERS, FS_NUM_UNIFORM_BUFFERS);
    if (!frag_shader) {
        SDL_ReleaseGPUShader(device, vert_shader);
        goto init_fail;
    }

    /* ── Create graphics pipeline ───────────────────────────────── */
    {
        /* Vertex attributes: split ForgeUiVertex into 4 FLOAT2 slots.
         * This avoids FLOAT4 for color because the vertex struct stores
         * r, g, b, a as separate floats -- two FLOAT2 reads reconstruct
         * the full RGBA color in the vertex shader. */
        SDL_GPUVertexAttribute attrs[NUM_VERTEX_ATTRIBUTES];
        SDL_zero(attrs);

        /* Location 0: screen-space position (pos_x, pos_y). */
        attrs[0].location    = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[0].offset      = offsetof(ForgeUiVertex, pos_x);

        /* Location 1: atlas UV coordinates (uv_u, uv_v). */
        attrs[1].location    = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[1].offset      = offsetof(ForgeUiVertex, uv_u);

        /* Location 2: vertex color red + green (r, g). */
        attrs[2].location    = 2;
        attrs[2].buffer_slot = 0;
        attrs[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset      = offsetof(ForgeUiVertex, r);

        /* Location 3: vertex color blue + alpha (b, a). */
        attrs[3].location    = 3;
        attrs[3].buffer_slot = 0;
        attrs[3].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[3].offset      = offsetof(ForgeUiVertex, b);

        /* Vertex buffer description: 32 bytes per vertex (8 floats). */
        SDL_GPUVertexBufferDescription vbd;
        SDL_zero(vbd);
        vbd.slot       = 0;
        vbd.pitch      = sizeof(ForgeUiVertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        /* Alpha blending: standard pre-multiplied-alpha-compatible blend.
         * Color: src.rgb * src.a + dst.rgb * (1 - src.a)
         * This composites anti-aliased text edges and semi-transparent
         * panel backgrounds over the cleared framebuffer. */
        SDL_GPUColorTargetDescription ctd;
        SDL_zero(ctd);
        ctd.format = swapchain_format;
        ctd.blend_state.enable_blend = true;

        /* Source color is scaled by source alpha for correct translucency. */
        ctd.blend_state.src_color_blendfactor =
            SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        /* Destination color is scaled by (1 - src alpha) for correct
         * layering of overlapping translucent surfaces. */
        ctd.blend_state.dst_color_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        /* Add the two contributions together. */
        ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;

        /* Alpha channel: source alpha passes through at full strength,
         * destination alpha fades by (1 - src.a).  This preserves
         * correct alpha values in the framebuffer if read back. */
        ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        ctd.blend_state.dst_alpha_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        /* Write all four channels so alpha blending works correctly. */
        ctd.blend_state.color_write_mask =
            SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
            SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

        /* Assemble the full pipeline. */
        SDL_GPUGraphicsPipelineCreateInfo pipe_info;
        SDL_zero(pipe_info);

        pipe_info.vertex_shader   = vert_shader;
        pipe_info.fragment_shader = frag_shader;

        pipe_info.vertex_input_state.vertex_buffer_descriptions = &vbd;
        pipe_info.vertex_input_state.num_vertex_buffers         = 1;
        pipe_info.vertex_input_state.vertex_attributes          = attrs;
        pipe_info.vertex_input_state.num_vertex_attributes      =
            NUM_VERTEX_ATTRIBUTES;

        /* Triangle list: every 3 indices form one triangle.  UI quads
         * are emitted as two CCW triangles (6 indices each). */
        pipe_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        /* No backface culling: UI quads may have any winding order
         * depending on how the context emits flipped or mirrored
         * elements (e.g. collapse triangles). */
        pipe_info.rasterizer_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
        pipe_info.rasterizer_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
        pipe_info.rasterizer_state.front_face =
            SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        /* No depth buffer: 2D UI uses painter's algorithm -- the UI
         * context emits draw data back-to-front via z_order sorting
         * in the window context.  Depth testing would incorrectly
         * discard translucent fragments that should blend. */
        pipe_info.depth_stencil_state.enable_depth_test  = false;
        pipe_info.depth_stencil_state.enable_depth_write = false;

        /* Single color target, no depth-stencil target. */
        pipe_info.target_info.color_target_descriptions = &ctd;
        pipe_info.target_info.num_color_targets         = 1;
        pipe_info.target_info.has_depth_stencil_target  = false;

        state->pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipe_info);
        if (!state->pipeline) {
            SDL_Log("SDL_CreateGPUGraphicsPipeline failed: %s",
                    SDL_GetError());
            SDL_ReleaseGPUShader(device, vert_shader);
            SDL_ReleaseGPUShader(device, frag_shader);
            goto init_fail;
        }

        /* Shaders are compiled into the pipeline and no longer needed. */
        SDL_ReleaseGPUShader(device, vert_shader);
        SDL_ReleaseGPUShader(device, frag_shader);
    }

    /* ── Pre-allocate GPU vertex and index buffers ──────────────── */
    {
        /* Initial vertex buffer: sized for INITIAL_VERTEX_CAPACITY verts.
         * Power-of-two growth happens per-frame if the UI exceeds this. */
        Uint32 vb_init =
            INITIAL_VERTEX_CAPACITY * (Uint32)sizeof(ForgeUiVertex);

        SDL_GPUBufferCreateInfo vb_info;
        SDL_zero(vb_info);
        vb_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vb_info.size  = vb_init;

        state->vertex_buffer = SDL_CreateGPUBuffer(device, &vb_info);
        if (!state->vertex_buffer) {
            SDL_Log("SDL_CreateGPUBuffer (vertex) failed: %s",
                    SDL_GetError());
            goto init_fail;
        }
        state->vertex_buffer_size = vb_init;

        /* Initial index buffer: sized for INITIAL_INDEX_CAPACITY indices. */
        Uint32 ib_init =
            INITIAL_INDEX_CAPACITY * (Uint32)sizeof(Uint32);

        SDL_GPUBufferCreateInfo ib_info;
        SDL_zero(ib_info);
        ib_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        ib_info.size  = ib_init;

        state->index_buffer = SDL_CreateGPUBuffer(device, &ib_info);
        if (!state->index_buffer) {
            SDL_Log("SDL_CreateGPUBuffer (index) failed: %s",
                    SDL_GetError());
            goto init_fail;
        }
        state->index_buffer_size = ib_init;
    }

    /* ── Initialize UI contexts ─────────────────────────────────── */

    if (!forge_ui_ctx_init(&state->ui_ctx, &state->atlas)) {
        SDL_Log("forge_ui_ctx_init failed");
        goto init_fail;
    }

    if (!forge_ui_wctx_init(&state->ui_wctx, &state->ui_ctx)) {
        SDL_Log("forge_ui_wctx_init failed");
        goto init_fail;
    }

    /* Demo window: positioned at top-left with a comfortable size. */
    state->demo_window.rect.x     = DEMO_WIN_X;
    state->demo_window.rect.y     = DEMO_WIN_Y;
    state->demo_window.rect.w     = DEMO_WIN_W;
    state->demo_window.rect.h     = DEMO_WIN_H;
    state->demo_window.scroll_y   = 0.0f;
    state->demo_window.collapsed  = false;
    state->demo_window.z_order    = DEMO_WIN_Z_ORDER;

    state->slider_value   = SLIDER_INITIAL_VALUE;
    state->checkbox_value = false;
    state->click_count    = CLICK_COUNT_INITIAL;

    /* Text input: backed by a fixed-size buffer, initially empty. */
    state->text_buf[0]       = '\0';
    state->text_input.buffer   = state->text_buf;
    state->text_input.capacity = TEXT_INPUT_BUF_SIZE;
    state->text_input.length   = 0;
    state->text_input.cursor   = 0;

    /* Enable text input events so SDL delivers SDL_EVENT_TEXT_INPUT
     * for the text input widget. */
    if (!SDL_StartTextInput(window)) {
        SDL_Log("SDL_StartTextInput failed: %s", SDL_GetError());
        goto init_fail;
    }

    return SDL_APP_CONTINUE;

init_fail:
    /* SDL_AppQuit is called even when SDL_AppInit returns failure, and
     * *appstate was set right after allocation, so SDL_AppQuit handles
     * all resource cleanup via its NULL-checked release sequence. */
    return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        /* Escape quits the application. */
        if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
            return SDL_APP_SUCCESS;
        }

        /* Record key presses for forge_ui_ctx_set_keyboard.
         * These are consumed once per frame in SDL_AppIterate. */
        switch (event->key.scancode) {
        case SDL_SCANCODE_BACKSPACE:
            state->frame_key_backspace = true;
            break;
        case SDL_SCANCODE_DELETE:
            state->frame_key_delete = true;
            break;
        case SDL_SCANCODE_LEFT:
            state->frame_key_left = true;
            break;
        case SDL_SCANCODE_RIGHT:
            state->frame_key_right = true;
            break;
        case SDL_SCANCODE_HOME:
            state->frame_key_home = true;
            break;
        case SDL_SCANCODE_END:
            state->frame_key_end = true;
            break;
        default:
            break;
        }
        break;

    case SDL_EVENT_TEXT_INPUT: {
        /* Copy the typed text into a stable buffer owned by state.
         * Append rather than overwrite so multiple text events in the
         * same frame are all delivered to the UI context. */
        size_t cur = SDL_strlen(state->frame_text_buf);
        size_t add = SDL_strlen(event->text.text);
        if (cur + add < sizeof(state->frame_text_buf)) {
            SDL_memcpy(state->frame_text_buf + cur, event->text.text,
                       add + 1);
        }
        break;
    }

    case SDL_EVENT_MOUSE_WHEEL:
        /* Accumulate scroll delta.  Positive y = scroll down, matching
         * the ForgeUiContext convention. */
        state->frame_scroll_delta += event->wheel.y;
        break;

    case SDL_EVENT_WINDOW_RESIZED:
        /* The orthographic projection matrix is rebuilt every frame from
         * the current window size, so no explicit resize handling is
         * needed here. */
        break;

    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ──────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    SDL_GPUDevice *device = state->device;

    /* ── Query current window size and mouse state ──────────────── */

    int win_w = WINDOW_WIDTH;
    int win_h = WINDOW_HEIGHT;
    if (!SDL_GetWindowSizeInPixels(state->window, &win_w, &win_h)) {
        SDL_Log("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
    }

    float mx, my;
    Uint32 mouse_buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = (mouse_buttons & SDL_BUTTON_LMASK) != 0;

    /* ── UI declaration phase ───────────────────────────────────── */

    forge_ui_ctx_begin(&state->ui_ctx, mx, my, mouse_down);

    /* Pass scroll delta accumulated during SDL_AppEvent. */
    state->ui_ctx.scroll_delta = state->frame_scroll_delta;

    /* Forward keyboard state collected during SDL_AppEvent to the UI
     * context so that the text input widget can process key presses. */
    forge_ui_ctx_set_keyboard(
        &state->ui_ctx,
        state->frame_text_buf[0] ? state->frame_text_buf : NULL,
        state->frame_key_backspace,
        state->frame_key_delete,
        state->frame_key_left,
        state->frame_key_right,
        state->frame_key_home,
        state->frame_key_end,
        state->frame_key_escape);

    /* Begin window context (sorts and manages z-order). */
    forge_ui_wctx_begin(&state->ui_wctx);

    /* ── Demo window with widgets ─────────────────────────────── */

    if (forge_ui_wctx_window_begin(&state->ui_wctx,
                                    "UI Demo", &state->demo_window)) {

        /* Title label. */
        forge_ui_ctx_label_layout(&state->ui_ctx, "Hello, GPU UI!",
                                  LABEL_HEIGHT,
                                  TITLE_LABEL_R, TITLE_LABEL_G,
                                  TITLE_LABEL_B, TITLE_LABEL_A);

        /* Click counter label -- shows how many times the button has
         * been pressed, demonstrating persistent widget state. */
        char click_label[LABEL_BUF_SIZE];
        SDL_snprintf(click_label, sizeof(click_label),
                     "Clicks: %d", state->click_count);
        forge_ui_ctx_label_layout(&state->ui_ctx, click_label,
                                  LABEL_HEIGHT,
                                  INFO_LABEL_R, INFO_LABEL_G,
                                  INFO_LABEL_B, INFO_LABEL_A);

        /* Button: increments click counter on each press. */
        if (forge_ui_ctx_button_layout(&state->ui_ctx,
                                       "Click me", BUTTON_HEIGHT)) {
            state->click_count++;
        }

        /* Checkbox: toggles a boolean option. */
        forge_ui_ctx_checkbox_layout(&state->ui_ctx,
                                     "Toggle option",
                                     &state->checkbox_value,
                                     CHECKBOX_HEIGHT);

        /* Slider: adjustable value between SLIDER_MIN and SLIDER_MAX. */
        forge_ui_ctx_slider_layout(&state->ui_ctx, "##slider",
                                   &state->slider_value,
                                   SLIDER_MIN, SLIDER_MAX,
                                   SLIDER_HEIGHT);

        /* Slider value label -- shows the current numeric value. */
        char slider_label[LABEL_BUF_SIZE];
        SDL_snprintf(slider_label, sizeof(slider_label),
                     "Value: %.2f", (double)state->slider_value);
        forge_ui_ctx_label_layout(&state->ui_ctx, slider_label,
                                  LABEL_HEIGHT,
                                  INFO_LABEL_R, INFO_LABEL_G,
                                  INFO_LABEL_B, INFO_LABEL_A);

        /* Text input: editable single-line field with blinking cursor. */
        ForgeUiRect ti_rect = forge_ui_ctx_layout_next(&state->ui_ctx,
                                                        TEXT_INPUT_HEIGHT);

        /* Blink the cursor every CURSOR_BLINK_INTERVAL_MS milliseconds
         * using SDL_GetTicks to toggle visibility. */
        Uint64 ticks = SDL_GetTicks();
        bool cursor_visible =
            ((ticks / CURSOR_BLINK_INTERVAL_MS) % 2) == 0;

        forge_ui_ctx_text_input(&state->ui_ctx, "##text_input",
                                &state->text_input, ti_rect,
                                cursor_visible);

        forge_ui_wctx_window_end(&state->ui_wctx);
    }

    /* End window context: sorts windows by z_order and appends their
     * per-window draw lists into the main context vertex/index buffers. */
    forge_ui_wctx_end(&state->ui_wctx);
    forge_ui_ctx_end(&state->ui_ctx);

    /* Reset per-frame keyboard state now that the UI has consumed it.
     * This prevents stale key presses from being processed twice. */
    state->frame_text_buf[0]   = '\0';
    state->frame_key_backspace = false;
    state->frame_key_delete    = false;
    state->frame_key_left      = false;
    state->frame_key_right     = false;
    state->frame_key_home      = false;
    state->frame_key_end       = false;
    state->frame_key_escape    = false;
    state->frame_scroll_delta  = 0.0f;

    /* ── Skip rendering if no draw data ─────────────────────────── */

    if (state->ui_ctx.vertex_count == 0 ||
        state->ui_ctx.index_count == 0) {
        /* Even with no UI data, we must acquire and submit a command
         * buffer to present the cleared swapchain image. */
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
        if (cmd) {
            SDL_GPUTexture *swapchain = NULL;
            if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                                 &swapchain, NULL, NULL)) {
                SDL_Log("SDL_AcquireGPUSwapchainTexture (empty) failed: %s",
                        SDL_GetError());
            } else if (swapchain) {
                SDL_GPUColorTargetInfo ct;
                SDL_zero(ct);
                ct.texture     = swapchain;
                ct.load_op     = SDL_GPU_LOADOP_CLEAR;
                ct.store_op    = SDL_GPU_STOREOP_STORE;
                ct.clear_color.r = CLEAR_R;
                ct.clear_color.g = CLEAR_G;
                ct.clear_color.b = CLEAR_B;
                ct.clear_color.a = CLEAR_A;

                SDL_GPURenderPass *pass =
                    SDL_BeginGPURenderPass(cmd, &ct, 1, NULL);
                SDL_EndGPURenderPass(pass);
            }
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer (empty) failed: %s",
                        SDL_GetError());
            }
        }
        return SDL_APP_CONTINUE;
    }

    /* ── GPU buffer resize if needed ────────────────────────────── */

    Uint32 vb_needed =
        (Uint32)state->ui_ctx.vertex_count * (Uint32)sizeof(ForgeUiVertex);
    Uint32 ib_needed =
        (Uint32)state->ui_ctx.index_count * (Uint32)sizeof(Uint32);

    /* Grow vertex buffer using power-of-two sizing to amortize
     * reallocations across frames with varying UI complexity. */
    if (vb_needed > state->vertex_buffer_size) {
        if (state->vertex_buffer) {
            SDL_ReleaseGPUBuffer(device, state->vertex_buffer);
        }
        Uint32 new_size = next_power_of_two(vb_needed);

        SDL_GPUBufferCreateInfo vb_info;
        SDL_zero(vb_info);
        vb_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vb_info.size  = new_size;

        state->vertex_buffer = SDL_CreateGPUBuffer(device, &vb_info);
        if (!state->vertex_buffer) {
            SDL_Log("SDL_CreateGPUBuffer (vertex resize) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
        state->vertex_buffer_size = new_size;
    }

    /* Grow index buffer with the same power-of-two strategy. */
    if (ib_needed > state->index_buffer_size) {
        if (state->index_buffer) {
            SDL_ReleaseGPUBuffer(device, state->index_buffer);
        }
        Uint32 new_size = next_power_of_two(ib_needed);

        SDL_GPUBufferCreateInfo ib_info;
        SDL_zero(ib_info);
        ib_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        ib_info.size  = new_size;

        state->index_buffer = SDL_CreateGPUBuffer(device, &ib_info);
        if (!state->index_buffer) {
            SDL_Log("SDL_CreateGPUBuffer (index resize) failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
        state->index_buffer_size = new_size;
    }

    /* ── Upload vertex + index data via a single transfer buffer ── */

    Uint32 total_upload = vb_needed + ib_needed;

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_upload;

    SDL_GPUTransferBuffer *xfer =
        SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("SDL_CreateGPUTransferBuffer (frame) failed: %s",
                SDL_GetError());
        return SDL_APP_FAILURE;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("SDL_MapGPUTransferBuffer (frame) failed: %s",
                SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        return SDL_APP_FAILURE;
    }

    /* Vertex data at offset 0, index data immediately after. */
    SDL_memcpy(mapped, state->ui_ctx.vertices, vb_needed);
    SDL_memcpy((Uint8 *)mapped + vb_needed,
               state->ui_ctx.indices, ib_needed);
    SDL_UnmapGPUTransferBuffer(device, xfer);

    /* ── Acquire command buffer and upload via copy pass ─────────── */

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        return SDL_APP_FAILURE;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    /* Upload vertex data: transfer[0..vb_needed] -> vertex_buffer. */
    {
        SDL_GPUTransferBufferLocation src;
        SDL_zero(src);
        src.transfer_buffer = xfer;
        src.offset          = 0;

        SDL_GPUBufferRegion dst;
        SDL_zero(dst);
        dst.buffer = state->vertex_buffer;
        dst.offset = 0;
        dst.size   = vb_needed;

        SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    }

    /* Upload index data: transfer[vb_needed..total] -> index_buffer. */
    {
        SDL_GPUTransferBufferLocation src;
        SDL_zero(src);
        src.transfer_buffer = xfer;
        src.offset          = vb_needed;

        SDL_GPUBufferRegion dst;
        SDL_zero(dst);
        dst.buffer = state->index_buffer;
        dst.offset = 0;
        dst.size   = ib_needed;

        SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    }

    SDL_EndGPUCopyPass(copy);

    /* Release the transfer buffer now -- the copy pass has recorded
     * the upload commands, so the transfer data is no longer needed
     * after the copy pass ends. */
    SDL_ReleaseGPUTransferBuffer(device, xfer);

    /* ── Render pass ────────────────────────────────────────────── */

    SDL_GPUTexture *swapchain = NULL;
    if (!SDL_AcquireGPUSwapchainTexture(
            cmd, state->window, &swapchain, NULL, NULL)) {
        SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s",
                SDL_GetError());
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }

    if (!swapchain) {
        /* Window is minimized or not visible -- submit the command
         * buffer (which contains the copy pass) and skip drawing. */
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer (no swapchain) failed: %s",
                    SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }

    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture     = swapchain;
    /* Clear the framebuffer each frame to the dark background color.
     * LOAD_CLEAR is used instead of LOAD_LOAD because the UI is the
     * only content -- there is no previous pass to preserve. */
    color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op    = SDL_GPU_STOREOP_STORE;
    color_target.clear_color.r = CLEAR_R;
    color_target.clear_color.g = CLEAR_G;
    color_target.clear_color.b = CLEAR_B;
    color_target.clear_color.a = CLEAR_A;

    SDL_GPURenderPass *pass =
        SDL_BeginGPURenderPass(cmd, &color_target, 1, NULL);

    SDL_BindGPUGraphicsPipeline(pass, state->pipeline);

    /* Bind vertex buffer at slot 0. */
    SDL_GPUBufferBinding vb_binding;
    SDL_zero(vb_binding);
    vb_binding.buffer = state->vertex_buffer;
    vb_binding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer with 32-bit indices (matching ForgeUiContext). */
    SDL_GPUBufferBinding ib_binding;
    SDL_zero(ib_binding);
    ib_binding.buffer = state->index_buffer;
    ib_binding.offset = 0;
    SDL_BindGPUIndexBuffer(pass, &ib_binding,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    /* Bind atlas texture + sampler at fragment sampler slot 0.
     * The fragment shader reads the .r channel as glyph coverage. */
    SDL_GPUTextureSamplerBinding tex_binding;
    SDL_zero(tex_binding);
    tex_binding.texture = state->atlas_texture;
    tex_binding.sampler = state->atlas_sampler;
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

    /* Push the orthographic projection matrix as vertex uniform 0.
     * Rebuilt every frame so window resizes are handled automatically. */
    UiUniforms uniforms;
    uniforms.projection =
        ui_ortho_projection((float)win_w, (float)win_h);
    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(UiUniforms));

    /* Single draw call for all UI widgets.  The UI context has already
     * batched every widget (text glyphs, solid rects, panel backgrounds)
     * into one vertex/index buffer sharing the same atlas and pipeline. */
    SDL_DrawGPUIndexedPrimitives(
        pass,
        (Uint32)state->ui_ctx.index_count,
        1,    /* instance count   */
        0,    /* first index      */
        0,    /* vertex offset    */
        0);   /* first instance   */

    SDL_EndGPURenderPass(pass);

#ifdef FORGE_CAPTURE
    if (state->capture.mode != FORGE_CAPTURE_NONE) {
        if (!forge_capture_finish_frame(&state->capture, cmd, swapchain)) {
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
                return SDL_APP_FAILURE;
            }
        }
        if (forge_capture_should_quit(&state->capture)) {
            return SDL_APP_SUCCESS;
        }
    } else
#endif
    {
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                    SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ─────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    if (!appstate) return;
    app_state *state = (app_state *)appstate;
    SDL_GPUDevice *device = state->device;

#ifdef FORGE_CAPTURE
    forge_capture_destroy(&state->capture, device);
#endif

    /* Stop text input events. */
    if (state->window) {
        if (!SDL_StopTextInput(state->window)) {
            SDL_Log("SDL_StopTextInput failed: %s", SDL_GetError());
        }
    }

    /* UI contexts (CPU-side, free vertex/index draw lists). */
    forge_ui_wctx_free(&state->ui_wctx);
    forge_ui_ctx_free(&state->ui_ctx);

    /* Font atlas and font (CPU-side pixel data + glyph metadata). */
    forge_ui_atlas_free(&state->atlas);
    forge_ui_ttf_free(&state->font);

    /* GPU buffers. */
    if (state->index_buffer)
        SDL_ReleaseGPUBuffer(device, state->index_buffer);
    if (state->vertex_buffer)
        SDL_ReleaseGPUBuffer(device, state->vertex_buffer);

    /* GPU texture and sampler. */
    if (state->atlas_sampler)
        SDL_ReleaseGPUSampler(device, state->atlas_sampler);
    if (state->atlas_texture)
        SDL_ReleaseGPUTexture(device, state->atlas_texture);

    /* Graphics pipeline. */
    if (state->pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline);

    /* Release window from GPU device before destroying it (C-05). */
    SDL_ReleaseWindowFromGPUDevice(device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(device);
    SDL_free(state);
}
