cmake_minimum_required(VERSION 3.16)
project(HyRemoteQt5GlExistingFrameTapR1 LANGUAGES CXX)

# This standalone probe is deliberately Qt5 OpenGL-only. Qt6 RHI/D3D11
# afterRendering() means commands RECORDED, not submitted. No QtPrivate.
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Quick)
add_executable(hyremote-qt5-gl-existing-frame-r1 main.cpp)
target_compile_features(hyremote-qt5-gl-existing-frame-r1 PRIVATE cxx_std_17)
target_link_libraries(hyremote-qt5-gl-existing-frame-r1 PRIVATE
    Qt5::Core Qt5::Gui Qt5::Quick)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hyremote-qt5-gl-existing-frame-r1 PRIVATE -Wall -Wextra -Werror)
endif()
