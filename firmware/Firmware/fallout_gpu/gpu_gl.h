/* =============================================================================
 * FALLOUT GPU - gpu_gl.h
 * -----------------------------------------------------------------------------
 * Pure C99 software raster core.  No Arduino, no OS - operates on a caller-
 * supplied framebuffer so it compiles and unit-tests on any host with
 *   gcc -std=c99 -fsyntax-only gpu_gl.c
 *
 * Framebuffer: 160 x 120, 1 byte/pixel, RGB 3-3-2, linear addressing
 *   addr = y * 160 + x   (19200 bytes, fits SRAM A0..A14)
 * ============================================================================= */

#ifndef GPU_GL_H
#define GPU_GL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPU_W 160
#define GPU_H 120
#define GPU_FB_SIZE (160 * 120)

/* Color packing exactly per spec section 4:
 * 3 bits red (b7..b5), 3 bits green (b4..b2), 2 bits blue (b1..b0). */
#define RGB332(r, g, b) (((r) & 0xE0) | (((g) & 0xE0) >> 3) | (((b) & 0xC0) >> 6))

/* Fill the whole framebuffer with color c. */
void gl_clear(uint8_t *fb, uint8_t c);

/* Set one pixel; silently clips anything outside 0..GPU_W-1 / 0..GPU_H-1. */
void gl_pixel(uint8_t *fb, int x, int y, uint8_t c);

/* Bresenham line, all octants, endpoints inclusive, clipped per-pixel. */
void gl_line(uint8_t *fb, int x0, int y0, int x1, int y1, uint8_t c);

/* Filled triangle via integer edge functions with a top-left fill rule:
 * two triangles sharing an edge rasterize with no gap and no double-drawn
 * pixel.  Winding-insensitive (normalizes internally); degenerate (zero
 * area) triangles draw nothing.  Clipped to the framebuffer. */
void gl_tri(uint8_t *fb, int x0, int y0, int x1, int y1,
            int x2, int y2, uint8_t c);

#ifdef __cplusplus
}
#endif

#endif /* GPU_GL_H */
