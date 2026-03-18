/*
 * test_audio.c — Unit tests for forge_audio.h
 *
 * Tests buffer/source operations with synthetic sample data — no WAV files
 * needed.  Verifies volume scaling, panning, cursor advancement, looping,
 * end-of-buffer stop, additive mixing, progress reporting, and reset.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <limits.h>   /* INT_MAX — not provided by SDL_stdinc.h */
#include "math/forge_math.h"
#include "audio/forge_audio.h"

/* Portable isfinite — SDL provides isinf/isnan but no isfinite */
#ifndef forge_isfinite
#define forge_isfinite(x) (!SDL_isinf(x) && !SDL_isnan(x))
#endif

/* ── Test framework (same pattern as physics tests) ────────────────── */

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define EPSILON 0.001f

#define TEST(name) \
    do { \
        test_count++; \
        SDL_Log("  Testing: %s", name);

#define ASSERT_NEAR(a, b, eps) \
    do { \
        double actual_ = (double)(a); \
        double expected_ = (double)(b); \
        double eps_ = (double)(eps); \
        if (!forge_isfinite(actual_) || !forge_isfinite(expected_) || \
            SDL_fabs(actual_ - expected_) > eps_) { \
            SDL_Log("    FAIL: Expected %.6f, got %.6f (eps=%.6f)", \
                    expected_, actual_, eps_); \
            fail_count++; \
            return; \
        } \
    } while (0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            SDL_Log("    FAIL: Condition false: %s", #cond); \
            fail_count++; \
            return; \
        } \
    } while (0)

#define END_TEST() \
        SDL_Log("    PASS"); \
        pass_count++; \
    } while (0)

/* ── Helper: create a test buffer with known data ──────────────────── */

/* Create a stereo buffer with `frames` frames.  Left channel = left_val,
 * right channel = right_val, repeated for every frame. */
static ForgeAudioBuffer make_test_buffer(int frames, float left_val,
                                          float right_val)
{
    ForgeAudioBuffer buf;
    buf.channels    = 2;
    buf.sample_rate = FORGE_AUDIO_SAMPLE_RATE;

    if (frames <= 0) {
        buf.sample_count = 0;
        buf.data = NULL;
        return buf;
    }
    if (frames > (INT_MAX / 2)) {
        SDL_Log("FATAL: frame count overflow in make_test_buffer");
        buf.sample_count = 0;
        buf.data = NULL;
        return buf;
    }

    buf.sample_count = frames * 2;
    buf.data = (float *)SDL_calloc((size_t)buf.sample_count, sizeof(float));
    if (!buf.data) {
        SDL_Log("FATAL: SDL_calloc failed in make_test_buffer");
        buf.sample_count = 0;
        return buf;
    }
    for (int i = 0; i < frames; i++) {
        buf.data[i * 2]     = left_val;
        buf.data[i * 2 + 1] = right_val;
    }
    return buf;
}

/* Free a test buffer — exercises the same cleanup path as production code. */
static void free_test_buffer(ForgeAudioBuffer *buf)
{
    forge_audio_buffer_free(buf);
}

/* ── Tests ─────────────────────────────────────────────────────────── */

static void test_silent_source(void)
{
    TEST("silent source (volume = 0)");

    ForgeAudioBuffer buf = make_test_buffer(10, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 0.0f, false);
    src.playing = true;

    float out[20];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 10);

    /* Output should remain zero — source is at volume 0 */
    for (int i = 0; i < 20; i++) {
        ASSERT_NEAR(out[i], 0.0f, EPSILON);
    }

    /* Cursor should still advance */
    ASSERT_TRUE(src.cursor == 20);
    ASSERT_TRUE(!src.playing);  /* non-looping, reached end */

    free_test_buffer(&buf);
    END_TEST();
}

static void test_unity_volume(void)
{
    TEST("unity volume passthrough");

    ForgeAudioBuffer buf = make_test_buffer(4, 0.5f, 0.75f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    src.playing = true;

    float out[8];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 4);

    /* At unity volume, center pan: gain_l = gain_r = 0.5
     * So output = input * 0.5 */
    for (int i = 0; i < 4; i++) {
        ASSERT_NEAR(out[i * 2],     0.5f * 0.5f, EPSILON);
        ASSERT_NEAR(out[i * 2 + 1], 0.75f * 0.5f, EPSILON);
    }

    free_test_buffer(&buf);
    END_TEST();
}

static void test_volume_scaling(void)
{
    TEST("volume scaling");

    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 0.6f, false);
    src.playing = true;

    float out[8];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 4);

    /* volume=0.6, pan=0: gain_l = gain_r = 0.6 * 0.5 = 0.3 */
    for (int i = 0; i < 8; i++) {
        ASSERT_NEAR(out[i], 0.3f, EPSILON);
    }

    free_test_buffer(&buf);
    END_TEST();
}

static void test_pan_left(void)
{
    TEST("pan full left");

    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    src.pan = -1.0f;
    src.playing = true;

    float out[8];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 4);

    /* pan=-1: gain_l = 1.0*(1-(-1))/2 = 1.0, gain_r = 1.0*(1+(-1))/2 = 0.0 */
    for (int i = 0; i < 4; i++) {
        ASSERT_NEAR(out[i * 2],     1.0f, EPSILON);
        ASSERT_NEAR(out[i * 2 + 1], 0.0f, EPSILON);
    }

    free_test_buffer(&buf);
    END_TEST();
}

static void test_pan_right(void)
{
    TEST("pan full right");

    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    src.pan = 1.0f;
    src.playing = true;

    float out[8];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 4);

    /* pan=+1: gain_l = 0.0, gain_r = 1.0 */
    for (int i = 0; i < 4; i++) {
        ASSERT_NEAR(out[i * 2],     0.0f, EPSILON);
        ASSERT_NEAR(out[i * 2 + 1], 1.0f, EPSILON);
    }

    free_test_buffer(&buf);
    END_TEST();
}

static void test_cursor_advancement(void)
{
    TEST("cursor advances by frames * channels");

    ForgeAudioBuffer buf = make_test_buffer(10, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    src.playing = true;

    float out[8];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 4);

    /* Mixed 4 frames of stereo = 8 samples */
    ASSERT_TRUE(src.cursor == 8);
    ASSERT_TRUE(src.playing);  /* still has 6 frames left */

    free_test_buffer(&buf);
    END_TEST();
}

static void test_end_of_buffer_stop(void)
{
    TEST("non-looping source stops at buffer end");

    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    src.playing = true;

    /* Request more frames than the buffer contains */
    float out[16];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 8);

    ASSERT_TRUE(!src.playing);
    ASSERT_TRUE(src.cursor == 8);  /* sample_count = 8 */

    /* First 4 frames should have audio, last 4 should be zero */
    ASSERT_NEAR(out[0], 0.5f, EPSILON);  /* volume 1, center pan → 0.5 */
    ASSERT_NEAR(out[8], 0.0f, EPSILON);  /* beyond buffer → zero */

    free_test_buffer(&buf);
    END_TEST();
}

static void test_looping_wrap(void)
{
    TEST("looping source wraps cursor");

    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    /* Mix 6 frames from a 4-frame buffer — should wrap and mix 2 more */
    float out[12];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 6);

    ASSERT_TRUE(src.playing);  /* looping, still going */

    /* All 6 output frames should have audio (first 4 + 2 wrapped) */
    for (int i = 0; i < 12; i++) {
        ASSERT_NEAR(out[i], 0.5f, EPSILON);
    }

    free_test_buffer(&buf);
    END_TEST();
}

static void test_additive_mixing(void)
{
    TEST("mixing is additive");

    ForgeAudioBuffer buf_a = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioBuffer buf_b = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src_a = forge_audio_source_create(&buf_a, 1.0f, false);
    ForgeAudioSource src_b = forge_audio_source_create(&buf_b, 1.0f, false);
    src_a.playing = true;
    src_b.playing = true;

    float out[8];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src_a, out, 4);
    forge_audio_source_mix(&src_b, out, 4);

    /* Two sources at volume 1.0, center pan → each adds 0.5 → total 1.0 */
    for (int i = 0; i < 8; i++) {
        ASSERT_NEAR(out[i], 1.0f, EPSILON);
    }

    free_test_buffer(&buf_a);
    free_test_buffer(&buf_b);
    END_TEST();
}

static void test_progress(void)
{
    TEST("progress reports playback fraction");

    ForgeAudioBuffer buf = make_test_buffer(100, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    src.playing = true;

    ASSERT_NEAR(forge_audio_source_progress(&src), 0.0f, EPSILON);

    float out[100];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 50);

    ASSERT_NEAR(forge_audio_source_progress(&src), 0.5f, EPSILON);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_reset(void)
{
    TEST("reset moves cursor to 0");

    ForgeAudioBuffer buf = make_test_buffer(10, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    src.playing = true;
    src.cursor = 10;

    forge_audio_source_reset(&src);
    ASSERT_TRUE(src.cursor == 0);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_buffer_frames(void)
{
    TEST("buffer_frames returns sample_count / channels");

    ForgeAudioBuffer buf = make_test_buffer(42, 1.0f, 1.0f);
    ASSERT_TRUE(forge_audio_buffer_frames(&buf) == 42);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_buffer_duration(void)
{
    TEST("buffer_duration returns seconds");

    ForgeAudioBuffer buf = make_test_buffer(FORGE_AUDIO_SAMPLE_RATE, 1.0f, 1.0f);
    /* 44100 frames at 44100 Hz = 1.0 second */
    ASSERT_NEAR(forge_audio_buffer_duration(&buf), 1.0f, EPSILON);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_not_playing_produces_silence(void)
{
    TEST("source with playing=false produces no output");

    ForgeAudioBuffer buf = make_test_buffer(10, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    /* playing defaults to false */

    float out[20];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 10);

    for (int i = 0; i < 20; i++) {
        ASSERT_NEAR(out[i], 0.0f, EPSILON);
    }
    ASSERT_TRUE(src.cursor == 0);  /* cursor should not advance */

    free_test_buffer(&buf);
    END_TEST();
}

static void test_determinism(void)
{
    TEST("mixing is deterministic across two runs");

    ForgeAudioBuffer buf = make_test_buffer(8, 0.7f, 0.3f);

    float out_a[16], out_b[16];

    /* Run 1 */
    {
        ForgeAudioSource src = forge_audio_source_create(&buf, 0.8f, false);
        src.pan = 0.5f;
        src.playing = true;
        SDL_memset(out_a, 0, sizeof(out_a));
        forge_audio_source_mix(&src, out_a, 8);
    }

    /* Run 2 — identical parameters */
    {
        ForgeAudioSource src = forge_audio_source_create(&buf, 0.8f, false);
        src.pan = 0.5f;
        src.playing = true;
        SDL_memset(out_b, 0, sizeof(out_b));
        forge_audio_source_mix(&src, out_b, 8);
    }

    for (int i = 0; i < 16; i++) {
        ASSERT_NEAR(out_a[i], out_b[i], EPSILON);
    }

    free_test_buffer(&buf);
    END_TEST();
}

static void test_empty_buffer_no_hang(void)
{
    TEST("empty buffer does not hang or crash");

    /* Zero-frame buffer — sample_count = 0 */
    ForgeAudioBuffer buf = make_test_buffer(0, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    float out[8];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 4);

    /* Must return immediately: no output, source stopped, no crash */
    for (int i = 0; i < 8; i++) {
        ASSERT_NEAR(out[i], 0.0f, EPSILON);
    }
    ASSERT_TRUE(!src.playing);
    ASSERT_TRUE(src.cursor == 0);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_misaligned_cursor_no_hang(void)
{
    TEST("misaligned cursor does not hang");

    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    src.playing = true;
    src.cursor = buf.sample_count - 1; /* intentionally odd — misaligned for stereo */

    float out[16];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 4);

    /* Cursor must be channel-aligned after normalization */
    ASSERT_TRUE(src.cursor % FORGE_AUDIO_CHANNELS == 0);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_null_out_no_crash(void)
{
    TEST("NULL out pointer does not crash");

    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    src.playing = true;

    /* Should return immediately without crashing */
    forge_audio_source_mix(&src, NULL, 4);

    /* Source state unchanged — still playing since mix was a no-op */
    ASSERT_TRUE(src.playing);

    free_test_buffer(&buf);
    END_TEST();
}

/* ── Fade tests (Lesson 02) ────────────────────────────────────────── */

static void test_fade_in_ramps_up(void)
{
    TEST("fade-in ramps fade_volume from 0 to 1");

    ForgeAudioBuffer buf = make_test_buffer(FORGE_AUDIO_SAMPLE_RATE, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    forge_audio_source_fade_in(&src, 1.0f);

    ASSERT_TRUE(src.playing);
    ASSERT_NEAR(src.fade_volume, 0.0f, EPSILON);

    /* Advance half a second */
    forge_audio_source_fade_update(&src, 0.5f);
    ASSERT_NEAR(src.fade_volume, 0.5f, 0.01f);

    /* Advance to completion */
    forge_audio_source_fade_update(&src, 0.5f);
    ASSERT_NEAR(src.fade_volume, 1.0f, EPSILON);
    ASSERT_NEAR(src.fade_rate, 0.0f, EPSILON);  /* fade complete */

    free_test_buffer(&buf);
    END_TEST();
}

static void test_fade_out_ramps_down(void)
{
    TEST("fade-out ramps fade_volume from 1 to 0");

    ForgeAudioBuffer buf = make_test_buffer(FORGE_AUDIO_SAMPLE_RATE, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    forge_audio_source_fade_out(&src, 1.0f);
    ASSERT_NEAR(src.fade_target, 0.0f, EPSILON);

    forge_audio_source_fade_update(&src, 0.5f);
    ASSERT_NEAR(src.fade_volume, 0.5f, 0.01f);
    ASSERT_TRUE(src.playing);  /* still fading */

    forge_audio_source_fade_update(&src, 0.5f);
    ASSERT_NEAR(src.fade_volume, 0.0f, EPSILON);
    ASSERT_TRUE(!src.playing);  /* auto-stopped */

    free_test_buffer(&buf);
    END_TEST();
}

static void test_fade_out_auto_stop(void)
{
    TEST("fade-out auto-stops source when reaching 0");

    ForgeAudioBuffer buf = make_test_buffer(100, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 0.8f, true);
    src.playing = true;

    forge_audio_source_fade_out(&src, 0.1f);
    /* Advance past the fade duration */
    forge_audio_source_fade_update(&src, 0.2f);

    ASSERT_NEAR(src.fade_volume, 0.0f, EPSILON);
    ASSERT_TRUE(!src.playing);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_fade_independent_of_volume(void)
{
    TEST("fade_volume is independent of source volume");

    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 0.5f, false);
    src.fade_volume = 0.5f;
    src.playing = true;

    float out[8];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 4);

    /* volume=0.5, fade=0.5, pan=0 → gain_l = gain_r = 0.5 * 0.5 * 0.5 = 0.125 */
    ASSERT_NEAR(out[0], 0.125f, EPSILON);
    ASSERT_NEAR(out[1], 0.125f, EPSILON);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_fade_no_clicks(void)
{
    TEST("fade produces smooth ramp (no large jumps)");

    ForgeAudioBuffer buf = make_test_buffer(FORGE_AUDIO_SAMPLE_RATE, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    forge_audio_source_fade_in(&src, 1.0f);

    float prev = src.fade_volume;
    float dt = 1.0f / 60.0f;  /* 60 fps */
    float max_step = (1.0f / 1.0f) * dt + 0.001f;  /* rate * dt + tolerance */

    for (int i = 0; i < 60; i++) {
        forge_audio_source_fade_update(&src, dt);
        float diff = src.fade_volume - prev;
        if (diff < 0.0f) diff = -diff;
        ASSERT_TRUE(diff <= max_step);
        prev = src.fade_volume;
    }

    free_test_buffer(&buf);
    END_TEST();
}

static void test_fade_zero_duration_snaps(void)
{
    TEST("fade with zero duration snaps immediately");

    ForgeAudioBuffer buf = make_test_buffer(100, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    forge_audio_source_fade(&src, 0.0f, 0.0f);
    ASSERT_NEAR(src.fade_volume, 0.0f, EPSILON);
    ASSERT_NEAR(src.fade_rate, 0.0f, EPSILON);
    ASSERT_TRUE(!src.playing);  /* auto-stop on snap to 0 */

    free_test_buffer(&buf);
    END_TEST();
}

static void test_fade_in_starts_playback(void)
{
    TEST("fade_in starts playback");

    ForgeAudioBuffer buf = make_test_buffer(100, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    ASSERT_TRUE(!src.playing);

    forge_audio_source_fade_in(&src, 0.5f);
    ASSERT_TRUE(src.playing);
    ASSERT_NEAR(src.fade_volume, 0.0f, EPSILON);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_no_fade_no_change(void)
{
    TEST("zero fade_rate means no change");

    ForgeAudioBuffer buf = make_test_buffer(100, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;
    /* Default: fade_rate=0, fade_volume=1 */

    forge_audio_source_fade_update(&src, 1.0f);
    ASSERT_NEAR(src.fade_volume, 1.0f, EPSILON);
    ASSERT_TRUE(src.playing);

    free_test_buffer(&buf);
    END_TEST();
}

/* ── Pool tests (Lesson 02) ───────────────────────────────────────── */

static void test_pool_init(void)
{
    TEST("pool init sets all slots inactive");

    ForgeAudioPool pool;
    forge_audio_pool_init(&pool);

    ASSERT_TRUE(forge_audio_pool_active_count(&pool) == 0);
    for (int i = 0; i < FORGE_AUDIO_POOL_MAX_SOURCES; i++) {
        ASSERT_TRUE(!pool.sources[i].playing);
    }

    END_TEST();
}

static void test_pool_play_returns_valid_index(void)
{
    TEST("pool_play returns valid slot index");

    ForgeAudioPool pool;
    forge_audio_pool_init(&pool);
    ForgeAudioBuffer buf = make_test_buffer(100, 1.0f, 1.0f);

    int idx = forge_audio_pool_play(&pool, &buf, 1.0f, false);
    ASSERT_TRUE(idx >= 0 && idx < FORGE_AUDIO_POOL_MAX_SOURCES);
    ASSERT_TRUE(pool.sources[idx].playing);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_pool_full_returns_negative(void)
{
    TEST("pool_play returns -1 when full");

    ForgeAudioPool pool;
    forge_audio_pool_init(&pool);
    ForgeAudioBuffer buf = make_test_buffer(100, 1.0f, 1.0f);

    /* Fill all slots */
    for (int i = 0; i < FORGE_AUDIO_POOL_MAX_SOURCES; i++) {
        int idx = forge_audio_pool_play(&pool, &buf, 1.0f, true);
        ASSERT_TRUE(idx >= 0);
    }

    /* Next play should fail */
    int idx = forge_audio_pool_play(&pool, &buf, 1.0f, false);
    ASSERT_TRUE(idx == -1);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_pool_finished_sources_reclaimed(void)
{
    TEST("finished sources are reclaimed by pool_mix");

    ForgeAudioPool pool;
    forge_audio_pool_init(&pool);
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);

    int idx = forge_audio_pool_play(&pool, &buf, 1.0f, false);
    ASSERT_TRUE(idx >= 0);

    /* Mix more frames than the buffer has — source should stop */
    float out[16];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_pool_mix(&pool, out, 8);

    ASSERT_TRUE(!pool.sources[idx].playing);
    ASSERT_TRUE(forge_audio_pool_active_count(&pool) == 0);

    /* Slot should now be reusable */
    int idx2 = forge_audio_pool_play(&pool, &buf, 1.0f, false);
    ASSERT_TRUE(idx2 >= 0);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_pool_additive_mixing(void)
{
    TEST("pool mixes sources additively");

    ForgeAudioPool pool;
    forge_audio_pool_init(&pool);
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);

    forge_audio_pool_play(&pool, &buf, 1.0f, false);
    forge_audio_pool_play(&pool, &buf, 1.0f, false);

    float out[8];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_pool_mix(&pool, out, 4);

    /* Two sources at volume 1.0, center pan → each adds 0.5 → total 1.0 */
    ASSERT_NEAR(out[0], 1.0f, EPSILON);
    ASSERT_NEAR(out[1], 1.0f, EPSILON);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_pool_active_count(void)
{
    TEST("pool active_count tracks playing sources");

    ForgeAudioPool pool;
    forge_audio_pool_init(&pool);
    ForgeAudioBuffer buf = make_test_buffer(100, 1.0f, 1.0f);

    ASSERT_TRUE(forge_audio_pool_active_count(&pool) == 0);

    forge_audio_pool_play(&pool, &buf, 1.0f, false);
    forge_audio_pool_play(&pool, &buf, 1.0f, false);
    ASSERT_TRUE(forge_audio_pool_active_count(&pool) == 2);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_pool_stop_all(void)
{
    TEST("pool_stop_all stops everything");

    ForgeAudioPool pool;
    forge_audio_pool_init(&pool);
    ForgeAudioBuffer buf = make_test_buffer(100, 1.0f, 1.0f);

    forge_audio_pool_play(&pool, &buf, 1.0f, true);
    forge_audio_pool_play(&pool, &buf, 1.0f, true);
    forge_audio_pool_play(&pool, &buf, 1.0f, true);

    forge_audio_pool_stop_all(&pool);
    ASSERT_TRUE(forge_audio_pool_active_count(&pool) == 0);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_pool_get_valid_invalid(void)
{
    TEST("pool_get returns source or NULL");

    ForgeAudioPool pool;
    forge_audio_pool_init(&pool);

    ASSERT_TRUE(forge_audio_pool_get(&pool, 0) != NULL);
    ASSERT_TRUE(forge_audio_pool_get(&pool, FORGE_AUDIO_POOL_MAX_SOURCES - 1) != NULL);
    ASSERT_TRUE(forge_audio_pool_get(&pool, -1) == NULL);
    ASSERT_TRUE(forge_audio_pool_get(&pool, FORGE_AUDIO_POOL_MAX_SOURCES) == NULL);
    ASSERT_TRUE(forge_audio_pool_get(NULL, 0) == NULL);

    END_TEST();
}

/* ── Mixer tests (Lesson 03) ───────────────────────────────────────── */

static void test_mixer_create_defaults(void)
{
    TEST("mixer create defaults");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ASSERT_TRUE(m.channel_count == 0);
    ASSERT_NEAR(m.master_volume, 1.0f, EPSILON);
    ASSERT_NEAR(m.master_peak_l, 0.0f, EPSILON);
    ASSERT_NEAR(m.master_peak_r, 0.0f, EPSILON);

    END_TEST();
}

static void test_mixer_add_channels(void)
{
    TEST("mixer add channels returns sequential indices");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(100, 1.0f, 1.0f);
    ForgeAudioSource s1 = forge_audio_source_create(&buf, 1.0f, true);
    ForgeAudioSource s2 = forge_audio_source_create(&buf, 1.0f, true);

    int idx1 = forge_audio_mixer_add_channel(&m, &s1);
    int idx2 = forge_audio_mixer_add_channel(&m, &s2);

    ASSERT_TRUE(idx1 == 0);
    ASSERT_TRUE(idx2 == 1);
    ASSERT_TRUE(m.channel_count == 2);
    ASSERT_NEAR(m.channels[0].volume, 1.0f, EPSILON);
    ASSERT_NEAR(m.channels[1].pan, 0.0f, EPSILON);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_full_capacity(void)
{
    TEST("mixer rejects channels beyond max");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(10, 1.0f, 1.0f);
    ForgeAudioSource sources[FORGE_AUDIO_MIXER_MAX_CHANNELS + 1];
    for (int i = 0; i <= FORGE_AUDIO_MIXER_MAX_CHANNELS; i++) {
        sources[i] = forge_audio_source_create(&buf, 1.0f, true);
    }

    for (int i = 0; i < FORGE_AUDIO_MIXER_MAX_CHANNELS; i++) {
        ASSERT_TRUE(forge_audio_mixer_add_channel(&m, &sources[i]) == i);
    }
    ASSERT_TRUE(forge_audio_mixer_add_channel(&m, &sources[FORGE_AUDIO_MIXER_MAX_CHANNELS]) == -1);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_volume_per_channel(void)
{
    TEST("mixer per-channel volume scales output");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);
    m.channels[idx].volume = 0.5f;

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* channel volume=0.5, center pan → gain_l = gain_r = 0.5*0.5 = 0.25
     * Input is 1.0, so output = tanh(0.25) ≈ 0.2449 (close to 0.25 at low levels) */
    ASSERT_NEAR(out[0], forge_audio__tanhf(0.25f), 0.01f);
    ASSERT_NEAR(out[1], forge_audio__tanhf(0.25f), 0.01f);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_pan_per_channel(void)
{
    TEST("mixer per-channel pan steers L/R");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);
    m.channels[idx].pan = 1.0f;  /* full right */

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* pan=+1, vol=1: gain_l = 0, gain_r = 1.0*1.0 = 1.0
     * tanh(0) = 0, tanh(1) ≈ 0.7616 */
    ASSERT_NEAR(out[0], 0.0f, EPSILON);
    ASSERT_TRUE(out[1] > 0.5f);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_mute(void)
{
    TEST("muted channel produces no output");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);
    m.channels[idx].mute = true;

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    for (int i = 0; i < 8; i++) {
        ASSERT_NEAR(out[i], 0.0f, EPSILON);
    }

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_solo_single(void)
{
    TEST("solo isolates one channel");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf_a = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioBuffer buf_b = make_test_buffer(4, 0.5f, 0.5f);
    ForgeAudioSource src_a = forge_audio_source_create(&buf_a, 1.0f, true);
    ForgeAudioSource src_b = forge_audio_source_create(&buf_b, 1.0f, true);
    src_a.playing = true;
    src_b.playing = true;

    forge_audio_mixer_add_channel(&m, &src_a);
    int idx_b = forge_audio_mixer_add_channel(&m, &src_b);
    ASSERT_TRUE(idx_b >= 0);
    m.channels[idx_b].solo = true;

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* Only channel B should be heard.  buf_b has 0.5 input,
     * center pan vol=1 → gain=0.5 → 0.25 → tanh(0.25) */
    float expected = forge_audio__tanhf(0.25f);
    ASSERT_NEAR(out[0], expected, 0.01f);

    free_test_buffer(&buf_a);
    free_test_buffer(&buf_b);
    END_TEST();
}

static void test_mixer_solo_multiple(void)
{
    TEST("multiple solos play together");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src_a = forge_audio_source_create(&buf, 1.0f, true);
    ForgeAudioSource src_b = forge_audio_source_create(&buf, 1.0f, true);
    ForgeAudioSource src_c = forge_audio_source_create(&buf, 1.0f, true);
    src_a.playing = true;
    src_b.playing = true;
    src_c.playing = true;

    int a = forge_audio_mixer_add_channel(&m, &src_a);
    ASSERT_TRUE(a >= 0);
    int b = forge_audio_mixer_add_channel(&m, &src_b);
    ASSERT_TRUE(b >= 0);
    forge_audio_mixer_add_channel(&m, &src_c);  /* not solo'd */
    m.channels[a].solo = true;
    m.channels[b].solo = true;

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* Two solo'd channels each contribute 0.5 → sum 1.0 → tanh(1.0) */
    float two_ch = forge_audio__tanhf(1.0f);
    ASSERT_NEAR(out[0], two_ch, 0.02f);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_solo_overrides_mute(void)
{
    TEST("solo overrides mute (DAW convention)");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);
    m.channels[idx].mute = true;
    m.channels[idx].solo = true;

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* Solo overrides mute — channel should produce output */
    ASSERT_TRUE(out[0] > 0.1f);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_soft_clip(void)
{
    TEST("soft clip keeps output in [-1, 1]");

    ForgeAudioMixer m = forge_audio_mixer_create();
    m.master_volume = 5.0f;  /* crank it up */

    /* Five channels all at volume 2.0 with hot input */
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource sources[5];
    for (int i = 0; i < 5; i++) {
        sources[i] = forge_audio_source_create(&buf, 1.0f, true);
        sources[i].playing = true;
        int idx = forge_audio_mixer_add_channel(&m, &sources[i]);
        ASSERT_TRUE(idx >= 0);
        m.channels[idx].volume = 2.0f;
    }

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    for (int i = 0; i < 8; i++) {
        ASSERT_TRUE(out[i] >= -1.0f && out[i] <= 1.0f);
    }

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_tanh_near_identity(void)
{
    TEST("tanh is near-identity at low levels");

    /* For small x, tanh(x) ≈ x */
    float x = 0.1f;
    float y = forge_audio__tanhf(x);
    ASSERT_NEAR(y, x, 0.005f);

    /* Even at 0.3 the error is small */
    x = 0.3f;
    y = forge_audio__tanhf(x);
    ASSERT_NEAR(y, x, 0.01f);

    END_TEST();
}

static void test_mixer_peak_detection(void)
{
    TEST("peak detection captures max |sample|");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 0.8f, 0.6f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);
    m.channels[idx].volume = 2.0f;  /* vol=2 to get clear signal */

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* Channel peak should be non-zero */
    ASSERT_TRUE(m.channels[idx].peak_l > 0.0f);
    ASSERT_TRUE(m.channels[idx].peak_r > 0.0f);

    /* Master peak should also be non-zero */
    ASSERT_TRUE(m.master_peak_l > 0.0f);
    ASSERT_TRUE(m.master_peak_r > 0.0f);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_peak_hold_decay(void)
{
    TEST("peak hold decays over time");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 0.8f, 0.8f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);
    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    float hold_before = m.channels[idx].peak_hold_l;
    ASSERT_TRUE(hold_before > 0.0f);

    /* Wait past the hold time, then decay */
    forge_audio_mixer_update_peaks(&m, FORGE_AUDIO_PEAK_HOLD_TIME + 0.5f);

    ASSERT_TRUE(m.channels[idx].peak_hold_l < hold_before);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_master_volume(void)
{
    TEST("master volume scales final output");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    forge_audio_mixer_add_channel(&m, &src);
    m.master_volume = 0.1f;

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* Pre-tanh value: 0.5 (center pan) * 0.1 (master) = 0.05
     * tanh(0.05) ≈ 0.05 */
    ASSERT_NEAR(out[0], forge_audio__tanhf(0.05f), 0.01f);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_muted_cursor_advances(void)
{
    TEST("muted channel cursor advances to stay in sync");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(100, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);
    m.channels[idx].mute = true;

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* Cursor should have advanced even though channel is muted */
    ASSERT_TRUE(src.cursor > 0);
    ASSERT_TRUE(src.playing);  /* still looping */

    /* Output should be silence */
    for (int i = 0; i < 8; i++) {
        ASSERT_NEAR(out[i], 0.0f, EPSILON);
    }

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_nan_volume_sanitized(void)
{
    TEST("NaN channel volume is sanitized to 0");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);

    /* Poison the volume with NaN */
    m.channels[idx].volume = (float)SDL_sqrt(-1.0);  /* NaN */

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* Output should be finite (NaN was sanitized) */
    for (int i = 0; i < 8; i++) {
        ASSERT_TRUE(forge_isfinite(out[i]));
    }

    /* Volume should have been reset to 0 */
    ASSERT_NEAR(m.channels[idx].volume, 0.0f, EPSILON);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_nan_dt_rejected(void)
{
    TEST("NaN dt is rejected by update_peaks");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 0.8f, 0.8f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    float hold_before = m.channels[idx].peak_hold_l;
    ASSERT_TRUE(hold_before > 0.0f);

    /* Pass NaN as dt — should be rejected, hold unchanged */
    float nan_val = (float)SDL_sqrt(-1.0);
    forge_audio_mixer_update_peaks(&m, nan_val);

    ASSERT_NEAR(m.channels[idx].peak_hold_l, hold_before, EPSILON);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_nan_master_volume_sanitized(void)
{
    TEST("NaN master volume is sanitized to 1.0");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);

    /* Poison master volume with NaN */
    m.master_volume = (float)SDL_sqrt(-1.0);

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* Output should be finite */
    for (int i = 0; i < 8; i++) {
        ASSERT_TRUE(forge_isfinite(out[i]));
    }

    /* Master volume should have been reset to 1.0 */
    ASSERT_NEAR(m.master_volume, 1.0f, EPSILON);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_empty_clears_master_peaks(void)
{
    TEST("empty mixer clears master peaks on mix");

    ForgeAudioMixer m = forge_audio_mixer_create();
    /* Poison peaks to verify they get cleared */
    m.master_peak_l = 0.5f;
    m.master_peak_r = 0.5f;

    float out[8];
    for (int i = 0; i < 8; i++) out[i] = 99.0f;
    forge_audio_mixer_mix(&m, out, 4);

    ASSERT_NEAR(m.master_peak_l, 0.0f, EPSILON);
    ASSERT_NEAR(m.master_peak_r, 0.0f, EPSILON);

    /* Output should be silence */
    for (int i = 0; i < 8; i++) {
        ASSERT_NEAR(out[i], 0.0f, EPSILON);
    }

    END_TEST();
}

static void test_mixer_negative_master_volume_clamped(void)
{
    TEST("negative master volume is clamped to 0");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);
    m.master_volume = -0.5f;

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* Negative volume should be clamped to 0 → silence */
    for (int i = 0; i < 8; i++) {
        ASSERT_NEAR(out[i], 0.0f, EPSILON);
    }
    ASSERT_NEAR(m.master_volume, 0.0f, EPSILON);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_peak_exceeds_one_with_boost(void)
{
    TEST("channel peak can exceed 1.0 with gain > 1");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx >= 0);
    m.channels[idx].volume = 3.0f;  /* boost well above unity */

    float out[8];
    forge_audio_mixer_mix(&m, out, 4);

    /* Channel peak should exceed 1.0 (pre-master, pre-clip) */
    ASSERT_TRUE(m.channels[idx].peak_l > 1.0f);

    /* But output should still be in [-1, 1] thanks to tanh */
    for (int i = 0; i < 8; i++) {
        ASSERT_TRUE(out[i] >= -1.0f && out[i] <= 1.0f);
    }

    free_test_buffer(&buf);
    END_TEST();
}

static void test_mixer_add_channel_negative_count_rejected(void)
{
    TEST("add_channel rejects negative channel_count");

    ForgeAudioMixer m = forge_audio_mixer_create();
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);

    /* Corrupt channel_count to negative */
    m.channel_count = -1;
    int idx = forge_audio_mixer_add_channel(&m, &src);
    ASSERT_TRUE(idx == -1);

    free_test_buffer(&buf);
    END_TEST();
}

/* ── Attenuation tests (Lesson 04) ──────────────────────────────────── */

static void test_attenuation_linear_at_min(void)
{
    TEST("linear attenuation at min distance = 1.0");
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_LINEAR, 1.0f, 1.0f, 50.0f, 1.0f);
    ASSERT_NEAR(g, 1.0f, EPSILON);
    END_TEST();
}

static void test_attenuation_linear_at_max(void)
{
    TEST("linear attenuation at max distance = 0.0");
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_LINEAR, 50.0f, 1.0f, 50.0f, 1.0f);
    ASSERT_NEAR(g, 0.0f, EPSILON);
    END_TEST();
}

static void test_attenuation_linear_midpoint(void)
{
    TEST("linear attenuation at midpoint = 0.5");
    float mid = (1.0f + 50.0f) * 0.5f;
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_LINEAR, mid, 1.0f, 50.0f, 1.0f);
    ASSERT_NEAR(g, 0.5f, EPSILON);
    END_TEST();
}

static void test_attenuation_linear_beyond_max(void)
{
    TEST("linear attenuation beyond max clamped to 0.0");
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_LINEAR, 100.0f, 1.0f, 50.0f, 1.0f);
    ASSERT_NEAR(g, 0.0f, EPSILON);
    END_TEST();
}

static void test_attenuation_inverse_at_min(void)
{
    TEST("inverse attenuation at min distance = 1.0");
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_INVERSE, 1.0f, 1.0f, 50.0f, 1.0f);
    ASSERT_NEAR(g, 1.0f, EPSILON);
    END_TEST();
}

static void test_attenuation_inverse_double_distance(void)
{
    TEST("inverse attenuation at 2*min = 0.5");
    /* min / (min + rolloff*(dist - min)) = 1 / (1 + 1*1) = 0.5 */
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_INVERSE, 2.0f, 1.0f, 50.0f, 1.0f);
    ASSERT_NEAR(g, 0.5f, EPSILON);
    END_TEST();
}

static void test_attenuation_exponential_at_min(void)
{
    TEST("exponential attenuation at min distance = 1.0");
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_EXPONENTIAL, 1.0f, 1.0f, 50.0f, 1.0f);
    ASSERT_NEAR(g, 1.0f, EPSILON);
    END_TEST();
}

static void test_attenuation_exponential_double_distance(void)
{
    TEST("exponential attenuation at 2*min = 0.5");
    /* pow(2/1, -1) = 0.5 */
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_EXPONENTIAL, 2.0f, 1.0f, 50.0f, 1.0f);
    ASSERT_NEAR(g, 0.5f, EPSILON);
    END_TEST();
}

static void test_attenuation_zero_min_distance(void)
{
    TEST("zero min_distance returns 1.0 (no crash)");
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_LINEAR, 5.0f, 0.0f, 50.0f, 1.0f);
    ASSERT_NEAR(g, 1.0f, EPSILON);
    END_TEST();
}

static void test_attenuation_nan_min_dist(void)
{
    TEST("NaN min_dist returns 1.0 (no crash)");
    float nan_val = (float)SDL_sqrt(-1.0);
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_LINEAR, 5.0f, nan_val, 50.0f, 1.0f);
    ASSERT_NEAR(g, 1.0f, EPSILON);
    END_TEST();
}

static void test_attenuation_nan_max_dist(void)
{
    TEST("NaN max_dist returns 1.0 (no crash)");
    float nan_val = (float)SDL_sqrt(-1.0);
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_LINEAR, 5.0f, 1.0f, nan_val, 1.0f);
    ASSERT_NEAR(g, 1.0f, EPSILON);
    END_TEST();
}

static void test_attenuation_nan_rolloff(void)
{
    TEST("NaN rolloff returns 1.0 (no crash)");
    float nan_val = (float)SDL_sqrt(-1.0);
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_LINEAR, 5.0f, 1.0f, 50.0f, nan_val);
    ASSERT_NEAR(g, 1.0f, EPSILON);
    END_TEST();
}

static void test_attenuation_inf_distance(void)
{
    TEST("Inf distance returns clamped gain (no crash)");
    /* Create infinity via safe runtime computation */
    volatile float zero = 0.0f;  /* Volatile to prevent compile-time optimization */
    float inf_val = 1.0f / zero;
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_LINEAR, inf_val, 1.0f, 50.0f, 1.0f);
    ASSERT_NEAR(g, 1.0f, EPSILON);  /* NaN/Inf → 1.0 guard */
    END_TEST();
}

static void test_attenuation_inverted_range(void)
{
    TEST("inverted range (max < min) returns 1.0");
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_LINEAR, 5.0f, 10.0f, 2.0f, 1.0f);
    ASSERT_NEAR(g, 1.0f, EPSILON);
    END_TEST();
}

static void test_attenuation_equal_min_max(void)
{
    TEST("equal min and max returns 1.0 (linear range=0)");
    float g = forge_audio_spatial_attenuation(
        FORGE_AUDIO_ATTENUATION_LINEAR, 5.0f, 5.0f, 5.0f, 1.0f);
    ASSERT_NEAR(g, 1.0f, EPSILON);
    END_TEST();
}

/* ── Pan tests (Lesson 04) ──────────────────────────────────────── */

static void test_pan_source_directly_right(void)
{
    TEST("pan: source directly right = +1");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());
    /* Default orientation: forward = (0,0,-1), right = (1,0,0) */
    float p = forge_audio_spatial_pan(&l, vec3_create(10.0f, 0, 0));
    ASSERT_NEAR(p, 1.0f, EPSILON);
    END_TEST();
}

static void test_pan_source_directly_left(void)
{
    TEST("pan: source directly left = -1");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());
    float p = forge_audio_spatial_pan(&l, vec3_create(-10.0f, 0, 0));
    ASSERT_NEAR(p, -1.0f, EPSILON);
    END_TEST();
}

static void test_pan_source_directly_ahead(void)
{
    TEST("pan: source directly ahead = 0");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());
    /* Forward is (0,0,-1), so ahead is negative Z */
    float p = forge_audio_spatial_pan(&l, vec3_create(0, 0, -10.0f));
    ASSERT_NEAR(p, 0.0f, EPSILON);
    END_TEST();
}

static void test_pan_source_directly_behind(void)
{
    TEST("pan: source directly behind = 0");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());
    float p = forge_audio_spatial_pan(&l, vec3_create(0, 0, 10.0f));
    ASSERT_NEAR(p, 0.0f, EPSILON);
    END_TEST();
}

static void test_pan_source_at_listener(void)
{
    TEST("pan: source at listener = 0 (center)");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(5, 3, 2), quat_identity());
    float p = forge_audio_spatial_pan(&l, vec3_create(5, 3, 2));
    ASSERT_NEAR(p, 0.0f, EPSILON);
    END_TEST();
}

static void test_pan_rotated_listener(void)
{
    TEST("pan: rotated listener (facing +X, source at -Z = left)");
    /* quat_from_euler(yaw, pitch, roll).  Yaw = -PI/2 → facing +X.
     * Rotation of (1,0,0) by -PI/2 around Y → right = (0,0,1).
     * Source at (0,0,-10): direction = (0,0,-1), dot with right (0,0,1) = -1 → left. */
    quat q = quat_from_euler(-FORGE_PI * 0.5f, 0.0f, 0.0f);
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), q);
    float p = forge_audio_spatial_pan(&l, vec3_create(0, 0, -10.0f));
    ASSERT_NEAR(p, -1.0f, EPSILON);
    END_TEST();
}

/* ── Doppler tests (Lesson 04) ──────────────────────────────────── */

static void test_doppler_stationary(void)
{
    TEST("doppler: stationary = 1.0");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());
    ForgeAudioSpatialSource ss;
    SDL_memset(&ss, 0, sizeof(ss));
    ss.position = vec3_create(10, 0, 0);
    ss.velocity = vec3_create(0, 0, 0);
    float d = forge_audio_spatial_doppler(&l, &ss, FORGE_AUDIO_SPEED_OF_SOUND);
    ASSERT_NEAR(d, 1.0f, EPSILON);
    END_TEST();
}

static void test_doppler_approaching(void)
{
    TEST("doppler: approaching source > 1.0");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());
    ForgeAudioSpatialSource ss;
    SDL_memset(&ss, 0, sizeof(ss));
    ss.position = vec3_create(10, 0, 0);
    /* Source moving toward listener (negative X) */
    ss.velocity = vec3_create(-50.0f, 0, 0);
    float d = forge_audio_spatial_doppler(&l, &ss, FORGE_AUDIO_SPEED_OF_SOUND);
    ASSERT_TRUE(d > 1.0f);
    END_TEST();
}

static void test_doppler_receding(void)
{
    TEST("doppler: receding source < 1.0");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());
    ForgeAudioSpatialSource ss;
    SDL_memset(&ss, 0, sizeof(ss));
    ss.position = vec3_create(10, 0, 0);
    /* Source moving away from listener (positive X) */
    ss.velocity = vec3_create(50.0f, 0, 0);
    float d = forge_audio_spatial_doppler(&l, &ss, FORGE_AUDIO_SPEED_OF_SOUND);
    ASSERT_TRUE(d < 1.0f);
    END_TEST();
}

static void test_doppler_speed_of_sound_clamp(void)
{
    TEST("doppler: source at speed of sound — no infinity");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());
    ForgeAudioSpatialSource ss;
    SDL_memset(&ss, 0, sizeof(ss));
    ss.position = vec3_create(10, 0, 0);
    /* Source moving toward listener at exactly Mach 1 */
    ss.velocity = vec3_create(-FORGE_AUDIO_SPEED_OF_SOUND, 0, 0);
    float d = forge_audio_spatial_doppler(&l, &ss, FORGE_AUDIO_SPEED_OF_SOUND);
    ASSERT_TRUE(forge_isfinite(d));
    ASSERT_TRUE(d > 0.0f && d <= 2.0f);
    END_TEST();
}

static void test_doppler_zero_distance(void)
{
    TEST("doppler: zero distance = 1.0");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(5, 3, 2), quat_identity());
    ForgeAudioSpatialSource ss;
    SDL_memset(&ss, 0, sizeof(ss));
    ss.position = vec3_create(5, 3, 2);
    ss.velocity = vec3_create(100, 0, 0);
    float d = forge_audio_spatial_doppler(&l, &ss, FORGE_AUDIO_SPEED_OF_SOUND);
    ASSERT_NEAR(d, 1.0f, EPSILON);
    END_TEST();
}

/* ── Listener tests (Lesson 04) ─────────────────────────────────── */

static void test_listener_identity_quat(void)
{
    TEST("listener: identity quat → forward=(0,0,-1), right=(1,0,0)");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());
    ASSERT_NEAR(l.forward.x, 0.0f, EPSILON);
    ASSERT_NEAR(l.forward.y, 0.0f, EPSILON);
    ASSERT_NEAR(l.forward.z, -1.0f, EPSILON);
    ASSERT_NEAR(l.right.x, 1.0f, EPSILON);
    ASSERT_NEAR(l.right.y, 0.0f, EPSILON);
    ASSERT_NEAR(l.right.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_listener_yaw_90(void)
{
    TEST("listener: yaw -90° → forward=(1,0,0)");
    quat q = quat_from_euler(-FORGE_PI * 0.5f, 0.0f, 0.0f);
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), q);
    ASSERT_NEAR(l.forward.x, 1.0f, 0.01f);
    ASSERT_NEAR(l.forward.y, 0.0f, 0.01f);
    ASSERT_NEAR(l.forward.z, 0.0f, 0.01f);
    END_TEST();
}

static void test_listener_velocity_defaults_zero(void)
{
    TEST("listener: velocity defaults to zero");
    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(1, 2, 3), quat_identity());
    ASSERT_NEAR(l.velocity.x, 0.0f, EPSILON);
    ASSERT_NEAR(l.velocity.y, 0.0f, EPSILON);
    ASSERT_NEAR(l.velocity.z, 0.0f, EPSILON);
    END_TEST();
}

/* ── Spatial apply integration tests (Lesson 04) ────────────────── */

static void test_spatial_apply_sets_volume(void)
{
    TEST("spatial apply: sets volume = base_volume * attenuation");
    ForgeAudioBuffer buf = make_test_buffer(100, 0.5f, 0.5f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 0.8f, true);
    src.playing = true;

    ForgeAudioSpatialSource ss = forge_audio_spatial_source_create(
        &src, vec3_create(25.5f, 0, 0), NULL, -1);  /* midpoint of [1, 50] */

    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());

    forge_audio_spatial_apply(&l, &ss);

    /* Linear attenuation at midpoint = 0.5 → volume = 0.8 * 0.5 = 0.4 */
    ASSERT_NEAR(src.volume, 0.8f * 0.5f, 0.02f);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_spatial_apply_sets_pan(void)
{
    TEST("spatial apply: sets pan from 3D position");
    ForgeAudioBuffer buf = make_test_buffer(100, 0.5f, 0.5f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    /* Source to the right of the listener */
    ForgeAudioSpatialSource ss = forge_audio_spatial_source_create(
        &src, vec3_create(5.0f, 0, 0), NULL, -1);

    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());

    forge_audio_spatial_apply(&l, &ss);

    /* Source is directly right → pan should be +1 */
    ASSERT_NEAR(src.pan, 1.0f, EPSILON);

    free_test_buffer(&buf);
    END_TEST();
}

static void test_spatial_apply_doppler_sets_playback_rate(void)
{
    TEST("spatial apply: doppler sets playback_rate");
    ForgeAudioBuffer buf = make_test_buffer(100, 0.5f, 0.5f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, true);
    src.playing = true;

    ForgeAudioSpatialSource ss = forge_audio_spatial_source_create(
        &src, vec3_create(10, 0, 0), NULL, -1);
    ss.doppler_enabled = true;
    ss.velocity = vec3_create(-50.0f, 0, 0);  /* approaching */

    ForgeAudioListener l = forge_audio_listener_from_camera(
        vec3_create(0, 0, 0), quat_identity());

    forge_audio_spatial_apply(&l, &ss);

    /* Approaching source → playback_rate > 1.0 */
    ASSERT_TRUE(src.playback_rate > 1.0f);

    free_test_buffer(&buf);
    END_TEST();
}

/* ── Playback rate backward compatibility tests (Lesson 04) ──── */

static void test_playback_rate_default(void)
{
    TEST("playback_rate: default is 1.0, cursor_frac is 0.0");
    ForgeAudioBuffer buf = make_test_buffer(4, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    ASSERT_NEAR(src.playback_rate, 1.0f, EPSILON);
    ASSERT_NEAR(src.cursor_frac, 0.0f, EPSILON);
    free_test_buffer(&buf);
    END_TEST();
}

static void test_playback_rate_double_speed(void)
{
    TEST("playback_rate: rate=2.0 advances cursor twice as fast");
    /* 8-frame buffer of constant 1.0 stereo */
    ForgeAudioBuffer buf = make_test_buffer(8, 1.0f, 1.0f);
    ForgeAudioSource src = forge_audio_source_create(&buf, 1.0f, false);
    src.playing = true;
    src.playback_rate = 2.0f;

    /* Mix 4 output frames at rate 2.0 → should consume 8 buffer frames */
    float out[8];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_source_mix(&src, out, 4);

    /* Source should have reached end of buffer (8 frames consumed) */
    ASSERT_TRUE(!src.playing);

    free_test_buffer(&buf);
    END_TEST();
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    SDL_Log("=== forge_audio.h tests ===\n");

    test_silent_source();
    test_unity_volume();
    test_volume_scaling();
    test_pan_left();
    test_pan_right();
    test_cursor_advancement();
    test_end_of_buffer_stop();
    test_looping_wrap();
    test_additive_mixing();
    test_progress();
    test_reset();
    test_buffer_frames();
    test_buffer_duration();
    test_not_playing_produces_silence();
    test_determinism();
    test_empty_buffer_no_hang();
    test_misaligned_cursor_no_hang();
    test_null_out_no_crash();

    /* Fade tests (Lesson 02) */
    test_fade_in_ramps_up();
    test_fade_out_ramps_down();
    test_fade_out_auto_stop();
    test_fade_independent_of_volume();
    test_fade_no_clicks();
    test_fade_zero_duration_snaps();
    test_fade_in_starts_playback();
    test_no_fade_no_change();

    /* Pool tests (Lesson 02) */
    test_pool_init();
    test_pool_play_returns_valid_index();
    test_pool_full_returns_negative();
    test_pool_finished_sources_reclaimed();
    test_pool_additive_mixing();
    test_pool_active_count();
    test_pool_stop_all();
    test_pool_get_valid_invalid();

    /* Mixer tests (Lesson 03) */
    test_mixer_create_defaults();
    test_mixer_add_channels();
    test_mixer_full_capacity();
    test_mixer_volume_per_channel();
    test_mixer_pan_per_channel();
    test_mixer_mute();
    test_mixer_solo_single();
    test_mixer_solo_multiple();
    test_mixer_solo_overrides_mute();
    test_mixer_soft_clip();
    test_mixer_tanh_near_identity();
    test_mixer_peak_detection();
    test_mixer_peak_hold_decay();
    test_mixer_master_volume();
    test_mixer_muted_cursor_advances();
    test_mixer_nan_volume_sanitized();
    test_mixer_nan_dt_rejected();
    test_mixer_nan_master_volume_sanitized();
    test_mixer_empty_clears_master_peaks();
    test_mixer_negative_master_volume_clamped();
    test_mixer_peak_exceeds_one_with_boost();
    test_mixer_add_channel_negative_count_rejected();

    /* Spatial audio tests (Lesson 04) */
    test_attenuation_linear_at_min();
    test_attenuation_linear_at_max();
    test_attenuation_linear_midpoint();
    test_attenuation_linear_beyond_max();
    test_attenuation_inverse_at_min();
    test_attenuation_inverse_double_distance();
    test_attenuation_exponential_at_min();
    test_attenuation_exponential_double_distance();
    test_attenuation_zero_min_distance();
    test_attenuation_nan_min_dist();
    test_attenuation_nan_max_dist();
    test_attenuation_nan_rolloff();
    test_attenuation_inf_distance();
    test_attenuation_inverted_range();
    test_attenuation_equal_min_max();
    test_pan_source_directly_right();
    test_pan_source_directly_left();
    test_pan_source_directly_ahead();
    test_pan_source_directly_behind();
    test_pan_source_at_listener();
    test_pan_rotated_listener();
    test_doppler_stationary();
    test_doppler_approaching();
    test_doppler_receding();
    test_doppler_speed_of_sound_clamp();
    test_doppler_zero_distance();
    test_listener_identity_quat();
    test_listener_yaw_90();
    test_listener_velocity_defaults_zero();
    test_spatial_apply_sets_volume();
    test_spatial_apply_sets_pan();
    test_spatial_apply_doppler_sets_playback_rate();
    test_playback_rate_default();
    test_playback_rate_double_speed();

    SDL_Log("\n=== Results: %d/%d passed, %d failed ===",
            pass_count, test_count, fail_count);

    return fail_count > 0 ? 1 : 0;
}
