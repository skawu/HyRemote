cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(required_files
    "src/embedded/CMakeLists.txt"
    "src/declarative/CMakeLists.txt"
    "src/transparent/CMakeLists.txt"
    "src/transparent/tests/check_source_payload_relocation.cmake"
    "cmake/HyRemoteDeploy.cmake"
    "verification/consumer-source/main.cpp"
    "verification/consumer-installed-qpa/product_fit.py"
    "verification/release-readiness/verify_linux_dependency_origin.py")
foreach(path IN LISTS required_files)
    if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "deployment-relocation: missing required evidence file: ${path}")
    endif()
endforeach()

# The shared facade may be copied directly from an add_subdirectory() build. It must therefore have
# an origin-local build lookup as well as the installed one; otherwise a clean source deployment can
# accidentally depend on the original Qt/build path embedded by CMake.
file(READ "${HYREMOTE_SOURCE_DIR}/src/embedded/CMakeLists.txt" remoteaccess_cmake)
foreach(required_token
        [=[BUILD_RPATH "$ORIGIN"]=]
        [=[BUILD_RPATH_USE_ORIGIN TRUE]=]
        [=[INSTALL_RPATH "$ORIGIN"]=])
    string(FIND "${remoteaccess_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deployment-relocation: shared RemoteAccess lost source/install origin lookup: ${required_token}")
    endif()
endforeach()

# QML has two relocatable shared layers: the backing library lives in the runtime lib directory and
# its plugin lives in qml/HyRemote. Both source/build and installed variants must carry paths usable
# after Qt copies them into a deployed application tree.
file(READ "${HYREMOTE_SOURCE_DIR}/src/declarative/CMakeLists.txt" qml_cmake)
foreach(required_token
        [=[set_property(TARGET hyremote-qml PROPERTY BUILD_RPATH "$ORIGIN")]=]
        [=[set_property(TARGET hyremote-qml PROPERTY BUILD_RPATH_USE_ORIGIN TRUE)]=]
        [=[set_property(TARGET hyremote-qml PROPERTY INSTALL_RPATH "$ORIGIN")]=]
        [=[BUILD_RPATH "$ORIGIN/../..;$ORIGIN/../../${CMAKE_INSTALL_LIBDIR}"]=]
        [=[INSTALL_RPATH "$ORIGIN/../..;$ORIGIN/../../${CMAKE_INSTALL_LIBDIR}"]=])
    string(FIND "${qml_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deployment-relocation: QML payload lost a relocatable path contract: ${required_token}")
    endif()
endforeach()

# Source-tree and installed-SDK qhyremote payloads share one known non-toolchain RPATH segment. The
# trailing `/.` is semantically neutral in the SDK/build layout but reserves enough ELF RUNPATH string
# capacity for the deployed plugins/platforms -> lib replacement. The helper must rewrite exactly
# this package-owned segment without an undocumented ELF parser or external patching prerequisite.
file(READ "${HYREMOTE_SOURCE_DIR}/src/transparent/CMakeLists.txt" qpa_cmake)
foreach(required_token
        [=[BUILD_RPATH "$ORIGIN/../../../."]=]
        [=[BUILD_RPATH_USE_ORIGIN TRUE]=]
        [=[INSTALL_RPATH "$ORIGIN/../../../."]=]
        [=[hyremote-qpa-source-payload-relocation]=]
        [=[check_source_payload_relocation.cmake]=])
    string(FIND "${qpa_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deployment-relocation: qhyremote lost its source/install relocation evidence: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteDeploy.cmake" deploy_helper)
foreach(required_token
        [=[file(RPATH_CHANGE]=]
        [=[OLD_RPATH \"$ORIGIN/../../../.\"]=]
        [=[NEW_RPATH \"$ORIGIN/../../\${QT_DEPLOY_LIB_DIR}\"]=])
    string(FIND "${deploy_helper}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deployment-relocation: QPA helper lost bounded RPATH relocation: ${required_token}")
    endif()
endforeach()
foreach(forbidden_token
        "file(READ_ELF"
        "patchelf"
        "chrpath")
    string(FIND "${deploy_helper}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "deployment-relocation: V1 deployment gained an unsupported patching prerequisite: ${forbidden_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/src/transparent/tests/check_source_payload_relocation.cmake" source_qpa_relocation)
foreach(required_token
        [=[OLD_RPATH "$ORIGIN/../../../."]=]
        [=[NEW_RPATH "$ORIGIN/../../lib"]=]
        [=[--unset=LD_LIBRARY_PATH]=]
        [=[relocated qhyremote escaped deployment tree]=])
    string(FIND "${source_qpa_relocation}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deployment-relocation: executable source-QPA relocation proof regressed: ${required_token}")
    endif()
endforeach()

# Runtime evidence must prove where libraries were actually loaded from, not merely that the app can
# start while the original Qt SDK/build tree still exists on the runner.
file(READ "${HYREMOTE_SOURCE_DIR}/verification/consumer-source/main.cpp" source_consumer_main)
foreach(required_token
        "loadedProductLibrariesComeFromDeployment"
        "dl_iterate_phdr"
        "libHyRemoteRemoteAccess.so"
        "libQt6")
    string(FIND "${source_consumer_main}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deployment-relocation: source consumer lost loaded-library origin proof: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/verification/release-readiness/verify_linux_dependency_origin.py" origin_verifier)
foreach(required_token
        [=[PRODUCT_PREFIXES = ("libHyRemote", "libhyremote-qml", "libQt6")]=]
        [=[env.pop("LD_LIBRARY_PATH", None)]=]
        [=[deployed dependency escaped prefix]=])
    string(FIND "${origin_verifier}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deployment-relocation: Linux dependency-origin verifier lost required behavior: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/verification/consumer-installed-qpa/product_fit.py" qpa_product_fit)
foreach(required_token
        "verify_linux_dependency_origins"
        "verify_linux_dependency_origin.py"
        [=["QML2_IMPORT_PATH"]=]
        [=["QML_IMPORT_PATH"]=]
        [=["LD_LIBRARY_PATH"]=]
        [=[if os.name == "nt":]=]
        [=[env["PATH"] = os.pathsep.join]=]
        [=[str(app.parent)]=]
        [=[str(system_root / "System32")]=]
        [=[cwd=str(app.parent)]=])
    string(FIND "${qpa_product_fit}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deployment-relocation: clean QPA/combined product-fit lost isolation evidence: ${required_token}")
    endif()
endforeach()

message(STATUS
    "HyRemote deployment-relocation gate: PASS "
    "(source/install shared runtime + QML backing/plugin + QPA origin paths and Windows/Linux executable loaded-library/working-directory isolation frozen)")
