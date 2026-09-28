# WORK UNIT: TypeScript Migration

Started: 2026-09-27

Goal: convert the web client's JS (`src/client_web/web/src/*.js`) to TypeScript,
before the client input/camera/UI work unit adds more JS to migrate later.

## Tasks

- [x] Add Node.js + npm as a dev-machine requirement.
- [x] `tsconfig.json`: `tsc`-only, no bundler.
- [x] Wire the TS build into `deploy_web_client.bat` before the `xcopy` step.
- [x] Convert all files: `backend`, `render`, `input`, `debug`, `main`,
  `websocket_tests`.
- [x] Type the wasm export boundary in `backend.ts` (function signatures,
  render-state / message shapes) properly, then turn `strict` back on.

## Progress

- Source layout split: `src/client_web/front/` holds the TS project
  (`package.json`, `tsconfig.json`, `src/*.ts`, `front.imprint.md`);
  `src/client_web/web/` holds only deployable static resources, with `src/`
  now fully generated (gitignored) from `front/src/` by `tsc`.
- `deploy_web_client.bat` runs `npm run build` in `front/` before its
  existing `xcopy` of `web/`; verified end to end (clean `npm install`,
  build, deploy) produces a `web_root` with no leftover TS tooling.
- `strict: false` for now, to get the whole codebase over to `.ts` first;
  only `websocket_tests.ts` needed real fixes (untyped class fields, a
  `disabled` access needing an `HTMLButtonElement` cast).
- `include` must be `./src/**/*.ts`, not `./src/*.ts` - the single-star form
  silently skips files in subfolders (no error, just not compiled). Confirmed
  fixed with a subfolder probe file; matters once client-input work adds
  subfolders under `front/src/`.
- `strict: true`, whole codebase typed. `npx tsc --noEmit` clean.

Unit complete.

## Notes / Decisions

- `tsc`-only, no bundler: dev-time only, zero runtime footprint, output stays
  plain ES modules matching the current no-build-step setup as closely as
  possible.
- Scope: catches JS-level mistakes (typos, wrong argument types/counts,
  refactor breakage across modules). Does NOT catch wasm memory-layout bugs
  (wrong byte offset, wrong endianness) - those are invisible to the type
  system regardless; `backend.ts`'s raw memory reads still need the same care.
- Reasoning for doing it now: frontend is expected to grow substantially
  (UI, selection, camera, more advanced actions); paying the conversion cost
  before that code exists is cheaper than migrating a larger JS codebase later.
- `xcopy` in `deploy_web_client.bat` doesn't delete destination files that
  vanished from the source - a stale `web_root` can carry leftovers across
  a layout change. Pre-existing behavior, not fixed as part of this unit.
