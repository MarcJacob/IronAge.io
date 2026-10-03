# BACKLOG: String pass over the code

Input date: 2026-10-03

## Item

A complete pass over the code to get a proper string toolset and link the code
into it, so C strings (null-terminated) are no longer used outside constant
strings and direct platform calls that require them.

## Description

- String toolset: sized string views, plus formatting and building helpers
  (extends `include/core/string.h`).
- Logging takes string views directly, both as the format and as a string
  parameter (see `logging_string_view_format_2026-09-25.md`, part of this pass).
- Platform resource paths are sized, not null-terminated. The null-terminated
  copy is made only inside the platform call that needs it.
- File system use: the toolset should make resource / file path handling easy
  (the leading `./` stays the convention for resource-relative paths).
- Pending simplification of file and logging management (e.g. the resource
  listing wildcard convention) once the library is solid enough.
- Replace C-string usage across game_server, web_server, platform interfaces,
  and anywhere else found by the pass.
- Open design: the exact toolset (builder / formatter API), and where
  null-terminated conversion for platform calls lives.

## Decisions (2026-10-03)

- First unit of phase 2; everything else in the phase follows it.
- Goals: simplify logging and file management; related functions take string
  views wherever possible.
- Utility functions on views/strings: chop left / right, append left / right,
  and similar. They allocate from a memory arena.
- New "static" memory arena: declared in a function, usable immediately over
  the static / stack memory it occupies (no pre-allocated block to point into).
- Candidate: string builder structure holding the arena reference, so multiple
  / nested operations are easier to write. To be designed.

## Survey (2026-10-03, read-only builder, pre-work state)

- `include/core/string.h` had views, static string, fixed-buffer append; no
  chop / find / view append / signed or hex formatting / formatter.
- `ia_string_new` never copied its source (unused).
- C-string / varargs use: ~85 `server.log/logf` sites in the server, 28 in
  win32 `net.cpp`, ~12 in win32 main, ~14 `ASSERT_MSG`.
- Only the win32 platform truly needs terminators (`fopen_s`,
  `FindFirstFile`, `fputs`, printf-style formatting inside platform logging).
- Stack arenas already possible via `mem_arena_create(buff, size)`; it zeroes
  the whole buffer.
- Suspected lister bugs: wildcard length check inverted for long paths,
  unchecked `path_part` buffer (win32 main).
- Fragile mutual includes: `string.h` / `memory.h` / `core.h`.

## Suggested steps (survey input, Marc decides)

1. View basics. 2. Allocation-free view utilities (chop, find, starts-with).
3. Static arena + arena-backed appends. 4. Number formatting + builder.
5. Sized resource paths + lister rewrite. 6. Web server response building on
views. 7. Logging to a single view, migrate call sites. 8. `ASSERT_MSG`.
9. Final sweep for leftover C strings.

## Open questions (unanswered)

- Does the pass cover `ASSERT_MSG` (varargs, per-platform)? Survey default: no.
- Static arena zeroing: survey default is a non-zeroing variant for scratch.
- Formatter must work in wasm (no varargs)? Survey default: typed append
  calls (builder), not printf-style.

## Progress

- 2026-10-03: Marc did the bulk of the work (uncommitted: `string.h`,
  `memory.h`, new `cstring.h`, platform interface, game server, web server
  http, win32 main, CMakeLists).
- Review by read-only builder in progress; findings to be listed here.

## Why

Removes null-terminator assumptions and C-string handling bugs, and keeps
logging and path handling uniform and portable. Part of the dev
infrastructure phase (dev-plan.md phase 2).
