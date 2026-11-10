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
