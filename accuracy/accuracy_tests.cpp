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
uint8_t edge_particle_pool[0xbb80];
uint8_t explosion_descriptor[0xa0];
uint8_t emitter_pool[0x800];
uint8_t spatial_game_state[0x10000];
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
uint8_t sprite_grayscale[0x10000];

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
int DAT_0048925c = 0;
int DAT_004892d8 = 0;
void *DAT_00487ab0 = trig_table;
void *DAT_004892e8 = entity_pool;
void *DAT_00481f34 = particle_pool;
void *DAT_00481f2c = edge_particle_pool;
void *DAT_00481f20 = explosion_descriptor;
void *DAT_00487aa0 = emitter_pool;
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
void *DAT_00487aa4 = spatial_game_state;
int DAT_0048924c = 0;
void *DAT_00481f28 = building_pool;
int DAT_00489260 = 0;
void *DAT_00487818 = NULL;
float DAT_0048385c = 1.0f;
float DAT_00483854 = 1.0f;
void *DAT_00489e8c = sprite_widths;
void *DAT_00489e88 = sprite_heights;
void *DAT_00489234 = sprite_offsets;
void *DAT_00487ab4 = sprite_pixels;
void *DAT_00489e94 = sprite_grayscale;
int DAT_00483828 = 1;
char DAT_00489288 = 1;
unsigned char g_ConfigBlob[6408] = {0};
int captured_explosion_param11 = -1;
int captured_explosion_calls = 0;

void FUN_004355d0(unsigned int) {}
void FUN_004357b0(int, int, int, unsigned char, char, int, int, int, int,
                  char, char param11, unsigned char)
{
    captured_explosion_param11 = param11;
    ++captured_explosion_calls;
}
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

    /* Shared callbacks are claimed only for the exact lifted entity types. */
    tou_accuracy::store_u8(entity_pool, 0x21, 0x69);
    check(!Accuracy_DispatchEntityCallback(0x00438010u, 0),
          "shared Shotgun callback leaves Mine on its separate path");

    /* Airstrike impact emits the original 82-spoke burst plus its final flash. */
    memset(entity_pool, 0, 0x80);
    memset(particle_pool, 0, sizeof(particle_pool));
    memset(coarse_grid, 0, sizeof(coarse_grid));
    coarse_grid[0] = 8;
    DAT_00489248 = 1;
    DAT_00489250 = 0;
    DAT_00481e8f = 0;
    tou_accuracy::store_u8(entity_pool, 0x20, 0xff);
    tou_accuracy::store_u8(entity_pool, 0x21, 0x12);
    tou_accuracy::store_i32(entity_pool, 0x00, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x08, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x04, 9 << 18);
    tou_accuracy::store_i32(entity_pool, 0x0c, 9 << 18);
    check(Accuracy_DispatchEntityCallback(0x00438010u, 0),
          "shared callback recognizes Airstrike type 0x12");
    check(DAT_00489250 == 83 && DAT_00481e8f == 1,
          "Airstrike restores its 82-spoke impact burst and final flash");

    /* Batch 3 callback ownership is explicit and does not fall through legacy code. */
    memset(entity_pool, 0, 0x80);
    DAT_00489248 = 1;
    DAT_00481e8f = 0;
    tou_accuracy::store_u8(entity_pool, 0x20, 0xff);
    tou_accuracy::store_u8(entity_pool, 0x21, 0x11);
    check(Accuracy_DispatchEntityCallback(0x00441aa0u, 0) && DAT_00481e8f == 1,
          "Normal Fireball callback recognizes type 0x11");
    tou_accuracy::store_u8(entity_pool, 0x21, 0x10);
    check(!Accuracy_DispatchEntityCallback(0x00441aa0u, 0),
          "Normal Fireball callback rejects Flamethrower type 0x10");

    /* Nuclear Barrel anchors its visible blast above the buried projectile center. */
    memset(entity_pool, 0, 0x100);
    memset(particle_pool, 0, sizeof(particle_pool));
    memset(explosion_descriptor, 0, sizeof(explosion_descriptor));
    memset(coarse_grid, 0, sizeof(coarse_grid));
    explosion_descriptor[5] = 16;
    coarse_grid[0] = 8;
    DAT_00483854 = 0.0f;
    DAT_0048385c = 0.0f;
    DAT_00489248 = 1;
    DAT_00489250 = 0;
    DAT_00481e8f = 0;
    tou_accuracy::store_u8(entity_pool, 0x20, 0xfa);
    tou_accuracy::store_u8(entity_pool, 0x21, 0x0b);
    tou_accuracy::store_i32(entity_pool, 0, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 8, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 4, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x0c, 10 << 18);
    check(Accuracy_DispatchEntityCallback(0x00431650u, 0),
          "Nuclear Barrel callback recognizes type 0x0b");
    check(DAT_00489250 == 1 && tou_accuracy::load_i32(particle_pool, 4) == (7 << 18),
          "Nuclear Barrel restores the original sprite-height blast offset");

    /* Bone Crusher's dedicated four-team scan instantly kills touching infantry. */
    memset(entity_pool, 0, 0x80);
    memset(trooper_pool, 0, sizeof(trooper_pool));
    memset(spatial_game_state, 0, sizeof(spatial_game_state));
    memset(coarse_grid, 0, sizeof(coarse_grid));
    DAT_0048385c = 1.0f;
    DAT_00489248 = 1;
    DAT_0048924c = 1;
    DAT_00489240 = 1;
    DAT_00481e8f = 0;
    player_pool[0x2c] = 0;
    tou_accuracy::store_i32(spatial_game_state, 0x4008, 1);
    tou_accuracy::store_i32(spatial_game_state, 0x400c, 0);
    tou_accuracy::store_i32(trooper_pool, 0, 10 << 18);
    tou_accuracy::store_i32(trooper_pool, 8, 10 << 18);
    tou_accuracy::store_i32(trooper_pool, 0x28, 100);
    tou_accuracy::store_u8(entity_pool, 0x21, 0x0f);
    tou_accuracy::store_u8(entity_pool, 0x22, 0);
    tou_accuracy::store_u8(entity_pool, 0x64, 3);
    tou_accuracy::store_i32(entity_pool, 0, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 8, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 4, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x0c, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x60, 5000);
    check(Accuracy_DispatchEntityCallback(0x004330c0u, 0),
          "Bone Crusher callback recognizes type 0x0f");
    check(tou_accuracy::load_i32(trooper_pool, 0x28) == -1000000,
          "Bone Crusher uses its original instant-kill infantry state write");
    check(tou_accuracy::load_i32(entity_pool, 0x60) == 3999 &&
          tou_accuracy::load_i32(entity_pool, 0x3c) == 59,
          "Bone Crusher applies the original contact fuse and debris counters");

    /* A normal Mark I Fireball emission produces both flame pools. */
    memset(entity_pool, 0, 0x80);
    memset(particle_pool, 0, sizeof(particle_pool));
    memset(edge_particle_pool, 0, sizeof(edge_particle_pool));
    memset(coarse_grid, 0, sizeof(coarse_grid));
    memset(tile_properties, 0, sizeof(tile_properties));
    coarse_grid[0] = 8;
    tile_properties[2] = 1;
    DAT_00489248 = 1;
    DAT_00489250 = 0;
    DAT_0048925c = 0;
    DAT_00481e8f = 0;
    tou_accuracy::store_u8(entity_pool, 0x21, 0x11);
    tou_accuracy::store_u8(entity_pool, 0x5c, 1);
    tou_accuracy::store_i32(entity_pool, 0, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 8, 10 << 18);
    check(Accuracy_DispatchEntityCallback(0x00441aa0u, 0),
          "Normal Fireball Mark I emits through its lifted callback");
    check(DAT_00489250 == 1 && DAT_0048925c == 1,
          "Normal Fireball emits both primary and secondary flame particles");
    check(particle_pool[0x10] >= 3 && particle_pool[0x10] <= 4 &&
          edge_particle_pool[0x10] <= 1,
          "Normal Fireball Mark I uses the original paired flame sprite ranges");

    /* Fireball impact leaves the original persistent flame emitter behind. */
    memset(entity_pool, 0, 0x80);
    memset(emitter_pool, 0, sizeof(emitter_pool));
    memset(tile_properties, 0, sizeof(tile_properties));
    DAT_00489248 = 1;
    DAT_00489250 = 0;
    DAT_0048925c = 0;
    DAT_004892d8 = 0;
    DAT_00481e8f = 0;
    g_ConfigBlob[0x17d0] = 1;
    tou_accuracy::store_u8(entity_pool, 0x21, 0x11);
    tou_accuracy::store_i32(entity_pool, 0, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 8, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 4, 9 << 18);
    tou_accuracy::store_i32(entity_pool, 0x0c, 8 << 18);
    check(Accuracy_DispatchEntityCallback(0x00441aa0u, 0),
          "Normal Fireball impact dispatches through its lifted callback");
    check(DAT_004892d8 == 1 && tou_accuracy::load_i32(emitter_pool, 0) == 9 &&
          tou_accuracy::load_i32(emitter_pool, 4) == 8,
          "Normal Fireball impact restores its previous-position flame emitter");
    check(tou_accuracy::load_i32(emitter_pool, 8) == 300 && emitter_pool[0x0c] == 3 &&
          emitter_pool[0x0d] >= 2 && emitter_pool[0x0d] <= 3 && emitter_pool[0x0e] == 2,
          "Normal Fireball impact emitter matches the original lifetime and sprite family");

    /* Plastic Explosives Mark I stamps its grayscale explosion mask into terrain. */
    memset(entity_pool, 0, 0x80);
    memset(tilemap, 0, sizeof(tilemap));
    memset(tile_properties, 0, sizeof(tile_properties));
    memset(framebuffer, 0, sizeof(framebuffer));
    memset(sprite_widths, 0, sizeof(sprite_widths));
    memset(sprite_heights, 0, sizeof(sprite_heights));
    memset(sprite_offsets, 0, sizeof(sprite_offsets));
    memset(sprite_pixels, 0, sizeof(sprite_pixels));
    memset(sprite_grayscale, 0, sizeof(sprite_grayscale));
    for (int variant = 0; variant < 3; ++variant) {
        sprite_widths[0x194 + variant] = 1;
        sprite_heights[0x194 + variant] = 1;
        sprite_offsets[0x194 + variant] = variant;
        sprite_grayscale[variant] = 0xff;
    }
    sprite_widths[0x2f] = 1;
    sprite_heights[0x2f] = 1;
    sprite_offsets[0x2f] = 10;
    sprite_pixels[10] = 0x07e0;
    DAT_00489248 = 1;
    DAT_00481e8f = 0;
    tou_accuracy::store_u8(entity_pool, 0x21, 0x14);
    tou_accuracy::store_u8(entity_pool, 0x40, 0);
    tou_accuracy::store_i32(entity_pool, 0, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 8, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 4, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x0c, 10 << 18);
    check(Accuracy_DispatchEntityCallback(0x0043a4b0u, 0),
          "Plastic Explosives uses the shared 0x0043a4b0 callback");
    check(tilemap[(10 << 8) + 10] == 7 && framebuffer[(10 << 8) + 10] == 0x07e0,
          "Plastic Explosives Mark I restores its opaque terrain stamp");

    /* Mark II/III unit hits branch to the same impact VFX instead of returning early. */
    for (int fireball_mode = 1; fireball_mode <= 2; ++fireball_mode) {
        memset(entity_pool, 0, 0x80);
        memset(particle_pool, 0, sizeof(particle_pool));
        memset(emitter_pool, 0, sizeof(emitter_pool));
        memset(trooper_pool, 0, sizeof(trooper_pool));
        memset(coarse_grid, 0, sizeof(coarse_grid));
        coarse_grid[0] = 12; /* infantry collision plus visible-effect region */
        DAT_00489248 = 1;
        DAT_00489250 = 0;
        DAT_0048925c = 0;
        DAT_004892d8 = 0;
        DAT_0048924c = 1;
        DAT_00481e8f = 0;
        tou_accuracy::store_i32(trooper_pool, 0, 10 << 18);
        tou_accuracy::store_i32(trooper_pool, 8, 10 << 18);
        tou_accuracy::store_i32(trooper_pool, 0x20, 0);
        tou_accuracy::store_i32(trooper_pool, 0x28, 1000000);
        tou_accuracy::store_u8(trooper_pool, 0x14, 1);
        tou_accuracy::store_u8(entity_pool, 0x21, 0x11);
        tou_accuracy::store_u8(entity_pool, 0x22, 0);
        tou_accuracy::store_u8(entity_pool, 0x40, static_cast<uint8_t>(fireball_mode));
        tou_accuracy::store_i32(entity_pool, 0, 10 << 18);
        tou_accuracy::store_i32(entity_pool, 8, 10 << 18);
        tou_accuracy::store_i32(entity_pool, 4, 9 << 18);
        tou_accuracy::store_i32(entity_pool, 0x0c, 8 << 18);
        tou_accuracy::store_i32(entity_pool, 0x44, 100);
        Accuracy_DispatchEntityCallback(0x00441aa0u, 0);
        check(DAT_004892d8 == 1 && DAT_00481e8f == 1,
              fireball_mode == 1
                  ? "Normal Fireball Mark II unit hit keeps the full impact VFX path"
                  : "Normal Fireball Mark III unit hit keeps the full impact VFX path");
        check(DAT_00489250 == (fireball_mode == 1 ? 32 : 48),
              fireball_mode == 1
                  ? "Normal Fireball Mark II unit hit emits its 32-particle burst"
                  : "Normal Fireball Mark III unit hit emits its 48-particle burst");
    }

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

    /* Explosive-terrain fragments carry state 0x32/subtype 2.  The original
     * callback passes param_11=1 so FUN_004357b0 expands their crater. */
    memset(entity_pool, 0, 0x80);
    memset(tilemap, 0, sizeof(tilemap));
    memset(tile_properties, 0, sizeof(tile_properties));
    DAT_00483828 = 0;
    DAT_00489248 = 1;
    DAT_00489240 = 0;
    DAT_00481e8f = 0;
    captured_explosion_param11 = -1;
    captured_explosion_calls = 0;
    tilemap[10 * 256 + 10] = 7;
    tou_accuracy::store_u8(entity_pool, 0x20, 0x32);
    tou_accuracy::store_u8(entity_pool, 0x21, 0x00);
    tou_accuracy::store_u8(entity_pool, 0x22, 0x00);
    tou_accuracy::store_u8(entity_pool, 0x26, 0xff);
    tou_accuracy::store_u8(entity_pool, 0x40, 0x02);
    tou_accuracy::store_i32(entity_pool, 0x00, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x04, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x08, 10 << 18);
    tou_accuracy::store_i32(entity_pool, 0x0c, 10 << 18);
    Accuracy_DispatchEntityCallback(0x00438010u, 0);
    check(captured_explosion_calls == 1 && captured_explosion_param11 == 1,
          "Plastic debris restores the original randomized large-crater flag");

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
