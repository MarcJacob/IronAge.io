# BACKLOG: Blit canvas for expensive-to-compute pixel content

Input date: 2026-09-26

## Item

Blit canvas for expensive-to-compute pixel content; pure-JS rendering is
sufficient for now.

## Description

A second canvas layer, written to from WASM as a raw pixel buffer and blitted
to screen, for content too expensive to draw tile-by-tile through the JS 2D
canvas API (e.g. large-scale terrain/fog-of-war rendering).

## Why

Deferred since project inception - current JS canvas rendering handles
everything built so far. Only becomes necessary once a rendering workload
shows up that JS canvas drawing can't handle at acceptable performance.
