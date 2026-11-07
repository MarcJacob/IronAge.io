// Army entity symbols. The engagements armies fight in are in world_entity_engagements.h.

#ifndef WORLD_ENTITY_ARMIES_INCLUDED
#define WORLD_ENTITY_ARMIES_INCLUDED

#include "core.h"
#include "world_entity_common.h"
#include "world_entity_engagements.h"
#include "../troop_types.h"

// Standing orders of an army: what it does by itself. An army in an engagement it cannot leave (or that its order wants) is pinned there instead, see the order logic.
// Zero = STATION, a functional default. The logic of each type is in world_system_army_standing_orders.cpp.
enum ARMY_ORDER_TYPE : ui8
{
	ARMY_ORDER_STATION = 0, // Go to the order's location and stand there.
	ARMY_ORDER_ATTACK, // Chase the order's target entity (settlement, army or engagement), then start / join an engagement (a siege for a settlement) once close enough. The order goes on after the engagement.
	                   // A target army inside an engagement stands for that engagement (the order's location is then the engagement's); a neutral one is not joined, the army waits next to it.
	ARMY_ORDER_DEFEND_TERRITORY, // Target = a settlement of the army's owner: stand on it (no patrolling for now), attack intruders in its territory, retreat to the settlement from stronger forces.
	ARMY_ORDER_RAID, // Target = a hostile or neutral settlement: go to its territory, stay there, flee stronger forces.
	ARMY_ORDER_GO_HOME, // Walk to the home settlement: a standing army disbands there, a retinue (is_retinue) stops there (STATION).
	ARMY_ORDER_JOIN_CAMPAIGN, // Target = a campaign: walk to it. Once within its join radius (world_system_campaigns) the campaign takes the army over: it becomes a member and its order becomes STATION where it stands.
    ARMY_ORDER_ACCOMPANY, // Target = army entity. Accompany the target, sticking close to it and helping it in any engagement it joins.

	ARMY_ORDER_TYPE_COUNT,
};

// Sub-state of an order, interpreted by its type (values overlap between types). Zero is the state an order starts in.
enum ARMY_ORDER_STATE : ui8
{
    // COMMON
	ARMY_ORDER_STATE_TRAVELLING = 0, // Common: going to the order's objective area before running the order-specific logic (never movement inside it).

    // DEFEND
	ARMY_ORDER_STATE_DEFEND_PATROL = 1, // Inside the territory, attacking intruders.
	ARMY_ORDER_STATE_DEFEND_RETREATED = 2, // Stands on the settlement tile, fights what comes close.

    // RAID
	ARMY_ORDER_STATE_RAID_RAIDING = 1, // Inside the target's territory, standing (raiding).
	ARMY_ORDER_STATE_RAID_FLEEING = 2, // Running to the home settlement from stronger forces.
};

// An army's standing order: its type and a small fixed data buffer interpreted by the type (not every field is used by every type).
struct army_order
{
	ui8 type; // ARMY_ORDER_TYPE.
	ARMY_ORDER_STATE state; // Sub-state of the type, zero when the order is given.

	ui16 radius; // Tiles, for orders working on an area. Unused for now.
	entity_guid target; // ATTACK: the entity to attack. DEFEND_TERRITORY / RAID: the settlement. JOIN_CAMPAIGN: the campaign.
	world_location location; // STATION: where to stand. DEFEND_TERRITORY / RAID: unused.
	entity_guid engaged; // STATION / DEFEND_TERRITORY: the hostile entity the army is currently chasing, if any.

	bool hold_position; // The army does not move to do anything other than specifically its objective (e.g. a STATION order does not chase hostiles coming within range).
	ui8 reserved[3];
};
static_assert(sizeof(army_order) == 20, "army_order size changed.");

// Property arrays for a world's armies.
struct world_entity_armies : public world_entity_container
{
	match_player_id* owner;
	world_location*	location;

	world_entity_movement* movement;

    army_troop_count *composition;

	entity_guid* engagement; // Engagement this army is fighting in, if any.
	ENGAGEMENT_SIDE* engagement_side; // Side of the engagement the army is on.

	entity_guid* campaign; // Campaign this army is a member of, if any. Set by the campaign when it takes over an army with a JOIN_CAMPAIGN order.

	army_troop_count* raised_troops; // Raise target when raising, "Original" / "Max" counts after that.
	ui8* raising; // 1 while the army is raising.
	ui8* is_retinue; // 1 for armies created by the player's CREATE_RETINUE command, 0 for standing armies (created by a settlement for a campaign). Only retinues take player orders.

	entity_guid* home_settlement; // Settlement this army was raised from.
	army_order* order; // Standing order (see ARMY_ORDER_TYPE).

	ui32* gold; // Gold the army carries (pay from the owner's treasury, raid loot). Buys supplies at the settlement it stands in. Goes to the owner's treasury when the army disbands.
	ui32* supplies; // Goods the army carries (1 per good). Consumed daily by its soldiers, bought with gold from the settlement it stands in. See world_system_armies_upkeep.
	ui16* desertions; // Men who deserted at the last daily tick (reset by every daily tick).

	struct view;

	// Consolidated representation of a single army for viewing purposes.
	struct single
	{
		match_player_id owner;
		world_location location;
		world_entity_movement movement;

		army_troop_count composition;
		army_troop_count raised_troops;
		ui8 raising;
		ui8 is_retinue;

		entity_guid home_settlement;
		army_order order;
		entity_guid engagement;
        ENGAGEMENT_SIDE engagement_side;
		entity_guid campaign;

		ui32 gold;
		ui32 supplies;
		ui32 supplies_consumption; // Computed by the query: what the army's soldiers consume per day. Not stored.
		ui32 supplies_capacity; // Computed by the query: what the army carries at most in its supplies and in its gold (army_supply_capacity). Not stored.
		ui16 desertions;

		void copy_from(const view& source);
		void copy_from(const world_entity_armies& collection, ui16 index);
	};

	// Reference view of an army slot. Query-derived supply values are not stored in the collection.
	struct view
	{
		match_player_id& owner;
		world_location& location;
		world_entity_movement& movement;
		army_troop_count& composition;
		army_troop_count& raised_troops;
		ui8& raising;
		ui8& is_retinue;
		entity_guid& home_settlement;
		army_order& order;
		entity_guid& engagement;
		ENGAGEMENT_SIDE& engagement_side;
		entity_guid& campaign;
		ui32& gold;
		ui32& supplies;
		ui16& desertions;

		void copy_from(const single& source)
		{
			owner = source.owner;
			location = source.location;
			movement = source.movement;
			composition = source.composition;
			raised_troops = source.raised_troops;
			raising = source.raising;
			is_retinue = source.is_retinue;
			home_settlement = source.home_settlement;
			order = source.order;
			engagement = source.engagement;
			engagement_side = source.engagement_side;
			campaign = source.campaign;
			gold = source.gold;
			supplies = source.supplies;
			desertions = source.desertions;
		}

		view(world_entity_armies& collection, ui16 index)
			: owner(collection.owner[index])
			, location(collection.location[index])
			, movement(collection.movement[index])
			, composition(collection.composition[index])
			, raised_troops(collection.raised_troops[index])
			, raising(collection.raising[index])
			, is_retinue(collection.is_retinue[index])
			, home_settlement(collection.home_settlement[index])
			, order(collection.order[index])
			, engagement(collection.engagement[index])
			, engagement_side(collection.engagement_side[index])
			, campaign(collection.campaign[index])
			, gold(collection.gold[index])
			, supplies(collection.supplies[index])
			, desertions(collection.desertions[index])
		{}
	};
};

inline void world_entity_armies::single::copy_from(const world_entity_armies::view& source)
{
	owner = source.owner;
	location = source.location;
	movement = source.movement;
	composition = source.composition;
	raised_troops = source.raised_troops;
	raising = source.raising;
	is_retinue = source.is_retinue;
	home_settlement = source.home_settlement;
	order = source.order;
	engagement = source.engagement;
	engagement_side = source.engagement_side;
	campaign = source.campaign;
	gold = source.gold;
	supplies = source.supplies;
	supplies_consumption = 0;
	supplies_capacity = 0;
	desertions = source.desertions;
}

inline void world_entity_armies::single::copy_from(const world_entity_armies& collection, ui16 index)
{
	owner = collection.owner[index];
	location = collection.location[index];
	movement = collection.movement[index];
	composition = collection.composition[index];
	raised_troops = collection.raised_troops[index];
	raising = collection.raising[index];
	is_retinue = collection.is_retinue[index];
	home_settlement = collection.home_settlement[index];
	order = collection.order[index];
	engagement = collection.engagement[index];
	engagement_side = collection.engagement_side[index];
	campaign = collection.campaign[index];
	gold = collection.gold[index];
	supplies = collection.supplies[index];
	supplies_consumption = 0;
	supplies_capacity = 0;
	desertions = collection.desertions[index];
}

using world_entity_army = world_entity_armies::single;
using world_entity_army_view = world_entity_armies::view;

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_army)
	GAME_DATA_PROPERTY(owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)

	GAME_DATA_PROPERTY_PATH(movement_travel_speed, movement.travel_speed,       GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(movement_fractional_loc, movement.fractional_loc,   GAME_DATA_PROP_TYPE::VEC2_I16)
	GAME_DATA_PROPERTY_PATH(movement_destination, movement.destination,         GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(movement_move_target, movement.move_target,         GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(movement_path_handle, movement.path_handle,         GAME_DATA_PROP_TYPE::UI32)

	GAME_DATA_PROPERTY_PATH(composition_levies, composition.levies,             GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(composition_men_at_arms, composition.men_at_arms,   GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(composition_horsemen, composition.horsemen,         GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(composition_knights, composition.knights,           GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(composition_wagons, composition.wagons,             GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_levies, raised_troops.levies,                GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_men_at_arms, raised_troops.men_at_arms,      GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_horsemen, raised_troops.horsemen,            GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_knights, raised_troops.knights,              GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_wagons, raised_troops.wagons,                GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(raising,                                                 GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(is_retinue,                                                GAME_DATA_PROP_TYPE::UI8)

	GAME_DATA_PROPERTY(home_settlement,                                         GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY_PATH(order_type, order.type,                             GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY_PATH(order_radius, order.radius,                         GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(order_target, order.target,                         GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY_PATH(order_location, order.location,                     GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(order_hold_position, order.hold_position,           GAME_DATA_PROP_TYPE::UI8)

	GAME_DATA_PROPERTY(engagement,                                                  GAME_DATA_PROP_TYPE::ENTITY_GUID)
    GAME_DATA_PROPERTY(engagement_side,                                             GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(campaign,                                                    GAME_DATA_PROP_TYPE::ENTITY_GUID)

	GAME_DATA_PROPERTY(gold,                                                    GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(supplies,                                                GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(supplies_consumption,                                    GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(supplies_capacity,                                       GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(desertions,                                              GAME_DATA_PROP_TYPE::UI16)
GAME_DATA_OBJECT_DEF_END()

bool world_entity_query_army(const match_world_state& world, entity_guid entity, world_entity_army& out_state);

// SUPPLIES MECHANICS HELPERS / QUERIES

// What the given soldiers consume per day: sum of count x per-type upkeep rate. Levies are per mille on the levy total, rounded down.
ui32 army_daily_consumption(const army_troop_count& troops);

// The troops supplies are sized for: the raise targets while the army is raising, its composition after that.
const army_troop_count& army_supply_basis(const match_world_state& world, ui16 army_index);

// What the given soldiers carry at most in each pool: sum of count x per-type carry capacity (GAME_PARAMETERS.carry_*).
ui32 army_troops_carry_capacity(const army_troop_count& troops);

// What raising an army with these targets costs in gold, and what it starts with in gold and goods (the latter as far as the market gives):
// one day of the targets' consumption, at most their carry capacity. The single cost used by every raise path (command, campaign army, AI).
ui32 army_raising_cost(const army_troop_count& targets);

// What the army carries at most in each pool (supplies and gold, separate): its men x per-type carry capacity (GAME_PARAMETERS.carry_*), on its supply basis.
ui32 army_supply_capacity(const match_world_state& world, ui16 army_index);

#endif // WORLD_ENTITY_ARMIES_INCLUDED
