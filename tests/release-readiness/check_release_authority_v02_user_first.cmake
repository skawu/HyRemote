# Machine guard for the V0.2.0 release authority after #323.
#
# The authority is the user promise, not the implementation plan. 0.2.0.0 accepts a BASIC_TRUSTED_LAN candidate - the
# first real remote-access trial on a trusted LAN - and requires encrypted-profile evidence only from a candidate that
# actually declares AUTHENTICATED_ENCRYPTED. This test holds that contract, the classifications it must not lose, and
# the untouched state of every other train, in one place that a release-authority change cannot pass without.
#
#   A. the closeable set is exactly 143,174,259,271
#   B. #258 is a candidate prerequisite and never enters the closeable set
#   C. #170 and #239 remain declared non-blockers rather than children
#   D. no required evidence is an encrypted-profile requirement
#   E. the required evidence names the LAN trial, the truthful security state and the customer trial
#   F. every other train is unchanged

cmake_policy(SET CMP0057 NEW)  # IN_LIST membership, as written below

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(authority_path "${HYREMOTE_SOURCE_DIR}/.github/release/release-trains.json")
if(NOT EXISTS "${authority_path}")
    message(FATAL_ERROR "release-authority-v02-user-first: ${authority_path} is missing")
endif()
file(READ "${authority_path}" authority_json)

string(JSON v020_parent ERROR_VARIABLE parent_error GET "${authority_json}" trains "0.2.0.0" authority_parent)
if(parent_error)
    message(FATAL_ERROR "release-authority-v02-user-first: 0.2.0.0 is not present in the authority document")
endif()

function(_list version field out_var)
    string(JSON count ERROR_VARIABLE count_error LENGTH "${authority_json}" trains "${version}" "${field}")
    if(count_error)
        message(FATAL_ERROR "release-authority-v02-user-first: ${version} declares no ${field}")
    endif()
    set(seen)
    if(count GREATER 0)
        # An empty declared list is a real answer here (a train with no evidence of its own), and RANGE 0 -1 would
        # still visit index 0, so the empty case is answered before the loop rather than by it.
        math(EXPR last "${count} - 1")
        foreach(index RANGE 0 ${last})
            string(JSON entry GET "${authority_json}" trains "${version}" "${field}" ${index})
            list(APPEND seen "${entry}")
        endforeach()
    endif()
    set(${out_var} "${seen}" PARENT_SCOPE)
endfunction()

# A. The closeable set is exactly the four accepted children.
_list("0.2.0.0" mandatory_children v020_children)
if(NOT "${v020_children}" STREQUAL "143;174;259;271")
    message(FATAL_ERROR
        "release-authority-v02-user-first: 0.2.0.0 closeable set drifted. expected='143;174;259;271' "
        "actual='${v020_children}'")
endif()

# B. #258 is risk work that gates the candidate; it is not product implementation.
_list("0.2.0.0" candidate_prerequisites v020_prerequisites)
if(NOT "${v020_prerequisites}" STREQUAL "258")
    message(FATAL_ERROR
        "release-authority-v02-user-first: 0.2.0.0 candidate prerequisites drifted. expected='258' "
        "actual='${v020_prerequisites}'")
endif()
if("258" IN_LIST v020_children)
    message(FATAL_ERROR
        "release-authority-v02-user-first: #258 is a candidate prerequisite and must not be a closeable child")
endif()

# C. Later-train work stays a declared non-blocker, not a requirement of this trial.
_list("0.2.0.0" non_blockers v020_non_blockers)
if(NOT "${v020_non_blockers}" STREQUAL "170;239")
    message(FATAL_ERROR
        "release-authority-v02-user-first: 0.2.0.0 non-blockers drifted. expected='170;239' "
        "actual='${v020_non_blockers}'")
endif()
foreach(later_train_item IN ITEMS 170 239)
    if("${later_train_item}" IN_LIST v020_children)
        message(FATAL_ERROR
            "release-authority-v02-user-first: #${later_train_item} is a declared non-blocker of 0.2.0.0 and must not "
            "become a closeable child")
    endif()
endforeach()

_list("0.2.0.0" required_evidence v020_evidence)

# D. The first candidate is not an encrypted-profile delivery, so no required evidence may be one. The encrypted
#    profile is a condition on a later claim, expressed in the rule, never a standing requirement of this train.
if(v020_evidence STREQUAL "")
    message(FATAL_ERROR "release-authority-v02-user-first: 0.2.0.0 requires no evidence at all")
endif()
foreach(evidence_name IN LISTS v020_evidence)
    string(TOLOWER "${evidence_name}" lowered_evidence)
    foreach(banned_fragment IN ITEMS tls vencrypt encrypt secure-remote-evidence)
        string(FIND "${lowered_evidence}" "${banned_fragment}" banned_at)
        if(NOT banned_at EQUAL -1)
            message(FATAL_ERROR
                "release-authority-v02-user-first: 0.2.0.0 requires '${evidence_name}', which makes an "
                "encrypted-profile delivery a condition of the first LAN trial. Encrypted evidence must be conditional "
                "on the candidate's own shipped claim instead.")
        endif()
    endforeach()
endforeach()

# E. What the trial actually needs is named, and named by outcome.
foreach(required_outcome IN ITEMS
        usable-lan-remote-access-evidence
        truthful-trial-security-state-evidence
        customer-trial-evidence)
    if(NOT "${required_outcome}" IN_LIST v020_evidence)
        message(FATAL_ERROR
            "release-authority-v02-user-first: 0.2.0.0 no longer requires '${required_outcome}'. "
            "actual='${v020_evidence}'")
    endif()
endforeach()

# The rule has to say which candidate states exist and which one is acceptable first, so the classification cannot be
# read out of the evidence list alone.
string(JSON v020_rule ERROR_VARIABLE rule_error GET "${authority_json}" trains "0.2.0.0" rule)
if(rule_error)
    message(FATAL_ERROR "release-authority-v02-user-first: 0.2.0.0 declares no rule")
endif()
string(TOLOWER "${v020_rule}" lowered_rule)
foreach(required_statement IN ITEMS basic_trusted_lan authenticated_encrypted)
    string(FIND "${lowered_rule}" "${required_statement}" statement_at)
    if(statement_at EQUAL -1)
        message(FATAL_ERROR
            "release-authority-v02-user-first: the 0.2.0.0 rule must name '${required_statement}' so that the first "
            "candidate's accepted state and the conditional encrypted state are both explicit")
    endif()
endforeach()

# F. Every other train keeps its product authority. The snapshots below are the values that were in place when the
#    0.2.0.0 decision was aligned, so any drift in another release is a failure here rather than a quiet edit.
foreach(unchanged_expectation IN ITEMS
        "0.1.0.0|230,231,232,237,238,253|250|-|converged-one-core-shared-runtime-four-peer-frontends,exact-head-integrated-windows-linux-evidence,clean-installed-cpp-consumer-smoke,clean-installed-generic-consumer-smoke-preserving-native-qpa,minimal-developer-entry-and-01-02-03-learning-flow,truthful-v01-security-version-output,fail-closed-security-negative-evidence,release-scope-machine-gate-selecting-only-this-train,nonzero-hosted-normal-and-release-readiness-tests,release-notes-preview-support-boundaries-and-known-limitations"
        "0.2.1.0|170,239|-|0.2.0.0|accepted-v0.2.0.0-lineage,session-operations-evidence"
        "0.3.0.0|240,241,264|-|0.2.1.0|accepted-v0.2.1.0-lineage,four-frontend-clean-sdk-deploy-productization-evidence,example-and-localization-productization-evidence,productized-gui-branding-evidence"
        "0.3.1.0|265|260|0.3.0.0|accepted-v0.3.0.0-lineage,qt5-adaptation-candidate-evidence"
        "0.3.2.0|144,175|261|0.3.1.0|accepted-v0.3.1.0-lineage,damage-compression-delivery-evidence,maintained-viewer-interoperability-evidence"
        "0.4.0.0|9,57,109,134,165,242|-|0.3.2.0|accepted-v0.3.2.0-lineage,qt-compatibility-qualification-evidence,production-performance-qualification-evidence,real-world-verification-evidence,physical-native-acceptance-evidence,exact-candidate-freeze-under-165"
        "1.0.0.0|9,30,31,32,33,39,41,57,101,104,107,109,143,144,157,158,159,162,163,164,165,170,174,175,176,209|-|-|accepted-milestone-authorities,exact-candidate-freeze-under-165,physical-acceptance-under-109,ga-acceptance-under-33"
        "1.1.0.0|-|-|-|-"
)
    string(REPLACE "|" ";" unchanged_parts "${unchanged_expectation}")
    list(GET unchanged_parts 0 unchanged_version)
    list(GET unchanged_parts 1 unchanged_children)
    list(GET unchanged_parts 2 unchanged_prerequisites)
    list(GET unchanged_parts 3 unchanged_lineage)
    list(GET unchanged_parts 4 unchanged_evidence)
    foreach(unchanged_field IN ITEMS mandatory_children candidate_prerequisites required_evidence)
        _list("${unchanged_version}" "${unchanged_field}" actual_values)
        if(unchanged_field STREQUAL "mandatory_children")
            set(expected_values "${unchanged_children}")
        elseif(unchanged_field STREQUAL "candidate_prerequisites")
            set(expected_values "${unchanged_prerequisites}")
        else()
            set(expected_values "${unchanged_evidence}")
        endif()
        string(REPLACE "," ";" expected_values "${expected_values}")
        if(expected_values STREQUAL "-")
            set(expected_values "")
        endif()
        if(NOT "${actual_values}" STREQUAL "${expected_values}")
            message(FATAL_ERROR
                "release-authority-v02-user-first: ${unchanged_version} ${unchanged_field} changed. "
                "expected='${expected_values}' actual='${actual_values}'")
        endif()
    endforeach()
    string(JSON actual_lineage ERROR_VARIABLE lineage_error GET
           "${authority_json}" trains "${unchanged_version}" lineage_parent)
    if(lineage_error)
        set(actual_lineage "-")
    endif()
    if(NOT actual_lineage STREQUAL "${unchanged_lineage}")
        message(FATAL_ERROR
            "release-authority-v02-user-first: ${unchanged_version} lineage changed. "
            "expected='${unchanged_lineage}' actual='${actual_lineage}'")
    endif()
endforeach()

message(STATUS
    "release-authority-v02-user-first: PASS (0.2.0.0 = 143,174,259,271 with prerequisite 258 and non-blockers "
    "170,239; the LAN trial, the truthful security state and the customer trial are required; no encrypted profile is "
    "required of the first candidate; every other train unchanged)")
