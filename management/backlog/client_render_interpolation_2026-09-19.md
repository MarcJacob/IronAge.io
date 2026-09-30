# BACKLOG: Client render interpolation between ticks

Input date: 2026-09-19

## Item

Client render interpolation between ticks (smooth movement).

## Description

Entities currently snap to their new position on every match tick. Interpolate
their rendered position between the last two tick states over the time elapsed
since the last tick, so movement reads as smooth on-screen even though the
simulation itself stays fixed-timestep.

## Why

Ties into the known "jitter at high zoom" issue tracked in
`work_units/client_input_2026-09-27.md` - may turn out to be the actual fix for it,
not just a polish item.
