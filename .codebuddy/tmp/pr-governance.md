## What this changes

The product-owner scope review of 2026-09-19 confirmed four further V1.0.0.0 blockers and required the single machine-readable manifest - and every authority that repeats it - to include them, with a regression check so a future V1-labelled blocker cannot silently exist outside the manifest.

**Mandatory set: 21 -> 25 entries**, adding:

- **#170** secure configuration and authenticated-session operations;
- **#174** listener address-family and reachability contract;
- **#175** maintained-viewer interoperability against the final secure/compressed profile;
- **#176** reproducible Windows/Linux SDK release assets with integrity evidence.

**A classification list, not a weakener.** The manifest also gains `classified_referenced_issue_numbers` covering the numbers that appear in the release authorities without being V1 work items (14, 74, 90, 91, 95, 106, 134, 147, 156, 166), each with its reason in `notes`. Classifying them is deliberate: it is what makes the next unclassified number visible.

**The gate now enforces both.** `tests/release-readiness/check_release_authority_policy.cmake` pins the classified list as well, and scans seven in-tree authority documents for issue references - `docs/release-candidate-checklist.md`, `docs/v1-ga-acceptance.md`, `docs/development-roadmap.md`, `docs/known-limitations.md`, `docs/compatibility.md`, `docs/v1-physical-acceptance.md`, `docs/security-model.md`. Any number that is neither mandatory, embedded-deferred nor classified fails the gate and is named in the failure.

## Verification, run locally on this branch

- all **ten** standalone release-readiness gates PASS, and the authority gate reports the 25-entry manifest
  (`manifest=9;...;165;170;174;175;176;`);
- **negative test**: appending a fabricated `#999` reference to `docs/security-model.md` makes the gate **fail** and name
  the offending number and its document; restoring the document makes it **pass** again, with byte-identical content.

## What this does not do

It does not touch the frozen three-mode / one-runtime architecture, and it does not make any of the four new blockers
pass - they are now **visible to the gate**, which is precisely what was missing. `#157` was closed earlier the same day
after its five criteria were verified as they then stood; the review's correction arrived afterwards, so that close was
premature, and `#157` has been reopened and this is the first part of the correction.

Refs #157 #170 #174 #175 #176 #33 #95 #107 #106 #156
