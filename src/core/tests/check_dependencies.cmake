# Enforces the hyremote-core dependency rule (docs/adr/0001-core-boundaries.md):
#
#   no Qt Widgets/Quick/QML, Qt private/QPA, NeatVNC/AML, DRM/GBM/DMA-BUF or RKMPP/V4L2/VA-API
#   include may appear in Core, and the Core target may not link a Qt target.
#
# Run as: cmake -DCORE_DIR=<core> -P check_dependencies.cmake

if(NOT DEFINED CORE_DIR)
    message(FATAL_ERROR "CORE_DIR must be defined")
endif()

set(_forbidden
    "QtWidgets|QtQuick|QtQml|QtGui|QtOpenGL|Qt3D|QWidget|QQuick|QQml|QApplication|QGuiApplication|"
    "neatvnc|libvnc|rfb|"
    "rkmpp|rockchip|v4l2|va/va\\.h|"
    "libdrm|drm/|gbm|dma-buf|dma_buf|"
    "qpa|QPA|private/")

file(GLOB_RECURSE _core_sources
    "${CORE_DIR}/include/*.hpp" "${CORE_DIR}/include/*.h"
    "${CORE_DIR}/src/*.hpp" "${CORE_DIR}/src/*.h" "${CORE_DIR}/src/*.cpp")

if(NOT _core_sources)
    message(FATAL_ERROR "no Core sources found under ${CORE_DIR}")
endif()

set(_violations 0)
foreach(_file IN LISTS _core_sources)
    file(READ "${_file}" _content)
    string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"]?[^>\"]*[>\"]?" _includes "${_content}")
    foreach(_include IN LISTS _includes)
        if(_include MATCHES "${_forbidden}")
            message(SEND_ERROR "forbidden include in ${_file}: ${_include}")
            math(EXPR _violations "${_violations} + 1")
        endif()
    endforeach()
endforeach()

file(READ "${CORE_DIR}/CMakeLists.txt" _core_cmake)
if(_core_cmake MATCHES "find_package[ \t]*\\([ \t]*Qt|Qt6::|Qt5::|Qt::")
    message(SEND_ERROR "hyremote-core must not link a Qt target")
    math(EXPR _violations "${_violations} + 1")
endif()

if(_violations GREATER 0)
    message(FATAL_ERROR "hyremote-core dependency boundary violated (${_violations} finding(s))")
endif()

list(LENGTH _core_sources _count)
message(STATUS "hyremote-core dependency boundary OK: ${_count} source/header file(s) scanned")
