# BACKLOG: Web server tiered file caching

Input date: 2026-09-20

## Item

Web server: keep frequently-used files always loaded ("cached"), load
rarely-requested or large ones on demand.

## Description

Split servable files into an always-preloaded tier (small, frequently
requested) and an on-demand tier (large or rarely requested, loaded from disk
per request instead of held in memory).

## Why

Current web server preloads every servable file up front regardless of size
or request frequency - fine today's small file set, but won't scale once
larger or rarely-used assets are added.
