// UI element: the advanced action panel, above the bottom action bar (container in index.html, content built here).
// Holds the controls of a complex action (e.g. raising an army) while that action's mode is active. Its content is a PanelState,
// the same as the properties panel (stat rows, gauges with sliders), rendered by the same code.
// The action's mode keeps its own state, submits it with submit_advanced_action_panel (every frame is fine) and submits null on exit.

import { PanelView, type PanelState } from "./ui_element_properties_panel.js"

let panelView: PanelView | null = null;

export function init_ui_element_advanced_action_panel(): void
{
    panelView = new PanelView(
        document.getElementById("game_hud_advanced_action_panel") as HTMLElement,
        document.getElementById("game_hud_advanced_action_title") as HTMLElement,
        document.getElementById("game_hud_advanced_action_content") as HTMLElement);
}

// Makes the panel show the given state. Null clears it (hidden).
export function submit_advanced_action_panel(state: PanelState | null): void
{
    panelView?.submit(state);
}
