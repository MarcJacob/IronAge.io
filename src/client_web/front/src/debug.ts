// Textual debug panel showing the raw render state, as an alternative to reading it off the canvas.

import * as Backend from "./backend.js"

let panelElement: HTMLElement | null = null;
export function init_debug_panel(element: HTMLElement): void
{
    panelElement = element;
}

// Renders the given render state (see backend.read_render_state()) as text into the debug panel.
export function update_debug_panel(render_state: Backend.BackendRenderState): void
{
    if (panelElement === null) return;

    const lines:string[] = [`entity_count: ${render_state.entity_count}`];
    for (let i = 0; i < render_state.entity_count; i++)
    {
        const entity:Backend.BackendEntityState = render_state.entity_states[i];
        lines.push(`[${i}] loc: (${entity.location.x}, ${entity.location.y})  target: (${entity.target_location.x}, ${entity.target_location.y})`);
    }

    panelElement.textContent = lines.join('\n');
}
