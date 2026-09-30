// Textual debug panel showing the raw render state, as an alternative to reading it off the canvas.
import * as Backend from "./backend.js";
let panelElement = null;
export function init_debug_panel(element) {
    panelElement = element;
}
// Renders the given render state (see backend.read_render_state()) as text into the debug panel.
export function update_debug_panel(render_state, mouse_world_loc = null) {
    if (panelElement === null)
        return;
    const lines = [
        `viewport: (${render_state.viewport_bottom_left_x}, ${render_state.viewport_bottom_left_y}) ${render_state.viewport_width}x${render_state.viewport_height}`,
        `mouse world loc: ${mouse_world_loc === null ? 'n/a' : `(${mouse_world_loc.x}, ${mouse_world_loc.y})`}`,
        `entity_count: ${render_state.entity_count}`,
    ];
    for (let i = 0; i < render_state.entity_count; i++) {
        const entity = render_state.entity_states[i];
        lines.push(`[${i}] type: ${Backend.ENTITY_TYPE[entity.entity_type]}  owner: ${entity.owner}  viewport loc: (${entity.viewport_x}, ${entity.viewport_y})  size: ${entity.size_viewport}`);
    }
    panelElement.textContent = lines.join('\n');
}
