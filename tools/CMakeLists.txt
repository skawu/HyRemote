# Companion tools live outside the learning examples. HyRemoteTool is an
# out-of-process RFB client; it consumes only Qt and the product's wire behavior.
if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS AND TARGET Qt6::Widgets)
    add_subdirectory(hyremote-tool)
else()
    message(STATUS "  HyRemoteTool skipped: Widgets adapter/Qt Widgets unavailable")
endif()
