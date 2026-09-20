cmake_minimum_required(VERSION 3.21)

project(HyRemoteExampleWidgetsBasic LANGUAGES CXX)

add_executable(hyremote-example-widgets-basic
    main.cpp
)

include("${CMAKE_CURRENT_LIST_DIR}/../../common/HyRemoteExampleBranding.cmake")
hyremote_example_enable_branding(hyremote-example-widgets-basic)

target_compile_features(hyremote-example-widgets-basic PRIVATE cxx_std_17)

if(TARGET Qt6::Widgets)
    set(_hyremote_qt_widgets Qt6::Widgets)
elseif(TARGET Qt5::Widgets)
    set(_hyremote_qt_widgets Qt5::Widgets)
else()
    message(FATAL_ERROR "01-widgets-basic requires Qt Widgets")
endif()

target_link_libraries(hyremote-example-widgets-basic
    PRIVATE
        ${_hyremote_qt_widgets}
        HyRemote::RemoteAccess
)
