cmake_minimum_required(VERSION 3.21)
project(HyRemoteQt68OpenGlPboAsyncR3 LANGUAGES CXX)
# Qt6.8.3 OpenGL RHI, PUBLIC Qt Gui+Quick only, no QtGuiPrivate.
# The PBO/fence APIs are OpenGL-only and never exposed to core/transport.
find_package(Qt6 6.8.3 REQUIRED COMPONENTS Core Gui Quick)
add_executable(hyremote-qt68-gl-pbo-async-r3 main.cpp)
target_compile_features(hyremote-qt68-gl-pbo-async-r3 PRIVATE cxx_std_17)
target_link_libraries(hyremote-qt68-gl-pbo-async-r3 PRIVATE Qt6::Core Qt6::Gui Qt6::Quick)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hyremote-qt68-gl-pbo-async-r3 PRIVATE -Wall -Wextra -Werror)
endif()
