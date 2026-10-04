# BACKLOG: Web server tiered file caching

Input date: 2026-09-20

## Item

Always-preloaded tier for small, frequently requested files; on-demand tier
(read from disk per request) for large or rarely requested ones.

## Why

Every servable file is preloaded today; won't scale with larger assets.
