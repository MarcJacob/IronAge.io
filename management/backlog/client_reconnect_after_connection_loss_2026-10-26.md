# BACKLOG: Client reconnect after connection loss

Input date: 2026-10-26

## Item

The client has no path back to the "connecting" state after the socket closes; login / lobby list show "Connection lost. Reload the page to reconnect."

## Notes

- `AUTHENTICATED` already has a reserved `auth_key` field (zero for now) meant for quick reconnect (key stored in the browser).
- Related: `persistent_client_identity_2026-09-24.md`.

## Next step

Not started.
