/* =============================================================================
 * FALLOUT GPU - gpu_math.c
 * Pure C99.  See gpu_math.h for the contract.
 * ============================================================================= */

#include "gpu_math.h"
#include <math.h>

void mat4_identity(mat4 *out)
{
    int i;
    for (i = 0; i < 16; i++) {
        out->m[i] = 0.0f;
    }
    out->m[0] = out->m[5] = out->m[10] = out->m[15] = 1.0f;
}

void mat4_mul(mat4 *out, const mat4 *a, const mat4 *b)
{
    mat4 r;  /* local temp makes out == a or out == b safe */
    int col, row, k;
    for (col = 0; col < 4; col++) {
        for (row = 0; row < 4; row++) {
            float s = 0.0f;
            for (k = 0; k < 4; k++) {
                s += a->m[k * 4 + row] * b->m[col * 4 + k];
            }
            r.m[col * 4 + row] = s;
        }
    }
    *out = r;
}

void mat4_rotate_x(mat4 *out, float rad)
{
    float c = cosf(rad), s = sinf(rad);
    mat4_identity(out);
    out->m[5]  =  c;   /* row1,col1 */
    out->m[9]  = -s;   /* row1,col2 */
    out->m[6]  =  s;   /* row2,col1 */
    out->m[10] =  c;   /* row2,col2 */
}

void mat4_rotate_y(mat4 *out, float rad)
{
    float c = cosf(rad), s = sinf(rad);
    mat4_identity(out);
    out->m[0]  =  c;   /* row0,col0 */
    out->m[8]  =  s;   /* row0,col2 */
    out->m[2]  = -s;   /* row2,col0 */
    out->m[10] =  c;   /* row2,col2 */
}

void mat4_rotate_z(mat4 *out, float rad)
{
    float c = cosf(rad), s = sinf(rad);
    mat4_identity(out);
    out->m[0] =  c;    /* row0,col0 */
    out->m[4] = -s;    /* row0,col1 */
    out->m[1] =  s;    /* row1,col0 */
    out->m[5] =  c;    /* row1,col1 */
}

void mat4_translate(mat4 *out, float tx, float ty, float tz)
{
    mat4_identity(out);
    out->m[12] = tx;
    out->m[13] = ty;
    out->m[14] = tz;
}

vec3 mat4_apply(const mat4 *m, vec3 v)
{
    vec3 r;
    r.x = m->m[0] * v.x + m->m[4] * v.y + m->m[8]  * v.z + m->m[12];
    r.y = m->m[1] * v.x + m->m[5] * v.y + m->m[9]  * v.z + m->m[13];
    r.z = m->m[2] * v.x + m->m[6] * v.y + m->m[10] * v.z + m->m[14];
    return r;
}

bool project(vec3 p, int *sx, int *sy)
{
    float inv;
    if (p.z < PROJ_NEAR) {
        return false;   /* behind / piercing the near plane: caller skips it */
    }
    inv = PROJ_FOCAL / p.z;
    /* +0.5f is cheap round-to-nearest for the on-screen range; gl_* clips
     * anything that lands outside anyway. */
    *sx = (int)(PROJ_CX + p.x * inv + 0.5f);
    *sy = (int)(PROJ_CY - p.y * inv + 0.5f);   /* 3D y-up -> screen y-down */
    return true;
}
