# BACKLOG: Memory arena functionality expansion

Input date: 2026-09-30

## Item

Extend `mem_arena` (`include/core/memory.h`):
- Virtual-memory strategy: reserve a large address range, commit pages on
  demand (win32 first).
- Contract flags (e.g. can grow, contiguous allocations) so callers can assert
  the arena has the properties they rely on.

## Why

Match slots could each reserve a huge range and commit only what the match
uses, instead of a worst-case block per slot.
