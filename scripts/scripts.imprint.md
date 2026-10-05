# PROJECT SCRIPTS FOLDER

This folder contains general-purpose scripts that are shipped alongside the rest of the project in source control.
The scripts are tied to a specific platform and are made to facilitate building & deployment & testing systems for the project.

The user is encouraged to create a *scripts.local* file and / or local scripts with the *.local.* string in their names to customize their dev experience,
or even ignore the scripts entirely and setup alternative building methods (like setting up a VS project (@TODO(Marc): Provide a script to generate VS project ?).

## Win32

Win32 scripts:
- **win32_dev_env_setup.cmd** = Adds a set of useful environment variables to the caller, specifically various paths to source and build locations.
                                - You can assume that any batch script shipped with this project will be calling this.
                                - @TODO: Send a message to any running local game server to have itself be reloaded (game server library AND resource files).

- ***_build.cmd**           = rebuilds the corresponding project only. These live next to the app's sources in *src/platform/*, not in this folder.
                                - Called by win32_build_all and other building scripts.
                                - The web client build also deploys the web root into the *game_server_resources* folder for discovery by an active game server instance.

- **win32_build_all.cmd**   = rebuilds every single application in the project. Good for a first time build or when you've made changes to common code.

- **win32_launch_server.cmd** = launches the built game server, optionally with a given executable and resources folder.

- **full_ship.cmd**         = @TODO Triggers a full rebuild in release mode and packs everything together neatly in an archive at <Build Folder>/Ship.

## MacOS

@TODO (God help us)

## Linux distros

@TODO
