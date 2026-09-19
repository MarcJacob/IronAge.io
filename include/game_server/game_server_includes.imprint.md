# GAME SERVER INCLUDE FOLDER

"Global" symbols useable anywhere in the project related to the Game Server.

Contains the Platform <-> Game Server framework.

## Net framework

The Game Server assumes the platform is able to handle a large number of stable connections identified through a 4 bytes signed integer handle.
The platform must, as quickly as possible, service the various query and send function. To this end it is not recommended to have the platform
directly go into its TCP / IP API which in many cases will triggers blocks or expensive syscalls. Instead everything should be buffered and ready
for serving to the server on request (which does still imply thread-safety).