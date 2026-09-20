# Licensing boundary gate.
#
# HyRemote ships under Apache-2.0. The repository must not silently vendor third-party code or
# acquire Qt/OpenSSL through the product build; those dependencies remain externally supplied and
# retain their own licensing obligations.

set(_vendored_dirs "vendor" "extern" "deps" "subprojects" "third_party")

if(EXISTS "${HYREMOTE_SOURCE_DIR}/.gitmodules")
    message(FATAL_ERROR
        "licensing-boundary: .gitmodules exists. Product dependencies are environment-supplied; "
        "adding a submodule requires an explicit product/licensing decision.")
endif()

foreach(_dir IN LISTS _vendored_dirs)
    if(IS_DIRECTORY "${HYREMOTE_SOURCE_DIR}/${_dir}")
        file(GLOB_RECURSE _vendored_files RELATIVE "${HYREMOTE_SOURCE_DIR}" "${HYREMOTE_SOURCE_DIR}/${_dir}/*")
        if(_vendored_files)
            list(LENGTH _vendored_files _count)
            list(GET _vendored_files 0 _first)
            message(FATAL_ERROR
                "licensing-boundary: ${_count} tracked file(s) under vendored directory '${_dir}/' "
                "(for example '${_first}'). Third-party sources must not be committed as product dependencies.")
        endif()
    endif()
endforeach()

# tests/third_party is a verification lane, not a vendoring lane. Harnesses may fetch recorded upstream
# revisions into ignored build/work directories, but upstream project sources/licence trees are not tracked.
if(IS_DIRECTORY "${HYREMOTE_SOURCE_DIR}/tests/third_party")
    file(GLOB_RECURSE _lane_files RELATIVE "${HYREMOTE_SOURCE_DIR}"
         "${HYREMOTE_SOURCE_DIR}/tests/third_party/*")
    foreach(_lane_file IN LISTS _lane_files)
        if(_lane_file MATCHES "(LICENSE|COPYING|COPYRIGHT)(\\.|$)"
           OR (_lane_file MATCHES "/src/" AND _lane_file MATCHES "\\.(c|cc|cpp|h|hpp)$"))
            message(FATAL_ERROR
                "licensing-boundary: '${_lane_file}' looks like vendored upstream content. "
                "The third-party lane stores verification harnesses, not upstream source trees.")
        endif()
    endforeach()
endif()

# Qt and OpenSSL must come from the environment or an explicitly selected prefix. Include the grouped
# integrations tree in this scan so a frontend cannot bypass the repository-level dependency policy.
file(GLOB_RECURSE _build_drivers RELATIVE "${HYREMOTE_SOURCE_DIR}"
     "${HYREMOTE_SOURCE_DIR}/CMakeLists.txt"
     "${HYREMOTE_SOURCE_DIR}/cmake/*.cmake"
     "${HYREMOTE_SOURCE_DIR}/cmake/*.in"
     "${HYREMOTE_SOURCE_DIR}/src/*/CMakeLists.txt"
     "${HYREMOTE_SOURCE_DIR}/src/integrations/*/CMakeLists.txt"
     "${HYREMOTE_SOURCE_DIR}/tests/*/CMakeLists.txt"
     "${HYREMOTE_SOURCE_DIR}/examples/*/CMakeLists.txt")
foreach(_driver IN LISTS _build_drivers)
    file(READ "${HYREMOTE_SOURCE_DIR}/${_driver}" _driver_text)
    if(_driver_text MATCHES "FetchContent|ExternalProject_Add|CPMAddPackage")
        foreach(_forbidden IN ITEMS "Qt6" "Qt5" "OpenSSL" "openssl")
            if(_driver_text MATCHES "${_forbidden}")
                message(FATAL_ERROR
                    "licensing-boundary: ${_driver} acquires ${_forbidden} with a build-time dependency fetch. "
                    "Qt/OpenSSL must remain environment/prefix supplied.")
            endif()
        endforeach()
    endif()
endforeach()

foreach(_required IN ITEMS "LICENSE" "NOTICE.md")
    if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/${_required}")
        message(FATAL_ERROR "licensing-boundary: release must carry ${_required}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/NOTICE.md" _notice)
foreach(_classified IN ITEMS "Qt" "OpenSSL")
    if(NOT _notice MATCHES "${_classified}")
        message(FATAL_ERROR "licensing-boundary: NOTICE.md no longer classifies ${_classified}")
    endif()
endforeach()

message(STATUS
    "HyRemote licensing boundary gate: PASS "
    "(no submodules/vendored dependency trees, no Qt/OpenSSL product-build acquisition, LICENSE + NOTICE present)")
