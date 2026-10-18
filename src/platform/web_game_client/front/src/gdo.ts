// Reading Game Data Objects (GDO, see include/game_common/game_data_object.h) living in backend memory.
// Only the generic layout is known here; what a GDO is used for (e.g. building match commands, see input.ts) is up to the caller.

import * as Backend from "./backend.js"

// Mirrors GAME_DATA_PROP_TYPE (include/game_common/game_data_object.h). Update here if the enum changes.
export enum GAME_DATA_PROP_TYPE
{
    UI8, UI16, UI32, UI64,
    I8, I16, I32, I64,
    VEC2_UI8, VEC2_UI16, VEC2_UI32,
    VEC2_I8, VEC2_I16, VEC2_I32,
    GAME_TYPES_BEGIN,
    WORLD_LOCATION,
    ENTITY_GUID,
    PLAYER_ID,
}

export interface GDOPropDef
{
    name: string;
    type: GAME_DATA_PROP_TYPE;
    offset: number; // In the covered struct.
}

const TEXT_DECODER = new TextDecoder();

// Reads the property definitions of the GDO at the given wasm memory offset, for debugging.
// Layout (packed, wasm32): game_data_object header = total size, data size, props offset (ui32 each), then the struct copy,
// then one GAME_DATA_PROP_DEF per property = name pointer (ui32 @0), type (ui8 @4), offset in struct (ui32 @5), 9 bytes.
export function read_GDO_schema(gdo_offset: number): GDOPropDef[]
{
    const memory = Backend.get_memory_bytes();
    const header = new DataView(memory.buffer, gdo_offset, 12);
    const totalSize = header.getUint32(0, true);
    const propsOffset = header.getUint32(8, true);
    const view = new DataView(memory.buffer, gdo_offset, totalSize);

    const props: GDOPropDef[] = [];
    for (let at = propsOffset; at + 9 <= totalSize; at += 9)
    {
        const nameStart = view.getUint32(at, true);
        let nameEnd = nameStart;
        while (memory[nameEnd] !== 0) nameEnd++;
        props.push({
            name: TEXT_DECODER.decode(memory.subarray(nameStart, nameEnd)),
            type: view.getUint8(at + 4),
            offset: view.getUint32(at + 5, true),
        });
    }
    return props;
}
