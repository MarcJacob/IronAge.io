# Win32 Game Server hosting (HTTPS / WSS)

Input date: 2026-10-15

## Goal

1. Let people on other networks / machines reach the Win32 Game Server through a secure link (HTTPS / WSS), for testing.
2. Online hosting that pulls a specific git branch, builds and starts the server, so pushing to git makes the update testable online from anywhere a bit later.

## Decisions

- Cloudflare Tunnel (`cloudflared`) over Caddy / nginx / hand-rolled TLS.
- Domain `ironage.dev` bought and set up on Cloudflare; named tunnel set up, server self-hosted for now.
- No TLS code in the server; it speaks plain HTTP / WS. Origin check compares Origin to Host: don't set `httpHostHeader`.
- Server binds loopback only; no router port forward.

## Tasks

1. [DONE] Read-only survey: no code change needed for the tunnel.
2. [DONE] Quick tunnel tested in game.
3. [DONE] Domain + named tunnel, self-hosted.
4. [ ] Online hosting with branch-driven deploy.
   - Hosting service / machine (must run the Win32 server build): to choose.
   - Deploy branch name; build + start script; how updates restart the server (match in progress?); tunnel runs on the host.

## Open questions

- Hosting service choice and its cost.
- Where the build happens (on the host vs CI) and with which toolchain after the build system revamp.
- Pull mechanism: polling, webhook or CI-triggered.
- Access control: server has no authentication.

## Notes

- Possible failure signs: 403 "Origin doesn't match Host"; 431 if request head > 2048 bytes (`WEB_CLIENT_RECEPTION_BUFFER_SIZE`, web_server.h:24, shared with WS reception buffer); rare 502 from the 2 s HTTP idle close.
- Limit tweaks belong to `backlog/server_configuration_2026-10-15.md` (its text says port 8000, now 8080; bind address now done).
- Related: `dev_iteration_loop_2026-10-03.md` (build / deploy / hot-reload; the server DLL hot reload may suit updates without restart).

## Next step

Discuss task 4: hosting options, then a design for build + deploy.
