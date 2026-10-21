# Debug / test commands

Raised: 2026-10-21.

- Way to issue game commands without building UI for each (e.g. `send_command` is inside a JS module, so not reachable from the browser console).
- Idea: expose a debug entry point (console-reachable) or a small dev command panel; ties to server dev mode.
- Also wanted: a getter for worker progress %, or a common progress value on all workers (GDO).
