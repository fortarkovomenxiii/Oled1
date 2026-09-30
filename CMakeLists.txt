# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Oled1)
# Workaround for GCC segfault in esp_lcd_panel_rgb.c
# (файл не нужен для I2C OLED, но почему-то собирается)
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/components/esp_lcd/rgb/esp_lcd_panel_rgb.c
    PROPERTIES HEADER_FILE_ONLY TRUE
)
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/components/esp_lcd/rgb/esp_lcd_panel_rgb.c
    PROPERTIES COMPILE_FLAGS "-O0"
)