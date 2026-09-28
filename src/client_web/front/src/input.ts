// Browser input events, translated into calls the rest of the client understands.

import * as Backend from "./backend.js"
import * as Render from "./render.js"

// Right now a click sets the entity's target location. Later the destination of the input will be the network, not the local backend.
export function init_input(canvas_element: HTMLCanvasElement)
{
    canvas_element.addEventListener('click', (clickEvent) =>
    {
        const worldLocation = Render.page_to_world(clickEvent.clientX, clickEvent.clientY);
        Backend.set_target_loc(worldLocation);
    });
}
