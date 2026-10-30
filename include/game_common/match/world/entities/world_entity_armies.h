// Army & Engagement entity symbols.

#ifndef WORLD_ENTITY_ARMIES_INCLUDED
#define WORLD_ENTITY_ARMIES_INCLUDED

#include "core.h"
#include "world_entity_common.h"
#include "game_common/match/game_parameters.h"

// Standing orders of an army: what it does by itself while it is not pinned in an engagement (engagement walking wins over the order).
// Zero = STATION, a functional default. The logic of each type is in world_system_standing_orders.cpp.
enum ARMY_ORDER_TYPE : ui8
{
	ARMY_ORDER_STATION = 0, // Go to the order's location and stand there.
	ARMY_ORDER_ATTACK, // Chase the order's target entity, then start / join an engagement (a siege for a settlement) once close enough.
	ARMY_ORDER_DEFEND_TERRITORY, // Target = a settlement of the army's owner: wander in its territory, attack intruders, retreat to the settlement from stronger forces.
	ARMY_ORDER_RAID, // Target = a hostile settlement: go to its territory, stay there, flee stronger forces.
	ARMY_ORDER_GO_HOME, // Walk to the home settlement and disband.
	ARMY_ORDER_TYPE_COUNT,
};

// An army's standing order: its type and a small fixed data buffer interpreted by the type (not every field is used by every type).
struct army_order
{
	ui8 type; // ARMY_ORDER_TYPE.
	ui8 state; // Sub-state of the type (see world_system_standing_orders.cpp), zero when the order is given.
	ui16 radius; // Tiles, for orders working on an area. Unused for now.
	entity_guid target; // ATTACK: the entity to attack. DEFEND_TERRITORY / RAID: the settlement.
	world_location location; // STATION: where to stand. DEFEND_TERRITORY / RAID: the point the army is walking to.
	entity_guid engaged; // STATION / DEFEND_TERRITORY: the hostile entity the army is currently chasing, if any.
	ui32 reserved;
};
static_assert(sizeof(army_order) == 20, "army_order size changed.");

// Property arrays for a world's armies.
struct world_entity_armies : public world_entity_container
{
	match_player_id* owners;
	world_location*	locations;

	world_entity_movement* movements;

    army_troop_count *compositions;

	entity_guid* engagements; // Engagement this army is fighting in, if any.
	ui8* engagement_sides; // Side (0 = Attackers or 1 = Defenders) of the engagement the army is on.

	army_troop_count* raised_troops; // Raise target when raising, "Original" / "Max" counts after that.
	ui8* raising; // 1 while the army is raising.

	entity_guid* home_settlements; // Settlement this army was raised from.
	army_order* orders; // Standing order (see ARMY_ORDER_TYPE).

	ui32* gold; // Gold the army carries (loot). Nothing fills it yet. Added to the owner's treasury when the army disbands.
	ui32* supplies; // Wealth the army carries. Consumed daily by its soldiers, refilled from the owner's treasury inside the owner's territory. See world_system_armies_upkeep.
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

		entity_guid home_settlement;
		army_order order;
		entity_guid engagement;
        ui8 engagement_side;

		ui32 gold;
		ui32 supplies;
		ui32 supplies_consumption; // Computed by the query: what the army's soldiers consume per day. Not stored.
		ui32 supplies_capacity; // Computed by the query: what the army refills its supplies up to (army_supply_capacity). Not stored.
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
		entity_guid& home_settlement;
		army_order& order;
		entity_guid& engagement;
		ui8& engagement_side;
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
			home_settlement = source.home_settlement;
			order = source.order;
			engagement = source.engagement;
			engagement_side = source.engagement_side;
			gold = source.gold;
			supplies = source.supplies;
			desertions = source.desertions;
		}

		view(world_entity_armies& collection, ui16 index)
			: owner(collection.owners[index])
			, location(collection.locations[index])
			, movement(collection.movements[index])
			, composition(collection.compositions[index])
			, raised_troops(collection.raised_troops[index])
			, raising(collection.raising[index])
			, home_settlement(collection.home_settlements[index])
			, order(collection.orders[index])
			, engagement(collection.engagements[index])
			, engagement_side(collection.engagement_sides[index])
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
	home_settlement = source.home_settlement;
	order = source.order;
	engagement = source.engagement;
	engagement_side = source.engagement_side;
	gold = source.gold;
	supplies = source.supplies;
	supplies_consumption = 0;
	supplies_capacity = 0;
	desertions = source.desertions;
}

inline void world_entity_armies::single::copy_from(const world_entity_armies& collection, ui16 index)
{
	owner = collection.owners[index];
	location = collection.locations[index];
	movement = collection.movements[index];
	composition = collection.compositions[index];
	raised_troops = collection.raised_troops[index];
	raising = collection.raising[index];
	home_settlement = collection.home_settlements[index];
	order = collection.orders[index];
	engagement = collection.engagements[index];
	engagement_side = collection.engagement_sides[index];
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
	GAME_DATA_PROPERTY_PATH(raised_levies, raised_troops.levies,                GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_men_at_arms, raised_troops.men_at_arms,      GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_horsemen, raised_troops.horsemen,            GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_knights, raised_troops.knights,              GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(raising,                                                 GAME_DATA_PROP_TYPE::UI8)

	GAME_DATA_PROPERTY(home_settlement,                                         GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY_PATH(order_type, order.type,                             GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY_PATH(order_radius, order.radius,                         GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(order_target, order.target,                         GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY_PATH(order_location, order.location,                     GAME_DATA_PROP_TYPE::WORLD_LOCATION)

	GAME_DATA_PROPERTY(engagement,                                                  GAME_DATA_PROP_TYPE::ENTITY_GUID)
    GAME_DATA_PROPERTY(engagement_side,                                             GAME_DATA_PROP_TYPE::UI8)

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

// Supplies the army refills up to: army_supply_capacity_days of the consumption of its supply basis.
ui32 army_supply_capacity(const match_world_state& world, ui16 army_index);

// A engagement between two sides of armies. A engagement is never targeted: armies join by targeting an army (or a besieged settlement). Fixed size: it doesn't know which armies are in it, armies point to it instead.
struct world_entity_engagements : public world_entity_container
{
	static constexpr ui8 UNIT_TYPES = 4; // Army unit types, in comp order: levies, men at arms, horsemen, knights.

	// Sides of a engagement. Armies point at their side with these (armies.engagement_sides). The attacker is the side that started it.
	static constexpr ui8 SIDE_ATTACKER = 0;
	static constexpr ui8 SIDE_DEFENDER = 1;
	static inline ui8 opposite_side(ui8 side) { return side == SIDE_ATTACKER ? SIDE_DEFENDER : SIDE_ATTACKER; }

	// What the engagement is.
	static constexpr ui8 KIND_FIELD = 0; // Armies against armies.
	static constexpr ui8 KIND_SIEGE = 1; // Armies against a settlement (main_target) and what defends it.

	// Phase of the engagement. A field engagement is always ACTIVE. A siege goes BESIEGING (idle, no damage), then ASSAULT (started by the attacker commander)
	// or SALLY (started by the defender commander); OUTSIDE is a clash of defending armies against the besiegers, the garrison does not take part.
	// Damage rounds only happen in every phase but BESIEGING.
	static constexpr ui8 PHASE_ACTIVE = 0;
	static constexpr ui8 PHASE_BESIEGING = 1;
	static constexpr ui8 PHASE_ASSAULT = 2;
	static constexpr ui8 PHASE_SALLY = 3;
	static constexpr ui8 PHASE_OUTSIDE = 4;

	struct side_state
	{
		ui32 men[UNIT_TYPES]; // Men still fighting, per unit type.
		ui32 damage; // Damage points dealt this round BY this side, before the defences of the other side.
		match_player_id commander; // Player leading this side.
	};

    struct view;

    // Single state structure should work since everything gets read engagement-by-engagement anyway.
	struct state
	{
		world_location location;
		ui8 kind; // KIND_*.
		ui8 phase; // PHASE_*.
		entity_guid main_target; // Field engagement: the army that was attacked first. Siege: the besieged settlement, whose siege field points back here.
		ui32 phase_tick; // World age when the current phase started.
		side_state attacker;
		side_state defender;
		ui32 duration; // Rounds since the engagement started (one round every ENGAGEMENT_TICK_INTERVAL ticks).

		inline side_state& side(ui8 side_index) { return side_index == SIDE_ATTACKER ? attacker : defender; }
		inline const side_state& side(ui8 side_index) const { return side_index == SIDE_ATTACKER ? attacker : defender; }
                       
		// Siege (kind SIEGE): the attacker winning takes the settlement (main_target) for the attacker commander.
		// The garrison fights as part of the defender (counted into its men every round, casualties come out of the garrison).

		void copy_from(const view& source);
		void copy_from(const world_entity_engagements& collection, ui16 index);

	} *states;

	using single = state; // Consolidated representation of a single engagement for viewing purposes.

	// Reference view of a engagement slot.
	struct view
	{
		state& data; // The whole slot: the state is plain data.

		void copy_from(const single& source) { data = source; }

		view(world_entity_engagements& collection, ui16 index)
			: data(collection.states[index])
		{}
	};
};

inline void world_entity_engagements::state::copy_from(const world_entity_engagements::view& source) { *this = source.data; }

inline void world_entity_engagements::state::copy_from(const world_entity_engagements& collection, ui16 index) { *this = collection.states[index]; }

using world_entity_engagement = world_entity_engagements::single;
using world_entity_engagement_view = world_entity_engagements::view;

// Men per side and unit type are flattened: <side>_men_<unit type>, unit types in the order of army comp. The client sums the "attacker_men_" and "defender_men_" properties.
GAME_DATA_OBJECT_DEF_BEGIN(world_entity_engagement)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY(kind, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(phase, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(main_target, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(phase_tick, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(attacker_men_levies, attacker.men[0], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(attacker_men_men_at_arms, attacker.men[1], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(attacker_men_horsemen, attacker.men[2], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(attacker_men_knights, attacker.men[3], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(attacker_damage, attacker.damage, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(attacker_commander, attacker.commander, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY_PATH(defender_men_levies, defender.men[0], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(defender_men_men_at_arms, defender.men[1], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(defender_men_horsemen, defender.men[2], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(defender_men_knights, defender.men[3], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(defender_damage, defender.damage, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(defender_commander, defender.commander, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY(duration, GAME_DATA_PROP_TYPE::UI32)
GAME_DATA_OBJECT_DEF_END()

bool world_entity_query_engagement(const match_world_state& world, entity_guid entity, world_entity_engagement& out_state);

#endif // WORLD_ENTITY_ARMIES_INCLUDED
