/* =============================================================================
 * FALLOUT GPU - scene.c
 * Pure C99 demo scenes.  See scene.h.
 * ============================================================================= */

#include "scene.h"
#include "gpu_gl.h"
#include "gpu_math.h"

/* ---------------------------------------------------------------------------
 * Test pattern: 8 vertical color bars, white border, orange diagonal.
 * ------------------------------------------------------------------------- */
void scene_test_pattern(uint8_t *fb)
{
    static const uint8_t bars[8] = {
        RGB332(255, 255, 255),   /* white   */
        RGB332(255, 255,   0),   /* yellow  */
        RGB332(  0, 255, 255),   /* cyan    */
        RGB332(  0, 255,   0),   /* green   */
        RGB332(255,   0, 255),   /* magenta */
        RGB332(255,   0,   0),   /* red     */
        RGB332(  0,   0, 255),   /* blue    */
        RGB332( 64,  64,  64),   /* dark gray (not black, so bar 8 is visible
                                    against a dead/blank framebuffer) */
    };
    const int bar_w = GPU_W / 8;   /* 20 px per bar */
    int x, y;

    for (y = 0; y < GPU_H; y++) {
        for (x = 0; x < GPU_W; x++) {
            fb[y * GPU_W + x] = bars[x / bar_w];
        }
    }

    /* 1px white border - instantly shows if the scan clips or wraps. */
    {
        uint8_t w = RGB332(255, 255, 255);
        gl_line(fb, 0,         0,         GPU_W - 1, 0,         w);
        gl_line(fb, 0,         GPU_H - 1, GPU_W - 1, GPU_H - 1, w);
        gl_line(fb, 0,         0,         0,         GPU_H - 1, w);
        gl_line(fb, GPU_W - 1, 0,         GPU_W - 1, GPU_H - 1, w);
    }

    /* Diagonal - shows if the address mapping (y*160+x) is scrambled. */
    gl_line(fb, 0, 0, GPU_W - 1, GPU_H - 1, RGB332(255, 128, 0));
}

/* ---------------------------------------------------------------------------
 * Gradient: exercises every framebuffer byte with a position-dependent value.
 * ------------------------------------------------------------------------- */
void scene_gradient(uint8_t *fb)
{
    int x, y;
    for (y = 0; y < GPU_H; y++) {
        int g = (y * 255) / (GPU_H - 1);
        for (x = 0; x < GPU_W; x++) {
            int r = (x * 255) / (GPU_W - 1);
            int b = ((x + y) * 255) / (GPU_W + GPU_H - 2);
            fb[y * GPU_W + x] = (uint8_t)RGB332(r, g, b);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Spinning cube.
 *
 * Unit cube (half-extent CUBE_HALF) rotated about x and y at different
 * rates, pushed CUBE_DIST down +z, perspective-projected.  With focal 115
 * and distance 4 the cube reads as roughly 70% of the 120px screen height
 * and its worst-case corner still projects inside the frame.
 * ------------------------------------------------------------------------- */
#define CUBE_HALF 1.0f
#define CUBE_DIST 4.0f

static const float cube_v[8][3] = {
    { -1.0f, -1.0f, -1.0f },   /* 0 */
    {  1.0f, -1.0f, -1.0f },   /* 1 */
    {  1.0f,  1.0f, -1.0f },   /* 2 */
    { -1.0f,  1.0f, -1.0f },   /* 3 */
    { -1.0f, -1.0f,  1.0f },   /* 4 */
    {  1.0f, -1.0f,  1.0f },   /* 5 */
    {  1.0f,  1.0f,  1.0f },   /* 6 */
    { -1.0f,  1.0f,  1.0f },   /* 7 */
};

/* Quads ordered so that, projected through project() (which flips y), a face
 * whose outward normal points at the camera winds to NEGATIVE screen area.
 * Backface test below relies on exactly this convention. */
static const uint8_t cube_face[6][4] = {
    { 0, 1, 2, 3 },   /* front  (z = -1) */
    { 5, 4, 7, 6 },   /* back   (z = +1) */
    { 4, 0, 3, 7 },   /* left   (x = -1) */
    { 1, 5, 6, 2 },   /* right  (x = +1) */
    { 3, 2, 6, 7 },   /* top    (y = +1) */
    { 4, 5, 1, 0 },   /* bottom (y = -1) */
};

static const uint8_t cube_color[6] = {
    RGB332(255,   0,   0),   /* front  red     */
    RGB332(  0, 255,   0),   /* back   green   */
    RGB332(  0,   0, 255),   /* left   blue    */
    RGB332(255, 255,   0),   /* right  yellow  */
    RGB332(  0, 255, 255),   /* top    cyan    */
    RGB332(255,   0, 255),   /* bottom magenta */
};

void scene_cube_frame(uint8_t *fb, float t)
{
    mat4 rx, ry, rot, trans, model;
    int sx[8], sy[8];
    bool vis[8];
    int i, f;

    gl_clear(fb, RGB332(0, 0, 64));   /* dark navy backdrop */

    /* model = translate(0,0,CUBE_DIST) * rotY(0.7t) * rotX(t) */
    mat4_rotate_x(&rx, t);
    mat4_rotate_y(&ry, t * 0.7f);
    mat4_mul(&rot, &ry, &rx);
    mat4_translate(&trans, 0.0f, 0.0f, CUBE_DIST);
    mat4_mul(&model, &trans, &rot);

    for (i = 0; i < 8; i++) {
        vec3 p;
        p.x = cube_v[i][0] * CUBE_HALF;
        p.y = cube_v[i][1] * CUBE_HALF;
        p.z = cube_v[i][2] * CUBE_HALF;
        p = mat4_apply(&model, p);
        vis[i] = project(p, &sx[i], &sy[i]);
    }

    for (f = 0; f < 6; f++) {
        const uint8_t *q = cube_face[f];
        int32_t area2;

        if (!vis[q[0]] || !vis[q[1]] || !vis[q[2]] || !vis[q[3]]) {
            continue;   /* near-plane guarded vertex: skip the face */
        }

        /* Backface cull on signed projected area: camera-facing faces wind
         * negative (see cube_face comment).  This must happen HERE because
         * gl_tri normalizes winding and would happily fill backfaces too.
         * Culling alone is a correct visibility solution for a convex cube;
         * no depth sort required. */
        area2 = (int32_t)(sx[q[1]] - sx[q[0]]) * (sy[q[2]] - sy[q[0]])
              - (int32_t)(sy[q[1]] - sy[q[0]]) * (sx[q[2]] - sx[q[0]]);
        if (area2 >= 0) {
            continue;
        }

        /* Quad as two triangles sharing the q0-q2 diagonal; gl_tri's
         * top-left rule makes the seam pixel-exact. */
        gl_tri(fb, sx[q[0]], sy[q[0]], sx[q[1]], sy[q[1]],
                   sx[q[2]], sy[q[2]], cube_color[f]);
        gl_tri(fb, sx[q[0]], sy[q[0]], sx[q[2]], sy[q[2]],
                   sx[q[3]], sy[q[3]], cube_color[f]);
    }
}
