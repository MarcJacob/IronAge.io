# BACKLOG: WASM client backend logging to the JS frontend

Input date: 2026-09-22

## Item

WASM client backend: log messages (typed, like the server's `LOG_TYPE`)
straight to the JS frontend.

## Description

Give the client backend a typed logging call (mirroring the server's
`LOG_TYPE`) that the WASM platform layer routes to the browser console (or a
future in-page debug log), instead of the backend having no way to surface
diagnostic messages at all.

## Why

The client backend currently has no logging story whatsoever - debugging
relies entirely on asserts/traps or manual `console.log`s added ad hoc on the
JS side. A real logging path would make client-side issues much easier to
diagnose.
