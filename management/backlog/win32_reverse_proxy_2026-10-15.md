
# BACKLOG: Win32 Game Server Reverse Proxy

Input date: 2026-10-15

## Item

Establish a reverse proxy & secured HTTPS / WSS domain for hosting the Win32 Game Server.

Look into using [Caddy](https://caddyserver.com/docs/quick-starts/https) as it seems like the best option. Otherwise, look into nginx. Rolling this by hand could be an interesting exercize but it seems like an actual security concern to do so...

## Why

To facilitate testing across many networks / machines, the server needs to be seen as secure. Furthermore, the earlier the domain can be secured, the better.
