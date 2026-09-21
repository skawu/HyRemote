# Clean-consumer acquisition audit, shared by the release evidence runner and its self test.
#
# A consumer that builds against a clean installed SDK must acquire HyRemote and Qt from packages, not from the
# product source tree or the product build tree. The audit below answers exactly that question for one consumer cache.
#
# It lives in its own file so the decision logic can be exercised without configuring or building a product: the
# evidence runner includes it, and tests/release-readiness/acquisition_audit_self_test.cmake drives the same functions
# with synthetic caches. Keeping one implementation is the point - a second copy of this logic in the runner would be
# free to disagree with the copy under test.

# normalize_path_cased(<in> <fold_case> <out_var>) - the comparable form of a path.
#
# Forward slashes, no repeated separators, no trailing separator, "." removed and ".." resolved syntactically, and
# lower case when case folding is on (Windows, where the filesystem is case-insensitive and a cache records whatever
# the generator happened to write). Nothing here touches the filesystem: a path is audited on its spelling, because a
# forbidden acquisition has to be caught even when the path it names does not exist.
#
# Case folding is a parameter rather than a direct WIN32 test so the Windows representation can be proved on any host.
function(normalize_path_cased in_path fold_case out_var)
    set(_path "${in_path}")
    string(STRIP "${_path}" _path)
    string(REPLACE "\\" "/" _path "${_path}")
    string(REGEX REPLACE "//+" "/" _path "${_path}")
    string(REGEX REPLACE "/\\./" "/" _path "${_path}")
    # One removal can expose another ("a/b/../.." -> "a/.." -> ""), so this repeats until it stops changing. The second
    # pattern covers a ".." that cancels the first element, which has no separator in front of it; a leading ".." that
    # has nothing left to cancel is left in place rather than silently dropped, because that would change the meaning
    # of a relative path.
    foreach(_pass RANGE 1 16)
        string(REGEX REPLACE "/([^/]+)/\\.\\./" "/" _resolved "${_path}")
        if(_resolved STREQUAL _path)
            string(REGEX REPLACE "^([^/]+)/\\.\\./" "" _resolved "${_path}")
        endif()
        if(_resolved STREQUAL _path)
            break()
        endif()
        set(_path "${_resolved}")
    endforeach()
    string(REGEX REPLACE "/\\.$" "" _path "${_path}")
    string(REGEX REPLACE "/+$" "" _path "${_path}")
    if(fold_case)
        string(TOLOWER "${_path}" _path)
    endif()
    set(${out_var} "${_path}" PARENT_SCOPE)
endfunction()

# normalize_path(<in> <out_var>) - normalize_path_cased with this host's case semantics.
function(normalize_path in_path out_var)
    normalize_path_cased("${in_path}" "${WIN32}" _normalized)
    set(${out_var} "${_normalized}" PARENT_SCOPE)
endfunction()

# path_is_under_cased(<path> <root> <fold_case> <out_bool>) - true when path is root or below it. Compared with
# string(FIND) rather than a regex, because a path contains regex metacharacters on both platforms. The comparison keeps
# a separator after the root so "/build-extra" is not read as being under "/build".
function(path_is_under_cased path root fold_case out_var)
    normalize_path_cased("${path}" "${fold_case}" _path_normalized)
    normalize_path_cased("${root}" "${fold_case}" _root_normalized)
    if(_path_normalized STREQUAL "" OR _root_normalized STREQUAL "")
        set(${out_var} FALSE PARENT_SCOPE)
        return()
    endif()
    string(FIND "${_path_normalized}/" "${_root_normalized}/" _offset)
    if(_offset EQUAL 0)
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

# path_is_under(<path> <root> <out_bool>) - path_is_under_cased with this host's case semantics.
function(path_is_under path root out_var)
    path_is_under_cased("${path}" "${root}" "${WIN32}" _under)
    set(${out_var} "${_under}" PARENT_SCOPE)
endfunction()

# cache_field(<cache_file> <key> <out_var>) - read exactly one CMake cache entry, by parsing the file as lines.
#
# A CMakeCache is not a CMake list. Its values legitimately contain semicolons, spaces, backslashes and escape
# sequences - Windows paths are the everyday case - so reading the whole file into one variable and iterating it as a
# list splits values that were never lists, and any audit built on it depends on quoting luck rather than on the file
# format. file(STRINGS) yields the real lines; the key line is then joined back losslessly, because splitting on
# semicolons and rejoining with them is a round trip.
function(cache_field cache_file key out_var)
    file(STRINGS "${cache_file}" _key_lines REGEX "^${key}:[A-Za-z0-9_]+=")
    if(NOT _key_lines)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    list(JOIN _key_lines ";" _entry)
    string(REGEX REPLACE "^${key}:[A-Za-z0-9_]+=" "" _value "${_entry}")
    string(REPLACE "\r" "" _value "${_value}")
    string(STRIP "${_value}" _value)
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

# audit_acquisition_entries(<cache_file> <run_dir> <source_root> <build_root> <out_source_count> <out_build_count>
#                           [<fold_case>])
#
# Counts acquisition entries that come from the forbidden trees. Only acquisition fields are examined - a CMake
# package, include, library or plugin path - because an audit that scans every line of the cache counts paths the
# consumer never acquired anything from.
#
# Every cache value is a CMake list, and each element is judged on its own:
#
#   1. an element inside the run's own evidence tree         -> allowed (the run lives inside the build tree, and the
#                                                               staged consumer source is the run's own input)
#   2. otherwise an element inside the product source tree   -> source violation
#   3. otherwise an element inside the product build tree    -> build violation
#
# Step 1 must be tested before step 3, because the run directory is normally a child of the build directory. What is
# not allowed is deciding at the level of the whole line: "<run-prefix>;<forbidden-path>" contains the run path, so a
# line-level skip hides the forbidden element behind it and reports a clean acquisition that is not clean.
function(audit_acquisition_entries cache_file run_dir source_root build_root out_source_count out_build_count)
    set(_fold "${WIN32}")
    if(ARGC GREATER 6)
        set(_fold "${ARGV6}")
    endif()

    set(_source_hits 0)
    set(_build_hits 0)

    if(EXISTS "${cache_file}")
        file(STRINGS "${cache_file}" _cache_lines
            REGEX "^(HyRemote|Qt6|[A-Za-z0-9_]*Qt6|CMAKE_PREFIX_PATH|CMAKE_[A-Za-z_]*PATH|.*_DIR|.*_INCLUDE_DIR|.*_LIBRARY|.*_PLUGIN|.*_PLUGINS|.*_FILE):")
        foreach(_line IN LISTS _cache_lines)
            string(REGEX REPLACE "^[^=]*=" "" _value "${_line}")
            string(REPLACE "\r" "" _value "${_value}")
            foreach(_entry IN LISTS _value)
                normalize_path_cased("${_entry}" "${_fold}" _entry_normalized)
                if(_entry_normalized STREQUAL "")
                    continue()
                endif()
                path_is_under_cased("${_entry_normalized}" "${run_dir}" "${_fold}" _in_run)
                if(_in_run)
                    continue()
                endif()
                path_is_under_cased("${_entry_normalized}" "${source_root}" "${_fold}" _in_source)
                if(_in_source)
                    math(EXPR _source_hits "${_source_hits} + 1")
                    continue()
                endif()
                path_is_under_cased("${_entry_normalized}" "${build_root}" "${_fold}" _in_build)
                if(_in_build)
                    math(EXPR _build_hits "${_build_hits} + 1")
                endif()
            endforeach()
        endforeach()
    endif()

    set(${out_source_count} "${_source_hits}" PARENT_SCOPE)
    set(${out_build_count} "${_build_hits}" PARENT_SCOPE)
endfunction()
