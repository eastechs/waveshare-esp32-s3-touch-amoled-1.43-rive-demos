# Rive on a round AMOLED

Render a `.riv` file on a computer or play it on the **Waveshare ESP32-S3-Touch-AMOLED-1.43 (non-C)**. The same software renderer drives both. The board examples also show how to trade flash space for faster drawing and how to drive the panel directly from an accelerometer.

![Pocket watch movement, thermostat at 72 degrees and brass VU meter](docs/rendering-examples.png)

| Example | What it does |
| --- | --- |
| [Rive player](examples/rive-player/main/main.c) | Plays the pocket watch’s wheel train or your own embedded `.riv`. Forwards touch to the artboard and transfers only changed regions after the first frame. |
| [Thermostat](examples/thermostat/main/main.c) | Touch the rim to change the setpoint. At startup the firmware renders the Rive view model at each setting, then releases the runtime. A drag blends two cached landscapes and draws the selected indicators. |
| [Baked VU meter](examples/baked-meter/main/main.c) | Replays an eight-second Rive animation at native panel resolution. Firmware restores the old needle pixels and applies the current pose from flash. |
| [Tilt water](examples/tilt-water/main/main.c) | Tilt the board and the water follows gravity. Two cores shade alternating groups of rows directly into the display’s DMA buffers. |

## Render a file on your computer

From the repository root, use Git, CMake 3.20 or later, a C11/C++17 toolchain, the Unix `patch` utility and Python 3 for the tests. The renderer builds on macOS with Apple Clang. A board, ESP-IDF and the Rive authoring CLI are optional for this workflow.

```sh
git submodule update --init --recursive --depth 1
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
ctest --test-dir build --output-on-failure

build/host/riv-render assets/watch/pocket-watch.riv watch.ppm --artboard Movement --time 1
```

The command writes a PPM image of the watch one second into its animation. Change the output extension to `.rgb565` for tightly packed, row-major little-endian pixels. For example, this writes a full-size image ready for the board’s RGB565 blitter.

```sh
build/host/riv-render assets/thermostat/thermostat.riv dial.rgb565 --size 466x466
```

The default viewport is 233 × 233. A named artboard is optional. The player centers and fits the selected artboard, advances its default or first state machine, and falls back to the first timeline on an artboard that has only animations. Static artboards render a still. The desktop and embedded builds use the same [timeline change](runtime/timeline.patch).

Use self-contained files with embedded assets. The embedded backend uses outlined lettering, leaves live text unrendered and omits audio and scripting. Transparent pixels composite against black on the panel and in the host output.

## Build the firmware

Activate an ESP-IDF **5.3.2** environment for ESP32-S3. The examples configure a 466 × 466 QSPI panel, octal PSRAM and a 16 MiB flash layout. From the toolkit root, fetch the board support and build the player.

```sh
git submodule update --init --recursive --depth 1
python3 tools/setup-board.py
idf.py -C examples/rive-player build
```

The setup script checks the Waveshare V3 ZIP against its SHA-256 digest before installing the required files. If you already have the ZIP, pass its path to `tools/setup-board.py`. ESP-IDF resolves the pinned Espressif components during configuration.

To play your own file, supply its absolute path. Omit `RIV_ARTBOARD` on a fresh build to use the file’s default artboard, or set it to a name. The bundled watch uses `Movement`.

```sh
idf.py -C examples/rive-player -DRIV_FILE="/absolute/path/to/your.riv" -DRIV_ARTBOARD="" build
```

ESP-IDF caches those settings. To return to the watch, build with `-DRIV_FILE="" -DRIV_ARTBOARD=""`. To run a built example on hardware, replace `PORT` with the board’s serial port. Flashing installs the example firmware and partition table. Use Ctrl+] to leave the monitor.

```sh
idf.py -C examples/rive-player -p PORT flash monitor
```

Use `examples/thermostat`, `examples/baked-meter` or `examples/tilt-water` in the same commands to build and flash those projects. The meter and water projects build with just ESP-IDF and the Waveshare setup. Run `tools/build-firmware` to compile all four projects in one pass.

## Reuse the board code

The [board component](board/) contains the display and sensor APIs. Use `firmware/board.cmake` and an example project’s top-level CMake file as a reference when adding it to another ESP-IDF project. The PSRAM and partition defaults are in [firmware](firmware/).

- **Panel.** Initialize it once and draw from one task. `display_frame_region` takes a rectangle inside a full 233 × 233 premultiplied ARGB8888 frame. It rotates the image clockwise, doubles each pixel and converts it to RGB565. `display_rgb565_region` takes a tightly packed native-resolution patch in upright artwork coordinates. [Display API](board/display.h)
- **Streaming.** For procedural images, `display_scanlines` and `display_parallel_rows` take physical panel coordinates and expect RGB565 in wire byte order. The latter splits a tile across the two cores and overlaps painting with the preceding transfer. [Water shader](examples/tilt-water/main/water.c)
- **Input.** Initialize the shared I²C bus, then enable the FT3168 touch controller or QMI8658 accelerometer. Touch arrives in upright 466 × 466 artwork coordinates. Divide by two before calling `rive_screen_to_artboard` with the half-resolution framebuffer. Acceleration uses screen-relative axes in g. The standalone shake detector takes samples at 100 Hz. [Sensor API](board/sensors.h) · [Shake detector](board/shake.h)

The LCD probe selects the SH8601 command table for ID `0x86`. Other values select the CO5300 sequence and a six-column X offset, following Waveshare’s non-C example. Board pin assignments and the vendor code come from the [Waveshare resources](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.43/Resources-And-Documents).

## Edit the artwork

The watch, thermostat and meter include editable Rive CLI projects under [assets](assets/). The checked-in exports and baked files are ready to build. To regenerate the source from its drawing parameters, use Python 3.9 or later. The two instruments with outlined Inter labels also use the `fonttools` package and the recursive Rive submodule.

```sh
python3 tools/build_watch_art.py
python3 tools/build_thermostat_art.py
python3 tools/build_vu_meter_art.py
python3 tools/build_water_art.py
```

After changing the watch or thermostat RML, use the Rive CLI’s local export to replace its adjacent `.riv`. Local export requires a signed-in Rive CLI. For the VU meter, the bake tool uses the CLI’s headless capture mode and FFmpeg to convert the frames to RGB565 patches.

```sh
rive assets/watch --publish=local --output-dir=.
rive assets/thermostat --publish=local --output-dir=.
python3 tools/bake_meter.py --rive rive
```

The meter bake was verified with Rive CLI 1.9.2.

The VU file contains a base frame, a little-endian table of 201 byte offsets and 200 sets of pixel replacements. Each replacement stores a 16-bit position in the needle rectangle and a 16-bit color. Every pose refers to the base frame so playback can skip directly to the current time. [Encoder](tools/bake_meter.py) · [Decoder](examples/baked-meter/main/vu_meter.c)

To run just the board logic tests, configure a separate host build with `-DWITH_RIVE=OFF`. Those tests exercise the water simulation, RGB565 display patches, changed-frame transfers and shake detection without downloading the renderer.

## License

Original toolkit code and artwork use the MIT license. Dependency licenses and artwork credits are listed separately. [License](LICENSE) · [Third-party sources](THIRD_PARTY.md)
