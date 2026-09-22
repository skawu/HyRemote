
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was HyRemoteConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include(CMakeFindDependencyMacro)

# CMake 3.21 is the V1 package baseline. Before CMake 3.30, nested find_package()/find_dependency()
# calls may overwrite PACKAGE_PREFIX_DIR. Preserve HyRemote's own package prefix before discovering
# Qt so optional QML/QPA payload metadata can never accidentally resolve relative to the Qt SDK.
set(_hyremote_package_prefix "${PACKAGE_PREFIX_DIR}")

# One consumer configure must use one HyRemote acquisition. Re-finding the same installed prefix is
# harmless, but mixing two installed SDK prefixes or layering find_package() over a local/source
# HyRemote::RemoteAccess target can combine one runtime with another package's optional metadata.
get_property(_hyremote_previous_package_prefix GLOBAL PROPERTY HYREMOTE_INSTALLED_PACKAGE_PREFIX)
if(_hyremote_previous_package_prefix
   AND NOT "${_hyremote_previous_package_prefix}" STREQUAL "${_hyremote_package_prefix}")
    message(FATAL_ERROR
        "HyRemote package acquisition conflict: this configure already loaded HyRemote from "
        "${_hyremote_previous_package_prefix}; refusing a second installed prefix ${_hyremote_package_prefix}")
endif()

if(TARGET HyRemote::RemoteAccess)
    get_target_property(_hyremote_existing_remoteaccess_imported HyRemote::RemoteAccess IMPORTED)
    if(NOT _hyremote_existing_remoteaccess_imported)
        message(FATAL_ERROR
            "HyRemote package acquisition conflict: find_package(HyRemote) cannot be combined with "
            "a source/add_subdirectory HyRemote::RemoteAccess target in the same configure")
    endif()
endif()
set_property(GLOBAL PROPERTY HYREMOTE_INSTALLED_PACKAGE_PREFIX "${_hyremote_package_prefix}")

# The installed V1 package exposes one shared RemoteAccess C++ target. Its installed public header
# uses Qt Core/Network types, so those are the only unconditional package-level Qt dependencies.
# Widgets, Quick and QML remain application-selected UI layers.
if(TRUE)
    find_dependency(Qt6 6.8 COMPONENTS Core Network)
endif()

# QML is optional package payload, not a second C++ target. Publish the import root plus the absolute
# backing-library payload location consumed internally by hyremote_deploy(... QML). The backing file
# metadata does not create a link target or another runtime personality; applications still consume
# the stable `import HyRemote` URI. Clear both values when the SDK has no QML payload so repeated
# package discovery cannot retain stale paths from another configuration.
# How the transport security runtime is provided, published as package metadata for hyremote_deploy() only. It is
# deliberately not a link target and not a dependency: a clean consumer never calls find_package(OpenSSL), never links
# OpenSSL and never needs to know that the runtime has a private OpenSSL dependency. The directory is derived from
# this package's own prefix, so an installed SDK stays relocatable and records no build-machine path.
set(HyRemote_TRANSPORT_SECURITY_AVAILABLE ON)
set(HyRemote_SECURITY_RUNTIME_MODE "BUNDLED")
set(HyRemote_SECURITY_RUNTIME_SUBDIR "bin")
set(HyRemote_SECURITY_RUNTIME_FILES "libcrypto-3-x64.dll;libssl-3-x64.dll")
if(HyRemote_SECURITY_RUNTIME_SUBDIR STREQUAL "")
    set(HyRemote_SECURITY_RUNTIME_DIR "")
else()
    set(HyRemote_SECURITY_RUNTIME_DIR "${_hyremote_package_prefix}/bin")
endif()

if(FALSE)
    set(HyRemote_QML_IMPORT_PATH "${_hyremote_package_prefix}/lib/qml")
    set(HyRemote_QML_BACKING_FILE
        "${_hyremote_package_prefix}//")
else()
    set(HyRemote_QML_IMPORT_PATH "")
    set(HyRemote_QML_BACKING_FILE "")
endif()

# Transparent QPA is a package-owned plugin payload. There is no consumer link target and no
# static/shared personality switch in V1: qhyremote always reuses the one shared RemoteAccess runtime.
set(HyRemote_QPA_AVAILABLE FALSE)
if(HyRemote_QPA_AVAILABLE)
    set(HyRemote_QPA_QT_VERSION "6.8.3")
    set(HyRemote_QPA_PLUGIN_FILE
        "${_hyremote_package_prefix}//")
else()
    set(HyRemote_QPA_QT_VERSION "")
    set(HyRemote_QPA_PLUGIN_FILE "")
endif()

# Generic Plugin is the other zero-code package payload. It is a QGenericPlugin that leaves the application's native
# Qt platform integration untouched, so it publishes availability and its installed payload location only.
set(HyRemote_GENERIC_AVAILABLE FALSE)
if(HyRemote_GENERIC_AVAILABLE)
    set(HyRemote_GENERIC_PLUGIN_FILE
        "${_hyremote_package_prefix}//")
else()
    set(HyRemote_GENERIC_PLUGIN_FILE "")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/HyRemoteTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/HyRemoteDeploy.cmake")

check_required_components(HyRemote)
