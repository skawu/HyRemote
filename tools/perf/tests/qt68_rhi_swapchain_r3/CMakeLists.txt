cmake_minimum_required(VERSION 3.21)
project(HyRemoteR3Qt68SwapchainAsyncReadback LANGUAGES CXX)

# Hard research sandbox: never linked into HyRemote Runtime or a released kit.
find_package(Qt6 6.8.3 REQUIRED COMPONENTS Core Gui Quick)
if(NOT TARGET Qt6::GuiPrivate)
  message(FATAL_ERROR "Exact Qt6.8.3 private Gui Kit not installed: cannot run R3")
endif()
add_executable(hyremote-qt68-rhi-swapchain-readback-r3 main.cpp)
target_compile_features(hyremote-qt68-rhi-swapchain-readback-r3 PRIVATE cxx_std_17)
target_link_libraries(hyremote-qt68-rhi-swapchain-readback-r3 PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::GuiPrivate)
if(MSVC)
  target_compile_options(hyremote-qt68-rhi-swapchain-readback-r3 PRIVATE /W4 /permissive-)
endif()
