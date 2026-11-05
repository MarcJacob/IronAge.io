// Starting a campaign from an own settlement (child of Settlement selected). Territories and armies are picked, then Enter confirms:
//   left click on an own army (not in a campaign): adds / removes it as a member.
//   left click on another own settlement: adds / removes it as a member (it raises an army for the campaign).
//   left click on a settlement of another player: sets it as the objective target (conquer it).
// The campaign's objective is only set here: the Campaign selected mode is read-only. (A limit of this UI, not of the SET_CAMPAIGN_OBJECTIVE command.)
// Confirming sends CAMPAIGN_JOIN for the settlement (founding the campaign), waits for the campaign to appear on the settlement, then sends the objective
// and one CAMPAIGN_JOIN per member. FUTURE: several targets (the sim's objective is prepared for it), abstract area objectives.
// Leaves with the exit key, Escape or right click.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, send_command, is_command_valid } from "../input.js"
import { query_entity } from "../gdo.js"
import { get_local_player_id, is_own_settlement } from "../main.js"
import { show_entity } from "../ui/inspection/inspect_entity.js"

const COLOR_MEMBER: Render.RGB = [70, 140, 240];
const COLOR_TARGET: Render.RGB = [230, 60, 60];
const COLOR_HOVERED: Render.RGB = [240, 220, 40];
const COLOR_OTHER: Render.RGB = [130, 130, 130];
const ALPHA_MEMBER = 0.5;
const ALPHA_TARGET = 0.6;
const ALPHA_HOVERED = 0.6;
const ALPHA_OTHER = 0.1;
const ARMY_DOT_COLOR = 'rgba(70, 140, 240, 0.9)';
const FOUNDING_TIMEOUT_S = 5; // The campaign should show on the settlement well before this.
const CONFIRM_KEY = "Enter";

function slot_of(guid: number): number { return (guid >>> 8) & 0xFFF; }
function type_of(guid: number): number { return guid & 0xFF; }

export class CampaignStart extends Core.Mode
{
    // settlement_guid: the own settlement the campaign starts from. exit_key: code of the key that entered the mode, pressing it again leaves it.
    constructor(private settlement_guid: number, private exit_key: string) { super(); }

    private target_guid = 0; // Settlement to conquer (0: none yet).
    private member_armies: number[] = [];
    private member_settlements: number[] = [];
    private hovered_slot = -1;
    private founding_time_s = -1; // >= 0 once confirmed: seconds waiting for the campaign to appear.

    private saved_territory_color = Render.RENDER_CONFIG.territory_color;
    private saved_territory_alpha = Render.RENDER_CONFIG.territory_alpha;

    on_entry(): void
    {
        this.saved_territory_color = Render.RENDER_CONFIG.territory_color;
        this.saved_territory_alpha = Render.RENDER_CONFIG.territory_alpha;

        const role = (slot: number) =>
        {
            if (slot === slot_of(this.target_guid) && this.target_guid !== 0) return 'target';
            if (slot === slot_of(this.settlement_guid) || this.member_settlements.some(guid => slot_of(guid) === slot)) return 'member';
            return slot === this.hovered_slot ? 'hovered' : 'other';
        };
        Render.RENDER_CONFIG.territory_color = (owner, local_player, slot) => {
            switch (role(slot))
            {
            case 'target': return COLOR_TARGET;
            case 'member': return COLOR_MEMBER;
            case 'hovered': return COLOR_HOVERED;
            default: return COLOR_OTHER;
            }
        };
        Render.RENDER_CONFIG.territory_alpha = (owner, local_player, slot) => {
            switch (role(slot))
            {
            case 'target': return ALPHA_TARGET;
            case 'member': return ALPHA_MEMBER;
            case 'hovered': return ALPHA_HOVERED;
            default: return ALPHA_OTHER;
            }
        };
    }

    on_exit(): void
    {
        Render.RENDER_CONFIG.territory_color = this.saved_territory_color;
        Render.RENDER_CONFIG.territory_alpha = this.saved_territory_alpha;
        Render.RENDER_CONFIG.overlay_dots = [];
    }

    on_frame_update(delta_time_s: number): void
    {
        const settlement = query_entity(this.settlement_guid);
        if (settlement === null || !is_own_settlement(settlement) || get_selected_entity_guid() !== this.settlement_guid)
        {
            Core.pop_mode();
            return;
        }

        if (this.founding_time_s >= 0)
        {
            this.founding_time_s += delta_time_s;
            const campaign = settlement.get_number("campaign");
            if (campaign !== 0) this.finish(campaign);
            else if (this.founding_time_s > FOUNDING_TIMEOUT_S) Core.pop_mode();
            return;
        }

        this.hovered_slot = this.slot_at(Render.get_hovered_tile());

        // Armies of the campaign, marked on the map. Ones that are gone (or got into a campaign) are dropped.
        this.member_armies = this.member_armies.filter(guid => { const army = query_entity(guid); return army !== null && army.get_number("campaign") === 0; });
        this.member_settlements = this.member_settlements.filter(guid => { const member = query_entity(guid); return member !== null && is_own_settlement(member); });
        Render.RENDER_CONFIG.overlay_dots = this.member_armies.map(guid => {
            const location = query_entity(guid)!.get_location("location");
            return { x: location.x, y: location.y, color: ARMY_DOT_COLOR };
        });

        show_entity(settlement, { subtitle_note: this.note() });
    }

    private note(): string
    {
        if (this.target_guid === 0) return "Click a foreign settlement to conquer.";
        return `${this.member_armies.length} armies, ${this.member_settlements.length} more settlements. Click to add or remove, ${CONFIRM_KEY} to start.`;
    }

    // The campaign exists: its objective, then every member.
    private finish(campaign: number): void
    {
        send_command("SET_CAMPAIGN_OBJECTIVE", { campaign, objective_type: Backend.CAMPAIGN_OBJECTIVE.CONQUER_SETTLEMENT, target: this.target_guid });
        for (const entity of [...this.member_settlements, ...this.member_armies]) send_command("CAMPAIGN_JOIN", { entity, campaign });
        Core.pop_mode();
    }

    private confirm(): void
    {
        if (this.target_guid === 0) return;
        if (!is_command_valid("CAMPAIGN_JOIN", { entity: this.settlement_guid, campaign: 0 })) return;

        send_command("CAMPAIGN_JOIN", { entity: this.settlement_guid, campaign: 0 });
        this.founding_time_s = 0;
    }

    private toggle(list: number[], guid: number): void
    {
        const at = list.indexOf(guid);
        if (at >= 0) list.splice(at, 1);
        else list.push(guid);
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

        return state.entity_states.find(entity => entity.entity_type === Backend.ENTITY_TYPE.SETTLEMENT && slot_of(entity.guid) === slot) ?? null;
    }

    on_key_down(event: KeyboardEvent): boolean
    {
        if (event.code === CONFIRM_KEY)
        {
            if (!event.repeat && this.founding_time_s < 0) this.confirm();
            return true;
        }
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

    on_click(event: MouseEvent): boolean
    {
        if (event.button !== 0 || this.founding_time_s >= 0) return true;

        const localPlayer = get_local_player_id();
        const tile = Render.page_to_tile(event.clientX, event.clientY);
        if (localPlayer === null || !Render.is_tile_in_world(tile)) return true;

        // An army under the cursor first.
        const picked = Render.pick_entity_at(event.clientX, event.clientY, null);
        if (picked !== null && type_of(picked) === Backend.ENTITY_TYPE.ARMY)
        {
            const army = query_entity(picked);
            if (army !== null && army.get_number("owner") === localPlayer && army.get_number("campaign") === 0) this.toggle(this.member_armies, picked);
            return true;
        }

        const settlement = this.settlement_of_slot(this.slot_at(tile));
        if (settlement === null || settlement.guid === this.settlement_guid) return true;

        if (settlement.owner === localPlayer)
        {
            const view = query_entity(settlement.guid);
            if (view !== null && view.get_number("campaign") === 0) this.toggle(this.member_settlements, settlement.guid);
        }
        else if (settlement.owner >= 0) this.target_guid = settlement.guid === this.target_guid ? 0 : settlement.guid;
        return true;
    }
}
