// Initialization parameters for a Game Server.

#ifndef GAME_SERVER_INIT_PARAMS_INCLUDED
#define GAME_SERVER_INIT_PARAMS_INCLUDED

#include "core.h"

struct game_server_init_params
{
	ui8 match_slot_count;
	ui16 max_client_count;

	ia_string_view web_root; // Folder holding the web client bundle to serve over HTTP, relative to the platform resources folder. 
									// Must be in the format "./<path>/" (leading "./" and trailing "/"), matching the paths returned by the platform's list_resource_files.

	// Test mode parameters.
	bool run_test_scenario; // If set to true, the server will start, run a match scenario on its first tick, dump it to a specific file then shutdown.
	ia_string_view test_scenario_dump_filename; // If set to run test scenario, this indicates what file to dump the match data into once done.
};


#endif // GAME_SERVER_INIT_PARAMS_INCLUDED
