# Render .riv files into a framebuffer

Use the renderer in a C or C++ application that supplies a pixel buffer. It reads a self-contained Rive file from memory, selects an artboard, advances its animation and draws into the buffer. The application owns the display, clock and input. The same API can render a single desktop image, update a device display, or draw assets at startup and keep the pixels for later.

This package builds Rive’s C++ runtime and ThorVG through the pinned upstream `rive-thorvg` C API. The toolkit adds playback for artboards whose animation is a timeline. [C API](../vendor/rive-thorvg/include/rive_thorvg.h) · [Timeline change](timeline.patch) · [Credits and licenses](../THIRD_PARTY.md)

## Add the renderer to an ESP-IDF application

These paths assume a checkout of this toolkit under `third_party/rive-round-amoled` in your application. From that checkout, run `git submodule update --init --recursive --depth 1` once.

In your application's top-level `CMakeLists.txt`, append the component directory before including ESP-IDF's project file. Use your existing project name and settings.

```cmake
cmake_minimum_required(VERSION 3.20)
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/third_party/rive-round-amoled/runtime/esp-idf/rive_thorvg")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_device)
```

In `main/CMakeLists.txt`, declare the dependency. This example embeds a file named `scene.riv` alongside `main.c`.

```cmake
idf_component_register(SRCS "main.c" PRIV_REQUIRES rive_thorvg EMBED_FILES "scene.riv")
```

ESP-IDF exposes that file as `_binary_scene_riv_start` and `_binary_scene_riv_end`. Pass the start pointer and byte length to `rive_load`. Bytes read from storage work through the same API. Choose a flash layout with room for the linked image. A standalone ESP32-S3 build embedding the bundled watch used 1.34 MiB of app flash with ESP-IDF 5.3.2. Its test configuration used the large single-app partition (`CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y`). Enable PSRAM to suit your module and scene size. The Waveshare player uses octal PSRAM and a 16 KiB main-task stack. [Working firmware configuration](../firmware/sdkconfig.defaults)

## Add the renderer to a desktop application

From a CMake 3.20+ project with C and C++ enabled, add the runtime directory and link the same target. The target exports `rive_thorvg.h`. Building either integration requires the Unix `patch` utility.

```cmake
add_subdirectory(third_party/rive-round-amoled/runtime)
target_link_libraries(my_app PRIVATE rive_thorvg)
```

## Load and draw

Call `rive_thorvg_init(0)` once before creating any instances. This function loads a scene and draws its first frame into a caller-owned ARGB8888 buffer. Allocate at least `width * height` 32-bit pixels and keep the buffer alive until you destroy the scene.

```c
#include "rive_thorvg.h"

rive_instance_t *open_scene(const uint8_t *file, size_t length,
                            uint32_t *pixels, int width, int height)
{
    rive_instance_t *scene = rive_create(pixels, width, height, RIVE_PIXEL_FORMAT_ARGB8888);
    if (!scene) return NULL;
    if (!rive_load(scene, file, length)) {
        rive_destroy(scene);
        return NULL;
    }
    rive_advance(scene, 0.0f);
    return scene;
}
```

Present the first buffer to your display. On each subsequent tick, pass elapsed **seconds** to `rive_advance(scene, elapsed_seconds)`. A true result means the renderer drew a frame and the application can present the pixels. Use `rive_destroy(scene)` at teardown and call `rive_thorvg_term()` once after the last instance. The [desktop command](../host/render.cpp) shows the same lifecycle for rendering a snapshot to disk.

| To do this | Use |
| --- | --- |
| Select a named artboard or state machine | `rive_load_ex`. With default selection, an artboard with no state machine plays its first timeline when one is available. |
| Change a state-machine input or view-model property from firmware | `rive_set_number`, `rive_set_bool`, `rive_fire_trigger` or the `rive_vm_set_*` calls. Use the names authored in the file. |
| Forward a touch or mouse event | Convert framebuffer coordinates with `rive_screen_to_artboard`, then call `rive_pointer_down`, `rive_pointer_move` or `rive_pointer_up`. |
| Use the Waveshare round display | Present the first 233 × 233 ARGB frame with `display_frame`. Keep a copy and use `display_changes` for subsequent frames. The [board player](../examples/rive-player/main/main.c) handles touch and measures frame time. |

The embedded build supports vector paths and outlined lettering. Live text, scripting and audio are omitted. A 233 × 233 ARGB buffer takes 217,156 bytes before runtime allocations. The bridge's RGB565 mode also uses an internal ARGB staging buffer. Render cost depends on the artwork and pixel dimensions.

On the Waveshare ESP32-S3 at 233 × 233, an earlier live test of the upstream loader completed 948 full-screen transfers in 40.2 seconds (23.6 FPS). The live player's detailed pocket-watch `Movement` artboard measured 5.4 FPS with partial screen updates. These are measurements for those two files and display paths. [Loader asset](../vendor/rive-thorvg/assets/loader.riv) · [Player and watch measurements](../README.md#run-the-board-examples)

The [thermostat](../examples/thermostat/main/main.c) uses the renderer to build a cache at startup for an instrument that only needs a finite set of visual states.
