cmake_minimum_required(VERSION 3.25)
project(HyRemoteThirdPartySQLiteBrowserQpa LANGUAGES CXX)

if(NOT HYREMOTE_THIRD_PARTY_SOURCE OR NOT IS_DIRECTORY "${HYREMOTE_THIRD_PARTY_SOURCE}")
    message(FATAL_ERROR "HYREMOTE_THIRD_PARTY_SOURCE must point to a disposable SQLiteBrowser source tree")
endif()

# The wrapper owns HyRemote integration; the upstream source remains pristine.
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Network Widgets)
find_package(HyRemote CONFIG REQUIRED)

set(QT_MAJOR Qt6 CACHE STRING "SQLiteBrowser Qt major" FORCE)
set(BUILD_STABLE_VERSION ON CACHE BOOL "Deterministic upstream version string" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "Third-party unit tests are outside this product-fit lane" FORCE)
set(FORCE_INTERNAL_QSCINTILLA ON CACHE BOOL "Use upstream-vendored QScintilla" FORCE)
set(FORCE_INTERNAL_QCUSTOMPLOT ON CACHE BOOL "Use upstream-vendored QCustomPlot" FORCE)
set(FORCE_INTERNAL_QHEXEDIT ON CACHE BOOL "Use upstream-vendored QHexEdit" FORCE)
set(sqlcipher OFF CACHE BOOL "Use ordinary SQLite" FORCE)

add_subdirectory("${HYREMOTE_THIRD_PARTY_SOURCE}" upstream)

if(NOT TARGET sqlitebrowser)
    message(FATAL_ERROR "Expected pristine upstream target 'sqlitebrowser'")
endif()

# SQLiteBrowser's Linux install is distribution-oriented and leaves the executable
# with an empty install RPATH. This harness intentionally turns the installed test
# tree into a relocatable/private Qt bundle, so the application binary must be able
# to find Qt libraries deployed beside that bundle. This is packaging glue owned by
# the disposable test wrapper, not a HyRemote product/API behavior and not an edit
# to the pristine upstream source tree.
if(UNIX AND NOT APPLE)
    set_property(TARGET sqlitebrowser PROPERTY INSTALL_RPATH "$ORIGIN/../lib")
endif()

# This is deliberately the only HyRemote-specific operation. The application
# remains Qt-only and is not linked to a HyRemote C++ target.
hyremote_deploy(TARGET sqlitebrowser QPA)
