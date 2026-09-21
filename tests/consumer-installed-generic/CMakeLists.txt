cmake_minimum_required(VERSION 3.21)
project(HyRemoteInstalledGenericConsumer LANGUAGES CXX)

# A Generic consumer is an ordinary Qt application. It acquires HyRemote only through the installed package (or,
# under source acquisition, through the same helper in the same build), it deliberately links no HyRemote target,
# and the zero-code integration arrives at run time as a Qt generic plugin.
find_package(HyRemote CONFIG REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Quick)

add_executable(generic-widgets-consumer generic_widgets_app.cpp)
target_compile_features(generic-widgets-consumer PRIVATE cxx_std_17)
target_link_libraries(generic-widgets-consumer PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

add_executable(generic-quick-consumer generic_quick_app.cpp)
target_compile_features(generic-quick-consumer PRIVATE cxx_std_17)
target_link_libraries(generic-quick-consumer PRIVATE Qt6::Core Qt6::Gui Qt6::Quick)

# No HyRemote link interface and no manual dependency: pulling either in here would create exactly the
# source/build-tree coupling these consumers exist to disprove.
install(TARGETS generic-widgets-consumer generic-quick-consumer RUNTIME DESTINATION bin)

# One deployment family, one call per consumer: Generic payload, shared runtime and private runtime closure,
# with the native platform plugin deliberately untouched.
hyremote_deploy(TARGET generic-widgets-consumer GENERIC)
hyremote_deploy(TARGET generic-quick-consumer GENERIC)
