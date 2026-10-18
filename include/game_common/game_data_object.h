// Declaration file for the Game Data Object system.
// A Game Data Object ("GDOs") is essentially a "reflection friendly" version of a struct made for backward compatibility,
// the construction of debug UIs and abstracting away the specific byte offset of each data member when building
// or reading the structure in from an environment unaware of the actual C structure definition.

// Game Data Object structures (getting the "gdo_" prefix) can be created anywhere to cover any struct using a Macro-based system.
// The point of GDOs isn't storage or mass processing, but rather to provide a layer of interface between bytes known to be carrying properties we're interested in,
// and a system which only knows what name and kind of properties it is interested in, without caring about specific offsets or read order, or whatever else
// may be in the structure.
// GDOs can be passed "abstractly" as a game_data_object pointer or ref, and since they follow a predictable layout, can be read so meta / reflection systems
// can have some understanding of what they contain, and make changes based on known names which are more resistant to change than specific layouts / offsets.
//
// Be aware: using GDOs is very underperformant because everything is sacrificed to make it work (memory alignment, speed of random access...).
// Only use when absolutely necessary (typically with network / user interfaces).

#ifndef GAME_DATA_OBJECT_INCLUDED
#define GAME_DATA_OBJECT_INCLUDED

#include "core.h"

enum class GAME_DATA_PROP_TYPE : ui8
{
    // Integer types
    UI8, UI16, UI32, UI64,
    I8, I16, I32, I64,

    // VEC 2 types
    VEC2_UI8, VEC2_UI16, VEC2_UI32,
    VEC2_I8, VEC2_I16, VEC2_I32,
};

// GDO & Property definitions are packed to ensure GDO structs have predictable layouts.
#pragma pack(push, 1)
struct GAME_DATA_PROP_DEF
{
    const char* name;
    GAME_DATA_PROP_TYPE type;
    ui32 offset;
};

// "Header" structure for any type of underlying game data object.
// Abstract struct, to be passed as a pointer / ref and used in places where we're working with the "meta" layer of game data objects.
struct game_data_object
{
    ui32 _totalSize;
    ui32 _dataSize;
    ui32 _propsOffset;

    game_data_object(ui32 totalSize, ui32 dataSize, ui32 propsOffset) : 
        _totalSize(totalSize),
        _dataSize(dataSize),
        _propsOffset(propsOffset)
    {}

    // Returns the number of properties this gdo type has.
    inline ui32 get_prop_count() const
    {
        // Assumes the structure follows the proper internal layout: a copy of the covered data, followed by GAME_DATA_PROP_DEF structures.
        return (_totalSize - _propsOffset) / sizeof(GAME_DATA_PROP_DEF);
    }

    inline const GAME_DATA_PROP_DEF* get_property(ui16 propIndex) const
    {
        void* propPtr = (ui8*)this + _propsOffset + (sizeof(GAME_DATA_PROP_DEF) * propIndex);
        ASSERT((iptr)propPtr < (iptr)((ui8*)this + _totalSize));

        const GAME_DATA_PROP_DEF* propDefPtr = (const GAME_DATA_PROP_DEF*)propPtr;
        return propDefPtr;
    }
};
#pragma pack(pop)

#define GAME_DATA_OBJECT_DEF_BEGIN(StructType) \
        static_assert(alignof(StructType) == 1, "Game Data Object definitions can only work with packed structures."); \
        _Pragma("pack(push, 1)") \
        struct gdo_##StructType : public game_data_object { \
            using _StructType = StructType; \
            StructType _data; \
            gdo_##StructType() : game_data_object(sizeof(*this), sizeof(_StructType), (ui8*)&_data - (ui8*)this + sizeof(_StructType)), _data{}  \
            {}

#define GAME_DATA_PROPERTY(PropName, PropType) \
            GAME_DATA_PROP_DEF PropName = { #PropName, PropType, __builtin_offsetof(_StructType, PropName) };

#define GAME_DATA_OBJECT_DEF_END() \
        }; \
        _Pragma("pack(pop)")

// Example:
#if 1

#pragma pack(push, 1)
struct GameStuff
{
    i32 a, b;
    vec2i location;
};
#pragma pack(pop)

GAME_DATA_OBJECT_DEF_BEGIN(GameStuff)
    GAME_DATA_PROPERTY(a, GAME_DATA_PROP_TYPE::I32)
    GAME_DATA_PROPERTY(b, GAME_DATA_PROP_TYPE::I32)
    GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::VEC2_I32)
GAME_DATA_OBJECT_DEF_END()

inline void stuff_get_prop_names_list(ia_string_builder& str_builder)
{
    gdo_GameStuff gdo; // Create GDO of specific type with default data.

    for (ui16 propIndex = 0; propIndex < gdo.get_prop_count(); propIndex++)
    {
        const GAME_DATA_PROP_DEF* prop = gdo.get_property(propIndex);
        ASSERT_MSG(prop != nullptr, "Invalid property name ! Index = %hu", propIndex);

        str_builder.push_back(prop->name);
        str_builder.push_back('\n');
    }
}

#endif // End example

#endif // GAME_DATA_OBJECT_INCLUDED
