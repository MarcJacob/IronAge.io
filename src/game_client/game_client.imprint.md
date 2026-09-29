# GAME CLIENT BACKEND SOURCES

This folder contains the Game Client backend implementation of the game.

Contains the common backend behavior used by any game client:
- Running a local match instance for solo play or synchronized multiplayer play.
- Maintaining a coherent input & abstract view / rendering state.
- Supplying the necessary information for the platform / front code to do its work, and for it to be able to pass input events.
- Receiving & handling network messages from the platform.

In Game Client code, the 'platform' is the "host" code that takes care of allocating resources, calling major lifecycle / bridging functions, capturing user
input and presenting them the game's current state.

See \<Project Root>/include/game_client/ for more about the Game Client <-> Client platform relationship.
