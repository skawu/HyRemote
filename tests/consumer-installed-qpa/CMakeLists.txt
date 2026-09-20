cmake_minimum_required(VERSION 3.21)

project(HyRemoteInstalledQpaConsumer LANGUAGES CXX)

include(GNUInstallDirs)

# This is one clean external Qt-only application fixture with two acquisition paths. By default it
# consumes the installed HyRemote SDK. V1 GA also configures the exact same 05 Widgets QPA source
# against a vendored/add_subdirectory HyRemote tree so source consumption cannot silently diverge
# from the installed deployment contract.
set(HYREMOTE_CONSUMER_SOURCE_DIR "" CACHE PATH
    "Optional HyRemote source tree; when set, exercise add_subdirectory() instead of find_package()")

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)
qt_standard_project_setup(REQUIRES 6.8)

if(HYREMOTE_CONSUMER_SOURCE_DIR)
    if(NOT EXISTS "${HYREMOTE_CONSUMER_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "HYREMOTE_CONSUMER_SOURCE_DIR is not a HyRemote source tree: ${HYREMOTE_CONSUMER_SOURCE_DIR}")
    endif()

    # QPA is an optional V1 source-consumption package and therefore must be explicitly selected
    # before adding HyRemote. Tests/examples remain at their product-only defaults.
    set(HYREMOTE_WITH_QPA_PROXY ON CACHE BOOL "" FORCE)
    add_subdirectory("${HYREMOTE_CONSUMER_SOURCE_DIR}" hyremote-source EXCLUDE_FROM_ALL)

    if(NOT COMMAND hyremote_deploy)
        message(FATAL_ERROR "HyRemote source consumption must provide hyremote_deploy()")
    endif()
    if(NOT TARGET HyRemote::QpaPlatform)
        message(FATAL_ERROR "source HyRemote QPA selection did not create its internal qhyremote target")
    endif()
else()
    # Installed QPA remains an ordinary Qt application at the source/link layer. HyRemote is found
    # only for package/deployment metadata; the executable links Qt Widgets only.
    find_package(HyRemote CONFIG REQUIRED)

    if(NOT HyRemote_QPA_AVAILABLE)
        message(FATAL_ERROR "installed HyRemote SDK does not contain the QPA package")
    endif()
    if(TARGET HyRemote::QpaPlatform)
        message(FATAL_ERROR
            "installed V1 SDK must not expose qhyremote as a consumer CMake link target")
    endif()
    if(NOT DEFINED HyRemote_QPA_PLUGIN_FILE OR "${HyRemote_QPA_PLUGIN_FILE}" STREQUAL "")
        message(FATAL_ERROR "installed HyRemote SDK did not publish its QPA payload location")
    endif()
    if(NOT IS_ABSOLUTE "${HyRemote_QPA_PLUGIN_FILE}" OR NOT EXISTS "${HyRemote_QPA_PLUGIN_FILE}")
        message(FATAL_ERROR
            "installed HyRemote QPA payload path is invalid: ${HyRemote_QPA_PLUGIN_FILE}")
    endif()
endif()

# Do not maintain a second toy application for package/source acceptance. Compile the exact
# `05-qpa-existing-app/widgets-app` source through this clean external project. The application
# source stays Qt-only; this fixture contributes only HyRemote acquisition/deployment and a bounded
# executable name used by the product-fit harness.
set(_hyremote_qpa_widgets_source
    "${CMAKE_CURRENT_LIST_DIR}/../../examples/learning/05-qpa-existing-app/widgets-app/main.cpp")
if(NOT EXISTS "${_hyremote_qpa_widgets_source}")
    message(FATAL_ERROR "05 QPA Widgets source is missing: ${_hyremote_qpa_widgets_source}")
endif()

add_executable(hyremote-installed-qpa-consumer
    "${_hyremote_qpa_widgets_source}"
)
target_compile_features(hyremote-installed-qpa-consumer PRIVATE cxx_std_17)
target_link_libraries(hyremote-installed-qpa-consumer PRIVATE Qt6::Widgets)

get_target_property(_consumer_links hyremote-installed-qpa-consumer LINK_LIBRARIES)
foreach(_consumer_link IN LISTS _consumer_links)
    if(_consumer_link MATCHES "^HyRemote::")
        message(FATAL_ERROR
            "QPA consumer must not link HyRemote application/runtime targets: ${_consumer_links}")
    endif()
endforeach()

install(TARGETS hyremote-installed-qpa-consumer
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION .
)

# The same public deployment entry point owns the proxy plugin/runtime placement for installed and
# source acquisition. The application source/link layer remains unaware of HyRemote in both cases.
hyremote_deploy(TARGET hyremote-installed-qpa-consumer QPA)
