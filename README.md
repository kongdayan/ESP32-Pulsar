# ESP32-Pulsar

A multi-purpose **ESP32-S3** display badge: a **ST77916 QSPI 360×360 round display** with **CST816S capacitive touch**, built with [LVGL 8.3](https://lvgl.io) and [ESP32_Display_Panel v1.0.4](https://github.com/esp-arduino-libs/ESP32_Display_Panel).

![Build](https://github.com/kongdayan/ESP32-Pulsar/actions/workflows/build.yml/badge.svg)

---

## Hardware

| Component | Details |
|-----------|---------|
| MCU | ESP32-S3 (DevKitC-1, 16 MB flash, 8 MB PSRAM) |
| Display | ST77916 · 360×360 · QSPI 4-line |
| Touch | CST816S · I2C |
| TF card | SD_MMC 4-bit |
| Backlight | PWM via LEDC |

### Pin Configuration (`hal/pincfg.h`)

| Signal | GPIO |
|--------|------|
| Backlight PWM | 15 |
| Display RST | 47 |
| Display CS | 10 |
| Display SCK | 9 |
| Display DATA0–3 | 11–14 |
| Touch SCL | 8 |
| Touch SDA | 7 |
| Touch INT | 41 |
| Touch RST | 40 |
| SD D0-D3 | 2 / 1 / 6 / 5 |
| SD CLK | 3 |
| SD CMD | 4 |

---

## Project Structure

```
ESP32-Pulsar/
├── main.cpp                     # Arduino setup() / loop()
├── ESP32-Pulsar.ino             # Arduino IDE stub
├── platformio.ini               # PlatformIO build config
├── lv_conf.h                    # LVGL feature flags
├── CMakeLists.txt               # Optional ESP-IDF/CMake project metadata
├── AGENTS.md                    # Coding-agent project instructions
│
├── core/                        # Hardware-free logic (unit-testable, no LVGL)
│   ├── app_config.h             # Global constants: screen size, timings, paths, theme
│   ├── nav_map.c/.h             # Screen navigation topology (left/right → target + anim)
│   ├── ui_math.h/.c             # Angles, clamping, percent mapping
│   ├── ui_theme.c/.h            # Dark/light palette tables
│   ├── power_mgmt.c/.h          # Idle-timeout / day-night backlight policy
│   ├── display_rotation.c/.h    # Panel rotation & touch remapping
│   ├── hexball.c/.h             # Hex-Ball physics, scoring, drag-rotate
│   ├── cube3d.c/.h              # 3D cube projection, hit zones, inertia
│   ├── watchface.c/.h           # Dot-matrix glyphs, percent/battery formatting
│   └── video_source.c/.h        # TF-card RGB565 file reader (mockable)
│
├── common/
│   └── ui_screen.c/.h           # Shared LVGL screen scaffolding (lazy create, nav wiring,
│                                #   widget factories, timer lifecycle)
│
├── hal/
│   ├── display.cpp              # ST77916 + CST816S + LEDC backlight driver, LVGL init
│   ├── display.h
│   ├── sd_card.cpp/.h           # SD_MMC mount at /sdcard
│   └── pincfg.h                 # GPIO pin definitions (and only GPIO pins)
│
├── screens/                     # One .c/.h pair per screen, views only
│   ├── screen_dashboard.c/.h    ├── screen_info.c/.h
│   ├── screen_image.c/.h        ├── screen_video.c/.h
│   ├── screen_about.c/.h        ├── screen_agent.c/.h   (Hex-Ball)
│   ├── screen_3dmodel.c/.h      ├── screen_codex_usage.c/.h
│   └── *_layout.h               # per-screen geometry / colour / text constants
│
├── ui/                          # SquareLine-generated layer — avoid hand edits
│   ├── ui.c / ui.h              # Theme init, startup screen, shared declarations
│   ├── ui_helpers.c / .h        # Shared LVGL helper functions
│   └── ui_img_*.c               # Embedded image assets
│
├── tests/                       # Host-side unit tests (no hardware, no flashing)
│   ├── Makefile                 # make -C tests test | coverage | check-coverage
│   ├── unit/                    # pure-logic tests (core/, common/)
│   ├── lvgl/                    # real LVGL: rendering, nav, interaction tests
│   ├── support/                 # fakes for hal/, Arduino, heap_caps + virtual display/touch
│   └── reference/               # verbatim pre-refactor maths, used for differential tests
│
├── assets/                      # Source image assets
├── cache/                       # Generated image thumbnails/cache
├── .claude/                     # Claude Code command references
├── .github/workflows/           # GitHub Actions CI
├── .vscode/                     # Editor settings
│
└── backup/                      # Archived legacy design exports
```

The active UI is implemented directly in LVGL C code under `screens/`. Swipe left/right to navigate between lazily-created screens.

#### TF Card Video Screen

The video screen mounts the TF card at `/sdcard` and plays `/sdcard/video.rgb`.
The file is a raw stream of consecutive `360×360` RGB565 frames, with each frame
exactly `259200` bytes. Playback is scheduled at 42 ms per frame, about 24 FPS,
and loops back to the beginning at EOF.

Convert an MP4 to the expected raw format with:

```bash
ffmpeg -i input.mp4 -vf "scale=360:360,fps=24" -pix_fmt rgb565be -f rawvideo video.rgb
```

#### Hex-Ball Game (Agent screen)

- A white ball starts at the screen centre with an initial velocity
- Three concentric hexagonal rings (blue / orange / green) each have one open gap
- Touch and drag near a ring to rotate it
- Ball passing through a gap scores a point — the ring breaks and respawns at the outer edge, then slowly shrinks inward
- Ball bouncing off a solid side reflects elastically based on surface normal

---

## Prerequisites

### PlatformIO (recommended)

```bash
pip install platformio
```

> The project uses the [pioarduino](https://github.com/pioarduino/platform-espressif32) platform to provide **Arduino ESP32 3.1.1 / IDF 5.3**, which is required by ESP32_Display_Panel v1.x. The official `espressif32` PlatformIO platform ships IDF 4.x and is incompatible.

### Arduino IDE (optional)

Install **Arduino ESP32 core ≥ 3.1.0** via Boards Manager, then add these libraries via Library Manager:

| Library | Version |
|---------|---------|
| ESP32_Display_Panel | v1.0.4 |
| ESP32_IO_Expander | v1.1.0 |
| esp-lib-utils | v0.2.0 |
| lvgl | 8.3.11 |

---

## Build & Flash

### PlatformIO

```bash
git clone https://github.com/kongdayan/ESP32-Pulsar.git
cd ESP32-Pulsar

# Compile only
pio run

# Compile + upload to connected ESP32-S3
pio run --target upload

# Open serial monitor (115200 baud)
pio device monitor

# List connected devices
pio device list
```

Dependencies are fetched automatically on first build.

> If upload fails with "Failed to connect", hold the **BOOT** button on the DevKit while clicking Upload, then release after connection is established.

### Manual Flash with esptool

```bash
# Flash app partition only (quickest for firmware updates)
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash 0x10000 .pio/build/esp32s3/firmware.bin

# Full flash (bootloader + partition table + app)
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash \
  0x0000  .pio/build/esp32s3/bootloader.bin \
  0x8000  .pio/build/esp32s3/partitions.bin \
  0xe000  ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/esp32s3/firmware.bin

# Read chip info
esptool.py --chip esp32s3 --port /dev/ttyUSB0 chip_id

# Erase entire flash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 erase_flash
```

### Arduino IDE

1. Open `ESP32-Pulsar.ino` in Arduino IDE 2.x.
2. Select board: **ESP32S3 Dev Module**.
3. Set **USB CDC On Boot** → **Enabled**.
4. Click **Upload**.

---

## Unit Tests (host-side, no hardware)

The pure logic (`core/`), the shared screen scaffolding (`common/`) and every screen
are exercised on the host with a real LVGL build — a 360×360 virtual framebuffer,
a scripted touch device and fakes for `hal/`, `Arduino` and `heap_caps`.

```bash
pio run                 # once, so .pio/libdeps/*/lvgl exists
make -C tests test             # run all unit + LVGL tests
make -C tests coverage         # line / function / branch report
make -C tests coverage-html    # browsable report in tests/build/coverage-html
make -C tests check-coverage   # exit non-zero if line coverage < 90% (CI gate)
```

Run a subset with `make -C tests test RUN=video` (maps to the test-name filter),
and see [`tests/README.md`](tests/README.md) for what is mocked and what is
deliberately left untested.

Current status: **99.5% line / 100% function coverage** over
`core/ + common/ + screens/ + ui/ui.c + ui/ui_helpers.c + main.cpp`.
`hal/display.cpp` and `hal/sd_card.cpp` talk to QSPI/I²C/SDMMC registers and are
covered on-device instead (their decision logic was extracted into `core/power_mgmt`
and `core/display_rotation`, both at 100%).

---

## Configuration

### LVGL (`lv_conf.h`)

Only the widgets used by this project are enabled to keep binary size small:

**Enabled:** ARC, BTN, CHECKBOX, IMG, LABEL, SLIDER, SWITCH, CALENDAR, SPINNER, TABVIEW, BTNMATRIX, FLEX layout, GRID layout, Montserrat fonts (14, 44), Default theme.

**Disabled:** DROPDOWN, ROLLER, TEXTAREA, KEYBOARD, CHART, METER, TABLE, SPINBOX, MENU, all demo/benchmark widgets.

### Display Driver (`hal/display.cpp`)

Key parameters:

```cpp
#define TFT_SPI_FREQ_HZ (50 * 1000 * 1000)  // QSPI clock — lower if signal quality is poor
const size_t lv_cache_rows = 72;             // LVGL draw buffer height (rows)
```

### Adding a New Screen

1. Create `screens/screen_newname.c` following the same pattern as existing screens.
2. Declare `void screen_newname_init(void)` and `lv_obj_t **screen_newname_get_ptr(void)` in `ui/ui.h`.
3. Add `+<screens/screen_newname.c>` to `build_src_filter` in `platformio.ini`.
4. Wire up swipe navigation in adjacent screens using `_ui_screen_change()`.

---

## AI Coding Assistants

This project includes configuration files for AI coding assistants:

- **Claude Code** — invoke `/pio` for build, upload, flash, and debug command reference
- **Codex / OpenAI agents** — see `AGENTS.md` for project context, build rules, and GPIO reference

---

## CI/CD

Every push and pull request to `main` triggers a GitHub Actions build that:

1. Restores PlatformIO package cache (keyed on `platformio.ini`)
2. Compiles the firmware with `pio run`
3. Uploads `firmware.bin` + `firmware.elf` as a workflow artifact (retained 30 days)

The build badge at the top of this README reflects the current `main` branch status.

---

## Library Versions

| Library | Version | Notes |
|---------|---------|-------|
| ESP32_Display_Panel | v1.0.4 | Requires Arduino ESP32 ≥ 3.1.0 / IDF 5.x |
| ESP32_IO_Expander | v1.1.0 | |
| esp-lib-utils | v0.2.0 | |
| lvgl | 8.3.11 | Config in `lv_conf.h` |
| Arduino ESP32 core | 3.1.1 | IDF 5.3, via pioarduino platform |

---

## License

This project is provided as-is for demonstration purposes.
