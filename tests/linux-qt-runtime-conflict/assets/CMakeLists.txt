cmake_minimum_required(VERSION 3.21)
project(HyRemoteLinuxQtRuntimeConflictAssets LANGUAGES CXX)

# Synthetic ELF assets for the Linux Qt runtime conflict regression (#382).
#
# Every payload below is a real shared library with a real SONAME, because the subject under test is the
# dynamic-loader dependency resolution CMake performs during deployment. A fixture of text files could not
# reproduce it, and relying on a distribution Qt happening to be installed would make the mechanism
# untestable wherever it is not - which is every hosted Linux job and every machine whose Qt layout differs.
#
# How the conflict is actually produced matters, so it is modelled the way the loader and CMake really work:
# `file(GET_RUNTIME_DEPENDENCIES)` resolves each dependency from the *depending file* in a fixed order
# (RUNPATH, then ldconfig, then DIRECTORIES) and stops at the first hit, so a conflict appears when two
# different depending files resolve the same name to two different paths - which is exactly what happens on a
# host where the deployed runtime and a distribution Qt are both reachable. Each consumer and each plugin
# below therefore carries one explicit RUNPATH, and a case is a choice of two of them.
#
#   qt-a/lib/libQt6Core.so.6              the selected consumer Qt runtime root's payload
#   qt-a/lib/sub/libQt6Core.so.6          a second candidate inside that same selected root
#   qt-b/lib/libQt6Core.so.6              a same-SONAME foreign payload
#   qt-c/lib/libQt6Core.so.6              a second same-SONAME foreign payload
#   qt-b|qt-c/lib/libhyremote-probe.so.1  a same-SONAME conflict on a library that is not the Qt runtime
#   qt-empty/lib/libplaceholder.so        a selected root that does not carry the Qt runtime at all
#   consumer-*/libhyremote-qtconflict-consumer.so deployed-runtime-side depending files
#   plugin-*/libqconflictplugin*.so       platform-plugin-side depending files

if(NOT ASSET_DIR)
    message(FATAL_ERROR "ASSET_DIR is required")
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(QT_A "${ASSET_DIR}/qt-a/lib")
set(QT_A_SUB "${ASSET_DIR}/qt-a/lib/sub")
set(QT_B "${ASSET_DIR}/qt-b/lib")
set(QT_C "${ASSET_DIR}/qt-c/lib")

# A payload carries no search path of its own: which root a depending file reaches is stated by that
# depending file, one case at a time, and nothing else may leak in and make a case pass for the wrong reason.
function(_hyremote_conflict_shared_library target source directory output_name suffix)
    add_library(${target} SHARED "${source}")
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${output_name}"
        PREFIX "lib"
        SUFFIX "${suffix}"
        LIBRARY_OUTPUT_DIRECTORY "${directory}"
        SKIP_BUILD_RPATH TRUE
        BUILD_RPATH ""
    )
endfunction()

_hyremote_conflict_shared_library(payload-a payload_a.cpp "${QT_A}" Qt6Core ".so.6")
_hyremote_conflict_shared_library(payload-a-sub payload_a.cpp "${QT_A_SUB}" Qt6Core ".so.6")
_hyremote_conflict_shared_library(payload-b payload_b.cpp "${QT_B}" Qt6Core ".so.6")
_hyremote_conflict_shared_library(payload-c payload_c.cpp "${QT_C}" Qt6Core ".so.6")
_hyremote_conflict_shared_library(probe-b probe.cpp "${QT_B}" hyremote-probe ".so.1")
_hyremote_conflict_shared_library(probe-c probe.cpp "${QT_C}" hyremote-probe ".so.1")
_hyremote_conflict_shared_library(placeholder placeholder.cpp "${ASSET_DIR}/qt-empty/lib" placeholder ".so")

function(_hyremote_conflict_link_dependencies target)
    # The Qt payload has no `libQt6Core.so` development symlink - a distribution Qt runtime does not have one
    # either - so the exact SONAME is named with the linker's `-l:` form rather than letting CMake turn a full
    # path into `-lQt6Core`, which no such installation can satisfy.
    target_link_directories(${target} PRIVATE "${QT_B}")
    target_link_libraries(${target} PRIVATE "-l:libQt6Core.so.6")
endfunction()

# Deployed-runtime-side depending files. The consumer is what the deployment bootstraps the private Qt
# closure from, so it must really NEED the Qt SONAME.
#
# The name is deliberately not the shipped runtime's name: these are the build tree's own artifacts, and the
# canonical build authority puts every directory in the build tree that holds a shared runtime on the loader
# path so the tests under test are the ones that were built. A fixture that borrowed that name would be
# loaded by unrelated tests instead of the product, which is a failure of the fixture rather than a finding
# about the product.
function(_hyremote_conflict_consumer target runpaths source)
    add_library(${target} SHARED "${source}")
    _hyremote_conflict_link_dependencies(${target})
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "hyremote-qtconflict-consumer"
        PREFIX "lib"
        SUFFIX ".so"
        LIBRARY_OUTPUT_DIRECTORY "${ASSET_DIR}/${target}"
        BUILD_RPATH "${runpaths}"
        SKIP_BUILD_RPATH FALSE
    )
endfunction()

# Platform-plugin-side depending files. These are the second opinion the loader forms about the same SONAME,
# and the reason a real deployment sees an ambiguity at all.
function(_hyremote_conflict_plugin target runpaths suffix)
    add_library(${target} SHARED consumer_qt.cpp)
    _hyremote_conflict_link_dependencies(${target})
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "qconflictplugin${suffix}"
        PREFIX "lib"
        SUFFIX ".so"
        LIBRARY_OUTPUT_DIRECTORY "${ASSET_DIR}/${target}"
        BUILD_RPATH "${runpaths}"
        SKIP_BUILD_RPATH FALSE
    )
endfunction()

# `echo` resolves whatever sits beside the deployed runtime, which is how an already staged copy is reached.
_hyremote_conflict_consumer(consumer-echo "$ORIGIN" consumer_qt.cpp)
_hyremote_conflict_consumer(consumer-into-a "${QT_A}" consumer_qt.cpp)
_hyremote_conflict_consumer(consumer-into-b "${QT_B}" consumer_qt.cpp)

# A consumer that additionally NEEDs a non-Qt library present in two foreign roots: the Qt runtime resolves
# from the selected root, and the non-Qt name is what stays ambiguous.
add_library(consumer-probe-into-a SHARED consumer_qt_probe.cpp)
_hyremote_conflict_link_dependencies(consumer-probe-into-a)
target_link_directories(consumer-probe-into-a PRIVATE "${QT_B}")
target_link_libraries(consumer-probe-into-a PRIVATE "-l:libhyremote-probe.so.1")
set_target_properties(consumer-probe-into-a PROPERTIES
    OUTPUT_NAME "hyremote-qtconflict-consumer"
    PREFIX "lib"
    SUFFIX ".so"
    LIBRARY_OUTPUT_DIRECTORY "${ASSET_DIR}/consumer-probe-into-a"
    BUILD_RPATH "${QT_A};${QT_B}"
    SKIP_BUILD_RPATH FALSE
)

_hyremote_conflict_plugin(plugin-into-b "${QT_B}" "-b")
_hyremote_conflict_plugin(plugin-into-c "${QT_C}" "-c")
_hyremote_conflict_plugin(plugin-into-a-sub "${QT_A_SUB}" "-a-sub")

add_library(plugin-probe-into-a SHARED consumer_qt_probe.cpp)
_hyremote_conflict_link_dependencies(plugin-probe-into-a)
target_link_directories(plugin-probe-into-a PRIVATE "${QT_C}")
target_link_libraries(plugin-probe-into-a PRIVATE "-l:libhyremote-probe.so.1")
set_target_properties(plugin-probe-into-a PROPERTIES
    OUTPUT_NAME "qconflictplugin-probe-a-c"
    PREFIX "lib"
    SUFFIX ".so"
    LIBRARY_OUTPUT_DIRECTORY "${ASSET_DIR}/plugin-probe-into-a"
    BUILD_RPATH "${QT_A};${QT_C}"
    SKIP_BUILD_RPATH FALSE
)
