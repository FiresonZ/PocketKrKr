# Frequently Asked Questions

## Android Build Dependency Failure

If the Android arm64 build fails during glib, Meson, or installation, first confirm:

1. Use the NDK, vcpkg manifest, and overlay port declared by the repository.
2. Remove invalid binary caches corresponding to the target triplet, then configure again.
3. Check whether `vcpkg/ports/glib/portfile.cmake` includes the fix for the current platform.
4. Do not modify third-party source code in the vcpkg download directory; temporary environment fixes must not be committed as project configuration.

## Build Cache Anomalies

Source changes should trigger recompilation of the corresponding target. If the artifact does not include header changes, clear the target build directory and ccache, then configure again; vcpkg dependency caches normally do not need to be deleted.

## Android Installation Failure

Confirm that the device meets the API 24, arm64-v8a, and Vulkan requirements. When overwriting an existing installation, you must use the same signature as the old package; different signatures require uninstalling the old package first.

## iOS Installation Failure

iOS artifacts are usually unsigned IPAs and must be re-signed with a valid Apple account and signing tools. The device OS must meet the project's minimum version requirement.

## Black Screen or Frozen Frame

Enable the rendering probe and check the source texture, redraw requests, event delivery, and target texture in the order described in [Rendering Diagnosis](dev/rendering-diagnosis.md).
