// Main implementation file for the Game Server Linux Platform Layer.

#include "core.h"
#include "game_server/game_server_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

_ASSERTION_HANDLER* _ASSERTION_HANDLER_PTR = NULL; 

struct LINUX_APP_STATE
{
	game_server_program server_program;
};

LINUX_APP_STATE APP_STATE;

void ASSERT_EXIT_FUNC()
{
	printf("ASSERTION RAISED.");
	__builtin_trap();
}

void ASSERT_MSG_FUNC(const char* assertMsg, const char* filename, ui32 line, ...)
{
	// @TODO(Marc): Add use of extra params.
	printf("ASSERTION RAISED: %s\n\tFile: %s, Line: %d", assertMsg, filename, line);
	__builtin_trap();
}

int main(int argc, char** argv)
{
	printf("Starting Linux Platform for Game Server...\n");

	// Init assertions handler.
	_ASSERTION_HANDLER ASSERTION_HANDLER = {
		.ASSERT_EXIT_FUNC = ASSERT_EXIT_FUNC,
		.ASSERT_MSG_FUNC = ASSERT_MSG_FUNC,
	};
	_ASSERTION_HANDLER_PTR = &ASSERTION_HANDLER;

	return 0;
}
