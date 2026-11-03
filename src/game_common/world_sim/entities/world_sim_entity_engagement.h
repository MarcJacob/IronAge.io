// Engagement symbols / helpers shared between world simulation themes.

#ifndef WORLD_SIM_ENGAGEMENT_INCLUDED
#define WORLD_SIM_ENGAGEMENT_INCLUDED

#include "game_common/match/world/world_state.h"

// Spawns a new engagement in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_engagement(match_world_state& world, const world_entity_engagements::state& start_state);
void world_destroy_engagement(match_world_state& world, entity_guid engagement);

// Ends a engagement: releases every army in it (clears their engagement link), then destroys it (a siege also clears its settlement's siege).
void world_engagement_end(match_world_state& world, entity_guid engagement);

// Recomputes the commanders of a engagement: per side, the owner with the most men commands, the incumbent keeps command on a tie.
// A siege's defender commander (its settlement's owner) is left alone. Call when an army joins or leaves, and each round.
void world_engagement_update_commanders(match_world_state& world, entity_guid engagement);

// To call after an army left a engagement (withdrew, was destroyed): ends a siege left without attackers (see below), otherwise updates the commanders.
void world_engagement_army_left(match_world_state& world, entity_guid engagement);

// Ends the engagement if it is a siege with no army left on the attacker side. Call after an army left a engagement or was destroyed. Does nothing for other engagements or invalid guids.
void world_siege_check_attackers(match_world_state& world, entity_guid engagement);

// Diplomatic status of an engagement for a player (the only place it is computed: world_entity_get_diplomatic_status and the side-picking world_army_join_engagement use it).
// Each side is rated by the player's status towards its commander (friendly > neutral > hostile). The player would join the better side, and the status says which:
// FRIENDLY = the attackers are the better side (join the attackers), HOSTILE = the defenders are (join the defenders), NEUTRAL = equal sides, not joinable.
PLAYER_DIPLOMATIC_STATUS engagement_diplomatic_status(const match_world_state& world, entity_guid engagement, match_player_id player);

// Tiles around an engagement's location within which a free army joins it (see engagement_join_* in the game parameters): base + 1 per power of 2 of the troops
// involved above the threshold. Troops: both sides' men; for a siege, minus the garrison counted in the defender side (the men inside the settlement).
ui32 engagement_join_radius(const match_world_state& world, entity_guid engagement);

// Allocates the engagements' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_engagements(match_world_state& world, mem_arena& memory, ui32 count);

// Makes an army fight in an engagement on the given side, whatever its owner's status: its men are added to the side, it keeps its standing order (not worked on while pinned),
// stops moving (the order system then walks it to the engagement) and the commanders are updated.
void world_army_join_engagement(match_world_state& world, entity_guid army, entity_guid engagement, ENGAGEMENT_SIDE side);

// Same, with the side picked for the army's owner (see engagement_diplomatic_status): friendly engagement = attackers, hostile = defenders. Returns whether the army joined.
// Does not join (false) if the engagement is neutral to the owner, or if the army stands on the besieged settlement and would take the defender side of its siege.
// Defenders joining an idle siege (BESIEGING) from outside turn it into an OUTSIDE clash.
// Callers make sure the army is valid, free (not in an engagement) and has men; the engagement must be valid. A raising army may join: its raising is suspended meanwhile.
bool world_army_join_engagement(match_world_state& world, entity_guid army, entity_guid engagement);

// Damage points the given men deal before the round's random percent: sum over the types of count x damage. 0 = they cannot hurt anyone (e.g. only wagons).
ui64 troops_base_damage(const army_troop_count& troops);

// The side the army would join the engagement on (see above). Returns false if it would not join (neutral, standing on the besieged settlement as a defender,
// or the army cannot inflict damage: troops_base_damage of its men is 0). This also keeps harmless armies from auto-joining.
bool world_army_pick_engagement_side(const match_world_state& world, entity_guid army, entity_guid engagement, ENGAGEMENT_SIDE& out_side);

// Whether an army at the location, on the side, can leave the engagement: when it is farther than engagement_leave_radius_multiplier join radii from it, or when it is
// on the attacker side of an idle siege (BESIEGING: nothing fights there, it just walks out). Otherwise the engagement pins it until concluded.
// Also decides if a free army auto-joins: one that could leave right away is not aggroed, its order logic joins it if it wants to.
bool world_engagement_can_be_left(const match_world_state& world, entity_guid engagement, world_location location, ENGAGEMENT_SIDE side);

// Can the army leave its current engagement right now? False if the army is not in an engagement. See world_engagement_can_be_left (world_army_leave_current_engagement).
bool world_army_can_leave_current_engagement(const match_world_state& world, entity_guid army);

// The army leaves its current engagement at once (its men leave the side's men). May end a siege left without attackers. Does nothing if the army is not in an engagement.
void world_army_leave_current_engagement(match_world_state& world, entity_guid army);

// Starts a siege of a settlement by an army (the army and the engagement must be valid and the army free, see world_army_join_engagement): spawns an idle (BESIEGING) siege
// halfway between them, with the garrison as the defender side, and joins the army to the attacker side. Returns the siege.
// The caller makes sure the settlement has a garrison to besiege and is not already besieged.
entity_guid world_engagement_start_siege(match_world_state& world, entity_guid attacker, entity_guid settlement);

// Starts a field battle between two free armies: spawns an engagement halfway between them (it never moves afterwards), with the attacker on the attacker side
// and the target on the defender side. Returns the engagement.
entity_guid world_engagement_start_field_battle(match_world_state& world, entity_guid attacker, entity_guid target);

// Siege commands. The engagement must be a siege (asserted, along with the phase each one is allowed in).
// The attackers begin the assault of an idle siege (BESIEGING -> ASSAULT).
void world_engagement_siege_begin_assault(match_world_state& world, entity_guid engagement);
// The garrison sallies out of an idle (BESIEGING) or OUTSIDE siege (-> SALLY).
void world_engagement_siege_sally_out(match_world_state& world, entity_guid engagement);
// The attackers lift an idle (BESIEGING) siege: every army in it gets a STATION order where it stands, then the siege ends.
void world_engagement_siege_lift(match_world_state& world, entity_guid engagement);

// Removes up to count troops of a type from a side of an engagement (the men still fighting for it). Returns how many were removed. Does not touch any army.
ui16 world_engagement_remove_troops(match_world_state& world, entity_guid engagement, ENGAGEMENT_SIDE side, TROOP_TYPE troop_type, ui16 count);

#endif // WORLD_SIM_ENGAGEMENT_INCLUDED
