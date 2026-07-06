/* =============================================================================
 * FALLOUT GPU - gpu_bus.cpp
 * -----------------------------------------------------------------------------
 * ESP32-S3 -> 74HCT595 chain -> IS62C256 SRAM write bridge.
 *
 * Chain (spec section 2): ESP32 MOSI -> U8 DS; U8 Q7S -> U19 DS;
 * U19 Q7S -> U20 DS.  After 24 SPI clocks the FIRST byte sent has shifted
 * all the way into U20.  U8 = data D0..D7, U19 = addr low A0..A7,
 * U20 = addr high A8..A14 (Q0 = A8).  So the per-pixel send order is:
 *     byte 1 = (addr >> 8) & 0x7F   -> lands in U20 (addr high)
 *     byte 2 =  addr       & 0xFF   -> lands in U19 (addr low)
 *     byte 3 =  data                -> lands in U8  (data)
 * then RLCK rising edge latches all 24 bits onto the SRAM buses, then a
 * QSR1 pulse (with QSR0 already high) fires WE# through the U23 AND ->
 * HC04 interlock and the byte lands in SRAM.
 *
 * THE hardware rule this file exists to respect (spec sections 2 and 8):
 * SRAM OE# and WE# must NEVER be low at the same time.  BLANK gates OE#,
 * QSR0&QSR1 gate WE#; the claim/release ordering below is load-bearing.
 * ============================================================================= */

#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include "gpu_gl.h"
#include "gpu_bus.h"

uint8_t gpu_fb[GPU_FB_SIZE];

static SPISettings s_spi_cfg(GPU_SPI_HZ, MSBFIRST, SPI_MODE0);

/* ---------------------------------------------------------------------------
 * Low-level strobes
 * ------------------------------------------------------------------------- */

/* 595 STCP: rising edge copies the 24 shifted bits to the output latches
 * (i.e. onto the SRAM address/data buses, since NBLANK enables 595 OE#). */
static inline void pulse_rlck(void)
{
    digitalWrite(PIN_RLCK, 1);
    delayMicroseconds(RLCK_PULSE_US);
    digitalWrite(PIN_RLCK, 0);
}

/* Write strobe: QSR1 high while QSR0 is already high -> U23 AND output high
 * -> HC04 -> SRAM WE# LOW for QSR1_PULSE_US.
 * SAFE ONLY because bus_claim() already put BLANK in the write state, which
 * drives SRAM OE# HIGH - so OE# and WE# are never low together. */
static inline void pulse_write_strobe(void)
{
    digitalWrite(PIN_QSR1, 1);
    delayMicroseconds(QSR1_PULSE_US);
    digitalWrite(PIN_QSR1, 0);
}

/* ---------------------------------------------------------------------------
 * Bus arbitration (spec section 4 write sequence)
 * ------------------------------------------------------------------------- */

static void bus_claim(void)
{
    /* ORDER IS LOAD-BEARING: BLANK must reach the write level FIRST.
     * That disables the counter-side address buffer (U6), enables the
     * ESP32-side buffer (U22), releases the pixel latches, enables the 595
     * outputs (NBLANK), and - critically - drives SRAM OE# HIGH.  Only
     * after OE# is high may any WE# pulse (QSR1) ever fire. */
    digitalWrite(PIN_BLANK, BLANK_WRITE_LEVEL);

    /* Request the bus: QSR0 high arms the U23 write interlock (counters
     * back off; WE# still idle because QSR1 is low). */
    digitalWrite(PIN_QSR0, 1);

    delayMicroseconds(QSR_SETTLE_US);   /* let buffers/latches settle */

    SPI.beginTransaction(s_spi_cfg);    /* one transaction per claim */
}

static void bus_release(void)
{
    SPI.endTransaction();

    /* ORDER IS LOAD-BEARING: QSR0 (and QSR1, already idle-low between
     * bytes) must BOTH be low BEFORE BLANK returns to the scan level.
     * Otherwise WE# could still be (or glitch) low while BLANK re-enables
     * SRAM OE# - the forbidden OE#+WE# overlap. */
    digitalWrite(PIN_QSR1, 0);          /* belt-and-braces; idle-low already */
    digitalWrite(PIN_QSR0, 0);
    delayMicroseconds(BUS_RELEASE_US);

    /* Now it is safe to hand the buses back to the counters (SRAM OE# may
     * go low again only after this point). */
    digitalWrite(PIN_BLANK, BLANK_SCAN_LEVEL);
}

/* Shift one address+data triplet into the 595 chain and strobe it into
 * SRAM.  Caller must hold the bus (bus_claim). */
static inline void write_sram_byte(uint16_t addr, uint8_t data)
{
    SPI.transfer((uint8_t)((addr >> 8) & 0x7F));   /* -> U20 addr high (A15 unused) */
    SPI.transfer((uint8_t)(addr & 0xFF));          /* -> U19 addr low  */
    SPI.transfer(data);                            /* -> U8  data      */
    pulse_rlck();           /* latch 24 bits onto the SRAM buses */
    pulse_write_strobe();   /* WE# pulse: byte lands in SRAM     */
}

/* Stream framebuffer rows [y0, y0+rows) to SRAM.  Caller holds the bus. */
static void stream_rows(int y0, int rows)
{
    uint16_t addr = (uint16_t)(y0 * GPU_W);
    const uint8_t *p = &gpu_fb[addr];
    long n = (long)rows * GPU_W;
    while (n-- > 0) {
        write_sram_byte(addr++, *p++);
    }
}

/* ---------------------------------------------------------------------------
 * Public API (spec section 4)
 * ------------------------------------------------------------------------- */

void gpu_scan_reset(void)
{
    /* Assert both counter resets together, release together: H and V
     * restart in lockstep at the top-left of the frame. */
    digitalWrite(PIN_HRST, RST_ACTIVE_LEVEL);
    digitalWrite(PIN_VRST, RST_ACTIVE_LEVEL);
    delayMicroseconds(RST_PULSE_US);
    digitalWrite(PIN_HRST, RST_IDLE_LEVEL);
    digitalWrite(PIN_VRST, RST_IDLE_LEVEL);
}

void gpu_init(void)
{
    /* Preload safe idle levels into the GPIO output latches BEFORE enabling
     * the drivers, so the pins never glitch through an active level (a
     * stray QSR pulse during boot could fire WE# mid-scan). */
    digitalWrite(PIN_QSR0, 0);                    /* no bus request        */
    digitalWrite(PIN_QSR1, 0);                    /* no write strobe       */
    digitalWrite(PIN_RLCK, 0);                    /* latch idle            */
    digitalWrite(PIN_BLANK, BLANK_SCAN_LEVEL);    /* counters own the bus  */
    digitalWrite(PIN_HRST, RST_IDLE_LEVEL);
    digitalWrite(PIN_VRST, RST_IDLE_LEVEL);

    pinMode(PIN_QSR0, OUTPUT);
    pinMode(PIN_QSR1, OUTPUT);
    pinMode(PIN_RLCK, OUTPUT);
    pinMode(PIN_BLANK, OUTPUT);
    pinMode(PIN_HRST, OUTPUT);
    pinMode(PIN_VRST, OUTPUT);

    /* FSPI with custom pins; no MISO (there is no read-back path - spec
     * section 8), no hardware CS. */
    SPI.begin(PIN_SRCLK, -1, PIN_SER, -1);

    gpu_scan_reset();          /* known scan phase */

    /* Spec section 4: gpu_init clears SRAM.  Clear the local framebuffer
     * and push it so the monitor shows black instead of power-on noise. */
    gpu_clear(0x00);
    gpu_present();
}

void gpu_clear(uint8_t color)
{
    gl_clear(gpu_fb, color);
}

void gpu_pixel(int x, int y, uint8_t color)
{
    gl_pixel(gpu_fb, x, y, color);
}

void gpu_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    gl_line(gpu_fb, x0, y0, x1, y1, color);
}

void gpu_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color)
{
    gl_tri(gpu_fb, x0, y0, x1, y1, x2, y2, color);
}

void gpu_present(void)
{
#if PRESENT_CHUNK_LINES > 0
    /* Chunked: release the bus between chunks so the scanout keeps
     * refreshing the monitor while a long present is in flight. */
    for (int y = 0; y < GPU_H; y += PRESENT_CHUNK_LINES) {
        int rows = PRESENT_CHUNK_LINES;
        if (y + rows > GPU_H) {
            rows = GPU_H - y;
        }
        bus_claim();
        stream_rows(y, rows);
        bus_release();
    }
#else
    /* Whole frame under one bus claim (fastest). */
    bus_claim();
    stream_rows(0, GPU_H);
    bus_release();
#endif

#if RESET_SCAN_AFTER_PRESENT
    gpu_scan_reset();
#endif
}

void gpu_present_rect(int x, int y, int w, int h)
{
    /* Clip the rect to the framebuffer. */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > GPU_W) { w = GPU_W - x; }
    if (y + h > GPU_H) { h = GPU_H - y; }
    if (w <= 0 || h <= 0) {
        return;
    }

    bus_claim();
    for (int row = y; row < y + h; row++) {
        uint16_t addr = (uint16_t)(row * GPU_W + x);
        const uint8_t *p = &gpu_fb[addr];
        for (int col = 0; col < w; col++) {
            write_sram_byte(addr++, *p++);
        }
    }
    bus_release();
}
