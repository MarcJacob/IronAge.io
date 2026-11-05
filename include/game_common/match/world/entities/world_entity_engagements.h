// Engagement entity symbols. Armies point at the engagement they fight in (world_entity_armies.h includes this file); nothing here knows about armies.

#ifndef WORLD_ENTITY_ENGAGEMENTS_INCLUDED
#define WORLD_ENTITY_ENGAGEMENTS_INCLUDED

#include "core.h"
#include "world_entity_common.h"
#include "../troop_types.h"

// Sides of a engagement. Armies point at their side with these (armies.engagement_side). The attacker is the side that started it.
enum class ENGAGEMENT_SIDE : ui8
{
	ATTACKER = 0,
	DEFENDER,

	ENGAGEMENT_SIDE_COUNT,
};

static inline ENGAGEMENT_SIDE engagement_opposite_side(ENGAGEMENT_SIDE side) { return side == ENGAGEMENT_SIDE::ATTACKER ? ENGAGEMENT_SIDE::DEFENDER : ENGAGEMENT_SIDE::ATTACKER; }

// What the engagement is.
enum class ENGAGEMENT_KIND : ui8
{
	FIELD_BATTLE = 0, // Armies against armies in the field.
	SIEGE, // Armies against a settlement (main_target) and what defends it.
};

// Phase of the engagement. A field engagement is always ACTIVE. A siege goes SIEGE_IDLE, then SIEGE_ASSAULT (started by the attacker commander)
// or SIEGE_SALLY (started by the defender commander); SIEGE_OUTSIDE_BATTLE is a clash of defending armies against the besiegers, the garrison does not take part.
// Whether troops deal and take damage is the engagement's fighting_active flag (set on phase changes), not the phase itself.
enum class ENGAGEMENT_PHASE : ui8
{
	ACTIVE = 0,

	SIEGE_IDLE, // Siege is ongoing, no active fighting is going on.
	SIEGE_ASSAULT, // Siege is ongoing, the attackers are assaulting the defenders in the settlement.
	SIEGE_SALLY, // Siege is ongoing, the defenders have left the settlement to engage the attackers.
	SIEGE_OUTSIDE_BATTLE, // Siege camp is under attack outside, the defenders in the settlement are not involved.
};

// A engagement between two sides of armies. Free armies close enough join it by themselves (engagement_join_radius), or by an ATTACK order targeting it, a member army or the besieged settlement. Fixed size: it doesn't know which armies are in it, armies point to it instead.
struct world_entity_engagements : public world_entity_container
{
	world_location* location;
	ENGAGEMENT_KIND* kind;
	ENGAGEMENT_PHASE* phase;
	entity_guid* main_target; // Field engagement: the army that was attacked first. Siege: the besieged settlement, whose siege field points back here.
	ui32* phase_tick; // World age when the current phase started.
	bool* fighting_active; // True while troops deal and take damage: damage rounds run only then, and other mechanics (army supply...) are suspended.
	ui32* duration; // Rounds since the engagement started (one round every GAME_PARAMETERS.engagement_tick_interval ticks).
	ui32* loot_gold; // Loot pool: gold dropped by destroyed armies and conquest. Shared among the victors when the engagement ends (world_engagement_on_finish).
	ui32* loot_supplies; // Loot pool: supplies dropped by destroyed armies, plus the daily upkeep of every soldier killed.

	// Per side. Siege (kind SIEGE): the attacker winning takes the settlement (main_target) for the attacker commander.
	// The garrison fights as part of the defender (counted into its men every round, casualties come out of the garrison).
	army_troop_count* attacker_troops; // Troops still fighting, per unit type.
	ui32* attacker_damage; // Damage points dealt this round BY the side, before the defences of the other side.
	match_player_id* attacker_commander; // Player leading the side.
	army_troop_count* defender_troops;
	ui32* defender_damage;
	match_player_id* defender_commander;

	inline army_troop_count& side_troops(ENGAGEMENT_SIDE side, ui16 index) { return side == ENGAGEMENT_SIDE::ATTACKER ? attacker_troops[index] : defender_troops[index]; }
	inline const army_troop_count& side_troops(ENGAGEMENT_SIDE side, ui16 index) const { return side == ENGAGEMENT_SIDE::ATTACKER ? attacker_troops[index] : defender_troops[index]; }
	inline ui32& side_damage(ENGAGEMENT_SIDE side, ui16 index) { return side == ENGAGEMENT_SIDE::ATTACKER ? attacker_damage[index] : defender_damage[index]; }
	inline match_player_id& side_commander(ENGAGEMENT_SIDE side, ui16 index) { return side == ENGAGEMENT_SIDE::ATTACKER ? attacker_commander[index] : defender_commander[index]; }
	inline const match_player_id& side_commander(ENGAGEMENT_SIDE side, ui16 index) const { return side == ENGAGEMENT_SIDE::ATTACKER ? attacker_commander[index] : defender_commander[index]; }

	struct view;

	// Consolidated representation of a single engagement for viewing purposes.
	struct single
	{
		world_location location;
		ENGAGEMENT_KIND kind;
		ENGAGEMENT_PHASE phase;
		entity_guid main_target;
		ui32 phase_tick;
		bool fighting_active;
		ui32 loot_gold;
		ui32 loot_supplies;

		army_troop_count attacker_troops;
		ui32 attacker_damage;
		match_player_id attacker_commander;
		army_troop_count defender_troops;
		ui32 defender_damage;
		match_player_id defender_commander;

		ui32 duration;
		ui8 join_radius; // Tiles. Not stored in the world: filled by world_entity_query_engagement (engagement_join_radius) for viewers.
		ui8 viewer_status; // PLAYER_DIPLOMATIC_STATUS of the engagement for the viewing player (engagement_diplomatic_status). Not stored either: filled by the query.

		void copy_from(const view& source);
		void copy_from(const world_entity_engagements& collection, ui16 index);
	};

	// Reference view of a engagement slot. Query-derived values are not stored in the collection.
	struct view
	{
		world_location& location;
		ENGAGEMENT_KIND& kind;
		ENGAGEMENT_PHASE& phase;
		entity_guid& main_target;
		ui32& phase_tick;
		bool& fighting_active;
		ui32& loot_gold;
		ui32& loot_supplies;

		army_troop_count& attacker_troops;
		ui32& attacker_damage;
		match_player_id& attacker_commander;
		army_troop_count& defender_troops;
		ui32& defender_damage;
		match_player_id& defender_commander;

		ui32& duration;

		void copy_from(const single& source)
		{
			location = source.location;
			kind = source.kind;
			phase = source.phase;
			main_target = source.main_target;
			phase_tick = source.phase_tick;
			fighting_active = source.fighting_active;
			loot_gold = source.loot_gold;
			loot_supplies = source.loot_supplies;
			attacker_troops = source.attacker_troops;
			attacker_damage = source.attacker_damage;
			attacker_commander = source.attacker_commander;
			defender_troops = source.defender_troops;
			defender_damage = source.defender_damage;
			defender_commander = source.defender_commander;
			duration = source.duration;
		}

		view(world_entity_engagements& collection, ui16 index)
			: location(collection.location[index])
			, kind(collection.kind[index])
			, phase(collection.phase[index])
			, main_target(collection.main_target[index])
			, phase_tick(collection.phase_tick[index])
			, fighting_active(collection.fighting_active[index])
			, loot_gold(collection.loot_gold[index])
			, loot_supplies(collection.loot_supplies[index])
			, attacker_troops(collection.attacker_troops[index])
			, attacker_damage(collection.attacker_damage[index])
			, attacker_commander(collection.attacker_commander[index])
			, defender_troops(collection.defender_troops[index])
			, defender_damage(collection.defender_damage[index])
			, defender_commander(collection.defender_commander[index])
			, duration(collection.duration[index])
		{}
	};
};

inline void world_entity_engagements::single::copy_from(const world_entity_engagements::view& source)
{
	location = source.location;
	kind = source.kind;
	phase = source.phase;
	main_target = source.main_target;
	phase_tick = source.phase_tick;
	fighting_active = source.fighting_active;
	loot_gold = source.loot_gold;
	loot_supplies = source.loot_supplies;
	attacker_troops = source.attacker_troops;
	attacker_damage = source.attacker_damage;
	attacker_commander = source.attacker_commander;
	defender_troops = source.defender_troops;
	defender_damage = source.defender_damage;
	defender_commander = source.defender_commander;
	duration = source.duration;
	join_radius = 0;
	viewer_status = 0;
}

inline void world_entity_engagements::single::copy_from(const world_entity_engagements& collection, ui16 index)
{
	location = collection.location[index];
	kind = collection.kind[index];
	phase = collection.phase[index];
	main_target = collection.main_target[index];
	phase_tick = collection.phase_tick[index];
	fighting_active = collection.fighting_active[index];
	loot_gold = collection.loot_gold[index];
	loot_supplies = collection.loot_supplies[index];
	attacker_troops = collection.attacker_troops[index];
	attacker_damage = collection.attacker_damage[index];
	attacker_commander = collection.attacker_commander[index];
	defender_troops = collection.defender_troops[index];
	defender_damage = collection.defender_damage[index];
	defender_commander = collection.defender_commander[index];
	duration = collection.duration[index];
	join_radius = 0;
	viewer_status = 0;
}

using world_entity_engagement = world_entity_engagements::single;
using world_entity_engagement_view = world_entity_engagements::view;

// Men per side and unit type are flattened: <side>_men_<unit type>, unit types in the order of army comp. The client sums the "attacker_men_" and "defender_men_" properties.
GAME_DATA_OBJECT_DEF_BEGIN(world_entity_engagement)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY(kind, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(phase, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(main_target, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(phase_tick, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(fighting_active, GAME_DATA_PROP_TYPE::UI8)

	GAME_DATA_PROPERTY_PATH(attacker_men_levies, attacker_troops.count[(ui8)TROOP_TYPE::LEVIES], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(attacker_men_men_at_arms, attacker_troops.count[(ui8)TROOP_TYPE::MEN_AT_ARMS], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(attacker_men_horsemen, attacker_troops.count[(ui8)TROOP_TYPE::HORSEMEN], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(attacker_men_knights, attacker_troops.count[(ui8)TROOP_TYPE::KNIGHTS], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(attacker_men_wagons, attacker_troops.count[(ui8)TROOP_TYPE::WAGONS], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(attacker_damage, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(attacker_commander, GAME_DATA_PROP_TYPE::PLAYER_ID)

	GAME_DATA_PROPERTY_PATH(defender_men_levies, defender_troops.count[(ui8)TROOP_TYPE::LEVIES], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(defender_men_men_at_arms, defender_troops.count[(ui8)TROOP_TYPE::MEN_AT_ARMS], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(defender_men_horsemen, defender_troops.count[(ui8)TROOP_TYPE::HORSEMEN], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(defender_men_knights, defender_troops.count[(ui8)TROOP_TYPE::KNIGHTS], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(defender_men_wagons, defender_troops.count[(ui8)TROOP_TYPE::WAGONS], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(defender_damage, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(defender_commander, GAME_DATA_PROP_TYPE::PLAYER_ID)

	GAME_DATA_PROPERTY(duration, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(join_radius, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(viewer_status, GAME_DATA_PROP_TYPE::UI8)
GAME_DATA_OBJECT_DEF_END()

// viewer: the player looking at the engagement (fills viewer_status).
bool world_entity_query_engagement(const match_world_state& world, entity_guid entity, match_player_id viewer, world_entity_engagement& out_state);

#endif // WORLD_ENTITY_ENGAGEMENTS_INCLUDED
