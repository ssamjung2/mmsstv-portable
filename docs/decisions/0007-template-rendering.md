# 0007: Templates render in the core, not in front ends

- Status: Proposed
- Date: 2026-09-19

## Context

A template is an overlay carrying the operator's callsign, the other station's
callsign, a report and decoration. MMSSTV composited it with a transparent
colour key. The same template must produce identical pixels on a Pi, a
desktop and a phone (`R-IMG-7`), and callsign and comment text may be
non-Latin.

## Decision

Templates are documents (layers, geometry, styles, and text bound to QSO
fields) stored as JSON and rendered to RGB **in the core**, with real alpha
compositing rather than colour keying. Rendering uses a vector rasteriser with
proper text shaping, so scripts beyond Latin work.

Front ends display the rendered result and send edits back as document
changes; they never draw the overlay themselves.

## Consequences

- One renderer to test, and templates that look the same everywhere.
- Rendering on demand costs CPU on the Pi; results are cached per QSO context.
- The template editor in each front end is a property editor over a document,
  which is far less work than three drawing engines.
- We must choose the rasteriser and text stack deliberately: candidates are
  Cairo with Pango, Blend2D, and a vendored SVG rasteriser with HarfBuzz and
  FreeType. The choice hinges on iOS build size and CJK shaping quality, and
  should be settled with a spike before the template milestone.
