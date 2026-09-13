# Text Baseline Contract

## Purpose

Define the shared data relationships for bitmap and runtime fonts during measurement, drawing, clipping, and scaling. This page records an abstract contract and does not bind it to a game, font file, or platform implementation.

## Unified Glyph Data

Before drawing, every character must be converted to one internal record:

```text
bitmap:
  width, height
origin:
  x, y
advance:
  x, y
```

The actual glyph rectangle is computed from that same record:

```text
glyph.left   = draw.x + origin.x
glyph.top    = draw.y + origin.y
glyph.right  = glyph.left + bitmap.width
glyph.bottom = glyph.top + bitmap.height
```

## Data Sources

- Bitmap fonts use the glyph size, origin, and advance stored in the font file.
- Runtime fonts use glyph size, bearing, and advance produced by the rasterizer.
- Missing-glyph fallback replaces only the glyph data source; it does not change the caller's coordinate contract.
- Measurement, top/bottom boundary checks, and final drawing must reuse the same internal glyph-data path.

## Baseline Requirements

- Glyph origins must be defined relative to the same drawing baseline.
- Do not add the fallback font's ascent to a bitmap glyph that already contains the source-font baseline.
- Rotation, shadows, and outlines may only add explicit offsets or expanded rectangles.
- Secondary scaling must use complete source and destination rectangles; a baseline coordinate must not be treated as the destination rectangle's top-left corner.
- When a glyph is taller than the clip region, use a deterministic alignment policy and avoid reversed or zero-size rectangles.

## Acceptance

- Measurement width and actual advance agree for bitmap hits, missing-glyph fallback, and ordinary runtime fonts.
- `getTextWidth`, glyph draw rect, clipping, and final drawing produce consistent geometry for the same character.
- Full-width characters, mixed text, shadows, rotation, negative coordinates, and small clip regions are reproducibly testable.
- No game-name, layer-name, archive-name, or fixed-size special case is required.
