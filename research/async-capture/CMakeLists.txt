cmake_minimum_required(VERSION 3.21)

project(
    HyRemoteAsyncCaptureSpike
    VERSION 0.0.0
    DESCRIPTION "Public-async-first capture evaluation harness (issue #16, non-production)"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Asynchronous capture spike for issue #16.
#
# This project is deliberately independent from the HyRemote core build graph
# and from spikes/capture: it defines no public HyRemote API, links no HyRemote
# target, and contains no transport code. It exists only to produce the evidence
# recorded in docs/async-capture-spike.md.
#
# It is also not part of the default root build. Enable it explicitly with
# -DHYREMOTE_BUILD_SPIKES=ON, or configure this directory directly.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Quick QuickWidgets OpenGLWidgets OpenGL)
find_package(Qt6 6.8 QUIET COMPONENTS Quick3D)

option(HYREMOTE_ASYNC_SPIKE_ENABLE_QUICK3D
       "Build the Quick3D asynchronous capture case when QtQuick3D is installed" ON)

qt_standard_project_setup(REQUIRES 6.8)

set(HYREMOTE_ASYNC_HAVE_QUICK3D OFF)
if(HYREMOTE_ASYNC_SPIKE_ENABLE_QUICK3D AND Qt6Quick3D_FOUND)
    set(HYREMOTE_ASYNC_HAVE_QUICK3D ON)
endif()

set(HYREMOTE_ASYNC_SOURCES
    main.cpp
    harness/process_metrics.cpp
    harness/process_metrics.h
    harness/async_probe.cpp
    harness/async_probe.h
    scenes/scene_host.cpp
    scenes/scene_host.h
)

set(HYREMOTE_ASYNC_QML_FILES
    qml/AsyncScene.qml
    qml/CustomFboScene.qml
)

if(HYREMOTE_ASYNC_HAVE_QUICK3D)
    list(APPEND HYREMOTE_ASYNC_QML_FILES qml/Quick3DScene.qml)
endif()

# Flatten the QML resource paths so the scenes are addressable as
# qrc:/qt/qml/HyRemoteAsyncSpike/<Scene>.qml.
foreach(qml_file IN LISTS HYREMOTE_ASYNC_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()

qt_add_executable(hyremote-async-spike ${HYREMOTE_ASYNC_SOURCES})

# ${CMAKE_CURRENT_SOURCE_DIR}/scenes is required because the generated
# qmltyperegistrations.cpp locates QML_ELEMENT headers as <basename>.
target_include_directories(hyremote-async-spike PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scenes"
)

target_link_libraries(hyremote-async-spike PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
)

qt_add_qml_module(hyremote-async-spike
    URI HyRemoteAsyncSpike
    VERSION 1.0
    QML_FILES ${HYREMOTE_ASYNC_QML_FILES}
    SOURCES
        scenes/custom_fbo_item.cpp
        scenes/custom_fbo_item.h
)

if(HYREMOTE_ASYNC_HAVE_QUICK3D)
    target_link_libraries(hyremote-async-spike PRIVATE Qt6::Quick3D)
    target_compile_definitions(hyremote-async-spike PRIVATE HYREMOTE_ASYNC_HAVE_QUICK3D=1)
endif()

# The harness reports through stdout, so it must remain a console application
# even though it links Qt::Gui.
set_target_properties(hyremote-async-spike PROPERTIES WIN32_EXECUTABLE FALSE)

if(WIN32)
    # GetProcessMemoryInfo
    target_link_libraries(hyremote-async-spike PRIVATE psapi)
endif()

if(MSVC)
    target_compile_options(hyremote-async-spike PRIVATE /W4 /permissive-)
else()
    target_compile_options(hyremote-async-spike PRIVATE -Wall -Wextra)
endif()

include(CTest)

if(BUILD_TESTING)
    add_test(NAME async-spike-list COMMAND hyremote-async-spike --list)

    set(HYREMOTE_ASYNC_SMOKE_CASES quick2d quick3d customfbo quickwidget openglwidget)
    if(NOT HYREMOTE_ASYNC_HAVE_QUICK3D)
        list(REMOVE_ITEM HYREMOTE_ASYNC_SMOKE_CASES quick3d)
    endif()

    foreach(case_name IN LISTS HYREMOTE_ASYNC_SMOKE_CASES)
        add_test(NAME async-spike-${case_name}
                 COMMAND hyremote-async-spike
                         --scene ${case_name}
                         --mode composition
                         --requests 4)
        set_tests_properties(async-spike-${case_name} PROPERTIES TIMEOUT 180)
    endforeach()

    add_test(NAME async-spike-pipeline
             COMMAND hyremote-async-spike
                     --scene quick2d
                     --mode pipeline
                     --requests 20
                     --max-inflight 4)
    set_tests_properties(async-spike-pipeline PROPERTIES TIMEOUT 180)
endif()
