cmake_minimum_required(VERSION 3.21)
project(HyRemoteQt68PublicRhiEligibility LANGUAGES CXX)

# Only public Qt6 Quick/Gui API. Explicitly excludes Qt6::GuiPrivate and
# internal rhi/qrhi.h. This qualification does NOT implement an RHI readback.
find_package(Qt6 6.8.3 REQUIRED COMPONENTS Core Gui Quick)
add_executable(hyremote-qt68-public-rhi-eligibility main.cpp)
target_compile_features(hyremote-qt68-public-rhi-eligibility PRIVATE cxx_std_17)
target_link_libraries(hyremote-qt68-public-rhi-eligibility PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick)
if(MSVC)
    target_compile_options(hyremote-qt68-public-rhi-eligibility PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hyremote-qt68-public-rhi-eligibility PRIVATE -Wall -Wextra)
endif()

# Research only: *read* the existing QQuickWindow swapchain capabilities
# through the exact Qt6.8.3 GuiPrivate Kit, and NEVER issue GPU operations
# or modify swapchain flags. This does not add QtPrivate to HyRemote products.
option(HYREMOTE_RHI_PRIVATE_INSPECTION_ONLY "Research: inspect existing RHI swapchain flags (NON-PRODUCT)" OFF)
if(HYREMOTE_RHI_PRIVATE_INSPECTION_ONLY)
    # Qt's official Kit can export Qt6::GuiPrivate as an internal target of
    # Qt6GuiConfig; find_package COMPONENTS GuiPrivate is not necessarily a
    # separately published CMake package (as shown by hosted Qt 6.8.3).
    if(NOT TARGET Qt6::GuiPrivate)
        message(FATAL_ERROR "Exact Qt6.8.3 Kit exports no Qt6::GuiPrivate: inspection BLOCKED")
    endif()
    target_link_libraries(hyremote-qt68-public-rhi-eligibility PRIVATE Qt6::GuiPrivate)
    target_compile_definitions(hyremote-qt68-public-rhi-eligibility
        PRIVATE HYREMOTE_RHI_PRIVATE_INSPECTION_ONLY=1)
    message(WARNING "RESEARCH ONLY: EXACT Qt6.8.3 GuiPrivate inspection; do not ship")
endif()

