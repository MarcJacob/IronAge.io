// Campaign objective entity symbols. A campaign objective is something that exists in the world (on the shared "board") and that campaigns can pursue.
// Not to be confused with the campaign_goal struct of world_entity_campaigns.h (what a campaign was assigned, to become its goals).
// Not visible to players: no GDO, no render, no query. Campaigns point at their current objective, objectives know nothing of campaigns except the strength they report (committed).

#ifndef WORLD_ENTITY_CAMPAIGN_OBJECTIVES_INCLUDED
#define WORLD_ENTITY_CAMPAIGN_OBJECTIVES_INCLUDED

#include "core.h"
#include "world_entity_common.h"

// Zero = NONE, a functional default. The logic of each type (scoring, range, execution) is in the campaign objective systems.
enum class CAMPAIGN_OBJECTIVE_TYPE : ui8
{
	NONE = 0,
	CONQUER_SETTLEMENT, // target = the settlement to conquer.
	TRAVEL_TO_SETTLEMENT, // target = a settlement a campaign can move to, as a step towards other objectives. Any number of campaigns may run it.
	COUNT,
};

// Type-specific data, interpreted by the objective's type. Zero when the objective is spawned.
union campaign_objective_data
{
	ui64 _unused;

	// CONQUER_SETTLEMENT: the campaigns pursuing it, by side relative to the target settlement (defenders = campaigns friendly to it, attackers = all others).
	// Recomputed every tick from the campaigns themselves (objective_conquer_refresh), so it cannot drift. A friendly target makes the objective a defense objective.
	struct
	{
		ui32 attacker_campaigns;
		ui32 defender_campaigns;
		ui32 attacker_troops;
		ui32 defender_troops;
	} conquer;
};

// Property arrays for a world's campaign objectives. One objective per type + target (see world_find_campaign_objective).
struct world_entity_campaign_objectives : public world_entity_container
{
	world_location* location; // Where campaigns have to go for it. The objective's own systems update it as needed.
	CAMPAIGN_OBJECTIVE_TYPE* type;
	entity_guid* target; // Entity the objective is about, if any. With type, the objective's identity.
	campaign_objective_data* data;

	ui32* committed; // Number of campaigns currently pursuing it. The campaigns keep it correct: +1 when one picks it, -1 when it leaves it (switches, ends...).
	ui16* idle_intervals; // Used by the type's despawn logic: how many checks in a row nobody was interested in it.

	struct view;

	// Consolidated data of a single campaign objective.
	struct single
	{
		world_location location;
		CAMPAIGN_OBJECTIVE_TYPE type;
		entity_guid target;
		campaign_objective_data data;
		ui32 committed;
		ui16 idle_intervals;

		void copy_from(const view& source);
	};

	// Reference view of a campaign objective slot.
	struct view
	{
		world_location& location;
		CAMPAIGN_OBJECTIVE_TYPE& type;
		entity_guid& target;
		campaign_objective_data& data;
		ui32& committed;
		ui16& idle_intervals;

		void copy_from(const single& source)
		{
			location = source.location;
			type = source.type;
			target = source.target;
			data = source.data;
			committed = source.committed;
			idle_intervals = source.idle_intervals;
		}

		view(world_entity_campaign_objectives& collection, ui16 index)
			: location(collection.location[index])
			, type(collection.type[index])
			, target(collection.target[index])
			, data(collection.data[index])
			, committed(collection.committed[index])
			, idle_intervals(collection.idle_intervals[index])
		{}
	};
};

inline void world_entity_campaign_objectives::single::copy_from(const world_entity_campaign_objectives::view& source)
{
	location = source.location;
	type = source.type;
	target = source.target;
	data = source.data;
	committed = source.committed;
	idle_intervals = source.idle_intervals;
}

using world_entity_campaign_objective = world_entity_campaign_objectives::single;
using world_entity_campaign_objective_view = world_entity_campaign_objectives::view;

#endif // WORLD_ENTITY_CAMPAIGN_OBJECTIVES_INCLUDED
