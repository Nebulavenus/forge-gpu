/*
 * forge_gltf_anim.h — Runtime animation evaluation for glTF data
 *
 * Evaluates animation channels parsed by forge_gltf.h at a given time.
 * Handles binary search over keyframes, linear interpolation (lerp for
 * vec3, slerp for quaternions), step interpolation, looping, and
 * clamping.
 *
 * After calling forge_gltf_anim_apply(), the caller must call
 * forge_gltf_compute_world_transforms() to propagate the updated
 * TRS values through the node hierarchy.
 *
 * Dependencies:
 *   - forge_gltf.h  (for ForgeGltfAnimation, ForgeGltfAnimSampler, etc.)
 *   - forge_math.h  (for vec3, quat, mat4, slerp, lerp)
 *
 * Usage:
 *   #include "gltf/forge_gltf.h"
 *   #include "gltf/forge_gltf_anim.h"
 *
 *   float t = fmodf(elapsed, scene.animations[0].duration);
 *   forge_gltf_anim_apply(&scene.animations[0],
 *                          scene.nodes, scene.node_count, t, true);
 *   mat4 identity = mat4_identity();
 *   for (int i = 0; i < scene.root_node_count; i++)
 *       forge_gltf_compute_world_transforms(&scene, scene.root_nodes[i],
 *                                            &identity);
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_GLTF_ANIM_H
#define FORGE_GLTF_ANIM_H

#include "gltf/forge_gltf.h"

/* Small epsilon for timestamp comparisons. */
#define FORGE_GLTF_ANIM_EPSILON 1e-7f

/* ── API ──────────────────────────────────────────────────────────────────── */

/* Find the keyframe interval containing time t.
 * Returns index lo such that timestamps[lo] <= t < timestamps[lo+1].
 * Uses binary search — O(log n) per evaluation.
 * Requires count >= 2.  Callers must clamp for count <= 1. */
static int forge_gltf_anim_find_keyframe(
    const float *timestamps, int count, float t);

/* Evaluate a vec3 sampler (translation or scale) at time t.
 * Returns the interpolated value.  Clamps at boundaries. */
static vec3 forge_gltf_anim_eval_vec3(
    const ForgeGltfAnimSampler *sampler, float t);

/* Evaluate a quaternion sampler (rotation) at time t.
 * Returns a normalized interpolated quaternion.  Handles the glTF
 * [x,y,z,w] to forge_math quat(w,x,y,z) conversion.
 * Clamps at boundaries. */
static quat forge_gltf_anim_eval_quat(
    const ForgeGltfAnimSampler *sampler, float t);

/* Apply all channels of an animation to scene nodes at time t.
 * If loop is true, wraps t to [0, duration).  Otherwise clamps.
 * Writes to node->translation, rotation, scale_xyz and recomputes
 * node->local_transform from the updated TRS.
 * Caller must call forge_gltf_compute_world_transforms() afterward. */
static void forge_gltf_anim_apply(
    const ForgeGltfAnimation *anim,
    ForgeGltfNode *nodes, int node_count,
    float t, bool loop);

/* ══════════════════════════════════════════════════════════════════════════
 * Implementation (header-only — all functions are static)
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Binary search for keyframe interval ─────────────────────────────────── */

static int forge_gltf_anim_find_keyframe(
    const float *timestamps, int count, float t)
{
    int lo = 0;
    int hi = count - 1;
    while (lo + 1 < hi) {
        int mid = (lo + hi) / 2;
        if (timestamps[mid] <= t) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

/* ── Evaluate vec3 (translation or scale) ────────────────────────────────── */

static vec3 forge_gltf_anim_eval_vec3(
    const ForgeGltfAnimSampler *sampler, float t)
{
    const float *ts = sampler->timestamps;
    const float *vals = sampler->values;
    int count = sampler->keyframe_count;

    /* Clamp before first keyframe. */
    if (count <= 0) return vec3_create(0.0f, 0.0f, 0.0f);
    if (t <= ts[0]) {
        return vec3_create(vals[0], vals[1], vals[2]);
    }
    /* Clamp after last keyframe. */
    if (t >= ts[count - 1]) {
        const float *v = vals + (count - 1) * 3;
        return vec3_create(v[0], v[1], v[2]);
    }

    int lo = forge_gltf_anim_find_keyframe(ts, count, t);

    /* STEP interpolation: hold previous value. */
    if (sampler->interpolation == FORGE_GLTF_INTERP_STEP) {
        const float *v = vals + lo * 3;
        return vec3_create(v[0], v[1], v[2]);
    }

    /* LINEAR interpolation. */
    float t0 = ts[lo];
    float t1 = ts[lo + 1];
    float span = t1 - t0;
    float alpha = (span > FORGE_GLTF_ANIM_EPSILON)
                ? (t - t0) / span : 0.0f;

    const float *a = vals + lo * 3;
    const float *b = vals + (lo + 1) * 3;

    return vec3_lerp(vec3_create(a[0], a[1], a[2]),
                     vec3_create(b[0], b[1], b[2]), alpha);
}

/* ── Evaluate quaternion (rotation) ──────────────────────────────────────── */

static quat forge_gltf_anim_eval_quat(
    const ForgeGltfAnimSampler *sampler, float t)
{
    const float *ts = sampler->timestamps;
    const float *vals = sampler->values;
    int count = sampler->keyframe_count;

    /* Clamp before first keyframe.
     * glTF quaternion order: [x, y, z, w] → quat_create(w, x, y, z). */
    if (count <= 0) return quat_create(1.0f, 0.0f, 0.0f, 0.0f);
    if (t <= ts[0]) {
        return quat_create(vals[3], vals[0], vals[1], vals[2]);
    }
    /* Clamp after last keyframe. */
    if (t >= ts[count - 1]) {
        const float *v = vals + (count - 1) * 4;
        return quat_create(v[3], v[0], v[1], v[2]);
    }

    int lo = forge_gltf_anim_find_keyframe(ts, count, t);

    const float *a = vals + lo * 4;
    quat qa = quat_create(a[3], a[0], a[1], a[2]);

    /* STEP interpolation: hold previous value. */
    if (sampler->interpolation == FORGE_GLTF_INTERP_STEP) {
        return qa;
    }

    /* LINEAR interpolation (slerp for quaternions). */
    float t0 = ts[lo];
    float t1 = ts[lo + 1];
    float span = t1 - t0;
    float alpha = (span > FORGE_GLTF_ANIM_EPSILON)
                ? (t - t0) / span : 0.0f;

    const float *b = vals + (lo + 1) * 4;
    quat qb = quat_create(b[3], b[0], b[1], b[2]);

    return quat_slerp(qa, qb, alpha);
}

/* ── Rebuild local_transform from TRS ────────────────────────────────────── */
/* Matches the TRS composition in forge_gltf__parse_nodes:
 * local_transform = T × R × S */

static void forge_gltf_anim__rebuild_local(ForgeGltfNode *node)
{
    mat4 t = mat4_translate(node->translation);
    mat4 r = quat_to_mat4(node->rotation);
    mat4 s = mat4_scale(node->scale_xyz);

    node->local_transform = mat4_multiply(t, mat4_multiply(r, s));
}

/* ── Apply animation to nodes ────────────────────────────────────────────── */

static void forge_gltf_anim_apply(
    const ForgeGltfAnimation *anim,
    ForgeGltfNode *nodes, int node_count,
    float t, bool loop)
{
    if (!anim || !nodes || node_count <= 0
        || anim->channel_count <= 0) return;

    /* Cap node_count to the modified[] array size to prevent stack OOB. */
    if (node_count > FORGE_GLTF_MAX_NODES) node_count = FORGE_GLTF_MAX_NODES;

    /* Wrap or clamp time. */
    if (anim->duration > FORGE_GLTF_ANIM_EPSILON) {
        if (loop) {
            t = SDL_fmodf(t, anim->duration);
            if (t < 0.0f) t += anim->duration;
        } else {
            if (t < 0.0f) t = 0.0f;
            if (t > anim->duration) t = anim->duration;
        }
    } else {
        t = 0.0f;
    }

    /* Track which nodes were modified so we only rebuild their transforms. */
    bool modified[FORGE_GLTF_MAX_NODES];
    SDL_memset(modified, 0, sizeof(modified));

    for (int ci = 0; ci < anim->channel_count; ci++) {
        const ForgeGltfAnimChannel *ch = &anim->channels[ci];

        if (ch->target_node < 0 || ch->target_node >= node_count) continue;
        if (ch->sampler_index < 0
            || ch->sampler_index >= anim->sampler_count) continue;

        const ForgeGltfAnimSampler *samp = &anim->samplers[ch->sampler_index];
        if (!samp->timestamps || !samp->values
            || samp->keyframe_count <= 0) continue;

        ForgeGltfNode *node = &nodes[ch->target_node];

        switch (ch->target_path) {
        case FORGE_GLTF_ANIM_TRANSLATION:
            node->translation = forge_gltf_anim_eval_vec3(samp, t);
            break;
        case FORGE_GLTF_ANIM_ROTATION:
            node->rotation = forge_gltf_anim_eval_quat(samp, t);
            break;
        case FORGE_GLTF_ANIM_SCALE:
            node->scale_xyz = forge_gltf_anim_eval_vec3(samp, t);
            break;
        }

        modified[ch->target_node] = true;
    }

    /* Rebuild local_transform for every modified node. */
    for (int i = 0; i < node_count; i++) {
        if (modified[i]) {
            forge_gltf_anim__rebuild_local(&nodes[i]);
        }
    }
}

#endif /* FORGE_GLTF_ANIM_H */
