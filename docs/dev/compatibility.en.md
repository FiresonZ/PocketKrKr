# Compatibility Testing

## Test Loop

```text
Prepare resources → Run → Record observations → Classify and locate → Fix → Regress
```

Test records must include at least the platform, system version, build type, resource type, reproduction steps, actual result, and expected result. Keep only problem-related log excerpts; do not submit personal paths or complete resources.

## Categories and Entry Points

| Symptom | Investigation location |
|---|---|
| Script error or different behavior | `cpp/core/tjs2/`, `cpp/core/base/` |
| Archive or resource read failure | `cpp/core/base/` |
| Corrupted display, color shift, or missing image | `cpp/core/visual/`, `cpp/plugins/psbfile/` |
| Missing animation frames or deformation error | `cpp/plugins/motionplayer/` |
| Missing glyphs or misalignment | `cpp/core/visual/` |
| Audio or video abnormality | `cpp/core/sound/`, `cpp/core/movie/` |
| Missing plugin interface | `cpp/plugins/`, ncbind registry |
| Black screen or frozen frame | [Rendering Diagnosis](rendering-diagnosis.md) |

## Regression Requirements

- When fixing script, save, resource format, or lifecycle issues, check creation, restart, exceptional, and destruction paths.
- When modifying pixel blending, run scalar and SIMD comparisons together.
- When modifying textures or layers, check different resolutions, alpha values, clipping, and scaling.
- When modifying fonts, check pre-rendered glyphs, runtime glyphs, full-width characters, and mixed-script text.
- Results must be based on code state and reproducible verification; compatibility cannot be judged from a single screenshot alone.

## External Behavior Comparison

When behavior comparison is required, use the unified entry points in [Compatibility and References](krkrz-compat.md). External runtime results may only be used for comparison and cannot replace this project's automation or target-platform verification.
