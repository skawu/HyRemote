#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "HyRemote::RemoteAccess" for configuration "Release"
set_property(TARGET HyRemote::RemoteAccess APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(HyRemote::RemoteAccess PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/HyRemoteRemoteAccess.lib"
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "Qt6::Widgets;Qt6::Quick;Qt6::Gui"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/HyRemoteRemoteAccess.dll"
  )

list(APPEND _cmake_import_check_targets HyRemote::RemoteAccess )
list(APPEND _cmake_import_check_files_for_HyRemote::RemoteAccess "${_IMPORT_PREFIX}/lib/HyRemoteRemoteAccess.lib" "${_IMPORT_PREFIX}/bin/HyRemoteRemoteAccess.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
