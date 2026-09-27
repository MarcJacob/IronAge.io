// Textual debug panel showing the raw render state, as an alternative to reading it off the canvas.

let panelElement = null;

export function init_debug_panel(element) {
    panelElement = element;
}

// Renders the given render state (see backend.read_render_state()) as text into the debug panel.
export function update_debug_panel(render_state) {
    if (panelElement === null) return;

    const lines = [`entity_count: ${render_state.entity_count}`];
    for (let i = 0; i < render_state.entity_count; i++) {
        const entity = render_state.entity_states[i];
        lines.push(`[${i}] loc: (${entity.loc.x}, ${entity.loc.y})  target: (${entity.target_loc.x}, ${entity.target_loc.y})`);
    }

    panelElement.textContent = lines.join('\n');
}
