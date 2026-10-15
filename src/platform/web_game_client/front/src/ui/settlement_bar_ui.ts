// Bottom bar with the actions of the selected settlement (static DOM in index.html). Only generates signals: the shown mode
// gives the handler to call when "Build Road" is pressed.

let barElement: HTMLElement | null = null;
let hudElement: HTMLElement | null = null;
let onBuildRoad: () => void = () => {};

export function init_settlement_bar_ui(): void
{
    barElement = document.getElementById("game_hud_settlement_bar") as HTMLElement;
    hudElement = document.getElementById("game_hud") as HTMLElement;

    const buildRoadButton = document.getElementById("game_hud_build_road") as HTMLElement;
    buildRoadButton.addEventListener('click', () => {
        buildRoadButton.blur(); // Keys should keep going to the game, not re-press the button.
        onBuildRoad();
    });
}

// Shows the bar. on_build_road is called when its "Build Road" button is pressed.
export function show_settlement_bar(on_build_road: () => void): void
{
    onBuildRoad = on_build_road;
    if (barElement !== null) barElement.hidden = false;
    if (hudElement !== null) hudElement.classList.add("has_bottom_bar"); // Makes room for the bar.
}

// Safe to call when not shown.
export function hide_settlement_bar(): void
{
    if (barElement !== null) barElement.hidden = true;
    if (hudElement !== null) hudElement.classList.remove("has_bottom_bar");
}
