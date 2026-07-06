/* =============================================================================
 * FALLOUT GPU - demos.h
 * -----------------------------------------------------------------------------
 * Maps DEMO_STAGE (config.h) onto the spec section 6 test ladder:
 * stage 4 = single pixel, stage 5 = clear + gradient, stage 6 = spinning
 * cube, anything else = static test pattern.
 * ============================================================================= */

#ifndef DEMOS_H
#define DEMOS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Call once from setup(): initializes the GPU bus and announces the stage. */
void demo_setup(void);

/* Call forever from loop(): runs one iteration of the selected stage. */
void demo_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* DEMOS_H */
