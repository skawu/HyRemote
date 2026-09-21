# Product documentation truth gate (#230).
#
# Release-facing documentation is a product artifact: an application developer reads it and builds against what it
# says. This gate therefore asserts the structural facts a stale document would get wrong - the frontend set, the
# canonical source layout, the V0.1 release identity, the deployment family and the security boundary train - rather
# than matching whole prose paragraphs, which would break on any legitimate rewording and would not actually catch a
# wrong claim.
#
# The rules are line-scoped on purpose: a document may mention a retired label in order to say it is retired, and may
# mention GA or production readiness in order to deny it. What must not happen is a bare assertion of the retired or
# over-claimed fact.

cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(release_facing_documents
    README.md
    docs/getting-started/cpp.md
    docs/getting-started/generic.md
    docs/getting-started/qml.md
    docs/getting-started/qpa-proxy.md
    docs/guide/deployment.md
    docs/guide/install.md
    docs/known-limitations.md
    docs/security-model.md
    docs/security.md
    docs/sdk-consumption.md
    docs/release-package-manifest.md)

function(read_document relative_path out_var)
    set(document_path "${HYREMOTE_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${document_path}")
        message(FATAL_ERROR "product-documentation-truth: declared release-facing document is missing: ${relative_path}")
    endif()
    file(READ "${document_path}" document_text)
    set(${out_var} "${document_text}" PARENT_SCOPE)
endfunction()

function(require_phrase document_var document_name phrase)
    string(FIND "${${document_var}}" "${phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "product-documentation-truth: ${document_name} must state '${phrase}'")
    endif()
endfunction()

# A forbidden token is allowed only on a line that also carries the context which makes it correct: a retired label
# has to be described as retired, and a GA/production claim has to be denied on the same line.
function(forbid_token document_var document_name token denial_hint)
    if("${denial_hint}" STREQUAL "")
        string(FIND "${${document_var}}" "${token}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR
                "product-documentation-truth: ${document_name} must not contain '${token}'")
        endif()
        return()
    endif()

    string(REPLACE "\r\n" "\n" document_body "${${document_var}}")
    string(REPLACE "\n" ";" document_lines "${document_body}")
    foreach(line IN LISTS document_lines)
        string(FIND "${line}" "${token}" token_found)
        if(token_found EQUAL -1)
            continue()
        endif()
        string(FIND "${line}" "${denial_hint}" hint_found)
        if(hint_found EQUAL -1)
            message(FATAL_ERROR
                "product-documentation-truth: ${document_name} states '${token}' without the context that makes it "
                "correct (expected '${denial_hint}' on the same line)")
        endif()
    endforeach()
endfunction()

# ---------------------------------------------------------------- the frontend set is four, not three

read_document("README.md" readme)
# The frontend set is four and they are peers on one runtime. The document is free to word that as it wants, so this
# asserts the claims rather than one phrasing of them.
require_phrase(readme "README.md" "All four frontends converge on the same Runtime/Core implementation")
require_phrase(readme "README.md" "**C++ API**")
require_phrase(readme "README.md" "**Generic Plugin**")
require_phrase(readme "README.md" "The QML type is a thin frontend over the same Runtime")
require_phrase(readme "README.md" "The QPA frontend uses a Factory Trampoline")
forbid_token(readme "README.md" "three mandatory integration modes" "")
forbid_token(readme "README.md" "three integration modes" "")
foreach(retired_mode_label IN ITEMS "MODE 1" "MODE 2" "MODE 3")
    forbid_token(readme "README.md" "${retired_mode_label}" "")
endforeach()

# ---------------------------------------------------------------- canonical source layout

# Physical layout has one authority, and the README has to point at it instead of restating a tree that can drift.
require_phrase(readme "README.md" "docs/internal/repository-layout.md")
foreach(retired_flat_directory IN ITEMS "  cpp/                 MODE" "  qml/                 MODE" "  qpa/                 MODE")
    forbid_token(readme "README.md" "${retired_flat_directory}" "")
endforeach()

# ---------------------------------------------------------------- V0.1 identity and the retired labels

require_phrase(readme "README.md" "Developer Preview: C++ API + Generic Plugin as the primary paths")
require_phrase(readme "README.md" "**V0.1 primary**")
foreach(retired_label IN ITEMS "v0.0.1.0" "v0.0.2.0" "v0.0.3.0")
    forbid_token(readme "README.md" "${retired_label}" "retired")
endforeach()

# The security docs must not send the VeNCrypt/TLS work back to V1.0: it is V0.2 work under #143, and V0.1 has no
# encrypted or authenticated-encrypted profile.
foreach(security_document IN ITEMS docs/known-limitations.md docs/security-model.md docs/security.md
                                 docs/getting-started/generic.md)
    read_document("${security_document}" security_text)
    string(REPLACE "\r\n" "\n" security_body "${security_text}")
    string(REPLACE "\n" ";" security_lines "${security_body}")
    foreach(line IN LISTS security_lines)
        foreach(token IN ITEMS "VeNCrypt" "TLS")
            string(FIND "${line}" "${token}" token_found)
            if(token_found EQUAL -1)
                continue()
            endif()
            string(FIND "${line}" "V1.0" v1_found)
            if(NOT v1_found EQUAL -1)
                # A line that denies the V1.0 claim is correct documentation, not a mislabel.
                string(FIND "${line}" "not " denial_found)
                if(denial_found EQUAL -1)
                    message(FATAL_ERROR
                        "product-documentation-truth: ${security_document} labels the ${token} work as V1.0.0.0; it "
                        "is V0.2 work under #143")
                endif()
            endif()
        endforeach()
    endforeach()
endforeach()

# ---------------------------------------------------------------- deployment family and Generic docs

read_document("docs/guide/deployment.md" deployment)
require_phrase(deployment "docs/guide/deployment.md" "hyremote_deploy(TARGET ExistingQtApp GENERIC)")
require_phrase(deployment "docs/guide/deployment.md" "plugins/generic/")
require_phrase(deployment "docs/guide/deployment.md" "plugins/platforms/")
forbid_token(deployment "docs/guide/deployment.md" "Generic **替换**" "")

read_document("docs/getting-started/generic.md" generic_doc)
require_phrase(generic_doc "docs/getting-started/generic.md" "find_package(HyRemote CONFIG REQUIRED)")
require_phrase(generic_doc "docs/getting-started/generic.md" "hyremote_deploy(TARGET MyApp GENERIC)")
require_phrase(generic_doc "docs/getting-started/generic.md" "-plugin hyremote")
require_phrase(generic_doc "docs/getting-started/generic.md" "loopback-only Developer Preview")
require_phrase(generic_doc "docs/getting-started/generic.md" "zero-code")

# The C++ documentation has to promise the same facade for Widgets and Quick, and it has to say that a Quick window is
# reached through C++ rather than through the QML frontend.
read_document("docs/getting-started/cpp.md" cpp_doc)
require_phrase(cpp_doc "docs/getting-started/cpp.md" "find_package(HyRemote CONFIG REQUIRED)")
require_phrase(cpp_doc "docs/getting-started/cpp.md" "HyRemote::RemoteAccess")
require_phrase(cpp_doc "docs/getting-started/cpp.md" "hyremote_deploy(TARGET MyApp)")
require_phrase(cpp_doc "docs/getting-started/cpp.md" "QQuickWindow")

message(STATUS
    "HyRemote product documentation truth gate: PASS "
    "(four frontends, canonical layout, V0.1 Developer Preview identity, Generic in the deployment family, "
    "VeNCrypt/TLS kept on the V0.2 train)")
