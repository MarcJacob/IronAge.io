# ARTISAN SKILL: add an interface or rendered element to the web client

Add something the player sees, independently of any action: a DOM widget (panel, bar, text) or
a canvas drawing option. Actions that use it (buttons, hotkeys, orders) are
`web_client_add_action.md`, which depends on this one when a new element is needed.
Written 2026-10-18; re-verify symbols against code.

Folders: *src/platform/web_game_client/front/src/* (TS, 4 spaces) and *web/* (html, css).
Read first: *front/front.imprint.md*, *front/src/front_src.imprint.md*.

## A. A DOM interface element

Precedents: *ui/ui_element_hint.ts* (simplest: one text), *ui/ui_element_action_bar.ts*
(dynamic content with callbacks).

1. Markup: an empty container with an id in *web/index.html* (hidden by default). Style in
   *web/style.css* (keep the bottom / top layout classes the other elements use).
2. A module *ui/ui_element_<name>.ts*. It owns its DOM and exports:
   - a state type describing everything the element can show;
   - `submit_<name>(state | null)`: renders the state (null hides / clears it). Rendering is a
     pure function of the submitted state;
   - `init_ui_element_<name>()`: looks up the DOM nodes; called once from *main.ts* with the
     other init calls.
   - callbacks inside the state for the events it emits (e.g. `on_activate`).
3. The module never sends commands, switches modes, or knows about modes. It only renders its
   state and emits callbacks.
4. Whoever uses it (a mode) keeps its own full state, submits it on entry and on resume, and
   clears it on exit. No mode base class. See the Modes section of *front_src.imprint.md*.
5. Strings: use `hotkey_text(code)` (action bar module) for key labels.

## B. A rendered (canvas) option

1. *render.ts* draws from the backend's render state and `RENDER_CONFIG` (a plain object modes
   mutate: layers, entity types, tints, outlines, previews, overlays). Add a field to
   `RENDER_CONFIG` with a default that draws the normal game view, and the drawing code that
   reads it. `render.ts` never reads mode or input state.
2. A mode sets the field on entry / resume and undoes it on exit.
3. Coordinate conversions already exist in render.ts (`page_to_tile`, `world_to_viewport`, ...):
   use them.

## Checks

- Default state (nothing submitted / default config) looks exactly like before.
- Stack behavior: a child mode overriding the element leaves the parent's state intact, and the
  parent re-submits on resume.
- The deployment copy never deletes files (stale `.js` can remain); hard reload after changes;
  the web server has a file count limit.

## No precedent: stop and report

- New data from the backend (render state or entity views): memory layouts are mirrored by hand in
  *backend.ts* and the C++ side.
- New entity visuals, sprites, new canvas layers or assets.
- Changes to the mode system or input dispatch.
- Elements that need server messages.
