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

// Size of the game_data_object header (3 ui32): the covered struct copy starts right after it.
const GDO_HEADER_SIZE = 12;

// A property value: a number, or {x, y} for the vec2 types and world locations. Entity guids are numbers, player ids too.
export type GDOValue = number | { x: number, y: number };

export interface GDOProperty
{
    name: string;
    type: GAME_DATA_PROP_TYPE;
    value: GDOValue;
}

// Reads one property value from the struct copy of a GDO (data starts at the copy). Null for unsupported types (64 bit ones).
function read_GDO_value(data: DataView, prop: GDOPropDef): GDOValue | null
{
    const at = prop.offset;
    switch (prop.type)
    {
    case GAME_DATA_PROP_TYPE.UI8: return data.getUint8(at);
    case GAME_DATA_PROP_TYPE.I8: return data.getInt8(at);
    case GAME_DATA_PROP_TYPE.UI16:
    case GAME_DATA_PROP_TYPE.PLAYER_ID: return data.getUint16(at, true);
    case GAME_DATA_PROP_TYPE.I16: return data.getInt16(at, true);
    case GAME_DATA_PROP_TYPE.UI32:
    case GAME_DATA_PROP_TYPE.ENTITY_GUID: return data.getUint32(at, true);
    case GAME_DATA_PROP_TYPE.I32: return data.getInt32(at, true);
    case GAME_DATA_PROP_TYPE.VEC2_UI8: return { x: data.getUint8(at), y: data.getUint8(at + 1) };
    case GAME_DATA_PROP_TYPE.VEC2_I8: return { x: data.getInt8(at), y: data.getInt8(at + 1) };
    case GAME_DATA_PROP_TYPE.VEC2_UI16:
    case GAME_DATA_PROP_TYPE.WORLD_LOCATION: return { x: data.getUint16(at, true), y: data.getUint16(at + 2, true) };
    case GAME_DATA_PROP_TYPE.VEC2_I16: return { x: data.getInt16(at, true), y: data.getInt16(at + 2, true) };
    case GAME_DATA_PROP_TYPE.VEC2_UI32: return { x: data.getUint32(at, true), y: data.getUint32(at + 4, true) };
    case GAME_DATA_PROP_TYPE.VEC2_I32: return { x: data.getInt32(at, true), y: data.getInt32(at + 4, true) };
    default: return null;
    }
}

// Reads every property of the GDO at the given wasm memory offset as a list, in definition order. Properties of unsupported types are left out.
// schema: the GDO's property definitions if the caller already has them (a GDO type's schema never changes), otherwise they are read from memory.
export function read_GDO_properties(gdo_offset: number, schema?: GDOPropDef[]): GDOProperty[]
{
    const memory = Backend.get_memory_bytes();
    const data = new DataView(memory.buffer, gdo_offset + GDO_HEADER_SIZE);

    const properties: GDOProperty[] = [];
    for (const prop of schema ?? read_GDO_schema(gdo_offset))
    {
        const value = read_GDO_value(data, prop);
        if (value !== null) properties.push({ name: prop.name, type: prop.type, value });
    }
    return properties;
}

// Reads the value of every numeric property of the GDO at the given wasm memory offset, by property name.
// schema: the GDO's property definitions if the caller already has them (they never change for a GDO type), saving reading them each time.
export function read_GDO_values(gdo_offset: number, schema?: GDOPropDef[]): { [name: string]: number }
{
    const values: { [name: string]: number } = {};
    for (const property of read_GDO_properties(gdo_offset, schema))
    {
        if (typeof property.value === "number") values[property.name] = property.value;
    }
    return values;
}

// A snapshot of a GDO's properties, lifted out of the backend: in definition order, and readable by name.
export class GDOData
{
    properties: GDOProperty[];
    private values = new Map<string, GDOValue>();

    constructor(properties: GDOProperty[])
    {
        this.properties = properties;
        for (const property of properties) this.values.set(property.name, property.value);
    }

    // Value of a numeric property (ints, guids, player ids). 0 if there is no such property.
    get_number(name: string): number
    {
        const value = this.values.get(name);
        return typeof value === "number" ? value : 0;
    }

    // Value of a location / vec2 property. {0, 0} if there is no such property.
    get_location(name: string): { x: number, y: number }
    {
        const value = this.values.get(name);
        return value !== undefined && typeof value !== "number" ? value : { x: 0, y: 0 };
    }
}

// Entity inspection. Entity GDOs have their own buffer in the backend, overwritten by every query: the values are lifted out right away.
// The schema of an entity type never changes at runtime, so it is read once per type (the entity type is the low byte of the guid).
const ENTITY_SCHEMAS = new Map<number, GDOPropDef[]>();

// A snapshot of an entity's state. Raw values, named as in the entity's GDO (world_entities.h). Stays as it was when queried: query again to see changes.
export class EntityData extends GDOData
{
    entity_type: number; // ENTITY_TYPE, from the guid.

    constructor(public guid: number, properties: GDOProperty[])
    {
        super(properties);
        this.entity_type = guid & 0xFF;
    }
}

// Current state of the entity, or null if it doesn't exist (anymore) or its type has no GDO.
// Cost: one backend call, plus one object per property.
export function query_entity(guid: number): EntityData | null
{
    if (Backend.entity_gdo_query(guid) === 0) return null;

    const offset = Backend.entity_gdo_get_buffer_offset();
    const entityType = guid & 0xFF;

    let schema = ENTITY_SCHEMAS.get(entityType);
    if (schema === undefined)
    {
        schema = read_GDO_schema(offset);
        ENTITY_SCHEMAS.set(entityType, schema);
    }
    return new EntityData(guid, read_GDO_properties(offset, schema));
}

let GAME_PARAMETERS: GDOData | null = null;

// The global game parameters (game_parameters in include/game_common/match/game_parameters.h): read them with get_number(<field name>).
// Per mille values are thousandths (20 = 2%). They are constants of the build: read from the backend once, then cached.
export function get_game_parameters(): GDOData
{
    if (GAME_PARAMETERS === null)
    {
        const offset = Backend.game_parameters_gdo_get_offset();
        GAME_PARAMETERS = new GDOData(read_GDO_properties(offset));
    }
    return GAME_PARAMETERS;
}

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
