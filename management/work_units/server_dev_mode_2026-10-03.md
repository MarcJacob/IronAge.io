# Server dev mode

Input date: 2026-10-03

## Item

Server startable in dev mode (flag, off by default). In that mode it accepts
dev messages from Administrator-authenticated connections. First action:
reload http-serveable resources (`web_server_file_auto_reload_2026-09-20.md`).
Prerequisite for `dev_iteration_loop_2026-10-03.md`.

## Open design

- How a connection authenticates as Administrator.
- The dev message set.
- How dev mode is turned on: launch parameter, remotely, or both.
- Transport: game messages from an elevated game client, and/or an HTTP
  request (e.g. curl) so tooling can trigger a reload.

## Next step

Not started. Deferred: the crude flag-on-reload resource reload in
`web_server_file_auto_reload` covers the immediate need.
