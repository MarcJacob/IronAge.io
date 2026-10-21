// Inspection section: construction. One building row per settlement building type: how many are built, the progress of each ongoing project,
// and (when the caller allows building) a button to start a project. Reads the settlement's building_counts and its extra data (ongoing projects,
// see web_game_client_query.cpp), and the type definitions (name, cost...) from the building table.

import { get_building_definition, type EntityData } from "../../gdo.js"
import type { PanelRow } from "./ui_element_properties_panel.js"

// on_build: starts a project of the given building type. Without it the rows have no button.
// can_build: whether a project of the type would be accepted right now (the button is disabled otherwise). Without it the button is always enabled.
export function construction_section(settlement: EntityData, on_build?: (building_type: number) => void, can_build?: (building_type: number) => boolean): PanelRow[]
{
    const counts = settlement.get_array("building_counts");

    // Ongoing projects by building type, as their progress (0..255).
    const project_types = settlement.extra?.get_array("construction_project_building_type") ?? [];
    const project_progress = settlement.extra?.get_array("construction_project_progress") ?? [];
    const project_count = settlement.extra?.get_number("construction_project_count") ?? 0;

    const rows: PanelRow[] = [];
    counts.forEach((count, building_type) => {
        const definition = get_building_definition(building_type);
        if (definition === null) return;

        const projects: number[] = [];
        for (let project = 0; project < project_count; project++)
            if (project_types[project] === building_type) projects.push(project_progress[project]);

        rows.push({ kind: "building", id: `building_${building_type}`, state: {
            name: definition.name,
            count,
            projects,
            description: [definition.name, definition.description, `Built: ${count}`],
            build_tooltip: [
                `Build ${definition.name}`,
                `Cost: ${definition.cost_per_day * definition.build_days} (${definition.cost_per_day} per day)`,
                `Workers: ${definition.workers}`,
                `Duration: ${definition.build_days} days`,
            ],
            build: on_build === undefined ? undefined : (can_build === undefined || can_build(building_type) ? "enabled" : "disabled"),
            on_build: on_build === undefined ? undefined : () => on_build(building_type),
        } });
    });
    return rows;
}
