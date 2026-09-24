set(CMAKE_AUTOMOC ON)

qt_add_executable(hyremote-qml-basic
    main.cpp
    ../hyremote_branding.cpp
    ../../logo/hyremote-branding.qrc
)

# The module this application loads is its own, and it loads it by name: main.cpp calls
# loadFromModule("HyRemoteExample", "Main"). That lookup goes through the QML module system, so the module has to be
# laid out the way a by-name lookup expects. QTP0001 is the policy that decides the resource prefix: unset, it keeps
# the legacy `:/` prefix, the module is built under `:/HyRemoteExample/` and a by-name type lookup fails with
# `Module "HyRemoteExample" contains no type named "Main"` even though both the qmldir and Main.qml are present. The
# policy is set to NEW here, exactly as the installed-consumer test and the HyRemote module itself already do.
qt_policy(SET QTP0001 NEW)

qt_add_qml_module(hyremote-qml-basic
    URI HyRemoteExample
    VERSION 1.0
    QML_FILES Main.qml
)

target_compile_features(hyremote-qml-basic PRIVATE cxx_std_17)
target_link_libraries(hyremote-qml-basic PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
add_dependencies(hyremote-qml-basic hyremote-qml hyremote-qmlplugin)

target_compile_definitions(hyremote-qml-basic PRIVATE
    HYREMOTE_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)

# Same treatment as the other examples: into the shared prefix, deployed by the shared helper, with the QML route
# selected so the module payload it imports is deployed with it.
install(TARGETS hyremote-qml-basic RUNTIME DESTINATION bin)
hyremote_deploy(TARGET hyremote-qml-basic QML)
