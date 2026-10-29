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
    count: number; // Number of items (>1 = array).
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
    value: GDOValue; // The value, or the first item of an array property.
    items?: GDOValue[]; // All the items of an array property (count > 1).
}

// Byte size of one item of a property type (mirrors get_data_prop_size). 0 for unsupported types.
function get_GDO_prop_size(type: GAME_DATA_PROP_TYPE): number
{
    switch (type)
    {
    case GAME_DATA_PROP_TYPE.UI8:
    case GAME_DATA_PROP_TYPE.I8: return 1;
    case GAME_DATA_PROP_TYPE.UI16:
    case GAME_DATA_PROP_TYPE.I16:
    case GAME_DATA_PROP_TYPE.VEC2_UI8:
    case GAME_DATA_PROP_TYPE.VEC2_I8:
    case GAME_DATA_PROP_TYPE.PLAYER_ID: return 2;
    case GAME_DATA_PROP_TYPE.UI32:
    case GAME_DATA_PROP_TYPE.I32:
    case GAME_DATA_PROP_TYPE.VEC2_UI16:
    case GAME_DATA_PROP_TYPE.VEC2_I16:
    case GAME_DATA_PROP_TYPE.WORLD_LOCATION:
    case GAME_DATA_PROP_TYPE.ENTITY_GUID: return 4;
    case GAME_DATA_PROP_TYPE.VEC2_UI32:
    case GAME_DATA_PROP_TYPE.VEC2_I32: return 8;
    default: return 0;
    }
}

// Reads one property value from the struct copy of a GDO (data starts at the copy). Null for unsupported types (64 bit ones).
// index: item of an array property.
function read_GDO_value(data: DataView, prop: GDOPropDef, index: number = 0): GDOValue | null
{
    const at = prop.offset + index * get_GDO_prop_size(prop.type);
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
        if (value === null) continue;

        const property: GDOProperty = { name: prop.name, type: prop.type, value };
        if (prop.count > 1)
        {
            property.items = [value];
            for (let index = 1; index < prop.count; index++) property.items.push(read_GDO_value(data, prop, index)!);
        }
        properties.push(property);
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
    private arrays = new Map<string, GDOValue[]>();

    constructor(properties: GDOProperty[])
    {
        this.properties = properties;
        for (const property of properties)
        {
            this.values.set(property.name, property.value);
            if (property.items !== undefined) this.arrays.set(property.name, property.items);
        }
    }

    // Items of a numeric array property (ints, guids, player ids). Empty if there is no such property.
    // An array of one item can't be told from a single value (count 1): it comes back as that one item.
    get_array(name: string): number[]
    {
        const items = this.arrays.get(name) ?? (this.values.has(name) ? [this.values.get(name)!] : []);
        return items.filter((item): item is number => typeof item === "number");
    }

    // Text of an array property of bytes (I8 / UI8 items), up to the first null. Empty if there is no such property.
    get_string(name: string): string
    {
        const bytes: number[] = [];
        for (const item of this.get_array(name))
        {
            if (item === 0) break;
            bytes.push(item & 0xFF);
        }
        return TEXT_DECODER.decode(new Uint8Array(bytes));
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

// Entity inspection. The backend writes the entity's GDO (and extra data for some types) in its inspection buffer, overwritten by every inspection:
// the values are lifted out right away. The schemas of an entity type never change at runtime, so they are read once per type (the entity type is the low byte of the guid).
const ENTITY_SCHEMAS = new Map<number, GDOPropDef[]>();
const ENTITY_EXTRA_SCHEMAS = new Map<number, GDOPropDef[]>();

// A snapshot of an entity's state. Raw values, named as in the entity's GDO (world_entities.h). Stays as it was when queried: query again to see changes.
export class EntityData extends GDOData
{
    entity_type: number; // ENTITY_TYPE, from the guid.

    // Data the entity's GDO doesn't provide, built by the web layer for the UI (web_game_client_query.cpp). Null for entity types without any.
    // Settlement: construction_project_count, and arrays construction_project_building_type / construction_project_progress (0..255) with that many items used.
    extra: GDOData | null;

    constructor(public guid: number, properties: GDOProperty[], extra: GDOData | null = null)
    {
        super(properties);
        this.entity_type = guid & 0xFF;
        this.extra = extra;
    }
}

// Current state of the entity, or null if it doesn't exist (anymore) or its type has no GDO.
// Cost: one backend call, plus one object per property.
export function query_entity(guid: number): EntityData | null
{
    const totalSize = Backend.entity_inspect(guid);
    if (totalSize === 0) return null;

    const offset = Backend.inspected_entity_gdo_get_offset();
    const entityType = guid & 0xFF;

    let schema = ENTITY_SCHEMAS.get(entityType);
    if (schema === undefined)
    {
        schema = read_GDO_schema(offset);
        ENTITY_SCHEMAS.set(entityType, schema);
    }
    const properties = read_GDO_properties(offset, schema);

    // The extra data GDO follows the entity's GDO: there is some if more than the entity GDO was written (its size is the first ui32 of its header).
    const gdoSize = new DataView(Backend.get_memory_bytes().buffer, offset, 4).getUint32(0, true);
    let extra: GDOData | null = null;
    if (totalSize > gdoSize)
    {
        const extraOffset = Backend.inspected_entity_extra_data_get_offset();
        let extraSchema = ENTITY_EXTRA_SCHEMAS.get(entityType);
        if (extraSchema === undefined)
        {
            extraSchema = read_GDO_schema(extraOffset);
            ENTITY_EXTRA_SCHEMAS.set(entityType, extraSchema);
        }
        extra = new GDOData(read_GDO_properties(extraOffset, extraSchema));
    }
    return new EntityData(guid, properties, extra);
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

// A settlement building type, from its row of SETTLEMENT_BUILDING_TABLE (world_entities.h). Type = index of the row, as in building_counts.
export interface BuildingDefinition
{
    name: string;
    description: string;
    build_days: number;
    cost_per_day: number; // Wealth per day while under construction.
    workers: number; // Workers taken from the peasants while under construction.
    requires_coast: boolean; // Only coastal settlements can build it.
}

// The table is constant: each type is read from the backend once, then cached. Schema (same for all rows) cached too.
const BUILDING_DEFINITIONS = new Map<number, BuildingDefinition>();
let BUILDING_DEFINITION_SCHEMA: GDOPropDef[] | null = null;

// Definition of a building type, or null if there is no such type.
export function get_building_definition(type: number): BuildingDefinition | null
{
    const cached = BUILDING_DEFINITIONS.get(type);
    if (cached !== undefined) return cached;

    const offset = Backend.building_definition_gdo_get_offset(type);
    if (offset === 0) return null;

    if (BUILDING_DEFINITION_SCHEMA === null) BUILDING_DEFINITION_SCHEMA = read_GDO_schema(offset);
    const data = new GDOData(read_GDO_properties(offset, BUILDING_DEFINITION_SCHEMA));
    const definition: BuildingDefinition = {
        name: data.get_string("name"),
        description: data.get_string("description"),
        build_days: data.get_number("build_days"),
        cost_per_day: data.get_number("cost_per_day"),
        workers: data.get_number("workers"),
        requires_coast: data.get_number("requires_coast") !== 0,
    };
    BUILDING_DEFINITIONS.set(type, definition);
    return definition;
}

// The local player's state (world_player_state in include/game_common/match/world/world.h).
export function get_local_player_state(): GDOData
{
    return new GDOData(read_GDO_properties(Backend.player_state_gdo_get_offset()));
}

// Reads the property definitions of the GDO at the given wasm memory offset, for debugging.
// Layout (packed, wasm32): game_data_object header = total size, data size, props offset (ui32 each), then the struct copy,
// then one GAME_DATA_PROP_DEF per property = name pointer (ui32 @0), type (ui8 @4), offset in struct (ui32 @5), item count (ui16 @9), 11 bytes.
export function read_GDO_schema(gdo_offset: number): GDOPropDef[]
{
    const memory = Backend.get_memory_bytes();
    const header = new DataView(memory.buffer, gdo_offset, 12);
    const totalSize = header.getUint32(0, true);
    const propsOffset = header.getUint32(8, true);
    const view = new DataView(memory.buffer, gdo_offset, totalSize);

    const props: GDOPropDef[] = [];
    for (let at = propsOffset; at + 11 <= totalSize; at += 11)
    {
        const nameStart = view.getUint32(at, true);
        let nameEnd = nameStart;
        while (memory[nameEnd] !== 0) nameEnd++;
        props.push({
            name: TEXT_DECODER.decode(memory.subarray(nameStart, nameEnd)),
            type: view.getUint8(at + 4),
            offset: view.getUint32(at + 5, true),
            count: view.getUint16(at + 9, true),
        });
    }
    return props;
}
