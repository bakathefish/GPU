/* =============================================================================
 * FALLOUT GPU - config.h
 * -----------------------------------------------------------------------------
 * ALL pins, polarities and timing knobs for the ESP32-S3 side of the discrete
 * GPU live in this one file.  Nothing hardware-tunable is hidden anywhere else.
 *
 * This header is Arduino-side only (included by the .ino / .cpp files).  The
 * pure C raster core (gpu_gl.c / gpu_math.c / scene.c) never includes it.
 * ============================================================================= */

#ifndef FALLOUT_CONFIG_H
#define FALLOUT_CONFIG_H

/* -----------------------------------------------------------------------------
 * DEMO_STAGE - which rung of the spec section 6 test ladder to run.
 *   4 = single white pixel at (80,60), rewritten once a second
 *       (proves write bridge + QSR0/QSR1/BLANK arbitration)
 *   5 = solid color fill for 2 s, then gradient (proves full-frame writes)
 *   6 = spinning cube (headline demo)
 *   anything else = static test pattern (color bars + border + diagonal)
 * Stages 1-3 of the ladder are hardware-only; no firmware involved.
 * --------------------------------------------------------------------------- */
#define DEMO_STAGE 6

/* -----------------------------------------------------------------------------
 * Pin map (spec section 3).  All 3.3 V outputs.
 * SER/SRCLK/RLCK go to the HCT595s, which accept 3.3 V logic-high directly.
 * QSR0/QSR1/BLANK/hRST/vRST land on plain HC inputs: they MUST be level
 * shifted in hardware (HCT buffer, or run those chips at 3.3 V).  See README.
 * --------------------------------------------------------------------------- */
#define PIN_SER    11   /* SPI MOSI -> U8 DS (pin 14), first 595 in the chain  */
#define PIN_SRCLK  12   /* SPI SCK  -> all three 595 SHCP (pin 11)             */
#define PIN_RLCK   10   /* latch strobe -> all three 595 STCP (pin 12)         */
#define PIN_QSR0   13   /* bus request  -> U23 pin 1 (HC08 AND input)          */
#define PIN_QSR1   14   /* write strobe -> U23 pin 2 (HC08 AND input)          */
#define PIN_BLANK   9   /* bus ownership select -> U6/U22 OE, SRAM OE#,        */
                        /*   pixel-latch OE, 595 OE# (as NBLANK)               */
#define PIN_HRST   47   /* H counter reset -> U2/U10 MR# (pin 1)               */
#define PIN_VRST   48   /* V counter reset -> U11/U12/U13 MR# (pin 1)          */

/* -----------------------------------------------------------------------------
 * SPI clock for the 595 daisy chain.  8 MHz is breadboard-safe; a clean board
 * can usually take 20-27 MHz.  Drop to 1-4 MHz while debugging flaky wiring.
 * --------------------------------------------------------------------------- */
#define GPU_SPI_HZ 8000000UL

/* -----------------------------------------------------------------------------
 * Polarities.
 *
 * BLANK_WRITE_LEVEL: level of the BLANK pin while the ESP32 owns the buses
 * (counter-side address buffer U6 off, ESP32-side buffer U22 on, SRAM OE#
 * HIGH, pixel latches released, 595 outputs enabled via NBLANK).  Default 1
 * (HIGH).  If the board inverts BLANK before it reaches those enables, flip
 * this single define - all code derives from it.
 *
 * QSR0/QSR1 are ACTIVE-HIGH by construction: they feed the U23 HC08 AND gate,
 * whose output goes through an HC04 inverter to SRAM WE#.  Both high => WE#
 * low.  Not a knob; the gate defines it.
 *
 * RLCK latches the 595 shift registers to their output pins on the RISING
 * edge (STCP).  Idle low, pulse high.  Not a knob; the 595 defines it.
 *
 * RST_ACTIVE_LEVEL: HC163 MR# is active-LOW, so hRST/vRST reset when driven
 * low and must idle high.  If the board buffers/inverts them, flip this.
 * --------------------------------------------------------------------------- */
#define BLANK_WRITE_LEVEL 1
#define BLANK_SCAN_LEVEL  ((BLANK_WRITE_LEVEL) ? 0 : 1)

#define RST_ACTIVE_LEVEL  0
#define RST_IDLE_LEVEL    ((RST_ACTIVE_LEVEL) ? 0 : 1)

/* -----------------------------------------------------------------------------
 * Pulse widths / settle times (microseconds, via delayMicroseconds).
 * Generous breadboard values; a clean board tolerates 0-1 everywhere because
 * a single digitalWrite edge is already >100 ns.
 * --------------------------------------------------------------------------- */
#define QSR_SETTLE_US    3  /* after BLANK->write + QSR0 high, before 1st byte */
#define RLCK_PULSE_US    1  /* 595 output-latch strobe high time               */
#define QSR1_PULSE_US    2  /* write strobe high time (= WE# low time)         */
#define BUS_RELEASE_US   2  /* after QSR0 low, before BLANK returns to scan    */
#define RST_PULSE_US     5  /* hRST/vRST active time                           */

/* -----------------------------------------------------------------------------
 * Present behaviour.
 *
 * PRESENT_CHUNK_LINES: 0 = gpu_present() claims the bus once and streams the
 * whole 19200-byte frame (fastest, but the display shows nothing while the
 * write runs, because BLANK is held in the write state).  N>0 = claim/release
 * the bus every N scanlines so the scanout keeps refreshing the monitor
 * between chunks (visible tearing, but a live picture).
 *
 * RESET_SCAN_AFTER_PRESENT: 1 = pulse hRST+vRST after each full present so
 * the scan restarts at the top of the frame in a known state.  0 = let the
 * counters free-run.
 * --------------------------------------------------------------------------- */
#define PRESENT_CHUNK_LINES      0
#define RESET_SCAN_AFTER_PRESENT 1

/* -----------------------------------------------------------------------------
 * Demo pacing knobs.
 * --------------------------------------------------------------------------- */
#define STAGE4_REWRITE_MS 1000  /* stage 4: re-write the pixel this often      */
#define STAGE5_HOLD_MS    2000  /* stage 5: hold solid color / gradient        */

#define SERIAL_BAUD 115200

#endif /* FALLOUT_CONFIG_H */
