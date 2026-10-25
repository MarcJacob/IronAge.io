# INSPECTION

Inspection is showing what an entity is: the panel for whatever is selected or interacted with, any entity type, own or not.

Each entity type has its own panel: a UI module bound to static markup in *index.html* (the new module style, see the web client's AI notes), all living inside one shared frame module (title and owner subtitle).
So a new entity type is a new panel module, and a new piece of information is a new static field in its type's panel. Which panel shows is decided in one place, in *inspect_entity.ts*.
Display only: what the player can do is decided by the owning mode, which turns on editing through the context it passes and subscribes to the panel's events.
The data comes from the backend as snapshots (Game Data Objects, plus extra data for what the world sim does not shape for the UI). Nothing here reads world state directly.
