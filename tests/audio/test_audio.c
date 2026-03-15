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
#include <limits.h>
#include <math.h>
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
        if (!isfinite(actual_) || !isfinite(expected_) || \
            fabs(actual_ - expected_) > eps_) { \
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
    memset(out, 0, sizeof(out));
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
    memset(out, 0, sizeof(out));
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
    memset(out, 0, sizeof(out));
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
    memset(out, 0, sizeof(out));
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
    memset(out, 0, sizeof(out));
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
    memset(out, 0, sizeof(out));
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
    memset(out, 0, sizeof(out));
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
    memset(out, 0, sizeof(out));
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
    memset(out, 0, sizeof(out));
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
    memset(out, 0, sizeof(out));
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
    memset(out, 0, sizeof(out));
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
        memset(out_a, 0, sizeof(out_a));
        forge_audio_source_mix(&src, out_a, 8);
    }

    /* Run 2 — identical parameters */
    {
        ForgeAudioSource src = forge_audio_source_create(&buf, 0.8f, false);
        src.pan = 0.5f;
        src.playing = true;
        memset(out_b, 0, sizeof(out_b));
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
    memset(out, 0, sizeof(out));
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
    memset(out, 0, sizeof(out));
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

    SDL_Log("\n=== Results: %d/%d passed, %d failed ===",
            pass_count, test_count, fail_count);

    return fail_count > 0 ? 1 : 0;
}
