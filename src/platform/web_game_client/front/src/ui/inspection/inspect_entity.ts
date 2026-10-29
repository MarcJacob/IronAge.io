// Entity inspection: the entry points modes call to show something in the bottom center of the HUD (the entity frame and one panel inside it).
//   show_entity(view, context)                   the panel of the entity's type. show_entity(null) hides everything.
//   show_raise_army_panel(settlement, targets)   the settlement's "Raise army" panel (Raise army mode).
//   show_found_settlement_panel(state)           the settlement's "Found settlement" panel (Found settlement mode).
// Routing (show below) is the one place deciding what that area shows: every panel is submitted on every call, all but the shown one with null,
// so panels never conflict. Modes never submit the frame or a panel directly.
// Display only: whether the local player may act on the entity is decided by the caller, which turns editing on through context flags
// and subscribes to the panel's events. Everything inspection-related lives in this folder.
// To add a panel: a ui_element_<name>_panel.ts module with its static markup inside the frame (index.html), a state builder below,
// a field in Shown and a line in show. Its events are subscribed by the mode owning it.

import * as Backend from "../../backend.js"
import { query_entity, get_building_definition, get_game_parameters, get_local_player_state, type EntityData } from "../../gdo.js"
import * as EntityFrameUI from "./ui_element_entity_frame.js"
import * as SettlementPanelUI from "./ui_element_settlement_panel.js"
import * as ArmyPanelUI from "./ui_element_army_panel.js"
import * as CaravanPanelUI from "./ui_element_caravan_panel.js"
import * as WorkerPanelUI from "./ui_element_worker_panel.js"
import * as BattlePanelUI from "./ui_element_battle_panel.js"
import * as RaiseArmyPanelUI from "./ui_element_raise_army_panel.js"
import * as FoundSettlementPanelUI from "./ui_element_found_settlement_panel.js"
import { raise_available, TROOPS, MAX_COUNT } from "./section_troops.js"
import { troops_consumption } from "./section_supplies.js"
import { settlement_text } from "./section_trade.js"

export interface InspectionContext
{
    // Settlement panel: own settlement. Makes the tax, export and ceiling sliders interactive and shows the build buttons.
    // The mode owning the panel subscribes to SettlementPanelUI "tax_rate", "export_ratio", "ceiling" and "build".
    edit_settlement?: boolean;

    // Settlement panel with edit_settlement: whether a project of each building type (index = row of the building table) would be accepted now
    // (its button is disabled otherwise). Without it the buttons are always enabled.
    buildable?: boolean[];

    // Army being raised (army panel): makes the target sliders interactive. The mode owning the panel subscribes to ArmyPanelUI "raise_targets".
    edit_raise_targets?: boolean;
}

// The owner's name, for entities that have an owner property. Empty if unowned or the name is unknown.
function owner_name(view: EntityData): string
{
    if (!view.properties.some(property => property.name === "owner")) return "";
    return Backend.get_player_name(view.get_number("owner"));
}

// Settlement panel. All information is shown for any settlement. Only the controls depend on edit_settlement: sliders (read-only otherwise) and build buttons.
function settlement_panel_state(settlement: EntityData, context: InspectionContext): SettlementPanelUI.SettlementPanelState
{
    const editable = context.edit_settlement === true;
    const prop = (name: string) => settlement.get_number(name);

    // Levies have no home pool (always 0 at home), so the home soldiers are the pooled types.
    const soldiers_home = prop("troops_men_at_arms") + prop("troops_horsemen") + prop("troops_knights");
    const raised = prop("raised_levies") + prop("raised_men_at_arms") + prop("raised_horsemen") + prop("raised_knights");
    const parameters = get_game_parameters();

    // Ongoing projects by building type, as their progress (0..255).
    const project_types = settlement.extra?.get_array("construction_project_building_type") ?? [];
    const project_progress = settlement.extra?.get_array("construction_project_progress") ?? [];
    const project_count = settlement.extra?.get_number("construction_project_count") ?? 0;

    return {
        population: Math.max(0, prop("population") - raised),
        raised,
        peasants: prop("pop_peasants"),
        peasants_cap: prop("pop_peasants_cap"),
        urban_cap: prop("pop_urban_cap"),
        workers: prop("pop_workers"),
        soldiers_home,
        unemployed: prop("pop_unemployed"),
        caravans: { active: prop("trading_caravans_active"), max: prop("trading_caravans_max") },
        ships: { active: prop("trading_ships_active"), max: prop("trading_ships_max") },
        // Server per-mille rate (guarded against an empty settlement there).
        growth_percent: prop("population_growth_rate") / 10,
        production: prop("economy_production"),
        tax: prop("economy_tax"),
        tax_rate: prop("tax_rate"),
        tax_rate_min: parameters.get_number("tax_rate_min"),
        tax_rate_max: parameters.get_number("tax_rate_max"),
        consumption: {
            sold: Math.min(prop("economy_consumption"), prop("economy_sellable")),
            needed: prop("economy_consumption"),
            population_change: prop("population_change"),
        },
        export_ratio: { ratio: prop("trading_export_ratio"), max: parameters.get_number("trade_export_ratio_max_percent") },
        market_goods: { goods: prop("trading_market_goods"), export_goods: prop("trading_export_goods"), capacity: prop("market_capacity") },
        trade_volume: { volume: prop("trading_trade_volume"), max: prop("trade_volume_max") },
        // Pooled types (no levies). A type with no max, nobody at home and nobody raised is left out (nothing to show or set yet).
        garrison: TROOPS.slice(1).map((troop): SettlementPanelUI.SettlementGarrisonState | null => {
            const home = prop(`troops_${troop.name}`);
            const troop_raised = prop(`raised_${troop.name}`);
            const max = prop(`max_${troop.name}`);
            if (max === 0 && home === 0 && troop_raised === 0) return null;
            return { home, raised: troop_raised, max, ceiling: prop(`ceiling_${troop.name}`) };
        }),
        buildings: settlement.get_array("building_counts").map((count, building_type): SettlementPanelUI.SettlementBuildingState | null => {
            const definition = get_building_definition(building_type);
            if (definition === null) return null;

            const projects: number[] = [];
            for (let project = 0; project < project_count; project++)
                if (project_types[project] === building_type) projects.push(project_progress[project]);

            const build: SettlementPanelUI.SettlementBuildingState["build"] = !editable ? null : (context.buildable?.[building_type] ?? true) ? "enabled" : "disabled";
            return { definition, count, projects, build };
        }),
        editable,
    };
}

// Army panel: count gauges against the original counts, or while raising the target sliders against what the home settlement can give now.
function army_panel_state(army: EntityData, edit_raise_targets: boolean): ArmyPanelUI.ArmyPanelState
{
    const raising = army.get_number("raising") === 1;
    const home_guid = army.get_number("home_settlement");
    const home = home_guid === 0 ? null : query_entity(home_guid);

    const troops = TROOPS.map((troop, i): ArmyPanelUI.ArmyTroopState | null => {
        const current = army.get_number(`composition_${troop.name}`);
        const raised = army.get_number(`raised_${troop.name}`); // Original counts, or the raise targets while raising.
        if (!raising) return { current, max: Math.max(raised, current), target: raised, losses: Math.max(raised - current, 0) };

        // A type with nothing available, no troops in the army and no target is left out (nothing to set).
        const max = home === null ? current : raise_available(home, i);
        if (max === 0 && current === 0 && raised === 0) return null;
        return { current, max: Math.min(max, MAX_COUNT), target: raised, losses: 0 };
    });

    return {
        origin: settlement_text(home_guid, "Lost"),
        raising,
        troops,
        targets_editable: edit_raise_targets,
        supplies: army.get_number("supplies"),
        supplies_capacity: army.get_number("supplies_capacity"),
        supplies_consumption: army.get_number("supplies_consumption"),
        desertions: army.get_number("desertions"),
    };
}

function caravan_panel_state(caravan: EntityData): CaravanPanelUI.CaravanPanelState
{
    return {
        value: caravan.get_number("value"),
        origin: settlement_text(caravan.get_number("origin_settlement"), "Lost"),
        destination: settlement_text(caravan.get_number("destination_settlement"), "None"),
    };
}

function worker_panel_state(worker: EntityData): WorkerPanelUI.WorkerPanelState
{
    return {
        origin: settlement_text(worker.get_number("origin_entity"), "Lost"),
        location: worker.get_location("location"),
    };
}

// Workers are named after their work.
function worker_title(worker: EntityData): string
{
    switch (worker.get_number("work_type"))
    {
    case Backend.WORK_TYPE.FOUND_SETTLEMENT: return "Settlers";
    case Backend.WORK_TYPE.BUILD_ROAD: return "Road crew";
    case Backend.WORK_TYPE.BUILD_SETTLEMENT_BUILDING: return "Builders";
    default: return "Worker";
    }
}

// "Raise army" panel: what the settlement can give now per troop type against the targets, and the cost against the treasury.
// A type with nothing available and no target is left out (nothing to set).
function raise_army_panel_state(settlement: EntityData, targets: number[]): RaiseArmyPanelUI.RaiseArmyPanelState
{
    return {
        troops: TROOPS.map((_, i): RaiseArmyPanelUI.RaiseArmyTroopState | null => {
            const max = raise_available(settlement, i);
            if (max === 0 && targets[i] === 0) return null;
            return { max: Math.min(max, MAX_COUNT), target: targets[i] };
        }),
        cost: troops_consumption(targets),
        treasury: get_local_player_state().get_number("treasury"),
    };
}

// The type name of an entity without a panel of its own ("Entity" if unknown).
function type_title(view: EntityData): string
{
    const name = Backend.ENTITY_TYPE[view.entity_type] ?? "Entity";
    return name.charAt(0) + name.slice(1).toLowerCase();
}

// What the bottom center area shows: the frame's title and subtitle, and the state of at most one panel (the others are hidden).
interface Shown
{
    title: string;
    subtitle: string;
    settlement?: SettlementPanelUI.SettlementPanelState;
    army?: ArmyPanelUI.ArmyPanelState;
    caravan?: CaravanPanelUI.CaravanPanelState;
    worker?: WorkerPanelUI.WorkerPanelState;
    battle?: BattlePanelUI.BattlePanelState;
    raise_army?: RaiseArmyPanelUI.RaiseArmyPanelState;
    found_settlement?: FoundSettlementPanelUI.FoundSettlementPanelState;
}

// The one place submitting the frame and every panel. Null hides everything.
function show(shown: Shown | null): void
{
    EntityFrameUI.submit(shown === null ? null : { title: shown.title, subtitle: shown.subtitle });
    SettlementPanelUI.submit(shown?.settlement ?? null);
    ArmyPanelUI.submit(shown?.army ?? null);
    CaravanPanelUI.submit(shown?.caravan ?? null);
    WorkerPanelUI.submit(shown?.worker ?? null);
    BattlePanelUI.submit(shown?.battle ?? null);
    RaiseArmyPanelUI.submit(shown?.raise_army ?? null);
    FoundSettlementPanelUI.submit(shown?.found_settlement ?? null);
}

// Shows the entity in the panel of its type. A type without a panel shows the frame alone (type name, owner). Null hides everything.
export function show_entity(view: EntityData | null, context: InspectionContext = {}): void
{
    if (view === null)
    {
        show(null);
        return;
    }

    const subtitle = owner_name(view);
    switch (view.entity_type)
    {
    case Backend.ENTITY_TYPE.SETTLEMENT: show({ title: "Settlement", subtitle, settlement: settlement_panel_state(view, context) }); break;
    case Backend.ENTITY_TYPE.ARMY: show({ title: "Army", subtitle, army: army_panel_state(view, context.edit_raise_targets === true) }); break;
    case Backend.ENTITY_TYPE.CARAVAN: show({ title: (view.get_number("flags") & Backend.CARAVAN_FLAG_IS_SHIP) !== 0 ? "Ship" : "Caravan", subtitle, caravan: caravan_panel_state(view) }); break;
    case Backend.ENTITY_TYPE.WORKER: show({ title: worker_title(view), subtitle, worker: worker_panel_state(view) }); break;
    case Backend.ENTITY_TYPE.BATTLE: show({ title: "Battle", subtitle, battle: { location: view.get_location("location") } }); break;
    default: show({ title: type_title(view), subtitle }); break;
    }
}

// Shows the settlement's "Raise army" panel against the targets (troop order). The target sliders are interactive:
// the Raise army mode subscribes to RaiseArmyPanelUI "raise_targets". Hidden by show_entity(null) or any other show_*.
export function show_raise_army_panel(settlement: EntityData, targets: number[]): void
{
    show({ title: "Raise army", subtitle: owner_name(settlement), raise_army: raise_army_panel_state(settlement, targets) });
}

// Shows the settlement's "Found settlement" panel (state built by the Found settlement mode). Hidden by show_entity(null) or any other show_*.
export function show_found_settlement_panel(state: FoundSettlementPanelUI.FoundSettlementPanelState): void
{
    show({ title: "Found settlement", subtitle: "", found_settlement: state });
}
