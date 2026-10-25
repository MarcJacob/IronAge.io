// UI module (new style): the player state panel, bottom left, always visible during play. Facts about the local player.
// Static markup in index.html (one element per fact, ids below), extracted once by init. submit(state) shows it, submit(null) hides it.
// Pure display, emits nothing. To add a fact: add its element to index.html, its id to IDS / el, its field to the state, and write it in submit.
// submit can be called every frame (the DOM is only touched when a text or tone changes).

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_player_state",
    gold: "player_state_gold",
    gold_delta: "player_state_gold_delta",
    settlements: "player_state_settlements",
    population: "player_state_population",
    soldiers: "player_state_soldiers",
};

export interface PlayerStateState
{
    gold: number;
    gold_per_day: number; // Signed net change of the gold per day (income - expenses).
    settlements: number;
    population: number;
    soldiers: number;
}

let el: {
    root: HTMLElement;
    gold: HTMLElement;
    gold_delta: HTMLElement;
    settlements: HTMLElement;
    population: HTMLElement;
    soldiers: HTMLElement;
} | null = null;

function init(): void
{
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        gold: document.getElementById(IDS.gold) as HTMLElement,
        gold_delta: document.getElementById(IDS.gold_delta) as HTMLElement,
        settlements: document.getElementById(IDS.settlements) as HTMLElement,
        population: document.getElementById(IDS.population) as HTMLElement,
        soldiers: document.getElementById(IDS.soldiers) as HTMLElement,
    };
}

export function submit(state: PlayerStateState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    set_text(el.gold, String(state.gold));
    set_text(el.settlements, String(state.settlements));
    set_text(el.population, String(state.population));
    set_text(el.soldiers, String(state.soldiers));

    // A signed per-day change: "+N / day" green, "-N / day" red, grey at 0.
    const change = state.gold_per_day;
    set_text(el.gold_delta, change < 0 ? `${change} / day` : `+${change} / day`);
    const className = "player_state_delta " + (change > 0 ? "up" : change < 0 ? "down" : "flat");
    if (el.gold_delta.className !== className) el.gold_delta.className = className;
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

init();
