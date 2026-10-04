# BACKLOG: Finalized string formatter

Input date: 2026-10-04

## Item

Finalize the formatter (redesign if needed): portable (no C varargs, works on
wasm), keeps `%` syntax.
- `%s` accepts views, strings and `const char*`.
- Support remaining indicators: floats, hex, 64-bit signed, sized string.
- Assert on mismatched specifier / argument type, missing or extra argument,
  unsupported specifier.
- Overflow: builder / fixed buffers assert; logging truncates.

Then convert dependents:
- `ASSERT_MSG`, on all platforms.
- Request parsing helpers in `web_server_http.cpp` onto bounded views.
- Check arena `clear()` callers do not rely on zeroed raw allocations.
- Final sweep for leftover C strings.

## Why

Removes `%s` / `%cs` mix-ups and ABI dependence; one formatter for logging,
asserts and the wasm client. Origin: `work_units/string_pass_2026-10-03.md`.
