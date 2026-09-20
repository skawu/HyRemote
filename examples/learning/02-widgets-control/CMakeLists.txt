cmake_minimum_required(VERSION 3.21)

project(HyRemoteWidgetsControl LANGUAGES CXX)

add_executable(hyremote-example-widgets-control
    main.cpp
)

include("${CMAKE_CURRENT_LIST_DIR}/../../common/HyRemoteExampleBranding.cmake")
hyremote_example_enable_branding(hyremote-example-widgets-control)

target_compile_features(hyremote-example-widgets-control PRIVATE cxx_std_17)

if(TARGET Qt6::Widgets)
    set(_hyremote_qt_widgets Qt6::Widgets)
elseif(TARGET Qt5::Widgets)
    set(_hyremote_qt_widgets Qt5::Widgets)
else()
    message(FATAL_ERROR "02-widgets-control requires Qt Widgets")
endif()

target_link_libraries(hyremote-example-widgets-control
    PRIVATE
        ${_hyremote_qt_widgets}
        HyRemote::RemoteAccess
)
