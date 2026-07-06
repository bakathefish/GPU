/* =============================================================================
 * FALLOUT GPU - scene.h
 * -----------------------------------------------------------------------------
 * Pure C99 demo scenes.  Each renders into a caller-supplied 160x120 RGB332
 * framebuffer; no hardware access, so they run identically on host and ESP32.
 * ============================================================================= */

#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Color bars + 1px white border + orange diagonal.  Simulator reference
 * image and fallback demo. */
void scene_test_pattern(uint8_t *fb);

/* Full-screen gradient: red ramps with x, green with y, blue with x+y.
 * Test-ladder stage 5. */
void scene_gradient(uint8_t *fb);

/* One frame of the spinning cube at animation time t (seconds).  12 filled
 * triangles, 6 distinct RGB332 face colors, backface culling via signed
 * projected area (sufficient for a convex cube - no painter sort needed).
 * Test-ladder stage 6. */
void scene_cube_frame(uint8_t *fb, float t);

#ifdef __cplusplus
}
#endif

#endif /* SCENE_H */
