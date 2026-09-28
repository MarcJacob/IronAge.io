# GAME WEB CLIENT SOURCES

This folder contains the web-client-side implementation of the game.

It is split in three parts: the *Front* code, the *Web* resources and the *Backend* code.

## Front Code

The *front* folder contains the Typescript sources of the web browser frontend directly presented to clients.
It gets "compiled" into *.js* then stored in *web/src/* to server as the webpage source code.

## Backend Code

Located directly in this folder, some C++ code exists to manage a web-facing client's backend memory and logic, meant to be compiled into Web Assembly,
although the intention is to eventually shift its design towards being platform-agnostic and reusable for a native client at some point.

## Web resources folder

The *web* folder contains all web resources we want to issue to the game server for serving to the clients.
Contains the source *.js* files compiled from Front, the *html* and *css* files and accompanying resources such as images / icons.

The *src/* folder within contains the source *.js* files compiled from the *front/src/* folder Typescript files.
