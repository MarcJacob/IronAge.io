# BACKLOG: Persistent client identity / reconnection

Input date: 2026-09-24

## Item

Persistent client identity / reconnection: a client outlives its connection
(OFFLINE and CONNECTION_LOST states, reconnection grace time).

## Description

Extend the server's client lifecycle beyond the current UNKNOWN ->
NON_GAME_CLIENT -> GAME_CLIENT states so a client's identity (and match seat)
survives a dropped connection for a grace period, rather than being torn down
immediately on disconnect. Adds OFFLINE / CONNECTION_LOST states and
reconnection handling.

## Why

Without this, any network hiccup ends a player's match seat outright. Needed
before matches can be trusted to run for any real length of time over real
networks (as opposed to local dev testing).
