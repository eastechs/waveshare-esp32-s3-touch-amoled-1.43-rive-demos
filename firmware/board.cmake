# Shared by the ESP-IDF projects under examples/.
get_filename_component(TOOLKIT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(IDF_TARGET esp32s3)
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
include("${TOOLKIT_ROOT}/board/esp-idf.cmake")
set(COMPONENTS main)
