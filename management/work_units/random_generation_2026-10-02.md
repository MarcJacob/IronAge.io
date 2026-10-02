# WORK UNIT: Deterministic Random Generation

Started: 2026-10-02
Closed: 2026-10-02

Goal: a random generation system usable across the codebase (native server,
wasm client, tests) with determinism baked in, so it's safe to use for match
simulation (same seed -> same sequence on every platform/compiler).

## Tasks

- [x] Split `include/core/math.h` into sub-files
  (`math/{random,numerical,vec2,crypto}.h`) - no behavior change, just file
  organization now that math.h was growing unwieldy.
- [x] Replace the old `ia_rand_*` functions (which had a freestanding-build
  stub path that silently returned `min` - see
  `work_units/world_entities_2026-10-01.md`'s bug list) with a hand-rolled
  xorshift32/64 PRNG, integer-based so it can't diverge between compilers:
  `ia_xorshift_i32`/`_i64`, ranged variants (`ia_xorshift_range_i32/f32/f64`),
  and a stateful `rand_generator_32` (seed, `next<T>`, `next_range`,
  `next_vec2`) so each match (and, in a test, each independent draw) can own
  its own generator state instead of sharing one global.
- [x] Wired into match code: `game_match::main_rand_gen`, seeded from
  `game_match_start_params::random_seed` in `match_start`; used for the
  caravan-spawn chance roll and destination pick in `match_tick`
  (`game_common_main.cpp`).

## Issues found reviewing the code - fixed

- **`ia_xorshift_range_f32`/`ia_xorshift_range_f64` were ignoring the `state`
  parameter they were handed.** Both took `state` by reference but then
  called `ia_xorshift_i32()` / `ia_xorshift_i64()` with no argument, which
  defaulted back to the *global* `XORSHIFT_STATIC_STATE_32`/`_64` regardless
  of what the caller passed in - silently undercutting this unit's own
  "individually tracked states" goal for these two functions specifically.
  Fixed by threading `state` through to `ia_xorshift_i32(state)` /
  `ia_xorshift_i64(state)`.
- **`rand_generator_32::next`/`next_range`/`next_vec2` (both overloads)'s
  `static_assert`s checked `sizeof(Numeric) <= 32`, not `<= 4`.** The
  comments and "32-bits" naming both say the intent is to cap `Numeric` at 4
  bytes (32 bits), pointing callers at a `_64` generator above that - but
  checking bytes against `32` only rejected types 32 bytes (256 bits) or
  larger, making the guard effectively dead. Fixed all four `static_assert`s
  to `<= 4`.

## Notes / Decisions

- Integer-based generation (`ia_xorshift_range_i32`) is the path used for
  anything that needs to match bit-for-bit across native/wasm builds -
  float/double math (`ia_xorshift_range_f32/f64`) is for cases that don't
  need cross-build determinism (e.g. the debug log use), given float
  rounding isn't guaranteed identical across compilers/targets.
- `rand_generator_32` can be seeded directly or "spawned" from another
  generator (`rand_generator_create_32(spawner)`, draws a seed from it) -
  intended for deriving independent sub-streams (e.g. per-system or
  per-entity generators) from one root seed without them stepping on each
  other's state.
