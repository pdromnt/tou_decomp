#ifndef TOU_TERRAIN_PROPERTIES_H
#define TOU_TERRAIN_PROPERTIES_H

#include <stdint.h>

/* The original binary owns a 256 x 0x20 byte table initialized by
 * FUN_0041EAE0. These accessors type its physical shape without pretending
 * every byte's gameplay meaning has been proven. Prefer a raw offset here over
 * inventing broad IsSolid/IsFluid helpers from one caller's interpretation. */
enum { TERRAIN_PROPERTY_STRIDE = 0x20, TERRAIN_PROPERTY_COUNT = 256 };

typedef struct TerrainPropertyRecord {
    uint8_t bytes[TERRAIN_PROPERTY_STRIDE];
} TerrainPropertyRecord;

static_assert(sizeof(TerrainPropertyRecord) == TERRAIN_PROPERTY_STRIDE,
              "terrain property record stride");

extern void *DAT_00487928;

static inline const TerrainPropertyRecord *TerrainProperty_Get(uint8_t tile)
{
    return &static_cast<const TerrainPropertyRecord *>(DAT_00487928)[tile];
}

static inline uint8_t TerrainProperty_Read(uint8_t tile, uint8_t offset)
{
    return offset < TERRAIN_PROPERTY_STRIDE
        ? TerrainProperty_Get(tile)->bytes[offset] : 0;
}

#endif
