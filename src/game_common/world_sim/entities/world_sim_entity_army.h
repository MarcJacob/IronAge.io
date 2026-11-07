// Army symbols / helpers shared between world simulation themes.

#ifndef WORLD_SIM_ARMY_INCLUDED
#define WORLD_SIM_ARMY_INCLUDED

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/entities/world_entity_armies.h"
#include "game_common/match/world/troop_types.h"
#include "game_common/match/world/world_state.h"

// Gives the army a STATION order at the location (the order system then walks it there). Does not touch its movement.
// hold_position: see army_order::hold_position (here: the army does not chase hostile armies / caravans coming within range).
static inline void army_order_station(world_entity_armies& armies, ui16 index, world_location location, bool hold_position = false)
{
	armies.order[index] = {};
	armies.order[index].type = ARMY_ORDER_STATION;
	armies.order[index].location = location;
	armies.order[index].hold_position = hold_position;
}

// Gives the army an ATTACK order on the target entity (the order system chases it and resolves the attack on reaching it).
static inline void army_order_attack(world_entity_armies& armies, ui16 index, entity_guid target)
{
	armies.order[index] = {};
	armies.order[index].type = ARMY_ORDER_ATTACK;
	armies.order[index].target = target;
}

// Gives the army a DEFEND_TERRITORY order for the settlement (its own). The location is the first point it walks to.
static inline void army_order_defend_territory(world_entity_armies& armies, ui16 index, entity_guid settlement, world_location location)
{
	armies.order[index] = {};
	armies.order[index].type = ARMY_ORDER_DEFEND_TERRITORY;
	armies.order[index].target = settlement;
	armies.order[index].location = location;
}

// Whether the player can give the army orders (move, attack, order, raise-target, stop-raising, withdraw, join-campaign commands): only retinues can, and not while they are members of a campaign.
// Standing armies (created by a settlement for its campaign) are controlled by the campaign logic only.
static inline bool army_takes_player_orders(const world_entity_armies& armies, ui16 index)
{
	return armies.is_retinue[index] && !armies.campaign[index].is_valid();
}

// Gives the army an ACCOMPANY order for the target entity.
static inline void army_order_accompany(world_entity_armies& armies, ui16 index, entity_guid accompanied)
{
    armies.order[index] = {};
    armies.order[index].type = ARMY_ORDER_ACCOMPANY;
    armies.order[index].target = accompanied;
}

// Spawns a new army in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_army(match_world_state& world, const world_entity_armies::single& start_state);
void world_destroy_army(match_world_state& world, entity_guid army);

// A settlement that joined a campaign raises an army for it: campaign_army_garrison_percent of each type of its garrison, paid like the raise command (gold and goods),
// raised at the settlement, with a JOIN_CAMPAIGN order it follows once raised. It becomes the settlement's campaign_army (kept after the campaign ends), a standing army (not a retinue).
// If the settlement still has its campaign_army alive, that army is sent to the campaign instead (JOIN_CAMPAIGN order, nothing raised or paid).
// Does nothing if the settlement is besieged or the kept army's owner is not the settlement's; otherwise, when raising, if the garrison gives no men, the treasury cannot pay, or there is no army slot left.
// Returns whether the settlement now has an army on its way to the campaign.
bool world_settlement_join_campaign(match_world_state& world, ui16 settlement_index, entity_guid campaign);

// The troops such an army would be raised with.
army_troop_count world_settlement_campaign_army_targets(const world_entity_settlements& settlements, ui16 settlement_index);

// Allocates the armies' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_armies(match_world_state& world, mem_arena& memory, ui32 count);

// An army cannot carry more gold or supplies than its capacity (army_supply_capacity): call after anything that can lower the capacity (men removed, raise targets lowered,
// raising stopped). The excess goes into the loot pool of the army's engagement if it is in one, otherwise it vanishes. Skipped for a non-raising army without men:
// it is about to be destroyed, and its gold goes home or into the loot pool then.
void army_enforce_carry_capacity(match_world_state& world, ui16 army_index);

// ARMY PRIMITIVES
// Army composition changes. Take the army's index. They only touch the army: the home settlement's counts and the player stats are the caller's business.

// Adds men of a type to the army's composition (clamped to the ui16 count). Returns how many were added.
static inline ui16 army_add_troops(match_world_state& world, ui16 army_index, TROOP_TYPE troop_type, ui16 count)
{
	ui16& troops = world.entities->armies.composition[army_index].access_troop_count(troop_type);
	ui16 added = ia_min(count, (ui16)(0xFFFF - troops));
	troops += added;
	return added;
}

// Removes up to count men of a type from the army's composition (and drops the gold and supplies it can no longer carry). Returns how many were removed.
static inline ui16 army_remove_troops(match_world_state& world, ui16 army_index, TROOP_TYPE troop_type, ui16 count)
{
	ui16& troops = world.entities->armies.composition[army_index].access_troop_count(troop_type);
	ui16 removed = ia_min(count, troops);
	troops -= removed;
	if (removed > 0) army_enforce_carry_capacity(world, army_index);
	return removed;
}

// Destroys an army and returns its men to the settlement: levies become unemployed pop, the other types rejoin the local troops (raised count down, home count up).
void world_disband_army(match_world_state& world, entity_guid army, entity_guid settlement);

// Homecoming: the army's gold goes to its owner's treasury, its supplies to the home settlement's goods (if still the owner's, lost otherwise). Call before destroying an army
// that is not disbanded (world_disband_army and the last deserter do it themselves). The army keeps its gold and supplies: destroy it right after.
void army_homecoming(match_world_state& world, ui16 army_index);

// Removes up to count deserters of a type from the army. They go back to the army's home settlement (levies as unemployed pop, the others in the local pool), even if it changed owner,
// or vanish if the home is invalid. Returns how many left the army.
// @TODO(Marc): The further from home the army is, the less likely deserters should be to make it back.
ui16 world_army_desertion(match_world_state& world, entity_guid army, TROOP_TYPE troop_type, ui16 count);

// Kills up to kills[type] men of each unit type of an army, returns the dead per unit type, and removes them from the home settlement's raised counts and total population (skipped if the home is invalid).
void world_army_take_casualties(match_world_state& world, entity_guid army, const army_troop_count& kills, army_troop_count& out_dead);

// Raiding. The army loots the settlement it raids: production (gold and supplies), peasants killed (gold), goods (supplies). Each is the min of the daily potential left on the
// settlement and the room left in the army's gold / supplies pool (capacity: army_supply_capacity). What is taken leaves the settlement; nothing else is destroyed.
void world_army_loot_settlement(match_world_state& world, entity_guid army, entity_guid settlement);

// Has the army destroy the target caravan and take its value as gold and supplies.
// Whatever the army can't carry is left for the local settlement.
void world_army_loot_caravan(match_world_state& world, entity_guid army, entity_guid caravan);

// Runs daily, before the settlement economy. Resets the settlements' daily raided totals (raided = looted yesterday), then every army raiding (RAID_RAIDING,
// not fleeing / travelling / raising / in an engagement) loots its target. Armies go in index order from a random start so none has a standing advantage on a shared target.
// Declared in systems_war.h.

// Gives the army a STATION order at the nearest friendly settlement (at its own location if there is none).
void army_order_station_nearest_friendly_settlement(match_world_state& world, ui16 army_index);

// CARAVAN TRADE TARGET QUERIES (armies as caravan destinations)

// Index of the settlement whose territory the army stands in, world_terrain::UNOWNED_TILE if none (defined in world_system_army_economy.cpp).
ui16 army_tile_owner(const match_world_state& world, ui16 army_index);

// Room left in the army's supplies: army_supply_capacity - supplies, floor 0.
ui32 army_supplies_room(const match_world_state& world, ui16 army_index);

// Goods the army's gold can buy: gold / good_price_gold (a 0 price counts as 1).
ui32 army_supplies_buy_power(const match_world_state& world, ui16 army_index);

// Status of the army relative to the origin owner (HOSTILE for a dead army). The owner's own armies are whatever the diplomatic stub says (friendly).
PLAYER_DIPLOMATIC_STATUS army_trade_status(const match_world_state& world, ui16 army_index, match_player_id origin_owner);

// True for any live army that is not hostile to the origin owner, whatever it is doing (engagement, fleeing, GO_HOME). Friendly vs neutral: army_trade_status.
bool army_is_trade_target(const match_world_state& world, ui16 army_index, match_player_id origin_owner);

#endif // WORLD_SIM_ARMY_INCLUDED
