# Documentation path-existence gate.
#
# Why this exists: the repository twice drifted into documentation that *named* paths which do not exist - a
# `docs/reference/**` zone described in three indexes and a `docs/acceptance/**` directory designated by the layout
# authority - because every other documentation gate pins words, not paths. A reader who follows such a link learns
# that the documentation cannot be trusted, and a maintained link costs nothing while a dead one costs a support
# question.
#
# What it checks: every relative markdown link in every maintained markdown file resolves to a path that exists in the
# source tree. External URLs, pure `#anchor` links and non-file targets are ignored, and anchors are stripped before
# resolution. A repository-absolute target written as a link (for example a bare `docs/guide/install.md` inside a
# root-level document) is also checked against the repository root.
#
# What it deliberately does NOT check: prose that mentions a path between backticks without linking it. That would
# produce false positives for intentional examples, and the linked form is the one a reader clicks.

set(_md_roots
    "docs"
    "examples"
    "src"
    "tests"
    "verification"
    "cmake"
    ".github")

set(_md_files "")
foreach(_root IN LISTS _md_roots)
    if(EXISTS "${HYREMOTE_SOURCE_DIR}/${_root}")
        file(GLOB_RECURSE _found RELATIVE "${HYREMOTE_SOURCE_DIR}" "${HYREMOTE_SOURCE_DIR}/${_root}/*.md")
        list(APPEND _md_files ${_found})
    endif()
endforeach()
file(GLOB _root_md RELATIVE "${HYREMOTE_SOURCE_DIR}" "${HYREMOTE_SOURCE_DIR}/*.md")
list(APPEND _md_files ${_root_md})

list(LENGTH _md_files _md_count)
if(_md_count EQUAL 0)
    message(FATAL_ERROR "documentation-paths: no markdown documents found; the check would silently pass")
endif()

# Targets worth verifying: anything that should exist as a file in the tree. A link to a directory, a bare word or an
# in-page anchor is out of scope by construction.
set(_file_suffixes
    "md" "png" "jpg" "jpeg" "svg" "json" "txt" "cmake" "yml" "yaml"
    "hpp" "h" "cpp" "cc" "qml" "py" "ps1" "sh" "cmd" "in")

set(_checked 0)
set(_broken "")
foreach(_doc IN LISTS _md_files)
    file(READ "${HYREMOTE_SOURCE_DIR}/${_doc}" _text)
    string(REPLACE "\r" "" _text "${_text}")
    get_filename_component(_doc_dir "${HYREMOTE_SOURCE_DIR}/${_doc}" DIRECTORY)

    # Walk the text link by link instead of splitting it into a list: markdown prose contains `;`, which would break
    # CMake list semantics, and regex character classes for the delimiters behave poorly here. Every `](target)` is
    # read as: find `](`, take everything up to the next `)`.
    set(_rest "${_text}")
    # `while(TRUE)` is not usable here: these gates run in script mode (cmake -P), where the boolean constants are not
    # recognised unless CMP0012 is set, so the condition silently evaluates false and the loop body never runs. A
    # plain variable condition behaves the same under every policy.
    set(_scanning 1)
    while(_scanning)
        string(FIND "${_rest}" "](" _open)
        if(_open EQUAL -1)
            break()
        endif()
        math(EXPR _after_open "${_open} + 2")
        string(SUBSTRING "${_rest}" ${_after_open} -1 _after)
        string(FIND "${_after}" ")" _close)
        if(_close EQUAL -1)
            break()
        endif()
        string(SUBSTRING "${_after}" 0 ${_close} _target)
        math(EXPR _next "${_after_open} + ${_close} + 1")
        string(SUBSTRING "${_rest}" ${_next} -1 _rest)
        string(STRIP "${_target}" _target)
        if(_target STREQUAL "")
            continue()
        endif()
        # In-page anchors and external URLs are not repository paths.
        string(REGEX REPLACE "#.*$" "" _target "${_target}")
        if(_target STREQUAL "" OR _target MATCHES "^[a-zA-Z][a-zA-Z0-9+.-]*:")
            continue()
        endif()
        # Only verify targets that look like a file we expect to exist.
        string(REGEX REPLACE "^.*\\.([a-zA-Z0-9]+)$" "\\1" _suffix "${_target}")
        list(FIND _file_suffixes "${_suffix}" _is_file)
        if(_is_file EQUAL -1)
            continue()
        endif()

        math(EXPR _checked "${_checked} + 1")
        get_filename_component(_resolved "${_doc_dir}/${_target}" ABSOLUTE)
        if(NOT EXISTS "${_resolved}")
            # A repository-absolute target written as a link (a bare `docs/guide/x.md` inside a root-level document)
            # is a legitimate form; accept it if it resolves from the repository root.
            get_filename_component(_from_root "${HYREMOTE_SOURCE_DIR}/${_target}" ABSOLUTE)
            if(NOT EXISTS "${_from_root}")
                list(APPEND _broken "${_doc} => ${_target}")
            endif()
        endif()
    endwhile()
endforeach()

list(LENGTH _broken _broken_count)
if(_broken_count GREATER 0)
    list(JOIN _broken "\n    " _broken_text)
    message(FATAL_ERROR
        "documentation-paths: ${_broken_count} markdown link(s) point at paths that do not exist:\n    ${_broken_text}\n"
        "Fix the link, or create the path. Documentation must not name files or directories the repository does not "
        "contain.")
endif()

message(STATUS
    "HyRemote documentation path gate: PASS (${_checked} relative file link(s) across ${_md_count} document(s) resolve; "
    "external URLs, anchors and non-file targets are out of scope)")
