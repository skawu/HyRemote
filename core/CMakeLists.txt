# hyremote-core: UI-neutral Session, RemoteFrame and bounded dispatch pipeline.
#
# The frozen semantics live in docs/adr/0001-core-boundaries.md,
# docs/adr/0002-remoteframe-lifetime-timestamps.md and
# docs/adr/0003-threading-backpressure.md; docs/core-architecture.md connects them.
#
# Dependency rule (ADR-0001): this target must not link or include Qt Widgets, Qt Quick,
# Qt QML, Qt private/QPA, NeatVNC/AML, DRM/GBM/DMA-BUF or RKMPP/V4L2/VA-API types.
# core/tests/check_dependencies.cmake enforces the include/link part of that rule.

include(GNUInstallDirs)

add_library(hyremote-core
    src/capabilities.cpp
    src/detail/mailbox.cpp
    src/frame.cpp
    src/session.cpp
    src/storage.cpp
    src/types.cpp
)
add_library(HyRemote::Core ALIAS hyremote-core)
set_target_properties(hyremote-core PROPERTIES EXPORT_NAME Core)

target_include_directories(hyremote-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(hyremote-core PUBLIC cxx_std_17)

# std::thread / std::condition_variable only; no third-party dependency.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(hyremote-core PUBLIC Threads::Threads)

install(
    TARGETS hyremote-core
    EXPORT HyRemoteTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp"
)

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
