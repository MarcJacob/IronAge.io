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
- Server stays on port 8080 everywhere (tunnel is outbound-only, no 443 needed). `-port=<n>` param (default 8080) for running several instances.
- `ironage.dev` -> rented box's tunnel; dev PC on `test.ironage.dev` via its own tunnel (path routing across machines is not possible).
- Branches: `master` became `dev`; `release-alpha` is advanced onto `dev` by hand (crude versioning), some tagged for major advancements.
- Supervisor: PowerShell script started at boot, loops ~15 s; update check, build, restart, crash restart cap. Calls the existing `.cmd` scripts.
- "Obtain a new build" is a swappable step (`Invoke-FullBuild`): local build first; GitHub Actions release download is the alternative if the box toolchain is painful.
- Build mode: GitHub Actions builds the release; box downloads it (public repo, no token). Box needs no toolchain / git. Falls back to building on the box if the runner can't build.
- No server authentication, intended.
- Provider: OVH Windows VPS (cheaper than the alternatives). Reevaluate when the Linux platform exists (`backlog/linux_game_server_platform_2026-10-17.md`).
- Supervisor is standalone: meant to be copied and run outside the workspace (deploy dir sits next to the script). If run from `scripts/`, `.deploy` must stay out of git (`.gitignore` entry still to add, user).
- Release URL: `https://github.com/<owner>/<repo>/releases/download/build-release-alpha-latest/IronAge_Win32_Game_Server.zip`. Zip is the only deliverable (no workflow artifact). Runner has LLVM 20 (not 18); builds pass with it.
- Update check: poll the release asset download URL (not the API) every ~1-2 min; last-seen ETag/Last-Modified kept in a supervisor variable (no file). Supervisor start therefore always downloads and swaps.
- Swap: hard swap every time. Stop server fully, active dir -> backup (previous backup deleted), staged download -> active dir, launch. No rollback for now: on a failed swap or launch the server stays down (running a backup silently would need a visible indicator; later).
- `front/package.json` now has a real `typescript` dependency (no global `tsc` needed).

## Tasks

1. [DONE] Read-only survey: no code change needed for the tunnel.
2. [DONE] Quick tunnel tested in game.
3. [DONE] Domain + named tunnel, self-hosted.
4. [DONE] Git: rename `master` -> `dev`, create `release-alpha`.
5. [DONE] `-port=<n>` server param.
6. [DONE] Supervisor `scripts/win32_start_supervisor_game_server.ps1`, verified on the dev PC (update, crash restart + cap, graceful stop).
7. [DONE] GitHub Actions workflow `.github/workflows/branch_build_and_release_latest.yaml`: on push to `release-alpha` (or manual on any branch), runs `scripts\win32_build_all.cmd`, publishes `IronAge_Win32_Game_Server.zip` to a rolling pre-release `build-<branch>-latest` (deleted and recreated each run). Tested via manual `dev` run (downloaded and ran locally). Still to check: a real `release-alpha` push produces `build-release-alpha-latest`.
8. [DONE] Supervisor downloads / swaps the latest release (`scripts\.deploy`). Verified on the dev PC with a real `release-alpha` push (workflow trigger, download, swap, restart). `CloseMainWindow` fails when the supervisor spawns the server, so the stop falls back to `Kill()` (shutdown handler skipped); accepted for now. `.deploy` to gitignore (user).
9. [DONE] OVH Windows box rented; `cloudflared` tunnel for `ironage.dev`; dev PC tunnel moved to `test.ironage.dev`.
10. [DONE] Task Scheduler boot task on the box ("Run whether user is logged on or not" required); reboot test passed, unattended; a release-alpha update reaches the live server.
11. [DONE] Release tags: informal, no doc (convention to be written when the first tag is made).

## Open questions

- Box size (small is enough now).
- Zip entry path separators when unpacking (Compress-Archive may store backslashes).

## Notes

- Possible failure signs: 403 "Origin doesn't match Host"; 431 if request head > 2048 bytes (`WEB_CLIENT_RECEPTION_BUFFER_SIZE`, web_server.h:24, shared with WS reception buffer); rare 502 from the 2 s HTTP idle close.
- Limit tweaks belong to `backlog/server_configuration_2026-10-15.md`.
- Build needs `clang++` (LLVM 18) on PATH + MSVC/WinSDK libs, Node, global `tsc` (nothing installs them). Run set = `build/Win32_Game_Server/` (exe, `game_server.dll`, `game_server_resources/web_root`).
- Build scripts skip env setup when `IRONAGE_DEV_SETUP` is inherited (they build whatever repo it points at); supervisor clears it. Same trap for any future script launching a build from an existing shell.
- `web_game_client_build.cmd` swallowing `tsc` failures was fixed (lines 80-88, not yet run).
- Related: `dev_iteration_loop_2026-10-03.md` (build / deploy / hot-reload; the server DLL hot reload may suit updates without restart).

## Box setup (tasks 7-8)

Do after tasks 7-8 work. Decide provider / size first (small box, no toolchain).

Steps (user, on the box):
1. Rent box, RDP, strong admin password.
2. Copy the supervisor script (and the `.cmd` scripts it needs, if any) onto the box.
3. One manual run to prove download + start.
4. `cloudflared` as a service (`cloudflared service install`), tunnel for `ironage.dev` -> `localhost:8080`. Move dev PC tunnel to `test.ironage.dev`.
5. Task Scheduler: trigger at startup, `powershell -ExecutionPolicy Bypass -File <repo>\scripts\win32_start_supervisor_game_server.ps1`, run whether logged on or not.
6. Reboot test: tunnel, supervisor, server all come up unattended.

To verify on the box:
- Graceful stop needs a console window (`CloseMainWindow`); with no logged-in session it may fall back to `Kill()` (shutdown handler skipped). Test under Task Scheduler.
- First run needs a successful release download before the server exists.

## Next step

Unit complete.
