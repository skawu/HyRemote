# Acceptance evidence

`docs/repository-layout.md` puts recorded acceptance/review evidence here: one directory per candidate or version,
next to the runbook that produced it. Evidence is documentation, not a test case - the distinction is deliberate, so
that "a test exists" is never confused with "the product was accepted on real hardware".

Convention:

- one directory per candidate or version, named for it (for example `docs/acceptance/v1.0.0.0/`);
- the directory holds what the run produced: logs, screenshots, measurement tables, the exact environment;
- the runbook that produced it stays where it is (`docs/v1-physical-acceptance.md`, `docs/v1-ga-acceptance.md`) and is
  linked from the evidence directory rather than duplicated there;
- nothing in here is installed, packaged or compiled - it is not part of any build graph.

This directory exists because the layout document named it while it did not exist: a documented location that a reader
cannot open is a defect in the documentation, not a detail. It is created empty on purpose - adding fabricated evidence
would be worse than having nowhere to put real evidence.
