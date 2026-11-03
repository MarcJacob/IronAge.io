
#ifndef TROOP_TYPES_INCLUDED
#define TROOP_TYPES_INCLUDED

#include "core.h"

// Troop types, cheapest first. Also the index into army_troop_count::count and GAME_PARAMETERS.troop_type_stats.
enum class TROOP_TYPE : ui8
{
    LEVIES = 0,
    MEN_AT_ARMS = 1,
    HORSEMEN = 2,
    KNIGHTS = 3,
    WAGONS = 4, // Handled like levies (civilian while in the garrison), but deal no damage and carry goods.

    TROOP_TYPE_COUNT,
};

// Contains a count of all troop types.
struct army_troop_count
{
    // @TODO(Marc): Remove union, leaving only the array and the helper.
    union
    {
        struct {
            ui16 levies;
            ui16 men_at_arms;
            ui16 horsemen;
            ui16 knights;
            ui16 wagons;
        };
        ui16 count[(ui8)TROOP_TYPE::TROOP_TYPE_COUNT];
    };

    inline ui16& access_troop_count(TROOP_TYPE type)
    {
        ASSERT(type < TROOP_TYPE::TROOP_TYPE_COUNT);
        switch(type)
        {
            case TROOP_TYPE::LEVIES: return count[(ui8)type];
            case TROOP_TYPE::MEN_AT_ARMS: return count[(ui8)type];
            case TROOP_TYPE::HORSEMEN: return count[(ui8)type];
            case TROOP_TYPE::KNIGHTS: return count[(ui8)type];
            case TROOP_TYPE::WAGONS: return count[(ui8)type];
            default: return count[0];
        }
    }

};

// Total number of men of all troop types. ui64: the sum of several ui16 counts (and of several groups) must not overflow.
static inline ui64 army_total_manpower(const army_troop_count& troops)
{
    ui64 sum = 0;
    for (ui8 troopTypeIndex = 0; troopTypeIndex < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; troopTypeIndex++)
        sum += troops.count[troopTypeIndex];
    return sum;
}

#endif // TROOP_TYPES_INCLUDED
