// Territory targeting for an own army's standing orders (Defend, Attack, Raid): one mode, one preset per order.
// Territories are tinted: green = valid target for the preset, gray = not, yellow = the hovered valid one.
// A left click on a valid territory (any tile) sends SET_ARMY_ORDER with its settlement as target, and the mode stays.
// Leaves with the preset's hotkey again, Escape or right click. Another order's hotkey is handled by the parent (Army selected), which switches the preset.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, send_command, is_command_valid } from "../input.js"
import { query_entity } from "../gdo.js"
import { get_local_player_id, is_own_army } from "../main.js"
import { show_entity } from "../ui/inspection/inspect_entity.js"

export interface TargetingPreset
{
    order_type: Backend.ARMY_ORDER;
    friendly: boolean; // true: valid targets are own territories (Defend). false: non-owned territories (Attack, Raid).
    note: string;
}

const COLOR_VALID: Render.RGB = [60, 200, 70];
const COLOR_INVALID: Render.RGB = [130, 130, 130];
const COLOR_HOVERED: Render.RGB = [240, 220, 40];
const ALPHA_VALID = 0.5;
const ALPHA_INVALID = 0.1;
const ALPHA_HOVERED = 0.75; // Hovered valid territory only.

export class TerritoryTargeting extends Core.Mode
{
    // exit_key: code of the key that entered the mode, pressing it again leaves the mode.
    constructor(private army_guid: number, private preset: TargetingPreset, private exit_key: string) { super(); }

    private hovered_slot = -1; // Settlement slot owning the hovered tile (-1: none).
    private chosen_slot = -1; // Settlement slot targeted by the army's current order (-1: none). Shown like a hovered valid one.
    private saved_territory_color = Render.RENDER_CONFIG.territory_color;
    private saved_territory_alpha = Render.RENDER_CONFIG.territory_alpha;

    on_entry(): void
    {
        this.saved_territory_color = Render.RENDER_CONFIG.territory_color;
        this.saved_territory_alpha = Render.RENDER_CONFIG.territory_alpha;
        Render.RENDER_CONFIG.territory_alpha = (owner, local_player, slot) => {
            if (!this.is_valid_owner(owner, local_player)) return ALPHA_INVALID;
            return slot === this.hovered_slot || slot === this.chosen_slot ? ALPHA_HOVERED : ALPHA_VALID;
        };
        Render.RENDER_CONFIG.territory_color = (owner, local_player, slot) => {
            if (!this.is_valid_owner(owner, local_player)) return COLOR_INVALID;
            return slot === this.hovered_slot || slot === this.chosen_slot ? COLOR_HOVERED : COLOR_VALID;
        };
    }

    on_exit(): void
    {
        Render.RENDER_CONFIG.territory_color = this.saved_territory_color;
        Render.RENDER_CONFIG.territory_alpha = this.saved_territory_alpha;
    }

    on_frame_update(delta_time_s: number): void
    {
        const army = query_entity(this.army_guid);
        if (army === null || !is_own_army(army) || get_selected_entity_guid() !== this.army_guid)
        {
            Core.pop_mode();
            return;
        }

        this.hovered_slot = this.slot_at(Render.get_hovered_tile());
        this.chosen_slot = this.chosen_slot_of(army.get_number("order_type"), army.get_number("order_target"));
        show_entity(army, { army_note: this.preset.note });
    }

    // Slot of the settlement targeted by the army's current order (any preset), or -1. An attack target that is not a visible settlement gives -1.
    private chosen_slot_of(order_type: number, target_guid: number): number
    {
        if (order_type !== Backend.ARMY_ORDER.ATTACK && order_type !== Backend.ARMY_ORDER.DEFEND_TERRITORY && order_type !== Backend.ARMY_ORDER.RAID) return -1;

        const slot = (target_guid >>> 8) & 0xFFF;
        return this.settlement_of_slot(slot)?.guid === target_guid ? slot : -1;
    }

    // Local mirror of the sim's friendly / hostile check (world_entity_is_friendly / _is_hostile): same owner or not. Unknown owner (< 0) is never valid.
    // The sim's validity feedback still decides at click time.
    private is_valid_owner(owner: number, local_player: number): boolean
    {
        if (owner < 0) return false;
        return (owner === local_player) === this.preset.friendly;
    }

    // Settlement slot owning the tile, or -1.
    private slot_at(tile: { x: number, y: number } | null): number
    {
        const state = Backend.LAST_RENDER_STATE;
        const owners = Backend.get_owner_settlement_tiles_view();
        const flags = Backend.get_tile_flags_view();
        if (tile === null || state === null || owners === null || flags === null) return -1;

        const index = tile.y * state.world_size.width + tile.x;
        return (flags[index] & Render.TILE_FLAG_OWNED) !== 0 ? owners[index] : -1;
    }

    // Visible settlement entity of the slot (settlements touching the viewport are always in the render state).
    private settlement_of_slot(slot: number): Backend.RenderEntity | null
    {
        const state = Backend.LAST_RENDER_STATE;
        if (state === null || slot < 0) return null;

        return state.entity_states.find(entity => entity.entity_type === Backend.ENTITY_TYPE.SETTLEMENT && ((entity.guid >>> 8) & 0xFFF) === slot) ?? null;
    }

    // Only the exit key is handled here: other keys (e.g. another order's hotkey) fall through to the modes below.
    on_key_down(event: KeyboardEvent): boolean
    {
        if (event.code !== this.exit_key) return false;

        if (!event.repeat) Core.pop_mode();
        return true;
    }

    // Right click leaves, like Escape.
    on_context_menu(event: MouseEvent): boolean
    {
        Core.pop_mode();
        return true;
    }

    // Left click on a valid territory sends the order. Anything else does nothing (the mode stays).
    on_click(event: MouseEvent): boolean
    {
        if (event.button !== 0) return true;

        const localPlayer = get_local_player_id();
        const tile = Render.page_to_tile(event.clientX, event.clientY);
        if (localPlayer === null || !Render.is_tile_in_world(tile)) return true;

        const settlement = this.settlement_of_slot(this.slot_at(tile));
        if (settlement === null || !this.is_valid_owner(settlement.owner, localPlayer)) return true;

        const order = { army: this.army_guid, order_type: this.preset.order_type, target: settlement.guid };
        if (is_command_valid("SET_ARMY_ORDER", order)) send_command("SET_ARMY_ORDER", order);
        return true;
    }
}
