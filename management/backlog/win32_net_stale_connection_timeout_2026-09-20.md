# BACKLOG: Win32 net timeout for a stale-but-alive peer

Input date: 2026-09-20

## Item

Force-close a connection whose peer stays open but stops reading; it lingers
forever once in SERVER_CLOSED.

## Why

A frozen peer can hold a connection slot indefinitely.
