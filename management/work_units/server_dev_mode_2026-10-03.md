# BACKLOG: Server dev mode

Input date: 2026-10-03

## Item

Server can be started in dev mode. In that mode it accepts special game
messages from connections authenticated as Administrator, which trigger
monitoring / dev actions. First action: reload the server's resource files.

## Description

- Startup flag enabling dev mode; off by default.
- Dev messages only accepted from Administrator-authenticated connections.
- Initial dev action: reload http-serveable resources (see
  `web_server_file_auto_reload_2026-09-20.md`; the explicit reload could
  supersede automatic reload).
- Related: `platform_list_resource_files_2026-09-20.md` (discovery is needed
  for a meaningful reload).
- Open design: how a connection authenticates as Administrator, and the
  dev message set.

## Why

Prerequisite for the dev iteration loop (`dev_iteration_loop_2026-10-03.md`):
lets tooling tell a running server to reload things without a restart.
