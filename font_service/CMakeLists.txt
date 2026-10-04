idf_component_register(SRCS "src/font_service.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES "driver"
                       REQUIRES "version_manager")

include(${CMAKE_CURRENT_LIST_DIR}/../version_manager/cmake/version_helper.cmake)

register_component_version(
    NAME "font_service"
    VERSION_FILE "${CMAKE_CURRENT_LIST_DIR}/version.txt"
    OUTPUT_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/font_service_version.h"
)

target_sources(${COMPONENT_LIB} PRIVATE "src/font_builtin.c")
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/longyin_column_lsb_font_data.bin" BINARY)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/longyin_column_lsb_unicode_to_gbk.bin" BINARY)
target_sources(${COMPONENT_LIB} PRIVATE "src/font_display.c")
target_include_directories(${COMPONENT_LIB} PRIVATE "include")
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

if(CONFIG_FONT_SERVICE_ENABLE_GBK OR DEFINED FONT_SERVICE_ENABLE_GBK)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_FONT_SERVICE_ENABLE_GBK)
    target_sources(${COMPONENT_LIB} PRIVATE "src/font_gbk.c")
    target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../display_driver/examples/tft_test/main/unicode_to_gbk.bin" BINARY)
    target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../display_driver/examples/tft_test/main/gbk_font_data.bin" BINARY)
endif()

if(CONFIG_FONT_SERVICE_ENABLE_YUESONG OR DEFINED FONT_SERVICE_ENABLE_YUESONG)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_FONT_SERVICE_ENABLE_YUESONG)
    target_sources(${COMPONENT_LIB} PRIVATE "src/font_yuesong.c")
    target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/yuesong_unicode_to_gbk.bin" BINARY)
    target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/yuesong_font_data.bin" BINARY)
endif()

if(CONFIG_FONT_SERVICE_ENABLE_W25Q OR DEFINED FONT_SERVICE_ENABLE_W25Q)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_FONT_SERVICE_ENABLE_W25Q)
    target_sources(${COMPONENT_LIB} PRIVATE "src/font_w25q.c")
endif()
