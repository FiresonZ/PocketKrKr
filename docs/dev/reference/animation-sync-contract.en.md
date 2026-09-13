# Animation Sync Contract

## Purpose

Define general behavior for multi-instance timeline playback, skipping, completion signals, and target isolation. This page stores an abstract contract and does not copy any external implementation.

## Instance Ownership

- Each animation controller must independently own its motion name, clock, loop state, completion state, and draw target.
- Capture, drawing, and skipping must resolve the controller through the current script object or an explicit instance reference.
- A global “most recently played” or “last drawn” pointer must not route operations across instances.
- Scene transitions must clear old targets, wait state, and temporary resources.

## Timeline Behavior

```text
play(motion):
  reset instance clock and completion state
  load the selected timeline

progress(delta):
  advance only this instance
  evaluate loop or completion
  emit completion once for non-looping timelines

skipToSync():
  move only this instance to its synchronization boundary
  preserve the caller's completion contract
```

Skipping one animation is not the same as enabling a global skip mode, and it must not modify a later animation that has not started. The later animation must initialize its own clock and loop state through its own `play` operation.

## Completion Signals

- Natural completion and active skipping may emit at most one completion signal each.
- The signal must be associated with the animation instance that emitted it.
- After stop, destruction, or a scene transition, an old instance must not emit further signals.
- Command lists, script callbacks, and render-target updates must follow the same instance boundary.

## Acceptance

- With two controllers active, skipping the first does not change the second controller's clock, loop state, or completion state.
- After M2logo is skipped, the later title-background animation starts from its own initial time.
- Repeated clicks, natural completion, replay, stop, and destruction do not trigger duplicate completion callbacks.
- Each capture target belongs to exactly one animation instance; routing does not depend on global last-draw state.
