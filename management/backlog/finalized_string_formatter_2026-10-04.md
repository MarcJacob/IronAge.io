# BACKLOG: Finalized string formatter

Input date: 2026-10-04

## Item

Finalize the string formatter: redesign it if necessary, and support the other
type indicators.

## Objectives

- Type-aware formatter that is portable (no C varargs, works on wasm), keeping
  the `%` format syntax.
- `%s` accepts views, strings and `const char*`.
- Support the other type indicators (floats, hex, 64-bit signed, sized string
  specifier).
- Mismatched specifier / argument type, missing or extra argument, unsupported
  specifier: assert.
- Overflow: builder / fixed buffers assert; logging truncates.
- Then convert what depends on it:
  - `ASSERT_MSG`, on all platforms.
  - Request parsing helpers in `web_server_http.cpp` onto bounded views.
  - Check arena `clear()` callers do not rely on zeroed raw allocations.
  - Final sweep for leftover C strings.

## Why

Removes the `%s` / `%cs` mix-ups and ABI dependence, and lets logging, assert
messages and the wasm client share one formatter. Origin:
`work_units/string_pass_2026-10-03.md`.
