# Include before ESP-IDF's project.cmake to register the Waveshare board drivers.
get_filename_component(_waveshare "${CMAKE_CURRENT_LIST_DIR}/../vendor/waveshare" ABSOLUTE)
if(NOT EXISTS "${_waveshare}/panel_init.h")
    message(FATAL_ERROR "Run python3 tools/setup-board.py from the toolkit root first.")
endif()
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}" "${_waveshare}/read_lcd_id_bsp" "${_waveshare}/i2c_bsp")
unset(_waveshare)
