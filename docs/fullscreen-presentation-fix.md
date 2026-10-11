# Fullscreen presentation after level loading

In exclusive fullscreen, loading a Dead Air save could leave the desktop visible
while the game window and controls remained active. Switching away and back with
Alt-Tab restored the image. Borderless fullscreen did not reproduce the issue.

The DX11 renderer now selects the traditional DXGI DISCARD swap chain for the
global game window in exclusive fullscreen. Windowed and borderless startup retain
the existing flip-discard presentation path. Entering exclusive fullscreen from a
running flip-chain session replaces the swap chain during renderer reset, after
render targets are released; D3D contexts are cleared and deferred destruction is
flushed before creating the replacement on the same window.

The application loop also drains queued window-event batches and derives main
window activation from current SDL focus and visibility flags, avoiding activation
decisions based on stale transitions queued during display reset.

## Validation

- Release x64 engine and R4 renderer builds passed.
- Source whitespace checks passed.
- User verified the affected save remains visible at 5120x2880, 60 Hz in exclusive
  fullscreen with R4 and `-noprefetch`.
- Runtime log confirmed `swap effect=0, exclusive fullscreen=1` after the reset and
  completed loading with `End of synchronization A[1] R[1]`.

Changing from a running borderless session to exclusive fullscreen is compiled and
source-reviewed but has not been interactively tested. Switching back to borderless
keeps the traditional swap chain for that session; restarting in borderless restores
flip presentation.
