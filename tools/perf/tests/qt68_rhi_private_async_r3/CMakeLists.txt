cmake_minimum_required(VERSION 3.21)
project(HyRemoteQt68RhiPrivateAsyncR3 LANGUAGES CXX)

# R3 is intentionally NOT a HyRemote product target. Its use of GuiPrivate
# exists solely to validate the actual Qt6.8.3 visible swapchain capability.
# No approved Runtime/SDK QtPrivate dependency change is implied.
find_package(Qt6 6.8.3 REQUIRED COMPONENTS Core Gui Quick)
# Qt's online SDK typically exports Qt6::GuiPrivate *as a target* when
# finding Gui, without providing a separate Qt6GuiPrivateConfig.cmake.
# Match the existing HyRemote Runtime private-input integration pattern.
if(NOT TARGET Qt6::GuiPrivate)
    message(FATAL_ERROR
        "Qt6::GuiPrivate target missing in exact Qt6.8.3 kit: R3 remains BLOCKED, not RHI unsupported")
endif()
add_executable(hyremote-qt68-private-rhi-async-r3 main.cpp)
target_compile_features(hyremote-qt68-private-rhi-async-r3 PRIVATE cxx_std_17)
target_link_libraries(hyremote-qt68-private-rhi-async-r3 PRIVATE
    Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Quick)
if(MSVC)
    target_compile_options(hyremote-qt68-private-rhi-async-r3 PRIVATE /W4 /permissive-)
endif()
