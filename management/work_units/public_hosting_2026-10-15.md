# Public hosting (HTTPS / WSS, online server, branch-driven deploy)

Input date: 2026-10-15

## Goal

1. Let people on other networks / machines reach the Win32 Game Server through a secure link (HTTPS / WSS), for testing.
2. A rented Windows box serves `ironage.dev`; pushing to the release branch updates it automatically a bit later.
3. The home dev PC stays reachable on a separate hostname for iterating on a home-run server.

## Decisions

- Cloudflare Tunnel (`cloudflared`) over Caddy / nginx / hand-rolled TLS.
- Domain `ironage.dev` bought and set up on Cloudflare; named tunnel set up, server self-hosted for now.
- No TLS code in the server; it speaks plain HTTP / WS. Origin check compares Origin to Host: don't set `httpHostHeader`.
- Server binds loopback only; no router port forward.
- Server stays on port 8080 everywhere (tunnel is outbound-only, no 443 needed). `-port=<n>` param (default 8080) added by the user, for running several instances.
- `ironage.dev` -> rented box's tunnel; dev PC on a short subdomain of its own tunnel (path routing across machines is not possible). Name to pick.
- Branches: `master` becomes `dev`; `release-alpha` is advanced onto `dev` by hand (crude versioning), some tagged for major advancements.
- Box update loop: one long-lived script started at boot (Task Scheduler minimum is 1 min), sleeping ~15 s per cycle: compare local vs origin commit; if different `git fetch` + `reset --hard origin/release-alpha`, stop server, build, start server. Otherwise check the process is alive.
- Crash handling: restart up to N times, then stop until manually fixed.
- "Obtain a new build" is a swappable step: local build first; GitHub Actions release download is the alternative if the box toolchain is painful.
- Repo access from the box: read-only deploy key / token. No server authentication, intended.

## Tasks

1. [DONE] Read-only survey: no code change needed for the tunnel.
2. [DONE] Quick tunnel tested in game.
3. [DONE] Domain + named tunnel, self-hosted.
4. [ ] Git: rename `master` -> `dev`, create `release-alpha` (user).
5. [ ] `-port=<n>` server param (user).
6. [ ] Loop script (update check, build, restart, crash cap), tested on the dev PC against a second instance.
7. [ ] Rent the Windows box; install toolchain / git / `cloudflared`; second tunnel for `ironage.dev`; move the dev PC tunnel to its subdomain.
8. [ ] Run the script on the box at boot.
9. [ ] Release tags in the management docs (after all works).

## Open questions

- Dev PC subdomain name.
- Hosting provider / box size (depends on build toolchain).
- Build on the host vs GitHub Actions release; needs the build's toolchain requirements.
- Poll with a token if using the GitHub API (rate limit).

## Notes

- Possible failure signs: 403 "Origin doesn't match Host"; 431 if request head > 2048 bytes (`WEB_CLIENT_RECEPTION_BUFFER_SIZE`, web_server.h:24, shared with WS reception buffer); rare 502 from the 2 s HTTP idle close.
- Limit tweaks belong to `backlog/server_configuration_2026-10-15.md` (its text says port 8000, now 8080; bind address now done; `-port` overlaps it).
- Related: `dev_iteration_loop_2026-10-03.md` (build / deploy / hot-reload; the server DLL hot reload may suit updates without restart).

## Next step

User does tasks 4-5; then design the loop script (task 6).
