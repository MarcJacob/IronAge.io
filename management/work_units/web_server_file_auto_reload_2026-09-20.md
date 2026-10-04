# Web server reload of preloaded files

Input date: 2026-09-20

## Item

Reload preloaded web files at runtime without a server restart: re-list
resource files, reload on a dev mode message (`server_dev_mode_2026-10-03.md`),
optionally auto-reload when the file's modified-time changed.

## Decisions

- Reload while an HTTP response is in flight: unhandled now (TODO in web
  server); body pointers reference the file data arena, which reload clears.
- Failure to load web files stays fatal. Later option: double buffering.
- Re-listing reuses the `resource_files` buffer (allocated once); resource
  names point into it.
- Explicit reload via dev mode may supersede auto-reload.

## Next step

Not started. String pass dependency is done. Phase order: build revamp, dev
mode, this, dev iteration loop.
