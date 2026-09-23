cmake_minimum_required(VERSION 3.21)
project(HyRemoteLinuxQtRuntimeConflictCase LANGUAGES NONE)

# Generates one case's real deployment step (#382).
#
# The point of this project is that the deployment script it writes is the shipped one:
# `_hyremote_linux_private_runtime_bootstrap` is what every frontend's generated deploy script embeds, and it
# is given the same two depending files a real Linux deployment has - the deployed shared runtime and the
# deployed native platform plugin. Staging and asserting happen in verify.cmake, because a generated script
# only exists after generation, and asserting against the deployment code must happen against the artifact
# that was generated rather than against a re-derivation of it.

foreach(_required IN ITEMS HYREMOTE_SOURCE_DIR TEST_CASE SELECTED_ROOT CONSUMER PLUGIN_NAME)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "linux-qt-runtime-conflict: ${_required} is required")
    endif()
endforeach()

add_library(HyRemote::RemoteAccess SHARED IMPORTED GLOBAL)
set_target_properties(HyRemote::RemoteAccess PROPERTIES IMPORTED_LOCATION "${CONSUMER}")

# The selected consumer Qt runtime root. The bootstrap takes DIRECTORIES and the adjudication root from this
# target, which is the same source of truth production deployment uses.
add_library(Qt6::Core SHARED IMPORTED GLOBAL)
set_target_properties(Qt6::Core PROPERTIES IMPORTED_LOCATION "${SELECTED_ROOT}/libQt6Core.so.6")

include("${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteDeploy.cmake")

_hyremote_linux_private_runtime_bootstrap("\${QT_DEPLOY_LIB_DIR}" _bootstrap "${PLUGIN_NAME}")
if("${_bootstrap}" STREQUAL "")
    message(FATAL_ERROR
        "linux-qt-runtime-conflict: the Linux private runtime bootstrap produced no deployment step for a "
        "Qt-linked consumer, so this case would assert nothing")
endif()

file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/runtime-bootstrap.cmake" CONTENT "${_bootstrap}")
