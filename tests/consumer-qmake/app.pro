QT += widgets

TEMPLATE = app
TARGET = hyremote-qmake-consumer
SOURCES += main.cpp

# Set HYREMOTE_PREFIX to the installed SDK prefix, e.g.
#   set HYREMOTE_PREFIX=C:\path\to\hyremote-sdk
HYREMOTE_PREFIX = $$(HYREMOTE_PREFIX)
isEmpty(HYREMOTE_PREFIX): error("HYREMOTE_PREFIX must point at the installed HyRemote SDK")
include($$HYREMOTE_PREFIX/lib/cmake/HyRemote/hyremote.pri)
