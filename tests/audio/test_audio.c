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
#include "math/forge_math.h"
#include "audio/forge_audio.h"

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
    if (frames > (SDL_MAX_SINT32 / 2)) {
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
    ASSERT_NEAR(out[0], forge_audio_tanhf(0.25f), 0.01f);
    ASSERT_NEAR(out[1], forge_audio_tanhf(0.25f), 0.01f);

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
    float expected = forge_audio_tanhf(0.25f);
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
    float two_ch = forge_audio_tanhf(1.0f);
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
    float y = forge_audio_tanhf(x);
    ASSERT_NEAR(y, x, 0.005f);

    /* Even at 0.3 the error is small */
    x = 0.3f;
    y = forge_audio_tanhf(x);
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
    ASSERT_NEAR(out[0], forge_audio_tanhf(0.05f), 0.01f);

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

/* ── Helper: create a synthetic WAV file on disk ──────────────────── */

/* Write exactly `size` bytes or return false. */
static bool write_all(SDL_IOStream *io, const void *data, size_t size)
{
    return io && SDL_WriteIO(io, data, size) == size;
}

/* WAV fixture format constants */
#define TEST_WAV_CHANNELS       2
#define TEST_WAV_SAMPLE_RATE    44100
#define TEST_WAV_BITS           32
#define TEST_WAV_FMT_CHUNK_SIZE 16

/* Write a test WAV with position-dependent ramp data:
 *   left  = 0.25 + 0.50 * (i / frames)
 *   right = 0.75 - 0.25 * (i / frames) */
static bool write_test_wav(const char *path, int frames)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) return false;

    int channels = TEST_WAV_CHANNELS;
    int sample_rate = TEST_WAV_SAMPLE_RATE;
    int bits = TEST_WAV_BITS;
    int data_size = (int)((size_t)frames * (size_t)channels * (size_t)(bits / 8));
    int fmt_chunk_size = TEST_WAV_FMT_CHUNK_SIZE;
    int riff_size = 4 + (8 + fmt_chunk_size) + (8 + data_size);

    /* RIFF header */
    Uint8 buf4[4];
    buf4[0] = (Uint8)(riff_size & 0xFF);
    buf4[1] = (Uint8)((riff_size >> 8) & 0xFF);
    buf4[2] = (Uint8)((riff_size >> 16) & 0xFF);
    buf4[3] = (Uint8)((riff_size >> 24) & 0xFF);
    if (!write_all(io, "RIFF", 4) || !write_all(io, buf4, 4) ||
        !write_all(io, "WAVE", 4)) {
        SDL_CloseIO(io); return false;
    }

    /* fmt chunk */
    Uint8 fmt[16];
    fmt[0] = 3; fmt[1] = 0;   /* IEEE float */
    fmt[2] = 2; fmt[3] = 0;   /* 2 channels */
    fmt[4] = (Uint8)(sample_rate & 0xFF);
    fmt[5] = (Uint8)((sample_rate >> 8) & 0xFF);
    fmt[6] = (Uint8)((sample_rate >> 16) & 0xFF);
    fmt[7] = (Uint8)((sample_rate >> 24) & 0xFF);
    int byte_rate = sample_rate * channels * (bits / 8);
    fmt[8]  = (Uint8)(byte_rate & 0xFF);
    fmt[9]  = (Uint8)((byte_rate >> 8) & 0xFF);
    fmt[10] = (Uint8)((byte_rate >> 16) & 0xFF);
    fmt[11] = (Uint8)((byte_rate >> 24) & 0xFF);
    int block_align = channels * (bits / 8);
    fmt[12] = (Uint8)(block_align & 0xFF);
    fmt[13] = (Uint8)((block_align >> 8) & 0xFF);
    fmt[14] = (Uint8)(TEST_WAV_BITS & 0xFF); fmt[15] = (Uint8)((TEST_WAV_BITS >> 8) & 0xFF);
    buf4[0] = (Uint8)(fmt_chunk_size & 0xFF);
    buf4[1] = (Uint8)((fmt_chunk_size >> 8) & 0xFF);
    buf4[2] = 0; buf4[3] = 0;
    if (!write_all(io, "fmt ", 4) || !write_all(io, buf4, 4) ||
        !write_all(io, fmt, 16)) {
        SDL_CloseIO(io); return false;
    }

    /* data chunk */
    buf4[0] = (Uint8)(data_size & 0xFF);
    buf4[1] = (Uint8)((data_size >> 8) & 0xFF);
    buf4[2] = (Uint8)((data_size >> 16) & 0xFF);
    buf4[3] = (Uint8)((data_size >> 24) & 0xFF);
    if (!write_all(io, "data", 4) || !write_all(io, buf4, 4)) {
        SDL_CloseIO(io); return false;
    }

    /* Write F32 stereo samples */
    for (int i = 0; i < frames; i++) {
        float left  = 0.25f + 0.5f * ((float)i / (float)frames);
        float right = 0.75f - 0.25f * ((float)i / (float)frames);
        if (!write_all(io, &left, 4) || !write_all(io, &right, 4)) {
            SDL_CloseIO(io); return false;
        }
    }

    SDL_CloseIO(io);
    return true;
}

/* Write a test WAV with constant amplitude per channel. */
static bool write_test_wav_const(const char *path, int frames,
                                  float left_val, float right_val)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) return false;

    int channels = TEST_WAV_CHANNELS;
    int sample_rate = TEST_WAV_SAMPLE_RATE;
    int bits = TEST_WAV_BITS;
    int data_size = (int)((size_t)frames * (size_t)channels * (size_t)(bits / 8));
    int fmt_chunk_size = TEST_WAV_FMT_CHUNK_SIZE;
    int riff_size = 4 + (8 + fmt_chunk_size) + (8 + data_size);

    Uint8 buf4[4];
    buf4[0] = (Uint8)(riff_size & 0xFF);
    buf4[1] = (Uint8)((riff_size >> 8) & 0xFF);
    buf4[2] = (Uint8)((riff_size >> 16) & 0xFF);
    buf4[3] = (Uint8)((riff_size >> 24) & 0xFF);
    if (!write_all(io, "RIFF", 4) || !write_all(io, buf4, 4) ||
        !write_all(io, "WAVE", 4)) {
        SDL_CloseIO(io); return false;
    }

    Uint8 fmt[16];
    fmt[0] = 3; fmt[1] = 0;
    fmt[2] = 2; fmt[3] = 0;
    fmt[4] = (Uint8)(sample_rate & 0xFF);
    fmt[5] = (Uint8)((sample_rate >> 8) & 0xFF);
    fmt[6] = (Uint8)((sample_rate >> 16) & 0xFF);
    fmt[7] = (Uint8)((sample_rate >> 24) & 0xFF);
    int byte_rate = sample_rate * channels * (bits / 8);
    fmt[8]  = (Uint8)(byte_rate & 0xFF);
    fmt[9]  = (Uint8)((byte_rate >> 8) & 0xFF);
    fmt[10] = (Uint8)((byte_rate >> 16) & 0xFF);
    fmt[11] = (Uint8)((byte_rate >> 24) & 0xFF);
    int block_align = channels * (bits / 8);
    fmt[12] = (Uint8)(block_align & 0xFF);
    fmt[13] = (Uint8)((block_align >> 8) & 0xFF);
    fmt[14] = (Uint8)(TEST_WAV_BITS & 0xFF); fmt[15] = (Uint8)((TEST_WAV_BITS >> 8) & 0xFF);
    buf4[0] = (Uint8)(fmt_chunk_size & 0xFF);
    buf4[1] = (Uint8)((fmt_chunk_size >> 8) & 0xFF);
    buf4[2] = 0; buf4[3] = 0;
    if (!write_all(io, "fmt ", 4) || !write_all(io, buf4, 4) ||
        !write_all(io, fmt, 16)) {
        SDL_CloseIO(io); return false;
    }

    buf4[0] = (Uint8)(data_size & 0xFF);
    buf4[1] = (Uint8)((data_size >> 8) & 0xFF);
    buf4[2] = (Uint8)((data_size >> 16) & 0xFF);
    buf4[3] = (Uint8)((data_size >> 24) & 0xFF);
    if (!write_all(io, "data", 4) || !write_all(io, buf4, 4)) {
        SDL_CloseIO(io); return false;
    }

    for (int i = 0; i < frames; i++) {
        if (!write_all(io, &left_val, 4) || !write_all(io, &right_val, 4)) {
            SDL_CloseIO(io); return false;
        }
    }

    SDL_CloseIO(io);
    return true;
}

/* Path for temporary test WAV file */
#ifdef _WIN32
#define TEST_WAV_PATH "test_stream_tmp.wav"
#define TEST_WAV_PATH2 "test_stream_tmp2.wav"
#else
#define TEST_WAV_PATH "/tmp/test_stream_tmp.wav"
#define TEST_WAV_PATH2 "/tmp/test_stream_tmp2.wav"
#endif

/* ── Streaming tests (Lesson 05) ──────────────────────────────────── */

static void test_stream_open_close(void)
{
    TEST("stream open/close");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 8192));

    ForgeAudioStream s;
    ASSERT_TRUE(forge_audio_stream_open(TEST_WAV_PATH, &s));
    ASSERT_TRUE(s.playing);
    ASSERT_TRUE(!s.finished);
    ASSERT_TRUE(s.ring != NULL);
    ASSERT_TRUE(s.converter != NULL);
    ASSERT_TRUE(s.total_frames == 8192);
    ASSERT_TRUE(s.duration > 0.0f);

    forge_audio_stream_close(&s);
    ASSERT_TRUE(s.ring == NULL);
    ASSERT_TRUE(s.io == NULL);

    END_TEST();
}

static void test_stream_read_all(void)
{
    TEST("stream read entire file matches load_wav");

    int frames = 4096;
    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, frames));

    /* Load via stream */
    ForgeAudioStream s;
    ASSERT_TRUE(forge_audio_stream_open(TEST_WAV_PATH, &s));

    float *stream_out = (float *)SDL_calloc((size_t)(frames * 2), sizeof(float));
    ASSERT_TRUE(stream_out != NULL);

    int total_read = 0;
    for (int iter = 0; iter < 20 && total_read < frames; iter++) {
        forge_audio_stream_update(&s);
        int got = forge_audio_stream_read(&s, stream_out + total_read * 2,
                                          frames - total_read);
        total_read += got;
        if (got == 0 && s.finished) break;
    }

    /* Load via load_wav for comparison */
    ForgeAudioBuffer buf;
    ASSERT_TRUE(forge_audio_load_wav(TEST_WAV_PATH, &buf));

    /* Compare — both should have the same data.  The stream reads
     * additively so we need to check against the buffer. */
    int compare_frames = total_read < forge_audio_buffer_frames(&buf)
                       ? total_read : forge_audio_buffer_frames(&buf);
    ASSERT_TRUE(compare_frames > 0);

    bool match = true;
    for (int i = 0; i < compare_frames * 2; i++) {
        float diff = stream_out[i] - buf.data[i];
        if (diff < 0.0f) diff = -diff;
        if (diff > 0.01f) {
            match = false;
            break;
        }
    }
    ASSERT_TRUE(match);

    SDL_free(stream_out);
    forge_audio_buffer_free(&buf);
    forge_audio_stream_close(&s);

    END_TEST();
}

static void test_stream_loop_wrap(void)
{
    TEST("stream loop wraps back to start");

    /* Small file — 1024 frames, loop enabled */
    int frames = 1024;
    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, frames));

    ForgeAudioStream s;
    ASSERT_TRUE(forge_audio_stream_open(TEST_WAV_PATH, &s));
    forge_audio_stream_set_loop(&s, 0);
    ASSERT_TRUE(s.looping);

    /* Read more than the file length — should wrap */
    float *out = (float *)SDL_calloc(4096 * 2, sizeof(float));
    ASSERT_TRUE(out != NULL);

    int total_read = 0;
    for (int iter = 0; iter < 40; iter++) {
        forge_audio_stream_update(&s);
        int got = forge_audio_stream_read(&s, out, 512);
        total_read += got;
        if (total_read >= frames * 2) break; /* read 2x the file */
    }

    /* Should have read more than the file length */
    ASSERT_TRUE(total_read >= frames);
    /* Stream should still be playing (looping) */
    ASSERT_TRUE(s.playing);
    ASSERT_TRUE(!s.finished);

    SDL_free(out);
    forge_audio_stream_close(&s);

    END_TEST();
}

static void test_stream_loop_with_intro(void)
{
    TEST("stream loop-with-intro skips intro on wrap");

    int frames = 2048;
    int intro_frames = 512;
    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, frames));

    ForgeAudioStream s;
    ASSERT_TRUE(forge_audio_stream_open(TEST_WAV_PATH, &s));
    forge_audio_stream_set_loop(&s, intro_frames);
    ASSERT_TRUE(s.loop_start_frame == intro_frames);

    /* Read the entire file + more to trigger wrap */
    float *out = (float *)SDL_calloc(4096 * 2, sizeof(float));
    ASSERT_TRUE(out != NULL);

    int total_read = 0;
    for (int iter = 0; iter < 40; iter++) {
        forge_audio_stream_update(&s);
        int got = forge_audio_stream_read(&s, out, 512);
        total_read += got;
        if (total_read >= frames * 2) break;
    }

    ASSERT_TRUE(s.playing);
    ASSERT_TRUE(!s.finished);

    /* The bulk read above triggered at least one natural wrap.
     * Now seek to just before the end, let the stream wrap naturally,
     * and verify the post-wrap audio matches the ramp at intro_frames.
     *
     * Approach: seek to (frames - 16), drain the remaining ~16 frames
     * plus whatever wraps around, then check the sample values. */
    forge_audio_stream_seek(&s, frames - 16);
    {
        /* Drain the pre-wrap portion (~16 frames) */
        float drain[32 * 2];
        int drain_iter;
        SDL_memset(drain, 0, sizeof(drain));
        for (drain_iter = 0; drain_iter < 10; drain_iter++) {
            forge_audio_stream_update(&s);
            forge_audio_stream_read(&s, drain, 32);
        }
    }
    /* Now the stream should have wrapped.  Read fresh post-wrap data. */
    forge_audio_stream_update(&s);
    float post_wrap[64 * 2];
    SDL_memset(post_wrap, 0, sizeof(post_wrap));
    int got = forge_audio_stream_read(&s, post_wrap, 64);
    ASSERT_TRUE(got > 0);

    /* Verify post-wrap sample is in the loop region, NOT in the intro.
     * Ramp formula: left = 0.25 + 0.5 * (frame / total_frames).
     * Intro region: [0.25, 0.375).  Loop region: [0.375, 0.75].
     * The ring buffer prefetch means we can't predict the exact frame,
     * but the value must be >= intro start value (0.375) to confirm
     * the wrap landed past the intro. */
    float intro_start_val = 0.25f + 0.5f * ((float)intro_frames / (float)frames);
    ASSERT_TRUE(post_wrap[0] >= intro_start_val - 0.01f);

    SDL_free(out);
    forge_audio_stream_close(&s);

    END_TEST();
}

static void test_stream_seek(void)
{
    TEST("stream seek repositions and produces output");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 8192));

    ForgeAudioStream s;
    ASSERT_TRUE(forge_audio_stream_open(TEST_WAV_PATH, &s));

    /* Seek to middle — cursor_frame advances past 4096 during refill,
     * so we check playing/finished state and that data is readable */
    forge_audio_stream_seek(&s, 4096);
    ASSERT_TRUE(s.playing);
    ASSERT_TRUE(!s.finished);

    /* Read some data — should work */
    float out[512 * 2];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_stream_update(&s);
    int got = forge_audio_stream_read(&s, out, 512);
    ASSERT_TRUE(got > 0);

    forge_audio_stream_close(&s);

    END_TEST();
}

static void test_stream_progress(void)
{
    TEST("stream progress reports [0..1]");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));

    ForgeAudioStream s;
    ASSERT_TRUE(forge_audio_stream_open(TEST_WAV_PATH, &s));

    float p0 = forge_audio_stream_progress(&s);
    /* After opening, some data has been pre-read but not consumed,
     * so progress should be near 0 */
    ASSERT_TRUE(p0 >= 0.0f && p0 <= 1.0f);

    /* Read most of the file */
    float *out = (float *)SDL_calloc(4096 * 2, sizeof(float));
    ASSERT_TRUE(out != NULL);
    for (int iter = 0; iter < 20; iter++) {
        forge_audio_stream_update(&s);
        forge_audio_stream_read(&s, out, 512);
    }

    float p1 = forge_audio_stream_progress(&s);
    ASSERT_TRUE(p1 >= 0.0f && p1 <= 1.0f);
    /* Progress should have advanced */
    ASSERT_TRUE(p1 > p0 || s.finished);

    SDL_free(out);
    forge_audio_stream_close(&s);

    END_TEST();
}

static void test_stream_invalid_path(void)
{
    TEST("stream open with invalid path returns false");

    ForgeAudioStream s;
    ASSERT_TRUE(!forge_audio_stream_open("nonexistent_file.wav", &s));
    ASSERT_TRUE(s.ring == NULL);
    ASSERT_TRUE(s.io == NULL);

    END_TEST();
}

static void test_stream_finished_non_looping(void)
{
    TEST("stream finished flag set for non-looping");

    /* Small file — easy to exhaust */
    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 512));

    ForgeAudioStream s;
    ASSERT_TRUE(forge_audio_stream_open(TEST_WAV_PATH, &s));
    /* Default: not looping */
    ASSERT_TRUE(!s.looping);

    float *out = (float *)SDL_calloc(4096 * 2, sizeof(float));
    ASSERT_TRUE(out != NULL);

    /* Read until done */
    for (int iter = 0; iter < 20; iter++) {
        forge_audio_stream_update(&s);
        forge_audio_stream_read(&s, out, 1024);
    }

    /* Should eventually finish */
    ASSERT_TRUE(s.finished || s.ring_available == 0);

    SDL_free(out);
    forge_audio_stream_close(&s);

    END_TEST();
}

static void test_stream_loop_start_clamped(void)
{
    TEST("stream loop_start_frame clamped to total_frames");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 1024));

    ForgeAudioStream s;
    ASSERT_TRUE(forge_audio_stream_open(TEST_WAV_PATH, &s));

    /* Set intro_frames beyond total — should be clamped */
    forge_audio_stream_set_loop(&s, 999999);
    ASSERT_TRUE(s.loop_start_frame <= s.total_frames);

    /* Update should not hang even with clamped loop point */
    float out[512 * 2];
    SDL_memset(out, 0, sizeof(out));
    for (int iter = 0; iter < 10; iter++) {
        forge_audio_stream_update(&s);
        forge_audio_stream_read(&s, out, 512);
    }
    ASSERT_TRUE(s.playing);

    forge_audio_stream_close(&s);

    END_TEST();
}

static void test_stream_looping_no_hang(void)
{
    TEST("stream looping update does not hang on zero-progress");

    /* Very small file — ring pre-fill may consume entire file */
    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 64));

    ForgeAudioStream s;
    ASSERT_TRUE(forge_audio_stream_open(TEST_WAV_PATH, &s));
    forge_audio_stream_set_loop(&s, 0);

    /* Multiple updates should return promptly, not spin */
    float out[256 * 2];
    SDL_memset(out, 0, sizeof(out));
    for (int iter = 0; iter < 20; iter++) {
        forge_audio_stream_update(&s);
        forge_audio_stream_read(&s, out, 256);
    }
    /* If we got here, no infinite loop occurred */
    ASSERT_TRUE(s.playing);

    forge_audio_stream_close(&s);

    END_TEST();
}

/* ── Crossfader tests (Lesson 05) ─────────────────────────────────── */

static void test_crossfader_single_track(void)
{
    TEST("crossfader single track plays without crash");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));

    ForgeAudioCrossfader xf;
    forge_audio_crossfader_init(&xf);
    ASSERT_TRUE(forge_audio_crossfader_play(&xf, TEST_WAV_PATH, 0.0f, true));

    float out[512 * 2];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_crossfader_update(&xf, 1.0f / 60.0f);
    forge_audio_crossfader_read(&xf, out, 512);

    /* Should have produced non-zero output */
    bool has_signal = false;
    for (int i = 0; i < 512 * 2; i++) {
        if (out[i] > 0.001f || out[i] < -0.001f) { has_signal = true; break; }
    }
    ASSERT_TRUE(has_signal);

    forge_audio_crossfader_close(&xf);

    END_TEST();
}

static void test_crossfader_blend_50(void)
{
    TEST("crossfader at 50% blend mixes distinct sources");

    /* Two WAVs with distinct constant amplitudes so we can verify blending */
    ASSERT_TRUE(write_test_wav_const(TEST_WAV_PATH,  4096, 0.2f, 0.2f));
    ASSERT_TRUE(write_test_wav_const(TEST_WAV_PATH2, 4096, 0.8f, 0.8f));

    ForgeAudioCrossfader xf;
    forge_audio_crossfader_init(&xf);
    ASSERT_TRUE(forge_audio_crossfader_play(&xf, TEST_WAV_PATH, 0.0f, true));

    /* Update to establish first track */
    forge_audio_crossfader_update(&xf, 1.0f / 60.0f);

    /* Start crossfade to second track */
    ASSERT_TRUE(forge_audio_crossfader_play(&xf, TEST_WAV_PATH2, 1.0f, true));
    ASSERT_TRUE(xf.fading);

    /* Advance to 50% */
    forge_audio_crossfader_update(&xf, 0.5f);
    ASSERT_NEAR(xf.fade_progress, 0.5f, 0.05f);

    float out[512 * 2];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_crossfader_read(&xf, out, 512);

    /* At 50% equal-power: gain_out = sqrt(0.5) ≈ 0.707, gain_in = sqrt(0.5).
     * Expected blend: (0.2 + 0.8) * sqrt(0.5) ≈ 0.7071.
     * Assert each non-silent sample is close to this value. */
    float expected_blend = (0.2f + 0.8f) * SDL_sqrtf(0.5f);
    bool has_signal = false;
    bool blend_correct = true;
    {
        int i;
        for (i = 0; i < 512 * 2; i++) {
            float v = out[i];
            if (v > 0.001f || v < -0.001f) {
                has_signal = true;
                float err = v - expected_blend;
                if (err < 0.0f) err = -err;
                if (err > 0.05f) blend_correct = false;
            }
        }
    }
    ASSERT_TRUE(has_signal);
    ASSERT_TRUE(blend_correct);

    forge_audio_crossfader_close(&xf);

    END_TEST();
}

static void test_crossfader_fade_complete(void)
{
    TEST("crossfader fade completes and swaps active");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));
    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH2, 4096));

    ForgeAudioCrossfader xf;
    forge_audio_crossfader_init(&xf);
    ASSERT_TRUE(forge_audio_crossfader_play(&xf, TEST_WAV_PATH, 0.0f, true));
    forge_audio_crossfader_update(&xf, 1.0f / 60.0f);

    int initial_active = xf.active;
    ASSERT_TRUE(forge_audio_crossfader_play(&xf, TEST_WAV_PATH2, 0.5f, true));

    /* Advance past the fade duration */
    forge_audio_crossfader_update(&xf, 0.6f);

    ASSERT_TRUE(!xf.fading);
    /* Active should have swapped */
    ASSERT_TRUE(xf.active != initial_active);

    forge_audio_crossfader_close(&xf);

    END_TEST();
}

/* ── Layer group tests (Lesson 05) ─────────────────────────────────── */

static void test_layer_group_single_layer(void)
{
    TEST("layer group single layer produces output");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));

    ForgeAudioLayerGroup group;
    forge_audio_layer_group_init(&group);
    group.playing = true;

    int idx = forge_audio_layer_group_add(&group, TEST_WAV_PATH, 1.0f);
    ASSERT_TRUE(idx == 0);
    ASSERT_TRUE(group.layer_count == 1);

    float out[512 * 2];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_layer_group_update(&group, 1.0f / 60.0f);
    forge_audio_layer_group_read(&group, out, 512);

    bool has_signal = false;
    for (int i = 0; i < 512 * 2; i++) {
        if (out[i] > 0.001f || out[i] < -0.001f) { has_signal = true; break; }
    }
    ASSERT_TRUE(has_signal);

    forge_audio_layer_group_close(&group);

    END_TEST();
}

static void test_layer_group_weight_zero_silence(void)
{
    TEST("layer group weight=0 produces silence");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));

    ForgeAudioLayerGroup group;
    forge_audio_layer_group_init(&group);
    group.playing = true;

    forge_audio_layer_group_add(&group, TEST_WAV_PATH, 0.0f);

    float out[512 * 2];
    SDL_memset(out, 0, sizeof(out));
    forge_audio_layer_group_update(&group, 1.0f / 60.0f);
    forge_audio_layer_group_read(&group, out, 512);

    /* Weight=0 should skip this layer */
    bool all_zero = true;
    for (int i = 0; i < 512 * 2; i++) {
        if (out[i] > 0.0001f || out[i] < -0.0001f) { all_zero = false; break; }
    }
    ASSERT_TRUE(all_zero);

    forge_audio_layer_group_close(&group);

    END_TEST();
}

static void test_layer_group_weight_fade(void)
{
    TEST("layer group weight fade reaches target");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));

    ForgeAudioLayerGroup group;
    forge_audio_layer_group_init(&group);
    group.playing = true;

    forge_audio_layer_group_add(&group, TEST_WAV_PATH, 0.0f);
    forge_audio_layer_group_fade_weight(&group, 0, 1.0f, 0.5f);

    /* Advance past fade duration */
    for (int i = 0; i < 60; i++) {
        forge_audio_layer_group_update(&group, 1.0f / 60.0f);
    }

    ASSERT_NEAR(group.layers[0].weight, 1.0f, 0.05f);

    forge_audio_layer_group_close(&group);

    END_TEST();
}

static void test_layer_group_progress(void)
{
    TEST("layer group progress reports [0..1]");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));

    ForgeAudioLayerGroup group;
    forge_audio_layer_group_init(&group);
    group.playing = true;

    forge_audio_layer_group_add(&group, TEST_WAV_PATH, 1.0f);

    float p = forge_audio_layer_group_progress(&group);
    ASSERT_TRUE(p >= 0.0f && p <= 1.0f);

    forge_audio_layer_group_close(&group);

    END_TEST();
}

static void test_layer_group_weight_clamped(void)
{
    TEST("layer group add clamps weight to [0,1]");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));

    ForgeAudioLayerGroup group;
    forge_audio_layer_group_init(&group);
    group.playing = true;

    /* Negative weight should be clamped to 0 */
    int idx0 = forge_audio_layer_group_add(&group, TEST_WAV_PATH, -5.0f);
    ASSERT_TRUE(idx0 == 0);
    ASSERT_NEAR(group.layers[0].weight, 0.0f, EPSILON);

    forge_audio_layer_group_close(&group);

    /* Weight > 1 should be clamped to 1 */
    forge_audio_layer_group_init(&group);
    group.playing = true;
    int idx1 = forge_audio_layer_group_add(&group, TEST_WAV_PATH, 3.0f);
    ASSERT_TRUE(idx1 == 0);
    ASSERT_NEAR(group.layers[0].weight, 1.0f, EPSILON);

    forge_audio_layer_group_close(&group);

    END_TEST();
}

/* Helper: write a minimal WAV with configurable fmt fields for rejection tests.
 * Returns true if the file was written successfully. */
static bool write_rejection_wav(const char *path, Uint16 format_tag,
                                Uint16 channels, Uint32 sample_rate)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (!io) return false;

    int data_size = 64 * (int)channels * (TEST_WAV_BITS / 8);
    int fmt_chunk_size = TEST_WAV_FMT_CHUNK_SIZE;
    int riff_size = 4 + (8 + fmt_chunk_size) + (8 + data_size);
    bool ok = true;

    /* RIFF header */
    Uint8 buf4[4];
    buf4[0] = (Uint8)(riff_size & 0xFF);
    buf4[1] = (Uint8)((riff_size >> 8) & 0xFF);
    buf4[2] = (Uint8)((riff_size >> 16) & 0xFF);
    buf4[3] = (Uint8)((riff_size >> 24) & 0xFF);
    ok = ok && write_all(io, "RIFF", 4);
    ok = ok && write_all(io, buf4, 4);
    ok = ok && write_all(io, "WAVE", 4);

    /* fmt chunk */
    buf4[0] = (Uint8)(fmt_chunk_size & 0xFF);
    buf4[1] = 0; buf4[2] = 0; buf4[3] = 0;
    ok = ok && write_all(io, "fmt ", 4);
    ok = ok && write_all(io, buf4, 4);

    Uint8 fmt[16];
    fmt[0] = (Uint8)(format_tag & 0xFF);
    fmt[1] = (Uint8)((format_tag >> 8) & 0xFF);
    fmt[2] = (Uint8)(channels & 0xFF);
    fmt[3] = (Uint8)((channels >> 8) & 0xFF);
    fmt[4] = (Uint8)(sample_rate & 0xFF);
    fmt[5] = (Uint8)((sample_rate >> 8) & 0xFF);
    fmt[6] = (Uint8)((sample_rate >> 16) & 0xFF);
    fmt[7] = (Uint8)((sample_rate >> 24) & 0xFF);
    /* byte_rate and block_align — computed from parameters */
    Uint16 block_align = channels * (TEST_WAV_BITS / 8);
    Uint32 byte_rate = sample_rate * (Uint32)block_align;
    fmt[8]  = (Uint8)(byte_rate & 0xFF);
    fmt[9]  = (Uint8)((byte_rate >> 8) & 0xFF);
    fmt[10] = (Uint8)((byte_rate >> 16) & 0xFF);
    fmt[11] = (Uint8)((byte_rate >> 24) & 0xFF);
    fmt[12] = (Uint8)(block_align & 0xFF);
    fmt[13] = (Uint8)((block_align >> 8) & 0xFF);
    fmt[14] = (Uint8)(TEST_WAV_BITS & 0xFF); fmt[15] = (Uint8)((TEST_WAV_BITS >> 8) & 0xFF);
    ok = ok && write_all(io, fmt, 16);

    /* data chunk */
    buf4[0] = (Uint8)(data_size & 0xFF);
    buf4[1] = (Uint8)((data_size >> 8) & 0xFF);
    buf4[2] = 0; buf4[3] = 0;
    ok = ok && write_all(io, "data", 4);
    ok = ok && write_all(io, buf4, 4);

    /* Data is always 32-bit float (TEST_WAV_BITS == 32).  The rejection
     * targets fmt-header fields, not the data payload. */
    float zero = 0.0f;
    for (int i = 0; i < 64 * (int)channels; i++) {
        ok = ok && write_all(io, &zero, 4);
    }
    SDL_CloseIO(io);
    return ok;
}

static void test_stream_zero_rate_wav_rejected(void)
{
    TEST("stream rejects WAV with zero sample rate");

    /* Write a WAV with sample_rate = 0 — should be rejected by the parser */
    ASSERT_TRUE(write_rejection_wav(TEST_WAV_PATH, 3, 2, 0));

    /* Opening should fail */
    ForgeAudioStream s;
    ASSERT_TRUE(!forge_audio_stream_open(TEST_WAV_PATH, &s));

    END_TEST();
}

static void test_stream_excessive_channels_rejected(void)
{
    TEST("stream rejects WAV with excessive channel count");

    /* Write a WAV with channels = 99 — exceeds upper bound (8) */
    ASSERT_TRUE(write_rejection_wav(TEST_WAV_PATH, 3, 99, 44100));

    ForgeAudioStream s;
    ASSERT_TRUE(!forge_audio_stream_open(TEST_WAV_PATH, &s));

    END_TEST();
}

/* ── Total-frames overflow guard (PR #329 fix) ────────────────────── */

static void test_stream_total_frames_nonnegative(void)
{
    /* Verify that total_frames and duration are positive after opening.
     * The overflow guard (Uint64 clamp to SDL_MAX_SINT32) prevents negative
     * values on large files, but we can only test the normal path here. */
    TEST("stream total_frames is non-negative");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 8192));

    ForgeAudioStream s;
    ASSERT_TRUE(forge_audio_stream_open(TEST_WAV_PATH, &s));
    ASSERT_TRUE(s.total_frames > 0);
    ASSERT_TRUE(s.total_frames == 8192);
    ASSERT_TRUE(s.duration > 0.0f);

    forge_audio_stream_close(&s);

    END_TEST();
}

/* ── Public API and safety tests (PR #329 round 5) ─────────────────── */

/* Verify the public forge_audio_tanhf function works at typical values. */
static void test_tanhf_public_api(void)
{
    TEST("forge_audio_tanhf public API");

    /* tanh(0) = 0 */
    ASSERT_NEAR(forge_audio_tanhf(0.0f), 0.0f, 1e-6f);

    /* tanh(1) ≈ 0.7616 */
    ASSERT_NEAR(forge_audio_tanhf(1.0f), 0.7616f, 0.001f);

    /* tanh(-1) ≈ -0.7616 */
    ASSERT_NEAR(forge_audio_tanhf(-1.0f), -0.7616f, 0.001f);

    /* Symmetry: tanh(-x) = -tanh(x) */
    float val = forge_audio_tanhf(2.5f);
    ASSERT_NEAR(forge_audio_tanhf(-2.5f), -val, 1e-6f);

    END_TEST();
}

/* Verify extreme value clamping in forge_audio_tanhf. */
static void test_tanhf_clamp_extremes(void)
{
    TEST("forge_audio_tanhf clamp extremes");

    /* Values beyond ±10 should clamp to ±1.0 exactly. */
    ASSERT_NEAR(forge_audio_tanhf(100.0f), 1.0f, 0.0f);
    ASSERT_NEAR(forge_audio_tanhf(-100.0f), -1.0f, 0.0f);
    ASSERT_NEAR(forge_audio_tanhf(10.1f), 1.0f, 0.0f);
    ASSERT_NEAR(forge_audio_tanhf(-10.1f), -1.0f, 0.0f);

    /* Just under the clamp threshold should not clamp. */
    float near_clamp = forge_audio_tanhf(3.0f);
    ASSERT_TRUE(near_clamp < 1.0f);
    ASSERT_TRUE(near_clamp > 0.99f);

    END_TEST();
}

/* Verify the FORGE_AUDIO_SYNC_THRESHOLD_FLOOR constant exists and is sane. */
static void test_sync_threshold_floor_constant(void)
{
    TEST("FORGE_AUDIO_SYNC_THRESHOLD_FLOOR constant");

    /* The floor should be a small positive value. */
    ASSERT_TRUE(FORGE_AUDIO_SYNC_THRESHOLD_FLOOR > 0.0f);
    ASSERT_TRUE(FORGE_AUDIO_SYNC_THRESHOLD_FLOOR < 0.001f);

    /* For a very long track (~8 million frames), the computed threshold
     * (2.0 / total_frames) would be ~2.5e-7, which is below the floor.
     * The floor prevents float precision from causing false re-seeks. */
    float long_track_threshold = 2.0f / 8000000.0f;
    ASSERT_TRUE(long_track_threshold < FORGE_AUDIO_SYNC_THRESHOLD_FLOOR);

    END_TEST();
}

/* Verify that a crossfader can be closed and re-initialized. */
static void test_crossfader_close_reinit(void)
{
    TEST("crossfader close then reinit");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));

    ForgeAudioCrossfader xf;
    forge_audio_crossfader_init(&xf);

    /* Load a track and verify it works. */
    ASSERT_TRUE(forge_audio_crossfader_play(&xf, TEST_WAV_PATH, 0.0f, true));
    ASSERT_TRUE(xf.streams[xf.active].playing);

    /* Close the crossfader. */
    forge_audio_crossfader_close(&xf);

    /* Re-initialize and verify it works again. */
    forge_audio_crossfader_init(&xf);
    ASSERT_TRUE(forge_audio_crossfader_play(&xf, TEST_WAV_PATH, 0.0f, true));
    ASSERT_TRUE(xf.streams[xf.active].playing);

    forge_audio_crossfader_close(&xf);

    END_TEST();
}

/* Verify NaN/Inf weights are sanitized to safe values. */
static void test_layer_group_nan_weight(void)
{
    TEST("layer group NaN/Inf weight sanitized");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));

    ForgeAudioLayerGroup group;
    forge_audio_layer_group_init(&group);
    group.playing = true;

    /* NaN weight should be sanitized to 0 */
    volatile float zero = 0.0f;
    float nan_val = zero / zero;  /* NaN */
    int idx = forge_audio_layer_group_add(&group, TEST_WAV_PATH, nan_val);
    ASSERT_TRUE(idx == 0);
    ASSERT_NEAR(group.layers[0].weight, 0.0f, EPSILON);
    ASSERT_NEAR(group.layers[0].weight_target, 0.0f, EPSILON);

    /* NaN fade target should be sanitized */
    forge_audio_layer_group_fade_weight(&group, 0, nan_val, 1.0f);
    ASSERT_NEAR(group.layers[0].weight_target, 0.0f, EPSILON);

    /* NaN fade duration should snap immediately */
    forge_audio_layer_group_fade_weight(&group, 0, 0.5f, nan_val);
    ASSERT_NEAR(group.layers[0].weight, 0.5f, EPSILON);
    ASSERT_NEAR(group.layers[0].weight_rate, 0.0f, EPSILON);

    forge_audio_layer_group_close(&group);

    END_TEST();
}

/* Verify that 64-bit float WAVs are rejected by the streaming parser. */
static void test_stream_64bit_float_rejected(void)
{
    TEST("streaming rejects 64-bit float WAV");

    /* Write a WAV with format_tag=3 (IEEE float) but 64 bits per sample.
     * Reuse write_rejection_wav which writes format_tag=1 PCM header, then
     * manually craft a 64-bit float header. */
    const char *path = TEST_WAV_PATH;
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    ASSERT_TRUE(io != NULL);

    /* Minimal RIFF WAV with format_tag=3 (float), 64 bits, 1 channel.
     * Derived: block_align = channels * (bits/8) = 1 * 8 = 8
     *          byte_rate = sample_rate * block_align = 44100 * 8 = 352800 */
    Uint32 byte_rate = 352800;  /* 44100 * 8 */
    Uint8 header[] = {
        'R','I','F','F',  0,0,0,0,  /* RIFF chunk (size filled below) */
        'W','A','V','E',
        'f','m','t',' ',  16,0,0,0, /* fmt chunk, 16 bytes */
        3,0,                         /* format_tag = 3 (IEEE float) */
        1,0,                         /* channels = 1 */
        0x44,0xAC,0,0,              /* sample_rate = 44100 */
        (Uint8)(byte_rate),
        (Uint8)(byte_rate >> 8),
        (Uint8)(byte_rate >> 16),
        (Uint8)(byte_rate >> 24),    /* byte_rate = 352800 */
        8,0,                         /* block_align = 8 */
        64,0,                        /* bits_per_sample = 64 */
        'd','a','t','a',  8,0,0,0,  /* data chunk, 8 bytes */
        0,0,0,0, 0,0,0,0            /* one 64-bit float sample */
    };
    /* Fix RIFF size = file_size - 8 */
    Uint32 riff_size = (Uint32)(sizeof(header) - 8);
    header[4] = (Uint8)(riff_size);
    header[5] = (Uint8)(riff_size >> 8);
    header[6] = (Uint8)(riff_size >> 16);
    header[7] = (Uint8)(riff_size >> 24);

    ASSERT_TRUE(SDL_WriteIO(io, header, sizeof(header)));
    SDL_CloseIO(io);

    ForgeAudioStream s;
    bool opened = forge_audio_stream_open(path, &s);
    ASSERT_TRUE(!opened);  /* Should be rejected */

    SDL_RemovePath(path);

    END_TEST();
}

/* Verify NaN/negative dt is rejected by crossfader_update and layer_group_update. */
static void test_crossfader_nan_dt(void)
{
    TEST("crossfader_update rejects NaN/negative dt");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));

    ForgeAudioCrossfader xf;
    forge_audio_crossfader_init(&xf);
    ASSERT_TRUE(forge_audio_crossfader_play(&xf, TEST_WAV_PATH, 0.0f, true));

    /* Start a crossfade */
    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH2, 4096));
    ASSERT_TRUE(forge_audio_crossfader_play(&xf, TEST_WAV_PATH2, 1.0f, true));
    ASSERT_TRUE(xf.fading);

    /* NaN dt should not advance fade_progress */
    float progress_before = xf.fade_progress;
    volatile float zero = 0.0f;
    float nan_val = zero / zero;
    forge_audio_crossfader_update(&xf, nan_val);
    ASSERT_NEAR(xf.fade_progress, progress_before, EPSILON);

    /* Negative dt should not advance fade_progress */
    forge_audio_crossfader_update(&xf, -1.0f);
    ASSERT_NEAR(xf.fade_progress, progress_before, EPSILON);

    forge_audio_crossfader_close(&xf);

    END_TEST();
}

/* Verify NaN/negative dt is rejected by layer_group_update. */
static void test_layer_group_nan_dt(void)
{
    TEST("layer_group_update rejects NaN/negative dt");

    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 4096));

    ForgeAudioLayerGroup group;
    forge_audio_layer_group_init(&group);
    group.playing = true;
    forge_audio_layer_group_add(&group, TEST_WAV_PATH, 1.0f);

    /* Start a weight fade */
    forge_audio_layer_group_fade_weight(&group, 0, 0.0f, 2.0f);
    float weight_before = group.layers[0].weight;

    /* NaN dt should not advance weight */
    volatile float zero = 0.0f;
    float nan_val = zero / zero;
    forge_audio_layer_group_update(&group, nan_val);
    ASSERT_NEAR(group.layers[0].weight, weight_before, EPSILON);

    /* Negative dt should not advance weight */
    forge_audio_layer_group_update(&group, -1.0f);
    ASSERT_NEAR(group.layers[0].weight, weight_before, EPSILON);

    forge_audio_layer_group_close(&group);

    END_TEST();
}

/* Verify that zero-weight layers stay phase-locked (ring buffer drained). */
static void test_layer_group_zero_weight_phase_lock(void)
{
    TEST("zero-weight layers stay phase-locked");

    /* Use a file larger than the ring buffer so progress is meaningful */
    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH, 44100));
    ASSERT_TRUE(write_test_wav(TEST_WAV_PATH2, 44100));

    ForgeAudioLayerGroup group;
    forge_audio_layer_group_init(&group);
    group.playing = true;

    /* Add two layers: leader at full weight, follower at zero */
    forge_audio_layer_group_add(&group, TEST_WAV_PATH, 1.0f);
    forge_audio_layer_group_add(&group, TEST_WAV_PATH2, 0.0f);

    /* Run many update+read cycles to advance both streams.
     * Each read drains 512 frames; need enough iterations to consume
     * a meaningful fraction of the 44100-frame file. */
    float out[512 * 2];
    int iter;
    for (iter = 0; iter < 40; iter++) {
        forge_audio_layer_group_update(&group, 1.0f / 60.0f);
        SDL_memset(out, 0, sizeof(out));
        forge_audio_layer_group_read(&group, out, 512);
    }

    /* Both streams should have advanced — the zero-weight stream should
     * NOT be stuck at the start because its ring buffer was drained. */
    float leader_prog = forge_audio_stream_progress(&group.layers[0].stream);
    float follower_prog = forge_audio_stream_progress(&group.layers[1].stream);
    ASSERT_TRUE(leader_prog > 0.1f);
    ASSERT_TRUE(follower_prog > 0.1f);

    /* They should be close in progress (sync maintained) */
    float diff = leader_prog - follower_prog;
    if (diff < 0.0f) diff = -diff;
    ASSERT_TRUE(diff < 0.15f);

    forge_audio_layer_group_close(&group);

    END_TEST();
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    SDL_Init(0);
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

    /* Streaming tests (Lesson 05) */
    test_stream_open_close();
    test_stream_read_all();
    test_stream_loop_wrap();
    test_stream_loop_with_intro();
    test_stream_seek();
    test_stream_progress();
    test_stream_invalid_path();
    test_stream_finished_non_looping();
    test_stream_loop_start_clamped();
    test_stream_looping_no_hang();

    /* Crossfader tests (Lesson 05) */
    test_crossfader_single_track();
    test_crossfader_blend_50();
    test_crossfader_fade_complete();

    /* Layer group tests (Lesson 05) */
    test_layer_group_single_layer();
    test_layer_group_weight_zero_silence();
    test_layer_group_weight_fade();
    test_layer_group_progress();
    test_layer_group_weight_clamped();
    test_stream_zero_rate_wav_rejected();
    test_stream_excessive_channels_rejected();
    test_stream_total_frames_nonnegative();

    /* Public API and safety tests (PR #329 round 5) */
    test_tanhf_public_api();
    test_tanhf_clamp_extremes();
    test_sync_threshold_floor_constant();
    test_crossfader_close_reinit();
    test_layer_group_nan_weight();
    test_crossfader_nan_dt();
    test_layer_group_nan_dt();
    test_layer_group_zero_weight_phase_lock();
    test_stream_64bit_float_rejected();

    /* Clean up temporary test WAV files */
    SDL_RemovePath(TEST_WAV_PATH);
    SDL_RemovePath(TEST_WAV_PATH2);

    SDL_Log("\n=== Results: %d/%d passed, %d failed ===",
            pass_count, test_count, fail_count);

    SDL_Quit();
    return fail_count > 0 ? 1 : 0;
}
