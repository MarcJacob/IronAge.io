# INSPECTION

Inspection is showing what an entity is: the panel for whatever is selected or interacted with, any entity type, own or not.

It is built generic on purpose: a panel is a title and ordered rows, rows are reusable elements, and entity types differ only in which sections they compose.
So a new entity type (or a new piece of information) is a new section, not a new panel. Display only: what the player can do is decided by the caller, which passes callbacks only for what it allows.

The data comes from the backend as snapshots (Game Data Objects, plus extra data for what the world sim does not shape for the UI). Nothing here reads world state directly.
