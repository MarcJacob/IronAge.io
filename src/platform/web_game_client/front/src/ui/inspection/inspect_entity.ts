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
import * as EngagementPanelUI from "./ui_element_engagement_panel.js"
import * as CampaignPanelUI from "./ui_element_campaign_panel.js"
import { RENDER_CONFIG } from "../../render.js"
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

    // Army panel: a line of text under the rows (e.g. why the Raid order is refused).
    army_note?: string;

    // Engagement panel: a line of text under the rows (e.g. why the selected action is refused).
    engagement_note?: string;

    // Any entity: text added to the frame's subtitle line (e.g. what the active mode expects from the player).
    subtitle_note?: string;
}

const ENGAGEMENT_PHASE_NAMES: { [phase: number]: string } = {
    [Backend.ENGAGEMENT_PHASE.ACTIVE]: "Fighting",
    [Backend.ENGAGEMENT_PHASE.SIEGE_IDLE]: "Besieging",
    [Backend.ENGAGEMENT_PHASE.SIEGE_ASSAULT]: "Assault",
    [Backend.ENGAGEMENT_PHASE.SIEGE_SALLY]: "Sally",
    [Backend.ENGAGEMENT_PHASE.SIEGE_OUTSIDE_BATTLE]: "Outside clash",
};

// Engagement / siege panel: kind, phase, each side's commander with its total men, and the main target.
function engagement_panel_state(engagement: EntityData, note: string): EngagementPanelUI.EngagementPanelState
{
    const side = (name: string) =>
    {
        const perType = ["levies", "men_at_arms", "horsemen", "knights", "wagons"].map(type => engagement.get_number(`${name}_men_${type}`));
        const men = perType.reduce((sum, count) => sum + count, 0);
        return `${Backend.get_player_name(engagement.get_number(`${name}_commander`))}, ${men} men (levies ${perType[0]}, men at arms ${perType[1]}, horsemen ${perType[2]}, knights ${perType[3]}, wagons ${perType[4]})`;
    };

    let target = "";
    const targetGuid = engagement.get_number("main_target");
    const targetView = targetGuid !== 0 ? query_entity(targetGuid) : null;
    if (targetView !== null)
    {
        const where = targetView.get_location("location");
        target = `${type_title(targetView)} at (${where.x}, ${where.y})`;
    }

    return {
        kind: engagement.get_number("kind") === Backend.ENGAGEMENT_KIND.SIEGE ? "Siege" : "Field battle",
        phase: ENGAGEMENT_PHASE_NAMES[engagement.get_number("phase")] ?? "",
        attacker: side("attacker"),
        defender: side("defender"),
        target,
        location: engagement.get_location("location"),
        note,
    };
}

const CAMPAIGN_STATE_NAMES: { [state: number]: string } = {
    [Backend.CAMPAIGN_STATE.WAITING]: "Waiting for armies",
    [Backend.CAMPAIGN_STATE.GATHERING]: "Gathering",
    [Backend.CAMPAIGN_STATE.MARCHING]: "Marching",
    [Backend.CAMPAIGN_STATE.ON_OBJECTIVE]: "On objective",
};

// The campaign's objective as text: its type, and its target if it has one.
function campaign_objective_text(campaign: EntityData): string
{
    switch (campaign.get_number("objective_type"))
    {
    case Backend.CAMPAIGN_OBJECTIVE.CONQUER_SETTLEMENT: return `Conquer ${settlement_text(campaign.get_number("objective_target"), "a lost settlement").toLowerCase()}`;
    default: return "None";
    }
}

// Campaign panel (read-only): properties, and its member armies from the extra data (scanned by the backend): owner color, men, composition, joined or not.
function campaign_panel_state(campaign: EntityData): CampaignPanelUI.CampaignPanelState
{
    const armies = campaign.extra?.get_array("member_army") ?? [];
    const joined = campaign.extra?.get_array("member_joined") ?? [];
    const count = campaign.extra?.get_number("member_count") ?? 0;
    const local = Backend.LAST_RENDER_STATE?.controlled_player_id ?? 0;

    const members: CampaignPanelUI.CampaignMemberState[] = [];
    for (let i = 0; i < count && i < armies.length; i++)
    {
        const army = query_entity(armies[i]);
        if (army === null) continue;

        const counts = TROOPS.map(troop => army.get_number(`composition_${troop.name}`));
        const [r, g, b] = RENDER_CONFIG.entity_color(army.get_number("owner"), local);
        members.push({
            color: `rgb(${r}, ${g}, ${b})`,
            men: counts.reduce((sum, value) => sum + value, 0),
            joined: joined[i] === 1,
            composition: TROOPS.map((troop, type) => ({ count: counts[type], color: troop.color })).filter(segment => segment.count > 0),
        });
    }

    return {
        state: CAMPAIGN_STATE_NAMES[campaign.get_number("state")] ?? "",
        objective: campaign_objective_text(campaign),
        troops: campaign.get_number("troop_count"),
        join_radius: campaign.get_number("join_radius"),
        members,
    };
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

    // Garrison levies are working pop (already in the strata), so the home soldiers shown apart from them are the other types.
    // Raised troops of every type, levies included, are in "population" and shown apart from it.
    const soldiers_home = prop("troops_men_at_arms") + prop("troops_horsemen") + prop("troops_knights");
    const raised = prop("raised_levies") + prop("raised_men_at_arms") + prop("raised_horsemen") + prop("raised_knights") + prop("raised_wagons");
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
        allow_foreign_export: prop("trading_allow_foreign_export") !== 0,
        market_goods: { goods: prop("trading_market_goods"), export_goods: prop("trading_export_goods"), capacity: prop("market_capacity") },
        trade_volume: { volume: prop("trading_trade_volume"), max: prop("trade_volume_max") },
        // Troop order (levies first: max, garrison + raised, follows the population; no ceiling). A type with no max, nobody at home and nobody raised is left out (nothing to show or set yet).
        garrison: TROOPS.map((troop): SettlementPanelUI.SettlementGarrisonState | null => {
            const home = prop(`troops_${troop.name}`);
            const troop_raised = prop(`raised_${troop.name}`);
            const max = prop(`max_${troop.name}`);
            if (max === 0 && home === 0 && troop_raised === 0) return null;
            return { home, raised: troop_raised, max, ceiling: troop.name === "levies" || troop.name === "wagons" ? max : prop(`ceiling_${troop.name}`) };
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
        under_siege: prop("siege") !== 0,
        raided: prop("raided") !== 0,
        editable,
    };
}

// Army panel: count gauges against the original counts, or while raising the target sliders against what the home settlement can give now.
// The army's standing order as text: its name, and its target (attack) or location (station).
function army_order_text(army: EntityData): string
{
    switch (army.get_number("order_type"))
    {
    case Backend.ARMY_ORDER.STATION:
    {
        const location = army.get_location("order_location");
        return `Station (${location.x}, ${location.y})`;
    }
    case Backend.ARMY_ORDER.ATTACK:
    {
        const target = query_entity(army.get_number("order_target"));
        if (target === null) return "Attack";
        const location = target.get_location("location");
        return `Attack ${type_title(target).toLowerCase()} (${location.x}, ${location.y})`;
    }
    case Backend.ARMY_ORDER.DEFEND_TERRITORY: return `Defend territory of ${settlement_text(army.get_number("order_target"), "a lost settlement").toLowerCase()}`;
    case Backend.ARMY_ORDER.RAID: return `Raid ${settlement_text(army.get_number("order_target"), "a lost settlement").toLowerCase()}`;
    case Backend.ARMY_ORDER.GO_HOME: return "Go home";
    case Backend.ARMY_ORDER.JOIN_CAMPAIGN: return "Join campaign";
    case Backend.ARMY_ORDER.ACCOMPANY:
    {
        const target = query_entity(army.get_number("order_target"));
        if (target === null) return "Accompany";
        const location = target.get_location("location");
        return `Accompany ${type_title(target).toLowerCase()} (${location.x}, ${location.y})`;
    }
    default: return "Unknown";
    }
}

function army_panel_state(army: EntityData, edit_raise_targets: boolean, note: string): ArmyPanelUI.ArmyPanelState
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
        order: army_order_text(army),
        raising,
        troops,
        targets_editable: edit_raise_targets,
        supplies: army.get_number("supplies"),
        supplies_capacity: army.get_number("supplies_capacity"),
        supplies_consumption: army.get_number("supplies_consumption"),
        gold: army.get_number("gold"),
        note,
        desertions: army.get_number("desertions"),
    };
}

// A caravan destination is a settlement or an army, told apart by the guid's type.
function caravan_destination_text(guid: number): string
{
    const target = guid === 0 ? null : query_entity(guid);
    if (target !== null && target.entity_type === Backend.ENTITY_TYPE.ARMY)
    {
        const location = target.get_location("location");
        return `Army (${location.x}, ${location.y})`;
    }
    return settlement_text(guid, "None");
}

function caravan_panel_state(caravan: EntityData): CaravanPanelUI.CaravanPanelState
{
    return {
        value: caravan.get_number("value"),
        origin: settlement_text(caravan.get_number("origin_settlement"), "Lost"),
        destination: caravan_destination_text(caravan.get_number("destination_settlement")),
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
    engagement?: EngagementPanelUI.EngagementPanelState;
    campaign?: CampaignPanelUI.CampaignPanelState;
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
    EngagementPanelUI.submit(shown?.engagement ?? null);
    CampaignPanelUI.submit(shown?.campaign ?? null);
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

    const owner = owner_name(view);
    const subtitle = context.subtitle_note === undefined || context.subtitle_note === "" ? owner : `${owner} - ${context.subtitle_note}`;
    switch (view.entity_type)
    {
    case Backend.ENTITY_TYPE.SETTLEMENT: show({ title: "Settlement", subtitle, settlement: settlement_panel_state(view, context) }); break;
    case Backend.ENTITY_TYPE.ARMY: show({ title: "Army", subtitle, army: army_panel_state(view, context.edit_raise_targets === true, context.army_note ?? "") }); break;
    case Backend.ENTITY_TYPE.CARAVAN: show({ title: (view.get_number("flags") & Backend.CARAVAN_FLAG_IS_SHIP) !== 0 ? "Ship" : "Caravan", subtitle, caravan: caravan_panel_state(view) }); break;
    case Backend.ENTITY_TYPE.WORKER: show({ title: worker_title(view), subtitle, worker: worker_panel_state(view) }); break;
    case Backend.ENTITY_TYPE.ENGAGEMENT: show({ title: view.get_number("kind") === Backend.ENGAGEMENT_KIND.SIEGE ? "Siege" : "Battle", subtitle, engagement: engagement_panel_state(view, context.engagement_note ?? "") }); break;
    case Backend.ENTITY_TYPE.CAMPAIGN: show({ title: "Campaign", subtitle, campaign: campaign_panel_state(view) }); break;
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
