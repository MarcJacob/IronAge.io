# WIN32 GAME SERVER PLATFORM LAYER SOURCES

Full Win32 implementation, entry point and compilation point for the Win32 Game Server.

The bulk of the code lives in _main.cpp, and it is also where everything else gets unity-compiled.

However, with complexification of the platform code to handle network activity there's now multiple files which justified putting it in its own folder.

Time will tell if we'll eventually move some code to some sort of "win32 common" folder for re-use by other Win32 platform programs.

## Win32 services for the game server

The Win32 platform allocates a generous amount of memory for the server and does not yet support giving more if needed at runtime.

It provides standard & error output through a Windows console which is also the only window that is opened (therefore other means of visual feedback are not supported / stubs).

Uptime is measured using Win32's QueryPerformanceCounter and QueryPerformanceFrequency to provide millisecond-accurate integer time measurements as required by the server.

Networking is provided by a WinSock2-based implementation.
