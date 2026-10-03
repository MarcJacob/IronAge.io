# Web server auto-reload of changed preloaded files

Input date: 2026-09-20

## Item

Web server: optional automatic reload of a preloaded file when it changed on
disk since it was loaded.

## Description

Detect (e.g. via file modified-time) that a preloaded file has changed on
disk, and reload it automatically, rather than requiring a full server
restart to pick up edits.

## Why

Speeds up the dev loop for web client / static asset changes - currently
every change to a servable file needs a server restart to take effect.

## Decisions (2026-10-03, from the resource discovery review)

- Part of the live reload work: re-listing resource files at runtime, and
  reload triggered by a dev mode message (`server_dev_mode_2026-10-03.md`).
- Reload while an HTTP response is in flight: unhandled now (TODO in the web
  server). Body pointers reference the file data arena, which reload clears.
- Failure to load web files stays fatal for now. Double buffering (keep the
  previous set usable on any failure) is the later option.
- Re-listing must reuse the `resource_files` buffer (allocated once); resource
  names point into it.
- Depends on the string pass for the lister rewrite
  (`string_pass_2026-10-03.md`).

## Next step

Not started. Suggested order for the phase: build system revamp, dev mode,
this, dev iteration loop; string pass independent.
