#ifndef TOU_SPRITE_ATLAS_H
#define TOU_SPRITE_ATLAS_H

#include <stdint.h>

/* all3.gfx keeps descriptor fields in three parallel tables. This view makes
 * reads explicit while retaining their verified widths: signed 32-bit pixel
 * offset and unsigned 8-bit dimensions. It intentionally does not reinterpret
 * grayscale indices as RGB565 pixels. */
typedef struct SpriteAtlasFrame {
    int32_t pixel_offset;
    uint8_t width;
    uint8_t height;
} SpriteAtlasFrame;

extern void *DAT_00489234;
extern void *DAT_00489e8c;
extern void *DAT_00489e88;

static inline SpriteAtlasFrame SpriteAtlas_GetFrame(int index)
{
    SpriteAtlasFrame frame;
    frame.pixel_offset = static_cast<const int32_t *>(DAT_00489234)[index];
    frame.width = static_cast<const uint8_t *>(DAT_00489e8c)[index];
    frame.height = static_cast<const uint8_t *>(DAT_00489e88)[index];
    return frame;
}

#endif
