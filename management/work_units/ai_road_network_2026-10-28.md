# AI road network

Started: 2026-10-28. Follows `match_ai_2026-10-22.md` (AI structure) and `trade_network_redesign_2026-10-28.md` (why: trade needs connected settlements). Roads: `road_system_2026-10-21.md`.

## Goal

AI builds roads to connect all its settlements into one network, so trade works for AIs and the game can be play-tested against them.

## Decisions

- Road placement is a mix of two purposes: connecting settlements into a network, and raising output / territory (current behavior kept).
- Connection is judged on travel cost (path requests), not tile distance: build where a road makes a worthwhile cost reduction.
- Target: all of the AI's own settlements in one network (not trade-partner or nearest-neighbour pairs).
- Wants a simple graph-based system (settlements = nodes, connections / path costs = edges).

## Tasks

1. [DONE] Survey (notes in `src/game_common/match_ai/.ai_imprint/ai_roads_survey.md`). Key facts: AI has spoke-only roads, no path use; `world_path_find` (const terrain, own scratch arena) is the usable public path call; BUILD_ROAD origin must be settlement tile or own road tile, destination unchecked; foreign / unowned tiles build 5x slower; water uncrossable.
2. [DONE] Design.
3. [DONE, hand-tested: clear improvement] Implement (Opus). AI also builds Housing near the urban cap (`AI_HOUSING_URBAN_PERCENT` 80) and finds buildings by name. Notes: `src/game_common/match_ai/.ai_imprint/ai_road_network.md`.

## Decisions

- Connected = road-only path exists (roads of any owner count, enemy included). No union-find: sweep of pairs, a few per tactics run, cheapest unconnected pair (route cost via crew path settings) becomes the link.
- Link built by two crews, one from each end, meeting halfway. One link at a time. Links go first on crew budget; spokes continue with the remaining crews.
- Unreachable pairs (water) skipped. No cost cap. Shared 256 KiB path scratch buffer (single-thread assumption).
- `ai_player_state` changed size: restart server, no hot reload.

## Open questions

- Risks to watch: links relying on enemy roads, road-only search running out of memory on very large networks (reports "not connected"), failed crew command drops the link until next sweep.

## Next step

Build, play-test (AI settlements join by roads, caravans run between AI settlements, tick time with many AIs).
