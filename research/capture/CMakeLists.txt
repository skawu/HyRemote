cmake_minimum_required(VERSION 3.21)

project(
    HyRemoteCaptureSpike
    VERSION 0.0.0
    DESCRIPTION "SPIKE-01 throwaway capture validation harness (issue #3, non-production)"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# SPIKE-01 capture harness.
#
# This project is deliberately independent from the HyRemote core build graph:
# it defines no public HyRemote API, links no HyRemote target, and contains no
# transport code. It exists only to produce the evidence recorded in
# docs/internal/capture-spike.md and is expected to be deleted once the capture
# architecture decision is implemented for real.
#
# It is also not part of the default root build. Enable it explicitly with
# -DHYREMOTE_BUILD_SPIKES=ON, or configure this directory directly.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Quick QuickWidgets OpenGLWidgets OpenGL)
find_package(Qt6 6.8 QUIET COMPONENTS Quick3D)

option(HYREMOTE_SPIKE_ENABLE_QUICK3D
       "Build the Quick3D capture case when QtQuick3D is installed" ON)

qt_standard_project_setup(REQUIRES 6.8)

set(HYREMOTE_SPIKE_HAVE_QUICK3D OFF)
if(HYREMOTE_SPIKE_ENABLE_QUICK3D AND Qt6Quick3D_FOUND)
    set(HYREMOTE_SPIKE_HAVE_QUICK3D ON)
endif()

set(HYREMOTE_SPIKE_SOURCES
    main.cpp
    harness/damage_tracker.cpp
    harness/damage_tracker.h
    harness/frame_sink.cpp
    harness/frame_sink.h
    harness/process_metrics.cpp
    harness/process_metrics.h
    harness/sample_runner.cpp
    harness/sample_runner.h
    harness/spike_types.h
    samples/custom_fbo_sample.cpp
    samples/custom_fbo_sample.h
    samples/opengl_widget_sample.cpp
    samples/opengl_widget_sample.h
    samples/quick_sample.cpp
    samples/quick_sample.h
    samples/quick_widget_sample.cpp
    samples/quick_widget_sample.h
    samples/widgets_sample.cpp
    samples/widgets_sample.h
)

set(HYREMOTE_SPIKE_QML_FILES
    qml/QuickScene.qml
    qml/CustomFboScene.qml
)

if(HYREMOTE_SPIKE_HAVE_QUICK3D)
    list(APPEND HYREMOTE_SPIKE_SOURCES
        samples/quick3d_sample.cpp
        samples/quick3d_sample.h
    )
    list(APPEND HYREMOTE_SPIKE_QML_FILES qml/Quick3DScene.qml)
endif()

# Flatten the QML resource paths so the scenes are addressable as
# qrc:/qt/qml/HyRemoteCaptureSpike/<Scene>.qml.
foreach(qml_file IN LISTS HYREMOTE_SPIKE_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()

qt_add_executable(hyremote-capture-spike ${HYREMOTE_SPIKE_SOURCES})

# ${CMAKE_CURRENT_SOURCE_DIR}/samples is required because the generated
# qmltyperegistrations.cpp locates QML_ELEMENT headers as <basename>.
target_include_directories(hyremote-capture-spike PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/samples"
)

target_link_libraries(hyremote-capture-spike PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
)

# The QML scenes are embedded in the binary, so the harness does not depend on
# the working directory. CustomFboItem is registered here as a QML element.
qt_add_qml_module(hyremote-capture-spike
    URI HyRemoteCaptureSpike
    VERSION 1.0
    QML_FILES ${HYREMOTE_SPIKE_QML_FILES}
    SOURCES
        samples/custom_fbo_item.cpp
        samples/custom_fbo_item.h
)

if(HYREMOTE_SPIKE_HAVE_QUICK3D)
    target_link_libraries(hyremote-capture-spike PRIVATE Qt6::Quick3D)
    target_compile_definitions(hyremote-capture-spike PRIVATE HYREMOTE_SPIKE_HAVE_QUICK3D=1)
endif()

# The harness reports through stdout, so it must remain a console application
# even though it links Qt::Gui.
set_target_properties(hyremote-capture-spike PROPERTIES WIN32_EXECUTABLE FALSE)

if(WIN32)
    # GetProcessMemoryInfo
    target_link_libraries(hyremote-capture-spike PRIVATE psapi)
endif()

if(MSVC)
    target_compile_options(hyremote-capture-spike PRIVATE /W4 /permissive-)
else()
    target_compile_options(hyremote-capture-spike PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Smoke tests.
#
# These are intentionally short. They only check that every capture case can
# produce at least one frame; the measurements that matter come from the longer
# documented runs that produce the evidence file.
# ---------------------------------------------------------------------------

include(CTest)

if(BUILD_TESTING)
    add_test(NAME capture-spike-list
             COMMAND hyremote-capture-spike --list)

    set(HYREMOTE_SPIKE_SMOKE_CASES widgets quick openglwidget quickwidget customfbo)
    if(HYREMOTE_SPIKE_HAVE_QUICK3D)
        list(APPEND HYREMOTE_SPIKE_SMOKE_CASES quick3d)
    endif()

    foreach(case_name IN LISTS HYREMOTE_SPIKE_SMOKE_CASES)
        add_test(NAME capture-spike-${case_name}
                 COMMAND hyremote-capture-spike
                         --sample ${case_name}
                         --frames 6
                         --warmup 3
                         --interval-ms 16
                         --no-sink)
        set_tests_properties(capture-spike-${case_name} PROPERTIES TIMEOUT 120)
    endforeach()
endif()
