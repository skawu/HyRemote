# HyRemote qmake consumption shim.
#
# This file is installed at <prefix>/lib/cmake/HyRemote/hyremote.pri. It is
# intentionally small: it adds the installed public header/library paths and
# the Qt modules RemoteAccess itself needs. A qmake application still owns its
# UI modules and its own deployment; hyremote_deploy() is CMake-only.
#
# Usage:
#   include(<prefix>/lib/cmake/HyRemote/hyremote.pri)
#   QT += widgets
#   SOURCES += main.cpp

HYREMOTE_PREFIX = $$clean_path($$PWD/../../..)

QT += core network gui
CONFIG += c++17
INCLUDEPATH += $$HYREMOTE_PREFIX/include
LIBS += -L$$HYREMOTE_PREFIX/lib -lHyRemoteRemoteAccess
