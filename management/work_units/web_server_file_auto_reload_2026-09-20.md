# BACKLOG: Web server auto-reload of changed preloaded files

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
