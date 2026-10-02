// Textual debug panel showing the raw render state, as an alternative to reading it off the canvas.

import * as Backend from "./backend.js"
import * as Core from "./core.js"
import { get_selected_entity_view, get_mouse_world_location } from "./input.js"

let panelElement: HTMLElement | null = null;
let selectedPanelElement: HTMLElement | null = null;
export function init_debug_panel(element: HTMLElement, selected_element: HTMLElement): void
{
    panelElement = element;
    selectedPanelElement = selected_element;

    Core.register_frame_callback((delta_time_s) => {
        // Re-query the selected entity every frame so its values stay live.
        update_selected_entity_panel(get_selected_entity_view());
        update_debug_panel(Backend.LAST_RENDER_STATE, get_mouse_world_location());
    });

}

// Shows the entity's full view as 'name = value' rows in the side panel, hidden when there is none.
// The DOM is only touched when the text changes.
function update_selected_entity_panel(view: Backend.EntityView | null): void
{
    if (selectedPanelElement === null) return;

    if (view === null)
    {
        if (!selectedPanelElement.hidden) selectedPanelElement.hidden = true;
        return;
    }

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

    const text = rows.map(([name, value]) => `${name} = ${value}`).join('\n');
    if (selectedPanelElement.textContent !== text) selectedPanelElement.textContent = text;
    if (selectedPanelElement.hidden) selectedPanelElement.hidden = false;
}

// Renders the given render state (see backend.read_render_state()) as text into the debug panel.
export function update_debug_panel(render_state: Backend.RenderState | null, mouse_world_loc: { x: number, y: number } | null = null): void
{
    if (panelElement === null || render_state == null) return;

    const lines:string[] = [
        `viewport: (${render_state.viewport_bottom_left_x}, ${render_state.viewport_bottom_left_y}) ${render_state.viewport_width}x${render_state.viewport_height}`,
        `mouse world loc: ${mouse_world_loc === null ? 'n/a' : `(${mouse_world_loc.x}, ${mouse_world_loc.y})`}`,
        `entity_count: ${render_state.entity_count}`,
    ];
    for (let i = 0; i < render_state.entity_count; i++)
    {
        const entity:Backend.RenderEntity = render_state.entity_states[i];
        lines.push(`[${i}] type: ${Backend.ENTITY_TYPE[entity.entity_type]}  owner: ${entity.owner}  viewport loc: (${entity.viewport_x}, ${entity.viewport_y})  size: ${entity.size_viewport}`);
    }

    panelElement.textContent = lines.join('\n');
}
