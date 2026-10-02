// Textual debug panel showing the raw render state, as an alternative to reading it off the canvas.

import * as Backend from "./backend.js"
import * as Core from "./core.js"
import { get_selected_entity_view, get_mouse_world_location } from "./input.js"

let panelElement: HTMLElement | null = null;
export function init_debug_panel(element: HTMLElement): void
{
    panelElement = element;

    Core.register_frame_callback((delta_time_s) => {
        // Re-query the selected entity every frame so its values stay live. Clear the selection if it is gone.
        const selectedEntityView = get_selected_entity_view();
        update_debug_panel(Backend.LAST_RENDER_STATE, get_mouse_world_location(), selectedEntityView);
    });

}

// Renders the given render state (see backend.read_render_state()) as text into the debug panel.
export function update_debug_panel(render_state: Backend.RenderState | null, mouse_world_loc: { x: number, y: number } | null = null,
    selected_entity: Backend.EntityView | null = null): void
{
    if (panelElement === null || render_state == null) return;

    const lines:string[] = [
        `selected: ${selected_entity === null ? 'none' : JSON.stringify(selected_entity)}`,
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
