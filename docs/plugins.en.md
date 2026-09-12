# Plugins and Extensions

PocketKrKr plugins fall into three categories: core built-in capabilities, C++ plugins compiled with the engine, and script extensions loaded by the game. A name being discoverable by `Plugins.link` does not mean that all related functionality has been implemented.

## Current Plugin Directories

| Directory or file | Purpose | Status |
|---|---|---|
| `cpp/plugins/psbfile/` | PSB resource and animation parsing | Built-in |
| `cpp/plugins/psdfile/` | PSD resource parsing | Built-in |
| `cpp/plugins/motionplayer/` | M2/PSB animation playback | Built-in, complex features under continuous development |
| `cpp/plugins/layerex_draw/` | Extended drawing | Built-in |
| `cpp/plugins/fstat/` | File status queries | Built-in |
| `cpp/plugins/cubism/` | Live2D integration | Optional SDK |
| `cpp/plugins/zcompat/` | Compatible name mappings and stubs | Some capabilities not yet implemented |
| `cpp/plugins/*.cpp` | Various TJS extensions | Determined by CMake and the registry |

## Implementation Boundaries

- Core capabilities should be placed in `cpp/core/`, such as Layer, rendering, storage, font, and video infrastructure.
- Reusable TJS extensions should be placed in `cpp/plugins/` and register interfaces through ncbind.
- Keep pure script compatibility layers within the script resource boundary; do not mix script state into the global C++ lifecycle.
- Desktop-specific drawing devices must not be treated directly as mobile implementations; mobile platforms should explicitly specify the graphics backend and target texture path they use.
- When an optional SDK is missing, keep it automatically disabled; optional features must not block the core build.

## External Resources

Historical plugin names, format information, and external repository links are listed in [Compatibility and Reference Materials](dev/krkrz-compat.md). That page serves only as an entry point to the materials and an explanation of capability boundaries; it does not guarantee that external plugins can be loaded directly.
