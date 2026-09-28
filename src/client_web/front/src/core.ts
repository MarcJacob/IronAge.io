// Common function & type declarations imported in every other src file.

// Commonly used type for bytes received / being sent to network.
export type ByteBuffer = Uint8Array<ArrayBuffer>;
export const ByteBuffer = Uint8Array;

export class WorldLocation
{
    x: number = 0;
    y: number = 0;
}

export class WorldSize
{
    width: number = 0;
    height: number = 0;
}

// Parameters of a match. To see current world state of a match, look into the world_state structure.
export class MatchInfo
{
    tick_rate: number = 0;
    world_size: WorldSize = new WorldSize();
}


