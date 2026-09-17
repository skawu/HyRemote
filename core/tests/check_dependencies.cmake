# Enforces the hyremote-core dependency rule (docs/adr/0001-core-boundaries.md):
#
#   no Qt Widgets/Quick/QML, Qt private/QPA, OpenGL/RHI/EGLFS, NeatVNC/AML, DRM/GBM/DMA-BUF or
#   RKMPP/V4L2/VA-API include or type declaration may appear in Core, and the Core target may not link
#   a Qt target.
#
# The scan covers includes *and* type declarations: a forward declaration smuggles the same type past an
# include-only rule, which the 2026-09 review proved for `class QWidget;`. A self-check at the end points
# the same scanner at synthetic sources, so a scanner that regressed to matching nothing fails here.
#
# Run as: cmake -DCORE_DIR=<core> -P check_dependencies.cmake

if(NOT DEFINED CORE_DIR)
    message(FATAL_ERROR "CORE_DIR must be defined")
endif()

# Scans one Core directory tree and reports how many boundary violations it contains. Kept as a function
# so the self-check below can point the identical scanner at synthetic sources. `_report` controls
# whether findings are emitted as CMake errors: the real scan must fail the build, while the self-check
# only counts them (a SEND_ERROR there would fail this very test for the violations it created on
# purpose).
function(hyremote_scan_core_dir _dir _report _out_violations _out_file_count)
    # The parts are joined into ONE regex: a CMake list would put its separators into the pattern, so
    # only some alternatives could ever match. (The self-check below fails if this regresses.)
    set(_forbiddenParts
        "QtWidgets|QtQuick|QtQml|QtGui|QtOpenGL|Qt3D|QWidget|QQuick|QQml|QApplication|QGuiApplication|"
        "neatvnc|libvnc|rfb|"
        "rkmpp|rockchip|v4l2|va/va\\.h|"
        "libdrm|drm/|gbm|dma-buf|dma_buf|"
        "qpa|QPA|private/|"
        "EGL|egl\\.h|EGLFS|"
        "aml/|AML|"
        "rhi/|qrhi|RHI")
    string(REPLACE ";" "" _forbidden "${_forbiddenParts}")

    # Declarations of the same families. An include-only rule misses `class QWidget;`, so the names are
    # checked where declarations appear as well.
    set(_forbiddenDeclarationParts
        "QWidget|QQuick[A-Za-z]*|QQml[A-Za-z]*|QApplication|QGuiApplication|QOpenGL[A-Za-z]*|"
        "AML|aml_[A-Za-z_]*|EGLSurface|EGLDisplay|EGLConfig")
    string(REPLACE ";" "" _forbidden_declarations "${_forbiddenDeclarationParts}")

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

        # Declaration scan, line by line. A line comment is stripped first, because this project
        # documents the rule in prose and naming a forbidden type in a comment is not a violation.
        string(REPLACE "\n" ";" _lines "${_content}")
        foreach(_line IN LISTS _lines)
            string(FIND "${_line}" "//" _commentAt)
            if(_commentAt GREATER -1)
                string(SUBSTRING "${_line}" 0 ${_commentAt} _line)
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

# Self-check: the same scanner must flag a synthetic tree that contains one violation per rule. This is
# the guard's own control experiment - without it, a scanner that silently matched nothing would report
# the real tree as clean. The probe directory is created below the working directory (the test's build
# directory), never inside the source tree.
set(_probe ".dependency-guard-selfcheck")
file(REMOVE_RECURSE "${_probe}")
file(MAKE_DIRECTORY "${_probe}/include" "${_probe}/src")
file(WRITE "${_probe}/CMakeLists.txt" "add_library(probe STATIC src/probe.cpp)\n")
file(WRITE "${_probe}/include/forbidden_includes.hpp"
    "#pragma once\n#include <EGL/egl.h>\n#include <aml/aml.h>\n#include <rhi/qrhi.h>\n")
file(WRITE "${_probe}/include/forward_declarations.hpp"
    "#pragma once\nclass QWidget;\nnamespace QQmlEngine { }\n")
file(WRITE "${_probe}/include/legitimate.hpp"
    "#pragma once\n// QWidget, EGL and AML are forbidden in Core; this comment must not count.\nnamespace hyremote { void probe(); }\n")
hyremote_scan_core_dir("${_probe}" FALSE _selfcheck _selfcheckFiles)
if(_selfcheck LESS 5)
    message(FATAL_ERROR "dependency guard self-check failed: expected at least 5 synthetic findings, got ${_selfcheck}")
endif()
message(STATUS "dependency guard self-check OK: ${_selfcheck} finding(s) in ${_selfcheckFiles} synthetic file(s)")

if(_violations GREATER 0)
    message(FATAL_ERROR "hyremote-core dependency boundary violated (${_violations} finding(s))")
endif()

message(STATUS "hyremote-core dependency boundary OK: ${_count} source/header file(s) scanned")
