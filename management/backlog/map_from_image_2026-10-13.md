# BACKLOG: Map from an image, and getting it to clients

Input date: 2026-10-13

## Item

Generate the world map from an image instead of only the standard generation in world state init.
Decide how clients receive the image: sent directly by the server, served over HTTP on the side
while in the lobby, or other.

## Why

Needed for designed maps. Clients currently regenerate the terrain from the match start params, which
works only for the standard generation.
