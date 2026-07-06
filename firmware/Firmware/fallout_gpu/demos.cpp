/* =============================================================================
 * FALLOUT GPU - demos.cpp
 * Test-ladder stage runner.  Stage selection is compile-time (DEMO_STAGE in
 * config.h) so each rung flashes as the smallest possible firmware.
 * ============================================================================= */

#include <Arduino.h>
#include "config.h"
#include "gpu_gl.h"
#include "gpu_bus.h"
#include "scene.h"
#include "demos.h"

void demo_setup(void)
{
    gpu_init();   /* safe pin idle, SPI up, scan reset, SRAM cleared to 0 */

#if DEMO_STAGE == 4
    Serial.println(F("[fallout] stage 4: one white pixel at (80,60), rewritten every second"));
#elif DEMO_STAGE == 5
    Serial.println(F("[fallout] stage 5: solid fill then gradient"));
#elif DEMO_STAGE == 6
    Serial.println(F("[fallout] stage 6: spinning cube"));
#else
    Serial.println(F("[fallout] fallback: static test pattern (stages 1-3 are hardware-only)"));
#endif
}

void demo_loop(void)
{
#if DEMO_STAGE == 4
    /* Ladder stage 4 - "ESP32 writes one pixel".
     * Done when: that pixel lights at the correct coordinates.  Validates
     * the write bridge plus QSR0/QSR1/BLANK arbitration.  Re-written every
     * second so a scope/analyzer can trigger on the write burst. */
    gpu_pixel(80, 60, 0xFF);            /* 0xFF = RGB332 white */
    gpu_present_rect(80, 60, 1, 1);     /* exactly one 24-bit shift + WE# pulse */
    delay(STAGE4_REWRITE_MS);

#elif DEMO_STAGE == 5
    /* Ladder stage 5 - "Clear and gradient".
     * Done when: solid color first, then a gradient renders. */
    gpu_clear(RGB332(0, 0, 255));       /* solid blue */
    gpu_present();
    delay(STAGE5_HOLD_MS);

    scene_gradient(gpu_fb);
    gpu_present();
    delay(STAGE5_HOLD_MS);

#elif DEMO_STAGE == 6
    /* Ladder stage 6 - "Primitives to cube".
     * Done when: the cube spins.  Headline demo. */
    static uint32_t frames = 0;
    static uint32_t t0 = 0;

    scene_cube_frame(gpu_fb, (float)millis() * 0.001f);
    gpu_present();

    if (++frames % 32 == 0) {           /* lightweight FPS telemetry */
        uint32_t now = millis();
        if (t0 != 0) {
            Serial.print(F("[fallout] ms/frame ~ "));
            Serial.println((now - t0) / 32);
        }
        t0 = now;
    }

#else
    /* Fallback / simulator reference: color bars + border + diagonal. */
    scene_test_pattern(gpu_fb);
    gpu_present();
    delay(1000);
#endif
}
