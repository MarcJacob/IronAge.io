# BACKLOG: Memory arena functionality expansion

Input date: 2026-09-30

## Item

Memory arenas functionality expansion:
- Ability to work with virtual memory and page sizes so they can be given
  *reserved* address spaces and commit as needed on platforms that support
  it.
- Contract flags: can be expanded, guarantees contiguousness between
  different allocations...

## Description

Two related extensions to `mem_arena` (`include/core/memory.h`):
- A virtual-memory-backed allocation strategy: reserve a large address range
  up front, commit physical pages on demand as the arena grows, on platforms
  that support it (win32 first).
- "Contract flags" on an arena describing properties some callers need and
  others don't (e.g. "can grow", "guarantees contiguous allocations"), so
  code can assert it's been handed an arena with the properties it actually
  relies on instead of assuming.

## Why

The virtual-memory strategy solves match-slot memory growth on the server
cleanly: each slot could reserve a huge address range and only commit what a
given match actually uses, instead of every slot needing a big up-front
block sized for the worst case. Contract flags catch an arena being used in
a way it wasn't built for.
