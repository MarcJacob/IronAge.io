# Client-facing game data store

Raised: 2026-10-22 (from the construction UI peer).

- Smarter way to store game data the client needs (UI names, building definitions, descriptions...) so C++ and TS both reach it without hand-written GDOs and exports each time.
- Related: dynamic GDO properties (per-building-type counts), worker progress as a common value.
- Related items: `client_backend_game_state_2026-10-13.md`.
