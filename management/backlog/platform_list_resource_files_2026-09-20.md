# BACKLOG: Platform call to list files in a resources folder

Input date: 2026-09-20

## Item

Platform call to list the files in a folder relative to resources, so the
server discovers the files under `web_root` instead of taking a list in the
init params.

## Description

Add a platform-level directory listing call (relative to
`GAME_SERVER_RESOURCES_DIR`), so the web server can discover which files to
preload under `web_root` on its own instead of being handed an explicit file
list at init time.

## Why

Removes the need to manually keep an init-time file list in sync with
whatever actually lives under `web_root` - currently a manual/error-prone
step whenever a file is added or removed.
