# BACKLOG: WASM client backend logging to the JS frontend

Input date: 2026-09-22

## Item

Typed logging call in the client backend (mirroring the server's `LOG_TYPE`),
routed by the wasm platform layer to the browser console or a future in-page
debug log.

## Why

The client backend has no logging; debugging relies on asserts or ad hoc
`console.log`.
