// Army & Battle entity symbols.

#ifndef WORLD_ENTITY_ARMIES_INCLUDED
#define WORLD_ENTITY_ARMIES_INCLUDED

#include "core.h"
#include "world_entity_common.h"

// Property arrays for a world's armies.
struct world_entity_armies : public world_entity_container
{
	match_player_id* owners;
	world_location*	locations;

	world_entity_movement* movements;

    army_troop_count *compositions;

	entity_guid* battles; // Battle this army is fighting in, if any.
	ui8* battle_sides; // Side (0 = Attackers or 1 = Defenders) of the battle the army is on.

	army_troop_count* raised_troops; // Raise target when raising, "Original" / "Max" counts after that.
	ui8* raising; // 1 while the army is raising.

	entity_guid* home_settlements; // Settlement this army was raised from.
	entity_guid* action_targets; // What entity this army is currently engaging or following, if any.

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
		entity_guid action_target;
		entity_guid battle;
        ui8 battle_side;

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
		entity_guid& action_target;
		entity_guid& battle;
		ui8& battle_side;
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
			action_target = source.action_target;
			battle = source.battle;
			battle_side = source.battle_side;
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
			, action_target(collection.action_targets[index])
			, battle(collection.battles[index])
			, battle_side(collection.battle_sides[index])
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
	action_target = source.action_target;
	battle = source.battle;
	battle_side = source.battle_side;
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
	action_target = collection.action_targets[index];
	battle = collection.battles[index];
	battle_side = collection.battle_sides[index];
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
	GAME_DATA_PROPERTY(action_target,                                           GAME_DATA_PROP_TYPE::ENTITY_GUID)

	GAME_DATA_PROPERTY(battle,                                                  GAME_DATA_PROP_TYPE::ENTITY_GUID)
    GAME_DATA_PROPERTY(battle_side,                                             GAME_DATA_PROP_TYPE::UI8)

	GAME_DATA_PROPERTY(supplies,                                                GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(supplies_consumption,                                    GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(supplies_capacity,                                       GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(desertions,                                              GAME_DATA_PROP_TYPE::UI16)
GAME_DATA_OBJECT_DEF_END()

bool world_entity_query_army(const match_world_state& world, entity_guid entity, world_entity_army& out_state);

// A battle between two sides of armies. A battle is never targeted: armies join by targeting an army (or a besieged settlement). Fixed size: it doesn't know which armies are in it, armies point to it instead.
struct world_entity_battles : public world_entity_container
{
	static constexpr ui8 UNIT_TYPES = 4; // Army unit types, in comp order: levies, men at arms, horsemen, knights.

    struct view;

    // Single state structure should work since everything gets read battle-by-battle anyway.
	struct state
	{
		world_location location;
		ui32 men[2][UNIT_TYPES]; // Men still fighting, per side and unit type.
		ui32 damage[2]; // Men killed this tick BY each side.
		ui32 duration; // Rounds since the battle started (one round every BATTLE_TICK_INTERVAL ticks).
                       
		entity_guid siege_settlement; // Settlement besieged by side 0, INVALID if this isn't a siege. Side 0 winning takes it.
		match_player_id siege_owner; // Owner of the army that started the siege: new owner of the settlement if side 0 wins.
		entity_guid siege_defenders; // Levy army raised by the besieged settlement. Disbanded back into it if side 1 wins.

		void copy_from(const view& source);
		void copy_from(const world_entity_battles& collection, ui16 index);

	} *states;

	using single = state; // Consolidated representation of a single battle for viewing purposes.

	// Reference view of a battle slot.
	struct view
	{
		world_location& location;
		ui32 (&men)[2][UNIT_TYPES];
		ui32 (&damage)[2];
		ui32& duration;
		entity_guid& siege_settlement;
		match_player_id& siege_owner;
		entity_guid& siege_defenders;

		void copy_from(const single& source)
		{
			location = source.location;
			for (ui8 side = 0; side < 2; side++)
			{
				for (ui8 unitType = 0; unitType < UNIT_TYPES; unitType++) men[side][unitType] = source.men[side][unitType];
				damage[side] = source.damage[side];
			}
			duration = source.duration;
			siege_settlement = source.siege_settlement;
			siege_owner = source.siege_owner;
			siege_defenders = source.siege_defenders;
		}

		view(world_entity_battles& collection, ui16 index)
			: location(collection.states[index].location)
			, men(collection.states[index].men)
			, damage(collection.states[index].damage)
			, duration(collection.states[index].duration)
			, siege_settlement(collection.states[index].siege_settlement)
			, siege_owner(collection.states[index].siege_owner)
			, siege_defenders(collection.states[index].siege_defenders)
		{}
	};
};

inline void world_entity_battles::state::copy_from(const world_entity_battles::view& source)
{
	location = source.location;
	for (ui8 side = 0; side < 2; side++)
	{
		for (ui8 unitType = 0; unitType < UNIT_TYPES; unitType++) men[side][unitType] = source.men[side][unitType];
		damage[side] = source.damage[side];
	}
	duration = source.duration;
	siege_settlement = source.siege_settlement;
	siege_owner = source.siege_owner;
	siege_defenders = source.siege_defenders;
}

inline void world_entity_battles::state::copy_from(const world_entity_battles& collection, ui16 index)
{
	location = collection.states[index].location;
	for (ui8 side = 0; side < 2; side++)
	{
		for (ui8 unitType = 0; unitType < UNIT_TYPES; unitType++) men[side][unitType] = collection.states[index].men[side][unitType];
		 damage[side] = collection.states[index].damage[side];
	}
	duration = collection.states[index].duration;
	siege_settlement = collection.states[index].siege_settlement;
	siege_owner = collection.states[index].siege_owner;
	siege_defenders = collection.states[index].siege_defenders;
}

using world_entity_battle = world_entity_battles::single;
using world_entity_battle_view = world_entity_battles::view;

// Men per side and unit type are flattened: men_side<side>_<unit type>, unit types in the order of army comp.
GAME_DATA_OBJECT_DEF_BEGIN(world_entity_battle)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(men_side0_levies, men[0][0], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side0_men_at_arms, men[0][1], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side0_horsemen, men[0][2], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side0_knights, men[0][3], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side1_levies, men[1][0], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side1_men_at_arms, men[1][1], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side1_horsemen, men[1][2], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side1_knights, men[1][3], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(damage_side0, damage[0], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(damage_side1, damage[1], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(duration, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(siege_settlement, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(siege_owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY(siege_defenders, GAME_DATA_PROP_TYPE::ENTITY_GUID)
GAME_DATA_OBJECT_DEF_END()

bool world_entity_query_battle(const match_world_state& world, entity_guid entity, world_entity_battle& out_state);

#endif // WORLD_ENTITY_ARMIES_INCLUDED
