# ARTISAN SKILL: add a player action to the web client

Derive a new player action (anything the player triggers: a button, a hotkey, a right-click
order, an interactive mode) that sends a match command and/or shows UI. Covers the web client
only. If the command does not exist yet, read `add_match_command.md` first and add it
(step 1 below needs it), then come back here.

Precedents: FOUND_SETTLEMENT (button, no mode: `army_selected.ts`), RAISE_ARMY (hotkey +
mode with preview: `army_raising.ts`), BUILD_ROAD (hotkey + mode: `road_building.ts`),
SET_ENTITY_MOVE_TARGET / SET_ENTITY_ATTACK_TARGET (right-click orders: `army_selected.ts`).
Rewritten 2026-10-18; re-verify symbols against code.

Folders: *src/platform/web_game_client/front/src/* (TS, 4 spaces) and *web/* (html, css),
plus *src/platform/web_game_client/web_game_client_input_commands.cpp* (one case).
Read first: *front/front.imprint.md*, *front/src/front_src.imprint.md*.

## How a command is sent

`send_command("NAME", { member: value, ... })` in *input.ts* (generic, knows no command):
asks the wasm client to create a GDO of the command type named `NAME` (the bare enum
identifier, the same as the command table row), sets each property by name, and sends it.
A value is a number, or `{x, y}` for a world location. A wrong command or property name
logs the command's property list in the console and sends nothing (the TS is out of date).
Nothing about byte offsets lives in TS. Call sites build the property object themselves.

## Steps

1. Wasm side, one case: add the command to the switch in `gdo_buffer_create`
   (*web_game_client_input_commands.cpp*), copying a precedent. The GDO definition itself
   lives with the command struct in *commands.h* (see `add_match_command.md`). The GDO's
   property names are what call sites pass to `send_command`. A new validity error bit must
   also be mirrored in *input.ts* (`COMMAND_ERROR_*`).
2. Pick the trigger, as the task says:
   - Button / hotkey: an item in the owning mode's action bar state (below).
   - Right-click order on the selected entity: the owning mode handles it in its
     `on_context_menu` (copy `army_selected.ts`): `Render.page_to_tile` for the world tile,
     `pick_entity_at(x, y, exclude_owner?)` for targets, then `send_command`.
   - Interactive mode (targeting, preview, stays or exits after the order): a new class in
     *modes/*, copy `road_building.ts` (stays active) or `army_raising.ts` (exits after one
     order). It holds its hint, its exit key, per-frame preview, `on_click`, `on_context_menu`.
3. Action bar (bottom bar, *ui/ui_element_action_bar.ts*): the owning mode keeps its own
   `ActionBarState` (items: `name`, `hotkey` as a `KeyboardEvent.code` for display only,
   `state` selectable / unselectable / selected, `on_activate`). It calls `submit_action_bar`
   on entry and on resume and `submit_action_bar(null)` on exit. Items with a hotkey go left,
   without go right (placeholder). A child mode leaves the bar alone (the parent shows the
   active item as selected) or submits its own and the parent re-submits on resume.
4. Hotkeys are handled by the mode in its own `on_key_down` (find its own item by hotkey, run
   `on_activate`, ignore key repeats, return true only when handled). Keep hotkeys clear of
   the camera keys (WASD, arrows, numpad + and -).
5. Starting a child mode: `push_mode(new Child(...), this)`; the second argument is the mode
   that is starting it. Modes above that parent are popped first, so a mode never stacks on a
   sibling. A mode returns true only for inputs it owns (its exit key, its clicks); other keys
   fall through to the parent (this is how pressing another action's hotkey switches modes).
6. Hint line (*ui/ui_element_hint.ts*): `submit_hint({ text })` on entry / resume, `null` on
   exit. Build key texts with `hotkey_text(code)`, never hard-code "[1]".
7. Validity and feedback: `is_command_valid("NAME", {...})` (bool, uses the server's check) for
   simple previews; `get_command_feedback("NAME", {...})` returns `{ valid, values }`, the
   `values` being the fields of the command's validity feedback struct by name
   (`error_flags`, `invalid_path_reason`, `population_needed`, ...). Flag bits are mirrored by
   hand in *input.ts* (`COMMAND_ERROR_*`): keep them in sync with *commands.h*. No flag set means
   valid. How to show the feedback is decided per mode / action; the raise army mode
   (`army_raising.ts`) is an example of using it.
   Unset properties of a half-built command are zero.
8. State helpers: `get_local_player_id()`, `is_own_settlement` / `is_own_army` (*main.ts*),
   `query_entity(guid)` (*gdo.ts*: `EntityData` with `entity_type`, `get_number(name)`,
   `get_location(name)` over the entity's GDO properties, named after the C++ fields),
   `get_selected_entity_guid()`, `pick_entity_at(...)`.

## Checks

- Property names in the TS object == the command struct member names; command name == the
  command table row name.
- The mode returns true / false correctly for each handler (see the mode dispatch in the imprint).
- Hotkey texts come from `hotkey_text`; the bar and hint are cleared on exit.
- Mode tree rules in *front_src.imprint.md* respected (children pop themselves when the state
  stops matching; a mode that must not be left overrides Escape).
- The browser may serve cached modules after a redeploy: hard reload. The deployment copy never
  deletes files, so stale generated `.js` files can remain.

## No precedent: stop and report

- New UI elements (a new `ui_element_*` module) or render options: read
  `web_client_add_element.md`. New action bar capabilities: stop and report.
- Changes to the mode system (core.ts) or to input dispatch.
- Anything that needs a new wasm export or a change to the GDO system.
- Showing validity feedback to the player (design decided per mode / action).
- Actions that send several commands or depend on server messages.
