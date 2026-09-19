// Browser input events, translated into calls the rest of the client understands.

// Right now a click sets the entity's target location. Later the destination of the input will be the network, not the local backend.
export function init_input(canvas_element, page_to_world, set_target_loc) {
    canvas_element.addEventListener('click', (clickEvent) => {
        const world = page_to_world(clickEvent.clientX, clickEvent.clientY);
        set_target_loc(world.x, world.y);
    });
}
