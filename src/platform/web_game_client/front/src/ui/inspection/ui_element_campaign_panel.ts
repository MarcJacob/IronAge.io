// UI module (new style): the campaign panel, inside the entity frame (ui_element_entity_frame.ts). Read-only: state, objective, total troops, join radius,
// and one row per member army (or army on its way to join): owner-colored army icon, men, joined or joining, and a bar of its composition colored by troop type.
// Static markup in index.html (ids below), extracted once by init. submit(state) shows it, submit(null) hides it. Emits nothing.

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_campaign_panel",
    state: "campaign_panel_state",
    objective: "campaign_panel_objective",
    troops: "campaign_panel_troops",
    radius: "campaign_panel_radius",
    members: "campaign_panel_members",
    member_template: "campaign_panel_member_template",
};

export interface CampaignMemberState
{
    color: string; // CSS color of the army's owner.
    men: number;
    joined: boolean; // false = on its way to join.
    composition: { count: number, color: string }[]; // One segment per troop type with men, in order.
}

export interface CampaignPanelState
{
    state: string;
    objective: string;
    troops: number;
    join_radius: number; // Tiles.
    members: CampaignMemberState[];
}

let el: {
    root: HTMLElement;
    state: HTMLElement;
    objective: HTMLElement;
    troops: HTMLElement;
    radius: HTMLElement;
    members: HTMLElement;
    member_template: HTMLElement;
} | null = null;

function init(): void
{
    const get = (id: string) => document.getElementById(id) as HTMLElement;
    el = {
        root: get(IDS.root),
        state: get(IDS.state),
        objective: get(IDS.objective),
        troops: get(IDS.troops),
        radius: get(IDS.radius),
        members: get(IDS.members),
        member_template: get(IDS.member_template),
    };
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

function submit_member(row: HTMLElement, member: CampaignMemberState): void
{
    const icon = row.querySelector(".campaign_member_icon") as HTMLElement;
    if (icon.style.backgroundColor !== member.color) icon.style.backgroundColor = member.color;
    set_text(row.querySelector(".campaign_member_men") as HTMLElement, `${member.men} men`);
    set_text(row.querySelector(".campaign_member_status") as HTMLElement, member.joined ? "Joined" : "Joining");

    // Bar segments: added / removed when their number changes, otherwise updated in place.
    const bar = row.querySelector(".campaign_member_bar") as HTMLElement;
    while (bar.children.length > member.composition.length) bar.lastElementChild!.remove();
    while (bar.children.length < member.composition.length) bar.appendChild(document.createElement("div"));
    const total = member.composition.reduce((sum, segment) => sum + segment.count, 0);
    member.composition.forEach((segment, index) => {
        const fill = bar.children[index] as HTMLElement;
        const width = `${total > 0 ? segment.count / total * 100 : 0}%`;
        if (fill.style.width !== width) fill.style.width = width;
        if (fill.style.backgroundColor !== segment.color) fill.style.backgroundColor = segment.color;
    });
}

export function submit(state: CampaignPanelState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    set_text(el.state, state.state);
    set_text(el.objective, state.objective);
    set_text(el.troops, `${state.troops}`);
    set_text(el.radius, `${state.join_radius} tiles`);

    // The template is the first child of the member list: rows follow it.
    const first_row = 1;
    while (el.members.children.length - first_row > state.members.length) el.members.lastElementChild!.remove();
    while (el.members.children.length - first_row < state.members.length)
    {
        const row = el.member_template.cloneNode(true) as HTMLElement;
        row.removeAttribute("id");
        row.hidden = false;
        el.members.appendChild(row);
    }
    state.members.forEach((member, index) => submit_member(el!.members.children[first_row + index] as HTMLElement, member));
}

init();
