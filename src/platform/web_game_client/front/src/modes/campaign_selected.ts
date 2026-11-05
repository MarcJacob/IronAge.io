// A campaign is selected: play goes on as usual and the panel shows it (properties, objective, member armies). The panel is read-only (the objective is set when the
// campaign starts); an own campaign gets a bar with Disband, which ends it early and sends its armies home.
// Leaves when the selection stops being this campaign. Escape clears the selection.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import { get_selected_entity_guid, clear_selected_entity, send_command } from "../input.js"
import { query_entity } from "../gdo.js"
import { get_local_player_id } from "../main.js"
import * as ActionBarUI from "../ui/ui_element_action_bar.js"
import { show_entity } from "../ui/inspection/inspect_entity.js"

export class CampaignSelected extends Core.Mode
{
    // campaign_guid: the selected campaign entity this mode is about.
    constructor(private campaign_guid: number) { super(); }

    private disband_item: ActionBarUI.ActionBarItem = { id: "disband_campaign", name: "Disband", state: 'selectable' };
    private bar_shown = false;
    private unsubscribe: (() => void)[] = [];

    on_entry(): void
    {
        this.unsubscribe = [
            ActionBarUI.on("action", id => {
                if (id === this.disband_item.id) send_command("CAMPAIGN_DISBAND", { campaign: this.campaign_guid });
            }),
        ];
    }

    on_exit(): void
    {
        for (const unsubscribe of this.unsubscribe) unsubscribe();
        this.unsubscribe = [];
        ActionBarUI.submit(null);
        show_entity(null);
    }

    on_frame_update(delta_time_s: number): void
    {
        if (!this.is_top) return;

        const campaign = query_entity(this.campaign_guid);
        if (campaign === null || campaign.entity_type !== Backend.ENTITY_TYPE.CAMPAIGN || get_selected_entity_guid() !== this.campaign_guid)
        {
            Core.pop_mode();
            return;
        }

        const own = campaign.get_number("owner") === get_local_player_id();
        if (own !== this.bar_shown)
        {
            this.bar_shown = own;
            ActionBarUI.submit(own ? { items: [this.disband_item] } : null);
        }

        show_entity(campaign, {});
    }

    on_escape(): boolean
    {
        clear_selected_entity();
        Core.pop_mode();
        return true;
    }

    // Clicks fall through to main play (selection).
}
