// Initialization parameters for a Game Server.

#ifndef GAME_SERVER_INIT_PARAMS_INCLUDED
#define GAME_SERVER_INIT_PARAMS_INCLUDED

#include "core.h"

struct game_server_init_params
{
    bool dev_mode; // Whether server should be ready to handle special development commands / behavior like load / unload.

	ui8 match_slot_count;
	ui16 max_client_count;

	// Test mode parameters.
	bool run_test_scenario; // If set to true, the server will start, run a match scenario on its first tick, dump it to a specific file then shutdown.
	ia_string_view test_scenario_dump_filename; // If set to run test scenario, this indicates what file to dump the match data into once done.
};


#endif // GAME_SERVER_INIT_PARAMS_INCLUDED
