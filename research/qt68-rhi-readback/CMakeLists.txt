cmake_minimum_required(VERSION 3.21)
project(HyRemoteQt68RhiReadbackResearch LANGUAGES CXX)

# Independent research target: never pulled into HyRemote product builds.
find_package(Qt6 6.8.3 REQUIRED COMPONENTS Gui Quick)
if(NOT TARGET Qt6::GuiPrivate)
    find_package(Qt6GuiPrivate CONFIG QUIET)
endif()
if(NOT TARGET Qt6::GuiPrivate)
    message(FATAL_ERROR "Qt6::GuiPrivate is required for this isolated research probe")
endif()

add_executable(hyremote-rhi-readback-probe main.cpp)
target_compile_features(hyremote-rhi-readback-probe PRIVATE cxx_std_17)
target_link_libraries(hyremote-rhi-readback-probe PRIVATE
    Qt6::Gui Qt6::Quick Qt6::GuiPrivate)
