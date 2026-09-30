# BACKLOG: Win32 net timeout for a stale-but-alive peer

Input date: 2026-09-20

## Item

Win32 net: timeout to close a connection whose peer stays alive but stops
reading (it is never closed once in SERVER_CLOSED).

## Description

A connection whose peer keeps the socket technically open but stops reading
data currently lingers forever once the server has put it in SERVER_CLOSED -
nothing times it out. Add a timeout that force-closes such connections.

## Why

Resource leak risk: a misbehaving or frozen peer can hold a connection slot
indefinitely with no automatic cleanup.
