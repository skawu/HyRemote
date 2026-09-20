cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(shared_script ".github/scripts/install-linux-qt-desktop-deps.sh")
set(shared_script_path "${HYREMOTE_SOURCE_DIR}/${shared_script}")
if(NOT EXISTS "${shared_script_path}")
    message(FATAL_ERROR "ci-baseline: missing shared Linux Qt desktop dependency script")
endif()

file(READ "${shared_script_path}" deps_script)
foreach(required_token
        "set -euo pipefail"
        "xvfb"
        "libxcb-cursor0"
        "libxkbcommon-x11-0"
        "libxkbcommon-dev"
        "libxkbcommon-x11-dev"
        "libgl1-mesa-dev")
    string(FIND "${deps_script}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "ci-baseline: shared dependency script missing: ${required_token}")
    endif()
endforeach()

set(required_workflows
    ".github/workflows/remoteaccess-facade.yml"
    ".github/workflows/widgets-adapter.yml"
    ".github/workflows/quick-adapter.yml"
    ".github/workflows/qml-api.yml"
    ".github/workflows/qpa-proxy.yml"
    ".github/workflows/sdk-consumption.yml"
    ".github/workflows/v1-ga-acceptance.yml")

foreach(workflow IN LISTS required_workflows)
    set(path "${HYREMOTE_SOURCE_DIR}/${workflow}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "ci-baseline: missing required workflow: ${workflow}")
    endif()
    file(READ "${path}" workflow_text)
    string(FIND "${workflow_text}" "bash ${shared_script}" shared_call)
    if(shared_call EQUAL -1)
        message(FATAL_ERROR
            "ci-baseline: ${workflow} drifted from the shared Linux Qt desktop host baseline")
    endif()
    string(FIND "${workflow_text}" "'${shared_script}'" trigger_path)
    if(trigger_path EQUAL -1)
        message(FATAL_ERROR
            "ci-baseline: ${workflow} does not retrigger when the shared host dependency baseline changes")
    endif()
endforeach()

# Windows clean-deployment runtime evidence must not inherit the Qt SDK's bin directory. Build and
# install phases still need Qt/MSVC on PATH, but the launched deployed application must be able to
# resolve every Qt/HyRemote DLL from its deployment tree. Python-driven QPA checks capture Python by
# absolute path before narrowing PATH so the harness remains runnable without leaking the Qt SDK to
# the child process.
function(require_workflow_token workflow token description)
    set(path "${HYREMOTE_SOURCE_DIR}/${workflow}")
    file(READ "${path}" workflow_text)
    string(FIND "${workflow_text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "ci-baseline: ${workflow} missing ${description}: ${token}")
    endif()
endfunction()

foreach(workflow IN ITEMS
        ".github/workflows/qpa-proxy.yml"
        ".github/workflows/sdk-consumption.yml"
        ".github/workflows/v1-ga-acceptance.yml")
    require_workflow_token("${workflow}"
        [=[set "PYTHON_EXE=%pythonLocation%\python.exe"]=]
        "absolute Python capture for clean Windows QPA runtime checks")
endforeach()

# Focused declarative package evidence.
require_workflow_token(".github/workflows/qml-api.yml"
    [=[set "PATH=%CD%\qml-consumer-install\bin;%SystemRoot%\System32;%SystemRoot%"
          "%CD%\qml-consumer-install\bin\hyremote-installed-qml-consumer.exe"]=]
    "installed-QML clean runtime launch")

# Focused QPA package evidence.
require_workflow_token(".github/workflows/qpa-proxy.yml"
    [=[set "PATH=%CD%\qpa-consumer-install\bin;%SystemRoot%\System32;%SystemRoot%"
          "%PYTHON_EXE%" tests\consumer-installed-qpa\product_fit.py]=]
    "installed-QPA PATH isolation immediately before product-fit")

# SDK source/installed shapes: Embedded C++, QML, QPA and QML+QPA.
require_workflow_token(".github/workflows/sdk-consumption.yml"
    [=[set "PATH=%CD%\deploy-installed\bin;%SystemRoot%\System32;%SystemRoot%"
          "%CD%\deploy-installed\bin\hyremote-installed-consumer.exe"]=]
    "installed C++ clean runtime launch")
require_workflow_token(".github/workflows/sdk-consumption.yml"
    [=[set "PATH=%CD%\deploy-source\bin;%SystemRoot%\System32;%SystemRoot%"
          "%CD%\deploy-source\bin\hyremote-source-consumer.exe"]=]
    "source C++ clean runtime launch")
require_workflow_token(".github/workflows/sdk-consumption.yml"
    [=[set "PATH=%CD%\deploy-source-qml\bin;%SystemRoot%\System32;%SystemRoot%"
          "%CD%\deploy-source-qml\bin\hyremote-installed-qml-consumer.exe"]=]
    "source QML clean runtime launch")
require_workflow_token(".github/workflows/sdk-consumption.yml"
    [=[set "PATH=%CD%\deploy-source-qpa\bin;%SystemRoot%\System32;%SystemRoot%"
          "%PYTHON_EXE%" tests\consumer-installed-qpa\product_fit.py]=]
    "source QPA PATH isolation immediately before product-fit")
require_workflow_token(".github/workflows/sdk-consumption.yml"
    [=[set "PATH=%CD%\deploy-source-qml-qpa\bin;%SystemRoot%\System32;%SystemRoot%"
          "%PYTHON_EXE%" tests\consumer-installed-qpa\product_fit.py]=]
    "source QML+QPA PATH isolation immediately before product-fit")

# Integrated GA installed/source evidence repeats the same isolation on the release-like all-modes tree.
require_workflow_token(".github/workflows/v1-ga-acceptance.yml"
    [=[set "PATH=%CD%\consumer-installed-install\bin;%SystemRoot%\System32;%SystemRoot%"
          "%CD%\consumer-installed-install\bin\hyremote-installed-consumer.exe"]=]
    "GA installed C++ clean runtime launch")
require_workflow_token(".github/workflows/v1-ga-acceptance.yml"
    [=[set "PATH=%CD%\consumer-source-install\bin;%SystemRoot%\System32;%SystemRoot%"
          "%CD%\consumer-source-install\bin\hyremote-source-consumer.exe"]=]
    "GA source C++ clean runtime launch")
require_workflow_token(".github/workflows/v1-ga-acceptance.yml"
    [=[set "PATH=%CD%\consumer-qml-install\bin;%SystemRoot%\System32;%SystemRoot%"
          "%CD%\consumer-qml-install\bin\hyremote-installed-qml-consumer.exe"]=]
    "GA installed QML clean runtime launch")
require_workflow_token(".github/workflows/v1-ga-acceptance.yml"
    [=[set "PATH=%CD%\consumer-qpa-install\bin;%SystemRoot%\System32;%SystemRoot%"
          "%PYTHON_EXE%" tests\consumer-installed-qpa\product_fit.py]=]
    "GA installed QPA PATH isolation immediately before product-fit")
require_workflow_token(".github/workflows/v1-ga-acceptance.yml"
    [=[set "PATH=%CD%\consumer-qml-qpa-install\bin;%SystemRoot%\System32;%SystemRoot%"
          "%PYTHON_EXE%" tests\consumer-installed-qpa\product_fit.py]=]
    "GA combined QML+QPA PATH isolation immediately before product-fit")

message(STATUS
    "HyRemote CI environment baseline gate: PASS "
    "(shared Linux Qt desktop dependencies + Windows clean deployed-runtime isolation for all V1 modes)")
