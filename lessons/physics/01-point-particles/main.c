/*
 * Physics Lesson 01 — Point Particles
 *
 * Demonstrates: Symplectic Euler integration, gravity, drag, velocity damping,
 * sphere-plane collision with restitution, fixed timestep with accumulator.
 *
 * 20 spheres drop from random heights, bounce on a ground plane with varying
 * restitution. Sphere color maps to velocity magnitude — red at rest, blue
 * at high speed. The simulation uses a fixed 60 Hz timestep decoupled from
 * rendering, with interpolation for smooth visuals.
 *
 * Uses forge_scene.h for all rendering boilerplate (device, window, pipelines,
 * camera, grid, shadow map, sky, UI) — this file focuses on physics.
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause / resume simulation
 *   R                 — reset simulation
 *   T                 — toggle slow motion (1x / 0.25x)
 *   Escape            — release mouse / quit
 *
 * UI panel: pause checkbox, speed slider, reset button, simulation info.
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Physics library — particles, forces, integration, collision */
#include "physics/forge_physics.h"

/* Procedural geometry — sphere mesh */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer — replaces ~500 lines of rendering boilerplate */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

/* Physics simulation */
#define PHYSICS_DT        (1.0f / 60.0f)
#define NUM_PARTICLES     20
#define GRAVITY_Y        -9.81f
#define DRAG_COEFF        0.02f
#define GROUND_Y          0.0f
#define SPHERE_RADIUS     0.3f
#define SPAWN_HEIGHT_MIN  3.0f
#define SPAWN_HEIGHT_MAX  12.0f
#define SPAWN_SPREAD      8.0f
#define RESTITUTION_MIN   0.3f
#define RESTITUTION_MAX   0.9f

/* Camera start position */
#define CAM_START_Y       4.0f
#define CAM_START_Z       12.0f
#define CAM_START_PITCH  -0.2f

/* Delta-time clamp to prevent physics explosion after alt-tab */
#define MAX_DELTA_TIME    0.1f

/* Velocity-to-color mapping */
#define VELOCITY_COLOR_MAX 10.0f  /* speed (m/s) mapped to "max" color */

/* Particle defaults */
#define PARTICLE_MASS     1.0f    /* kg */
#define PARTICLE_DAMPING  0.01f   /* velocity damping per step [0..1] */

/* Sphere mesh resolution */
#define SPHERE_SLICES     32
#define SPHERE_STACKS     16

/* Pseudo-random number generation */
#define RNG_PRECISION     10000   /* modulus for float conversion */

/* UI panel layout */
#define PANEL_X           10.0f
#define PANEL_Y           10.0f
#define PANEL_W           220.0f
#define PANEL_H           300.0f
#define LABEL_HEIGHT      24.0f
#define BUTTON_HEIGHT     30.0f
#define SLIDER_HEIGHT     28.0f

/* Speed slider range */
#define SPEED_MIN         0.0f
#define SPEED_MAX         2.0f
#define SPEED_DEFAULT     1.0f

/* ── Types ────────────────────────────────────────────────────────── */

/* Application state — persists across all SDL callbacks. */
typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* Sphere GPU geometry */
    SDL_GPUBuffer *sphere_vb;
    SDL_GPUBuffer *sphere_ib;
    Uint32         sphere_index_count;

    /* Physics particles */
    ForgePhysicsParticle particles[NUM_PARTICLES];         /* active state */
    ForgePhysicsParticle initial_particles[NUM_PARTICLES]; /* for reset    */
    float  accumulator;    /* leftover time for fixed-step physics (s) */
    float  sim_time;       /* total simulated time (s)                */
    bool   paused;         /* true = physics frozen, camera still works */
    float  speed_scale;    /* simulation speed multiplier [0..2]       */

    /* UI state (persistent across frames) */
    ForgeUiWindowState ui_window;
} app_state;

/* ── Helper: upload_shape_vb ─────────────────────────────────────── */

/* Convert a ForgeShape (struct-of-arrays) to ForgeSceneVertex (AoS)
 * and upload to a GPU vertex buffer. */
static SDL_GPUBuffer *upload_shape_vb(ForgeScene *scene,
                                       const ForgeShape *shape)
{
    ForgeSceneVertex *verts = SDL_calloc((size_t)shape->vertex_count,
                                         sizeof(ForgeSceneVertex));
    if (!verts) {
        SDL_Log("ERROR: Failed to allocate %d vertices for shape upload",
                shape->vertex_count);
        return NULL;
    }

    for (int i = 0; i < shape->vertex_count; i++) {
        verts[i].position = shape->positions[i];
        verts[i].normal   = shape->normals[i];
    }

    SDL_GPUBuffer *buf = forge_scene_upload_buffer(scene,
        SDL_GPU_BUFFERUSAGE_VERTEX, verts,
        (Uint32)shape->vertex_count * (Uint32)sizeof(ForgeSceneVertex));
    SDL_free(verts);
    return buf;
}

/* ── Helper: velocity_to_color ───────────────────────────────────── */

/* Map velocity magnitude to a color gradient.
 * Rest (0 m/s) → warm red, mid speed (5 m/s) → yellow,
 * high speed (10+ m/s) → cool blue. */
static void velocity_to_color(float speed, float out_color[4])
{
    /* Normalize speed to [0, 1] range */
    float t = speed / VELOCITY_COLOR_MAX;
    if (t > 1.0f) t = 1.0f;

    if (t < 0.5f) {
        /* Red → Yellow */
        float s = t * 2.0f;
        out_color[0] = 1.0f;
        out_color[1] = s;
        out_color[2] = 0.0f;
    } else {
        /* Yellow → Blue */
        float s = (t - 0.5f) * 2.0f;
        out_color[0] = 1.0f - s;
        out_color[1] = 1.0f - s;
        out_color[2] = s;
    }
    out_color[3] = 1.0f;
}

/* ── Helper: init_particles ──────────────────────────────────────── */

/* Initialize particles with spread-out positions and varying restitution.
 * Uses a simple LCG seeded from the particle index for deterministic
 * placement — no rand() dependency. */
static void init_particles(ForgePhysicsParticle *particles,
                           ForgePhysicsParticle *initial)
{
    for (int i = 0; i < NUM_PARTICLES; i++) {
        /* Deterministic pseudo-random from index */
        unsigned int seed = (unsigned int)(i * 2654435761u);
        float fx = ((float)(seed % RNG_PRECISION) / (float)RNG_PRECISION) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float fz = ((float)(seed % RNG_PRECISION) / (float)RNG_PRECISION) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float fy = ((float)(seed % RNG_PRECISION) / (float)RNG_PRECISION);
        seed = seed * 1664525u + 1013904223u;
        float fr = ((float)(seed % RNG_PRECISION) / (float)RNG_PRECISION);

        float x = fx * SPAWN_SPREAD;
        float y = SPAWN_HEIGHT_MIN + fy * (SPAWN_HEIGHT_MAX - SPAWN_HEIGHT_MIN);
        float z = fz * SPAWN_SPREAD;
        float restitution = RESTITUTION_MIN + fr * (RESTITUTION_MAX - RESTITUTION_MIN);

        particles[i] = forge_physics_particle_create(
            vec3_create(x, y, z),
            PARTICLE_MASS, PARTICLE_DAMPING, restitution, SPHERE_RADIUS);

        initial[i] = particles[i];  /* save for reset */
    }
}

/* ── Helper: reset_simulation ────────────────────────────────────── */

/* Restore all particles to their initial state and zero the accumulator. */
static void reset_simulation(app_state *state)
{
    for (int i = 0; i < NUM_PARTICLES; i++) {
        state->particles[i] = state->initial_particles[i];
    }
    state->accumulator = 0.0f;
    state->sim_time = 0.0f;
}

/* ── SDL_AppInit ─────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("ERROR: Failed to allocate app_state");
        return SDL_APP_FAILURE;
    }
    *appstate = state;

    /* Configure the scene renderer — override camera start position */
    ForgeSceneConfig cfg = forge_scene_default_config(
        "Physics Lesson 01 \xe2\x80\x94 Point Particles");
    cfg.cam_start_pos   = vec3_create(0.0f, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path       = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";
    cfg.font_size       = 16.0f;

    /* One call replaces ~500 lines of SDL GPU boilerplate */
    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("ERROR: forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* Generate and upload sphere geometry */
    ForgeShape sphere = forge_shapes_sphere(SPHERE_SLICES, SPHERE_STACKS);
    if (sphere.vertex_count == 0) {
        SDL_Log("ERROR: forge_shapes_sphere failed to allocate");
        return SDL_APP_FAILURE;
    }

    state->sphere_vb = upload_shape_vb(&state->scene, &sphere);
    state->sphere_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, sphere.indices,
        (Uint32)sphere.index_count * (Uint32)sizeof(uint32_t));
    state->sphere_index_count = (Uint32)sphere.index_count;
    forge_shapes_free(&sphere);

    if (!state->sphere_vb || !state->sphere_ib) {
        SDL_Log("ERROR: Failed to upload sphere geometry");
        return SDL_APP_FAILURE;
    }

    /* Initialize particles (copies to initial_particles for reset) */
    init_particles(state->particles, state->initial_particles);

    /* Initialize simulation state */
    state->accumulator  = 0.0f;
    state->sim_time     = 0.0f;
    state->paused       = false;
    state->speed_scale  = SPEED_DEFAULT;
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    /* Let scene handle camera, mouse capture, quit, escape */
    SDL_AppResult result = forge_scene_handle_event(&state->scene, event);
    if (result != SDL_APP_CONTINUE) return result;

    /* Physics-specific keys */
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        if (event->key.scancode == SDL_SCANCODE_R) {
            reset_simulation(state);
            SDL_Log("Simulation reset");
        } else if (event->key.scancode == SDL_SCANCODE_P) {
            state->paused = !state->paused;
            SDL_Log("Simulation %s", state->paused ? "paused" : "resumed");
        } else if (event->key.scancode == SDL_SCANCODE_T) {
            /* Toggle slow motion: 1.0x ↔ 0.25x */
            state->speed_scale = (state->speed_scale < 0.5f) ? 1.0f : 0.25f;
            SDL_Log("Speed: %.2fx", (double)state->speed_scale);
        }
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ──────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    ForgeScene *s = &state->scene;

    if (!forge_scene_begin_frame(s)) return SDL_APP_CONTINUE;

    float dt = forge_scene_dt(s);
    if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;

    /* ── Fixed-timestep physics ──────────────────────────────────── */
    /* Accumulate frame time scaled by speed_scale (UI slider) and
     * step physics in fixed increments.  This decouples rendering
     * from simulation — the physics always runs at PHYSICS_DT. */

    if (!state->paused) {
        float sim_dt = dt * state->speed_scale;
        state->accumulator += sim_dt;

        while (state->accumulator >= PHYSICS_DT) {
            /* Apply forces to all particles */
            vec3 gravity = vec3_create(0.0f, GRAVITY_Y, 0.0f);
            for (int i = 0; i < NUM_PARTICLES; i++) {
                forge_physics_apply_gravity(&state->particles[i], gravity);
                forge_physics_apply_drag(&state->particles[i], DRAG_COEFF);
            }

            /* Integrate — symplectic Euler advances velocity then position */
            for (int i = 0; i < NUM_PARTICLES; i++) {
                forge_physics_integrate(&state->particles[i], PHYSICS_DT);
            }

            /* Collide with ground plane (y = GROUND_Y, normal = up) */
            vec3 ground_normal = vec3_create(0.0f, 1.0f, 0.0f);
            for (int i = 0; i < NUM_PARTICLES; i++) {
                forge_physics_collide_plane(&state->particles[i],
                                            ground_normal, GROUND_Y);
            }

            state->accumulator -= PHYSICS_DT;
            state->sim_time += PHYSICS_DT;
        }
    }

    /* Interpolation factor for smooth rendering between physics steps.
     * alpha blends from prev_position (alpha=0) to position (alpha=1). */
    float alpha = state->accumulator / PHYSICS_DT;

    /* ── Shadow pass ─────────────────────────────────────────────── */

    forge_scene_begin_shadow_pass(s);
    for (int i = 0; i < NUM_PARTICLES; i++) {
        vec3 render_pos = vec3_lerp(
            state->particles[i].prev_position,
            state->particles[i].position, alpha);
        mat4 model = mat4_multiply(
            mat4_translate(render_pos),
            mat4_scale_uniform(SPHERE_RADIUS));
        forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                     state->sphere_index_count, model);
    }
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */

    forge_scene_begin_main_pass(s);
    for (int i = 0; i < NUM_PARTICLES; i++) {
        vec3 render_pos = vec3_lerp(
            state->particles[i].prev_position,
            state->particles[i].position, alpha);
        mat4 model = mat4_multiply(
            mat4_translate(render_pos),
            mat4_scale_uniform(SPHERE_RADIUS));

        /* Color encodes velocity — red at rest, yellow mid, blue fast */
        float color[4];
        float speed = vec3_length(state->particles[i].velocity);
        velocity_to_color(speed, color);

        forge_scene_draw_mesh(s, state->sphere_vb, state->sphere_ib,
                              state->sphere_index_count, model, color);
    }
    forge_scene_draw_grid(s);
    forge_scene_end_main_pass(s);

    /* ── UI pass ─────────────────────────────────────────────────── */

    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    /* Only pass mouse-down to UI when mouse is not captured by camera */
    bool mouse_down = !state->scene.mouse_captured
                    && (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (wctx) {
            if (forge_ui_wctx_window_begin(wctx, "Simulation",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;

                /* Pause toggle */
                forge_ui_ctx_checkbox_layout(ui, "Paused",
                                             &state->paused, LABEL_HEIGHT);

                /* Speed slider with label */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Speed: %.2fx",
                                 (double)state->speed_scale);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##speed",
                                           &state->speed_scale,
                                           SPEED_MIN, SPEED_MAX,
                                           SLIDER_HEIGHT);

                /* Reset button */
                if (forge_ui_ctx_button_layout(ui, "Reset", BUTTON_HEIGHT)) {
                    reset_simulation(state);
                }

                /* Simulation info */
                {
                    char buf[64];

                    forge_ui_ctx_label_layout(ui, "", LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Time: %.1f s",
                                 (double)state->sim_time);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "Particles: %d",
                                 NUM_PARTICLES);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                    SDL_snprintf(buf, sizeof(buf), "dt: %.1f ms",
                                 (double)(forge_scene_dt(s) * 1000.0f));
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                forge_ui_wctx_window_end(wctx);
            }
        }
    }
    forge_scene_end_ui(s);

    return forge_scene_end_frame(s);
}

/* ── SDL_AppQuit ─────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (!state) return;

    /* Release lesson-specific GPU resources before destroying the scene */
    if (forge_scene_device(&state->scene)) {
        if (!SDL_WaitForGPUIdle(forge_scene_device(&state->scene))) {
            SDL_Log("ERROR: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
        }
        if (state->sphere_vb)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_vb);
        if (state->sphere_ib)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_ib);
    }

    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
