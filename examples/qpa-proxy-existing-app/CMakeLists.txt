cmake_minimum_required(VERSION 3.21)

project(HyRemoteQpaExistingAppExample LANGUAGES CXX)

include(GNUInstallDirs)

# E4 keeps the exact Qt 6.8.3 qualification boundary. The Transparent QPA proxy is a platform plugin shipped against
# one Qt line, so the constraint is stated here rather than widened by inference.
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)

add_executable(hyremote-qpa-proxy-existing-app
    main.cpp
    ../hyremote_branding.cpp
    ../../logo/hyremote-branding.qrc
)

target_compile_features(hyremote-qpa-proxy-existing-app PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-proxy-existing-app PRIVATE Qt6::Widgets)

# The application is installed unconditionally, and the target above stays Qt-only: it links no HyRemote target and
# includes no HyRemote header, in a repository build and in an installed SDK alike. HyRemote is consulted below for
# package-owned deployment metadata only.
install(TARGETS hyremote-qpa-proxy-existing-app
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION .
)

# Whether this SDK can deploy the Transparent QPA route, decided by what is actually present: the repository build
# with that frontend selected, or an installed SDK whose metadata says it ships the payload. This is the same
# availability detection the zero-code Generic examples use.
set(_qpa_deploy_available FALSE)
if(TARGET hyremote-qpa-platform)
    set(_qpa_deploy_available TRUE)
elseif(DEFINED HyRemote_QPA_AVAILABLE)
    set(_qpa_deploy_available "${HyRemote_QPA_AVAILABLE}")
endif()

# The switch follows the selected frontend instead of opposing it. It used to default OFF and to gate the install as
# well, which is how a QPA-enabled build produced a tree with no runnable consumer of the frontend it had selected -
# the packaging could disagree with the frontend. It now only allows an explicit opt-out of an otherwise default-on
# route, and the install above no longer depends on it.
option(HYREMOTE_EXAMPLE_DEPLOY_QPA
    "Install/deploy this ordinary Qt application with the HyRemote Transparent QPA package"
    ON)

if(_qpa_deploy_available AND HYREMOTE_EXAMPLE_DEPLOY_QPA)
    if(NOT COMMAND hyremote_deploy)
        # Reached when this file is configured outside the repository build, where the helper has not been included.
        find_package(HyRemote CONFIG REQUIRED)
    endif()
    hyremote_deploy(TARGET hyremote-qpa-proxy-existing-app QPA)
else()
    # No QPA frontend here, or it was explicitly opted out: an ordinary Qt application, no error either way.
    message(STATUS
        "  hyremote-qpa-proxy-existing-app: building as a plain Qt application; Transparent QPA deployment is "
        "unavailable in this SDK (configure with HYREMOTE_WITH_QPA_PROXY=ON to deploy the -platform hyremote route)")
endif()
