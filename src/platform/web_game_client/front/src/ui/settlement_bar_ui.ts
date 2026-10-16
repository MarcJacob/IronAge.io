// Bottom bar with the actions of the selected settlement or army (static DOM in index.html). Only generates signals: the shown mode
// gives the handlers to call when a button is pressed. Hotkey actions are on the left, committed actions (no hotkey) on the right.

let barElement: HTMLElement | null = null;
let hudElement: HTMLElement | null = null;
let buildRoadButton: HTMLElement | null = null;
let raiseArmyButton: HTMLElement | null = null;
let settleButton: HTMLElement | null = null;
let onBuildRoad: () => void = () => {};
let onRaiseArmy: () => void = () => {};
let onSettle: () => void = () => {};

export function init_settlement_bar_ui(): void
{
    barElement = document.getElementById("game_hud_settlement_bar") as HTMLElement;
    hudElement = document.getElementById("game_hud") as HTMLElement;
    buildRoadButton = document.getElementById("game_hud_build_road") as HTMLElement;
    raiseArmyButton = document.getElementById("game_hud_raise_army") as HTMLElement;
    settleButton = document.getElementById("game_hud_settle") as HTMLElement;

    // Keys should keep going to the game, not re-press the button.
    buildRoadButton.addEventListener('click', () => { buildRoadButton!.blur(); onBuildRoad(); });
    raiseArmyButton.addEventListener('click', () => { raiseArmyButton!.blur(); onRaiseArmy(); });
    settleButton.addEventListener('click', () => { settleButton!.blur(); onSettle(); });
}

// Shows the bar with a settlement's actions. The handlers are called when "Build Road" / "Raise Army" are pressed.
export function show_settlement_bar(on_build_road: () => void, on_raise_army: () => void): void
{
    onBuildRoad = on_build_road;
    onRaiseArmy = on_raise_army;
    show_bar(true);
}

// Shows the bar with an army's actions. on_settle is called when "Settle" is pressed.
export function show_army_bar(on_settle: () => void): void
{
    onSettle = on_settle;
    show_bar(false);
}

function show_bar(forSettlement: boolean): void
{
    if (buildRoadButton !== null) buildRoadButton.hidden = !forSettlement;
    if (raiseArmyButton !== null) raiseArmyButton.hidden = !forSettlement;
    if (settleButton !== null) settleButton.hidden = forSettlement;
    if (barElement !== null) barElement.hidden = false;
    if (hudElement !== null) hudElement.classList.add("has_bottom_bar"); // Makes room for the bar.
}

// Safe to call when not shown.
export function hide_settlement_bar(): void
{
    if (barElement !== null) barElement.hidden = true;
    if (hudElement !== null) hudElement.classList.remove("has_bottom_bar");
}
