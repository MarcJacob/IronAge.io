# Territory / economy redesign

Started: 2026-10-27. Supersedes the contestation model of `village_influence_2026-10-03.md`. Notes: `src/game_common/world_sim/.ai_imprint/territory_redesign_survey.md`.

## Scope

Rewrite land production, population strata and territory claiming / borders. Built and hand-tested by user 2026-10-28: territory behaves as wanted.

## Decisions

- Contestation / pressure-push mechanic deleted from the design. "Proximity" renamed "Distance".
- Tiles have peasant jobs + surplus factor. Peasants always produce 1 good; surplus falls off with distance. Non-peasants (incl. Unemployed class) live off surplus.
- Distances stay chamfer (orthogonal weight 5, diagonal 7, plains step 25); distance constants are in that scale (cap 180 = 7 plains tiles; user rescaled).
- Takeover: tile flips to another settlement if its distance is shorter by more than a margin (25); ties keep owner; taker must have unemployed left; capped by `TERRITORY_MAX_DISTANCE`. Player ownership ignored for now.
- Takeover is dirty-driven, snapshot-decided, staggered (4 settlements / tick, 128 decisions / tick).
- Distances rebuilt per settlement on tile removal (bounded shortest-path); cut-off tiles and tiles above the cap are unclaimed.
- Sea tiles not claimable (arable table entry kept).
- AI road check uses `peasant_jobs` (= `pop_peasants_cap`), not surplus.

## Tasks

1. [DONE] Survey + mechanical fixes; contestation removal; stale comments.
2. [DONE] Sea unclaimable, takeover, distance rebuild, aggressor gate, distance cap, AI fix.
3. Takeover check / flip counter + log: blocked (game_common cannot log).
4. Cross-player border rule: undecided (pressure vs military presence). Idea: unclaim furthest tiles when too few peasants, possibly replacing the push between different-player settlements.

## Open questions

- Counter plumbing: two ui32 counters on `match_world_state` + read-and-reset accessor the server logs every N ticks (suggested) vs log function pointer.
- Rebuild sweeps repeat until stable: may be slow on very large bounds.
- Levies: removed by user pending the military rework.
- Growth scales on pop at home (worker's choice) vs total.
- Unemployed not shown in settlement panel.
- `world_destroy_settlement` does not unclaim tiles (no caller yet).
- `spawn` default `pop_peasants_cap = pop_peasants` contradicts cap = land jobs.
- Stale `game_mechanics.imprint.md` sections (territory, population, wealth / tax, siege levies): user updates.

## Next step

Territory done. Remaining: open questions above (levies, growth basis, Unemployed UI, counter plumbing). Trade and military are to be redesigned from scratch (separate units).
