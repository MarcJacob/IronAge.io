// Symbol declaration for Game Message system, network-oriented message structures.

#ifndef GAME_MESSAGES_INCLUDED
#define GAME_MESSAGES_INCLUDED

// Defines the first portion of a game message.
// Use to interpret the following payload memory to the correct structure.
struct game_message_header
{
	ui16 message_type_code;

	ui16 payloadSize;
	ui8 _payload[];

	template<typename PayloadType>
	inline PayloadType& ReadPayload() { ASSERT(payloadSize >= sizeof(PayloadType)); return *(PayloadType*)_payload; }
};

#endif // GAME_MESSAGES_INCLUDED
