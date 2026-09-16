# hyremote-core: UI-neutral Session, RemoteFrame and bounded dispatch pipeline.
#
# The frozen semantics live in docs/adr/0001-core-boundaries.md,
# docs/adr/0002-remoteframe-lifetime-timestamps.md and
# docs/adr/0003-threading-backpressure.md; docs/core-architecture.md connects them.
#
# Dependency rule (ADR-0001): this target must not link or include Qt Widgets, Qt Quick,
# Qt QML, Qt private/QPA, NeatVNC/AML, DRM/GBM/DMA-BUF or RKMPP/V4L2/VA-API types.
# core/tests/check_dependencies.cmake enforces the include/link part of that rule.

# Core is an internal/static composition layer for the V1 product. Applications link the shared
# HyRemote::RemoteAccess facade; QPA is delivered as a platform MODULE. Core deliberately is not
# installed/exported as a second normal SDK product target in V1.
add_library(hyremote-core STATIC
    src/capabilities.cpp
    src/detail/mailbox.cpp
    src/frame.cpp
    src/input.cpp
    src/session.cpp
    src/storage.cpp
    src/types.cpp
)
add_library(HyRemote::Core ALIAS hyremote-core)
set_target_properties(hyremote-core PROPERTIES
    # RemoteAccess and other product modules are shared, so the internal static archive must remain
    # PIC-capable on ELF platforms.
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(hyremote-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(hyremote-core PUBLIC cxx_std_17)

# std::thread / std::condition_variable only; no third-party dependency.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(hyremote-core PUBLIC Threads::Threads)

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
