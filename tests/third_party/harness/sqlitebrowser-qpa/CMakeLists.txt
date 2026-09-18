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

# This is deliberately the only HyRemote-specific operation. The application
# remains Qt-only and is not linked to a HyRemote C++ target.
hyremote_deploy(TARGET sqlitebrowser QPA)
