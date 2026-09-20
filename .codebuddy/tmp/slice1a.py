"""Slice 1a of #143: the transport-security switch and its release-profile rule.

Deliberately excludes the OpenSSL wiring and the CI package, which must land together with the CI baseline gate;
this part is verifiable on its own and cannot turn any hosted workflow red.

Every edit is validated against the file as it exists; if any anchor is missing, nothing is written.
"""

import pathlib

WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")

OPTIONS = WT / "cmake/HyRemoteProjectOptions.cmake"
PROFILE = WT / "cmake/HyRemoteReleaseProfile.cmake"
PROFILE_TEST = WT / "tests/release-readiness/check_release_profile.cmake"
SIMPLICITY = WT / "tests/release-readiness/check_consumer_simplicity.cmake"
ROOT = WT / "CMakeLists.txt"

OPTION_LINE = 'option(HYREMOTE_WITH_QPA_PROXY "Enable the Transparent QPA Proxy integration mode" OFF)'
SECURITY_BLOCK = """
# Transport security is a V1.0.0.0 product requirement, not an optional extra, so its dependency belongs to the
# standard product build. Consumers that cannot provide OpenSSL 3 can turn the mode off explicitly, and the build
# fails closed rather than silently downgrading when it is on and the dependency is missing.
option(HYREMOTE_WITH_TRANSPORT_SECURITY "Enable authenticated and encrypted transport (requires OpenSSL 3)" ON)"""

edits = {}

# --- 1) the option ----------------------------------------------------------------------------------
text = OPTIONS.read_text(encoding="utf-8")
assert text.count(OPTION_LINE) == 1, "QPA option line not found exactly once"
edits[OPTIONS] = text.replace(OPTION_LINE, OPTION_LINE + "\n" + SECURITY_BLOCK, 1)

# --- 2) the consumer-simplicity contract -------------------------------------------------------------
text = SIMPLICITY.read_text(encoding="utf-8")
token = '    [=[option(HYREMOTE_WITH_QPA_PROXY "Enable the Transparent QPA Proxy integration mode" OFF)]=]'
assert text.count(token) == 1, "QPA token not found exactly once in the consumer-simplicity gate"
new_token = token + '\n    [=[option(HYREMOTE_WITH_TRANSPORT_SECURITY "Enable authenticated and encrypted transport (requires OpenSSL 3)" ON)]=]'
edits[SIMPLICITY] = text.replace(token, new_token, 1)

# --- 3) the release-profile module -------------------------------------------------------------------
text = PROFILE.read_text(encoding="utf-8")
sig = "    set(one_value_args VERSION QML_ENABLED QPA_ENABLED)"
assert text.count(sig) == 1, "one_value_args line not found exactly once"
text = text.replace(sig, "    set(one_value_args VERSION QML_ENABLED QPA_ENABLED SECURITY_ENABLED)", 1)

req = '''    if(NOT DEFINED HYREMOTE_PROFILE_QPA_ENABLED)
        message(FATAL_ERROR "hyremote_validate_release_profile requires QPA_ENABLED")
    endif()'''
assert text.count(req) == 1, "QPA_ENABLED requirement block not found exactly once"
text = text.replace(req, req + '''

    if(NOT DEFINED HYREMOTE_PROFILE_SECURITY_ENABLED)
        message(FATAL_ERROR "hyremote_validate_release_profile requires SECURITY_ENABLED")
    endif()''', 1)

rule_anchor = '''    if(HYREMOTE_PROFILE_VERSION VERSION_LESS "0.0.3.0" AND HYREMOTE_PROFILE_QPA_ENABLED)
        message(FATAL_ERROR
            "HyRemote ${HYREMOTE_PROFILE_VERSION} does not release the Transparent QPA integration mode. "
            "QPA becomes a released product surface at v0.0.3.0.")
    endif()'''
assert text.count(rule_anchor) == 1, "QPA version rule not found exactly once"
text = text.replace(rule_anchor, rule_anchor + '''

    # Authenticated/encrypted transport is a V1.0.0.0 requirement: earlier milestone products must reject it even
    # though the source is present, exactly as they reject QML and QPA before their own milestones.
    if(HYREMOTE_PROFILE_VERSION VERSION_LESS "1.0.0.0" AND HYREMOTE_PROFILE_SECURITY_ENABLED)
        message(FATAL_ERROR
            "HyRemote ${HYREMOTE_PROFILE_VERSION} does not release the authenticated/encrypted transport mode. "
            "Transport security becomes a released product surface at v1.0.0.0.")
    endif()''', 1)
edits[PROFILE] = text

# --- 4) the parameterized profile gate ----------------------------------------------------------------
text = PROFILE_TEST.read_text(encoding="utf-8")
vars_line = "foreach(required_var HYREMOTE_SOURCE_DIR HYREMOTE_TEST_VERSION HYREMOTE_TEST_QML HYREMOTE_TEST_QPA)"
assert text.count(vars_line) == 1, "required-var loop not found exactly once"
text = text.replace(vars_line, vars_line[:-1] + " HYREMOTE_TEST_SECURITY)", 1)
call = '''    QPA_ENABLED "${HYREMOTE_TEST_QPA}"
)'''
assert text.count(call) == 1, "profile call not found exactly once"
text = text.replace(call, '''    QPA_ENABLED "${HYREMOTE_TEST_QPA}"
    SECURITY_ENABLED "${HYREMOTE_TEST_SECURITY}"
)''', 1)
msg = '"qml=${HYREMOTE_TEST_QML}, qpa=${HYREMOTE_TEST_QPA}")'
assert text.count(msg) == 1, "profile message not found exactly once"
text = text.replace(msg, '"qml=${HYREMOTE_TEST_QML}, qpa=${HYREMOTE_TEST_QPA}, security=${HYREMOTE_TEST_SECURITY}")', 1)
edits[PROFILE_TEST] = text

# --- 5) the root project ------------------------------------------------------------------------------
text = ROOT.read_text(encoding="utf-8")
call = '    QPA_ENABLED "${HYREMOTE_WITH_QPA_PROXY}"'
assert text.count(call) == 1, "root profile call not found exactly once"
text = text.replace(call, call + '\n    SECURITY_ENABLED "${HYREMOTE_WITH_TRANSPORT_SECURITY}"', 1)

print_anchor = 'message(STATUS "  QPA Proxy:           ${HYREMOTE_WITH_QPA_PROXY}")'
assert text.count(print_anchor) == 1, "status print not found exactly once"
text = text.replace(print_anchor, print_anchor + '\nmessage(STATUS "  Transport security:  ${HYREMOTE_WITH_TRANSPORT_SECURITY}")', 1)

guard_anchor = '            "HYREMOTE_WITH_QPA_PROXY=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON and a usable HyRemote::RemoteAccess target")'
assert text.count(guard_anchor) == 1, "QPA RemoteAccess guard not found exactly once"
text = text.replace(guard_anchor, guard_anchor + '''

    if(HYREMOTE_WITH_TRANSPORT_SECURITY AND NOT HYREMOTE_BUILD_REMOTE_ACCESS)
        message(FATAL_ERROR
            "HYREMOTE_WITH_TRANSPORT_SECURITY=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON: the security mode is a")
            # placeholder replaced below
    endif()''', 1)
text = text.replace('''            "HYREMOTE_WITH_TRANSPORT_SECURITY=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON: the security mode is a")
            # placeholder replaced below''',
                    '''            "HYREMOTE_WITH_TRANSPORT_SECURITY=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON: the security mode is a "
            "capability of the public RemoteAccess facade, not a standalone artifact.")''', 1)

helper_sig = "function(hyremote_add_release_profile_test test_name version qml_enabled qpa_enabled expect_reject)"
assert text.count(helper_sig) == 1, "profile-test helper signature not found exactly once"
text = text.replace(helper_sig, "function(hyremote_add_release_profile_test test_name version qml_enabled qpa_enabled security_enabled expect_reject)", 1)

old_tail = '                -DHYREMOTE_TEST_QPA=${qpa_enabled}'
assert text.count(old_tail) == 1, "helper QPA argument not found exactly once"
text = text.replace(old_tail, old_tail + '\n                -DHYREMOTE_TEST_SECURITY=${security_enabled}', 1)

cases = [
    ("hyremote-release-profile-develop-all-modes", "0.0.0", "ON", "ON", "ON", "FALSE"),
    ("hyremote-release-profile-v001-cpp-only", "0.0.1.0", "OFF", "OFF", "OFF", "FALSE"),
    ("hyremote-release-profile-v001-reject-qml", "0.0.1.0", "ON", "OFF", "OFF", "TRUE"),
    ("hyremote-release-profile-v001-reject-qpa", "0.0.1.0", "OFF", "ON", "OFF", "TRUE"),
    ("hyremote-release-profile-v001-reject-security", "0.0.1.0", "OFF", "OFF", "ON", "TRUE"),
    ("hyremote-release-profile-v002-qml", "0.0.2.0", "ON", "OFF", "OFF", "FALSE"),
    ("hyremote-release-profile-v002-reject-qpa", "0.0.2.0", "ON", "ON", "OFF", "TRUE"),
    ("hyremote-release-profile-v002-reject-security", "0.0.2.0", "ON", "OFF", "ON", "TRUE"),
    ("hyremote-release-profile-v003-all-modes", "0.0.3.0", "ON", "ON", "OFF", "FALSE"),
    ("hyremote-release-profile-v003-reject-security", "0.0.3.0", "ON", "ON", "ON", "TRUE"),
    ("hyremote-release-profile-v100-all-modes", "1.0.0.0", "ON", "ON", "ON", "FALSE"),
]
start = text.index("hyremote_add_release_profile_test(hyremote-release-profile-develop-all-modes")
end = text.index("\n", text.index("hyremote-release-profile-v100-all-modes"))
new_cases = "\n".join("hyremote_add_release_profile_test(%s %s %s %s %s %s)" % c for c in cases)
text = text[:start] + new_cases + text[end:]
edits[ROOT] = text

for path, content in edits.items():
    path.write_text(content, encoding="utf-8")
    print("edited: %s (%d bytes)" % (path.relative_to(WT), len(content)), flush=True)
print("case count in root: %d" % new_cases.count("hyremote_add_release_profile_test("), flush=True)
