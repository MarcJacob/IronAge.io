# PROJECT SCRIPTS FOLDER

This folder contains general-purpose scripts that are shipped alongside the rest of the project in source control.
The scripts are tied to a specific platform and are made to facilitate building & deployment & testing systems for the project.

The user is encouraged to create a *scripts.local* file and / or local scripts with the *.local.* string in their names to customize their dev experience,
or even ignore the scripts entirely and setup alternative building methods (like setting up a VS project (@TODO(Marc): Provide a script to generate VS project ?).

## Win32

Win32 scripts:
- **dev_env_setup.cmd**     = Adds a set of useful environment variables to the caller, specifically various paths to source and build locations.
                                - You can assume that any batch script shipped with this project will be calling this.
                                - @TODO: Send a message to any running local game server to have itself be reloaded (game server library AND resource files).

- **deploy_web_client.cmd** = Fetches the contents of the web root within the web client sources and copies them to the *game_server_resources* folder for discovery by an active game server instance.
                                - The output resources can of course be moved anywhere else.

- ***_build.cmd**           = rebuilds the corresponding project only.
                                - Called by full_rebuild and other building scripts.

- **full_rebuild.cmd**      = rebuilds every single application in the project. Good for a first time build or when you've made changes to common code.
                                - Also triggers deploy_web_client

- **full_ship.cmd**         = @TODO Triggers a full rebuild in release mode and packs everything together neatly in an archive at <Build Folder>/Ship.

## MacOS

@TODO (God help us)

## Linux distros

@TODO
