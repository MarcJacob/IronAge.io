# BACKLOG: Persistent client identity / reconnection

Input date: 2026-09-24

## Item

A client and its match seat outlive a dropped connection for a grace period:
OFFLINE and CONNECTION_LOST states beyond UNKNOWN -> NON_GAME_CLIENT ->
GAME_CLIENT, plus reconnection handling.

## Why

Any network hiccup currently ends a player's seat.
