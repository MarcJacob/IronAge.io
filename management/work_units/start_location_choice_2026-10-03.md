# Start location choice (placement phase)

Started: 2026-10-03. Part of `game_server_match_lifecycle_2026-10-03.md`.

## Goal

Before a match ticks, each player sends a "choose start location" message.
On the match's first tick the choices become a spawn command / routine.
Then players get MATCH_STARTED and normal ticking begins.

## Open design

- Placement phase duration and what happens to players who don't choose
  (random / auto-pick).
- Validity rules (terrain, minimum distance between starts).
- Client UI for choosing (uses terrain view from `blit_canvas`).
- Message layout (packed struct, cross-check JS reader).

## Progress

Not started.
