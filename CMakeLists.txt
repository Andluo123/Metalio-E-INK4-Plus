# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "2.0.66")

add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Metalio-E-Ink4-Plus)

# 随 app 一并烧录 fontpack 到 font_data（替换 epd_font24）
set(FONTPACK_FILE "${CMAKE_SOURCE_DIR}/use_font/font.fontpack")
if(EXISTS "${FONTPACK_FILE}")
    esptool_py_flash_to_partition(flash font_data "${FONTPACK_FILE}")
    message(STATUS "font_data flash: ${FONTPACK_FILE}")
else()
    message(WARNING "use_font/font.fontpack missing — font_data will not be flashed")
endif()

# ---------------------------------------------------------------------------
# 产线全量包：idf.py build 结束时自动生成一个「首地址 0x0」的整片镜像。
#
# 产线烧录工具只接受「单个文件 + 首地址 0x0」，所以这一步不能靠人去拼。
# 合成 + 布局校验都在 scripts/merge_full_image.py 里（校验 0x2000/0x8000/app
# 三处 magic），CMake 只负责在 app 及各资源段就绪后触发它。
#
# 产物：build/Metalio-E-Ink4-Plus-full-0x0.bin
# ---------------------------------------------------------------------------
set(FULL_IMAGE "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}-full-0x0.bin")
if(EXISTS "${CMAKE_BINARY_DIR}/flash_args")
    # flash_args 里列出了所有要烧录的段，正好可以当作依赖列表。
    set(_full_image_deps "")
    file(STRINGS "${CMAKE_BINARY_DIR}/flash_args" _flash_args_lines)
    foreach(_line IN LISTS _flash_args_lines)
        if(_line MATCHES "^0x[0-9a-fA-F]+[ \t]+(.+)$")
            set(_rel "${CMAKE_MATCH_1}")
            if(IS_ABSOLUTE "${_rel}")
                list(APPEND _full_image_deps "${_rel}")
            else()
                list(APPEND _full_image_deps "${CMAKE_BINARY_DIR}/${_rel}")
            endif()
        endif()
    endforeach()

    add_custom_command(
        OUTPUT "${FULL_IMAGE}"
        COMMAND ${PYTHON} "${CMAKE_SOURCE_DIR}/scripts/merge_full_image.py"
            --build-dir "${CMAKE_BINARY_DIR}"
            --output "${FULL_IMAGE}"
        DEPENDS
            ${_full_image_deps}
            "${CMAKE_SOURCE_DIR}/scripts/merge_full_image.py"
        COMMENT "Merging production full image (flash from 0x0)"
        VERBATIM)
    add_custom_target(merge_full_image ALL DEPENDS "${FULL_IMAGE}")
    message(STATUS "full image: ${FULL_IMAGE} (flash at 0x0)")
else()
    message(WARNING "flash_args missing — full image will not be generated")
endif()
