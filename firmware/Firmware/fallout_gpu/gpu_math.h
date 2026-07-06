/* =============================================================================
 * FALLOUT GPU - gpu_math.h
 * -----------------------------------------------------------------------------
 * Pure C99 minimal 3D math: exactly what the spinning cube needs and nothing
 * more.  vec3 + column-major mat4 + perspective projection to 160x120.
 * ============================================================================= */

#ifndef GPU_MATH_H
#define GPU_MATH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x, y, z;
} vec3;

/* Column-major 4x4: element(row, col) = m[col * 4 + row].
 * Matches OpenGL convention; translation lives in m[12..14]. */
typedef struct {
    float m[16];
} mat4;

/* Perspective projection constants (kept here so scenes can reason about
 * them).  Camera at the origin looking down +z, x right, y up in 3D;
 * screen y grows downward, hence the flip inside project(). */
#define PROJ_CX    80.0f    /* screen center x (GPU_W / 2)  */
#define PROJ_CY    60.0f    /* screen center y (GPU_H / 2)  */
#define PROJ_FOCAL 115.0f   /* pixels per unit of x/z, y/z  */
#define PROJ_NEAR  0.25f    /* near-plane guard             */

void mat4_identity(mat4 *out);

/* out = a * b (apply b first, then a).  Aliasing-safe: out may be a or b. */
void mat4_mul(mat4 *out, const mat4 *a, const mat4 *b);

/* Each sets *out to the full transform (not a multiply-accumulate). */
void mat4_rotate_x(mat4 *out, float rad);
void mat4_rotate_y(mat4 *out, float rad);
void mat4_rotate_z(mat4 *out, float rad);
void mat4_translate(mat4 *out, float tx, float ty, float tz);

/* Transform a point (w = 1). */
vec3 mat4_apply(const mat4 *m, vec3 v);

/* Perspective-project camera-space point p to integer screen coordinates.
 * Returns false (outputs untouched) if p.z < PROJ_NEAR: the caller must
 * skip geometry that pierces the near plane. */
bool project(vec3 p, int *sx, int *sy);

#ifdef __cplusplus
}
#endif

#endif /* GPU_MATH_H */
