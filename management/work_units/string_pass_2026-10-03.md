# String pass over the code

Input date: 2026-10-03

## Item

A pass over the code to get a proper string toolset, so C strings
(null-terminated) are no longer used where sized views fit.

## Decisions

- Goals: simplify logging and file management; related functions take string
  views wherever possible.
- Utility functions on views/strings (chop, append, etc.) allocate from a
  memory arena.
- `static_mem_arena`: declared in a function, usable over its own stack /
  static memory. Not copyable.
- String builder holding the arena reference, for multiple / nested operations.
- String types are never null-terminated by nature. `ia_static_string`
  capacity is `<=`; default ctor zeroes the buffer; stale bytes after
  `chop_left` are accepted.
- Failure to ingest a resource file is fatal: assert.
- Arenas do not zero on create / clear; typed `alloc<T>` zeroes new items
  (zero is initialization).
- Null-terminated conversion lives only in the win32 platform.
- Overflow: builder / fixed-buffer operations assert; logging truncates.

## Outcome

- Sized string toolset: view utilities, `static_mem_arena`, string builder,
  formatter, integer to string (`include/core/cstring.h`).
- Logging takes a pre-built view at the platform; `server.log` / `server.logf`
  take component, `LOG_TYPE`, format.
- Config, web server files and resource paths use views; win32 path handling
  is length-checked and fatal on ingest failure.
- Finalizing the formatter and the conversions that depend on it:
  `backlog/finalized_string_formatter_2026-10-04.md`.
