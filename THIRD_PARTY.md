# Third-party sources

The MIT license at the root covers the original toolkit code, artwork and documentation. The following dependencies retain their own licenses and notices.

| Source | Use in this repository |
| --- | --- |
| [rive-thorvg v0.2.0](https://github.com/xseignard/rive-thorvg/tree/208c68c9b638949c49ce3308a03632105ba3ba43) | The pinned MIT-licensed bridge supplies the C API and the ESP-IDF component. The small timeline patch is applied to a build-directory copy of that bridge. See its [license](vendor/rive-thorvg/LICENSE). |
| [Rive C++ runtime](https://github.com/rive-app/rive-runtime/tree/8bb659992d81f6f53a72ea21f7079149a1b8fced) and [ThorVG](https://github.com/thorvg/thorvg/tree/8e912af4f050e1c25dcddf0c4324779516f7a028) | Recursive submodules of the bridge. Their source trees include component-specific licenses. |
| [Waveshare board examples](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.43/Resources-And-Documents) | The setup script reads the LCD probe and initialization tables from `07_LVGL_Test` and the I²C component and IMU register definitions from `03_I2C_QMI8658` in the V3 demo ZIP. Files extracted locally under `vendor/waveshare` come from Waveshare. |
| [ESP-IDF v5.3.2](https://github.com/espressif/esp-idf/tree/v5.3.2) and [Espressif's SH8601 driver](https://components.espressif.com/components/espressif/esp_lcd_sh8601) | The board examples use ESP-IDF and resolve the display driver through its component manager. Each example has a dependency lock. |
| [Inter](https://rsms.me/inter/) | The thermostat and VU meter use outlined labels from Inter Regular and Bold. The optional art generators read the font files from the pinned runtime test assets. Inter is distributed under the SIL Open Font License 1.1. |
