# Blit canvas

Input date: 2026-09-26

## Item

Second canvas layer written from WASM as a raw pixel buffer and blitted to
screen, for content too expensive to draw through the JS 2D canvas API (e.g.
terrain / fog of war).

## Decisions

- Pure-JS rendering is sufficient for now; needed once a workload exceeds it.
  Phase 3 terrain bitmap is the expected first use.

## Next step

Not started.
