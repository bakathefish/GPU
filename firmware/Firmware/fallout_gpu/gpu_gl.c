/* =============================================================================
 * FALLOUT GPU - gpu_gl.c
 * Pure C99 raster core.  See gpu_gl.h for the contract.
 * ============================================================================= */

#include "gpu_gl.h"

void gl_clear(uint8_t *fb, uint8_t c)
{
    size_t i;
    for (i = 0; i < (size_t)GPU_FB_SIZE; i++) {
        fb[i] = c;
    }
}

void gl_pixel(uint8_t *fb, int x, int y, uint8_t c)
{
    /* Unsigned compare rejects negatives and overruns in one test each. */
    if ((unsigned)x >= (unsigned)GPU_W || (unsigned)y >= (unsigned)GPU_H) {
        return;
    }
    fb[(size_t)y * GPU_W + (size_t)x] = c;
}

/* Bresenham (Zingl formulation), handles all octants, guaranteed to
 * terminate.  Off-screen pixels are clipped by gl_pixel - at 160x120 the
 * per-pixel test is cheaper than a Cohen-Sutherland pre-clip. */
void gl_line(uint8_t *fb, int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0);   /* dy <= 0 by construction */
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        int e2;
        gl_pixel(fb, x0, y0, c);
        e2 = 2 * err;
        if (e2 >= dy) {                 /* step x */
            if (x0 == x1) break;
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {                 /* step y */
            if (y0 == y1) break;
            err += dx;
            y0 += sy;
        }
    }
}

/* Signed twice-area / edge function.
 * In y-down screen coordinates a POSITIVE result for (v0,v1,v2) means the
 * vertices wind clockwise as seen on the monitor. */
static int32_t edge_fn(int ax, int ay, int bx, int by, int px, int py)
{
    return (int32_t)(bx - ax) * (int32_t)(py - ay)
         - (int32_t)(by - ay) * (int32_t)(px - ax);
}

/* Top-left fill rule classification for edge a->b of a POSITIVE-area
 * (screen-clockwise, y-down) triangle:
 *   top edge  = exactly horizontal, walked left-to-right (dy == 0, dx > 0)
 *   left edge = walked upward on screen (dy < 0)
 * Pixels exactly on top/left edges belong to this triangle; pixels on
 * bottom/right edges belong to the neighbor.  A shared edge is traversed in
 * opposite directions by the two adjacent triangles, so exactly one of them
 * claims the boundary pixels: no gaps, no double-draw. */
static bool edge_is_top_left(int ax, int ay, int bx, int by)
{
    if (ay == by) {
        return bx > ax;     /* top edge */
    }
    return by < ay;         /* left edge */
}

void gl_tri(uint8_t *fb, int x0, int y0, int x1, int y1,
            int x2, int y2, uint8_t c)
{
    int32_t area2 = edge_fn(x0, y0, x1, y1, x2, y2);
    int minx, miny, maxx, maxy, x, y;
    int32_t a0, b0, a1, b1, a2, b2;
    int32_t w0row, w1row, w2row;

    if (area2 == 0) {
        return;                         /* degenerate: draw nothing */
    }
    if (area2 < 0) {
        /* Normalize winding to positive area so one inside test works for
         * either input winding (backface culling is the caller's job). */
        int t;
        t = x1; x1 = x2; x2 = t;
        t = y1; y1 = y2; y2 = t;
    }

    /* Bounding box, clipped to the framebuffer. */
    minx = x0; if (x1 < minx) minx = x1; if (x2 < minx) minx = x2;
    miny = y0; if (y1 < miny) miny = y1; if (y2 < miny) miny = y2;
    maxx = x0; if (x1 > maxx) maxx = x1; if (x2 > maxx) maxx = x2;
    maxy = y0; if (y1 > maxy) maxy = y1; if (y2 > maxy) maxy = y2;
    if (minx < 0)      minx = 0;
    if (miny < 0)      miny = 0;
    if (maxx > GPU_W - 1) maxx = GPU_W - 1;
    if (maxy > GPU_H - 1) maxy = GPU_H - 1;
    if (minx > maxx || miny > maxy) {
        return;                         /* fully off-screen */
    }

    /* Edge functions evaluated incrementally: w(p+dx) = w + A, w(p+dy) = w + B.
     * w0 guards edge v1->v2, w1 guards v2->v0, w2 guards v0->v1. */
    a0 = y1 - y2;  b0 = x2 - x1;
    a1 = y2 - y0;  b1 = x0 - x2;
    a2 = y0 - y1;  b2 = x1 - x0;

    /* Fill-rule bias: edges that are NOT top-left get -1, turning their
     * ">= 0" inside test into "> 0" (exclusive). */
    w0row = edge_fn(x1, y1, x2, y2, minx, miny)
          + (edge_is_top_left(x1, y1, x2, y2) ? 0 : -1);
    w1row = edge_fn(x2, y2, x0, y0, minx, miny)
          + (edge_is_top_left(x2, y2, x0, y0) ? 0 : -1);
    w2row = edge_fn(x0, y0, x1, y1, minx, miny)
          + (edge_is_top_left(x0, y0, x1, y1) ? 0 : -1);

    for (y = miny; y <= maxy; y++) {
        int32_t w0 = w0row, w1 = w1row, w2 = w2row;
        uint8_t *row = fb + (size_t)y * GPU_W;
        for (x = minx; x <= maxx; x++) {
            /* All three non-negative <=> sign bits all clear. */
            if ((w0 | w1 | w2) >= 0) {
                row[x] = c;
            }
            w0 += a0; w1 += a1; w2 += a2;
        }
        w0row += b0; w1row += b1; w2row += b2;
    }
}
