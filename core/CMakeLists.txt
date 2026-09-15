# hyremote-core: UI-neutral Session, RemoteFrame and bounded dispatch pipeline.
#
# The frozen semantics live in docs/adr/0001-core-boundaries.md,
# docs/adr/0002-remoteframe-lifetime-timestamps.md and
# docs/adr/0003-threading-backpressure.md; docs/core-architecture.md connects them.
#
# Dependency rule (ADR-0001): this target must not link or include Qt Widgets, Qt Quick,
# Qt QML, Qt private/QPA, NeatVNC/AML, DRM/GBM/DMA-BUF or RKMPP/V4L2/VA-API types.
# core/tests/check_dependencies.cmake enforces the include/link part of that rule.

add_library(hyremote-core
    src/capabilities.cpp
    src/detail/mailbox.cpp
    src/frame.cpp
    src/session.cpp
    src/storage.cpp
    src/types.cpp
)
add_library(HyRemote::Core ALIAS hyremote-core)

target_include_directories(hyremote-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
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
