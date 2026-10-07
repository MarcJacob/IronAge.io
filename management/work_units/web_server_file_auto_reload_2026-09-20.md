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

- Crude approach chosen (replaces waiting on dev mode): on server DLL load, if
  the server already exists (reload, not first load), flag it to reload
  resources. Resources rediscovery + reload runs whenever the flag is set (or on
  init). Safe because reloads only happen with no external connections.
- Dev mode / auth / HTTP trigger deferred (`server_dev_mode_2026-10-03.md` stays
  open, not started).

- Implemented: flag `reload_resources` on `game_server`, set in
  `game_server_load_program` on reload, checked at the top of `game_server_tick`
  (rediscover + `web_server_reload_files`). Hand-tested.

## Next step

Crude version [DONE]. Left open: in-flight response handling, explicit
reload via dev mode, mtime auto-reload. Nothing planned.
