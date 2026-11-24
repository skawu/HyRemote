# #800: CTest must load live build-tree runtime libraries, not packaged copies.
# In-tree installs may ship Qt DLLs but omit Qt's test-only offscreen platform plugin.
# Both PATH (Windows) and LD_LIBRARY_PATH (Unix) use this private filter.
function(hyb_filter_test_runtime_candidates candidate_var build_dir)
    set(accepted)
    foreach(candidate IN LISTS ${candidate_var})
        file(RELATIVE_PATH relative "${build_dir}" "${candidate}")
        string(REPLACE "\\" "/" relative "${relative}")
        # Only paths owned by this build tree may be providers.
        if(relative MATCHES "^[.][.](/|$)" OR IS_ABSOLUTE "${relative}")
            continue()
        endif()
        # The supported install root is <build>/install. Evidence and QPA
        # deploy-helper payloads can appear below additional build subfolders.
        if(relative MATCHES "^install/"
            OR relative MATCHES "(^|/)(evidence|deploy-helper-cases)/")
            continue()
        endif()
        list(APPEND accepted "${candidate}")
    endforeach()
    set(${candidate_var} "${accepted}" PARENT_SCOPE)
endfunction()
