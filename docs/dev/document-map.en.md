# Documentation Map

This page is the single classification entry for `docs/dev/`. Before adding a document, choose its category and reuse the existing canonical page. Keep each fact in one authoritative source.

## Categories

| Category | Purpose | Current entry |
|---|---|---|
| Orientation and facts | Project structure, technology stack, and current architecture facts | [getting-started.en.md](getting-started.en.md), [tech-stack.en.md](tech-stack.en.md), [architecture.en.md](architecture.en.md) |
| Source and interfaces | File responsibilities, key symbols, C ABI, and module boundaries | [source-map.en.md](source-map.en.md), [key-references.en.md](key-references.en.md), [api-contracts.en.md](api-contracts.en.md) |
| Build and platforms | Toolchains, presets, artifacts, CI, and platform differences | [build.en.md](build.en.md) |
| Development rules | Pre-change constraints, lifecycle, rendering, linking, and documentation rules | [conventions.en.md](conventions.en.md), [documentation-rules.en.md](documentation-rules.en.md) |
| Compatibility | Current capabilities, game regression, plugin gaps, and external implementation comparison | [compatibility.en.md](compatibility.en.md), [plugin-compatibility.en.md](plugin-compatibility.en.md), [aetherkiri-audit.en.md](aetherkiri-audit.en.md), [krkrz-compat.en.md](krkrz-compat.en.md) |
| Tests and acceptance | Reproducible fixtures, regression matrices, and acceptance conditions | [test-fixtures.en.md](test-fixtures.en.md), [compatibility.en.md](compatibility.en.md) |
| Diagnostics and incidents | Probes, rendering diagnosis, and structured issue records | [probes.en.md](probes.en.md), [rendering-diagnosis.en.md](rendering-diagnosis.en.md), [incident-reports.en.md](incident-reports.en.md) |
| Tools and analysis | Extraction, TJS2 decompilation, and developer tools | [tools.en.md](tools.en.md) |
| Planning and optimization | Current todos, approved roadmaps, and performance optimization | [todo.en.md](todo.en.md), [optimization-roadmap.en.md](optimization-roadmap.en.md), [perf-optimization.en.md](perf-optimization.en.md) |
| Releases and changes | Versioned change summaries and compatibility notes | [release-notes.en.md](release-notes.en.md) |

## Boundaries Between Similar Topics

- `compatibility.en.md` records the test loop and regression requirements, not the external reference catalog.
- `krkrz-compat.en.md` records external reference entry points and project compatibility boundaries, not individual game investigations.
- `plugin-compatibility.en.md` records plugin status, gaps, compatibility adapters, and plugin references only.
- `aetherkiri-audit.en.md` records the itemized AetherKiri comparison and does not replace the general compatibility documents.
- `todo.en.md` records unfinished work only; verified items should be removed or moved into a facts document.
- `optimization-roadmap.en.md` records confirmed long-term directions; `perf-optimization.en.md` records performance evaluation rules and acceptance methods.
- `probes.en.md` records the probe inventory; `rendering-diagnosis.en.md` records how to use probes to classify a problem.
- `architecture.en.md` records system data flow; `source-map.en.md` records file locations; `key-references.en.md` records key symbols.

## Bilingual Rule

Stable development documentation must have both a Chinese `.md` page and an English `.en.md` page. Keep structure, headings, links, and facts aligned; do not translate code identifiers, paths, commands, or external project names.
