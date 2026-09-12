# Rendering Diagnosis

This document is for investigating black screens, frozen frames, frames not being submitted, and video display issues. Probes are disabled by default and are only used for issue localization when enabled.

## Enable Probes

Enable `enable_render_probe=true` during manual CI builds, and set `log_level=debug` or `trace` as needed. For local builds, set:

```bash
cmake -DENABLE_RENDER_PROBE=ON <other parameters>
cmake -DKRKR_LOG_LEVEL=debug <other parameters>
```

Disable probes after the investigation to prevent high-frequency logging from changing timing.

## Key Logs

| Prefix | Purpose |
|---|---|
| `UpdateDrawBuffer` | Records the rendering path, target texture, layer count, and compositing count |
| `SourceSample` | Samples the engine source texture to determine whether the engine wrote valid pixels |
| `IOSurfacePixelSample` | Determines whether the shared IOSurface received valid pixels |
| `BlackScreen` | Summarizes drawing, layer, and video state during continuous black screens |
| `engine_tick` | Records whether the bridge frame call entered and returned |
| `RequestUpdate` | Determines whether layer changes requested a window redraw |
| `DeliverWinUpdate` | Determines whether the window redraw event was delivered and handled |

## Order of Judgment

1. Check `SourceSample`. If the source texture is not black, continue checking blit or shared-texture paths; if the source texture is completely black, check compositing and layer updates.
2. Check the `draw` or compositing count. A growing count indicates that the engine is still drawing; when it does not grow, check update requests, event delivery, and the drawing device.
3. Check `IOSurfacePixelSample` or the SurfaceTexture path to confirm that the destination texture received pixels.
4. Check `engine_tick` entry and return records to confirm that the bridge call did not stop midway.
5. For video issues, also check video frame counts, playback state, and target Layer updates; do not diagnose video faults from black-screen state alone.

`avg=(0,0,0,255)` is opaque black and usually means that the source texture was not covered by valid content; `avg=(0,0,0,0)` is transparent black and means that the content may be empty while the alpha is normal.

## Log Capacity

Logs use rotating files. A file is rotated after reaching 4 MiB, with up to 3 files retained. The default log level is set according to the build type; increase it only when locating an issue.

## Reporting Requirements

Record the platform, build type, reproduction steps, symptom, key log prefixes, and expected behavior. Do not submit personal paths, device privacy data, complete game resources, or unrelated high-frequency logs to the repository.
