// In-match UI (HUD overlay on top of the canvas). An extension of input.ts: it works from input.ts's state (selection) and only
// generates signals (e.g. button clicks calling input.ts's send_* functions). It never writes input events itself.
// DOM is only touched when the text / visibility actually changed.

import * as Backend from "./backend.js"
import { get_selected_entity_guid, get_selected_entity_view, send_found_settlement } from "./input.js"
import { get_local_player_id } from "./main.js"

let selectedPanelElement: HTMLElement | null = null;
let selectedTextElement: HTMLElement | null = null;
let foundSettlementButton: HTMLElement | null = null;

export function init_game_ui(): void
{
    selectedPanelElement = document.getElementById("game_hud_selected_panel") as HTMLElement,
    selectedTextElement = document.getElementById("game_hud_selected_text") as HTMLElement,
    foundSettlementButton = document.getElementById("game_hud_found_settlement") as HTMLElement

    foundSettlementButton.addEventListener('click', () => {
        const guid = get_selected_entity_guid();
        if (guid !== null) send_found_settlement(guid);
    });
}

// Sets an element's hidden state, only if it differs.
function set_hidden(element: HTMLElement, hidden: boolean): void
{
    if (element.hidden !== hidden) element.hidden = hidden;
}

// Builds the 'name = value' rows describing an entity's full view.
function entity_view_text(view: Backend.EntityView): string
{
    const rows: [string, string | number][] = [
        ["entity_type", Backend.ENTITY_TYPE[view.entity_type]],
        ["guid", "0x" + view.guid.toString(16)],
        ["location", `(${view.location_x}, ${view.location_y})`],
        ["owner", view.owner],
    ];
    if (view.target_entity !== null) rows.push(["target_entity", "0x" + view.target_entity.toString(16)]);
    if (view.target_location !== null) rows.push(["target_location", `(${view.target_location.x}, ${view.target_location.y})`]);
    rows.push(["travel_speed", view.travel_speed]);

    // Only the block matching entity_type is set (see backend.EntityView). Null fields (no target) are skipped.
    const typeBlock = view.settlement ?? view.caravan ?? view.army ?? {};
    for (const [name, value] of Object.entries(typeBlock))
    {
        if (value !== undefined && value != null) rows.push([name, value as string | number]);
    }

    return rows.map(([name, value]) => `${name} = ${value}`).join('\n');
}

// Per-frame refresh. Call once the match is running.
export function update_game_ui(): void
{
    if (selectedPanelElement === null || selectedTextElement === null || foundSettlementButton === null) return;

    // Re-query the selected entity every frame so its values stay live.
    const view = get_selected_entity_view();

    set_hidden(selectedPanelElement, view === null);
    if (view === null) return;

    const text = entity_view_text(view);
    if (selectedTextElement.textContent !== text) selectedTextElement.textContent = text;

    const canFound = view.entity_type === Backend.ENTITY_TYPE.ARMY && view.owner === get_local_player_id();
    set_hidden(foundSettlementButton, !canFound);
}
