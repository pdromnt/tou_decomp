#include "accuracy_core.h"
#include "accuracy_runtime.h"

#include <stdio.h>
#include <string.h>

namespace {
int failures = 0;

uint8_t entity_pool[0x51400];
uint8_t type_table[0x11030];
int32_t trig_table[0xa00];
uint16_t palette[0x100];
uint8_t particle_pool[0xfa00];
uint8_t coarse_grid[0x100];
int link_table[0xc000];
uint8_t tilemap[0x10000];
uint8_t tile_properties[0x2000];
uint16_t framebuffer[0x10000];
uint8_t player_pool[0x1660];
uint8_t ship_stats[0x240];
uint8_t trooper_pool[0x1000];
uint8_t building_pool[0x1000];
uint8_t wall_records[0x2000];
uint8_t sprite_widths[0x5000];
uint8_t sprite_heights[0x5000];
int sprite_offsets[0x5000];
uint16_t sprite_pixels[0x10000];

void check(bool condition, const char *message)
{
    if (!condition) {
        ++failures;
        fprintf(stderr, "FAIL: %s\n", message);
    }
}
} // namespace

int DAT_00489248 = 0;
int DAT_00489250 = 0;
void *DAT_00487ab0 = trig_table;
void *DAT_004892e8 = entity_pool;
void *DAT_00481f34 = particle_pool;
void *DAT_00487abc = type_table;
void *DAT_00487aa8 = palette;
void *DAT_00487814 = coarse_grid;
void *DAT_0048781c = link_table;
int DAT_004879f8 = 8;
int DAT_00487834[12] = {0};
unsigned char DAT_00481e8f = 0;
unsigned int DAT_004879f0 = 256;
unsigned int DAT_004879f4 = 256;
int DAT_00487a00 = 256;
int DAT_00487a18 = 8;
void *DAT_0048782c = tilemap;
void *DAT_00487928 = tile_properties;
void *DAT_00481f50 = framebuffer;
void *DAT_00489e80 = wall_records;
int DAT_00489240 = 0;
int DAT_00487810 = reinterpret_cast<int>(player_pool);
void *DAT_0048780c = ship_stats;
char DAT_004892e5 = 0;
char DAT_0048373d = 0;
void *DAT_00487884 = trooper_pool;
int DAT_0048924c = 0;
void *DAT_00481f28 = building_pool;
int DAT_00489260 = 0;
void *DAT_00487818 = NULL;
float DAT_0048385c = 1.0f;
void *DAT_00489e8c = sprite_widths;
void *DAT_00489e88 = sprite_heights;
void *DAT_00489234 = sprite_offsets;
void *DAT_00487ab4 = sprite_pixels;
int DAT_00483828 = 1;
char DAT_00489288 = 1;

void FUN_004355d0(unsigned int) {}
void FUN_004357b0(int, int, int, unsigned char, char, int, int, int, int,
                  char, char, unsigned char) {}
void FUN_0040f9b0(int, int, int, int, int) {}
void FUN_00437cf0(int, int, int, int, int) {}
int FUN_004257e0(int, int, int, int) { return 0; }

int main()
{
    uint8_t bytes[8];
    memset(bytes, 0xcc, sizeof(bytes));
    tou_accuracy::store_u32(bytes, 1, 0x89abcdefu);
    check(bytes[1] == 0xef && bytes[2] == 0xcd && bytes[3] == 0xab && bytes[4] == 0x89,
          "guest stores are little-endian");
    check(tou_accuracy::load_u32(bytes, 1) == 0x89abcdefu,
          "guest u32 load round-trips");
    check(static_cast<uint32_t>(tou_accuracy::add_wrap_i32(0x7fffffff, 1)) == 0x80000000u,
          "signed add wraps at 32 bits");
    check(tou_accuracy::sar_i32(-8, 2) == -2, "SAR sign-extends");
    check(static_cast<uint32_t>(tou_accuracy::shl_wrap_i32(0x40000000, 1)) == 0x80000000u,
          "SHL wraps at 32 bits");

    tou_accuracy::MsvcRng rng(1u);
    const int expected[] = {41, 18467, 6334, 26500, 19169};
    for (unsigned int i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        check(rng.next() == expected[i], "MSVC6 RNG sequence matches seed 1");
    }

    uint16_t control_before = tou_accuracy::x87_control_word();
    check(tou_accuracy::x87_ftol(3.99L) == 3, "x87 ftol truncates positive values");
    check(tou_accuracy::x87_ftol(-3.99L) == -3, "x87 ftol truncates negative values");
    check(tou_accuracy::x87_ftol(9007199254740992.0L + 1.0L) == 9007199254740993LL,
          "x87 helper retains 80-bit integer precision");
    check(tou_accuracy::x87_control_word() == control_before,
          "x87 helper restores the control word");

    memset(type_table, 0xcc, sizeof(type_table));
    Accuracy_InitEntityCallbackTable();
    check(tou_accuracy::load_u32(type_table, 0x00 * 0x218) == 0x00438010u,
          "callback table restores type 0x00");
    check(tou_accuracy::load_u32(type_table, 0x17 * 0x218) == 0x00432c80u,
          "callback table restores Nucleus");
    check(tou_accuracy::load_u32(type_table, 0x2c * 0x218) == 0x0043f990u,
          "callback table restores type 0x2c");
    check(tou_accuracy::load_u32(type_table, 0x03 * 0x218) == 0u,
          "callback table preserves original null entries");

    memset(entity_pool, 0xaa, 0x80);
    Accuracy_InitNucleusMarkIIEntity(entity_pool);
    check(tou_accuracy::load_i32(entity_pool, 0x28) == 0x19,
          "Nucleus Mark II spawn initializes original 25-tick countdown");
    check(tou_accuracy::load_u32(entity_pool, 0x60) == 0xaaaaaaaau,
          "Nucleus Mark II spawn does not use reconstructed +0x60 timer");

    memset(entity_pool, 0, sizeof(entity_pool));
    memset(particle_pool, 0, sizeof(particle_pool));
    memset(coarse_grid, 0, sizeof(coarse_grid));
    for (int i = 0; i < 0xa00; ++i) trig_table[i] = i - 0x500;
    palette[15] = 0x1234;
    coarse_grid[0] = 8;
    DAT_00489248 = 1;
    DAT_00489250 = 0;
    DAT_00481e8f = 0;
    TOU_Accuracy_Srand(1u);
    tou_accuracy::store_u8(entity_pool, 0x21, 0x17);
    tou_accuracy::store_u8(entity_pool, 0x40, 1);
    tou_accuracy::store_i32(entity_pool, 0x28, 1);
    tou_accuracy::store_u8(entity_pool, 0x5c, 2);
    check(Accuracy_DispatchEntityCallback(0x00432c80u, 0),
          "Nucleus dispatch recognizes original callback address");
    check(tou_accuracy::load_i32(entity_pool, 0x28) == 0,
          "Nucleus countdown reaches zero without early detonation");
    check(tou_accuracy::load_u8(entity_pool, 0x20) == 0,
          "Nucleus does not detonate on the decrement-to-zero tick");
    check(tou_accuracy::load_u8(entity_pool, 0x5c) == 1,
          "Nucleus guard decrements byte-exactly");
    check(DAT_00489248 == 1 && DAT_00489250 == 0 && DAT_00481e8f == 0,
          "non-detonating Nucleus leaves pools and removal flag untouched");

    DAT_00481e8f = 0;
    check(Accuracy_DispatchEntityCallback(0x00432c80u, 0),
          "Nucleus second callback dispatch succeeds");
    check(tou_accuracy::load_u8(entity_pool, 0x20) == 0xfa,
          "Nucleus detonates on the following tick");
    check(DAT_00489248 == 14, "Nucleus spawns the original 13-point ring");
    check(DAT_00489250 == 1, "Nucleus spawns one gated flash particle");
    check(DAT_00481e8f == 1, "Nucleus requests removal after detonation");
    check(TOU_Accuracy_RandCallCount() == 2,
          "Nucleus consumes exactly two RNG calls when the flash gate passes");
    check(tou_accuracy::load_u32(entity_pool + 0x80, 0x34) == 0x00438010u,
          "Nucleus ring children retain the original callback address");
    check(tou_accuracy::load_i32(entity_pool + 0x80, 0x44) == 0x32000,
          "Nucleus subtype 1 ring damage matches assembly");
    check(tou_accuracy::load_u32(entity_pool + 0x80, 0x4c) == 0x1234u + 30000u,
          "Nucleus subtype 1 ring palette value matches assembly");

    /* Machine Gun Mark I performs the whole trace in one callback call. */
    memset(entity_pool, 0, sizeof(entity_pool));
    memset(coarse_grid, 0, sizeof(coarse_grid));
    memset(tilemap, 0, sizeof(tilemap));
    memset(tile_properties, 0, sizeof(tile_properties));
    tile_properties[2] = 1; /* tile 0 is passable */
    trig_table[0] = 0x40000;
    trig_table[0x200] = 0;
    DAT_00489248 = 1;
    DAT_00481e8f = 0;
    tou_accuracy::store_u8(entity_pool, 0x21, 0x2c);
    tou_accuracy::store_u8(entity_pool, 0x40, 0);
    tou_accuracy::store_u32(entity_pool, 0x2c, 0);
    tou_accuracy::store_i32(entity_pool, 0x00, 255 << 18);
    tou_accuracy::store_i32(entity_pool, 0x08, 10 << 18);
    check(Accuracy_DispatchEntityCallback(0x0043f990u, 0),
          "Machine Gun callback recognizes Mark I");
    check(tou_accuracy::load_i32(entity_pool, 0x18) == 0x80000,
          "Machine Gun derives doubled sine velocity in callback");
    check(DAT_00481e8f == 1,
          "Machine Gun instant trace terminates at the map boundary");

    /* At density 1.0 the original gate is rand()%20, not rand()%1. */
    memset(entity_pool, 0, sizeof(entity_pool));
    coarse_grid[15] = 8;
    DAT_00489248 = 1;
    DAT_00481e8f = 0;
    TOU_Accuracy_Srand(1u); /* first result 41: does not pass rand()%20 */
    tou_accuracy::store_u8(entity_pool, 0x21, 0x2c);
    tou_accuracy::store_u32(entity_pool, 0x2c, 0);
    tou_accuracy::store_i32(entity_pool, 0x00, 253 << 18);
    tou_accuracy::store_i32(entity_pool, 0x08, 10 << 18);
    Accuracy_DispatchEntityCallback(0x0043f990u, 0);
    check(DAT_00489248 == 1,
          "Machine Gun density 1.0 does not emit a trace mote when rand()%20 misses");

    memset(entity_pool, 0, sizeof(entity_pool));
    DAT_00489248 = 1;
    DAT_00481e8f = 0;
    TOU_Accuracy_Srand(19u); /* first result is divisible by 20 */
    tou_accuracy::store_u8(entity_pool, 0x21, 0x2c);
    tou_accuracy::store_u32(entity_pool, 0x2c, 0);
    tou_accuracy::store_i32(entity_pool, 0x00, 253 << 18);
    tou_accuracy::store_i32(entity_pool, 0x08, 10 << 18);
    Accuracy_DispatchEntityCallback(0x0043f990u, 0);
    check(DAT_00489248 > 1,
          "Machine Gun density 1.0 emits trace motes when rand()%20 passes");
    coarse_grid[15] = 0;
    tou_accuracy::store_u8(entity_pool, 0x21, 0x2d);
    check(!Accuracy_DispatchEntityCallback(0x0043f990u, 0),
          "shared Laser callback remains on its separate legacy path");

    /* Batch 2 callbacks are claimed only for their original weapon types. */
    tou_accuracy::store_u8(entity_pool, 0x21, 0x12);
    check(!Accuracy_DispatchEntityCallback(0x00438010u, 0),
          "shared Shotgun callback leaves Pipebomb on its separate path");
    tou_accuracy::store_u8(entity_pool, 0x21, 0x69);
    check(!Accuracy_DispatchEntityCallback(0x00438010u, 0),
          "shared Shotgun callback leaves Mine on its separate path");

    /* Shotgun/Rapidfire expires on the original life 2 -> 1 transition. */
    memset(entity_pool, 0, 0x80);
    DAT_00489248 = 1;
    DAT_00481e8f = 0;
    tou_accuracy::store_u8(entity_pool, 0x21, 0x00);
    tou_accuracy::store_i32(entity_pool, 0x28, 2);
    check(Accuracy_DispatchEntityCallback(0x00438010u, 0),
          "Shotgun/Rapidfire dispatch recognizes type 0");
    check(tou_accuracy::load_i32(entity_pool, 0x28) == 1 && DAT_00481e8f == 1,
          "Shotgun/Rapidfire keeps the assembly life-removal ordering");

    /* Dumbfire bounces only while +0x3c is greater than one. */
    memset(entity_pool, 0, 0x80);
    memset(tilemap, 0, sizeof(tilemap));
    memset(tile_properties, 0, sizeof(tile_properties));
    coarse_grid[0] = 8;
    DAT_00483828 = 0;
    DAT_00489248 = 1;
    DAT_00489250 = 0;
    DAT_00481e8f = 0;
    tou_accuracy::store_u8(entity_pool, 0x21, 0x01);
    tou_accuracy::store_u8(entity_pool, 0x40, 0x01);
    tou_accuracy::store_u8(entity_pool, 0x26, 0xff);
    tou_accuracy::store_i32(entity_pool, 0x00, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x04, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x08, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x0c, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x3c, 2);
    Accuracy_DispatchEntityCallback(0x00438d90u, 0);
    check(tou_accuracy::load_i32(entity_pool, 0x3c) == 1 && DAT_00481e8f == 0,
          "Dumbfire counter 2 performs its last bounce");
    Accuracy_DispatchEntityCallback(0x00438d90u, 0);
    check(DAT_00481e8f == 1,
          "Dumbfire counter 1 detonates instead of taking an extra bounce");
    check(DAT_00489250 == 1 && particle_pool[0x10] >= 13 && particle_pool[0x10] <= 16 &&
          particle_pool[0x15] == 1,
          "Bouncy Dumbfire impact emits its original fire-particle family");

    /* Collapser has no invented hand-spawned debris after its crater helper. */
    memset(entity_pool, 0, 0x80);
    DAT_00489248 = 1;
    DAT_00481e8f = 0;
    tou_accuracy::store_u8(entity_pool, 0x21, 0x05);
    tou_accuracy::store_u8(entity_pool, 0x26, 0xff);
    tou_accuracy::store_i32(entity_pool, 0x00, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x04, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x08, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x0c, 10 << 18);
    Accuracy_DispatchEntityCallback(0x00439880u, 0);
    check(DAT_00481e8f == 1 && DAT_00489248 == 1,
          "Collapser impact delegates debris to the original crater helper only");

    /* Kicker uses quarter-step physics and the binary's 1/density trail gate. */
    memset(entity_pool, 0, 0x100);
    memset(coarse_grid, 0, sizeof(coarse_grid));
    tile_properties[2] = 1;
    coarse_grid[0] = 8;
    DAT_0048385c = 1.0f;
    DAT_00483828 = 4;
    DAT_00489248 = 1;
    DAT_00481e8f = 0;
    TOU_Accuracy_Srand(1u);
    tou_accuracy::store_u8(entity_pool, 0x21, 0x09);
    tou_accuracy::store_u8(entity_pool, 0x26, 0xff);
    tou_accuracy::store_i32(entity_pool, 0x00, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x04, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x08, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x0c, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x18, 0x40000);
    tou_accuracy::store_i32(entity_pool, 0x1c, 0);
    tou_accuracy::store_i32(entity_pool, 0x38, 4);
    Accuracy_DispatchEntityCallback(0x0043cc20u, 0);
    check(tou_accuracy::load_i32(entity_pool, 0x00) == (10 << 18) + 0x50000,
          "Kicker performs five quarter-step horizontal integrations");
    check(tou_accuracy::load_i32(entity_pool, 0x08) == (10 << 18) + 15,
          "Kicker accumulates quartered gravity across five substeps");
    check(DAT_00489248 == 6,
          "Kicker emits one density-gated trail mote per substep at density 1");
    DAT_0048385c = 1.0f;
    DAT_00483828 = 1;

    /* Organic Waste I expires on life 2 -> 1, clearing the grow guard first. */
    memset(entity_pool, 0, 0x80);
    DAT_00489248 = 1;
    DAT_00481e8f = 0;
    tou_accuracy::store_u8(entity_pool, 0x21, 0x02);
    tou_accuracy::store_u8(entity_pool, 0x20, 10);
    tou_accuracy::store_i32(entity_pool, 0x28, 2);
    tou_accuracy::store_u8(entity_pool, 0x5c, 20);
    check(Accuracy_DispatchEntityCallback(0x004427e0u, 0),
          "Organic Waste I dispatch recognizes original callback");
    check(tou_accuracy::load_i32(entity_pool, 0x28) == 1 &&
          tou_accuracy::load_u8(entity_pool, 0x5c) == 0 && DAT_00481e8f == 1,
          "Organic Waste I preserves original life/guard removal ordering");

    /* Organic Waste II uses sprite 0x42..0x44 as a masked RGB555 terrain stamp. */
    memset(entity_pool, 0, 0x80);
    memset(tilemap, 0, sizeof(tilemap));
    memset(tile_properties, 0, sizeof(tile_properties));
    memset(framebuffer, 0, sizeof(framebuffer));
    memset(sprite_widths, 0, sizeof(sprite_widths));
    memset(sprite_heights, 0, sizeof(sprite_heights));
    memset(sprite_offsets, 0, sizeof(sprite_offsets));
    for (int sprite = 0x42; sprite <= 0x44; ++sprite) {
        sprite_widths[sprite] = 1;
        sprite_heights[sprite] = 1;
        sprite_offsets[sprite] = 0;
    }
    sprite_pixels[0] = 0x7c1f;
    tile_properties[1 * 0x20 + 0] = 1;
    tile_properties[1 * 0x20 + 2] = 0;
    tilemap[(10 << 8) + 10] = 1;
    DAT_00483828 = 0;
    DAT_00489248 = 1;
    DAT_00481e8f = 0;
    TOU_Accuracy_Srand(1u);
    tou_accuracy::store_u8(entity_pool, 0x21, 0x14);
    tou_accuracy::store_u8(entity_pool, 0x40, 1);
    tou_accuracy::store_u8(entity_pool, 0x26, 0xff);
    tou_accuracy::store_i32(entity_pool, 0x00, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x08, 10 << 18);
    check(Accuracy_DispatchEntityCallback(0x0043a4b0u, 0),
          "Organic Waste II dispatch recognizes original callback");
    check(tilemap[(10 << 8) + 10] == 0x15 && framebuffer[(10 << 8) + 10] == 0x7c1f,
          "Organic Waste II stamps tile 0x15 with the sprite RGB555 pixel");
    check(DAT_00481e8f == 1, "Organic Waste II removes after impact stamp");

    /* Machine Gun trace motes fade toward palette cutoff instead of living forever. */
    memset(entity_pool, 0, 0x80);
    DAT_00489248 = 1;
    DAT_00481e8f = 0;
    tou_accuracy::store_u8(entity_pool, 0x21, 0x67);
    tou_accuracy::store_u8(entity_pool, 0x40, 0);
    tou_accuracy::store_u8(entity_pool, 0x65, 20);
    tou_accuracy::store_u8(entity_pool, 0x64, 18);
    tou_accuracy::store_u8(entity_pool, 0x5c, 1);
    tou_accuracy::store_i32(entity_pool, 0x00, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x08, 10 << 18);
    check(Accuracy_DispatchEntityCallback(0x00430480u, 0),
          "trace mote dispatch recognizes original callback");
    check(tou_accuracy::load_u8(entity_pool, 0x65) == 19 && DAT_00481e8f == 0,
          "trace mote fades one palette step per configured tick");
    Accuracy_DispatchEntityCallback(0x00430480u, 0);
    check(tou_accuracy::load_u8(entity_pool, 0x65) == 18 && DAT_00481e8f == 1,
          "trace mote vanishes at the original palette cutoff");

    memset(entity_pool, 0xaa, 0x100);
    memset(entity_pool + 0x80, 0xbb, 0x80);
    DAT_00489248 = 2;
    tou_accuracy::store_u8(entity_pool, 0x21, 0x03);
    tou_accuracy::store_u8(entity_pool + 0x80, 0x21, 0x04);
    tou_accuracy::store_i32(entity_pool + 0x80, 0x00, 0x12345678);
    Accuracy_RemoveEntityAt(0);
    check(DAT_00489248 == 1, "entity removal decrements the pool count");
    check(tou_accuracy::load_i32(entity_pool, 0x00) == 0x12345678,
          "entity removal copies selected original fields");
    check(entity_pool[0x23] == 0xaa,
          "entity removal does not invent a full-record memcpy");

    if (failures != 0) {
        fprintf(stderr, "%d accuracy test(s) failed\n", failures);
        return 1;
    }
    puts("accuracy core tests passed");
    return 0;
}
