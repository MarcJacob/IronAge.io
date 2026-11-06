// Campaign entity symbols. Armies point at the campaign they belong to (world_entity_armies.h); nothing here knows about armies.

#ifndef WORLD_ENTITY_CAMPAIGNS_INCLUDED
#define WORLD_ENTITY_CAMPAIGNS_INCLUDED

#include "core.h"
#include "world_entity_common.h"

// Where a campaign stands. Zero = WAITING, the state of a new campaign. Once it has armies it ends when none is close to it, whatever the state.
enum CAMPAIGN_STATE : ui8
{
	CAMPAIGN_STATE_WAITING = 0, // Founded on a settlement: stays on it, waiting for its first army to join. Does not end while it has no army.
	CAMPAIGN_STATE_GATHERING, // Has armies, which stand where they are. Waits for every joining army to arrive, or for the begin command, then marches.
	CAMPAIGN_STATE_MARCHING, // Moves towards the objective's location, its armies spread around waypoints ahead of it.
	CAMPAIGN_STATE_ON_OBJECTIVE, // At the objective: the objective's own logic runs (e.g. assaulting a settlement) until it is accomplished.
};

// What a campaign is trying to achieve. Zero = none, a functional default: the campaign then does nothing but gather. The logic of each type is in world_system_campaigns.cpp.
// FUTURE: objectives should become more abstract (concern a whole area), and a campaign should have several, ending when all are accomplished.
enum CAMPAIGN_OBJECTIVE_TYPE : ui8
{
	CAMPAIGN_OBJECTIVE_NONE = 0,
	CAMPAIGN_OBJECTIVE_CONQUER_SETTLEMENT, // target = the settlement to conquer: accomplished once it is no longer hostile or neutral to the campaign's owner.
	CAMPAIGN_OBJECTIVE_TYPE_COUNT,
};

// A campaign's objective: its type and a small fixed data buffer interpreted by the type (not every field is used by every type), like army_order.
// FUTURE: several targets (up to ~32) instead of one.
struct campaign_objective
{
	CAMPAIGN_OBJECTIVE_TYPE type;
	ui8 state; // Sub-state of the type, interpreted by it. Zero when the objective is given.

	ui16 radius; // Tiles, for objectives working on an area.
	entity_guid target; // Entity the objective is about, if any.
	world_location location; // Place the objective is about, if any.

	ui32 reserved;
};
static_assert(sizeof(campaign_objective) == 16, "campaign_objective size changed.");

// Discriminated union of state required by the campaign over its states.
union campaign_state_data
{
    struct
    {
        ui64 _unused;
    } waiting;

    struct
    {
        entity_guid guide_army; // Army used as a "guide" for the others as the campaign members travel together to the objective.
                                // Usually chosen as some combination of slowest and largest army, to lower the chances it or others will be left behind.
    } marching;

    struct
    {

    } on_objective;
};

// Property arrays for a world's campaigns. A campaign is a group of armies moving and acting together. Fixed size: it doesn't know its armies, armies point to it instead.
// Its location follows its armies (world_system_campaigns). It ends when none of its armies is close enough to it.
struct world_entity_campaigns : public world_entity_container
{
	world_location* location;

	match_player_id* owner;

	CAMPAIGN_STATE* state;
    campaign_state_data* state_data;

	ui32* troop_count; // Men in all the armies of the campaign (recomputed every tick).
                       
	campaign_objective* objective; // Current objective of the campaign.

	struct view;

	// Consolidated data of a single campaign.
	struct single
	{
		world_location location;
		match_player_id owner;
		CAMPAIGN_STATE state;
        campaign_state_data state_data;
		ui32 troop_count;
		campaign_objective objective;

		ui8 join_radius; // Tiles. Not stored in the world: filled by world_entity_query_campaign (campaign_join_radius) for viewers.

		void copy_from(const view& source);
		void copy_from(const world_entity_campaigns& collection, ui16 index);
	};

	// Reference view of a campaign slot. Query-derived values are not stored in the collection.
	struct view
	{
		world_location& location;
		match_player_id& owner;
		CAMPAIGN_STATE& state;
        campaign_state_data& state_data;
		ui32& troop_count;
		campaign_objective& objective;

		void copy_from(const single& source)
		{
			location = source.location;
			owner = source.owner;
			state = source.state;
            state_data = source.state_data;
			troop_count = source.troop_count;
			objective = source.objective;
		}

		view(world_entity_campaigns& collection, ui16 index)
			: location(collection.location[index])
			, owner(collection.owner[index])
			, state(collection.state[index])
            , state_data(collection.state_data[index])
			, troop_count(collection.troop_count[index])
			, objective(collection.objective[index])
		{}
	};
};

inline void world_entity_campaigns::single::copy_from(const world_entity_campaigns::view& source)
{
	location = source.location;
	owner = source.owner;
	state = source.state;
	troop_count = source.troop_count;
	objective = source.objective;
	join_radius = 0;
}

inline void world_entity_campaigns::single::copy_from(const world_entity_campaigns& collection, ui16 index)
{
	location = collection.location[index];
	owner = collection.owner[index];
	state = collection.state[index];
	troop_count = collection.troop_count[index];
	objective = collection.objective[index];
	join_radius = 0;
}

using world_entity_campaign = world_entity_campaigns::single;
using world_entity_campaign_view = world_entity_campaigns::view;

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_campaign)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY(owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY(state, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(troop_count, GAME_DATA_PROP_TYPE::UI32)

	GAME_DATA_PROPERTY_PATH(objective_type, objective.type, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY_PATH(objective_state, objective.state, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY_PATH(objective_radius, objective.radius, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(objective_target, objective.target, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY_PATH(objective_location, objective.location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)

	GAME_DATA_PROPERTY(join_radius, GAME_DATA_PROP_TYPE::UI8)
GAME_DATA_OBJECT_DEF_END()

bool world_entity_query_campaign(const match_world_state& world, entity_guid entity, world_entity_campaign& out_state);

#endif // WORLD_ENTITY_CAMPAIGNS_INCLUDED
