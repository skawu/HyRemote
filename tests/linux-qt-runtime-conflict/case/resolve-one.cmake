if(NOT PROBE_LIBRARY OR NOT PROBE_DIRECTORIES)
    message(FATAL_ERROR "PROBE_LIBRARY and PROBE_DIRECTORIES are required")
endif()

# Resolves one depending file on its own and reports the root its Qt runtime came from. The regression uses
# this to prove the ambiguity it is about to decide actually exists: a case that assumed two depending files
# reach two roots, without checking it, would silently stop testing the decision as soon as a linker or a
# generator changed which root a depending file reaches.
file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES "${PROBE_LIBRARY}"
    DIRECTORIES "${PROBE_DIRECTORIES}"
    RESOLVED_DEPENDENCIES_VAR _probe_resolved
    UNRESOLVED_DEPENDENCIES_VAR _probe_unresolved)

foreach(_probe_entry IN LISTS _probe_resolved)
    get_filename_component(_probe_name "${_probe_entry}" NAME)
    if(_probe_name STREQUAL "libQt6Core.so.6")
        message(STATUS "QT6CORE_RESOLVED=${_probe_entry}")
    endif()
endforeach()

foreach(_probe_entry IN LISTS _probe_unresolved)
    message(STATUS "QT6CORE_UNRESOLVED=${_probe_entry}")
endforeach()
