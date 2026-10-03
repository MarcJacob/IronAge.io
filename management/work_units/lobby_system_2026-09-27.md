# BACKLOG: Lobby system

Input date: 2026-09-27

## Item

Lobby system: player list, ready-up, synchronized match start, replacing the
2-connection headcount start.

## Description

Replace the current "match slot 0 starts once 2 clients are connected, no
player list / ready-up" placeholder with a real lobby: visible player list,
per-player ready state, and a synchronized match start once everyone's ready.

## Why

The headcount-based start is a temporary stand-in from the wire protocol v0
bring-up phase - not a real lobby experience, and doesn't generalize past
exactly 2 players.
