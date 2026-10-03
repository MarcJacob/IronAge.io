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

## Why

Removes null-terminator assumptions and C-string handling bugs, and keeps
logging and path handling uniform and portable. Part of the dev
infrastructure phase (dev-plan.md phase 2).
