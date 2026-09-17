# WIN32 SOURCE FOLDER

This is the Win32 Platform implementation folder.

Using the Win32 library, the program starts and allocates the desired resources to start a Game Server, preparing a Game Server Platform structure and enough memory for the server's entire lifetime.

Currently this platform layer doesn't do much else. In the future it needs to handle:

- File access
- Network connection management / routing
- Thread management
- Simple graphics for managing the server at runtime ?
- Hotreloading the game server code once it is turned into a dynamic library.