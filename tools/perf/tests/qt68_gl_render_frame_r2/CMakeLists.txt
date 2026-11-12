cmake_minimum_required(VERSION 3.21)
project(HyRemoteQt68GlRhiFramebufferReadR2 LANGUAGES CXX)
# Fail-closed research of the Qt6.8 OpenGL RHI variant. Not a re-use
# authorization for Qt6 D3D11/Vulkan/Metal or a shipping FrameSource.
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Quick)
add_executable(hyremote-qt68-gl-rhi-render-frame-r2 main.cpp)
target_compile_features(hyremote-qt68-gl-rhi-render-frame-r2 PRIVATE cxx_std_17)
target_link_libraries(hyremote-qt68-gl-rhi-render-frame-r2 PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick)
option(HYREMOTE_GL_RHI_CAPTURE_AFTER_FRAME_END
  "TEST ONLY: read GL framebuffer at Qt6 afterFrameEnd rather than afterRendering" OFF)
if(HYREMOTE_GL_RHI_CAPTURE_AFTER_FRAME_END)
  target_compile_definitions(hyremote-qt68-gl-rhi-render-frame-r2
    PRIVATE HYREMOTE_GL_RHI_CAPTURE_AFTER_FRAME_END=1)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hyremote-qt68-gl-rhi-render-frame-r2 PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(hyremote-qt68-gl-rhi-render-frame-r2 PRIVATE /W4 /permissive-)
endif()
