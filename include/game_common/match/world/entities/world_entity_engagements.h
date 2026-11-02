// Engagement entity symbols. Armies point at the engagement they fight in (world_entity_armies.h includes this file); nothing here knows about armies.

#ifndef WORLD_ENTITY_ENGAGEMENTS_INCLUDED
#define WORLD_ENTITY_ENGAGEMENTS_INCLUDED

#include "core.h"
#include "world_entity_common.h"
#include "../troop_types.h"

// Sides of a engagement. Armies point at their side with these (armies.engagement_sides). The attacker is the side that started it.
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
	FIELD = 0, // Armies against armies.
	SIEGE, // Armies against a settlement (main_target) and what defends it.
};

// Phase of the engagement. A field engagement is always ACTIVE. A siege goes BESIEGING (idle, no damage), then ASSAULT (started by the attacker commander)
// or SALLY (started by the defender commander); OUTSIDE is a clash of defending armies against the besiegers, the garrison does not take part.
// Damage rounds only happen in every phase but BESIEGING.
enum class ENGAGEMENT_PHASE : ui8
{
	ACTIVE = 0,
	BESIEGING,
	ASSAULT,
	SALLY,
	OUTSIDE,
};

// A engagement between two sides of armies. Free armies close enough join it by themselves (engagement_join_radius), or by an ATTACK order targeting it, a member army or the besieged settlement. Fixed size: it doesn't know which armies are in it, armies point to it instead.
struct world_entity_engagements : public world_entity_container
{
	struct side_state
	{
		army_troop_count troops; // Troops still fighting, per unit type.
		ui32 damage; // Damage points dealt this round BY this side, before the defences of the other side.
		match_player_id commander; // Player leading this side.
	};

	struct view;

	// Single state structure should work since everything gets read engagement-by-engagement anyway.
	struct state
	{
		world_location location;
		ENGAGEMENT_KIND kind;
		ENGAGEMENT_PHASE phase;
		entity_guid main_target; // Field engagement: the army that was attacked first. Siege: the besieged settlement, whose siege field points back here.
		ui32 phase_tick; // World age when the current phase started.
		side_state attacker;
		side_state defender;
		ui32 duration; // Rounds since the engagement started (one round every GAME_PARAMETERS.engagement_tick_interval ticks).
		ui8 join_radius; // Tiles. Not stored in the world: filled by world_entity_query_engagement (engagement_join_radius) for viewers.
		ui8 viewer_status; // PLAYER_DIPLOMATIC_STATUS of the engagement for the viewing player (engagement_diplomatic_status). Not stored either: filled by the query.

		inline side_state& side(ENGAGEMENT_SIDE side_index) { return side_index == ENGAGEMENT_SIDE::ATTACKER ? attacker : defender; }
		inline const side_state& side(ENGAGEMENT_SIDE side_index) const { return side_index == ENGAGEMENT_SIDE::ATTACKER ? attacker : defender; }

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

	GAME_DATA_PROPERTY_PATH(attacker_men_levies, attacker.troops.count[(ui8)TROOP_TYPE::LEVIES], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(attacker_men_men_at_arms, attacker.troops.count[(ui8)TROOP_TYPE::MEN_AT_ARMS], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(attacker_men_horsemen, attacker.troops.count[(ui8)TROOP_TYPE::HORSEMEN], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(attacker_men_knights, attacker.troops.count[(ui8)TROOP_TYPE::KNIGHTS], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(attacker_damage, attacker.damage, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(attacker_commander, attacker.commander, GAME_DATA_PROP_TYPE::PLAYER_ID)

	GAME_DATA_PROPERTY_PATH(defender_men_levies, defender.troops.count[(ui8)TROOP_TYPE::LEVIES], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(defender_men_men_at_arms, defender.troops.count[(ui8)TROOP_TYPE::MEN_AT_ARMS], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(defender_men_horsemen, defender.troops.count[(ui8)TROOP_TYPE::HORSEMEN], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(defender_men_knights, defender.troops.count[(ui8)TROOP_TYPE::KNIGHTS], GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(defender_damage, defender.damage, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(defender_commander, defender.commander, GAME_DATA_PROP_TYPE::PLAYER_ID)

	GAME_DATA_PROPERTY(duration, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(join_radius, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(viewer_status, GAME_DATA_PROP_TYPE::UI8)
GAME_DATA_OBJECT_DEF_END()

// viewer: the player looking at the engagement (fills viewer_status).
bool world_entity_query_engagement(const match_world_state& world, entity_guid entity, match_player_id viewer, world_entity_engagement& out_state);

#endif // WORLD_ENTITY_ENGAGEMENTS_INCLUDED
