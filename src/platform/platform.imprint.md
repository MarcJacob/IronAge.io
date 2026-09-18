# WIN32 SOURCE FOLDER

This is the Platform implementations folder.
For every supported app type X platform, this folder contains the startup file and platform-specific source.
Startup files should take the form : PLATFORM_APP_NAME_MAIN.xxx

Currently the existing Win32 platform layer doesn't do much. In the future it needs to handle:

- File access
- Network connection management / routing
- Thread management
- CPU rendering (later OpenGL / WebGL ?)
- Hotreloading the game server code once it is turned into a dynamic library.