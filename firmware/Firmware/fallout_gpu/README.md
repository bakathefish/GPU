# FALLOUT GPU — ESP32-S3 Firmware

Firmware for the render half of the FALLOUT GPU: a discrete-logic GPU
(74-series counters + IS62C256 SRAM framebuffer + R-2R DAC → VGA) whose
scanout runs with **zero software**. This ESP32-S3 code only decides *what*
the pixels are: it rasterizes into a local 160×120 RGB332 framebuffer and
streams it into the scanout SRAM through a 3× 74HCT595 write bridge during
blanking.

Even if none of this firmware runs, the discrete half scanning power-on
garbage to a monitor is already the core demo. Protect that path first.

## File map

| File | Role |
|---|---|
| `fallout_gpu.ino` | Entry point: `setup()`/`loop()`, dispatches to the configured demo stage |
| `config.h` | **All** pins, polarities, timing knobs, `DEMO_STAGE` select |
| `gpu_bus.h/.cpp` | SPI + 595 chain + QSR0/QSR1/BLANK/hRST/vRST arbitration; spec §4 API (`gpu_init`, `gpu_pixel`, `gpu_clear`, `gpu_line`, `gpu_tri`, `gpu_present`) |
| `gpu_gl.h/.c` | Pure C99 raster core (clear/pixel/Bresenham line/filled triangle, top-left fill rule) — compiles standalone on any host |
| `gpu_math.h/.c` | Pure C99 vec3/mat4 (column-major), rotations, perspective projection |
| `scene.h/.c` | Pure C99 scenes: test pattern, gradient, spinning cube frame |
| `demos.h/.cpp` | Maps `DEMO_STAGE` 4/5/6 to scene + present calls |

## Wiring (spec §3 pin map)

Avoid on the N16R8: GPIO0/3/45/46 (strapping), GPIO19/20 (USB), GPIO26–32
(flash), GPIO33–37 (octal PSRAM), GPIO43/44 (UART0).

| ESP32 role            | GPU net | Lands on                                            | Suggested GPIO |
|-----------------------|---------|-----------------------------------------------------|----------------|
| SPI MOSI              | SER     | U8 DS (pin 14), first 595                            | GPIO11         |
| SPI SCK               | SRCLK   | all three 595 SHCP (pin 11)                          | GPIO12         |
| Latch strobe          | RLCK    | all three 595 STCP (pin 12)                          | GPIO10         |
| Bus request           | QSR0    | U23 pin 1 (AND input)                               | GPIO13         |
| Write strobe          | QSR1    | U23 pin 2 (AND input)                               | GPIO14         |
| Bus ownership select  | BLANK   | U6/U22 OE, SRAM OE#, latch OE, 595 OE# (as NBLANK)   | GPIO9          |
| H counter reset       | hRST    | U2/U10 MR# (pin 1)                                   | GPIO47         |
| V counter reset       | vRST    | U11/U12/U13 MR# (pin 1)                              | GPIO48         |

Power: module 3V3 and GND, **ground common with the 5V logic** — no common
ground, no demo.

Minimum to be programmable: the first 6 rows. hRST/vRST only if the ESP32
sequences frames (`RESET_SCAN_AFTER_PRESENT` in `config.h`).

### ⚠ Level shifting (hardware, not fixable in code)

ESP32 outputs are **3.3 V**. Signals going to plain **HC**-family inputs —
**QSR0, QSR1, BLANK, hRST, vRST** — sit *below* the HC logic-high threshold
(≈3.5 V at 5 V VCC). Route them through an HCT buffer, or run those chips at
3.3 V. The 595s are **HCT** and accept 3.3 V directly, so SER / SRCLK / RLCK
need no shifting. Symptom of skipping this: writes "sometimes" work, resets
never release (hRST/vRST idle-high at 3.3 V may read as low = stuck reset).

## Build & flash (arduino-cli, esp32 core 3.x)

The project directory contains a space — always quote the path.

```powershell
# Compile (ESP32-S3 N16R8: 16MB flash, octal PSRAM; CDC so Serial prints over USB)
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=cdc" "C:/Users/anees/OneDrive/Desktop/fallout_ gpu/Firmware/fallout_gpu"

# Upload (replace COM5 with your port; list ports with: arduino-cli board list)
arduino-cli upload -p COM5 --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=cdc" "C:/Users/anees/OneDrive/Desktop/fallout_ gpu/Firmware/fallout_gpu"

# Serial monitor
arduino-cli monitor -p COM5 --config 115200
```

Notes:
- The 19,200-byte framebuffer lives in internal RAM; PSRAM is not required —
  the `PSRAM=opi` option merely matches the N16R8 module.
- If the board enumerates via a USB-UART chip instead of native USB, drop
  `CDCOnBoot=cdc` and use the UART port.
- If upload fails, hold BOOT while tapping RESET to force the download mode.

## Picking a demo stage

Edit one line in `config.h`, rebuild, reflash:

```c
#define DEMO_STAGE 6   /* 4, 5, 6, or anything else for the test pattern */
```

## Test ladder (spec §6) — what each stage proves

Stages 1–3 are **hardware-only** (clock alive, counters tick, preloaded SRAM
scanning to a screen with NO ESP32 — "done when: a pattern appears on a
monitor or dongle. THIS IS THE PRIORITY").

| `DEMO_STAGE` | Ladder stage | What it does | Definition of done (spec, verbatim) |
|---|---|---|---|
| 4 | ESP32 writes one pixel | One white pixel (0xFF) at (80,60) via `gpu_pixel` + `gpu_present_rect`, rewritten every second | "that pixel lights at the correct coordinates. Validates the write bridge plus QSR0/QSR1/BLANK arbitration." |
| 5 | Clear and gradient | Solid blue for 2 s, then a full-screen gradient, alternating | "solid color first, then a gradient renders." |
| 6 | Primitives to cube | Spinning cube: 12 filled triangles, 6 face colors, backface culling, two-axis rotation | "the cube spins. This is the headline demo." |
| other | fallback | Color bars + white border + diagonal (also the simulator reference image) | — |

## Key knobs (`config.h`)

| Knob | Default | Meaning |
|---|---|---|
| `GPU_SPI_HZ` | 8 MHz | 595 chain clock. Drop to 1–4 MHz on flaky breadboards; clean boards take 20+ MHz |
| `BLANK_WRITE_LEVEL` | 1 | BLANK level while the ESP32 owns the buses. Flip if the board inverts BLANK |
| `RST_ACTIVE_LEVEL` | 0 | hRST/vRST active level (HC163 MR# is active-low). Flip if buffered/inverted |
| `QSR_SETTLE_US` etc. | 1–5 µs | Strobe widths / settle times. Shrink toward 0–1 µs once wiring is solid (big present speedup) |
| `PRESENT_CHUNK_LINES` | 0 | 0 = whole frame per bus claim (fastest, screen blanks during write); N = release the bus every N lines so the display keeps scanning |
| `RESET_SCAN_AFTER_PRESENT` | 1 | Pulse hRST+vRST after each present so the frame restarts cleanly |

A full present is 19,200 pixels × (24 SPI bits + RLCK + QSR1 pulses); at the
defaults expect roughly 100–200 ms per frame (~5–8 fps cube). Raising
`GPU_SPI_HZ` and shrinking the pulse knobs is the speed path.

## Troubleshooting (spec §8 gotchas)

| Symptom | Check |
|---|---|
| Nothing on screen at all, even without the ESP32 | Crystal load caps must be **pF not nF** (verify C19/C20; ~22 pF for 25 MHz). Scope the oscillator, then confirm the divide-by-4 output net actually reaches the counter FCLK pins |
| Picture exists but geometry is scrambled/mirrored | U2 output bit-reversal (Q0→H3 …) must be fixed to Q0→H0 … Q3→H3 |
| Chips getting hot, garbage bus levels | BLANK and BLANK2 must be **complementary** — if both address buffers (U6/U22) enable at once they fight and cook |
| SRAM corrupts during scan, or writes trash neighbors | OE# and WE# must **never** be low simultaneously. Verify BLANK polarity (`BLANK_WRITE_LEVEL`) and that the claim/release ordering in `gpu_bus.cpp` matches the board |
| Writes unreliable / resets stuck / counters frozen | 3.3 V into plain-HC inputs (QSR0/QSR1/BLANK/hRST/vRST) is below the HC threshold — add the HCT buffer (level-shift note above) |
| Stage 4 pixel lands at the wrong place | Byte order into the chain is addr-high, addr-low, data (first byte shifts through to U20). Check U8/U19/U20 daisy-chain order and that U2's bit-reversal fix is in |
| Stage 4 pixel never appears | Confirm U23 pin 3 (AND output) is routed to the HC04 inverter input, **not** tied to GND (known schematic fix); scope WE# for a low pulse while QSR1 fires |
| Everything works but colors are wrong | RGB332 packing is RRRGGGBB (`RGB332()` in `gpu_gl.h`); check R-2R ladder bit order into the HC574 latches |
| Want to read pixels back | You can't — there is no digital read-back path (output is analog via the DAC). Not needed for the visual demo |
