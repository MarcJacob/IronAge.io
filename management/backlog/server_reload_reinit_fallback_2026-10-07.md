# Server reload re-init fallback

Input date: 2026-10-07

## Item

When a hot-reloaded server DLL changes the memory block layout, detect it and run
a server "re-init" routine that cuts all connections, matches, etc., instead of
continuing on the old layout.

## Why

Today a layout-breaking reload may crash the server. Acceptable in dev (reloads only
happen with no external connections); needed before reloading on a live server.
