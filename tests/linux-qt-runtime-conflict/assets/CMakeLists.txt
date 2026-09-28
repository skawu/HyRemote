cmake_minimum_required(VERSION 3.21)
project(HyRemoteLinuxQtRuntimeConflictAssets LANGUAGES CXX)

# Synthetic ELF assets for the Linux Qt runtime conflict regression (#382).
#
# Every payload below is a real shared library with a real SONAME, because the subject under test is the
# dynamic-loader dependency resolution CMake performs during deployment. A fixture of text files could not
# reproduce it, and relying on a distribution Qt happening to be installed would make the mechanism
# untestable wherever it is not - which is every hosted Linux job and every machine whose Qt layout differs.
#
# How the conflict is actually produced matters. `file(GET_RUNTIME_DEPENDENCIES)` resolves each dependency from
# the *depending file* in a fixed order (RUNPATH, then ldconfig, then DIRECTORIES) and stops at the first hit, so
# a conflict appears when two different depending files resolve the same name to two different paths - which is
# what happens on a host where the deployed runtime and a distribution Qt are both reachable. Each consumer and
# each plugin below therefore carries exactly ONE explicit RUNPATH, and a case is a choice of two of them.
#
# The Qt and probe payloads deliberately use a normal versioned shared-library layout. In particular, the Qt
# fixture is libQt6Core.so.6 -> libQt6Core.so.6.8.3 rather than a regular file named .so.6, because preserving
# that SONAME alias is part of the deployment contract under test.
#
#   qt-a/lib/libQt6Core.so.6              the selected consumer Qt runtime root's payload
#   qt-a/lib/sub/libQt6Core.so.6          a second candidate inside that same selected root
#   qt-b/lib/libQt6Core.so.6              a same-SONAME foreign payload
#   qt-c/lib/libQt6Core.so.6              a second same-SONAME foreign payload
#   qt-b|qt-c/lib/libhyremote-probe.so.1  a same-SONAME conflict on a library that is not the Qt runtime
#   qt-empty/lib/libplaceholder.so        a selected root that does not carry the Qt runtime at all
#   consumer-*/libhyremote-qtconflict-consumer.so  deployed-runtime-side depending files
#   plugin-*/libqconflictplugin*.so                platform-plugin-side depending files

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

# Keep every fixture artifact at the exact path the regression names. Multi-config generators otherwise append
# Debug/Release below LIBRARY_OUTPUT_DIRECTORY, which makes the fixture fail before the deployment decision runs.
function(_hyremote_conflict_set_output target directory)
    set_target_properties(${target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${directory}")
    foreach(_config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${_config}" _config_upper)
        set_target_properties(${target} PROPERTIES
            "LIBRARY_OUTPUT_DIRECTORY_${_config_upper}" "${directory}")
    endforeach()
endfunction()

# A payload carries no search path of its own: which root a depending file reaches is stated by that
# depending file, one case at a time, and nothing else may leak in and make a case pass for the wrong reason.
function(_hyremote_conflict_shared_library target source directory output_name soversion version)
    add_library(${target} SHARED "${source}")
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${output_name}"
        PREFIX "lib"
        SKIP_BUILD_RPATH TRUE
        BUILD_RPATH ""
    )
    if(NOT "${soversion}" STREQUAL "")
        set_target_properties(${target} PROPERTIES SOVERSION "${soversion}" VERSION "${version}")
    endif()
    _hyremote_conflict_set_output(${target} "${directory}")
endfunction()

_hyremote_conflict_shared_library(payload-a payload_a.cpp "${QT_A}" Qt6Core 6 6.8.3)
_hyremote_conflict_shared_library(payload-a-sub payload_a.cpp "${QT_A_SUB}" Qt6Core 6 6.8.3)
_hyremote_conflict_shared_library(payload-b payload_b.cpp "${QT_B}" Qt6Core 6 6.8.3)
_hyremote_conflict_shared_library(payload-c payload_c.cpp "${QT_C}" Qt6Core 6 6.8.3)
_hyremote_conflict_shared_library(probe-b probe.cpp "${QT_B}" hyremote-probe 1 1.0.0)
_hyremote_conflict_shared_library(probe-c probe.cpp "${QT_C}" hyremote-probe 1 1.0.0)
_hyremote_conflict_shared_library(placeholder placeholder.cpp "${ASSET_DIR}/qt-empty/lib" placeholder "" "")

# The depending files link their payload through an imported target, so the linker is handed the payload file
# itself. Naming the payload by a search directory instead - a link directory, or a full path that CMake turns
# into a -L/-l pair - would also put that directory into the depending file's own RUNPATH, and the depending
# file would then reach two roots at once. That leaks a case's second root into its first one's resolution and
# makes the case depend on RUNPATH ordering, which is precisely the environment coupling a regression must not
# have: on one host the selected root wins by luck, on the next the foreign one does.
function(_hyremote_conflict_import_payload name file)
    if(NOT TARGET ${name})
        add_library(${name} SHARED IMPORTED GLOBAL)
        set_target_properties(${name} PROPERTIES IMPORTED_LOCATION "${file}")
    endif()
endfunction()

_hyremote_conflict_import_payload(hyr-payload-b "${QT_B}/libQt6Core.so.6")
_hyremote_conflict_import_payload(hyr-probe-b "${QT_B}/libhyremote-probe.so.1")
_hyremote_conflict_import_payload(hyr-probe-c "${QT_C}/libhyremote-probe.so.1")

# Deployed-runtime-side depending files. The consumer is what the deployment bootstraps the private Qt closure
# from, so it must really NEED the Qt SONAME.
#
# The name is deliberately not the shipped runtime's name: these are the build tree's own artifacts, and the
# canonical build authority puts every directory in the build tree that holds a shared runtime on the loader
# path so the tests under test are the ones that were built. A fixture that borrowed that name would be loaded
# by unrelated tests instead of the product, which is a failure of the fixture rather than a finding about it.
function(_hyremote_conflict_consumer target runpaths source)
    add_library(${target} SHARED "${source}")
    target_link_libraries(${target} PRIVATE hyr-payload-b)
    add_dependencies(${target} payload-b)
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "hyremote-qtconflict-consumer"
        PREFIX "lib"
        SUFFIX ".so"
        BUILD_RPATH "${runpaths}"
        SKIP_BUILD_RPATH FALSE
    )
    _hyremote_conflict_set_output(${target} "${ASSET_DIR}/${target}")
endfunction()

# Platform-plugin-side depending files. These are the second opinion the loader forms about the same SONAME, and
# the reason a real deployment sees an ambiguity at all.
function(_hyremote_conflict_plugin target runpaths suffix)
    add_library(${target} SHARED consumer_qt.cpp)
    target_link_libraries(${target} PRIVATE hyr-payload-b)
    add_dependencies(${target} payload-b)
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "qconflictplugin${suffix}"
        PREFIX "lib"
        SUFFIX ".so"
        BUILD_RPATH "${runpaths}"
        SKIP_BUILD_RPATH FALSE
    )
    _hyremote_conflict_set_output(${target} "${ASSET_DIR}/${target}")
endfunction()

# `echo` resolves whatever sits beside the deployed runtime, which is how an already staged copy is reached.
_hyremote_conflict_consumer(consumer-echo "$ORIGIN" consumer_qt.cpp)
_hyremote_conflict_consumer(consumer-into-a "${QT_A}" consumer_qt.cpp)
_hyremote_conflict_consumer(consumer-into-b "${QT_B}" consumer_qt.cpp)

# A consumer that additionally NEEDs a non-Qt library present in two foreign roots: the Qt runtime resolves
# from the selected root, and the non-Qt name is what stays ambiguous.
add_library(consumer-probe-into-a SHARED consumer_qt_probe.cpp)
target_link_libraries(consumer-probe-into-a PRIVATE hyr-payload-b hyr-probe-b)
add_dependencies(consumer-probe-into-a payload-b probe-b)
set_target_properties(consumer-probe-into-a PROPERTIES
    OUTPUT_NAME "hyremote-qtconflict-consumer"
    PREFIX "lib"
    SUFFIX ".so"
    BUILD_RPATH "${QT_A};${QT_B}"
    SKIP_BUILD_RPATH FALSE
)
_hyremote_conflict_set_output(consumer-probe-into-a "${ASSET_DIR}/consumer-probe-into-a")

_hyremote_conflict_plugin(plugin-into-b "${QT_B}" "-b")
_hyremote_conflict_plugin(plugin-into-c "${QT_C}" "-c")
_hyremote_conflict_plugin(plugin-into-a-sub "${QT_A_SUB}" "-a-sub")

add_library(plugin-probe-into-a SHARED consumer_qt_probe.cpp)
target_link_libraries(plugin-probe-into-a PRIVATE hyr-payload-b hyr-probe-c)
add_dependencies(plugin-probe-into-a payload-b probe-c)
set_target_properties(plugin-probe-into-a PROPERTIES
    OUTPUT_NAME "qconflictplugin-probe-a-c"
    PREFIX "lib"
    SUFFIX ".so"
    BUILD_RPATH "${QT_A};${QT_C}"
    SKIP_BUILD_RPATH FALSE
)
_hyremote_conflict_set_output(plugin-probe-into-a "${ASSET_DIR}/plugin-probe-into-a")
