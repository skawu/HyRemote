cmake_minimum_required(VERSION 3.18)
project(hyremote_qt_gl_async_retire_r1 LANGUAGES CXX)

# Standalone, genuine Qt Quick rendering/GL sync test. It intentionally
# cannot include HyRemote Runtime or QtPrivate. All production experiments
# and default product flags are unaffected.
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Quick)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Quick)

add_executable(hyremote-qt-gl-async-retire-r1 main.cpp)
target_compile_features(hyremote-qt-gl-async-retire-r1 PRIVATE cxx_std_17)
target_link_libraries(hyremote-qt-gl-async-retire-r1 PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Quick
)

if(MSVC)
    target_compile_options(hyremote-qt-gl-async-retire-r1 PRIVATE /W4)
else()
    target_compile_options(hyremote-qt-gl-async-retire-r1 PRIVATE
        -Wall -Wextra -Werror)
endif()

message(STATUS "R953_QUICK_GL_READER=NON_PRODUCT Qt${QT_VERSION_MAJOR} ${Qt${QT_VERSION_MAJOR}Core_VERSION}")
