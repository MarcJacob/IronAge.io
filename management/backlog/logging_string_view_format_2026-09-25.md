# BACKLOG: Logging takes a string view only, formatting moves to platform-independent code

Input date: 2026-09-25

## Item

Logging takes a string view only (no variable arguments) on the server and
server platform; formatting happens in platform-independent code through an
in-house string format implementation (numbers, string views).

## Description

Replace the current varargs-style `log`/`logf` platform calls with calls that
take a single already-formatted string view. Formatting (numbers, string
views, etc.) moves into shared/platform-independent code via a hand-rolled
string format implementation, instead of relying on each platform to support
varargs formatting itself.

## Why

Keeps platform logging endpoints minimal (just "accept and output a string
view") and formatting logic shared/portable instead of duplicated or
platform-dependent, matching the project's general preference for hand-rolled
minimal-dependency infrastructure.
