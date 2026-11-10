# Platform call to list files in a resources folder

Input date: 2026-09-20

## Item

Platform call listing files in a folder relative to `GAME_SERVER_RESOURCES_DIR`,
so the server discovers `web_root` files instead of taking a list in init params.

## Decisions

- `./` leading convention for resource-relative paths; `web_root` is `./<path>/`.
- List call is recursive, with a trailing `*` search path.
- Failures (truncation, missing folder, zero files) are fatal for now.

## Status

Complete. [DONE]
- Lister rewrite was left to the string pass (done).
- Live reload: `web_server_file_auto_reload_2026-09-20.md`.
