# Enforces the hyremote-core dependency rule (docs/adr/0001-core-boundaries.md):
#
#   no Qt Widgets/Quick/QML, Qt private/QPA, OpenGL/RHI/EGLFS, NeatVNC/AML, DRM/GBM/DMA-BUF or
#   RKMPP/V4L2/VA-API include or type declaration may appear in Core, and the Core target may not link
#   a Qt target.
#
# The scan covers includes and type declarations: a forward declaration can smuggle the same dependency
# family past an include-only rule. A synthetic self-check runs the identical scanner over known-bad
# sources, so a scanner that accidentally matches nothing cannot report the real tree as clean.
#
# Run as: cmake -DCORE_DIR=<core> -P check_dependencies.cmake

if(NOT DEFINED CORE_DIR)
    message(FATAL_ERROR "CORE_DIR must be defined")
endif()

function(hyremote_scan_core_dir _dir _report _out_violations _out_file_count)
    # Join the parts into one regex. Keeping this as a single expression avoids list separators becoming
    # literal pattern content; the synthetic self-check below detects a regression here.
    set(_forbidden_parts
        "QtWidgets|QtQuick|QtQml|QtGui|QtOpenGL|Qt3D|QWidget|QQuick|QQml|QApplication|QGuiApplication|"
        "QImage|QPixmap|QPainter|QWindow|QMouseEvent|QKeyEvent|QTouchEvent|QRhi[A-Za-z]*|QPlatform[A-Za-z]*|"
        "neatvnc|libvnc|rfb|"
        "rkmpp|rockchip|v4l2|va/va\\.h|"
        "libdrm|drm/|gbm|dma-buf|dma_buf|"
        "qpa|QPA|private/|"
        "EGL|egl\\.h|EGLFS|"
        "aml/|AML|"
        "rhi/|qrhi|RHI")
    string(REPLACE ";" "" _forbidden "${_forbidden_parts}")

    # Type declarations are checked separately so a forward declaration cannot bypass the include rule.
    # This list mirrors, family by family, what ADR-0001 names as outside Core ownership. QtCore remains
    # allowed internally, so only GUI/Quick/QML/QPA/platform/transport/hardware-shaped names appear here.
    set(_forbidden_declaration_parts
        "QWidget|QQuick[A-Za-z]*|QQml[A-Za-z]*|QApplication|QGuiApplication|QOpenGL[A-Za-z]*|"
        "QRhi[A-Za-z]*|QPlatform[A-Za-z]*|"
        "AML|aml_[A-Za-z_]*|EGLSurface|EGLDisplay|EGLConfig|EGLContext|EGLImage|EGLNativeDisplayType|EGLFS|"
        "nvnc_[A-Za-z_]*|rfb[A-Za-z_]*|"
        "gbm_[A-Za-z_]*|drm_[A-Za-z_]*|dma_buf[A-Za-z_]*|"
        "v4l2_[A-Za-z_]*|rkmpp_[A-Za-z_]*|VADisplay|VASurface|VAImage|"
        "GLuint|GLenum|GLint|GLsizei|GLboolean|GLfloat|GLdouble|GLchar|GLvoid")
    string(REPLACE ";" "" _forbidden_declarations "${_forbidden_declaration_parts}")

    file(GLOB_RECURSE _sources
        "${_dir}/include/*.hpp" "${_dir}/include/*.h"
        "${_dir}/src/*.hpp" "${_dir}/src/*.h" "${_dir}/src/*.cpp")

    set(_violations 0)
    foreach(_file IN LISTS _sources)
        file(READ "${_file}" _content)

        string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"]?[^>\"]*[>\"]?" _includes "${_content}")
        foreach(_include IN LISTS _includes)
            if(_include MATCHES "${_forbidden}")
                if(_report)
                    message(SEND_ERROR "forbidden include in ${_file}: ${_include}")
                endif()
                math(EXPR _violations "${_violations} + 1")
            endif()
        endforeach()

        # Scan declaration-shaped lines after stripping // comments. Documentation is allowed to name a
        # forbidden type; an actual declaration is not.
        string(REPLACE "\n" ";" _lines "${_content}")
        foreach(_line IN LISTS _lines)
            string(FIND "${_line}" "//" _comment_at)
            if(_comment_at GREATER -1)
                string(SUBSTRING "${_line}" 0 ${_comment_at} _line)
            endif()
            if(NOT _line MATCHES "(^|[ \t])(class|struct|namespace|enum|using)[ \t]+")
                continue()
            endif()
            if(_line MATCHES "(${_forbidden_declarations})")
                if(_report)
                    message(SEND_ERROR "forbidden type declaration in ${_file}: ${_line}")
                endif()
                math(EXPR _violations "${_violations} + 1")
            endif()
        endforeach()
    endforeach()

    list(LENGTH _sources _file_count)
    set(${_out_violations} ${_violations} PARENT_SCOPE)
    set(${_out_file_count} ${_file_count} PARENT_SCOPE)
endfunction()

if(NOT EXISTS "${CORE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "CORE_DIR does not look like a Core directory: ${CORE_DIR}")
endif()

hyremote_scan_core_dir("${CORE_DIR}" TRUE _violations _count)

file(READ "${CORE_DIR}/CMakeLists.txt" _core_cmake)
if(_core_cmake MATCHES "find_package[ \t]*\\([ \t]*Qt|Qt6::|Qt5::|Qt::")
    message(SEND_ERROR "hyremote-core must not link a Qt target")
    math(EXPR _violations "${_violations} + 1")
endif()

# Control experiment for the guard itself. The probe is created in the test working directory, never in
# the source tree, and intentionally contains fourteen violations across the ADR-0001 forbidden families
# plus a comment that must not count.
set(_probe ".dependency-guard-selfcheck")
file(REMOVE_RECURSE "${_probe}")
file(MAKE_DIRECTORY "${_probe}/include" "${_probe}/src")
file(WRITE "${_probe}/CMakeLists.txt" "add_library(probe STATIC src/probe.cpp)\n")
file(WRITE "${_probe}/include/forbidden_includes.hpp"
    "#pragma once\n#include <EGL/egl.h>\n#include <aml/aml.h>\n#include <rhi/qrhi.h>\n#include <QImage>\n")
file(WRITE "${_probe}/include/forward_declarations.hpp"
    "#pragma once\nclass QWidget;\nnamespace QQmlEngine { }\nclass QRhi;\nclass QPlatformWindow;\nstruct nvnc_display;\nstruct rfbScreenInfo;\nstruct gbm_bo;\nstruct v4l2_buffer;\nusing GLuint = unsigned int;\nusing VADisplay = void*;\n")
file(WRITE "${_probe}/include/legitimate.hpp"
    "#pragma once\n// QWidget, EGL and AML are forbidden in Core; this comment must not count.\nnamespace hyremote { void probe(); }\n")
hyremote_scan_core_dir("${_probe}" FALSE _selfcheck _selfcheck_files)
if(_selfcheck LESS 14)
    message(FATAL_ERROR
        "dependency guard self-check failed: expected at least 14 synthetic findings, got ${_selfcheck}")
endif()
message(STATUS
    "dependency guard self-check OK: ${_selfcheck} finding(s) in ${_selfcheck_files} synthetic file(s)")

if(_violations GREATER 0)
    message(FATAL_ERROR "hyremote-core dependency boundary violated (${_violations} finding(s))")
endif()

message(STATUS "hyremote-core dependency boundary OK: ${_count} source/header file(s) scanned")
