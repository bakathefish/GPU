/* =============================================================================
 * FALLOUT GPU - gpu_bus.h
 * -----------------------------------------------------------------------------
 * Arduino-side driver: SPI -> 3x 74HCT595 write bridge -> IS62C256 SRAM,
 * plus QSR0/QSR1/BLANK/hRST/vRST bus arbitration.
 *
 * This layer owns ONE global framebuffer (gpu_fb) and exposes the spec
 * section 4 API as thin wrappers over the pure-C gl_* raster core plus the
 * bus.  Draw into gpu_fb (directly or via gpu_* calls), then gpu_present()
 * to push it into the scanout SRAM.
 * ============================================================================= */

#ifndef GPU_BUS_H
#define GPU_BUS_H

#include <stdint.h>
#include "gpu_gl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The one global framebuffer (160*120 = 19200 bytes, internal RAM). */
extern uint8_t gpu_fb[GPU_FB_SIZE];

/* Pins to safe idle, SPI up, scan counters reset, SRAM cleared to 0. */
void gpu_init(void);

/* Spec section 4 drawing API - these touch gpu_fb only (no bus traffic
 * until gpu_present / gpu_present_rect). */
void gpu_clear(uint8_t color);
void gpu_pixel(int x, int y, uint8_t color);
void gpu_line(int x0, int y0, int x1, int y1, uint8_t color);
void gpu_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color);

/* Push the whole framebuffer to SRAM.  Honors PRESENT_CHUNK_LINES (release
 * the bus every N lines so the display keeps scanning) and
 * RESET_SCAN_AFTER_PRESENT (restart the scan at frame top when done). */
void gpu_present(void);

/* Push only the rect [x, x+w) x [y, y+h) (clipped).  Single bus claim;
 * ideal for stage 4's one-pixel write and small dirty regions. */
void gpu_present_rect(int x, int y, int w, int h);

/* Pulse hRST + vRST together: scan restarts at the top-left of the frame. */
void gpu_scan_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* GPU_BUS_H */
