# Documentation Rules

## Choose a Category First

Read the [documentation map](document-map.en.md) before adding content. Prefer updating the existing canonical page instead of creating a second source for the same fact.

## Content Boundaries

- Current architecture facts belong in `architecture.en.md`.
- File, symbol, and module locations belong in `source-map.en.md` or `key-references.en.md`.
- Build commands, toolchains, and artifacts belong in `build.en.md`.
- Modification constraints and long-term rules belong in `conventions.en.md` or this page.
- Current capabilities, regression loops, and game compatibility belong in `compatibility.en.md`.
- External repositories, format material, and protocol entry points belong in `krkrz-compat.en.md`.
- Plugin status and gaps belong in `plugin-compatibility.en.md`.
- Itemized external implementation differences belong in `aetherkiri-audit.en.md`.
- Probe names, switches, locations, and cost belong in `probes.en.md`; probe-based diagnosis belongs in `rendering-diagnosis.en.md`.
- Unfinished work belongs in `todo.en.md`; confirmed long-term directions belong in `optimization-roadmap.en.md`.
- Performance measurement methods and acceptance rules belong in `perf-optimization.en.md`.
- Reproduction, evidence, and conclusions for one issue belong in `incident-reports.en.md`, not in architecture or todo pages.
- Stable API, ABI, script-method, and parameter contracts belong in `api-contracts.en.md`.
- Reproducible test resources and fixture descriptions belong in `test-fixtures.en.md`.
- Version changes and compatibility impact belong in `release-notes.en.md`.

## Facts and Hypotheses

- Distinguish “exists in source,” “compiled,” “verified,” and “suspected.”
- The presence of code in an external repository does not prove that this project has equivalent behavior.
- Fixes without target-platform verification must be marked as pending verification.
- Do not put personal devices, temporary paths, complete logs, screenshot procedures, trial-and-error history, or commit IDs in long-lived documentation.
- Do not record complete user text, personal paths, secrets, or private data.

## Editing Rules

- Keep Chinese and English pages structurally and factually aligned; update both in the same change when the page is stable.
- Use stable topic titles, not dates, device names, personal names, or temporary symptom descriptions.
- Use repository-relative paths for code references and the unified reference entry point for external material.
- Use tables only for stable, comparable fields; put longer explanations below the table.
- Every todo must have scope, acceptance conditions, and a state; remove it or move it to current facts when complete.
- Run `git diff --check` after documentation changes and confirm index links and bilingual file pairs.

## Prohibited Patterns

- Do not duplicate architecture, build, or compatibility-boundary text across pages.
- Do not treat temporary investigation logs as test reports or long-lived facts.
- Do not describe undecided functionality as supported.
- Do not create a page that has only a title and no purpose or next entry point; a placeholder must state its goal and writing conditions.
