/* =============================================================================
 * FALLOUT GPU - fallout_gpu.ino
 * -----------------------------------------------------------------------------
 * ESP32-S3 firmware entry point for the discrete-logic GPU.
 *
 * The 74-series scanout half runs with zero software; this firmware is the
 * render half: it rasterizes into a local 160x120 RGB332 framebuffer and
 * pushes it into the scanout SRAM through the 3x 74HCT595 write bridge.
 *
 * Pick the test-ladder stage with DEMO_STAGE in config.h, then build/flash
 * per README.md.
 * ============================================================================= */

#include "config.h"
#include "demos.h"

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(200);   /* give USB-CDC a beat so the banner is not lost */

    Serial.println();
    Serial.println(F("[fallout] FALLOUT GPU firmware"));
    Serial.print(F("[fallout] DEMO_STAGE = "));
    Serial.println(DEMO_STAGE);

    demo_setup();
}

void loop()
{
    demo_loop();
}
