# RFB opaque presentation policy (2026-10-24)

## Decision

RFB true-colour pixels carry no alpha. HyRemote uses one deterministic **black matte** policy for
both Raw and TRLE:

- `AlphaMode::Opaque`: channels are unchanged.
- `AlphaMode::Premultiplied`: RGB is already composited over black, so channels are unchanged.
- `AlphaMode::Straight`: each RGB channel is multiplied by `alpha / 255` before encoding.

The same `rfbOpaqueChannel` helper is used by Raw and TRLE. This prevents the two encoders from
silently disagreeing on transparent pixels.

## Boundary

- This is a presentation policy, not a new codec or a second compositor.
- It does not claim capture of windows outside the captured target.
- A non-black matte or a different transparent-surface contract requires a separate Owner decision.
- Opaque surfaces and existing wire tests are unchanged.

## Follow-up

Add deterministic 2x2 fixtures for Opaque, Straight and Premultiplied with alpha 0/128/255 and
decode both Raw and TRLE to assert the same expected RGB. That test is not claimed by this decision
record.
