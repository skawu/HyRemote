cmake_minimum_required(VERSION 3.21)

project(HyRemoteQtLtsPreflight LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CheckCXXSourceCompiles)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Network Widgets Quick Qml)
set(QT_PACKAGE "Qt${QT_VERSION_MAJOR}")
find_package(${QT_PACKAGE} REQUIRED COMPONENTS Core Gui Network Widgets Quick Qml)

set(_qt_version_var "${QT_PACKAGE}_VERSION")
set(HYREMOTE_PREFLIGHT_QT_VERSION "${${_qt_version_var}}")
if(HYREMOTE_PREFLIGHT_QT_VERSION STREQUAL "")
    set(HYREMOTE_PREFLIGHT_QT_VERSION "unknown")
endif()

set(CMAKE_REQUIRED_QUIET TRUE)
set(CMAKE_REQUIRED_LIBRARIES
    ${QT_PACKAGE}::Core
    ${QT_PACKAGE}::Gui
    ${QT_PACKAGE}::Network
    ${QT_PACKAGE}::Widgets
    ${QT_PACKAGE}::Quick
    ${QT_PACKAGE}::Qml)

check_cxx_source_compiles([=[
#include <QCoreApplication>
#include <QHostAddress>
#include <QWidget>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickItemGrabResult>
#include <QtQuick/QQuickWindow>
#include <QSharedPointer>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QHostAddress address(QHostAddress::AnyIPv4);
    QWidget *widget = nullptr;
    QQuickWindow *window = nullptr;
    QQuickItem *item = window ? window->contentItem() : nullptr;
    QSharedPointer<QQuickItemGrabResult> grab = item ? item->grabToImage() : QSharedPointer<QQuickItemGrabResult>{};
    return address.isNull() || widget != nullptr || !grab.isNull();
}
]=] HYREMOTE_PREFLIGHT_PUBLIC_SURFACE_COMPILES)

check_cxx_source_compiles([=[
#if __has_include(<QtQmlIntegration/qqmlintegration.h>)
#  include <QtQmlIntegration/qqmlintegration.h>
#else
#  error "QtQmlIntegration/qqmlintegration.h unavailable"
#endif
#ifndef QML_NAMED_ELEMENT
#  error "QML_NAMED_ELEMENT unavailable"
#endif
int main() { return 0; }
]=] HYREMOTE_PREFLIGHT_CURRENT_QML_HEADER_COMPILES)

check_cxx_source_compiles([=[
#include <QObject>
#include <QtQml/qqml.h>
int main()
{
    volatile int typeId = qmlRegisterType<QObject>("HyRemotePreflight", 1, 0, "ProbeObject");
    return typeId < 0;
}
]=] HYREMOTE_PREFLIGHT_LEGACY_QML_REGISTRATION_COMPILES)

set(HYREMOTE_PREFLIGHT_GUI_PRIVATE_TARGET FALSE)
set(HYREMOTE_PREFLIGHT_PRIVATE_HEADERS_COMPILE FALSE)
if(TARGET ${QT_PACKAGE}::GuiPrivate)
    set(HYREMOTE_PREFLIGHT_GUI_PRIVATE_TARGET TRUE)
    set(CMAKE_REQUIRED_LIBRARIES ${QT_PACKAGE}::Gui ${QT_PACKAGE}::GuiPrivate)

    # This is deliberately an availability probe, not a compatibility claim about one copied
    # QWindowSystemInterface call signature. HyRemote's input ingress and QPA remain private-ABI
    # adaptation points that must be compiled against each exact admitted Qt patch.
    check_cxx_source_compiles([=[
#include <private/qhighdpiscaling_p.h>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformintegrationplugin.h>
#include <qpa/qplatformintegrationfactory_p.h>
int main() { return 0; }
]=] HYREMOTE_PREFLIGHT_PRIVATE_HEADERS_COMPILE)
endif()

function(_hyremote_yes_no out value)
    if(${value})
        set(${out} "YES" PARENT_SCOPE)
    else()
        set(${out} "NO" PARENT_SCOPE)
    endif()
endfunction()

_hyremote_yes_no(_public_surface HYREMOTE_PREFLIGHT_PUBLIC_SURFACE_COMPILES)
_hyremote_yes_no(_current_qml_header HYREMOTE_PREFLIGHT_CURRENT_QML_HEADER_COMPILES)
_hyremote_yes_no(_legacy_qml HYREMOTE_PREFLIGHT_LEGACY_QML_REGISTRATION_COMPILES)
_hyremote_yes_no(_gui_private HYREMOTE_PREFLIGHT_GUI_PRIVATE_TARGET)
_hyremote_yes_no(_private_headers HYREMOTE_PREFLIGHT_PRIVATE_HEADERS_COMPILE)

if(COMMAND qt_add_qml_module)
    set(_qt_add_qml_module YES)
else()
    set(_qt_add_qml_module NO)
endif()
if(COMMAND qt_generate_deploy_app_script)
    set(_qt_deploy_app YES)
else()
    set(_qt_deploy_app NO)
endif()
if(COMMAND qt_generate_deploy_qml_app_script)
    set(_qt_deploy_qml YES)
else()
    set(_qt_deploy_qml NO)
endif()
if(TARGET ${QT_PACKAGE}::qmake)
    set(_qmake_target YES)
else()
    set(_qmake_target NO)
endif()

file(WRITE "${CMAKE_BINARY_DIR}/qt-lts-evidence.txt"
"QT_PACKAGE=${QT_PACKAGE}\n"
"QT_VERSION=${HYREMOTE_PREFLIGHT_QT_VERSION}\n"
"PUBLIC_SURFACE_COMPILES=${_public_surface}\n"
"CURRENT_QML_INTEGRATION_HEADER_COMPILES=${_current_qml_header}\n"
"LEGACY_QML_REGISTRATION_COMPILES=${_legacy_qml}\n"
"GUI_PRIVATE_TARGET=${_gui_private}\n"
"PRIVATE_GUI_QPA_HEADERS_COMPILE=${_private_headers}\n"
"QT_ADD_QML_MODULE_COMMAND=${_qt_add_qml_module}\n"
"QT_DEPLOY_APP_COMMAND=${_qt_deploy_app}\n"
"QT_DEPLOY_QML_COMMAND=${_qt_deploy_qml}\n"
"QMAKE_TARGET=${_qmake_target}\n")

add_executable(hyremote-qt-lts-public-probe qt_lts_public_probe.cpp)
target_link_libraries(hyremote-qt-lts-public-probe
    PRIVATE
        ${QT_PACKAGE}::Core
        ${QT_PACKAGE}::Gui
        ${QT_PACKAGE}::Network
        ${QT_PACKAGE}::Widgets
        ${QT_PACKAGE}::Quick
        ${QT_PACKAGE}::Qml)
