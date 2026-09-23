cmake_minimum_required(VERSION 3.21)

get_filename_component(HYREMOTE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Built-in defaults describe a neutral Common Runtime build. The repository-root build.yml is the
# normal developer profile and explicitly selects the desired peer frontends. CLI values always win.
set(HYB_CONFIG_PATH "${HYREMOTE_SOURCE_DIR}/build.yml")
set(HYB_CONFIG_REQUIRED OFF)
set(HYB_BUILD_TYPE "Release")
set(HYB_BUILD_DIR "build")
set(HYB_GENERATOR "Ninja")
set(HYB_JOBS "")
set(HYB_QT_PREFIX "")
set(HYB_TOOLCHAIN "")
set(HYB_C_COMPILER "")
set(HYB_CXX_COMPILER "")
set(HYB_CPP OFF)
set(HYB_QML OFF)
set(HYB_GENERIC OFF)
set(HYB_QPA OFF)
set(HYB_SECURITY OFF)
set(HYB_TESTS_BUILD OFF)
set(HYB_TESTS_RUN OFF)
set(HYB_TESTS_PARALLEL "")
set(HYB_TESTS_EXCLUDE "")
set(HYB_TESTS_XVFB OFF)
set(HYB_EXAMPLES OFF)
set(HYB_CLEAN OFF)
set(HYB_VERBOSE OFF)
set(HYB_SHOW_CONFIG OFF)
# build.cmd's subcommand. Absent means build, so every historical invocation keeps its meaning.
set(HYB_SUBCOMMAND "build")
set(HYB_INSTALL OFF)
set(HYB_CMAKE_CACHE_ENTRIES "")
set(HYB_ENV_ENTRIES "")

function(hyb_trim_quotes input output)
    string(STRIP "${input}" value)
    if(value MATCHES "^\"(.*)\"$")
        set(value "${CMAKE_MATCH_1}")
    elseif(value MATCHES "^'(.*)'$")
        set(value "${CMAKE_MATCH_1}")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

function(hyb_bool input output context)
    string(TOLOWER "${input}" value)
    if(value STREQUAL "true" OR value STREQUAL "yes" OR value STREQUAL "on" OR value STREQUAL "1")
        set(${output} ON PARENT_SCOPE)
    elseif(value STREQUAL "false" OR value STREQUAL "no" OR value STREQUAL "off" OR value STREQUAL "0")
        set(${output} OFF PARENT_SCOPE)
    else()
        message(FATAL_ERROR "${context}: expected boolean, got '${input}'")
    endif()
endfunction()

function(hyb_append_pair variable key value)
    # Both consumers expand these entries unquoted into a command line (`cmake -E env` for the environment and `-D`
    # arguments for the cache), where an unescaped semicolon inside a value - a Windows PATH, for example - is read
    # as a list separator: the pair would become several arguments, and `cmake -E env` would try to execute a value
    # fragment as the command. Escaping keeps the value inside its own argument; CMake unescapes it on expansion.
    string(REPLACE ";" "\\;" value "${value}")
    set(items "${${variable}}")
    list(APPEND items "${key}=${value}")
    set(${variable} "${items}" PARENT_SCOPE)
endfunction()

function(hyb_set_all_frontends value)
    set(HYB_CPP "${value}" PARENT_SCOPE)
    set(HYB_QML "${value}" PARENT_SCOPE)
    set(HYB_GENERIC "${value}" PARENT_SCOPE)
    set(HYB_QPA "${value}" PARENT_SCOPE)
endfunction()

function(hyb_select_mode mode)
    string(TOLOWER "${mode}" mode_l)
    set(cpp OFF)
    set(qml OFF)
    set(generic OFF)
    set(qpa OFF)
    if(mode_l STREQUAL "cpp")
        set(cpp ON)
    elseif(mode_l STREQUAL "qml")
        set(qml ON)
    elseif(mode_l STREQUAL "generic")
        set(generic ON)
    elseif(mode_l STREQUAL "qpa")
        set(qpa ON)
    elseif(mode_l STREQUAL "all")
        set(cpp ON)
        set(qml ON)
        set(generic ON)
        set(qpa ON)
    elseif(mode_l STREQUAL "runtime" OR mode_l STREQUAL "minimal")
        # Common Runtime only: no application-facing integration frontend.
    else()
        message(FATAL_ERROR "Unknown mode '${mode}'; expected cpp, qml, generic, qpa, all, runtime or minimal")
    endif()
    set(HYB_CPP "${cpp}" PARENT_SCOPE)
    set(HYB_QML "${qml}" PARENT_SCOPE)
    set(HYB_GENERIC "${generic}" PARENT_SCOPE)
    set(HYB_QPA "${qpa}" PARENT_SCOPE)
endfunction()

function(hyb_select_integrations csv)
    set(cpp OFF)
    set(qml OFF)
    set(generic OFF)
    set(qpa OFF)
    string(REPLACE "," ";" items "${csv}")
    foreach(item IN LISTS items)
        string(STRIP "${item}" item)
        string(TOLOWER "${item}" item)
        if(item STREQUAL "")
            continue()
        elseif(item STREQUAL "cpp")
            set(cpp ON)
        elseif(item STREQUAL "qml")
            set(qml ON)
        elseif(item STREQUAL "generic")
            set(generic ON)
        elseif(item STREQUAL "qpa")
            set(qpa ON)
        else()
            message(FATAL_ERROR "Unknown integration '${item}'; expected cpp, qml, generic or qpa")
        endif()
    endforeach()
    set(HYB_CPP "${cpp}" PARENT_SCOPE)
    set(HYB_QML "${qml}" PARENT_SCOPE)
    set(HYB_GENERIC "${generic}" PARENT_SCOPE)
    set(HYB_QPA "${qpa}" PARENT_SCOPE)
endfunction()

function(hyb_set_entry section key raw_value origin)
    hyb_trim_quotes("${raw_value}" value)
    string(TOLOWER "${section}" section_l)
    string(TOLOWER "${key}" key_l)

    if(section_l STREQUAL "")
        if(key_l STREQUAL "version")
            if(NOT value STREQUAL "1")
                message(FATAL_ERROR "${origin}: unsupported build.yml version '${value}', expected 1")
            endif()
        else()
            message(FATAL_ERROR "${origin}: unsupported top-level key '${key}'")
        endif()
    elseif(section_l STREQUAL "build")
        if(key_l STREQUAL "type")
            set(HYB_BUILD_TYPE "${value}" PARENT_SCOPE)
        elseif(key_l STREQUAL "directory")
            set(HYB_BUILD_DIR "${value}" PARENT_SCOPE)
        elseif(key_l STREQUAL "generator")
            set(HYB_GENERATOR "${value}" PARENT_SCOPE)
        elseif(key_l STREQUAL "jobs")
            if(value STREQUAL "auto" OR value STREQUAL "")
                set(value "")
            elseif(NOT value MATCHES "^[1-9][0-9]*$")
                message(FATAL_ERROR "${origin}: build.jobs must be a positive integer or 'auto'")
            endif()
            set(HYB_JOBS "${value}" PARENT_SCOPE)
        elseif(key_l STREQUAL "examples")
            hyb_bool("${value}" parsed "${origin}: build.examples")
            set(HYB_EXAMPLES "${parsed}" PARENT_SCOPE)
        else()
            message(FATAL_ERROR "${origin}: unsupported build key '${key}'")
        endif()
    elseif(section_l STREQUAL "qt")
        if(key_l STREQUAL "prefix")
            set(HYB_QT_PREFIX "${value}" PARENT_SCOPE)
        else()
            message(FATAL_ERROR "${origin}: unsupported qt key '${key}'")
        endif()
    elseif(section_l STREQUAL "compiler")
        if(key_l STREQUAL "c")
            set(HYB_C_COMPILER "${value}" PARENT_SCOPE)
        elseif(key_l STREQUAL "cxx")
            set(HYB_CXX_COMPILER "${value}" PARENT_SCOPE)
        elseif(key_l STREQUAL "toolchain")
            set(HYB_TOOLCHAIN "${value}" PARENT_SCOPE)
        else()
            message(FATAL_ERROR "${origin}: unsupported compiler key '${key}'")
        endif()
    elseif(section_l STREQUAL "integrations")
        hyb_bool("${value}" parsed "${origin}: integrations.${key_l}")
        if(key_l STREQUAL "cpp")
            set(HYB_CPP "${parsed}" PARENT_SCOPE)
        elseif(key_l STREQUAL "qml")
            set(HYB_QML "${parsed}" PARENT_SCOPE)
        elseif(key_l STREQUAL "generic")
            set(HYB_GENERIC "${parsed}" PARENT_SCOPE)
        elseif(key_l STREQUAL "qpa")
            set(HYB_QPA "${parsed}" PARENT_SCOPE)
        else()
            message(FATAL_ERROR "${origin}: unsupported integration '${key}'")
        endif()
    elseif(section_l STREQUAL "transport")
        if(key_l STREQUAL "security")
            hyb_bool("${value}" parsed "${origin}: transport.security")
            set(HYB_SECURITY "${parsed}" PARENT_SCOPE)
        else()
            message(FATAL_ERROR "${origin}: unsupported transport key '${key}'")
        endif()
    elseif(section_l STREQUAL "tests")
        if(key_l STREQUAL "build")
            hyb_bool("${value}" parsed "${origin}: tests.build")
            set(HYB_TESTS_BUILD "${parsed}" PARENT_SCOPE)
        elseif(key_l STREQUAL "run")
            hyb_bool("${value}" parsed "${origin}: tests.run")
            set(HYB_TESTS_RUN "${parsed}" PARENT_SCOPE)
        elseif(key_l STREQUAL "parallel")
            if(value STREQUAL "auto" OR value STREQUAL "")
                set(value "")
            elseif(NOT value MATCHES "^[1-9][0-9]*$")
                message(FATAL_ERROR "${origin}: tests.parallel must be a positive integer or 'auto'")
            endif()
            set(HYB_TESTS_PARALLEL "${value}" PARENT_SCOPE)
        elseif(key_l STREQUAL "exclude")
            set(HYB_TESTS_EXCLUDE "${value}" PARENT_SCOPE)
        elseif(key_l STREQUAL "xvfb")
            hyb_bool("${value}" parsed "${origin}: tests.xvfb")
            set(HYB_TESTS_XVFB "${parsed}" PARENT_SCOPE)
        else()
            message(FATAL_ERROR "${origin}: unsupported tests key '${key}'")
        endif()
    elseif(section_l STREQUAL "cmake")
        if(key MATCHES "^(HYREMOTE_BUILD_CPP_API|HYREMOTE_BUILD_QML_API|HYREMOTE_WITH_GENERIC_PLUGIN|HYREMOTE_WITH_QPA_PROXY|HYREMOTE_WITH_TRANSPORT_SECURITY|HYREMOTE_BUILD_TESTS|HYREMOTE_BUILD_EXAMPLES|CMAKE_PREFIX_PATH|CMAKE_TOOLCHAIN_FILE|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER)$")
            message(FATAL_ERROR
                "${origin}: '${key}' has a dedicated build.yml field and may not be duplicated under cmake")
        endif()
        hyb_append_pair(HYB_CMAKE_CACHE_ENTRIES "${key}" "${value}")
        set(HYB_CMAKE_CACHE_ENTRIES "${HYB_CMAKE_CACHE_ENTRIES}" PARENT_SCOPE)
    elseif(section_l STREQUAL "env")
        hyb_append_pair(HYB_ENV_ENTRIES "${key}" "${value}")
        set(HYB_ENV_ENTRIES "${HYB_ENV_ENTRIES}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "${origin}: unsupported section '${section}'")
    endif()
endfunction()

# Deterministic YAML subset: scalar top-level version plus one-level mappings. This keeps the build
# bootstrap self-contained; unsupported nesting/list syntax fails closed instead of being guessed.
macro(hyb_load_config path)
    if(NOT EXISTS "${path}")
        if(HYB_CONFIG_REQUIRED)
            message(FATAL_ERROR "Build configuration not found: ${path}")
        endif()
        return()
    endif()

    file(STRINGS "${path}" lines ENCODING UTF-8)
    set(section "")
    set(line_number 0)
    foreach(raw_line IN LISTS lines)
        math(EXPR line_number "${line_number}+1")
        set(line "${raw_line}")
        string(REGEX REPLACE "[ \t]+#.*$" "" line "${line}")
        string(STRIP "${line}" stripped)
        if(stripped STREQUAL "" OR stripped MATCHES "^#")
            continue()
        endif()

        string(REGEX MATCH "^[ \t]+" indent_match "${line}")
        if(indent_match STREQUAL "")
            set(indent 0)
        else()
            string(LENGTH "${indent_match}" indent)
        endif()
        set(origin "${path}:${line_number}")

        if(indent EQUAL 0)
            set(section "")
            if(NOT stripped MATCHES "^([A-Za-z0-9_.-]+):[ \t]*(.*)$")
                message(FATAL_ERROR "${origin}: expected 'key: value' or a supported section")
            endif()
            set(key "${CMAKE_MATCH_1}")
            set(value "${CMAKE_MATCH_2}")
            if(value STREQUAL "")
                string(TOLOWER "${key}" section)
                if(NOT section MATCHES "^(build|qt|compiler|integrations|transport|tests|cmake|env)$")
                    message(FATAL_ERROR "${origin}: unsupported section '${key}'")
                endif()
            else()
                hyb_set_entry("" "${key}" "${value}" "${origin}")
            endif()
        else()
            if(section STREQUAL "")
                message(FATAL_ERROR "${origin}: nested value without a parent section")
            endif()
            if(NOT stripped MATCHES "^([A-Za-z0-9_.-]+):[ \t]*(.*)$")
                message(FATAL_ERROR "${origin}: expected 'key: value'; lists and deeper nesting are unsupported")
            endif()
            hyb_set_entry("${section}" "${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}" "${origin}")
        endif()
    endforeach()
endmacro()

function(hyb_collect_args output)
    set(args "")
    set(after_separator OFF)
    math(EXPR last "${CMAKE_ARGC}-1")
    foreach(i RANGE 0 ${last})
        set(value "${CMAKE_ARGV${i}}")
        if(after_separator)
            list(APPEND args "${value}")
        elseif(value STREQUAL "--")
            set(after_separator ON)
        endif()
    endforeach()
    set(${output} "${args}" PARENT_SCOPE)
endfunction()

hyb_collect_args(HYB_ARGS)
list(LENGTH HYB_ARGS argc)

# Pass 1 selects/omits the project configuration file. Loading it before normal CLI parsing gives
# the canonical precedence: built-in defaults < build.yml < command-line overrides.
set(i 0)
while(i LESS argc)
    list(GET HYB_ARGS ${i} arg)
    if(arg MATCHES "^--config=(.+)$")
        set(HYB_CONFIG_PATH "${CMAKE_MATCH_1}")
        set(HYB_CONFIG_REQUIRED ON)
    elseif(arg STREQUAL "--config")
        math(EXPR i "${i}+1")
        if(i GREATER_EQUAL argc)
            message(FATAL_ERROR "--config requires a path")
        endif()
        list(GET HYB_ARGS ${i} HYB_CONFIG_PATH)
        set(HYB_CONFIG_REQUIRED ON)
    elseif(arg STREQUAL "--no-config")
        set(HYB_CONFIG_PATH "")
    endif()
    math(EXPR i "${i}+1")
endwhile()

if(NOT HYB_CONFIG_PATH STREQUAL "")
    if(NOT IS_ABSOLUTE "${HYB_CONFIG_PATH}")
        set(HYB_CONFIG_PATH "${HYREMOTE_SOURCE_DIR}/${HYB_CONFIG_PATH}")
    endif()
    cmake_path(NORMAL_PATH HYB_CONFIG_PATH)
    hyb_load_config("${HYB_CONFIG_PATH}")
endif()

# The first bare word is the subcommand. It is read before the option loop so that loop keeps sole ownership of
# every option, and `help` is folded into --help here so the usage text exists in exactly one place.
set(i 0)
while(i LESS argc)
    list(GET HYB_ARGS ${i} arg)
    if(NOT arg MATCHES "^-")
        set(HYB_SUBCOMMAND "${arg}")
        break()
    endif()
    math(EXPR i "${i}+1")
endwhile()
if(HYB_SUBCOMMAND STREQUAL "help")
    list(APPEND HYB_ARGS "--help")
    # argc was measured before this fold, so it has to be measured again or the option loop would never see the word
    # that was just added to the list it iterates.
    list(LENGTH HYB_ARGS argc)
endif()

# Pass 2 applies explicit one-off overrides.
set(i 0)
while(i LESS argc)
    list(GET HYB_ARGS ${i} arg)
    if(arg STREQUAL "--help")
        message(STATUS "HyRemote build entry point\n\nUsage: build.cmd [command] [options]\n\nCommands: build (default) | install | test | clean | rebuild | help\n\nConfiguration precedence: defaults < build.yml < command line\n\n  --config=FILE | --no-config\n  --mode=cpp|qml|generic|qpa|all|runtime|minimal\n  --integrations=cpp,qml,generic,qpa\n  --cpp|--no-cpp --qml|--no-qml --generic|--no-generic --qpa|--no-qpa\n  --security | --no-security\n  --build-type=Release|Debug --build-dir=DIR --generator=NAME\n  --qt-prefix=PATH --toolchain=FILE.cmake\n  --c-compiler=PATH --cxx-compiler=PATH\n  --tests|--no-tests --run-tests|--no-run-tests\n  --examples|--no-examples --test-exclude=REGEX --xvfb|--no-xvfb\n  --cmake=KEY=VALUE --env=KEY=VALUE (repeatable)\n  --clean -v -jN --jobs=N --show-config")
        return()
    elseif(arg MATCHES "^--config=" OR arg STREQUAL "--no-config")
        # Already consumed in pass 1.
    elseif(arg STREQUAL "--config")
        math(EXPR i "${i}+1")
    elseif(arg STREQUAL "--mode" OR arg STREQUAL "--integrations" OR arg STREQUAL "--build-type"
           OR arg STREQUAL "--build-dir" OR arg STREQUAL "--generator" OR arg STREQUAL "--qt-prefix"
           OR arg STREQUAL "--toolchain" OR arg STREQUAL "--c-compiler" OR arg STREQUAL "--cxx-compiler"
           OR arg STREQUAL "--test-exclude" OR arg STREQUAL "--cmake" OR arg STREQUAL "--env"
           OR arg STREQUAL "--jobs")
        set(option_name "${arg}")
        math(EXPR i "${i}+1")
        if(i GREATER_EQUAL argc)
            message(FATAL_ERROR "${option_name} requires a value")
        endif()
        list(GET HYB_ARGS ${i} option_value)
        if(option_name STREQUAL "--mode")
            hyb_select_mode("${option_value}")
            if(option_value STREQUAL "minimal")
                set(HYB_TESTS_BUILD OFF)
                set(HYB_TESTS_RUN OFF)
                set(HYB_EXAMPLES OFF)
            endif()
        elseif(option_name STREQUAL "--integrations")
            hyb_select_integrations("${option_value}")
        elseif(option_name STREQUAL "--build-type")
            set(HYB_BUILD_TYPE "${option_value}")
        elseif(option_name STREQUAL "--build-dir")
            set(HYB_BUILD_DIR "${option_value}")
        elseif(option_name STREQUAL "--generator")
            set(HYB_GENERATOR "${option_value}")
        elseif(option_name STREQUAL "--qt-prefix")
            set(HYB_QT_PREFIX "${option_value}")
        elseif(option_name STREQUAL "--toolchain")
            set(HYB_TOOLCHAIN "${option_value}")
        elseif(option_name STREQUAL "--c-compiler")
            set(HYB_C_COMPILER "${option_value}")
        elseif(option_name STREQUAL "--cxx-compiler")
            set(HYB_CXX_COMPILER "${option_value}")
        elseif(option_name STREQUAL "--test-exclude")
            set(HYB_TESTS_EXCLUDE "${option_value}")
        elseif(option_name STREQUAL "--cmake")
            if(NOT option_value MATCHES "^([^=]+)=(.*)$")
                message(FATAL_ERROR "--cmake expects KEY=VALUE")
            endif()
            hyb_append_pair(HYB_CMAKE_CACHE_ENTRIES "${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
        elseif(option_name STREQUAL "--env")
            if(NOT option_value MATCHES "^([^=]+)=(.*)$")
                message(FATAL_ERROR "--env expects KEY=VALUE")
            endif()
            hyb_append_pair(HYB_ENV_ENTRIES "${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
        elseif(option_name STREQUAL "--jobs")
            if(NOT option_value MATCHES "^[1-9][0-9]*$")
                message(FATAL_ERROR "--jobs expects a positive integer")
            endif()
            set(HYB_JOBS "${option_value}")
        endif()
    elseif(arg MATCHES "^--mode=(.+)$")
        hyb_select_mode("${CMAKE_MATCH_1}")
        if(CMAKE_MATCH_1 STREQUAL "minimal")
            set(HYB_TESTS_BUILD OFF)
            set(HYB_TESTS_RUN OFF)
            set(HYB_EXAMPLES OFF)
        endif()
    elseif(arg MATCHES "^--integrations=(.*)$")
        hyb_select_integrations("${CMAKE_MATCH_1}")
    elseif(arg STREQUAL "--cpp")
        set(HYB_CPP ON)
    elseif(arg STREQUAL "--no-cpp")
        set(HYB_CPP OFF)
    elseif(arg STREQUAL "--qml")
        set(HYB_QML ON)
    elseif(arg STREQUAL "--no-qml")
        set(HYB_QML OFF)
    elseif(arg STREQUAL "--generic")
        set(HYB_GENERIC ON)
    elseif(arg STREQUAL "--no-generic")
        set(HYB_GENERIC OFF)
    elseif(arg STREQUAL "--qpa")
        set(HYB_QPA ON)
    elseif(arg STREQUAL "--no-qpa")
        set(HYB_QPA OFF)
    elseif(arg STREQUAL "--security")
        set(HYB_SECURITY ON)
    elseif(arg STREQUAL "--no-security")
        set(HYB_SECURITY OFF)
    elseif(arg MATCHES "^--build-type=(.+)$")
        set(HYB_BUILD_TYPE "${CMAKE_MATCH_1}")
    elseif(arg MATCHES "^--build-dir=(.+)$")
        set(HYB_BUILD_DIR "${CMAKE_MATCH_1}")
    elseif(arg MATCHES "^--generator=(.+)$")
        set(HYB_GENERATOR "${CMAKE_MATCH_1}")
    elseif(arg MATCHES "^--qt-prefix=(.+)$")
        set(HYB_QT_PREFIX "${CMAKE_MATCH_1}")
    elseif(arg MATCHES "^--toolchain=(.+)$")
        set(HYB_TOOLCHAIN "${CMAKE_MATCH_1}")
    elseif(arg MATCHES "^--c-compiler=(.+)$")
        set(HYB_C_COMPILER "${CMAKE_MATCH_1}")
    elseif(arg MATCHES "^--cxx-compiler=(.+)$")
        set(HYB_CXX_COMPILER "${CMAKE_MATCH_1}")
    elseif(arg STREQUAL "--tests")
        set(HYB_TESTS_BUILD ON)
    elseif(arg STREQUAL "--no-tests")
        set(HYB_TESTS_BUILD OFF)
        set(HYB_TESTS_RUN OFF)
    elseif(arg STREQUAL "--run-tests")
        set(HYB_TESTS_BUILD ON)
        set(HYB_TESTS_RUN ON)
    elseif(arg STREQUAL "--no-run-tests")
        set(HYB_TESTS_RUN OFF)
    elseif(arg STREQUAL "--examples")
        set(HYB_EXAMPLES ON)
    elseif(arg STREQUAL "--no-examples")
        set(HYB_EXAMPLES OFF)
    elseif(arg MATCHES "^--test-exclude=(.*)$")
        set(HYB_TESTS_EXCLUDE "${CMAKE_MATCH_1}")
    elseif(arg STREQUAL "--xvfb")
        set(HYB_TESTS_XVFB ON)
    elseif(arg STREQUAL "--no-xvfb")
        set(HYB_TESTS_XVFB OFF)
    elseif(arg MATCHES "^--cmake=(.+)$")
        if(NOT CMAKE_MATCH_1 MATCHES "^([^=]+)=(.*)$")
            message(FATAL_ERROR "--cmake expects KEY=VALUE")
        endif()
        hyb_append_pair(HYB_CMAKE_CACHE_ENTRIES "${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
    elseif(arg MATCHES "^--env=(.+)$")
        if(NOT CMAKE_MATCH_1 MATCHES "^([^=]+)=(.*)$")
            message(FATAL_ERROR "--env expects KEY=VALUE")
        endif()
        hyb_append_pair(HYB_ENV_ENTRIES "${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
    elseif(arg STREQUAL "--clean")
        set(HYB_CLEAN ON)
    elseif(arg STREQUAL "-v" OR arg STREQUAL "--verbose")
        set(HYB_VERBOSE ON)
    elseif(arg STREQUAL "--show-config")
        set(HYB_SHOW_CONFIG ON)
    elseif(arg MATCHES "^-j([1-9][0-9]*)$")
        set(HYB_JOBS "${CMAKE_MATCH_1}")
    elseif(arg MATCHES "^--jobs=([1-9][0-9]*)$")
        set(HYB_JOBS "${CMAKE_MATCH_1}")
    elseif(NOT arg MATCHES "^-")
        # Already consumed as the subcommand.
    else()
        message(FATAL_ERROR "Unknown build option '${arg}'. Use --help.")
    endif()
    math(EXPR i "${i}+1")
endwhile()

# What the invocation does. Options keep working beside the command, so "build.cmd install --security" configures
# the tree it installs; nothing here is a second configuration system.
if(HYB_SUBCOMMAND STREQUAL "build")
    # configure if required, then compile
elseif(HYB_SUBCOMMAND STREQUAL "install")
    # build if required, then install
    set(HYB_INSTALL ON)
elseif(HYB_SUBCOMMAND STREQUAL "test")
    # the test command owns both switches, so tests are built and run even when build.yml leaves them off
    set(HYB_TESTS_BUILD ON)
    set(HYB_TESTS_RUN ON)
elseif(HYB_SUBCOMMAND STREQUAL "clean")
    set(HYB_CLEAN ON)
elseif(HYB_SUBCOMMAND STREQUAL "rebuild")
    set(HYB_CLEAN ON)
else()
    message(FATAL_ERROR
        "Unknown command '${HYB_SUBCOMMAND}'. Use one of: build, install, test, clean, rebuild, help.")
endif()

if(HYB_TESTS_RUN AND NOT HYB_TESTS_BUILD)
    set(HYB_TESTS_BUILD ON)
endif()

function(hyb_resolve_path variable must_exist description)
    set(value "${${variable}}")
    if(value STREQUAL "")
        return()
    endif()
    if(NOT IS_ABSOLUTE "${value}" AND value MATCHES "[/\\\\]")
        set(value "${HYREMOTE_SOURCE_DIR}/${value}")
    endif()
    if(IS_ABSOLUTE "${value}")
        cmake_path(NORMAL_PATH value)
    endif()
    if(must_exist AND value MATCHES "[/\\\\]" AND NOT EXISTS "${value}")
        message(FATAL_ERROR "${description} does not exist: ${value}")
    endif()
    set(${variable} "${value}" PARENT_SCOPE)
endfunction()

hyb_resolve_path(HYB_QT_PREFIX TRUE "Qt prefix")
hyb_resolve_path(HYB_TOOLCHAIN TRUE "Toolchain file")
hyb_resolve_path(HYB_C_COMPILER TRUE "C compiler")
hyb_resolve_path(HYB_CXX_COMPILER TRUE "C++ compiler")

# --------------------------------------------------------------------------- Qt discovery
#
# Precedence is deliberate and absolute: a --qt-prefix on the command line, or a value in build.yml, is used exactly as
# given and is never second-guessed. Only an empty prefix is resolved here, first from the environment, then - on
# Windows - from the standard Qt Online Installer layout. Discovery adopts a kit only when exactly one candidate is
# both real and usable; anything else fails closed with the candidates and the exact next command, because choosing
# between an MSVC and a MinGW kit on the user's behalf would produce a build that cannot link.
#
# Nothing here writes an absolute Qt path into the repository: the resolved value lives in memory and in the build
# tree's own identity marker, both of which are untracked.

# A kit's directory name is the layout's own contract: <Qt>/<version>/<kit>, where the kit names its target arch and
# its compiler family - msvc2022_64, msvc2022_arm64, mingw_64, llvm-mingw_64. The kit is still validated by requiring
# the real Qt6Config.cmake below, so the name can only reject, never invent, a candidate.
function(hyb_qt_kit_class kit_dir out_class)
    get_filename_component(_kit "${kit_dir}" NAME)
    if(_kit MATCHES "arm64")
        set(_arch "arm64")
    else()
        set(_arch "x64")
    endif()
    if(_kit MATCHES "llvm")
        set(_family "clang")
        set(_needs "clang++")
    elseif(_kit MATCHES "mingw")
        set(_family "gcc")
        set(_needs "g++")
    else()
        set(_family "msvc")
        set(_needs "cl.exe")
    endif()
    set(${out_class} "${_arch}:${_family}:${_needs}" PARENT_SCOPE)
endfunction()

function(hyb_qt_candidates out_list)
    set(_candidates "")
    set(_roots "")

    # 1. the environment already names a Qt, so use it before looking anywhere else
    foreach(_env IN ITEMS QTDIR CMAKE_PREFIX_PATH)
        if(DEFINED ENV{${_env}} AND NOT "$ENV{${_env}}" STREQUAL "")
            set(_paths "$ENV{${_env}}")
            string(REPLACE "\\" "/" _paths "${_paths}")
            foreach(_entry IN LISTS _paths)
                if(NOT _entry STREQUAL "")
                    string(REGEX REPLACE "[/]+$" "" _entry "${_entry}")
                    list(APPEND _roots "${_entry}")
                endif()
            endforeach()
        endif()
    endforeach()

    # 2. the standard Windows Online Installer layout. POSIX keeps relying on CMake's own discovery, which is what the
    #    product already does when the prefix is empty.
    if(WIN32)
        file(GLOB _version_dirs "C:/Qt/6.8.*")
        foreach(_version_dir IN LISTS _version_dirs)
            file(GLOB _kit_dirs "${_version_dir}/*")
            foreach(_kit_dir IN LISTS _kit_dirs)
                list(APPEND _roots "${_kit_dir}")
            endforeach()
        endforeach()
    endif()

    list(REMOVE_DUPLICATES _roots)
    foreach(_root IN LISTS _roots)
        if(EXISTS "${_root}/lib/cmake/Qt6/Qt6Config.cmake")
            hyb_qt_kit_class("${_root}" _class)
            list(APPEND _candidates "${_root}|${_class}")
        endif()
    endforeach()
    set(${out_list} "${_candidates}" PARENT_SCOPE)
endfunction()

if(HYB_QT_PREFIX STREQUAL "")
    hyb_qt_candidates(_qt_candidates)

    if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "ARM64|AARCH64")
        set(_qt_host_arch "arm64")
    else()
        set(_qt_host_arch "x64")
    endif()

    # The compiler family the shell can actually use. Mixing an MSVC kit with a MinGW compiler (or the reverse) links
    # nothing, so a kit whose compiler this environment does not provide is not a candidate.
    find_program(_qt_cl cl.exe)
    find_program(_qt_gxx g++)
    find_program(_qt_clangxx clang++)
    set(_qt_have_msvc FALSE)
    set(_qt_have_gcc FALSE)
    set(_qt_have_clang FALSE)
    if(_qt_cl)
        set(_qt_have_msvc TRUE)
    endif()
    if(_qt_gxx)
        set(_qt_have_gcc TRUE)
    endif()
    if(_qt_clangxx)
        set(_qt_have_clang TRUE)
    endif()

    set(_qt_usable "")
    set(_qt_rejected "")
    foreach(_candidate IN LISTS _qt_candidates)
        string(REPLACE "|" ";" _parts "${_candidate}")
        list(GET _parts 0 _prefix)
        list(GET _parts 1 _class)
        string(REPLACE ":" ";" _fields "${_class}")
        list(GET _fields 0 _arch)
        list(GET _fields 1 _family)
        list(GET _fields 2 _needs)
        if(NOT _arch STREQUAL _qt_host_arch)
            list(APPEND _qt_rejected "${_prefix}  (targets ${_arch}; this host is ${_qt_host_arch})")
        elseif(NOT _qt_have_${_family})
            list(APPEND _qt_rejected "${_prefix}  (needs ${_needs}, which this shell does not provide)")
        else()
            list(APPEND _qt_usable "${_prefix}")
        endif()
    endforeach()

    if(_qt_usable)
        list(LENGTH _qt_usable _qt_usable_count)
        if(_qt_usable_count EQUAL 1)
            list(GET _qt_usable 0 HYB_QT_PREFIX)
            cmake_path(NORMAL_PATH HYB_QT_PREFIX)
            set(HYB_QT_DISCOVERY_NOTE "Qt prefix        : ${HYB_QT_PREFIX} (discovered)")
        else()
            message(FATAL_ERROR
                "Several Qt 6.8 kits on this machine could build HyRemote, and picking one would guess wrong about "
                "the compiler. Choose one explicitly:\n"
                "\n"
                "  .\\build.cmd ${HYB_SUBCOMMAND} --qt-prefix=<one-of-these>\n"
                "\n"
                "Candidates:\n    ${_qt_usable}\n"
                "\n"
                "On a POSIX shell use: sh ./build.cmd ${HYB_SUBCOMMAND} --qt-prefix=<one-of-these>")
        endif()
    elseif(_qt_candidates)
        message(FATAL_ERROR
            "Qt 6.8 kits were found on this machine, but none of them can be used from this shell as it stands. "
            "Choose one explicitly and, if it needs a compiler this shell does not have, start the matching "
            "developer environment first:\n"
            "\n"
            "  .\\build.cmd ${HYB_SUBCOMMAND} --qt-prefix=<one-of-these>\n"
            "\n"
            "Found but not usable:\n    ${_qt_rejected}\n"
            "\n"
            "On a POSIX shell use: sh ./build.cmd ${HYB_SUBCOMMAND} --qt-prefix=<one-of-these>")
    else()
        message(FATAL_ERROR
            "No Qt 6.8+ installation was found, and this top-level build needs one.\n"
            "\n"
            "Point the build at a Qt 6.8.3 desktop kit, which is the only qualified line today (Qt 5.15 is tracked "
            "by issue #57 and cannot configure this product):\n"
            "\n"
            "  .\\build.cmd ${HYB_SUBCOMMAND} --qt-prefix=<path-to-Qt/6.8.3/mingw_64>\n"
            "\n"
            "On a POSIX shell use: sh ./build.cmd ${HYB_SUBCOMMAND} --qt-prefix=<path-to-Qt/6.8.3/gcc_64>\n"
            "\n"
            "Searched: QTDIR, CMAKE_PREFIX_PATH, and C:/Qt/6.8.*/\* (a kit is recognised by its "
            "lib/cmake/Qt6/Qt6Config.cmake). To build the Core library alone on purpose instead, pass "
            "-DHYREMOTE_BUILD_REMOTE_ACCESS=OFF -DHYREMOTE_BUILD_EXAMPLES=OFF -DHYREMOTE_BUILD_TESTS=OFF.")
    endif()
endif()

if(NOT IS_ABSOLUTE "${HYB_BUILD_DIR}")
    set(HYB_BUILD_DIR "${HYREMOTE_SOURCE_DIR}/${HYB_BUILD_DIR}")
endif()
cmake_path(NORMAL_PATH HYB_BUILD_DIR)

set(HYB_INTEGRATIONS "")
if(HYB_CPP)
    list(APPEND HYB_INTEGRATIONS cpp)
endif()
if(HYB_QML)
    list(APPEND HYB_INTEGRATIONS qml)
endif()
if(HYB_GENERIC)
    list(APPEND HYB_INTEGRATIONS generic)
endif()
if(HYB_QPA)
    list(APPEND HYB_INTEGRATIONS qpa)
endif()
if(HYB_INTEGRATIONS)
    string(JOIN "," HYB_INTEGRATIONS_TEXT ${HYB_INTEGRATIONS})
else()
    set(HYB_INTEGRATIONS_TEXT "runtime-only")
endif()

# Normal output is deliberately short. Where the configuration came from, where the tree goes and what kind of build
# this is are the facts a normal run needs; the full cpp/qml/generic/qpa/compiler/toolchain/env/cache dump is
# diagnostic and is printed when it is asked for. Diagnostics are not reduced, only their noise in the default path.
message(STATUS "HyRemote build")
message(STATUS "  config    : ${HYB_CONFIG_PATH}")
message(STATUS "  build dir : ${HYB_BUILD_DIR}")
message(STATUS "  build type: ${HYB_BUILD_TYPE}")
if(HYB_QT_DISCOVERY_NOTE)
    message(STATUS "  ${HYB_QT_DISCOVERY_NOTE}")
endif()

if(HYB_SHOW_CONFIG OR HYB_VERBOSE)
    message(STATUS "HyRemote build configuration")
    message(STATUS "  command           : ${HYB_SUBCOMMAND}")
    message(STATUS "  integrations.cpp  : ${HYB_CPP}")
    message(STATUS "  integrations.qml  : ${HYB_QML}")
    message(STATUS "  integrations.generic: ${HYB_GENERIC}")
    message(STATUS "  integrations.qpa  : ${HYB_QPA}")
    message(STATUS "  integrations      : ${HYB_INTEGRATIONS_TEXT}")
    message(STATUS "  transport.security: ${HYB_SECURITY}")
    message(STATUS "  build type        : ${HYB_BUILD_TYPE}")
    message(STATUS "  build dir         : ${HYB_BUILD_DIR}")
    message(STATUS "  generator         : ${HYB_GENERATOR}")
    message(STATUS "  jobs              : ${HYB_JOBS}")
    message(STATUS "  Qt prefix         : ${HYB_QT_PREFIX}")
    message(STATUS "  toolchain         : ${HYB_TOOLCHAIN}")
    message(STATUS "  C compiler        : ${HYB_C_COMPILER}")
    message(STATUS "  C++ compiler      : ${HYB_CXX_COMPILER}")
    message(STATUS "  tests             : build=${HYB_TESTS_BUILD}, run=${HYB_TESTS_RUN}, xvfb=${HYB_TESTS_XVFB}")
    message(STATUS "  examples          : ${HYB_EXAMPLES}")
    message(STATUS "  cmake cache       : ${HYB_CMAKE_CACHE_ENTRIES}")
    message(STATUS "  env               : ${HYB_ENV_ENTRIES}")
endif()

if(HYB_SHOW_CONFIG)
    return()
endif()

if(HYB_CLEAN AND EXISTS "${HYB_BUILD_DIR}")
    file(REMOVE_RECURSE "${HYB_BUILD_DIR}")
endif()
if(HYB_SUBCOMMAND STREQUAL "clean")
    # The install root is inside the build tree, so removing the tree removes the deployed product with it.
    message(STATUS "HYREMOTE_BUILD_DIR=${HYB_BUILD_DIR}")
    message(STATUS "HyRemote clean complete")
    return()
endif()
file(MAKE_DIRECTORY "${HYB_BUILD_DIR}")

# One build tree must not silently mix compiler/Qt/frontend/security identities, but a configuration change the tool
# can handle itself must not become the user's problem: a tree that carries HyRemote's own marker is recreated here, so
# changing build.yml or a command-line option is enough and rebuild is no longer a prerequisite.
#
# The marker is also the safety boundary. A directory that does not carry it was not created by this entry point, so it
# is never removed automatically - it fails closed and names the two ways forward. `rebuild` stays the explicit
# force-clean path, and it removes the tree before this block, so it never depends on the marker.
set(identity
    "generator=${HYB_GENERATOR}\nqt=${HYB_QT_PREFIX}\ntoolchain=${HYB_TOOLCHAIN}\nc=${HYB_C_COMPILER}\ncxx=${HYB_CXX_COMPILER}\ncpp=${HYB_CPP}\nqml=${HYB_QML}\ngeneric=${HYB_GENERIC}\nqpa=${HYB_QPA}\nsecurity=${HYB_SECURITY}\n")
set(identity_file "${HYB_BUILD_DIR}/.hyremote-build-identity")
set(hyb_recreate_build_tree FALSE)
if(EXISTS "${identity_file}")
    file(READ "${identity_file}" previous_identity)
    if(NOT previous_identity STREQUAL identity)
        set(hyb_recreate_build_tree TRUE)
    endif()
elseif(EXISTS "${HYB_BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR
        "${HYB_BUILD_DIR} holds a CMake build tree that HyRemote did not create: it carries no "
        ".hyremote-build-identity marker, so HyRemote will not delete it automatically. Remove that directory "
        "yourself, or point --build-dir at another directory.")
else()
    file(GLOB hyb_build_dir_entries "${HYB_BUILD_DIR}/*")
    list(LENGTH hyb_build_dir_entries hyb_build_dir_entry_count)
    if(hyb_build_dir_entry_count GREATER 0)
        message(FATAL_ERROR
            "${HYB_BUILD_DIR} is not empty and carries no .hyremote-build-identity marker, so HyRemote did not create "
            "it and will not delete it automatically. Remove that directory yourself, or point --build-dir at another "
            "directory.")
    endif()
endif()

if(hyb_recreate_build_tree)
    message(STATUS "HyRemote: build configuration changed; recreating ${HYB_BUILD_DIR}")
    file(REMOVE_RECURSE "${HYB_BUILD_DIR}")
    file(MAKE_DIRECTORY "${HYB_BUILD_DIR}")
endif()
file(WRITE "${identity_file}" "${identity}")

set(configure_cmd
    "${CMAKE_COMMAND}"
    -S "${HYREMOTE_SOURCE_DIR}"
    -B "${HYB_BUILD_DIR}"
    -G "${HYB_GENERATOR}"
    "-DCMAKE_BUILD_TYPE=${HYB_BUILD_TYPE}"
    "-DHYREMOTE_BUILD_TESTS=${HYB_TESTS_BUILD}"
    "-DHYREMOTE_BUILD_EXAMPLES=${HYB_EXAMPLES}"
    "-DHYREMOTE_BUILD_CPP_API=${HYB_CPP}"
    "-DHYREMOTE_BUILD_QML_API=${HYB_QML}"
    "-DHYREMOTE_WITH_GENERIC_PLUGIN=${HYB_GENERIC}"
    "-DHYREMOTE_WITH_QPA_PROXY=${HYB_QPA}"
    "-DHYREMOTE_WITH_TRANSPORT_SECURITY=${HYB_SECURITY}")
if(NOT HYB_QT_PREFIX STREQUAL "")
    list(APPEND configure_cmd "-DCMAKE_PREFIX_PATH=${HYB_QT_PREFIX}")
endif()
if(NOT HYB_TOOLCHAIN STREQUAL "")
    list(APPEND configure_cmd "-DCMAKE_TOOLCHAIN_FILE=${HYB_TOOLCHAIN}")
endif()
if(NOT HYB_C_COMPILER STREQUAL "")
    list(APPEND configure_cmd "-DCMAKE_C_COMPILER=${HYB_C_COMPILER}")
endif()
if(NOT HYB_CXX_COMPILER STREQUAL "")
    list(APPEND configure_cmd "-DCMAKE_CXX_COMPILER=${HYB_CXX_COMPILER}")
endif()
foreach(entry IN LISTS HYB_CMAKE_CACHE_ENTRIES)
    list(APPEND configure_cmd "-D${entry}")
endforeach()

set(env_cmd "${CMAKE_COMMAND}" -E env)
foreach(entry IN LISTS HYB_ENV_ENTRIES)
    list(APPEND env_cmd "${entry}")
endforeach()

set(configure_log "${HYB_BUILD_DIR}/configure.log")
if(HYB_VERBOSE)
    execute_process(COMMAND ${env_cmd} ${configure_cmd} RESULT_VARIABLE rc)
else()
    execute_process(
        COMMAND ${env_cmd} ${configure_cmd}
        RESULT_VARIABLE rc
        OUTPUT_FILE "${configure_log}"
        ERROR_FILE "${configure_log}")
endif()
if(NOT rc EQUAL 0)
    # The same reasoning the test-failure path already follows: the run knows the log, so it must show it. "See
    # configure.log" makes the user open a file to learn something as basic as "Qt6 not found".
    if(EXISTS "${configure_log}")
        file(READ "${configure_log}" _hyb_configure_log_text)
        string(STRIP "${_hyb_configure_log_text}" _hyb_configure_log_text)
        message(STATUS "CONFIGURE_EXIT_STATUS=${rc}")
        message(STATUS "CONFIGURE_LOG=${configure_log}")
        message(STATUS "--- begin ${configure_log} ---")
        message(STATUS "${_hyb_configure_log_text}")
        message(STATUS "--- end ${configure_log} ---")
    endif()
    message(FATAL_ERROR
        "Configure failed with exit status ${rc}. The full log is above and in ${configure_log}.")
endif()

set(build_cmd "${CMAKE_COMMAND}" --build "${HYB_BUILD_DIR}")
if(NOT HYB_JOBS STREQUAL "")
    list(APPEND build_cmd --parallel "${HYB_JOBS}")
else()
    list(APPEND build_cmd --parallel)
endif()
set(build_log "${HYB_BUILD_DIR}/build.log")
if(HYB_VERBOSE)
    execute_process(COMMAND ${env_cmd} ${build_cmd} RESULT_VARIABLE rc)
else()
    execute_process(
        COMMAND ${env_cmd} ${build_cmd}
        RESULT_VARIABLE rc
        OUTPUT_FILE "${build_log}"
        ERROR_FILE "${build_log}")
endif()
if(NOT rc EQUAL 0)
    if(EXISTS "${build_log}")
        file(READ "${build_log}" _hyb_build_log_text)
        string(STRIP "${_hyb_build_log_text}" _hyb_build_log_text)
        message(STATUS "BUILD_EXIT_STATUS=${rc}")
        message(STATUS "BUILD_LOG=${build_log}")
        message(STATUS "--- begin ${build_log} ---")
        message(STATUS "${_hyb_build_log_text}")
        message(STATUS "--- end ${build_log} ---")
    endif()
    message(FATAL_ERROR
        "Build failed with exit status ${rc}. The full log is above and in ${build_log}.")
endif()

if(HYB_INSTALL)
    set(HYB_INSTALL_ROOT "${HYB_BUILD_DIR}/install")
    # Stale safety: the tree is materialized from this configuration alone, so a previous run cannot leave a
    # frontend, example or security payload behind that this configuration no longer produces. The install root
    # lives inside the build tree, which is also why clean removes it.
    if(EXISTS "${HYB_INSTALL_ROOT}")
        file(REMOVE_RECURSE "${HYB_INSTALL_ROOT}")
    endif()
    file(MAKE_DIRECTORY "${HYB_INSTALL_ROOT}")

    set(install_cmd "${CMAKE_COMMAND}" --install "${HYB_BUILD_DIR}" --prefix "${HYB_INSTALL_ROOT}")
    set(install_log "${HYB_BUILD_DIR}/install.log")
    if(HYB_VERBOSE)
        execute_process(COMMAND ${env_cmd} ${install_cmd} RESULT_VARIABLE rc)
    else()
        execute_process(
            COMMAND ${env_cmd} ${install_cmd}
            RESULT_VARIABLE rc
            OUTPUT_FILE "${install_log}"
            ERROR_FILE "${install_log}")
    endif()
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "Install failed (${rc}). See ${install_log}")
    endif()
    message(STATUS "HYREMOTE_INSTALL_ROOT=${HYB_INSTALL_ROOT}")
    # A truthful, human-readable manifest beside the tree. It is deliberately not a schema or a framework: it states
    # the facts this build already knows, so a user holding the directory can tell what it is and what it contains.
    # The shipped security facts and the build capability are separate fields on purpose: AUTHENTICATION_* and
    # TRANSPORT_ENCRYPTION_* say what this artifact does by default, and TRANSPORT_SECURITY_CAPABILITY says only what
    # it could be configured to do. The defaults here mirror the runtime's own defaults (SecurityProfile::Insecure and
    # remote input off); the install contract gate asserts both sides.
    # The Qt version is read back from the cache Qt itself populated, so it reports the Qt that was actually used.
    set(_hyb_qt_version "unknown")
    set(_hyb_default_port "5921")
    if(EXISTS "${HYB_BUILD_DIR}/CMakeCache.txt")
        file(READ "${HYB_BUILD_DIR}/CMakeCache.txt" _hyb_cache)
        if(_hyb_cache MATCHES "Qt6Core_DIR[^\n]*/([0-9]+\\.[0-9]+\\.[0-9]+)/")
            set(_hyb_qt_version "${CMAKE_MATCH_1}")
        endif()
        # The runtime's own default listener port, read from the configuration that compiled it, so the manifest
        # states the port the artifact actually uses rather than a number copied into this file.
        if(_hyb_cache MATCHES "HYREMOTE_DEFAULT_PORT[^\n]*=([0-9]+)")
            set(_hyb_default_port "${CMAKE_MATCH_1}")
        endif()
    endif()
    set(_hyb_source_sha "unknown")
    execute_process(
        COMMAND git -C "${HYREMOTE_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE _hyb_git_rc
        OUTPUT_VARIABLE _hyb_git_out
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_hyb_git_rc EQUAL 0 AND NOT _hyb_git_out STREQUAL "")
        set(_hyb_source_sha "${_hyb_git_out}")
    endif()

    # CMAKE_HOST_SYSTEM_PROCESSOR is not populated by every generator/host pair, and an empty ARCH= line would be a
    # manifest that claims less than it knows. The environment's own architecture is the first fallback because that is
    # the word a Windows user recognises, then CMake's target processor, and only then "unknown".
    set(_hyb_arch "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    if(_hyb_arch STREQUAL "")
        set(_hyb_arch "$ENV{PROCESSOR_ARCHITECTURE}")
    endif()
    if(_hyb_arch STREQUAL "")
        set(_hyb_arch "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    if(_hyb_arch STREQUAL "")
        set(_hyb_arch "unknown")
    endif()

    file(WRITE "${HYB_INSTALL_ROOT}/HYREMOTE-MANIFEST.txt"
        "SOURCE_SHA=${_hyb_source_sha}\n"
        "OS=${CMAKE_HOST_SYSTEM_NAME}\n"
        "ARCH=${_hyb_arch}\n"
        "QT_VERSION=${_hyb_qt_version}\n"
        "BUILD_TYPE=${HYB_BUILD_TYPE}\n"
        "CPP=${HYB_CPP}\n"
        "QML=${HYB_QML}\n"
        "GENERIC=${HYB_GENERIC}\n"
        "QPA=${HYB_QPA}\n"
        "LISTENER_DEFAULT=0.0.0.0:${_hyb_default_port}\n"
        "AUTHENTICATION_ENABLED=OFF\n"
        "AUTHENTICATION_PROFILE=none\n"
        "TRANSPORT_ENCRYPTION_ENABLED=OFF\n"
        "TRANSPORT_ENCRYPTION_PROFILE=none\n"
        "REMOTE_INPUT_DEFAULT=OFF\n"
        "TRANSPORT_SECURITY_CAPABILITY=${HYB_SECURITY}\n")
    message(STATUS "HYREMOTE_INSTALL_MANIFEST=${HYB_INSTALL_ROOT}/HYREMOTE-MANIFEST.txt")
endif()

if(HYB_TESTS_RUN)
    set(test_env ${HYB_ENV_ENTRIES})
    if(WIN32)
        # The runtime library is named after its target's OUTPUT_NAME, and its prefix and suffix belong to the
        # toolchain: MinGW emits libHyRemoteRemoteAccess.dll where MSVC emits HyRemoteRemoteAccess.dll. Matching only
        # the toolchain-independent part, and only directories the build tree itself produced, keeps a test process
        # resolving the runtime under test. Looking for one spelling and taking the first hit did neither: under
        # MinGW it matched only copies installed by the deployment fixtures, and the directory the tests load from
        # never reached PATH, so every test loading the runtime exited with 0xc0000135.
        file(GLOB_RECURSE runtime_candidates LIST_DIRECTORIES FALSE
            "${HYB_BUILD_DIR}/*HyRemoteRemoteAccess.dll")
        # Release evidence deploys the product under <build>/evidence/<run>/ (checklist section 10). A deployed
        # layout carries its own plugins beside its own library, so those directories must stay off PATH: with them
        # present the tests resolved a foreign copy and the whole Widgets/Quick family failed with 0xc0000602.
        list(FILTER runtime_candidates EXCLUDE REGEX "[/\\\\]evidence[/\\\\]")
        set(test_path "$ENV{PATH}")
        if(NOT HYB_QT_PREFIX STREQUAL "")
            string(PREPEND test_path "${HYB_QT_PREFIX}/bin;")
        endif()
        foreach(runtime_candidate IN LISTS runtime_candidates)
            get_filename_component(runtime_dir "${runtime_candidate}" DIRECTORY)
            string(PREPEND test_path "${runtime_dir};")
        endforeach()
        # test_env is expanded unquoted into `cmake -E env`, so this PATH has to keep its semicolons inside one
        # argument. Unescaped, the list separator split it and the first PATH fragment was executed as the command,
        # which failed as "no such file or directory" and made --run-tests unusable on Windows.
        string(REPLACE ";" "\\;" test_path "${test_path}")
        list(APPEND test_env "PATH=${test_path}")
    elseif(UNIX)
        # Same rule as the Windows branch: match the toolchain-independent part and keep every directory the build
        # tree produced, so the tests load the runtime under test rather than a deployed copy.
        file(GLOB_RECURSE runtime_candidates LIST_DIRECTORIES FALSE
            "${HYB_BUILD_DIR}/*HyRemoteRemoteAccess.so*")
        list(FILTER runtime_candidates EXCLUDE REGEX "[/\\\\]evidence[/\\\\]")
        set(test_ld "$ENV{LD_LIBRARY_PATH}")
        if(NOT HYB_QT_PREFIX STREQUAL "")
            string(PREPEND test_ld "${HYB_QT_PREFIX}/lib:")
        endif()
        foreach(runtime_candidate IN LISTS runtime_candidates)
            get_filename_component(runtime_dir "${runtime_candidate}" DIRECTORY)
            string(PREPEND test_ld "${runtime_dir}:")
        endforeach()
        list(APPEND test_env "LD_LIBRARY_PATH=${test_ld}")
    endif()

    # --run-tests must fail closed on a suite that discovers nothing. "No tests were found!!!" followed by exit 0 and
    # "HyRemote build succeeded" is a false green: it lets a lane claim integration while executing none of it, which
    # is how the release-readiness gates could stay unexecuted for as long as they did. This asks CTest itself what it
    # discovered (-N registers the tests without running them), so it keys off real test registration rather than
    # workflow text.
    execute_process(
        COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${HYB_BUILD_DIR}" -N
        RESULT_VARIABLE discover_status
        OUTPUT_VARIABLE discover_output
        ERROR_VARIABLE discover_error)
    set(HYB_DISCOVERED_TEST_COUNT 0)
    if(discover_output MATCHES "Total Tests: ([0-9]+)")
        set(HYB_DISCOVERED_TEST_COUNT "${CMAKE_MATCH_1}")
    endif()
    message(STATUS "DISCOVERED_TEST_COUNT=${HYB_DISCOVERED_TEST_COUNT}")
    if(HYB_DISCOVERED_TEST_COUNT EQUAL 0)
        message(FATAL_ERROR
            "--run-tests was requested but CTest discovers no test (ctest -N reported 0, status ${discover_status}). "
            "Configure with tests enabled, or drop --run-tests: a run that executes nothing must not report success.")
    endif()

    set(ctest_cmd "${CMAKE_CTEST_COMMAND}" --test-dir "${HYB_BUILD_DIR}" --output-on-failure)
    if(NOT HYB_TESTS_PARALLEL STREQUAL "")
        list(APPEND ctest_cmd --parallel "${HYB_TESTS_PARALLEL}")
    elseif(NOT HYB_JOBS STREQUAL "")
        list(APPEND ctest_cmd --parallel "${HYB_JOBS}")
    endif()
    if(NOT HYB_TESTS_EXCLUDE STREQUAL "")
        list(APPEND ctest_cmd -E "${HYB_TESTS_EXCLUDE}")
    endif()

    if(HYB_TESTS_XVFB AND UNIX AND NOT APPLE)
        find_program(XVFB_RUN xvfb-run)
        if(NOT XVFB_RUN)
            message(FATAL_ERROR "tests.xvfb=true but xvfb-run was not found")
        endif()
        set(run_test_cmd "${XVFB_RUN}" -a ${ctest_cmd})
    else()
        set(run_test_cmd ${ctest_cmd})
    endif()

    set(test_log "${HYB_BUILD_DIR}/test.log")
    if(HYB_VERBOSE)
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env ${test_env} ${run_test_cmd}
            RESULT_VARIABLE rc)
    else()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env ${test_env} ${run_test_cmd}
            RESULT_VARIABLE rc
            OUTPUT_FILE "${test_log}"
            ERROR_FILE "${test_log}")
    endif()
    if(NOT rc EQUAL 0)
        # rc is the ctest process exit status, not a count of failures - ctest exits 8 whenever one or more tests
        # failed, and reporting it as "Tests failed (8)" read like eight failures and hid the actual names from
        # every caller. Echo the log the same run already produced, so the build system itself is diagnosable
        # without a second CI round trip, and state what the number is.
        if(EXISTS "${test_log}")
            file(READ "${test_log}" _hyb_test_log_text)
            string(STRIP "${_hyb_test_log_text}" _hyb_test_log_text)
            message(STATUS "TEST_EXIT_STATUS=${rc}")
            message(STATUS "TEST_LOG=${test_log}")
            message(STATUS "--- begin ${test_log} ---")
            message(STATUS "${_hyb_test_log_text}")
            message(STATUS "--- end ${test_log} ---")
        endif()
        message(FATAL_ERROR
            "CTest exited with status ${rc}. See ${test_log}. "
            "That status is the ctest process exit code, not a failure count: ctest exits 8 when one or more tests "
            "failed, and the failing test names and their output are in the log echoed above.")
    endif()

    # Discovered is not the same as executed: an exclusion can filter the whole suite away, and CTest still exits 0.
    # The executed count therefore comes from the run's own result line, which is the only place that states how many
    # tests actually ran.
    if(EXISTS "${test_log}")
        file(READ "${test_log}" hyb_test_log)
        set(HYB_EXECUTED_TEST_COUNT 0)
        if(hyb_test_log MATCHES "out of ([0-9]+)")
            set(HYB_EXECUTED_TEST_COUNT "${CMAKE_MATCH_1}")
        endif()
        message(STATUS "EXECUTED_TEST_COUNT=${HYB_EXECUTED_TEST_COUNT}")
        if(HYB_EXECUTED_TEST_COUNT EQUAL 0 OR hyb_test_log MATCHES "No tests were found")
            message(FATAL_ERROR
                "--run-tests discovered ${HYB_DISCOVERED_TEST_COUNT} test(s) but executed "
                "${HYB_EXECUTED_TEST_COUNT}. See ${test_log}. A run that executes nothing must not report success.")
        endif()
    endif()
endif()

message(STATUS "HyRemote build succeeded")
