// A engagement / siege (engagement) is selected: play goes on as usual, the panel shows it, and for a siege a bottom bar offers Begin assault.
// The button is greyed out unless the command is valid for the local player (attacker commander, besieging phase), the panel's note says why not.
// Leaves when the selection stops being this engagement. Escape clears the selection.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import { get_selected_entity_guid, clear_selected_entity, send_command, is_command_valid, get_command_feedback, COMMAND_ERROR } from "../input.js"
import { query_entity, type EntityData } from "../gdo.js"
import * as ActionBarUI from "../ui/ui_element_action_bar.js"
import { show_entity } from "../ui/inspection/inspect_entity.js"

export class EngagementSelected extends Core.Mode
{
    // engagement_guid: the selected engagement entity this mode is about.
    constructor(private engagement_guid: number) { super(); }

    private is_siege: boolean | null = null; // Whether the bar currently shows the siege actions (null: no bar submitted yet).
    private can_assault: boolean | null = null;
    private can_lift: boolean | null = null;
    private can_sally: boolean | null = null;
    private sally_item: ActionBarUI.ActionBarItem = { id: "sally_out", name: "Sally out", state: 'unselectable' };

    private assault_item: ActionBarUI.ActionBarItem = { id: "begin_assault", name: "Begin assault", state: 'unselectable' };
    private lift_item: ActionBarUI.ActionBarItem = { id: "lift_siege", name: "Lift siege", state: 'unselectable' };

    private unsubscribe: (() => void)[] = [];

    on_entry(): void
    {
        this.unsubscribe = [
            ActionBarUI.on("action", id => {
                if (id === this.assault_item.id) send_command("BEGIN_ASSAULT", { engagement: this.engagement_guid });
                else if (id === this.lift_item.id) send_command("LIFT_SIEGE", { engagement: this.engagement_guid });
                else if (id === this.sally_item.id) send_command("SALLY_OUT", { engagement: this.engagement_guid });
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

        const engagement = query_entity(this.engagement_guid);
        if (engagement === null || engagement.entity_type !== Backend.ENTITY_TYPE.ENGAGEMENT || get_selected_entity_guid() !== this.engagement_guid)
        {
            Core.pop_mode();
            return;
        }

        this.update_panel(engagement);
    }

    // Bar: Begin assault for a siege. Note under the panel: why it is refused, if it is.
    private update_panel(engagement: EntityData): void
    {
        const siege = engagement.get_number("kind") === Backend.ENGAGEMENT_KIND.SIEGE;
        let note = "";
        let canAssault = false;
        let canLift = false;
        let canSally = false;
        if (siege)
        {
            canLift = is_command_valid("LIFT_SIEGE", { engagement: this.engagement_guid });
            canSally = is_command_valid("SALLY_OUT", { engagement: this.engagement_guid });
            const feedback = get_command_feedback("BEGIN_ASSAULT", { engagement: this.engagement_guid });
            canAssault = feedback.valid;
            const flags = feedback.values.error_flags ?? 0;
            if ((flags & COMMAND_ERROR.NOT_COMMANDER) !== 0) note = canSally ? "Only the attacker commander can begin the assault or lift the siege." : "Only the attacker commander can begin the assault.";
            else if ((flags & COMMAND_ERROR.WRONG_PHASE) !== 0) note = canSally ? "" : "The siege is not idle: the assault cannot begin.";
        }

        if (siege !== this.is_siege || canAssault !== this.can_assault || canLift !== this.can_lift || canSally !== this.can_sally)
        {
            this.can_sally = canSally;
            this.sally_item.state = canSally ? 'selectable' : 'unselectable';
            this.is_siege = siege;
            this.can_assault = canAssault;
            this.can_lift = canLift;
            this.assault_item.state = canAssault ? 'selectable' : 'unselectable';
            this.lift_item.state = canLift ? 'selectable' : 'unselectable';
            ActionBarUI.submit(siege ? { items: [this.assault_item, this.lift_item, this.sally_item] } : null);
        }

        show_entity(engagement, { engagement_note: note });
    }

    on_escape(): boolean
    {
        clear_selected_entity();
        Core.pop_mode();
        return true;
    }

    // Clicks fall through to main play (selection).
}
