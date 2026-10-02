# BACKLOG: Viewport aspect ratio awareness

RESOLVED 2026-10-03 (ui_first_commands work unit): the TS camera owns the world
rectangle and builds it from the canvas aspect ratio; safe to delete.

Input date: 2026-10-01

## Item

The client backend's viewport sizing (world tiles visible, driven by zoom_level)
knows nothing about the canvas's pixel resolution/aspect ratio, so a
non-square canvas stretches tiles non-uniformly on X vs Y.

## Description

`render.ts` computes `scaleX`/`scaleY` independently
(`canvas.width / viewport_width`, `canvas.height / viewport_height`). The
backend's `client_viewport_state` sizing (`viewport_size_min`/`viewport_size_max`,
eased by `zoom_level`) is square-ish and has no idea what the canvas's actual
pixel width/height ratio is. Need some way to tell the backend the
viewport's pixel resolution (or just its aspect ratio) so it can size the
world-tile viewport to match - e.g. keep viewport height zoom-driven and
derive viewport width from it times the canvas aspect ratio.

## Why

Surfaced 2026-10-01 widening the game canvas from 800x800 to 1200x800 for a
better layout - immediately made tile stretching on X visible. Needs a real
design decision (how resolution gets communicated to the backend, where the
aspect-correction math lives) rather than a quick patch.
