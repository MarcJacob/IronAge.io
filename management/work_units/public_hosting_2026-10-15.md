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
- `ironage.dev` -> rented box's tunnel; dev PC on `test.ironage.dev` via its own tunnel (path routing across machines is not possible).
- Branches: `master` becomes `dev`; `release-alpha` is advanced onto `dev` by hand (crude versioning), some tagged for major advancements.
- Box update loop: one long-lived script started at boot (Task Scheduler minimum is 1 min), sleeping ~15 s per cycle: compare local vs origin commit; if different `git fetch` + `reset --hard origin/release-alpha`, stop server, build, start server. Otherwise check the process is alive.
- Crash handling: restart up to N times, then stop until manually fixed.
- "Obtain a new build" is a swappable step: local build first; GitHub Actions release download is the alternative if the box toolchain is painful.
- Repo access from the box: read-only deploy key / token. No server authentication, intended.

## Tasks

1. [DONE] Read-only survey: no code change needed for the tunnel.
2. [DONE] Quick tunnel tested in game.
3. [DONE] Domain + named tunnel, self-hosted.
4. [DONE] Git: rename `master` -> `dev`, create `release-alpha` (user).
5. [DONE] `-port=<n>` server param (user).
6. [ ] Loop script (update check, build, restart, crash cap), tested on the dev PC against a second instance.
7. [ ] Rent the Windows box; install toolchain / git / `cloudflared`; second tunnel for `ironage.dev`; move the dev PC tunnel to its subdomain.
8. [ ] Run the script on the box at boot.
9. [ ] Release tags in the management docs (after all works).

## Open questions

- Loop script language: PowerShell recommended, C (user's idea) as possible later pass.
- Hosting provider / box size (depends on build toolchain).
- Build on the host vs GitHub Actions release; needs the build's toolchain requirements.
- Poll with a token if using the GitHub API (rate limit).

## Notes

- Possible failure signs: 403 "Origin doesn't match Host"; 431 if request head > 2048 bytes (`WEB_CLIENT_RECEPTION_BUFFER_SIZE`, web_server.h:24, shared with WS reception buffer); rare 502 from the 2 s HTTP idle close.
- Limit tweaks belong to `backlog/server_configuration_2026-10-15.md` (its text says port 8000, now 8080; bind address now done; `-port` overlaps it).
- Supervisor script: `scripts/win32_start_supervisor_game_server.ps1` (documented in scripts imprint).
- Build survey: needs `clang++` (LLVM 18) on PATH + MSVC/WinSDK libs, Node, global `tsc` (nothing installs them). Run set = `build/Win32_Game_Server/` (exe, `game_server.dll`, `game_server_resources/web_root`).
- Unattended: build error paths `pause` unconditionally (supervisor: stdin from NUL); exe locked while running, stop before full build; launch the exe directly (`win32_game_server.exe`, instances differ by `-port`); ignored build outputs survive `git reset --hard`.
- To verify: `web_game_client_build.cmd:80-84` may swallow `tsc` failure; linker/SDK discovery on a clean box; graceful stop vs console handler.
- Related: `dev_iteration_loop_2026-10-03.md` (build / deploy / hot-reload; the server DLL hot reload may suit updates without restart).

## Peer brief

Goal: write `scripts/win32_start_supervisor_game_server.ps1` with the user, who wants to learn the PowerShell / Windows process mechanics. Explain as you go; let the user write parts.

Behavior (decided):
- Started once at boot; loops forever, ~15 s sleep per cycle.
- Each cycle: compare local commit with `origin/release-alpha`. On difference: `git fetch`, `git reset --hard origin/release-alpha`, stop server, full build, start server.
- Otherwise check the server process is alive; if not, restart up to N consecutive times, then stop restarting and log until fixed by hand.
- "Obtain a new build" step must be swappable (local build first; GitHub release download maybe later, undecided).
- Log to file + console. Run with `powershell -ExecutionPolicy Bypass -File`.

Constraints:
- Never duplicate build / launch logic: call `scripts/win32_build_all.cmd` etc. Redirect stdin from NUL on those (error paths `pause` unconditionally).
- Be careful with build exit codes: `web_game_client_build.cmd:80-84` may swallow a `tsc` failure. Flag to the user, don't fix silently.
- Stop the server before a full build (exe locked while running).
- Launch `build/Win32_Game_Server/win32_game_server.exe` directly, exe folder as working directory, `-port=<n>` (default 8080). Port, repo path, branch are parameters.
- A failed build must not start a stale mix of leftover outputs; a failed update must not crash-loop.
- Test on the user's PC against a second instance on another port, never the user's running server. No git writes: use a throwaway local remote / branch the user sets up.

Imprints: `scripts/scripts.imprint.md`, `src/platform/win32_game_server/` chain. Scripts: `scripts/win32_build_all.cmd`, `scripts/win32_launch_server_instance.cmd`. Build survey findings: Notes above.

Done when: script updates + restarts on a new commit, restarts a killed server, stops after N failures; all verified on the user's PC; user reviewed.

Open: N and sleep interval values; graceful stop vs hard kill (does a graceful stop reach the console handler? unknown); log file location.

## Next step

Design the supervisor (task 6): local-build mode first, update step swappable.
