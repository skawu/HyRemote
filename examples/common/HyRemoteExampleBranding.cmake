function(hyremote_example_enable_branding target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "hyremote_example_enable_branding: target '${target}' does not exist")
    endif()

    target_sources(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/hyremote-branding.qrc"
    )
    set_property(TARGET ${target} PROPERTY AUTORCC ON)
endfunction()
