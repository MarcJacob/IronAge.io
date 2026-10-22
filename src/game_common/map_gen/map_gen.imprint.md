# MAP GENERATION

Primitive procedural terrain generator. Meant to be replaced or grown later (more parameters, more features, file-based maps).

## Position

- It is NOT world simulation. It is a Component of game common: the world simulation calls it, it knows nothing about the simulation.
- It only uses the public terrain headers (*world_terrain.h*) and the core library.
- Called once from `world_state_init` when `procedural_map` is set in the match start params. Terrain tile ownership setup stays on the caller's side.

## Pipeline

- *map_gen_heightmap.cpp*: fills a heightmap (one elevation byte per tile, whole world) from the seed.
- *map_gen_apply.cpp*: classifies the heightmap into terrain types and writes `terrain_types` only.
- *map_gen_main.cpp*: entry point `map_gen_generate`, unity entry of this folder. The heightmap lives in scratch arena memory that is given back.
- *map_gen.h*: internal declarations and the heightmap structure.

## Parameters

- `game_map_gen_params` lives in *include/game_common/match/map_gen_params.h* and is part of the match start params. Zero is the default for every field.
- The start params are sent to clients as-is: the web client mirrors them by hand in *backend.ts*. Change both together.
- Tuning constants are at the top of *map_gen_heightmap.cpp* and *map_gen_apply.cpp*.

## Determinism

- Integer / fixed-point math only, no floats. Same result on every machine.
- Seeded from the world's rand generator. Noise helpers are in *include/core/math/noise.h* and *numerical.h*.
- Elevation is a pure function of tile position, params and seed.

## Unplugging

- Remove the include and the branch in `world_state_init`, delete this folder, remove `procedural_map` and `map_gen` from the start params and *backend.ts*.
