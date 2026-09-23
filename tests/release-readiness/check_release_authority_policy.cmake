cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(workflow_path "${HYREMOTE_SOURCE_DIR}/.github/workflows/git-flow-policy.yml")
set(checklist_path "${HYREMOTE_SOURCE_DIR}/docs/internal/release-candidate-checklist.md")
set(authority_path "${HYREMOTE_SOURCE_DIR}/.github/release/release-trains.json")

foreach(required_file "${workflow_path}" "${checklist_path}" "${authority_path}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "release-authority-policy: required file is missing: ${required_file}")
    endif()
endforeach()

# The authority is one train map that a requested release version selects from. This gate proves the map still
# carries every authority it is supposed to: the version facts selection depends on, the 1.0.0.0 train that the
# former standalone V1 manifest became, and the 0.1.0.0 train that must not require cross-version umbrellas.
file(READ "${authority_path}" authority_json)
string(JSON authority_schema ERROR_VARIABLE authority_schema_error GET "${authority_json}" schema)
if(authority_schema_error)
    message(FATAL_ERROR "release-authority-policy: release-train authority is malformed: ${authority_schema_error}")
endif()
if(NOT authority_schema STREQUAL "3")
    message(FATAL_ERROR "release-authority-policy: unsupported release-train authority schema ${authority_schema}")
endif()

string(JSON sentinel GET "${authority_json}" development_sentinel)
if(NOT sentinel STREQUAL "0.0.0")
    message(FATAL_ERROR "release-authority-policy: development sentinel drifted: ${sentinel}")
endif()

string(JSON retired_count LENGTH "${authority_json}" retired_profiles)
math(EXPR retired_last "${retired_count} - 1")
set(actual_retired_profiles)
foreach(index RANGE 0 ${retired_last})
    string(JSON retired_entry GET "${authority_json}" retired_profiles ${index})
    list(APPEND actual_retired_profiles "${retired_entry}")
endforeach()
set(expected_retired_profiles 0.0.1.0 0.0.2.0 0.0.3.0)
if(NOT "${actual_retired_profiles}" STREQUAL "${expected_retired_profiles}")
    message(FATAL_ERROR
        "release-authority-policy: the retired planning labels must stay unreleasable. "
        "expected='${expected_retired_profiles}' actual='${actual_retired_profiles}'")
endif()

string(JSON scope_authority ERROR_VARIABLE scope_authority_error GET
       "${authority_json}" trains "1.0.0.0" scope_authority)
string(JSON freeze_authority ERROR_VARIABLE freeze_authority_error GET
       "${authority_json}" trains "1.0.0.0" candidate_freeze_authority)
if(scope_authority_error OR freeze_authority_error)
    message(FATAL_ERROR "release-authority-policy: the 1.0.0.0 train lost its scope/freeze authorities")
endif()
if(NOT scope_authority STREQUAL "157" OR NOT freeze_authority STREQUAL "165")
    message(FATAL_ERROR
        "release-authority-policy: scope/freeze authorities must remain #157/#165; "
        "found #${scope_authority}/#${freeze_authority}")
endif()

set(v1_authority_parent_expected 33)
string(JSON v1_authority_parent GET "${authority_json}" trains "1.0.0.0" authority_parent)
if(NOT v1_authority_parent STREQUAL "${v1_authority_parent_expected}")
    message(FATAL_ERROR
        "release-authority-policy: 1.0.0.0 release authority must remain #${v1_authority_parent_expected}; "
        "found #${v1_authority_parent}")
endif()

set(expected_v1_issues
    9 30 31 32 33 39 41 57 101 104 107 109 143 144 157 158 159 162 163 164 165 170 174 175 176 209)
string(JSON authority_count LENGTH "${authority_json}" trains "1.0.0.0" mandatory_children)
math(EXPR authority_last "${authority_count} - 1")
set(actual_v1_issues)
foreach(index RANGE 0 ${authority_last})
    string(JSON issue GET "${authority_json}" trains "1.0.0.0" mandatory_children ${index})
    list(APPEND actual_v1_issues "${issue}")
endforeach()

if(NOT "${actual_v1_issues}" STREQUAL "${expected_v1_issues}")
    message(FATAL_ERROR
        "release-authority-policy: the 1.0.0.0 train must carry the complete former V1 mandatory set. "
        "expected='${expected_v1_issues}' actual='${actual_v1_issues}'")
endif()

set(expected_embedded_deferred 7 10 17 18)

set(expected_classified_referenced 14 74 90 91 95 106 134 147 156 166)
string(JSON classified_count LENGTH "${authority_json}" trains "1.0.0.0" classified_referenced_issue_numbers)
math(EXPR classified_last "${classified_count} - 1")
set(actual_classified_referenced)
foreach(index RANGE 0 ${classified_last})
    string(JSON issue GET "${authority_json}" trains "1.0.0.0" classified_referenced_issue_numbers ${index})
    list(APPEND actual_classified_referenced "${issue}")
endforeach()
if(NOT "${actual_classified_referenced}" STREQUAL "${expected_classified_referenced}")
    message(FATAL_ERROR
        "release-authority-policy: classified-reference list drifted. "
        "expected='${expected_classified_referenced}' actual='${actual_classified_referenced}'")
endif()

# The train map must stay selectable for the trains the release lifecycle is allowed to name, and the conditional
# train must stay conditional: a version number existing never authorizes a release scope by itself.
foreach(required_train IN ITEMS 0.1.0.0 0.2.0.0 0.2.1.0 0.3.0.0 0.3.1.0 0.3.2.0 0.4.0.0 1.0.0.0 1.1.0.0)
    string(JSON required_train_type ERROR_VARIABLE required_train_error TYPE
           "${authority_json}" trains "${required_train}")
    if(required_train_error OR NOT required_train_type STREQUAL "OBJECT")
        message(FATAL_ERROR
            "release-authority-policy: release train ${required_train} is missing from the authority")
    endif()
endforeach()

string(JSON conditional_count ERROR_VARIABLE conditional_error LENGTH "${authority_json}" conditional_trains)
if(conditional_error OR conditional_count EQUAL 0)
    message(FATAL_ERROR "release-authority-policy: the conditional train record is missing")
endif()
math(EXPR conditional_last "${conditional_count} - 1")
set(conditional_versions "")
foreach(index RANGE 0 ${conditional_last})
    string(JSON entry_version GET "${authority_json}" conditional_trains ${index} version)
    string(JSON entry_active GET "${authority_json}" conditional_trains ${index} active)
    string(JSON entry_activation GET "${authority_json}" conditional_trains ${index} activation_authority)
    if(entry_version STREQUAL "1.2.0.0")
        if(NOT entry_activation STREQUAL "123")
            message(FATAL_ERROR
                "release-authority-policy: V1.2 activation authority must remain #123; found #${entry_activation}")
        endif()
        if(entry_active STREQUAL "true")
            message(FATAL_ERROR
                "release-authority-policy: V1.2 must not be active until its activation evidence exists")
        endif()
        string(JSON v12_train_type ERROR_VARIABLE v12_train_error TYPE "${authority_json}" trains "1.2.0.0")
        if(NOT v12_train_error AND v12_train_type STREQUAL "OBJECT")
            message(FATAL_ERROR
                "release-authority-policy: an inactive conditional train must not also exist as a selectable train")
        endif()
    endif()
    list(APPEND conditional_versions "${entry_version}")
endforeach()

# V0.1 passes its own release authority, exactly its own closeable children, and the umbrellas it merely consumes
# evidence from: this is the property the train map exists for, and it is asserted here so a later edit cannot
# quietly make an earlier train depend on later work.
string(JSON v01_authority_parent GET "${authority_json}" trains "0.1.0.0" authority_parent)
if(NOT v01_authority_parent STREQUAL "229")
    message(FATAL_ERROR
        "release-authority-policy: 0.1.0.0 release authority must remain #229; found #${v01_authority_parent}")
endif()
string(JSON v01_child_count LENGTH "${authority_json}" trains "0.1.0.0" mandatory_children)
math(EXPR v01_child_last "${v01_child_count} - 1")
set(actual_v01_children)
foreach(index RANGE 0 ${v01_child_last})
    string(JSON child GET "${authority_json}" trains "0.1.0.0" mandatory_children ${index})
    list(APPEND actual_v01_children "${child}")
endforeach()
set(expected_v01_children 230 231 232 237 238 253)
if(NOT "${actual_v01_children}" STREQUAL "${expected_v01_children}")
    message(FATAL_ERROR
        "release-authority-policy: 0.1.0.0 mandatory children drifted. "
        "expected='${expected_v01_children}' actual='${actual_v01_children}'")
endif()
# #253 is closeable V0.1 release-truth work: shipping a false encryption capability or support statement is itself a
# product-contract defect, so a selector that can accept V0.1 while #253 is open is incomplete.
list(FIND actual_v01_children 253 v01_truth_child)
if(v01_truth_child EQUAL -1)
    message(FATAL_ERROR "release-authority-policy: #253 must remain a 0.1.0.0 mandatory child")
endif()
# Umbrellas, later-train productization and this governance migration are never V0.1 closeable requirements.
foreach(umbrella IN ITEMS 41 143 176 209 240 241 263 266)
    list(FIND actual_v01_children "${umbrella}" umbrella_as_child)
    if(NOT umbrella_as_child EQUAL -1)
        message(FATAL_ERROR
            "release-authority-policy: #${umbrella} must not be a 0.1.0.0 mandatory child")
    endif()
endforeach()

# The candidate prerequisite is the other half of the V0.1 contract: #250 is repository evidence-integrity work that
# blocks candidate acceptance without being part of the product work breakdown, so it must appear as a prerequisite
# and must not be folded into the closeable children.
set(expected_v01_prerequisites 250)
string(JSON v01_prerequisite_count LENGTH "${authority_json}" trains "0.1.0.0" candidate_prerequisites)
math(EXPR v01_prerequisite_last "${v01_prerequisite_count} - 1")
set(actual_v01_prerequisites)
foreach(index RANGE 0 ${v01_prerequisite_last})
    string(JSON prerequisite GET "${authority_json}" trains "0.1.0.0" candidate_prerequisites ${index})
    list(APPEND actual_v01_prerequisites "${prerequisite}")
endforeach()
if(NOT "${actual_v01_prerequisites}" STREQUAL "${expected_v01_prerequisites}")
    message(FATAL_ERROR
        "release-authority-policy: 0.1.0.0 candidate prerequisites drifted. "
        "expected='${expected_v01_prerequisites}' actual='${actual_v01_prerequisites}'")
endif()
list(FIND actual_v01_children 250 v01_prerequisite_as_child)
if(NOT v01_prerequisite_as_child EQUAL -1)
    message(FATAL_ERROR
        "release-authority-policy: #250 is a candidate prerequisite and must not be a 0.1.0.0 mandatory child")
endif()
# The V0.1 evidence contract must not require bilingual/branded productization, which is V0.3 work.
string(JSON v01_evidence_count LENGTH "${authority_json}" trains "0.1.0.0" required_evidence)
math(EXPR v01_evidence_last "${v01_evidence_count} - 1")
set(actual_v01_evidence)
foreach(index RANGE 0 ${v01_evidence_last})
    string(JSON evidence GET "${authority_json}" trains "0.1.0.0" required_evidence ${index})
    list(APPEND actual_v01_evidence "${evidence}")
endforeach()
foreach(required_v01_evidence IN ITEMS
        "minimal-developer-entry-and-01-02-03-learning-flow"
        "truthful-v01-security-version-output"
        "nonzero-hosted-normal-and-release-readiness-tests")
    if(NOT required_v01_evidence IN_LIST actual_v01_evidence)
        message(FATAL_ERROR
            "release-authority-policy: 0.1.0.0 must require '${required_v01_evidence}'")
    endif()
endforeach()
foreach(forbidden_v01_evidence IN ITEMS "bilingual" "branded")
    foreach(evidence IN LISTS actual_v01_evidence)
        if(evidence MATCHES "${forbidden_v01_evidence}")
            message(FATAL_ERROR
                "release-authority-policy: 0.1.0.0 evidence '${evidence}' pulls V0.3 productization into V0.1")
        endif()
    endforeach()
endforeach()

# Every exact Feature must carry its own closeable set, prerequisites and lineage: the whole point of the migration is
# that 0.2.0.0/0.2.1.0 and 0.3.0.0/0.3.1.0/0.3.2.0 are distinct decisions rather than one another's placeholders.
# A semicolon cannot appear inside a foreach(ITEMS) element - it would split the element itself - so the issue lists
# below are written with commas and converted before they are compared against a CMake list.
foreach(exact_expectation IN ITEMS
        "0.2.0.0|143,174,259,326,271|258,332|0.1.0.0"
        "0.2.1.0|170,239||0.2.0.0"
        "0.3.0.0|240,241,264||0.2.1.0"
        "0.3.1.0|265|260|0.3.0.0"
        "0.3.2.0|144,175|261|0.3.1.0"
        "0.4.0.0|9,57,109,134,165,242||0.3.2.0")
    string(REPLACE "|" ";" exact_parts "${exact_expectation}")
    list(GET exact_parts 0 exact_version)
    list(GET exact_parts 1 exact_children)
    string(REPLACE "," ";" exact_children "${exact_children}")
    list(GET exact_parts 2 exact_prerequisites)
    string(REPLACE "," ";" exact_prerequisites "${exact_prerequisites}")
    list(GET exact_parts 3 exact_lineage)
    string(JSON exact_child_count LENGTH "${authority_json}" trains "${exact_version}" mandatory_children)
    math(EXPR exact_child_last "${exact_child_count} - 1")
    set(seen_exact_children)
    foreach(index RANGE 0 ${exact_child_last})
        string(JSON child GET "${authority_json}" trains "${exact_version}" mandatory_children ${index})
        list(APPEND seen_exact_children "${child}")
    endforeach()
    if(NOT "${seen_exact_children}" STREQUAL "${exact_children}")
        message(FATAL_ERROR
            "release-authority-policy: ${exact_version} closeable set drifted. "
            "expected='${exact_children}' actual='${seen_exact_children}'")
    endif()
    string(JSON exact_prerequisite_count LENGTH
           "${authority_json}" trains "${exact_version}" candidate_prerequisites)
    math(EXPR exact_prerequisite_last "${exact_prerequisite_count} - 1")
    set(seen_exact_prerequisites)
    if(exact_prerequisite_count GREATER 0)
        foreach(index RANGE 0 ${exact_prerequisite_last})
            string(JSON prerequisite GET "${authority_json}" trains "${exact_version}" candidate_prerequisites ${index})
            list(APPEND seen_exact_prerequisites "${prerequisite}")
        endforeach()
    endif()
    if(NOT "${seen_exact_prerequisites}" STREQUAL "${exact_prerequisites}")
        message(FATAL_ERROR
            "release-authority-policy: ${exact_version} candidate prerequisites drifted. "
            "expected='${exact_prerequisites}' actual='${seen_exact_prerequisites}'")
    endif()
    string(JSON actual_exact_lineage ERROR_VARIABLE exact_lineage_error GET
           "${authority_json}" trains "${exact_version}" lineage_parent)
    if(exact_lineage_error)
        set(actual_exact_lineage "")
    endif()
    if(NOT actual_exact_lineage STREQUAL "${exact_lineage}")
        message(FATAL_ERROR
            "release-authority-policy: ${exact_version} lineage drifted. "
            "expected='${exact_lineage}' actual='${actual_exact_lineage}'")
    endif()
endforeach()

# Preflight is risk work, not product implementation: it is classified as a prerequisite, never as a Feature child.
foreach(preflight_expectation IN ITEMS "0.2.0.0;258" "0.3.1.0;260" "0.3.2.0;261")
    list(GET preflight_expectation 0 preflight_version)
    list(GET preflight_expectation 1 preflight_issue)
    list(FIND seen_exact_children "${preflight_issue}" preflight_placeholder)
    string(JSON preflight_count LENGTH "${authority_json}" trains "${preflight_version}" candidate_prerequisites)
    math(EXPR preflight_last "${preflight_count} - 1")
    set(preflight_found FALSE)
    foreach(index RANGE 0 ${preflight_last})
        string(JSON prerequisite GET "${authority_json}" trains "${preflight_version}" candidate_prerequisites ${index})
        if(prerequisite STREQUAL "${preflight_issue}")
            set(preflight_found TRUE)
        endif()
    endforeach()
    if(NOT preflight_found)
        message(FATAL_ERROR
            "release-authority-policy: preflight #${preflight_issue} must be a ${preflight_version} candidate "
            "prerequisite")
    endif()
    string(JSON preflight_child_count LENGTH "${authority_json}" trains "${preflight_version}" mandatory_children)
    math(EXPR preflight_child_last "${preflight_child_count} - 1")
    foreach(index RANGE 0 ${preflight_child_last})
        string(JSON child GET "${authority_json}" trains "${preflight_version}" mandatory_children ${index})
        if(child STREQUAL "${preflight_issue}")
            message(FATAL_ERROR
                "release-authority-policy: preflight #${preflight_issue} must not be a ${preflight_version} "
                "mandatory child")
        endif()
    endforeach()
endforeach()

# The first customer trial is a V0.2.0 decision, not a V0.1 one.
string(JSON v020_child_count LENGTH "${authority_json}" trains "0.2.0.0" mandatory_children)
math(EXPR v020_child_last "${v020_child_count} - 1")
set(v020_children)
foreach(index RANGE 0 ${v020_child_last})
    string(JSON child GET "${authority_json}" trains "0.2.0.0" mandatory_children ${index})
    list(APPEND v020_children "${child}")
endforeach()
list(FIND v020_children 271 v020_customer_trial)
if(v020_customer_trial EQUAL -1)
    message(FATAL_ERROR "release-authority-policy: #271 customer trial must be a 0.2.0.0 mandatory child")
endif()

# V0.4 is qualification, not a hidden Feature train: the V0.3 implementation children must not reappear as V0.4 work.
string(JSON v040_child_count LENGTH "${authority_json}" trains "0.4.0.0" mandatory_children)
math(EXPR v040_child_last "${v040_child_count} - 1")
foreach(index RANGE 0 ${v040_child_last})
    string(JSON child GET "${authority_json}" trains "0.4.0.0" mandatory_children ${index})
    if(child IN_LIST v020_children OR child EQUAL 264 OR child EQUAL 265 OR child EQUAL 144 OR child EQUAL 175
       OR child EQUAL 240 OR child EQUAL 241)
        message(FATAL_ERROR
            "release-authority-policy: #${child} is V0.2/V0.3 implementation work and must not be re-developed "
            "under the V0.4 qualification Feature")
    endif()
endforeach()
string(JSON v01_reference_count LENGTH "${authority_json}" trains "0.1.0.0" cross_version_references)
math(EXPR v01_reference_last "${v01_reference_count} - 1")
set(actual_v01_references)
foreach(index RANGE 0 ${v01_reference_last})
    string(JSON reference GET "${authority_json}" trains "0.1.0.0" cross_version_references ${index})
    list(APPEND actual_v01_references "${reference}")
endforeach()
list(FIND actual_v01_references 209 v01_references_umbrella)
if(v01_references_umbrella EQUAL -1)
    message(FATAL_ERROR
        "release-authority-policy: 0.1.0.0 must reference the cross-version umbrellas whose evidence it consumes")
endif()

# Anti-leak: every issue number mentioned by an in-tree release authority must be classified somewhere in the train
# map, so a blocker cannot silently exist outside the machine authority. The known set is the union of what every
# train classifies, which keeps the rule train-aware without weakening it to a single hard-coded V1 list.
set(declared_authority_documents
    docs/internal/release-candidate-checklist.md
    docs/internal/v1-ga-acceptance.md
    docs/internal/development-roadmap.md
    docs/known-limitations.md
    docs/compatibility.md
    docs/internal/v1-physical-acceptance.md
    docs/security-model.md)
set(known_issue_numbers
    ${expected_v1_issues} ${expected_embedded_deferred} ${expected_classified_referenced})
string(JSON trains_type GET "${authority_json}" trains)
string(REGEX MATCHALL "\"[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+\"[ \t]*:" _train_keys "${trains_type}")
foreach(_train_key IN LISTS _train_keys)
    string(REGEX REPLACE "[^0-9.]" "" train_version "${_train_key}")
    string(JSON train_parent GET "${authority_json}" trains "${train_version}" authority_parent)
    list(APPEND known_issue_numbers "${train_parent}")
    foreach(bucket IN ITEMS mandatory_children cross_version_references non_blockers conditional_items
                        embedded_deferred_issue_numbers classified_referenced_issue_numbers)
        string(JSON bucket_count ERROR_VARIABLE bucket_error LENGTH
               "${authority_json}" trains "${train_version}" ${bucket})
        if(bucket_error OR bucket_count EQUAL 0)
            continue()
        endif()
        math(EXPR bucket_last "${bucket_count} - 1")
        foreach(index RANGE 0 ${bucket_last})
            string(JSON entry GET "${authority_json}" trains "${train_version}" ${bucket} ${index})
            list(APPEND known_issue_numbers "${entry}")
        endforeach()
    endforeach()
endforeach()
list(APPEND known_issue_numbers 123)
list(REMOVE_DUPLICATES known_issue_numbers)
foreach(document IN LISTS declared_authority_documents)
    set(document_path "${HYREMOTE_SOURCE_DIR}/${document}")
    if(NOT EXISTS "${document_path}")
        message(FATAL_ERROR "release-authority-policy: declared authority document is missing: ${document}")
    endif()
    file(READ "${document_path}" document_text)
    string(REGEX MATCHALL "#[0-9]+" mentioned "${document_text}")
    foreach(token IN LISTS mentioned)
        string(REGEX REPLACE "#" "" number "${token}")
        if(NOT number IN_LIST known_issue_numbers)
            message(FATAL_ERROR
                "release-authority-policy: unclassified issue reference #${number} in ${document}; "
                "either classify it in the release train that owns it or remove the reference from a declared "
                "authority document (.github/release/release-trains.json)")
        endif()
    endforeach()
endforeach()

string(JSON deferred_count LENGTH "${authority_json}" trains "1.0.0.0" embedded_deferred_issue_numbers)
math(EXPR deferred_last "${deferred_count} - 1")
set(actual_embedded_deferred)
foreach(index RANGE 0 ${deferred_last})
    string(JSON issue GET "${authority_json}" trains "1.0.0.0" embedded_deferred_issue_numbers ${index})
    list(APPEND actual_embedded_deferred "${issue}")
endforeach()
if(NOT "${actual_embedded_deferred}" STREQUAL "${expected_embedded_deferred}")
    message(FATAL_ERROR
        "release-authority-policy: embedded-only defer list drifted. "
        "expected='${expected_embedded_deferred}' actual='${actual_embedded_deferred}'")
endif()

file(READ "${workflow_path}" policy)
foreach(required_token
        [=[issues: read]=]
        [=[pull-requests: read]=]
        [=[mainline-push-audit]=]
        [=[after-the-fact audit only]=]
        [=[cannot undo a direct push]=]
        [=[must never be described as branch protection]=]
        [=[Require accepted milestone authorities before release PR]=]
        [=[Resolve requested release train]=]
        [=[.github/release/release-trains.json]=]
        [=[RELEASE_TRAIN_MANDATORY]=]
        [=[RELEASE_TRAIN_PREREQUISITES]=]
        [=[RELEASE_TRAIN_LINEAGE]=]
        [=[PRODUCT CHILD]=]
        [=[CANDIDATE PREREQUISITE]=]
        [=[candidate_prerequisites list]=]
        [=[lineage_parent]=]
        [=[is a retired planning label, not a release train]=]
        [=[not an authorized release train]=]
        [=[activation authority]=]
        [=[state}" != "closed"]=]
        [=[reason}" == "not_planned"]=]
        [=[## Security boundary]=]
        [=[must be annotated]=]
        [=[exact current main HEAD]=]
        [=[backmerge requires annotated]=])
    string(FIND "${policy}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-authority-policy: Git Flow lost required authority invariant: ${required_token}")
    endif()
endforeach()

# The commit-to-pull-request association is the only evidence the mainline push audit accepts. That logic now lives in
# the audit script the workflow calls, so both halves stay pinned: the script must still prove the association, and the
# workflow must still call the script. A local commit shape is never evidence, and the audit cannot be unplugged.
set(mainline_audit_script "${HYREMOTE_SOURCE_DIR}/.github/scripts/mainline-push-audit.sh")
if(NOT EXISTS "${mainline_audit_script}")
    message(FATAL_ERROR "release-authority-policy: mainline push audit script is missing")
endif()
file(READ "${mainline_audit_script}" mainline_audit_body)
foreach(mainline_audit_token IN ITEMS
        [=[/commits/${sha}/pulls]=]
        [=[select(.merged_at != null and .base.ref ==]=])
    string(FIND "${mainline_audit_body}" "${mainline_audit_token}" audit_found)
    if(audit_found EQUAL -1)
        message(FATAL_ERROR
            "release-authority-policy: mainline push audit lost its PR-association invariant: ${mainline_audit_token}")
    endif()
endforeach()
string(FIND "${policy}" "mainline-push-audit.sh" policy_calls_audit)
if(policy_calls_audit EQUAL -1)
    message(FATAL_ERROR "release-authority-policy: the Git Flow policy no longer calls the mainline push audit")
endif()

# Release authorization may read issue state but must never manufacture acceptance or rewrite history.
foreach(forbidden_token
        [=[gh issue close]=]
        [=[-X PATCH]=]
        [=[--method PATCH]=]
        [=[git reset --hard]=]
        [=[git push --force]=])
    string(FIND "${policy}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "release-authority-policy: workflow must not manufacture acceptance/rollback: ${forbidden_token}")
    endif()
endforeach()

file(READ "${checklist_path}" checklist)
foreach(required_phrase
        [=[closed as completed]=]
        [=[`not_planned` is not release acceptance]=]
        [=[`.github/release/release-trains.json`]=]
        [=[CONVERGING]=]
        [=[RC-FROZEN]=]
        [=[PHYSICAL-ACCEPTANCE]=]
        [=[Editing release notes or toggling PR Draft state cannot substitute for authority closure]=]
        [=[release branch is therefore a versioned tests/finalization line]=]
        [=[after-the-fact direct-push audit]=]
        [=[cannot undo a push]=]
        [=[must not be described as branch protection]=]
        [=[The canonical repository execution/evidence template for #109 is `docs/internal/v1-physical-acceptance.md`]=]
        [=[same RC-FROZEN SHA]=]
        [=[## Security boundary]=])
    string(FIND "${checklist}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-authority-policy: canonical release checklist drifted: ${required_phrase}")
    endif()
endforeach()

message(STATUS
    "HyRemote release-authority policy gate: PASS "
    "(V1 manifest=${actual_v1_issues}; scope=#157; freeze=#165; "
    "issue-backed acceptance precedes release; physical evidence is RC-FROZEN-SHA bound; "
    "mainline push audit remains detection-only; tag/backmerge facts preserved)")
