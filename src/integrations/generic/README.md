# Generic Plugin integration

This directory is reserved for the QGenericPlugin zero-code integration frontend defined by ADR-0007.

It must remain a thin bootstrap over the common runtime in `src/runtime` and must not own capture, input, transport, surface-composition, or Core behavior. The implementation is introduced by issue #219; this marker establishes repository ownership without claiming the payload is complete.
