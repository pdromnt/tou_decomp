#include "entity_callbacks.h"

#include "binary_compat.h"
#include "tou.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

namespace {

const uint32_t kCallbackNucleus = 0x00432c80u;
const uint32_t kCallbackNuclearBarrel = 0x00431650u;
const uint32_t kCallbackMovingSucker = 0x00430dc0u;
const uint32_t kCallbackBoneCrusher = 0x004330c0u;
const uint32_t kCallbackShotgunRapidfire = 0x00438010u;
const uint32_t kCallbackDumbfire = 0x00438d90u;
const uint32_t kCallbackCollapser = 0x00439880u;
const uint32_t kCallbackTournaillerKicker = 0x0043cc20u;
const uint32_t kCallbackMachineGun = 0x0043f990u;
const uint32_t kCallbackSmokingNalle = 0x00432220u;
const uint32_t kCallbackMorningStar = 0x0043dbd0u;
const uint32_t kCallbackKometBomb = 0x0043e070u;
const uint32_t kCallbackGammaBoom = 0x004457b0u;
const uint32_t kCallbackWavyFireworks = 0x004442f0u;
const uint32_t kCallbackRomanCandle = 0x00446130u;
const uint32_t kCallbackEtna = 0x00447a70u;
const uint32_t kCallbackOrganicWaste = 0x004427e0u;
const uint32_t kCallbackOrganicWasteII = 0x0043a4b0u;
const uint32_t kCallbackNormalFireball = 0x00441aa0u;
const uint32_t kCallbackTrail = 0x00430480u;
uint8_t *entity_at(int index)
{
    return reinterpret_cast<uint8_t *>(&g_EntityPool[index]);
}

int game_rand()
{
    return TOU_Rand();
}

bool map_point_valid(int x, int y)
{
    return x > 0 && y > 0 && x < static_cast<int>(DAT_004879f0) &&
           y < static_cast<int>(DAT_004879f4);
}

int tile_offset(int x, int y)
{
    return (y << (static_cast<unsigned char>(DAT_00487a18) & 0x1f)) + x;
}

uint8_t tile_at(int x, int y)
{
    if (x < 0 || y < 0 || x >= static_cast<int>(DAT_004879f0) ||
        y >= static_cast<int>(DAT_004879f4)) return 0;
    return static_cast<uint8_t *>(DAT_0048782c)[tile_offset(x, y)];
}

uint8_t tile_property(uint8_t tile, int property)
{
    return static_cast<uint8_t *>(DAT_00487928)[static_cast<unsigned int>(tile) * 0x20u + property];
}

uint16_t rgb555_to_rgb565(uint16_t color)
{
    const uint16_t red = static_cast<uint16_t>((color >> 10) & 0x1fu);
    const uint16_t green = static_cast<uint16_t>((color >> 5) & 0x1fu);
    const uint16_t blue = static_cast<uint16_t>(color & 0x1fu);
    return static_cast<uint16_t>((red << 11) | (green << 6) | blue);
}

uint16_t rgb565_to_rgb555(uint16_t color)
{
    const uint16_t red = static_cast<uint16_t>((color >> 11) & 0x1fu);
    const uint16_t green = static_cast<uint16_t>((color >> 6) & 0x1fu);
    const uint16_t blue = static_cast<uint16_t>(color & 0x1fu);
    return static_cast<uint16_t>((red << 10) | (green << 5) | blue);
}

void damage_special_tile(uint8_t *entity)
{
    const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x00), 0x12);
    const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x08), 0x12);
    const uint8_t tile = tile_at(x, y);
    if (tile > 0xefu && DAT_00489e80 != NULL) {
        uint8_t *record = static_cast<uint8_t *>(DAT_00489e80) +
                          static_cast<unsigned int>(tile) * 0x20u - 0x1df4u;
        tou_binary::store_i32(record, 0,
            tou_binary::load_i32(record, 0) - tou_binary::load_i32(entity, 0x44));
    }
}

void collision_players(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    const uint8_t type = tou_binary::load_u8(entity, 0x21);
    const uint8_t subtype = tou_binary::load_u8(entity, 0x40);
    const uint8_t owner = tou_binary::load_u8(entity, 0x22);
    const uint8_t guard = tou_binary::load_u8(entity, 0x26);
    unsigned int guard_team = owner;
    if (type == 0x1fu && owner < static_cast<unsigned int>(DAT_00489240)) {
        guard_team = Player_Get(owner)->team;
    }
    uint8_t collision_w = 2;
    uint8_t collision_h = 2;
    if (DAT_00487abc != NULL) {
        const unsigned int entry = static_cast<unsigned int>(type) * 0x218u + subtype;
        collision_w = tou_binary::load_u8(DAT_00487abc, entry + 0x136u);
        collision_h = tou_binary::load_u8(DAT_00487abc, entry + 0x13cu);
    }
    const int32_t ex = tou_binary::load_i32(entity, 0x00);
    const int32_t ey = tou_binary::load_i32(entity, 0x08);
    const int32_t damage = tou_binary::load_i32(entity, 0x44);
    for (int player = 0; player < DAT_00489240; ++player) {
        PlayerData *p = Player_Get(player);
        if (p->health <= 0) continue;
        const unsigned int player_id = type == 0x1fu ? p->team
                                                     : static_cast<unsigned int>(player);
        if (guard != 0u && guard_team == player_id) continue;
        int ship_size = DAT_0048780c != NULL
            ? tou_binary::load_i32(DAT_0048780c, player * 0x40 + 0x38) : 0;
        int32_t hx = ship_size + static_cast<int32_t>(collision_w) * FIXED_SCALE;
        int32_t hy = ship_size + static_cast<int32_t>(collision_h) * FIXED_SCALE;
        if (DAT_004892e5 != 0) { hx += 0x140000; hy += 0x140000; }
        const int32_t px = p->position_x;
        const int32_t py = p->position_y;
        if (!(px - hx < ex && ex < px + hx && py - hy < ey && ey < py + hy)) continue;

        bool may_damage = true;
        if (owner < 0x50u) {
            const uint8_t owner_team = Player_Get(owner)->team;
            may_damage = owner_team != p->team || DAT_0048373d != 0;
        }
        if (may_damage) p->health = tou_binary::sub_wrap_i32(p->health, damage);
        p->timer_c4 = 5;
        p->flag_a3 = 1;
        if (DAT_00487abc != NULL) {
            const unsigned int entry = static_cast<unsigned int>(type) * 0x218u + subtype;
            const uint8_t divisor = tou_binary::load_u8(DAT_00487abc, entry + 0xa6u);
            if (divisor != 0u && divisor != 99u) {
                p->velocity_x = tou_binary::add_wrap_i32(
                    p->velocity_x, tou_binary::load_i32(entity, 0x18) / divisor);
                p->velocity_y = tou_binary::add_wrap_i32(
                    p->velocity_y, tou_binary::load_i32(entity, 0x1c) / divisor);
            }
        }
        DAT_00481e8f = 4;
        return;
    }
}

void collision_tracked(int entity_index)
{
    uint8_t *projectile = entity_at(entity_index);
    static const int32_t x_radius[9] = {
        0x100000, 0x100000, 0x140000, 0x140000, 0x140000,
        0x1c0000, 0x100000, 0x180000, 0x140000
    };
    static const int32_t y_above[9] = {
        FIXED_SCALE, 0x100000, FIXED_SCALE, FIXED_SCALE, 0xc0000,
        0x1c0000, 0x100000, 0x200000, 0x140000
    };
    static const int32_t y_below[9] = {
        0x140000, 0x100000, 0x200000, 0x1c0000, 0xc0000,
        0x1c0000, 0x100000, 0x200000, 0x140000
    };
    static const int32_t threshold[9] = {
        0x25800, 1, 0x271000, 0x7d000, 0xbb800,
        0x138800, 0, 0x465000, 0xfa000
    };
    const int32_t px = tou_binary::load_i32(projectile, 0x00);
    const int32_t py = tou_binary::load_i32(projectile, 0x08);
    const int32_t damage = tou_binary::load_i32(projectile, 0x44);
    const uint8_t owner = tou_binary::load_u8(projectile, 0x22);
    const uint8_t projectile_type = tou_binary::load_u8(projectile, 0x21);
    const uint8_t projectile_subtype = tou_binary::load_u8(projectile, 0x40);
    for (int category = 0; category < 9; ++category) {
        int *links = static_cast<int *>(DAT_0048781c) + category * 0x1000;
        for (int slot = 0; slot < DAT_00487834[category]; ++slot) {
            uint8_t *target = entity_at(links[slot]);
            const uint8_t state = tou_binary::load_u8(target, 0x20);
            if (state == 0xfau || (category == 7 && state == 0xfbu)) continue;
            if (tou_binary::load_u8(target, 0x5c) != 0u &&
                tou_binary::load_u8(target, 0x22) == owner) continue;
            if (category == 5 && state == 0xdeu) continue;
            const int32_t tx = tou_binary::load_i32(target, 0x00);
            const int32_t ty = tou_binary::load_i32(target, 0x08);
            if (!(tx - x_radius[category] < px && px < tx + x_radius[category] &&
                  ty - y_above[category] < py && py < ty + y_below[category])) continue;
            tou_binary::store_u16(target, 0x24, 1);
            tou_binary::store_i32(target, 0x58, damage);
            const int32_t accumulated = tou_binary::load_i32(target, 0x28) + damage;
            tou_binary::store_i32(target, 0x28, accumulated);
            int32_t limit = threshold[category];
            if (category == 6) {
                const uint8_t subtype = tou_binary::load_u8(target, 0x40);
                if (subtype == 0u) limit = 2000000000;
                else if (subtype == 1u) limit = 0x70800;
                else limit = 0;
            }
            bool kill = limit != 0 && accumulated >= limit;
            if (category == 8 && projectile_type == 0x6au && projectile_subtype == 2u) kill = false;
            if (kill) tou_binary::store_u8(target, 0x20, 0xfa);
            DAT_00481e8f = 3;
            return;
        }
    }
}

void collision_troopers(int entity_index)
{
    if (g_TrooperPool == NULL) return;
    uint8_t *projectile = entity_at(entity_index);
    const uint8_t owner = tou_binary::load_u8(projectile, 0x22);
    const uint8_t team = owner >= 0x50u && owner <= 0x63u ? owner - 0x50u : 0xfbu;
    const int32_t x = tou_binary::load_i32(projectile, 0x00);
    const int32_t y = tou_binary::load_i32(projectile, 0x08);
    for (int i = 0; i < g_TrooperCount; ++i) {
        TrooperRecord *trooper = &g_TrooperPool[i];
        const uint8_t velocity_y_low =
            reinterpret_cast<const uint8_t *>(&trooper->velocity_y)[0];
        if (trooper->movement_speed < 0 || velocity_y_low == owner) continue;
        const int32_t tx = trooper->position_x;
        const int32_t ty = trooper->position_y;
        if (!(tx - 0x140000 < x && x < tx + 0x140000 &&
              ty - 0x1c0000 < y && y < ty + FIXED_SCALE)) continue;
        DAT_00481e8f = 3;
        trooper->health_28 -= tou_binary::load_i32(projectile, 0x44);
        /* The original intentionally writes a dword at +0x2c, spanning the
         * byte flags and the first byte of aim_angle_30. Keep this packed view. */
        tou_binary::store_i32(trooper, 0x2c, 1);
        if (trooper->kind_25 == 1u && team != trooper->team &&
            tou_binary::load_i32(projectile, 0x44) > 0x7d000) {
            trooper->aim_angle_30 = tou_binary::load_i32(projectile, 0x18) < 0 ? -1 : 1;
        }
        return;
    }
}

bool collision_checks(int entity_index, bool tracked)
{
    uint8_t *entity = entity_at(entity_index);
    const int cx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x00), 0x16);
    const int cy = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x08), 0x16);
    const uint8_t bits = static_cast<uint8_t *>(DAT_00487814)[cx + cy * DAT_004879f8];
    if ((bits & 1u) != 0u) { collision_players(entity_index); if (DAT_00481e8f == 4u) return true; }
    if (tracked && (bits & 2u) != 0u) { collision_tracked(entity_index); if (DAT_00481e8f == 3u) return true; }
    if ((bits & 4u) != 0u) { collision_troopers(entity_index); if (DAT_00481e8f == 3u) return true; }
    const int tx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x00), 0x12);
    const int ty = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x08), 0x12);
    if (tile_property(tile_at(tx, ty), 10) == 1u) {
        FUN_004355d0(static_cast<unsigned int>(entity_index));
        if (DAT_00481e8f != 0u) return true;
    }
    return false;
}

void collision_prepass_00437b10(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    const uint8_t owner = tou_binary::load_u8(entity, 0x22);
    uint8_t team = 0xfb;
    if (owner < 0x46u) {
        team = Player_Get(owner)->team;
    } else if (owner >= 0x50u && owner <= 0x63u) team = owner - 0x50u;
    else if (owner >= 100u && owner <= 119u) team = owner - 100u;
    else if (owner >= 0x78u && owner <= 0x8bu) team = owner - 0x78u;
    int *links = static_cast<int *>(DAT_0048781c) + 0x6000;
    const int32_t x = tou_binary::load_i32(entity, 0x00);
    const int32_t y = tou_binary::load_i32(entity, 0x08);
    for (int slot = 0; slot < DAT_00487834[6]; ++slot) {
        uint8_t *target = entity_at(links[slot]);
        if (tou_binary::load_u8(target, 0x22) == team) continue;
        const int32_t tx = tou_binary::load_i32(target, 0x00);
        const int32_t ty = tou_binary::load_i32(target, 0x08);
        if (!(tx - 0x1900000 < x && x < tx + 0x1900000 &&
              ty - 0x1900000 < y && y < ty + 0x1900000)) continue;
        const uint32_t angle = static_cast<uint32_t>(FUN_004257e0(x, y, tx, ty)) & 0x7ffu;
        const int32_t sx = static_cast<int32_t *>(DAT_00487ab0)[angle];
        const int32_t sy = static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200u];
        if (tou_binary::load_u8(target, 0x40) != 0u) {
            tou_binary::store_i32(entity, 0x18, tou_binary::load_i32(entity, 0x18) + (sx >> 1));
            tou_binary::store_i32(entity, 0x1c, tou_binary::load_i32(entity, 0x1c) + (sy >> 1));
        } else {
            tou_binary::store_i32(entity, 0x18, tou_binary::load_i32(entity, 0x18) - sx);
            tou_binary::store_i32(entity, 0x1c, tou_binary::load_i32(entity, 0x1c) - sy);
        }
    }
}

int tracking_category(uint8_t type)
{
    switch (type) {
    case 0x0b: return 0;
    case 0x17: return 1;
    case 0x0f: return 2;
    case 0x18: return 3;
    case 0x1f: return 4;
    case 0x1c: return 5;
    case 0x0e: return 6;
    case 0x2e: return 7;
    case 0x27: return 8;
    default: return -1;
    }
}

bool is_tracked(const uint8_t *entity)
{
    if (DAT_00487abc == NULL) return false;
    uint8_t type = tou_binary::load_u8(entity, 0x21);
    uint8_t subtype = tou_binary::load_u8(entity, 0x40);
    const uint8_t *types = static_cast<const uint8_t *>(DAT_00487abc);
    return types[static_cast<unsigned int>(type) * 0x218u + subtype + 0x130u] == 1u;
}

void copy_entity_fields(uint8_t *destination, const uint8_t *source)
{
    static const uint8_t dword_offsets[] = {
        0x00, 0x08, 0x04, 0x0c, 0x2c, 0x30, 0x10, 0x14, 0x18, 0x1c,
        0x28, 0x34, 0x38, 0x3c, 0x44, 0x48, 0x4c, 0x50, 0x58, 0x60
    };
    static const uint8_t byte_offsets[] = {
        0x21, 0x20, 0x26, 0x22, 0x40, 0x54, 0x5c, 0x64, 0x65
    };

    for (unsigned int i = 0; i < sizeof(dword_offsets); ++i) {
        uint8_t offset = dword_offsets[i];
        tou_binary::store_u32(destination, offset,
                                tou_binary::load_u32(source, offset));
    }
    for (unsigned int i = 0; i < sizeof(byte_offsets); ++i) {
        uint8_t offset = byte_offsets[i];
        tou_binary::store_u8(destination, offset,
                               tou_binary::load_u8(source, offset));
    }
    tou_binary::store_u16(destination, 0x24,
                            tou_binary::load_u16(source, 0x24));
}

bool trace_enabled()
{
    static int initialized = 0;
    static bool enabled = false;
    if (!initialized) {
        const char *value = getenv("TOU_ENTITY_TRACE");
        enabled = value != NULL && value[0] != '\0' && value[0] != '0';
        initialized = 1;
    }
    return enabled;
}

void trace_callback(uint64_t sequence, uint32_t callback_address, int entity_index,
                    int32_t before_x, int32_t before_y, int32_t before_life,
                    uint8_t before_state, uint8_t before_guard,
                    uint64_t rng_before)
{
    if (!trace_enabled()) return;
    uint8_t *entity = entity_at(entity_index);
    FILE *file = fopen("entity-trace.csv", sequence == 0 ? "w" : "a");
    if (file == NULL) return;
    if (sequence == 0) {
        fputs("sequence,callback,index,type,subtype,before_x,before_y,after_x,after_y,"
              "before_life,after_life,before_state,after_state,before_guard,after_guard,"
              "remove,rng_before,rng_after,entity_count,particle_count\n", file);
    }
    fprintf(file,
            "%llu,%08lx,%d,%u,%u,%ld,%ld,%ld,%ld,%ld,%ld,%u,%u,%u,%u,%u,%llu,%llu,%d,%d\n",
            static_cast<unsigned long long>(sequence),
            static_cast<unsigned long>(callback_address), entity_index,
            static_cast<unsigned int>(tou_binary::load_u8(entity, 0x21)),
            static_cast<unsigned int>(tou_binary::load_u8(entity, 0x40)),
            static_cast<long>(before_x), static_cast<long>(before_y),
            static_cast<long>(tou_binary::load_i32(entity, 0x00)),
            static_cast<long>(tou_binary::load_i32(entity, 0x08)),
            static_cast<long>(before_life),
            static_cast<long>(tou_binary::load_i32(entity, 0x28)),
            static_cast<unsigned int>(before_state),
            static_cast<unsigned int>(tou_binary::load_u8(entity, 0x20)),
            static_cast<unsigned int>(before_guard),
            static_cast<unsigned int>(tou_binary::load_u8(entity, 0x5c)),
            static_cast<unsigned int>(DAT_00481e8f),
            static_cast<unsigned long long>(rng_before),
            static_cast<unsigned long long>(TOU_RandCallCount()),
            g_EntityCount, g_ParticleCount);
    fclose(file);
}

void callback_nucleus_00432c80(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    const uint8_t subtype = tou_binary::load_u8(entity, 0x40);

    if (subtype == 1u) {
        int32_t countdown = tou_binary::load_i32(entity, 0x28);
        if (countdown < 1) {
            tou_binary::store_u8(entity, 0x20, 0xfau);
        } else {
            tou_binary::store_i32(entity, 0x28, countdown - 1);
        }
    }

    uint8_t guard = tou_binary::load_u8(entity, 0x5c);
    if (guard != 0u) {
        tou_binary::store_u8(entity, 0x5c, static_cast<uint8_t>(guard - 1u));
    }

    if (tou_binary::load_u8(entity, 0x20) == 0xfau) {
        const uint32_t starting_angle = static_cast<uint32_t>(TOU_Rand()) & 0x7ffu;
        int angle_offset = 0;
        do {
            if (g_EntityCount >= ENTITY_ACTIVE_CAPACITY) break;

            const uint32_t angle = (starting_angle + static_cast<uint32_t>(angle_offset)) & 0x7ffu;
            uint8_t *spawned = entity_at(g_EntityCount);
            const int32_t x = tou_binary::load_i32(entity, 0x00);
            const int32_t y = tou_binary::load_i32(entity, 0x08);
            const int32_t sin_value = static_cast<int32_t *>(DAT_00487ab0)[angle];
            const int32_t cos_value = static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200u];

            tou_binary::store_i32(spawned, 0x00, x);
            tou_binary::store_i32(spawned, 0x08, y);
            tou_binary::store_i32(spawned, 0x18,
                tou_binary::sar_i32(tou_binary::shl_wrap_i32(
                    tou_binary::mul_wrap_i32(sin_value, 5), 4), 6));
            tou_binary::store_i32(spawned, 0x1c,
                tou_binary::sar_i32(tou_binary::shl_wrap_i32(
                    tou_binary::mul_wrap_i32(cos_value, 5), 4), 6));
            tou_binary::store_i32(spawned, 0x04, x);
            tou_binary::store_i32(spawned, 0x0c, y);
            tou_binary::store_i32(spawned, 0x10, 0);
            tou_binary::store_i32(spawned, 0x14, 0);
            tou_binary::store_u8(spawned, 0x21, 0);
            tou_binary::store_u16(spawned, 0x24, 0);
            tou_binary::store_u8(spawned, 0x20, 0);
            tou_binary::store_u8(spawned, 0x26, 0);
            tou_binary::store_u8(spawned, 0x22, tou_binary::load_u8(entity, 0x22));
            tou_binary::store_i32(spawned, 0x28, 0);
            tou_binary::store_i32(spawned, 0x38,
                                    tou_binary::load_i32(DAT_00487abc, 0x90));
            tou_binary::store_i32(spawned, 0x44,
                                    tou_binary::load_i32(DAT_00487abc, 0xcc));
            tou_binary::store_i32(spawned, 0x48, 0);
            tou_binary::store_i32(spawned, 0x4c,
                                    tou_binary::load_i32(DAT_00487abc, 0xfc));
            tou_binary::store_u8(spawned, 0x54, 0);
            tou_binary::store_u8(spawned, 0x40, 2);
            tou_binary::store_u32(spawned, 0x34,
                                    tou_binary::load_u32(DAT_00487abc, 0x00));
            tou_binary::store_i32(spawned, 0x3c, 0);
            tou_binary::store_u8(spawned, 0x5c, 0);

            ++g_EntityCount;
            const unsigned int palette_index = subtype == 0u ? 8u : 15u;
            tou_binary::store_u32(spawned, 0x4c,
                static_cast<uint32_t>(static_cast<uint16_t *>(DAT_00487aa8)[palette_index]) + 30000u);
            tou_binary::store_i32(spawned, 0x44, subtype == 0u ? 0x4b000 : 0x32000);
            angle_offset += 0xaa;
        } while (angle_offset < 0x800);
    } else if (DAT_00481e8f != 1u) {
        return;
    }

    if (g_ParticleCount < PARTICLE_CAPACITY) {
        const int32_t x = tou_binary::load_i32(entity, 0x00);
        const int32_t y = tou_binary::load_i32(entity, 0x08);
        const int32_t cell = tou_binary::sar_i32(x, 0x16) +
                             tou_binary::sar_i32(y, 0x16) * DAT_004879f8;
        if ((static_cast<uint8_t *>(DAT_00487814)[cell] & 8u) != 0u) {
            ParticleRecord *particle = &g_ParticlePool[g_ParticleCount];
            particle->position_x = x;
            particle->position_y = y;
            particle->velocity_x = 0;
            particle->velocity_y = 0;
            particle->sprite_index = static_cast<uint8_t>((TOU_Rand() & 1) + 3);
            particle->frame_number = 0;
            particle->frame_timer = 0;
            particle->flags_13 = 1;
            particle->owner_or_flags_14 = 0xff;
            particle->color_index = 0;
            ++g_ParticleCount;
        }
    }
    DAT_00481e8f = 1;
}

uint8_t *spawn_type67(int32_t x, int32_t y, int32_t vx, int32_t vy)
{
    if (g_EntityCount >= ENTITY_ACTIVE_CAPACITY) return NULL;
    uint8_t *spawned = entity_at(g_EntityCount++);
    tou_binary::store_i32(spawned, 0x00, x);
    tou_binary::store_i32(spawned, 0x08, y);
    tou_binary::store_i32(spawned, 0x18, vx);
    tou_binary::store_i32(spawned, 0x1c, vy);
    tou_binary::store_i32(spawned, 0x04, x);
    tou_binary::store_i32(spawned, 0x0c, y);
    tou_binary::store_i32(spawned, 0x10, 0);
    tou_binary::store_i32(spawned, 0x14, 0);
    tou_binary::store_u8(spawned, 0x21, 0x67);
    tou_binary::store_u16(spawned, 0x24, 0);
    tou_binary::store_u8(spawned, 0x20, 0);
    tou_binary::store_u8(spawned, 0x26, 0xff);
    tou_binary::store_u8(spawned, 0x22, 0xff);
    tou_binary::store_i32(spawned, 0x28, 0);
    tou_binary::store_i32(spawned, 0x38, tou_binary::load_i32(DAT_00487abc, 0xd830));
    tou_binary::store_i32(spawned, 0x44, tou_binary::load_i32(DAT_00487abc, 0xd86c));
    tou_binary::store_i32(spawned, 0x48, 0);
    tou_binary::store_i32(spawned, 0x4c, tou_binary::load_i32(DAT_00487abc, 0xd89c));
    tou_binary::store_u8(spawned, 0x54, 0);
    tou_binary::store_u8(spawned, 0x40, 0);
    tou_binary::store_i32(spawned, 0x34, tou_binary::load_i32(DAT_00487abc, 0xd7a8));
    tou_binary::store_i32(spawned, 0x3c, 0);
    tou_binary::store_u8(spawned, 0x5c, 0);
    return spawned;
}

void finish_type67(uint8_t *spawned, uint8_t owner, uint8_t fade,
                   uint8_t palette_high, uint8_t palette_low)
{
    if (spawned == NULL) return;
    tou_binary::store_u8(spawned, 0x22, owner);
    tou_binary::store_u8(spawned, 0x5c, fade);
    tou_binary::store_u8(spawned, 0x65, palette_high);
    tou_binary::store_u8(spawned, 0x64, palette_low);
    if (DAT_00487aa8 != NULL) {
        tou_binary::store_u32(spawned, 0x4c,
            static_cast<uint32_t>(static_cast<uint16_t *>(DAT_00487aa8)[palette_high]) + 30000u);
    }
}

void spawn_flash(int32_t x, int32_t y, uint8_t sprite, uint8_t behavior,
                 uint8_t owner, uint8_t group)
{
    if (g_ParticleCount >= PARTICLE_CAPACITY) return;
    ParticleRecord *particle = &g_ParticlePool[g_ParticleCount++];
    particle->position_x = x;
    particle->position_y = y;
    particle->velocity_x = 0;
    particle->velocity_y = 0;
    particle->sprite_index = sprite;
    particle->frame_number = 0;
    particle->frame_timer = 0;
    particle->flags_13 = behavior;
    particle->owner_or_flags_14 = owner;
    particle->color_index = group;
}

bool coarse_effect_allowed(const uint8_t *entity)
{
    const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x00), 0x16);
    const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x08), 0x16);
    const int width = (static_cast<int>(DAT_004879f0) + 15) >> 4;
    const int height = (static_cast<int>(DAT_004879f4) + 15) >> 4;
    return x >= 0 && y >= 0 && x < width && y < height &&
           (static_cast<uint8_t *>(DAT_00487814)[x + y * DAT_004879f8] & 8u) != 0u;
}

uint8_t *spawn_config_entity(uint8_t type, uint8_t subtype, int32_t x, int32_t y,
                             int32_t vx, int32_t vy, uint8_t owner)
{
    if (g_EntityCount >= ENTITY_ACTIVE_CAPACITY || DAT_00487abc == NULL) return NULL;
    uint8_t *spawned = entity_at(g_EntityCount++);
    const unsigned int base = static_cast<unsigned int>(type) * 0x218u;
    tou_binary::store_i32(spawned, 0x00, x);
    tou_binary::store_i32(spawned, 0x08, y);
    tou_binary::store_i32(spawned, 0x04, x);
    tou_binary::store_i32(spawned, 0x0c, y);
    tou_binary::store_i32(spawned, 0x10, 0);
    tou_binary::store_i32(spawned, 0x14, 0);
    tou_binary::store_i32(spawned, 0x18, vx);
    tou_binary::store_i32(spawned, 0x1c, vy);
    tou_binary::store_u8(spawned, 0x20, 0);
    tou_binary::store_u8(spawned, 0x21, type);
    tou_binary::store_u8(spawned, 0x22, owner);
    tou_binary::store_u16(spawned, 0x24, 0);
    tou_binary::store_u8(spawned, 0x26, 0);
    tou_binary::store_i32(spawned, 0x28, 0);
    tou_binary::store_u32(spawned, 0x34, tou_binary::load_u32(DAT_00487abc, base));
    tou_binary::store_i32(spawned, 0x38,
        tou_binary::load_i32(DAT_00487abc, base + 0x88u + subtype * 4u));
    tou_binary::store_i32(spawned, 0x3c, 0);
    tou_binary::store_u8(spawned, 0x40, subtype);
    tou_binary::store_i32(spawned, 0x44,
        tou_binary::load_i32(DAT_00487abc, base + 0xc4u + subtype * 4u));
    tou_binary::store_i32(spawned, 0x48, 0);
    tou_binary::store_u32(spawned, 0x4c,
        tou_binary::load_u32(DAT_00487abc, base + 0xf4u + subtype * 4u));
    tou_binary::store_u8(spawned, 0x54, 0);
    tou_binary::store_u8(spawned, 0x5c, 0);
    return spawned;
}

void spawn_edge_particle(int32_t x, int32_t y, int32_t vx, int32_t vy,
                         uint8_t sprite, uint8_t owner)
{
    if (DAT_0048925c >= 0x5dc || DAT_00481f2c == NULL) return;
    uint8_t *particle = static_cast<uint8_t *>(DAT_00481f2c) + DAT_0048925c++ * 0x20;
    tou_binary::store_i32(particle, 0x00, x);
    tou_binary::store_i32(particle, 0x04, y);
    tou_binary::store_i32(particle, 0x08, vx);
    tou_binary::store_i32(particle, 0x0c, vy);
    tou_binary::store_u8(particle, 0x10, sprite);
    tou_binary::store_u8(particle, 0x11, 0);
    tou_binary::store_u16(particle, 0x12, 0);
    tou_binary::store_u8(particle, 0x14, owner);
    tou_binary::store_u8(particle, 0x15, 0);
}

uint8_t previous_effect_tile(uint8_t *entity)
{
    const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x04), 0x12);
    const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x0c), 0x12);
    if (x < 0 || y < 0 || x >= static_cast<int>(DAT_004879f0) ||
        y >= static_cast<int>(DAT_004879f4)) return 0;
    const uint8_t tile = tile_at(x, y);
    return tile_property(tile, 4) != 0u ? tile : 0u;
}

void spawn_dumbfire_impact_particle(uint8_t *entity)
{
    const int cx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x16);
    const int cy = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x16);
    const int coarse_width = (static_cast<int>(DAT_004879f0) + 15) >> 4;
    const int coarse_height = (static_cast<int>(DAT_004879f4) + 15) >> 4;
    if (cx < 0 || cy < 0 || cx >= coarse_width || cy >= coarse_height ||
        (static_cast<uint8_t *>(DAT_00487814)[cx + cy * DAT_004879f8] & 8u) == 0u ||
        g_ParticleCount >= PARTICLE_CAPACITY) return;

    const uint8_t subtype = tou_binary::load_u8(entity, 0x40);
    uint8_t sprite;
    if (subtype == 2u) sprite = static_cast<uint8_t>((game_rand() & 3) + 7);
    else if (subtype == 1u) sprite = static_cast<uint8_t>((game_rand() & 3) + 13);
    else sprite = static_cast<uint8_t>(game_rand() % 3 + 17);
    spawn_flash(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                sprite, 0, 0xff, 1);
}

void settle_bounded_projectile(uint8_t *entity);

void callback_morning_star_0043dbd0(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    const uint8_t mode = tou_binary::load_u8(entity, 0x40);
    if (mode == 0u) {
        int32_t vx8 = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x18), 8);
        int32_t vy8 = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x1c), 8);
        int32_t speed2 = vx8 * vx8 + vy8 * vy8;
        if (speed2 > 0x10000) {
            const double mag = sqrt(static_cast<double>(speed2));
            tou_binary::store_i32(entity, 0x18, static_cast<int32_t>(vx8 * 256.0 / mag) << 8);
            tou_binary::store_i32(entity, 0x1c, static_cast<int32_t>(vy8 * 256.0 / mag) << 8);
        }
        tou_binary::store_i32(entity, 0x00,
            tou_binary::add_wrap_i32(tou_binary::load_i32(entity, 0x00), tou_binary::load_i32(entity, 0x18)));
        tou_binary::store_i32(entity, 0x08,
            tou_binary::add_wrap_i32(tou_binary::load_i32(entity, 0x08), tou_binary::load_i32(entity, 0x1c)));
        const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x00), 0x12);
        const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x08), 0x12);
        if (x < 0 || y < 0 || x >= static_cast<int>(DAT_004879f0) || y >= static_cast<int>(DAT_004879f4)) {
            DAT_00481e8f = 1;
            return;
        }
    }

    int32_t radius = tou_binary::load_i32(entity, 0x2c);
    int32_t phase = (tou_binary::load_i32(entity, 0x3c) + (radius >> 5)) & 0x7ff;
    tou_binary::store_i32(entity, 0x3c, phase);
    int32_t age = tou_binary::load_i32(entity, 0x28);
    if (age > 10 && age < 400 && radius < 400) {
        radius += 14;
        tou_binary::store_i32(entity, 0x2c, radius);
    }
    if (age > 10 && age < 400) {
        uint8_t tick = static_cast<uint8_t>(tou_binary::load_u8(entity, 0x20) + 1u);
        tou_binary::store_u8(entity, 0x20, tick);
        if (tick > 2u) {
            tou_binary::store_u8(entity, 0x20, 0);
            int32_t *sc = static_cast<int32_t *>(DAT_00487ab0);
            const int pos_scale = mode == 0u ? 0x10e : 0x190;
            const int vel_scale = mode == 0u ? 0x19 : 0x30;
            for (int n = 0; n < 4 && g_ParticleCount < PARTICLE_CAPACITY; ++n) {
                int dir = (phase + (n + 1) * 0x200) & 0x7ff;
                ParticleRecord *particle = &g_ParticlePool[g_ParticleCount++];
                particle->position_x = tou_binary::load_i32(entity, 0x00) +
                                       (sc[dir] * pos_scale >> 6);
                particle->position_y = tou_binary::load_i32(entity, 0x08) +
                                       (sc[dir + 0x200] * pos_scale >> 6);
                particle->velocity_x = sc[dir] * vel_scale >> 6;
                particle->velocity_y = sc[dir + 0x200] * vel_scale >> 6;
                particle->sprite_index =
                    static_cast<uint8_t>((game_rand() & 1) - 2 * mode + 5);
                particle->frame_number = 4;
                particle->frame_timer = 2;
                particle->flags_13 = 0xc8;
                particle->owner_or_flags_14 = tou_binary::load_u8(entity, 0x22);
                particle->color_index = 0;
            }
        }
    }
    ++age;
    tou_binary::store_i32(entity, 0x28, age);
    if (age >= 400) tou_binary::store_i32(entity, 0x2c, tou_binary::load_i32(entity, 0x2c) - 8);
    if (age == 450) {
        if (coarse_effect_allowed(entity))
            spawn_flash(tou_binary::load_i32(entity, 0x00), tou_binary::load_i32(entity, 0x08),
                        static_cast<uint8_t>(7 + (game_rand() & 3)), 0, 0xff, 1);
        DAT_00481e8f = 1;
    }
}

void callback_gamma_boom_004457b0(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    int32_t vx8 = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x18), 8);
    int32_t vy8 = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x1c), 8);
    int32_t speed2 = vx8 * vx8 + vy8 * vy8;
    if (speed2 > 0x10000) {
        const double mag = sqrt(static_cast<double>(speed2));
        tou_binary::store_i32(entity, 0x18, static_cast<int32_t>(vx8 * 256.0 / mag) << 8);
        tou_binary::store_i32(entity, 0x1c, static_cast<int32_t>(vy8 * 256.0 / mag) << 8);
    }
    int32_t fuse = tou_binary::load_i32(entity, 0x3c) + 1;
    if (fuse >= 0x24) {
        fuse = 0;
        FUN_0040f9b0(0x11c, tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
    }
    tou_binary::store_i32(entity, 0x3c, fuse);
    tou_binary::store_i32(entity, 0x04, tou_binary::load_i32(entity, 0x00));
    tou_binary::store_i32(entity, 0x0c, tou_binary::load_i32(entity, 0x08));
    tou_binary::store_i32(entity, 0x00,
        tou_binary::add_wrap_i32(tou_binary::load_i32(entity, 0x00), tou_binary::load_i32(entity, 0x18)));
    tou_binary::store_i32(entity, 0x08,
        tou_binary::add_wrap_i32(tou_binary::load_i32(entity, 0x08), tou_binary::load_i32(entity, 0x1c)));
    const int tx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x00), 0x12);
    const int ty = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x08), 0x12);
    if (!map_point_valid(tx, ty)) { DAT_00481e8f = 1; return; }
    if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);
    if (collision_checks(entity_index, true)) goto explode;
    damage_special_tile(entity);
    if (tile_property(tile_at(tx, ty), 2) != 0u) return;

explode:
    {
        const int32_t x = tou_binary::load_i32(entity, 0x00);
        const int32_t y = tou_binary::load_i32(entity, 0x08);
        const uint8_t owner = tou_binary::load_u8(entity, 0x22);
        const uint8_t old_tile = previous_effect_tile(entity);
        FUN_004357b0(tx, ty, 9, old_tile, tile_at(tx, ty) == 0x0c,
                    x, y, tou_binary::load_i32(entity, 0x04), tou_binary::load_i32(entity, 0x0c), 0, 0, owner);
        if (coarse_effect_allowed(entity)) spawn_flash(x, y, 0x0b, 0, 0xff, 1);
        FUN_0040f9b0(game_rand() % 7 + 0x65, x, y);
        FUN_00437cf0(x, y, 400, owner, 0);
        DAT_00481e8f = 1;
    }
}

void callback_fireworks_launcher_child_004442f0(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    const uint8_t subtype = tou_binary::load_u8(entity, 0x40);
    const uint8_t owner = tou_binary::load_u8(entity, 0x22);
    bool terminal = tou_binary::load_u8(entity, 0x20) == 0xfau;
    int32_t life = tou_binary::load_i32(entity, 0x28);
    if (life != 0 && --life == 1) terminal = true;
    tou_binary::store_i32(entity, 0x28, life);
    tou_binary::store_i32(entity, 0x1c, tou_binary::load_i32(entity, 0x1c) +
        tou_binary::load_i32(entity, 0x38) * DAT_00483828);
    tou_binary::store_i32(entity, 0,
        tou_binary::load_i32(entity, 0) + tou_binary::load_i32(entity, 0x18));
    tou_binary::store_i32(entity, 8,
        tou_binary::load_i32(entity, 8) + tou_binary::load_i32(entity, 0x1c));
    int tx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    int ty = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    if (!map_point_valid(tx, ty)) terminal = true;
    if (!terminal && DAT_00489288 == 0) collision_prepass_00437b10(entity_index);
    if (!terminal && collision_checks(entity_index, true)) terminal = true;

    uint8_t fuel = tou_binary::load_u8(entity, 0x20);
    if (!terminal && fuel != 0u) {
        tou_binary::store_u8(entity, 0x20, static_cast<uint8_t>(fuel - 1u));
        int heading = tou_binary::load_i32(entity, 0x3c) & 0x7ff;
        int32_t *sc = static_cast<int32_t *>(DAT_00487ab0);
        if (life == 0) {
            tou_binary::store_i32(entity, 0x18,
                tou_binary::load_i32(entity, 0x18) + (sc[heading] * 3 >> 6));
            tou_binary::store_i32(entity, 0x1c,
                tou_binary::load_i32(entity, 0x1c) + (sc[heading + 0x200] * 3 >> 6));
        } else {
            tou_binary::store_i32(entity, 0x18,
                tou_binary::load_i32(entity, 0x18) + (sc[heading] >> 5));
            tou_binary::store_i32(entity, 0x1c,
                tou_binary::load_i32(entity, 0x1c) + (sc[heading + 0x200] >> 5));
        }
        int trail_frequency = static_cast<int>(tou_binary::x87_ftol(
            3.0L / static_cast<long double>(DAT_0048385c)));
        if (trail_frequency < 1) trail_frequency = 1;
        if (coarse_effect_allowed(entity) && game_rand() % trail_frequency == 0) {
            int dir = (game_rand() & 0x7f) + 0x3c0 + heading;
            dir &= 0x7ff;
            int speed = game_rand() % 40 + 20;
            uint8_t *trail = spawn_config_entity(0x67, 0,
                tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                sc[dir] * speed >> 6, sc[dir + 0x200] * speed >> 6, 0xff);
            if (trail != NULL) finish_type67(trail, 0xff, 4,
                static_cast<uint8_t>(game_rand() % 12 + 20), 0x12);
        }
    }

    if (!terminal && map_point_valid(tx, ty) && tile_property(tile_at(tx, ty), 2) != 0u) return;

    const int32_t x = tou_binary::load_i32(entity, 4);
    const int32_t y = tou_binary::load_i32(entity, 0x0c);
    int32_t *sc = static_cast<int32_t *>(DAT_00487ab0);
    if (subtype == 2u) {
        for (int angle = 0; angle < 0x800; angle += 0x40) {
            int dir = ((game_rand() & 0xff) + angle) & 0x7ff;
            int speed = game_rand() % 20 + 2;
            uint8_t *burst = spawn_config_entity(0x6a, 0, x, y,
                (sc[dir] * speed >> 6) + (tou_binary::load_i32(entity, 0x18) >> 1),
                (sc[dir + 0x200] * speed >> 6) + (tou_binary::load_i32(entity, 0x1c) >> 1), owner);
            if (burst == NULL) break;
            tou_binary::store_i32(burst, 0x28, 0x50);
            const uint8_t color_range = tou_binary::load_u8(DAT_00487abc, 0xdf14);
            if (color_range != 0u) tou_binary::store_i32(burst, 0x48,
                game_rand() % color_range);
        }
        FUN_0040f9b0(0x114, tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
    } else if (subtype == 3u && terminal) {
        int base = game_rand() & 0xff;
        for (int angle = 0; angle < 0x800; angle += 0x100) {
            int dir = (base + angle) & 0x7ff;
            uint8_t *burst = spawn_config_entity(8, 1, x, y,
                (sc[dir] * 0x50 >> 7) + (tou_binary::load_i32(entity, 0x18) >> 2),
                (sc[dir + 0x200] * 0x50 >> 7) + (tou_binary::load_i32(entity, 0x1c) >> 2), owner);
            if (burst == NULL) break;
            tou_binary::store_u8(burst, 0x26, 1);
            tou_binary::store_i32(burst, 0x28, game_rand() % 30 + 190);
            tou_binary::store_i32(burst, 0x60,
                ((game_rand() & 1) * 2 - 1) * (game_rand() % 40 + 90));
        }
        FUN_0040f9b0(0x112, tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
    }
    if (owner < static_cast<uint8_t>(DAT_00489240) && DAT_00487810 != 0) {
        PlayerData *player = Player_Get(owner);
        if (player->counter_464 > 0) {
            player->counter_464 = tou_binary::sub_wrap_i32(player->counter_464, 1);
        }
    }
    if (coarse_effect_allowed(entity)) spawn_flash(tou_binary::load_i32(entity, 0),
        tou_binary::load_i32(entity, 8), static_cast<uint8_t>(13 + (game_rand() & 3)), 0, 0xff, 0);
    DAT_00481e8f = 1;
}

void callback_jupiter_missile_0043a4b0(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    int32_t life = tou_binary::load_i32(entity, 0x28);
    if (life > 0 && --life == 1) { DAT_00481e8f = 1; return; }
    tou_binary::store_i32(entity, 0x28, life);
    tou_binary::store_i32(entity, 0x1c, tou_binary::load_i32(entity, 0x1c) +
        tou_binary::load_i32(entity, 0x38) * DAT_00483828);
    int32_t timer = tou_binary::load_i32(entity, 0x2c) - 1;
    if (timer < 1) {
        timer = game_rand() % 25 + 10;
        tou_binary::store_u8(entity, 0x20, static_cast<uint8_t>(game_rand() & 1));
    }
    tou_binary::store_i32(entity, 0x2c, timer);
    int heading = tou_binary::load_i32(entity, 0x3c);
    heading += tou_binary::load_u8(entity, 0x20) == 0u ? -4 : 4;
    int best = 2000000000, target = -1;
    const uint8_t owner = tou_binary::load_u8(entity, 0x22);
    const uint8_t owner_team = owner < static_cast<uint8_t>(DAT_00489240)
        ? Player_Get(owner)->team : 0xff;
    for (int p = 0; p < DAT_00489240; ++p) {
        PlayerData *player = Player_Get(p);
        if (player->team == owner_team || player->state_24 != 0u) continue;
        int dx = (tou_binary::load_i32(entity, 0) - player->position_x) >> 0x12;
        int dy = (tou_binary::load_i32(entity, 8) - player->position_y) >> 0x12;
        int d = dx * dx + dy * dy;
        if (d < best) { best = d; target = p; }
    }
    if (target >= 0) {
        PlayerData *player = Player_Get(target);
        int desired = static_cast<int>(FUN_004257e0(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                                                    player->position_x, player->position_y)) & 0x7ff;
        int delta = (desired - heading) & 0x7ff;
        if (delta != 0) heading += delta < 0x400 ? 3 : -3;
    }
    heading &= 0x7ff;
    tou_binary::store_i32(entity, 0x3c, heading);
    int32_t *sc = static_cast<int32_t *>(DAT_00487ab0);
    tou_binary::store_i32(entity, 0x04, tou_binary::load_i32(entity, 0));
    tou_binary::store_i32(entity, 0x0c, tou_binary::load_i32(entity, 8));
    tou_binary::store_i32(entity, 0, static_cast<int32_t>(tou_binary::x87_ftol(
        static_cast<long double>(tou_binary::load_i32(entity, 0)) +
        static_cast<long double>(sc[heading]) * 1.5L)));
    tou_binary::store_i32(entity, 8, static_cast<int32_t>(tou_binary::x87_ftol(
        static_cast<long double>(tou_binary::load_i32(entity, 8)) +
        static_cast<long double>(sc[heading + 0x200]) * 1.5L)));
    int tx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    int ty = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    if (!map_point_valid(tx, ty)) { DAT_00481e8f = 1; return; }
    int trail_frequency = static_cast<int>(tou_binary::x87_ftol(
        4.0L / static_cast<long double>(DAT_0048385c)));
    if (trail_frequency < 1) trail_frequency = 1;
    if (coarse_effect_allowed(entity) && game_rand() % trail_frequency == 0) {
        int trail_dir = ((game_rand() & 0x7f) - 0x40 + heading) & 0x7ff;
        int trail_speed = game_rand() % 20 + 10;
        uint8_t *trail = spawn_config_entity(0x67, 0,
            tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
            -(sc[trail_dir] * trail_speed >> 6),
            -(sc[trail_dir + 0x200] * trail_speed >> 6), 0xff);
        if (trail != NULL) {
            tou_binary::store_u8(trail, 0x20, 2);
            finish_type67(trail, 0xff, 2,
                static_cast<uint8_t>(25 + game_rand() % 5), 0x12);
        }
    }
    if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);
    if (collision_checks(entity_index, true) || tile_property(tile_at(tx, ty), 2) == 0u) DAT_00481e8f = 1;
}

void callback_roman_candle_00446130(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    tou_binary::store_i32(entity, 0x1c, tou_binary::load_i32(entity, 0x1c) +
        tou_binary::load_i32(entity, 0x38) * DAT_00483828);
    tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 0) + tou_binary::load_i32(entity, 0x18));
    tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 8) + tou_binary::load_i32(entity, 0x1c));
    settle_bounded_projectile(entity);
    int tx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    int ty = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);
    const uint8_t subtype = tou_binary::load_u8(entity, 0x40);
    const uint8_t owner = tou_binary::load_u8(entity, 0x22);
    int32_t *sc = static_cast<int32_t *>(DAT_00487ab0);
    const bool planted = tile_property(tile_at(tx, ty), 1) == 0u;
    int32_t count = tou_binary::load_i32(entity, 0x3c);
    if (planted) {
        ++count;
        tou_binary::store_i32(entity, 0x3c, count);
    }
    if (planted && subtype == 0u && count > 0x50) {
        tou_binary::store_i32(entity, 0x3c, 0);
        if (game_rand() % 15 < 14) {
            int dir = (game_rand() & 0xff) + 0x380;
            int speed = game_rand() % 90 + 25;
            int32_t x = tou_binary::load_i32(entity, 0x04) - FIXED_SCALE;
            int32_t y = tou_binary::load_i32(entity, 0x0c) - 0x340000;
            uint8_t *shot = spawn_config_entity(0x6a, 1, x, y, sc[dir] * speed >> 6, sc[dir + 0x200] * speed >> 6, owner);
            if (shot != NULL) {
                tou_binary::store_i32(shot, 0x28, game_rand() % 50 + 90);
                const uint8_t damage_range = tou_binary::load_u8(DAT_00487abc, 0xdf14);
                if (damage_range != 0u) tou_binary::store_i32(shot, 0x4c,
                    tou_binary::load_i32(shot, 0x4c) + game_rand() % damage_range);
            }
            const int yellow_count = static_cast<uint8_t>(tou_binary::x87_ftol(
                static_cast<long double>(game_rand() % 10 + 5) * DAT_0048385c));
            for (int n = 0; n < yellow_count; ++n) {
                int yellow_dir = (game_rand() & 0x7f) + 0x3c0;
                int yellow_speed = game_rand() % 50 + 20;
                uint8_t *yellow = spawn_config_entity(0x67, 0, x, y,
                    sc[yellow_dir] * yellow_speed >> 7,
                    sc[yellow_dir + 0x200] * yellow_speed >> 7, 0xff);
                if (yellow == NULL) break;
                finish_type67(yellow, 0xff, 6,
                    static_cast<uint8_t>(game_rand() % 12 + 20), 0x12);
            }
        }
        tou_binary::store_u8(entity, 0x20,
            static_cast<uint8_t>(tou_binary::load_u8(entity, 0x20) + 1u));
    } else if (planted && subtype == 1u && count > 0x19) {
        tou_binary::store_i32(entity, 0x3c, 0);
        uint8_t stage = tou_binary::load_u8(entity, 0x20);
        int32_t x = tou_binary::load_i32(entity, 0x04) + ((3 - (stage >> 2)) << 0x12);
        int32_t y = tou_binary::load_i32(entity, 0x0c) - 0x1c0000;
        uint8_t *missile = spawn_config_entity(0x6b, 0, x, y, 0, 0, owner);
        if (missile != NULL) {
            tou_binary::store_i32(missile, 0x28, game_rand() % 200 + 200);
            tou_binary::store_i32(missile, 0x3c, 0x400);
            tou_binary::store_i32(missile, 0x2c, 1);
        }
        tou_binary::store_u8(entity, 0x20, static_cast<uint8_t>(stage + 1u));
    } else if (planted && subtype == 2u && count > 200) {
        tou_binary::store_i32(entity, 0x3c, 0);
        const uint8_t stage = tou_binary::load_u8(entity, 0x20);
        int32_t x_offset;
        if (stage < 2u) x_offset = 0x140000;
        else if (stage < 4u) x_offset = 0x80000;
        else if (stage < 6u) x_offset = -0x80000;
        else x_offset = -0x100000;
        const uint8_t firework_subtype = stage == 7u ? 3u : 2u;
        const int32_t x = tou_binary::load_i32(entity, 0x04) + x_offset;
        const int32_t y = tou_binary::load_i32(entity, 0x0c) - 0x280000;
        uint8_t *firework = spawn_config_entity(0x22, firework_subtype, x, y, 0, 0, owner);
        if (firework != NULL) {
            tou_binary::store_u8(firework, 0x20, 200);
            tou_binary::store_u8(firework, 0x26, 0xff);
            tou_binary::store_i32(firework, 0x28, stage == 7u ? 0x96 : 0x73);
            tou_binary::store_i32(firework, 0x2c, 0);
            tou_binary::store_i32(firework, 0x3c, 0x400);
        }
        tou_binary::store_u8(entity, 0x20, static_cast<uint8_t>(stage + 1u));

        const int cx = tou_binary::sar_i32(x, 0x16);
        const int cy = tou_binary::sar_i32(y, 0x16);
        const int cw = (static_cast<int>(DAT_004879f0) + 15) >> 4;
        const int ch = (static_cast<int>(DAT_004879f4) + 15) >> 4;
        if (cx >= 0 && cy >= 0 && cx < cw && cy < ch &&
            (static_cast<uint8_t *>(DAT_00487814)[cx + cy * DAT_004879f8] & 8u) != 0u) {
            const int yellow_count = static_cast<uint8_t>(tou_binary::x87_ftol(
                static_cast<long double>(game_rand() % 25 + 20) * DAT_0048385c));
            for (int n = 0; n < yellow_count; ++n) {
                int dir = (game_rand() & 0xff) + 0x380;
                int speed = game_rand() % 20 + 10;
                uint8_t *yellow = spawn_config_entity(0x67, 0, x, y,
                    sc[dir] * speed >> 6, sc[dir + 0x200] * speed >> 6, 0xff);
                if (yellow == NULL) break;
                tou_binary::store_u16(yellow, 0x24,
                    static_cast<uint16_t>(game_rand() % 6));
                finish_type67(yellow, 0xff, 10,
                    static_cast<uint8_t>(game_rand() % 12 + 20), 0x12);
            }
        }

        const int smoke_y = tou_binary::load_i32(entity, 0x0c) - 0x2c0000;
        const int smoke_cx = tou_binary::sar_i32(x, 0x16);
        const int smoke_cy = tou_binary::sar_i32(smoke_y, 0x16);
        if (smoke_cx >= 0 && smoke_cy >= 0 && smoke_cx < cw && smoke_cy < ch &&
            (static_cast<uint8_t *>(DAT_00487814)[smoke_cx + smoke_cy * DAT_004879f8] & 8u) != 0u) {
            for (int n = 0; n < 7; ++n) {
                int dir = (game_rand() & 0x1ff) + 0x300;
                int vx = (game_rand() % 30 + 15) * sc[dir] >> 6;
                int vy = (game_rand() % 30 + 15) * sc[dir + 0x200] >> 6;
                int before = DAT_0048925c;
                spawn_edge_particle(x, smoke_y, vx, vy,
                    static_cast<uint8_t>(game_rand() & 1), 0xff);
                if (DAT_0048925c > before)
                    tou_binary::store_u8(static_cast<uint8_t *>(DAT_00481f2c) + before * 0x20, 0x11, 4);
            }
        }
    }

    if (planted) {
        tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 4));
        tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x0c));
        tou_binary::store_i32(entity, 0x18, 0);
        tou_binary::store_i32(entity, 0x1c, 0);
    }

    const uint8_t stage = tou_binary::load_u8(entity, 0x20);
    const bool spent = (subtype == 0u && stage > 0x14u) ||
                       (subtype == 1u && stage > 0x1eu) ||
                       (subtype == 2u && stage > 7u);
    if (spent || collision_checks(entity_index, false)) {
        if (coarse_effect_allowed(entity)) spawn_flash(tou_binary::load_i32(entity, 0),
            tou_binary::load_i32(entity, 8), 0x0e, 0, 0xff, 1);
        FUN_00437cf0(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                     100, owner, -1);
        DAT_00481e8f = 1;
    }
}

void callback_etna_00447a70(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    tou_binary::store_i32(entity, 0x1c, tou_binary::load_i32(entity, 0x1c) +
        tou_binary::load_i32(entity, 0x38) * DAT_00483828);
    tou_binary::store_i32(entity, 0x00,
        tou_binary::load_i32(entity, 0x00) + tou_binary::load_i32(entity, 0x18));
    tou_binary::store_i32(entity, 0x08,
        tou_binary::load_i32(entity, 0x08) + tou_binary::load_i32(entity, 0x1c));
    settle_bounded_projectile(entity);
    if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);

    int tx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    int ty = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    bool supported = ty >= static_cast<int>(DAT_004879f4) - 4;
    if (!supported && map_point_valid(tx, ty + 2)) {
        supported = tile_property(tile_at(tx, ty), 1) != 0u &&
                    tile_property(tile_at(tx, ty + 1), 1) != 0u &&
                    tile_property(tile_at(tx, ty + 2), 1) != 0u;
    }

    if (!supported) {
        int frequency = static_cast<int>(tou_binary::x87_ftol(
            1.0L / static_cast<long double>(DAT_00483854)));
        if (frequency < 1) frequency = 1;
        if (g_EntityCount < ENTITY_ACTIVE_CAPACITY && game_rand() % frequency == 0) {
            int dir = (game_rand() & 0xff) + 0x380;
            if (dir < 0x3f8 || dir > 0x408) {
                int speed = game_rand() % 60 + 20;
                uint8_t palette_low, palette_high;
                if ((game_rand() & 1) == 0) {
                    palette_low = 1;
                    palette_high = static_cast<uint8_t>(game_rand() % 12 + 1);
                } else {
                    palette_low = 0x12;
                    palette_high = static_cast<uint8_t>(game_rand() % 12 + 20);
                }
                uint8_t state;
                if ((game_rand() & 7) == 0) state = 2;
                else if (coarse_effect_allowed(entity) && DAT_0048385c > 0.3f) state = 0x0f;
                else state = 0xff;
                if (state != 0xff) {
                    int32_t *sc = static_cast<int32_t *>(DAT_00487ab0);
                    uint8_t *particle = spawn_config_entity(0x67, 0,
                        tou_binary::load_i32(entity, 0x04),
                        tou_binary::load_i32(entity, 0x0c) - 0x1c0000,
                        sc[dir] * speed >> 6, sc[dir + 0x200] * speed >> 6,
                        tou_binary::load_u8(entity, 0x22));
                    if (particle != NULL) {
                        tou_binary::store_u16(particle, 0x24,
                            static_cast<uint16_t>(game_rand() % 6));
                        tou_binary::store_u8(particle, 0x20, state);
                        tou_binary::store_u8(particle, 0x5c,
                            static_cast<uint8_t>(game_rand() % 12 + 11));
                        finish_type67(particle, tou_binary::load_u8(entity, 0x22),
                                      tou_binary::load_u8(particle, 0x5c),
                                      palette_high, palette_low);
                        tou_binary::store_i32(particle, 0x44, frequency * 0x28000);
                    }
                }
            }
        }

        const int32_t smoke_x = tou_binary::load_i32(entity, 0x04);
        const int32_t smoke_y = tou_binary::load_i32(entity, 0x0c) - 0x1c0000;
        const int cx = tou_binary::sar_i32(smoke_x, 0x16);
        const int cy = tou_binary::sar_i32(smoke_y, 0x16);
        const int cw = (static_cast<int>(DAT_004879f0) + 15) >> 4;
        const int ch = (static_cast<int>(DAT_004879f4) + 15) >> 4;
        if (cx >= 0 && cy >= 0 && cx < cw && cy < ch &&
            (static_cast<uint8_t *>(DAT_00487814)[cx + cy * DAT_004879f8] & 8u) != 0u) {
            int dir = (game_rand() & 0x1ff) + 0x300;
            int32_t *sc = static_cast<int32_t *>(DAT_00487ab0);
            int vx = (game_rand() % 30 + 15) * sc[dir] >> 6;
            int vy = (game_rand() % 30 + 15) * sc[dir + 0x200] >> 6;
            int before = DAT_0048925c;
            spawn_edge_particle(tou_binary::load_i32(entity, 0),
                                tou_binary::load_i32(entity, 8) - 0x1c0000,
                                vx, vy, static_cast<uint8_t>(game_rand() & 1), 0xff);
            if (DAT_0048925c > before)
                tou_binary::store_u8(static_cast<uint8_t *>(DAT_00481f2c) + before * 0x20, 0x11, 2);
        }
        tou_binary::store_i32(entity, 0x3c, tou_binary::load_i32(entity, 0x3c) + 1);
    }

    tx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    ty = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    if (tile_property(tile_at(tx, ty), 1) == 0u) {
        tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 4));
        tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x0c));
        tou_binary::store_i32(entity, 0x18, 0);
        tou_binary::store_i32(entity, 0x1c, 0);
    }

    if (tou_binary::load_i32(entity, 0x3c) > 1000 || collision_checks(entity_index, false)) {
        if (coarse_effect_allowed(entity)) spawn_flash(tou_binary::load_i32(entity, 0),
            tou_binary::load_i32(entity, 8), 0x0e, 0, 0xff, 1);
        FUN_00437cf0(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                     100, tou_binary::load_u8(entity, 0x22), -1);
        FUN_0040f9b0(game_rand() % 7 + 0x65,
                     tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
        DAT_00481e8f = 1;
    }
}

bool move_full_gravity(uint8_t *entity)
{
    tou_binary::store_i32(entity, 0x1c,
        tou_binary::load_i32(entity, 0x1c) + tou_binary::load_i32(entity, 0x38) * DAT_00483828);
    tou_binary::store_i32(entity, 0x00,
        tou_binary::load_i32(entity, 0x00) + tou_binary::load_i32(entity, 0x18));
    tou_binary::store_i32(entity, 0x08,
        tou_binary::load_i32(entity, 0x08) + tou_binary::load_i32(entity, 0x1c));
    int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    if (x < 0) { tou_binary::store_i32(entity, 0, 0); tou_binary::store_i32(entity, 4, 0); return false; }
    if (x >= static_cast<int>(DAT_004879f0)) {
        const int32_t edge = static_cast<int32_t>(DAT_004879f0) << 0x12;
        tou_binary::store_i32(entity, 0, edge); tou_binary::store_i32(entity, 4, edge); return false;
    }
    if (y < 0) { tou_binary::store_i32(entity, 8, 0); tou_binary::store_i32(entity, 0x0c, 0); return false; }
    if (y >= static_cast<int>(DAT_004879f4)) {
        const int32_t edge = static_cast<int32_t>(DAT_004879f4) << 0x12;
        tou_binary::store_i32(entity, 8, edge); tou_binary::store_i32(entity, 0x0c, edge); return false;
    }
    return true;
}

bool projectile_collision_pass(int entity_index, bool tracked)
{
    uint8_t *entity = entity_at(entity_index);
    if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);
    uint8_t cooldown = tou_binary::load_u8(entity, 0x26);
    if (cooldown == 0xffu) return false;
    if (cooldown != 0u && cooldown < 0xfeu) tou_binary::store_u8(entity, 0x26, cooldown - 1u);
    if (collision_checks(entity_index, tracked)) return true;
    damage_special_tile(entity);
    return false;
}

void settle_bounded_projectile(uint8_t *entity)
{
    int32_t x = tou_binary::load_i32(entity, 0);
    int32_t y = tou_binary::load_i32(entity, 8);
    if (x >= 0 && y >= 0 && (x >> 0x12) < static_cast<int>(DAT_004879f0) &&
        (y >> 0x12) < static_cast<int>(DAT_004879f4)) return;
    x = tou_binary::load_i32(entity, 4);
    y = tou_binary::load_i32(entity, 0x0c);
    tou_binary::store_i32(entity, 0x18, 0);
    tou_binary::store_i32(entity, 0x1c, 0);
    if (x < 0) x = 0x200000;
    if (y < 0) y = 0x200000;
    const int32_t max_x = (static_cast<int32_t>(DAT_004879f0) - 8) << 0x12;
    const int32_t max_y = (static_cast<int32_t>(DAT_004879f4) - 8) << 0x12;
    if (x >= (static_cast<int32_t>(DAT_004879f0) << 0x12)) x = max_x;
    if (y >= (static_cast<int32_t>(DAT_004879f4) << 0x12)) y = max_y;
    tou_binary::store_i32(entity, 0, x);
    tou_binary::store_i32(entity, 8, y);
}

void spawn_radial_entity_debris(uint8_t *source, uint8_t subtype, int count,
                                int total_speed, int random_speed, int shift,
                                int vertical_bias, bool moving_sucker)
{
    if (count <= 0) return;
    const int life = total_speed / count;
    for (int i = 0; i < count && g_EntityCount < ENTITY_ACTIVE_CAPACITY; ++i) {
        const int angle = game_rand() & 0x7ff;
        const int speed = game_rand() % random_speed;
        const int32_t vx = tou_binary::sar_i32(tou_binary::mul_wrap_i32(
            static_cast<int32_t *>(DAT_00487ab0)[angle], speed), shift) +
            tou_binary::load_i32(source, 0x18);
        const int32_t vy = tou_binary::sar_i32(tou_binary::mul_wrap_i32(
            static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200], speed), shift) +
            tou_binary::load_i32(source, 0x1c) + vertical_bias;
        uint8_t *spawned = spawn_config_entity(0, subtype,
            tou_binary::load_i32(source, 0), tou_binary::load_i32(source, 8),
            vx, vy, moving_sucker ? 0xffu : tou_binary::load_u8(source, 0x22));
        if (spawned == NULL) break;
        tou_binary::store_u16(spawned, 0x24,
            static_cast<uint16_t>(moving_sucker ? 0 : game_rand() % 6));
        tou_binary::store_i32(spawned, 0x28,
            moving_sucker ? game_rand() % 60 + 80 : game_rand() % 100 + 90);
        tou_binary::store_i32(spawned, 0x3c, life);
        if (DAT_00487aa8 != NULL) {
            const int palette = moving_sucker ? game_rand() % 10 + 0xf6
                                              : game_rand() % 10 + 0xf6;
            tou_binary::store_u32(spawned, 0x4c,
                static_cast<uint32_t>(static_cast<uint16_t *>(DAT_00487aa8)[palette]) + 30000u);
        }
    }
}

void callback_nuclear_barrel_00431650(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    tou_binary::store_i32(entity, 0x1c, tou_binary::load_i32(entity, 0x1c) +
        tou_binary::load_i32(entity, 0x38) * DAT_00483828);
    tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 0) +
        tou_binary::load_i32(entity, 0x18));
    tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 8) +
        tou_binary::load_i32(entity, 0x1c));
    settle_bounded_projectile(entity);
    uint8_t guard = tou_binary::load_u8(entity, 0x5c);
    if (guard != 0u) tou_binary::store_u8(entity, 0x5c, guard - 1u);
    if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);

    const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    if (tile_property(tile_at(x, y), 1) == 0u) {
        tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 4));
        tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x0c));
        tou_binary::store_i32(entity, 0x18, -(tou_binary::load_i32(entity, 0x18) >> 3));
        tou_binary::store_i32(entity, 0x1c, tou_binary::load_i32(entity, 0x1c) >> 3);
    }

    bool detonate = tou_binary::load_u8(entity, 0x20) == 0xfau;
    if (!detonate) {
        if (projectile_collision_pass(entity_index, true)) detonate = true;
        if (!detonate) return;
    }
    const uint8_t subtype = tou_binary::load_u8(entity, 0x40);
    const int count = static_cast<int>(tou_binary::x87_ftol(
        (subtype == 1u ? 30.0L : 40.0L) * DAT_00483854)) + 1;
    spawn_radial_entity_debris(entity, subtype == 1u ? 3u : 4u, count,
        subtype == 1u ? 0x1482000 : 0x7148000, 70, 6, -0x57800, false);
    if (coarse_effect_allowed(entity)) {
        const int smoke_count = static_cast<int>(tou_binary::x87_ftol(16.0L * DAT_0048385c));
        for (int i = 0; i < smoke_count && g_EntityCount < ENTITY_ACTIVE_CAPACITY; ++i) {
            const int angle = game_rand() & 0x7ff;
            const int speed = game_rand() % 70;
            uint8_t *spawned = spawn_config_entity(0x64, 0,
                tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                tou_binary::sar_i32(tou_binary::mul_wrap_i32(
                    static_cast<int32_t *>(DAT_00487ab0)[angle], speed), 6) + tou_binary::load_i32(entity, 0x18),
                tou_binary::sar_i32(tou_binary::mul_wrap_i32(
                    static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200], speed), 6) - 0x57800 +
                    tou_binary::load_i32(entity, 0x1c), 0xff);
            if (spawned == NULL) break;
            tou_binary::store_u16(spawned, 0x24, static_cast<uint16_t>(game_rand() % 6));
            tou_binary::store_i32(spawned, 0x28, game_rand() % 100 + 90);
            if (DAT_00487aa8 != NULL) tou_binary::store_u32(spawned, 0x4c,
                static_cast<uint32_t>(static_cast<uint16_t *>(DAT_00487aa8)[game_rand() % 10 + 0xf6]) + 30000u);
        }
    }
    if (subtype == 1u && DAT_00487810 != 0) {
        uint8_t owner = tou_binary::load_u8(entity, 0x22);
        if (owner < static_cast<unsigned int>(DAT_00489240)) {
            PlayerData *player = Player_Get(owner);
            player->counter_470 = tou_binary::sub_wrap_i32(player->counter_470, 1);
        }
    }
    if (coarse_effect_allowed(entity)) {
        int32_t effect_y = tou_binary::load_i32(entity, 8);
        if (DAT_00481f20 != NULL) {
            const int sprite_height = static_cast<uint8_t *>(DAT_00481f20)[5] & 0xfe;
            effect_y += 0x140000 - sprite_height * 0x20000;
        }
        spawn_flash(tou_binary::load_i32(entity, 0), effect_y, 0, 1, 0xff, 1);
    }
    FUN_00437cf0(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                 400, tou_binary::load_u8(entity, 0x22), -1);
    FUN_0040f9b0(game_rand() % 7 + 0x65,
                 tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
    DAT_00481e8f = 1;
}

void callback_moving_sucker_00430dc0(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    const uint8_t subtype = tou_binary::load_u8(entity, 0x40);
    if (subtype == 0u) {
        int sx = tou_binary::load_i32(entity, 0x18) >> 8;
        int sy = tou_binary::load_i32(entity, 0x1c) >> 8;
        const int speed_sq = sx * sx + sy * sy;
        if (speed_sq > 0x10000) {
            const long double magnitude = sqrtl(static_cast<long double>(speed_sq));
            tou_binary::store_i32(entity, 0x18,
                static_cast<int32_t>(tou_binary::x87_ftol(sx * 256.0L / magnitude)) << 8);
            tou_binary::store_i32(entity, 0x1c,
                static_cast<int32_t>(tou_binary::x87_ftol(sy * 256.0L / magnitude)) << 8);
        }
        tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 0) +
            tou_binary::load_i32(entity, 0x18));
        tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 8) +
            tou_binary::load_i32(entity, 0x1c));
        if (!map_point_valid(tou_binary::load_i32(entity, 0) >> 0x12,
                             tou_binary::load_i32(entity, 8) >> 0x12)) {
            DAT_00481e8f = 1;
            return;
        }
        if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);
        const int x = tou_binary::load_i32(entity, 0) >> 0x12;
        const int y = tou_binary::load_i32(entity, 8) >> 0x12;
        if (tile_property(tile_at(x, y), 1) == 0u) tou_binary::store_u8(entity, 0x20, 0xfa);
    } else if ((game_rand() & 0x7f) == 0 && coarse_effect_allowed(entity) && DAT_0048385c > 0.1f) {
        for (int angle = 0; angle < 0x800 && g_EntityCount < ENTITY_ACTIVE_CAPACITY; angle += 42) {
            uint8_t *trail = spawn_type67(tou_binary::load_i32(entity, 0),
                tou_binary::load_i32(entity, 8),
                static_cast<int32_t *>(DAT_00487ab0)[angle] >> 1,
                static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200] >> 1);
            finish_type67(trail, tou_binary::load_u8(entity, 0x22), 1, 0x3e, 0x32);
        }
    }

    uint8_t guard = tou_binary::load_u8(entity, 0x5c);
    if (guard != 0u) tou_binary::store_u8(entity, 0x5c, guard - 1u);
    int32_t fuse = tou_binary::load_i32(entity, 0x60);
    if (fuse > 0) tou_binary::store_i32(entity, 0x60, --fuse);
    bool remove = fuse <= 0 || tou_binary::load_u8(entity, 0x20) == 0xfau || DAT_00481e8f == 1u;
    if (!remove) return;
    if (tou_binary::load_u8(entity, 0x20) == 0xfau) {
        const int count = 1 + static_cast<int>(tou_binary::x87_ftol(20.0L * DAT_00483854));
        spawn_radial_entity_debris(entity, 3, count, 0x177000, 35, 8, 0, true);
    }
    if (coarse_effect_allowed(entity)) spawn_flash(tou_binary::load_i32(entity, 0),
        tou_binary::load_i32(entity, 8), static_cast<uint8_t>((game_rand() & 1) + 1),
        1, 0xff, 2);
    DAT_00481e8f = 1;
}

bool bone_crusher_scan_infantry(uint8_t *entity)
{
    if (DAT_00487aa4 == NULL || g_TrooperPool == NULL || DAT_00487810 == 0) return false;
    const uint8_t owner = tou_binary::load_u8(entity, 0x22);
    const uint8_t owner_team = owner < static_cast<unsigned int>(DAT_00489240)
        ? Player_Get(owner)->team
        : 0xffu;
    const int32_t x = tou_binary::load_i32(entity, 0);
    const int32_t y = tou_binary::load_i32(entity, 8);
    uint8_t *grid = static_cast<uint8_t *>(DAT_00487aa4);
    for (int team = 0; team < 4; ++team) {
        if (team == owner_team) continue;
        const int base = team * 0x4000;
        const int count = tou_binary::load_i32(grid, base + 8);
        for (int slot = 0; slot < count; ++slot) {
            const int trooper_index = tou_binary::load_i32(grid, base + 0x0c + slot * 4);
            if (trooper_index < 0 || trooper_index >= g_TrooperCount) continue;
            TrooperRecord *trooper = &g_TrooperPool[trooper_index];
            if (trooper->health_28 <= 0) continue;
            const int32_t tx = trooper->position_x;
            const int32_t ty = trooper->position_y;
            if (!(tx - 0x180000 < x && x < tx + 0x180000 &&
                  ty - 0x100000 < y && y < ty + 0x80000)) continue;
            tou_binary::store_i32(entity, 0x60, tou_binary::load_i32(entity, 0x60) - 1000);
            tou_binary::store_i32(entity, 0x3c,
                static_cast<int32_t>(tou_binary::x87_ftol(60.0L * DAT_0048385c)));
            tou_binary::store_i32(entity, 0x30, game_rand() % 30 + 10);
            tou_binary::store_i32(entity, 0x2c, game_rand() % 30 + 10);
            trooper->health_28 = -1000000;
            return true;
        }
    }
    return false;
}

void callback_bone_crusher_004330c0(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    int32_t fuse = tou_binary::load_i32(entity, 0x60);
    if (static_cast<int8_t>(tou_binary::load_u8(entity, 0x20)) == -1 || fuse <= 0) {
        fuse = 0;
    } else {
        tou_binary::store_i32(entity, 0x60, --fuse);
        tou_binary::store_i32(entity, 0x1c, tou_binary::load_i32(entity, 0x1c) +
            tou_binary::load_i32(entity, 0x38) * DAT_00483828);
        tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 0) +
            tou_binary::load_i32(entity, 0x18));
        tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 8) +
            tou_binary::load_i32(entity, 0x1c));
        settle_bounded_projectile(entity);
        uint8_t guard = tou_binary::load_u8(entity, 0x5c);
        if (guard != 0u) tou_binary::store_u8(entity, 0x5c, guard - 1u);
        if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);
    }

    bool exploding = fuse <= 0 || tou_binary::load_u8(entity, 0x20) == 0xfau;
    if (!exploding) {
        uint8_t cadence = static_cast<uint8_t>(tou_binary::load_u8(entity, 0x64) + 1u);
        tou_binary::store_u8(entity, 0x64, cadence);
        if (cadence >= 4u) {
            tou_binary::store_u8(entity, 0x64, 0);
            bone_crusher_scan_infantry(entity);
        }
        int smoke = tou_binary::load_i32(entity, 0x3c);
        if (smoke > 0 && g_EntityCount < ENTITY_ACTIVE_CAPACITY) {
            tou_binary::store_i32(entity, 0x3c, smoke - 1);
            const int angle = (game_rand() & 0x1ff) + 0x300;
            const int sx = game_rand() % 20 + 10;
            const int sy = game_rand() % 20 + 10;
            uint8_t *debris = spawn_config_entity(2, 0, tou_binary::load_i32(entity, 0),
                tou_binary::load_i32(entity, 8) - 0x240000,
                tou_binary::sar_i32(tou_binary::mul_wrap_i32(
                    static_cast<int32_t *>(DAT_00487ab0)[angle], sx), 6),
                tou_binary::sar_i32(tou_binary::mul_wrap_i32(
                    static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200], sy), 6), 0xff);
            if (debris != NULL) {
                tou_binary::store_u8(debris, 0x20, 5);
                tou_binary::store_i32(debris, 0x28, game_rand() % 80 + 100);
                if (DAT_00487aa8 != NULL) tou_binary::store_u32(debris, 0x4c,
                    static_cast<uint32_t>(static_cast<uint16_t *>(DAT_00487aa8)[game_rand() % 6 + 0x22]) + 30000u);
            }
        }
        const int x = tou_binary::load_i32(entity, 0) >> 0x12;
        const int y = tou_binary::load_i32(entity, 8) >> 0x12;
        if (tile_property(tile_at(x, y), 1) == 0u) {
            tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 4));
            tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x0c));
            tou_binary::store_i32(entity, 0x18, 0);
            tou_binary::store_i32(entity, 0x1c, 0);
            tou_binary::store_i32(entity, 0x38, 0);
        }
        if (projectile_collision_pass(entity_index, true)) exploding = true;
        if (!exploding) return;
    }

    if (tou_binary::load_u8(entity, 0x20) == 0xfau) {
        const int count = 1 + static_cast<int>(tou_binary::x87_ftol(20.0L * DAT_00483854));
        spawn_radial_entity_debris(entity, 0, count, 0x2ee000, 35, 6, 0, false);
    }
    if (coarse_effect_allowed(entity)) spawn_flash(tou_binary::load_i32(entity, 0),
        tou_binary::load_i32(entity, 8), static_cast<uint8_t>(game_rand() % 3 + 0x11),
        1, 0xff, 1);
    FUN_00437cf0(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                 200, tou_binary::load_u8(entity, 0x22), 100);
    FUN_0040f9b0(game_rand() % 7 + 0x65,
                 tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
    DAT_00481e8f = 1;
}

void callback_airstrike_00438010(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    int32_t life = tou_binary::load_i32(entity, 0x28);
    if (life != 0) {
        --life;
        tou_binary::store_i32(entity, 0x28, life);
        if (life == 1) { DAT_00481e8f = 1; return; }
    }

    bool impact = static_cast<int8_t>(tou_binary::load_u8(entity, 0x20)) == -1;
    if (!impact) {
        impact = !move_full_gravity(entity);
        if (!impact) impact = projectile_collision_pass(entity_index, true);
        if (!impact) {
            const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
            const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
            if (tile_property(tile_at(x, y), 2) != 0u) return;
            impact = true;
        }
    }
    if (!impact) return;

    const int32_t px = tou_binary::load_i32(entity, 0x04);
    const int32_t py = tou_binary::load_i32(entity, 0x0c);
    const int32_t vx = tou_binary::load_i32(entity, 0x18);
    const int32_t vy = tou_binary::load_i32(entity, 0x1c);
    const uint8_t owner = tou_binary::load_u8(entity, 0x22);
    for (int angle_bytes = 0; angle_bytes < 0x2000 && g_ParticleCount < PARTICLE_CAPACITY;
         angle_bytes += 100) {
        const int angle = angle_bytes >> 2;
        ParticleRecord *particle = &g_ParticlePool[g_ParticleCount++];
        particle->position_x = px;
        particle->position_y = py;
        particle->velocity_x = tou_binary::sar_i32(tou_binary::mul_wrap_i32(
            game_rand() % 50, static_cast<int32_t *>(DAT_00487ab0)[angle]), 7) -
            tou_binary::sar_i32(vx, 2);
        particle->velocity_y = tou_binary::sar_i32(tou_binary::mul_wrap_i32(
            game_rand() % 50, static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200]), 7) -
            tou_binary::sar_i32(vy, 2);
        particle->sprite_index = static_cast<uint8_t>((game_rand() & 1) + 1);
        particle->frame_number = static_cast<uint8_t>(game_rand() % 6 + 1);
        particle->frame_timer = 2;
        particle->flags_13 = 200;
        particle->owner_or_flags_14 = owner;
        particle->color_index = 0;
    }
    FUN_0040f9b0(0x11, px, py);
    if (coarse_effect_allowed(entity)) {
        spawn_flash(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                    static_cast<uint8_t>((game_rand() & 1) + 1), 0, 0xff, 0);
    }
    DAT_00481e8f = 1;
}

void fireball_primary_trail(uint8_t *entity, uint8_t subtype)
{
    if (!coarse_effect_allowed(entity)) return;
    uint8_t sprite;
    if (subtype == 0u || subtype == 3u) sprite = static_cast<uint8_t>((game_rand() & 1) + 3);
    else if (subtype == 1u || subtype == 4u) sprite = static_cast<uint8_t>((game_rand() & 1) + 1);
    else sprite = static_cast<uint8_t>(game_rand() % 3 + 0x11);
    const uint8_t frame = subtype < 3u ? 4u : (subtype < 5u ? 7u : 10u);
    if (g_ParticleCount < PARTICLE_CAPACITY) {
        ParticleRecord *particle = &g_ParticlePool[g_ParticleCount++];
        particle->position_x = tou_binary::load_i32(entity, 0);
        particle->position_y = tou_binary::load_i32(entity, 8);
        particle->velocity_x = -tou_binary::load_i32(entity, 0x18) >> 1;
        particle->velocity_y = -tou_binary::load_i32(entity, 0x1c) >> 1;
        particle->sprite_index = sprite;
        particle->frame_number = frame;
        particle->frame_timer = 0;
        particle->flags_13 = 0;
        particle->owner_or_flags_14 = tou_binary::load_u8(entity, 0x22);
        particle->color_index = 0;
    }
}

void fireball_edge_trail(uint8_t *entity, uint8_t subtype)
{
    const int angle = (game_rand() & 0x1ff) + 0x300;
    uint8_t sprite;
    if (subtype == 0u) sprite = static_cast<uint8_t>(game_rand() & 1);
    else if (subtype == 1u) sprite = static_cast<uint8_t>((game_rand() & 1) + 2);
    else sprite = static_cast<uint8_t>(game_rand() % 3 + 4);
    const int speed_x = game_rand() % 60 + 20;
    const int speed_y = game_rand() % 60 + 20;
    spawn_edge_particle(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
        tou_binary::sar_i32(tou_binary::mul_wrap_i32(
            static_cast<int32_t *>(DAT_00487ab0)[angle], speed_x), 6),
        tou_binary::sar_i32(tou_binary::mul_wrap_i32(
            static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200], speed_y), 6),
        sprite, 0xff);
}

void callback_normal_fireball_00441aa0(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    if (static_cast<int8_t>(tou_binary::load_u8(entity, 0x20)) == -1 ||
        !move_full_gravity(entity)) { DAT_00481e8f = 1; return; }
    if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);

    if (tou_binary::load_u8(entity, 0x20) == 0x1bu && DAT_00487810 != 0) {
        int best = 0xf424;
        int best_player = -1;
        const uint8_t owner = tou_binary::load_u8(entity, 0x22);
        const uint8_t owner_team = owner < static_cast<unsigned int>(DAT_00489240)
            ? Player_Get(owner)->team
            : 0xffu;
        for (int i = 0; i < DAT_00489240; ++i) {
            PlayerData *player = Player_Get(i);
            if (player->health <= 0 || player->team == owner_team) continue;
            const int dx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0) -
                                                  player->position_x, 0x12);
            const int dy = tou_binary::sar_i32(tou_binary::load_i32(entity, 8) -
                                                  player->position_y, 0x12);
            const int distance = dx * dx + dy * dy;
            if (distance < best) { best = distance; best_player = i; }
        }
        if (best_player >= 0) {
            PlayerData *player = Player_Get(best_player);
            const int angle = FUN_004257e0(tou_binary::load_i32(entity, 0),
                tou_binary::load_i32(entity, 8), player->position_x,
                player->position_y) & 0x7ff;
            int32_t vx = tou_binary::load_i32(entity, 0x18) +
                         (static_cast<int32_t *>(DAT_00487ab0)[angle] >> 5);
            int32_t vy = tou_binary::load_i32(entity, 0x1c) +
                         (static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200] >> 5);
            const int sx = vx >> 8, sy = vy >> 8;
            const int speed_sq = sx * sx + sy * sy;
            if (speed_sq > 0xb06440) {
                const long double magnitude = sqrtl(static_cast<long double>(speed_sq));
                vx = static_cast<int32_t>(tou_binary::x87_ftol(sx * 3400.0L / magnitude)) << 8;
                vy = static_cast<int32_t>(tou_binary::x87_ftol(sy * 3400.0L / magnitude)) << 8;
            }
            tou_binary::store_i32(entity, 0x18, vx);
            tou_binary::store_i32(entity, 0x1c, vy);
        }
    }

    uint8_t subtype = tou_binary::load_u8(entity, 0x40);
    uint8_t cadence = static_cast<uint8_t>(tou_binary::load_u8(entity, 0x5c) + 1u);
    tou_binary::store_u8(entity, 0x5c, cadence);
    bool emit = subtype <= 2u
        ? ((cadence >= 2u && tou_binary::load_i32(entity, 0x3c) == 0) ||
           (cadence >= 1u && tou_binary::load_i32(entity, 0x3c) == 1))
        : cadence >= 4u;
    if (emit) {
        tou_binary::store_u8(entity, 0x5c, 0);
        fireball_primary_trail(entity, subtype);
        if (subtype <= 2u) fireball_edge_trail(entity, subtype);
    }

    const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    if (subtype <= 2u && tile_property(tile_at(x, y), 4) == 1u) {
        subtype = static_cast<uint8_t>(subtype + 3u);
        tou_binary::store_u8(entity, 0x40, subtype);
        tou_binary::store_u32(entity, 0x4c,
            tou_binary::load_u32(DAT_00487abc, 0x248cu + subtype * 4u));
    }
    const bool collided = collision_checks(entity_index, true);
    if (!collided) {
        damage_special_tile(entity);
        if (tile_property(tile_at(x, y), 2) != 0u) return;

        const uint8_t previous = previous_effect_tile(entity);
        if (map_point_valid(x, y)) {
            FUN_004357b0(x, y, game_rand() % 3 + 10, previous, tile_at(x, y) == 0x0cu,
                tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                tou_binary::load_i32(entity, 4), tou_binary::load_i32(entity, 0x0c),
                0, 0, tou_binary::load_u8(entity, 0x22));
        }
    }

    const int burst_count = subtype == 0u ? 16 : (subtype == 1u ? 32 : (subtype == 2u ? 48 : 0));
    int angle = 0;
    for (int i = 0; i < burst_count && g_ParticleCount < PARTICLE_CAPACITY; ++i) {
        angle = (angle + 0x800 / burst_count) & 0x7ff;
        const int sx = game_rand() & 0x3f;
        const int sy = game_rand() & 0x3f;
        ParticleRecord *particle = &g_ParticlePool[g_ParticleCount++];
        particle->position_x = tou_binary::load_i32(entity, 4);
        particle->position_y = tou_binary::load_i32(entity, 0x0c);
        particle->velocity_x = tou_binary::sar_i32(
            tou_binary::mul_wrap_i32(sx, static_cast<int32_t *>(DAT_00487ab0)[angle]), 6);
        particle->velocity_y = tou_binary::sar_i32(
            tou_binary::mul_wrap_i32(sy, static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200]), 6);
        particle->sprite_index = static_cast<uint8_t>((game_rand() & 3) + 1);
        particle->frame_number = static_cast<uint8_t>(game_rand() % 6 + 1);
        particle->frame_timer = 2;
        particle->flags_13 = 200;
        particle->owner_or_flags_14 = tou_binary::load_u8(entity, 0x22);
        particle->color_index = 0;
    }
    FUN_0040f9b0(0x11, tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
    if (g_ConfigBlob[0x17d0] != 0u && DAT_004892d8 < 0x78 && DAT_00487aa0 != NULL) {
        uint8_t *emitter = static_cast<uint8_t *>(DAT_00487aa0) + DAT_004892d8 * 0x10;
        tou_binary::store_i32(emitter, 0,
            tou_binary::sar_i32(tou_binary::load_i32(entity, 4), 0x12));
        tou_binary::store_i32(emitter, 4,
            tou_binary::sar_i32(tou_binary::load_i32(entity, 0x0c), 0x12));
        tou_binary::store_i32(emitter, 8, 0x12c);
        tou_binary::store_u8(emitter, 0x0c, 3);
        tou_binary::store_u8(emitter, 0x0d, static_cast<uint8_t>((game_rand() & 1) + 2));
        tou_binary::store_u8(emitter, 0x0e, 2);
        ++DAT_004892d8;
    }
    if (DAT_00481e8f != 4u) FUN_00437cf0(tou_binary::load_i32(entity, 0),
        tou_binary::load_i32(entity, 8), (subtype + 1) * 100,
        tou_binary::load_u8(entity, 0x22), -1);
    DAT_00481e8f = 1;
}

void callback_shotgun_rapidfire_00438010(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    int32_t life = tou_binary::load_i32(entity, 0x28);
    if (life != 0) {
        --life;
        tou_binary::store_i32(entity, 0x28, life);
        if (life == 1) { DAT_00481e8f = 1; return; }
    }
    if (static_cast<int8_t>(tou_binary::load_u8(entity, 0x20)) == -1) {
        DAT_00481e8f = 1;
        return;
    }
    if (!move_full_gravity(entity)) { DAT_00481e8f = 1; return; }
    if (projectile_collision_pass(entity_index, true)) { DAT_00481e8f = 1; return; }
    const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    const uint8_t current = tile_at(x, y);
    if (tile_property(current, 2) != 0u) return;
    const uint8_t subtype = tou_binary::load_u8(entity, 0x40);
    if (map_point_valid(x, y)) {
        /* Original 0x004389d4-0x00438a1a passes param_11=1 only for the
         * type-0/subtype-2 debris spawned by an explosive terrain tile.
         * FUN_004357b0 uses it to replace level 2 with level 20..22, clearing
         * the armed neighbourhood before the chain can multiply again. */
        const uint8_t randomize_plastic_crater =
            (subtype == 2u && tou_binary::load_u8(entity, 0x20) == 0x32u) ? 1u : 0u;
        FUN_004357b0(x, y, subtype, previous_effect_tile(entity), current == 0x0cu,
            tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
            tou_binary::load_i32(entity, 4), tou_binary::load_i32(entity, 0x0c),
            0, randomize_plastic_crater, tou_binary::load_u8(entity, 0x22));
    }
    uint8_t sprite = 0;
    if (subtype == 3u) sprite = static_cast<uint8_t>((game_rand() & 1) + 5);
    else if (subtype == 4u) sprite = static_cast<uint8_t>((game_rand() & 3) + 3);
    else if (subtype == 5u) sprite = static_cast<uint8_t>((game_rand() & 1) + 3);
    if (subtype >= 3u &&
        (static_cast<uint8_t *>(DAT_00487814)[
            tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x16) +
            tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x16) * DAT_004879f8] & 8u) != 0u) {
        spawn_flash(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                    sprite, 0, 0xff, 0);
    }
    DAT_00481e8f = 1;
}

void callback_dumbfire_00438d90(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    bool remove = static_cast<int8_t>(tou_binary::load_u8(entity, 0x20)) == -1;
    if (!remove) {
        remove = !move_full_gravity(entity);
        if (!remove) remove = projectile_collision_pass(entity_index, true);
    }
    if (!remove) {
        const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
        const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
        if (tile_property(tile_at(x, y), 2) == 0u) {
            int32_t bounces = tou_binary::load_i32(entity, 0x3c);
            if (bounces > 1) {
                tou_binary::store_i32(entity, 0x3c, bounces - 1);
                const int px = tou_binary::sar_i32(tou_binary::load_i32(entity, 4), 0x12);
                const int py = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x0c), 0x12);
                if (tile_property(tile_at(x, py), 2) == 1u) {
                    tou_binary::store_i32(entity, 0x1c, -tou_binary::load_i32(entity, 0x1c) >> 1);
                    tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x0c));
                } else if (tile_property(tile_at(px, y), 2) == 1u) {
                    tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 4));
                    tou_binary::store_i32(entity, 0x18, -tou_binary::load_i32(entity, 0x18) >> 1);
                } else {
                    tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 4));
                    tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x0c));
                    tou_binary::store_i32(entity, 0x18, -tou_binary::load_i32(entity, 0x18) >> 1);
                    tou_binary::store_i32(entity, 0x1c, -tou_binary::load_i32(entity, 0x1c) >> 1);
                }
                return;
            }
            const uint8_t subtype = tou_binary::load_u8(entity, 0x40);
            const int level = game_rand() % 3 - static_cast<int>(subtype) * 3 + 0x13;
            if (map_point_valid(x, y)) {
                FUN_004357b0(x, y, level, previous_effect_tile(entity), tile_at(x, y) == 0x0cu,
                    tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                    tou_binary::load_i32(entity, 4), tou_binary::load_i32(entity, 0x0c),
                    1, 0, tou_binary::load_u8(entity, 0x22));
            }
            remove = true;
        }
    }
    if (!remove) return;
    const uint8_t subtype = tou_binary::load_u8(entity, 0x40);
    spawn_dumbfire_impact_particle(entity);
    const int radius = subtype == 0u ? 50 : 300;
    const int force = subtype == 1u ? 0x578 : -1;
    if (DAT_00481e8f != 4u) {
        FUN_00437cf0(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                     radius, tou_binary::load_u8(entity, 0x22), force);
    }
    FUN_0040f9b0(game_rand() % 7 + 0x65,
                 tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
    DAT_00481e8f = 1;
}

void callback_collapser_00439880(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    if (static_cast<int8_t>(tou_binary::load_u8(entity, 0x20)) == -1 ||
        !move_full_gravity(entity) || projectile_collision_pass(entity_index, true)) {
        DAT_00481e8f = 1; return;
    }
    const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    const uint8_t current = tile_at(x, y);
    if (tile_property(current, 2) != 0u) return;
    if (map_point_valid(x, y)) {
        FUN_004357b0(x, y, 0x17, previous_effect_tile(entity), current == 0x0cu,
            tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
            tou_binary::load_i32(entity, 4), tou_binary::load_i32(entity, 0x0c),
            2, 0, tou_binary::load_u8(entity, 0x22));
    }
    FUN_0040f9b0(game_rand() % 7 + 0x65,
                 tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
    FUN_00437cf0(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                 200, tou_binary::load_u8(entity, 0x22), 800);
    DAT_00481e8f = 1;
}

void callback_tournailler_kicker_0043cc20(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    const uint8_t type = tou_binary::load_u8(entity, 0x21);
    const uint8_t subtype = tou_binary::load_u8(entity, 0x40);
    if (static_cast<int8_t>(tou_binary::load_u8(entity, 0x20)) == -1) {
        DAT_00481e8f = 1; return;
    }
    bool in_bounds = true;
    bool collision_checked = false;
    if (type == 9u) {
        /* 0x0043cc46-0x0043d8b3: Kicker intentionally advances in
         * quarter-steps, then loops through as many as five substeps.  The
         * loop is what makes its net motion faster than one normal step and
         * stretches the trail across the whole travelled segment. */
        for (int substep = 0; substep < 5; ++substep) {
            const int32_t vy = tou_binary::load_i32(entity, 0x1c) +
                (tou_binary::load_i32(entity, 0x38) * DAT_00483828 >> 2);
            tou_binary::store_i32(entity, 0x1c, vy);
            tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 0) +
                (tou_binary::load_i32(entity, 0x18) >> 2));
            tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 8) + (vy >> 2));
            const int32_t fixed_x = tou_binary::load_i32(entity, 0);
            const int32_t fixed_y = tou_binary::load_i32(entity, 8);
            in_bounds = fixed_x >= 0 && fixed_y >= 0 &&
                tou_binary::sar_i32(fixed_x, 0x12) < static_cast<int>(DAT_004879f0) &&
                tou_binary::sar_i32(fixed_y, 0x12) < static_cast<int>(DAT_004879f4);
            if (!in_bounds) break;

            const int px = tou_binary::sar_i32(tou_binary::load_i32(entity, 4), 0x16);
            const int py = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x0c), 0x16);
            const int coarse_width = (static_cast<int>(DAT_004879f0) + 15) >> 4;
            const int coarse_height = (static_cast<int>(DAT_004879f4) + 15) >> 4;
            if (px >= 0 && py >= 0 && px < coarse_width && py < coarse_height &&
                (static_cast<uint8_t *>(DAT_00487814)[px + py * DAT_004879f8] & 8u) != 0u) {
                int divisor = static_cast<int>(tou_binary::x87_ftol(
                    1.0L / static_cast<long double>(DAT_0048385c)));
                if (divisor < 1) divisor = 1;
                if (game_rand() % divisor == 0) {
                    uint8_t *trail = spawn_type67(tou_binary::load_i32(entity, 4),
                        tou_binary::load_i32(entity, 0x0c),
                        tou_binary::load_i32(entity, 0x18) >> 6,
                        tou_binary::load_i32(entity, 0x1c) >> 6);
                    finish_type67(trail, tou_binary::load_u8(entity, 0x22), 1, 0x9f, 0x93);
                }
            }

            collision_checked = true;
            if (projectile_collision_pass(entity_index, true)) {
                in_bounds = false;
                break;
            }
            const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
            const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
            if (tile_property(tile_at(x, y), 2) == 0u) break;
            if (substep == 4) return;

            tou_binary::store_i32(entity, 4, tou_binary::load_i32(entity, 0));
            tou_binary::store_i32(entity, 0x0c, tou_binary::load_i32(entity, 8));
            collision_checked = false;
        }
    } else {
        tou_binary::store_i32(entity, 0x1c,
            tou_binary::load_i32(entity, 0x1c) + tou_binary::load_i32(entity, 0x38) * DAT_00483828);
        if (tou_binary::load_u8(entity, 0x26) == (subtype == 1u ? 1u : 0x12u)) {
            tou_binary::store_i32(entity, 0x2c, tou_binary::load_i32(entity, 0));
            tou_binary::store_i32(entity, 0x30, tou_binary::load_i32(entity, 8));
        }
        if (subtype == 1u) {
            int32_t life = tou_binary::load_i32(entity, 0x28);
            if (life < 1) { DAT_00481e8f = 1; return; }
            tou_binary::store_i32(entity, 0x28, life - 1);
            uint8_t radius = static_cast<uint8_t>(tou_binary::load_u8(entity, 0x20) + 10u);
            if (radius > 30u) radius = 30u;
            tou_binary::store_u8(entity, 0x20, radius);
            tou_binary::store_i32(entity, 0x3c,
                (tou_binary::load_i32(entity, 0x3c) + tou_binary::load_i32(entity, 0x60)) & 0x7ff);
        } else {
            uint8_t delay = tou_binary::load_u8(entity, 0x5c);
            if (delay < 11u) tou_binary::store_u8(entity, 0x5c, delay + 1u);
            else {
                uint8_t radius = static_cast<uint8_t>(tou_binary::load_u8(entity, 0x20) + 2u);
                if (radius > 150u) radius = 150u;
                tou_binary::store_u8(entity, 0x20, radius);
            }
            tou_binary::store_i32(entity, 0x3c, (tou_binary::load_i32(entity, 0x3c) + 0x40) & 0x7ff);
        }
        const uint32_t angle = tou_binary::load_u32(entity, 0x3c) & 0x7ffu;
        const int32_t radius = tou_binary::load_u8(entity, 0x20);
        tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x30) +
            (static_cast<int32_t *>(DAT_00487ab0)[angle] * radius >> 3));
        tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 0x2c) +
            (static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200u] * radius >> 3));
        tou_binary::store_i32(entity, 0x2c,
            tou_binary::load_i32(entity, 0x2c) + tou_binary::load_i32(entity, 0x18));
        tou_binary::store_i32(entity, 0x30,
            tou_binary::load_i32(entity, 0x30) + tou_binary::load_i32(entity, 0x1c));
        const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
        const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
        in_bounds = map_point_valid(x, y);
    }
    if (!in_bounds || (!collision_checked && projectile_collision_pass(entity_index, true))) {
        tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 4));
        tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x0c));
        spawn_flash(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                    0x0c, 0, 0xff, 2);
        FUN_0040f9b0(0x1e, tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
        DAT_00481e8f = 1; return;
    }
    const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    if (tile_property(tile_at(x, y), 2) != 0u) return;
    if (type == 8u && subtype == 1u) { DAT_00481e8f = 1; return; }
    uint8_t previous = previous_effect_tile(entity);
    tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 4));
    tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x0c));
    const int32_t impact_x = tou_binary::load_i32(entity, 0);
    const int32_t impact_y = tou_binary::load_i32(entity, 8);
    uint32_t angle = static_cast<uint32_t>(FUN_004257e0(0, 0,
        tou_binary::load_i32(entity, 0x18), tou_binary::load_i32(entity, 0x1c))) & 0x7ffu;
    uint8_t strength = 0x20;
    for (int step = 0; step < 15; ++step) {
        const int tx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
        const int ty = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
        if (!map_point_valid(tx, ty)) break;
        FUN_004357b0(tx, ty, static_cast<uint8_t>(--strength) >> 3, previous,
                     tile_at(tx, ty) == 0x0cu, 0, 0, 0, 0, 0, 0,
                     tou_binary::load_u8(entity, 0x22));
        tou_binary::store_i32(entity, 0,
            tou_binary::load_i32(entity, 0) + static_cast<int32_t *>(DAT_00487ab0)[angle]);
        tou_binary::store_i32(entity, 8,
            tou_binary::load_i32(entity, 8) + static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200u]);
        if (type == 8u) {
            const int turn = game_rand() & 3;
            if (turn == 0) angle = (angle + 0x100u) & 0x7ffu;
            else if (turn == 1) angle = (angle - 0x100u) & 0x7ffu;
        }
    }
    tou_binary::store_i32(entity, 0, impact_x);
    tou_binary::store_i32(entity, 8, impact_y);
    spawn_flash(tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8),
                0x0c, 0, 0xff, 2);
    FUN_0040f9b0(0x1e, tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
    DAT_00481e8f = 1;
}

void spawn_machinegun_trace(uint8_t *projectile, int heading)
{
    const int32_t x = tou_binary::load_i32(projectile, 0x00);
    const int32_t y = tou_binary::load_i32(projectile, 0x08);
    const int32_t pvx = tou_binary::load_i32(projectile, 0x18);
    const int32_t pvy = tou_binary::load_i32(projectile, 0x1c);
    /* 0x00440176-0x00440193: x87 computes 20.0 / effect-density,
     * truncates through 0x00464488, then clamps the RNG divisor to one. */
    int divisor = static_cast<int>(tou_binary::x87_ftol(
        20.0L / static_cast<long double>(DAT_0048385c)));
    if (divisor < 1) divisor = 1;
    if (game_rand() % divisor != 0) return;
    int count = game_rand() % 3 + 1;
    while (count-- > 0 && g_EntityCount < ENTITY_ACTIVE_CAPACITY) {
        uint32_t angle = static_cast<uint32_t>(game_rand()) & 0x7ffu;
        const int speed = game_rand() % 10;
        int32_t vx = (static_cast<int32_t *>(DAT_00487ab0)[angle] * speed >> 7) + (pvx >> 2);
        int32_t vy = (static_cast<int32_t *>(DAT_00487ab0)[angle + 0x200u] * speed >> 7) + (pvy >> 2);
        uint8_t *trail = spawn_type67(x, y, vx, vy);
        if (trail == NULL) break;
        tou_binary::store_u8(trail, 0x5c, 1);
        const uint8_t palette_index = static_cast<uint8_t>(game_rand() % 12 + 0x14);
        tou_binary::store_u8(trail, 0x65, palette_index);
        tou_binary::store_u8(trail, 0x64, 0x12);
        tou_binary::store_u32(trail, 0x4c,
            static_cast<uint32_t>(static_cast<uint16_t *>(DAT_00487aa8)[palette_index]) + 30000u);
    }
    (void)heading;
}

void machinegun_impact(uint8_t *entity)
{
    uint8_t previous_tile = tile_at(
        tou_binary::sar_i32(tou_binary::load_i32(entity, 0x04), 0x12),
        tou_binary::sar_i32(tou_binary::load_i32(entity, 0x0c), 0x12));
    if (tile_property(previous_tile, 4) == 0u) previous_tile = 0;
    const int tx = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x00), 0x12);
    const int ty = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x08), 0x12);
    if (map_point_valid(tx, ty)) {
        FUN_004357b0(tx, ty, 2, previous_tile, tile_at(tx, ty) == 0x0f,
            tou_binary::load_i32(entity, 0x00), tou_binary::load_i32(entity, 0x08),
            tou_binary::load_i32(entity, 0x04), tou_binary::load_i32(entity, 0x0c),
            1, 0, tou_binary::load_u8(entity, 0x22));
    }
    DAT_00481e8f = 1;
}

void callback_machinegun_0043f990(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    const uint32_t heading = tou_binary::load_u32(entity, 0x2c) & 0x7ffu;
    tou_binary::store_i32(entity, 0x18,
        tou_binary::shl_wrap_i32(static_cast<int32_t *>(DAT_00487ab0)[heading], 1));
    tou_binary::store_i32(entity, 0x1c,
        tou_binary::shl_wrap_i32(static_cast<int32_t *>(DAT_00487ab0)[heading + 0x200u], 1));

    for (;;) {
        const int32_t old_x = tou_binary::load_i32(entity, 0x00);
        const int32_t old_y = tou_binary::load_i32(entity, 0x08);
        tou_binary::store_i32(entity, 0x04, old_x);
        tou_binary::store_i32(entity, 0x0c, old_y);
        const int32_t x = tou_binary::add_wrap_i32(old_x, tou_binary::load_i32(entity, 0x18));
        const int32_t y = tou_binary::add_wrap_i32(old_y, tou_binary::load_i32(entity, 0x1c));
        tou_binary::store_i32(entity, 0x00, x);
        tou_binary::store_i32(entity, 0x08, y);
        if (x < 0 || y < 0 || tou_binary::sar_i32(x, 0x12) >= static_cast<int>(DAT_004879f0) ||
            tou_binary::sar_i32(y, 0x12) >= static_cast<int>(DAT_004879f4)) {
            if (x < 0) tou_binary::store_i32(entity, 0x00, 0);
            else if (tou_binary::sar_i32(x, 0x12) >= static_cast<int>(DAT_004879f0))
                tou_binary::store_i32(entity, 0x00, static_cast<int32_t>(DAT_004879f0) << 0x12);
            if (y < 0) tou_binary::store_i32(entity, 0x08, 0);
            else if (tou_binary::sar_i32(y, 0x12) >= static_cast<int>(DAT_004879f4))
                tou_binary::store_i32(entity, 0x08, static_cast<int32_t>(DAT_004879f4) << 0x12);
            machinegun_impact(entity);
            return;
        }
        if (collision_checks(entity_index, true)) { machinegun_impact(entity); return; }
        damage_special_tile(entity);
        const int tx = tou_binary::sar_i32(x, 0x12);
        const int ty = tou_binary::sar_i32(y, 0x12);
        if (tile_property(tile_at(tx, ty), 2) == 0u) { machinegun_impact(entity); return; }
        const int cx = tou_binary::sar_i32(x, 0x16);
        const int cy = tou_binary::sar_i32(y, 0x16);
        if ((static_cast<uint8_t *>(DAT_00487814)[cx + cy * DAT_004879f8] & 8u) != 0u)
            spawn_machinegun_trace(entity, static_cast<int>(heading));
    }
}

uint32_t rgb555_luma(uint16_t color)
{
    return static_cast<uint8_t>((color >> 10) << 3) +
           static_cast<uint8_t>((color >> 5) << 3) +
           static_cast<uint8_t>(color << 3);
}

int blend_channel_256(int destination, int source, int alpha)
{
    const int delta = (source - destination) * alpha;
    return destination + tou_binary::sar_i32(delta + (delta < 0 ? 0xff : 0), 8);
}

void paint_plastic_explosive(uint8_t *entity)
{
    const int variant = game_rand() % 3;
    const int mask_sprite = 0x194 + variant;
    const int width = static_cast<uint8_t *>(DAT_00489e8c)[mask_sprite];
    const int height = static_cast<uint8_t *>(DAT_00489e88)[mask_sprite];
    const int start_x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12) - width / 2;
    const int start_y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12) - height / 2;
    int mask_offset = static_cast<int *>(DAT_00489234)[mask_sprite];
    const int texture_width = static_cast<uint8_t *>(DAT_00489e8c)[0x2f];
    const int texture_height = static_cast<uint8_t *>(DAT_00489e88)[0x2f];
    const int texture_offset = static_cast<int *>(DAT_00489234)[0x2f];
    if (width <= 0 || height <= 0 || texture_width <= 0 || texture_height <= 0 ||
        DAT_00489e94 == NULL || DAT_00487ab4 == NULL || DAT_00481f50 == NULL) return;

    uint8_t *tiles = static_cast<uint8_t *>(DAT_0048782c);
    uint16_t *framebuffer = static_cast<uint16_t *>(DAT_00481f50);
    const uint8_t *mask = static_cast<uint8_t *>(DAT_00489e94);
    const uint16_t *texture = static_cast<uint16_t *>(DAT_00487ab4);
    for (int row = 0; row < height; ++row) {
        const int y = start_y + row;
        for (int column = 0; column < width; ++column, ++mask_offset) {
            const int x = start_x + column;
            if (x <= 0 || y <= 0 || x >= static_cast<int>(DAT_004879f0) ||
                y >= static_cast<int>(DAT_004879f4)) continue;
            const int offset = tile_offset(x, y);
            uint8_t &tile = tiles[offset];
            const uint8_t *properties = static_cast<uint8_t *>(DAT_00487928) + tile * 0x20u;
            if (properties[0] != 0u || properties[4] != 0u || properties[0x0b] != 0u ||
                tile == 7u || tile == 10u || tile == 0x10u || properties[0x18] != 0u) continue;

            const int alpha = mask[mask_offset];
            const uint16_t source = texture[texture_offset +
                (y % texture_height) * texture_width + x % texture_width];
            uint16_t &destination = framebuffer[offset];
            if (alpha < 0xf0) {
                if (alpha == 0) continue;
                const int dr = ((destination >> 11) & 0x1f) << 3;
                const int dg = ((destination >> 5) & 0x3f) << 2;
                const int db = (destination & 0x1f) << 3;
                const int sr = ((source >> 11) & 0x1f) << 3;
                const int sg = ((source >> 5) & 0x3f) << 2;
                const int sb = (source & 0x1f) << 3;
                const int r = blend_channel_256(dr, sr, alpha);
                const int g = blend_channel_256(dg, sg, alpha);
                const int b = blend_channel_256(db, sb, alpha);
                destination = static_cast<uint16_t>(((r & 0xf8) << 8) |
                    ((g & 0xfc) << 3) | (b >> 3));
            } else {
                destination = source;
                tile = properties[0x0e] == 0x40u ? 0x12u : 7u;
            }
        }
    }
}

void paint_organic_waste_ii(uint8_t *entity)
{
    const int sprite = game_rand() % 3;
    const unsigned int frame = 0x42u + static_cast<unsigned int>(sprite);
    const int width = static_cast<uint8_t *>(DAT_00489e8c)[frame];
    const int height = static_cast<uint8_t *>(DAT_00489e88)[frame];
    const int start_x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x00), 0x12) - width / 2;
    const int start_y = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x08), 0x12) - height / 2;
    const int pixel_offset = static_cast<int *>(DAT_00489234)[0x42];
    const uint16_t *source = static_cast<uint16_t *>(DAT_00487ab4) + pixel_offset + sprite * 0;
    /* Each frame has its own offset at table bytes 0x108..0x110. */
    source = static_cast<uint16_t *>(DAT_00487ab4) +
             *reinterpret_cast<int *>(static_cast<uint8_t *>(DAT_00489234) + 0x108 + sprite * 4);
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            const int x = start_x + column;
            const int y = start_y + row;
            const uint16_t color = source[row * width + column];
            if (!map_point_valid(x, y) || color == 0u) continue;
            const int offset = tile_offset(x, y);
            uint8_t &tile = static_cast<uint8_t *>(DAT_0048782c)[offset];
            if (tile_property(tile, 0) != 1u && tile != 0x15u) continue;
            uint16_t &destination = static_cast<uint16_t *>(DAT_00481f50)[offset];
            if (tile != 0x15u) { tile = 0x15u; destination = color; }
            else if (rgb555_luma(destination) < rgb555_luma(color)) destination = color;
        }
    }
}

void callback_organic_waste_ii_0043a4b0(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    int32_t life = tou_binary::load_i32(entity, 0x28);
    if (life != 0) {
        --life;
        tou_binary::store_i32(entity, 0x28, life);
        if (life == 1) { DAT_00481e8f = 1; FUN_0040f9b0(0x10c, tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8)); return; }
    }
    if (tou_binary::load_u8(entity, 0x20) == 0xffu) {
        DAT_00481e8f = 1; FUN_0040f9b0(0x10c, tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8)); return;
    }
    tou_binary::store_i32(entity, 0x1c,
        tou_binary::load_i32(entity, 0x1c) + tou_binary::load_i32(entity, 0x38) * DAT_00483828);
    tou_binary::store_i32(entity, 0x00,
        tou_binary::load_i32(entity, 0x00) + tou_binary::load_i32(entity, 0x18));
    tou_binary::store_i32(entity, 0x08,
        tou_binary::load_i32(entity, 0x08) + tou_binary::load_i32(entity, 0x1c));
    const int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    const int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    if (!map_point_valid(x, y)) { DAT_00481e8f = 1; FUN_0040f9b0(0x10c, tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8)); return; }
    if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);
    uint8_t cooldown = tou_binary::load_u8(entity, 0x26);
    if (cooldown != 0xffu) {
        if (cooldown != 0u && cooldown < 0xfeu) tou_binary::store_u8(entity, 0x26, cooldown - 1u);
        if (collision_checks(entity_index, true)) { DAT_00481e8f = 1; FUN_0040f9b0(0x10c, tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8)); return; }
        damage_special_tile(entity);
    }
    if (tile_property(tile_at(x, y), 2) != 0u) return;
    if (tou_binary::load_u8(entity, 0x40) == 0u) paint_plastic_explosive(entity);
    else paint_organic_waste_ii(entity);
    FUN_0040f9b0(0x10c, tou_binary::load_i32(entity, 0), tou_binary::load_i32(entity, 8));
    DAT_00481e8f = 1;
}

void smooth_organic_patch(int center_offset)
{
    uint16_t *pixels = static_cast<uint16_t *>(DAT_00481f50);
    uint8_t *tiles = static_cast<uint8_t *>(DAT_0048782c);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const int offset = center_offset + row * DAT_00487a00 + column;
            const uint8_t *properties = static_cast<uint8_t *>(DAT_00487928) + tiles[offset] * 0x20u;
            if (properties[0x0b] != 0u || properties[4] != 0u || properties[0] != 0u) continue;
            const uint16_t center = rgb565_to_rgb555(pixels[offset]);
            uint16_t left = rgb565_to_rgb555(pixels[offset - 1]); if (left == 0u) left = center;
            uint16_t right = rgb565_to_rgb555(pixels[offset + 1]); if (right == 0u) right = center;
            uint16_t above = rgb565_to_rgb555(pixels[offset - DAT_00487a00]); if (above == 0u) above = center;
            uint16_t below = rgb565_to_rgb555(pixels[offset + DAT_00487a00]); if (below == 0u) below = center;
            const unsigned int r = (((left >> 10) & 0x1f) + ((right >> 10) & 0x1f) +
                                    ((above >> 10) & 0x1f) + ((below >> 10) & 0x1f) +
                                    ((center >> 10) & 0x1f) * 4u) >> 3;
            const unsigned int g = (((left >> 5) & 0x1f) + ((right >> 5) & 0x1f) +
                                    ((above >> 5) & 0x1f) + ((below >> 5) & 0x1f) +
                                    ((center >> 5) & 0x1f) * 4u) >> 3;
            const unsigned int b = ((left & 0x1f) + (right & 0x1f) + (above & 0x1f) +
                                    (below & 0x1f) + (center & 0x1f) * 4u) >> 3;
            pixels[offset] = rgb555_to_rgb565(
                static_cast<uint16_t>((r << 10) | (g << 5) | b));
        }
    }
}

void organic_tail(uint8_t *entity)
{
    uint8_t guard = tou_binary::load_u8(entity, 0x5c);
    if (guard == 0u) { DAT_00481e8f = 1; return; }
    tou_binary::store_u16(entity, 0x24, static_cast<uint16_t>(game_rand() % 6));
    const unsigned int palette_index = 0xa0u + static_cast<unsigned int>(game_rand() % 16);
    tou_binary::store_u32(entity, 0x4c,
        static_cast<uint32_t>(static_cast<uint16_t *>(DAT_00487aa8)[palette_index]) + 30000u);
    tou_binary::store_u8(entity, 0x5c, guard - 1u);
}

void callback_organic_waste_004427e0(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    int32_t life = tou_binary::load_i32(entity, 0x28);
    if (life > 0) {
        --life;
        tou_binary::store_i32(entity, 0x28, life);
        if (life == 1) { tou_binary::store_u8(entity, 0x5c, 0); organic_tail(entity); return; }
    }
    if (static_cast<int8_t>(tou_binary::load_u8(entity, 0x20)) == -1) {
        organic_tail(entity); return;
    }

    tou_binary::store_i32(entity, 0x1c,
        tou_binary::load_i32(entity, 0x1c) + tou_binary::load_i32(entity, 0x38) * DAT_00483828);
    tou_binary::store_i32(entity, 0x00,
        tou_binary::load_i32(entity, 0x00) + tou_binary::load_i32(entity, 0x18));
    tou_binary::store_i32(entity, 0x08,
        tou_binary::load_i32(entity, 0x08) + tou_binary::load_i32(entity, 0x1c));
    int x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    int y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    if (!map_point_valid(x, y)) {
        if (x <= 0) { tou_binary::store_i32(entity, 0, 0); tou_binary::store_i32(entity, 4, 0); }
        else if (x >= static_cast<int>(DAT_004879f0)) {
            const int32_t edge = static_cast<int32_t>(DAT_004879f0) << 0x12;
            tou_binary::store_i32(entity, 0, edge); tou_binary::store_i32(entity, 4, edge);
        }
        if (y <= 0) { tou_binary::store_i32(entity, 8, 0); tou_binary::store_i32(entity, 0x0c, 0); }
        else if (y >= static_cast<int>(DAT_004879f4)) {
            const int32_t edge = static_cast<int32_t>(DAT_004879f4) << 0x12;
            tou_binary::store_i32(entity, 8, edge); tou_binary::store_i32(entity, 0x0c, edge);
        }
        organic_tail(entity); return;
    }
    if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);

    uint8_t cooldown = tou_binary::load_u8(entity, 0x26);
    if (cooldown != 0xffu) {
        if (cooldown != 0u && cooldown < 0xfeu) tou_binary::store_u8(entity, 0x26, cooldown - 1u);
        if (collision_checks(entity_index, false)) {
            tou_binary::store_u8(entity, 0x5c, 0); organic_tail(entity); return;
        }
        damage_special_tile(entity);
    }

    x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    uint8_t current_tile = tile_at(x, y);
    if (current_tile >= 0xf0u) { tou_binary::store_u8(entity, 0x5c, 0); organic_tail(entity); return; }
    if (tile_property(current_tile, 2) != 0u) return;

    const int32_t vx = tou_binary::load_i32(entity, 0x18);
    const int32_t vy = tou_binary::load_i32(entity, 0x1c);
    const int32_t speed_x = tou_binary::sar_i32(vx, 9);
    const int32_t speed_y = tou_binary::sar_i32(vy, 9);
    const int32_t speed_squared = speed_x * speed_x + speed_y * speed_y;
    if (speed_squared > 1000) {
        const int previous_x = tou_binary::sar_i32(tou_binary::load_i32(entity, 4), 0x12);
        const int previous_y = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x0c), 0x12);
        if (tile_property(tile_at(x, previous_y), 2) == 1u) {
            tou_binary::store_i32(entity, 0x1c, -(vy / (game_rand() % 8 + 6)));
            tou_binary::store_i32(entity, 0x18, vx / (game_rand() % 4 + 6));
            tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x0c));
            return;
        }
        if (tile_property(tile_at(previous_x, y), 2) != 1u) {
            tou_binary::store_i32(entity, 0x18, -(vx / (game_rand() % 8 + 6)));
            tou_binary::store_i32(entity, 0x1c, -(vy / (game_rand() % 8 + 6)));
            tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 4));
            tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x0c));
            return;
        }
        tou_binary::store_i32(entity, 0x18, -(vx / (game_rand() % 8 + 6)));
        tou_binary::store_i32(entity, 0x1c, vy / (game_rand() % 4 + 6));
        tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 4));
        return;
    }

    /* Type 2/state 10 is the Organic Waste crawler and terrain grower. */
    if (tou_binary::load_u8(entity, 0x21) != 2u || tou_binary::load_u8(entity, 0x20) != 10u) return;
    int scan_count = 0;
    while (tou_binary::load_i32(entity, 8) > 0x180000) {
        const int previous_x = tou_binary::sar_i32(tou_binary::load_i32(entity, 4), 0x12);
        const int previous_y = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x0c), 0x12);
        if (scan_count > 5 || tile_property(tile_at(previous_x, previous_y), 1) != 0u) break;
        tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 8) - FIXED_SCALE);
        tou_binary::store_i32(entity, 0x0c, tou_binary::load_i32(entity, 8));
        tou_binary::store_i32(entity, 4, tou_binary::load_i32(entity, 0));
        tou_binary::store_i32(entity, 0x18, 0);
        tou_binary::store_i32(entity, 0x1c, 0);
        ++scan_count;
    }
    if (tou_binary::load_i32(entity, 8) <= 0x180000) {
        tou_binary::store_u8(entity, 0x5c, 0); tou_binary::store_i32(entity, 0x0c, 0);
        organic_tail(entity); return;
    }
    if (scan_count == 7) tou_binary::store_u8(entity, 0x5c, 0);

    const int previous_x = tou_binary::sar_i32(tou_binary::load_i32(entity, 4), 0x12);
    const int previous_y = tou_binary::sar_i32(tou_binary::load_i32(entity, 0x0c), 0x12);
    if (tile_property(tile_at(previous_x, previous_y), 1) != 1u ||
        tile_property(tile_at(previous_x, previous_y + 1), 0) != 0u) {
        organic_tail(entity); return;
    }
    tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 4));
    tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 0x0c));
    int direction = (game_rand() & 1) != 0 ? -1 : 1;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (tile_property(tile_at(previous_x + direction, previous_y), 1) == 1u &&
            tile_property(tile_at(previous_x + direction, previous_y + 1), 1) == 1u) {
            tou_binary::store_i32(entity, 0x1c, 0x32);
            tou_binary::store_i32(entity, 0, tou_binary::load_i32(entity, 0) + direction * FIXED_SCALE);
            tou_binary::store_i32(entity, 8, tou_binary::load_i32(entity, 8) + FIXED_SCALE);
            tou_binary::store_i32(entity, 0x18, (game_rand() % 0x5a) * direction);
            return;
        }
        direction = -direction;
    }

    x = tou_binary::sar_i32(tou_binary::load_i32(entity, 0), 0x12);
    y = tou_binary::sar_i32(tou_binary::load_i32(entity, 8), 0x12);
    if (tile_at(x, y + 1) < 0xf0u) {
        const uint32_t color_with_bias = tou_binary::load_u32(entity, 0x4c);
        tou_binary::store_i32(entity, 0x28, game_rand() % 400 + 0x96);
        const int paint_y = y - (game_rand() % 2) - 1;
        int width = 1;
        int left = x;
        for (int row = 0; width < 9; ++row, width += 2, --left) {
            for (int column = 0; column < width; ++column) {
                const int px = left + column;
                const int py = paint_y + row;
                if (!map_point_valid(px, py)) continue;
                const int offset = tile_offset(px, py);
                uint8_t &tile = static_cast<uint8_t *>(DAT_0048782c)[offset];
                if (tile_property(tile, 1) == 1u) {
                    tile = tile_property(tile, 0x0f);
                    const uint16_t original_color =
                        static_cast<uint16_t>(color_with_bias - 30000u);
                    static_cast<uint16_t *>(DAT_00481f50)[offset] =
                        rgb555_to_rgb565(original_color);
                }
            }
        }
        smooth_organic_patch(tile_offset(previous_x - 1, previous_y - 1));
    }
    organic_tail(entity);
}

void callback_trail_00430480(int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    tou_binary::store_i32(entity, 0x00,
        tou_binary::load_i32(entity, 0x00) + tou_binary::load_i32(entity, 0x18));
    tou_binary::store_i32(entity, 0x08,
        tou_binary::load_i32(entity, 0x08) + tou_binary::load_i32(entity, 0x1c));
    if (tou_binary::load_u8(entity, 0x40) != 5u && tou_binary::load_u8(entity, 0x21) != 0x65u) {
        tou_binary::store_i32(entity, 0x1c,
            tou_binary::load_i32(entity, 0x1c) + tou_binary::load_i32(entity, 0x38) * DAT_00483828);
    }
    if (tou_binary::load_u8(entity, 0x40) == 5u) {
        /* 0x430534-0x4305A1: Teleport particles damp their directed
         * velocity, then jitter independently around the departure point. */
        static const double kTeleportParticleDamping = 0.985;
        tou_binary::store_i32(entity, 0x18, static_cast<int32_t>(tou_binary::x87_ftol(
            static_cast<long double>(tou_binary::load_i32(entity, 0x18)) *
            static_cast<long double>(kTeleportParticleDamping))));
        tou_binary::store_i32(entity, 0x1c, static_cast<int32_t>(tou_binary::x87_ftol(
            static_cast<long double>(tou_binary::load_i32(entity, 0x1c)) *
            static_cast<long double>(kTeleportParticleDamping))));
        tou_binary::store_i32(entity, 0x00,
            tou_binary::add_wrap_i32(tou_binary::load_i32(entity, 0x00),
                (128 - static_cast<int>(game_rand() & 0xffu)) * 0x1000));
        tou_binary::store_i32(entity, 0x08,
            tou_binary::add_wrap_i32(tou_binary::load_i32(entity, 0x08),
                (128 - static_cast<int>(game_rand() & 0xffu)) * 0x1000));
    }
    const int32_t x = tou_binary::load_i32(entity, 0x00);
    const int32_t y = tou_binary::load_i32(entity, 0x08);
    if (x < 0 || y < 0 || tou_binary::sar_i32(x, 0x12) >= static_cast<int>(DAT_004879f0) ||
        tou_binary::sar_i32(y, 0x12) >= static_cast<int>(DAT_004879f4)) {
        DAT_00481e8f = 1;
        return;
    }
    if (DAT_00489288 == 0) collision_prepass_00437b10(entity_index);
    if (tou_binary::load_u8(entity, 0x21) != 0x67u) return;
    int32_t fade_counter = tou_binary::load_i32(entity, 0x3c) + 1;
    tou_binary::store_i32(entity, 0x3c, fade_counter);
    uint8_t palette_index = tou_binary::load_u8(entity, 0x65);
    tou_binary::store_u32(entity, 0x4c,
        static_cast<uint32_t>(static_cast<uint16_t *>(DAT_00487aa8)[palette_index]) + 30000u);
    if (fade_counter >= tou_binary::load_u8(entity, 0x5c)) {
        --palette_index;
        tou_binary::store_i32(entity, 0x3c, 0);
        tou_binary::store_u8(entity, 0x65, palette_index);
        tou_binary::store_u32(entity, 0x4c,
            static_cast<uint32_t>(static_cast<uint16_t *>(DAT_00487aa8)[palette_index]) + 30000u);
    }
    if (palette_index <= tou_binary::load_u8(entity, 0x64)) DAT_00481e8f = 1;
}

} // namespace

void EntityCallbacks_Init(void)
{
    if (DAT_00487abc == NULL) return;
    uint8_t *table = static_cast<uint8_t *>(DAT_00487abc);
    for (unsigned int type = 0; type < 128u; ++type) {
        tou_binary::store_u32(table, type * 0x218u, 0u);
    }

    struct Mapping { uint8_t type; uint32_t callback; };
    static const Mapping mappings[] = {
        {0x00, 0x00438010u}, {0x01, 0x00438d90u}, {0x02, 0x004427e0u},
        {0x05, 0x00439880u}, {0x08, 0x0043cc20u}, {0x09, 0x0043cc20u},
        {0x0b, 0x00431650u}, {0x0e, 0x00430dc0u}, {0x0f, 0x004330c0u},
        {0x11, 0x00441aa0u}, {0x12, 0x00438010u}, {0x13, 0x0043a4b0u},
        {0x14, 0x0043a4b0u}, {0x16, 0x00439b90u}, {0x17, 0x00432c80u},
        {0x18, 0x00433c80u}, {0x19, 0x00443420u}, {0x1b, 0x00443b10u},
        {0x1c, 0x00440e20u}, {0x1d, 0x0043c0b0u}, {0x1e, 0x0043c0b0u},
        {0x1f, 0x0043b370u}, {0x22, 0x004442f0u}, {0x23, 0x004457b0u},
        {0x24, 0x00447a70u}, {0x25, 0x00446130u}, {0x26, 0x0043dbd0u},
        {0x27, 0x0043e070u}, {0x28, 0x0043e890u}, {0x29, 0x0043e890u},
        {0x2a, 0x0043e890u}, {0x2b, 0x00439b90u}, {0x2c, 0x0043f990u},
        {0x2d, 0x0043f990u}, {0x2e, 0x00432220u}, {0x64, 0x004309f0u},
        {0x65, 0x00430480u}, {0x66, 0x004427e0u}, {0x67, 0x00430480u},
        {0x69, 0x00438010u}, {0x6a, 0x00439b90u}, {0x6b, 0x0043a4b0u},
        {0x6c, 0x004309f0u}, {0x6d, 0x004309f0u}
    };

    for (unsigned int i = 0; i < sizeof(mappings) / sizeof(mappings[0]); ++i) {
        tou_binary::store_u32(table, mappings[i].type * 0x218u,
                                mappings[i].callback);
    }
}

void EntityCallbacks_InitNucleusMarkII(void *entity)
{
    /* Original fire path 0x00406b20-0x00406b36. */
    tou_binary::store_i32(static_cast<uint8_t *>(entity), 0x28, 0x19);
}

void EntityCallbacks_ApplyStickyWasteSlowdown(int32_t *velocity_x, int32_t *velocity_y)
{
    if (velocity_x == NULL || velocity_y == NULL) return;
    const long double factor = static_cast<long double>(0.8);
    *velocity_x = static_cast<int32_t>(
        tou_binary::x87_ftol(static_cast<long double>(*velocity_x) * factor));
    *velocity_y = static_cast<int32_t>(
        tou_binary::x87_ftol(static_cast<long double>(*velocity_y) * factor));
}

bool EntityCallbacks_Dispatch(uint32_t callback_address, int entity_index)
{
    uint8_t *entity = entity_at(entity_index);
    const uint8_t entity_type = tou_binary::load_u8(entity, 0x21);
    if (callback_address != kCallbackNucleus &&
        callback_address != kCallbackNuclearBarrel &&
        callback_address != kCallbackMovingSucker &&
        callback_address != kCallbackBoneCrusher &&
        callback_address != kCallbackShotgunRapidfire &&
        callback_address != kCallbackDumbfire &&
        callback_address != kCallbackCollapser &&
        callback_address != kCallbackTournaillerKicker &&
        callback_address != kCallbackMachineGun &&
        callback_address != kCallbackMorningStar &&
        callback_address != kCallbackGammaBoom &&
        callback_address != kCallbackWavyFireworks &&
        callback_address != kCallbackRomanCandle &&
        callback_address != kCallbackEtna &&
        callback_address != kCallbackOrganicWaste &&
        callback_address != kCallbackOrganicWasteII &&
        callback_address != kCallbackNormalFireball &&
        callback_address != kCallbackTrail) return false;

    /* Several guest callbacks are shared by unrelated entity types. Only claim
     * the paths lifted above; unsupported siblings remain on the legacy path. */
    if ((callback_address == kCallbackNuclearBarrel && entity_type != 0x0bu) ||
        (callback_address == kCallbackMovingSucker && entity_type != 0x0eu) ||
        (callback_address == kCallbackBoneCrusher && entity_type != 0x0fu) ||
        (callback_address == kCallbackShotgunRapidfire && entity_type != 0x00u && entity_type != 0x12u) ||
        (callback_address == kCallbackDumbfire && entity_type != 0x01u) ||
        (callback_address == kCallbackCollapser && entity_type != 0x05u) ||
        (callback_address == kCallbackTournaillerKicker && entity_type != 0x08u && entity_type != 0x09u) ||
        (callback_address == kCallbackMachineGun && entity_type != 0x2cu) ||
        (callback_address == kCallbackMorningStar && entity_type != 0x26u) ||
        (callback_address == kCallbackGammaBoom && entity_type != 0x23u) ||
        (callback_address == kCallbackWavyFireworks &&
            (entity_type != 0x22u || tou_binary::load_u8(entity, 0x40) < 2u ||
             tou_binary::load_i32(entity, 0x28) <= 0 || tou_binary::load_u8(entity, 0x26) != 0xffu)) ||
        (callback_address == kCallbackRomanCandle && entity_type != 0x25u) ||
        (callback_address == kCallbackEtna && entity_type != 0x24u) ||
        (callback_address == kCallbackOrganicWaste && entity_type != 0x02u && entity_type != 0x66u) ||
        (callback_address == kCallbackOrganicWasteII && entity_type != 0x14u && entity_type != 0x6bu) ||
        (callback_address == kCallbackNormalFireball && entity_type != 0x11u) ||
        (callback_address == kCallbackTrail && entity_type != 0x67u)) return false;

    static uint64_t sequence = 0;
    const int32_t before_x = tou_binary::load_i32(entity, 0x00);
    const int32_t before_y = tou_binary::load_i32(entity, 0x08);
    const int32_t before_life = tou_binary::load_i32(entity, 0x28);
    const uint8_t before_state = tou_binary::load_u8(entity, 0x20);
    const uint8_t before_guard = tou_binary::load_u8(entity, 0x5c);
    const uint64_t rng_before = TOU_RandCallCount();

    if (callback_address == kCallbackNucleus) callback_nucleus_00432c80(entity_index);
    else if (callback_address == kCallbackNuclearBarrel) callback_nuclear_barrel_00431650(entity_index);
    else if (callback_address == kCallbackMovingSucker) callback_moving_sucker_00430dc0(entity_index);
    else if (callback_address == kCallbackBoneCrusher) callback_bone_crusher_004330c0(entity_index);
    else if (callback_address == kCallbackShotgunRapidfire && entity_type == 0x12u) callback_airstrike_00438010(entity_index);
    else if (callback_address == kCallbackShotgunRapidfire) callback_shotgun_rapidfire_00438010(entity_index);
    else if (callback_address == kCallbackDumbfire) callback_dumbfire_00438d90(entity_index);
    else if (callback_address == kCallbackCollapser) callback_collapser_00439880(entity_index);
    else if (callback_address == kCallbackTournaillerKicker) callback_tournailler_kicker_0043cc20(entity_index);
    else if (callback_address == kCallbackMachineGun) callback_machinegun_0043f990(entity_index);
    else if (callback_address == kCallbackMorningStar) callback_morning_star_0043dbd0(entity_index);
    else if (callback_address == kCallbackGammaBoom) callback_gamma_boom_004457b0(entity_index);
    else if (callback_address == kCallbackWavyFireworks) callback_fireworks_launcher_child_004442f0(entity_index);
    else if (callback_address == kCallbackRomanCandle) callback_roman_candle_00446130(entity_index);
    else if (callback_address == kCallbackEtna) callback_etna_00447a70(entity_index);
    else if (callback_address == kCallbackOrganicWaste) callback_organic_waste_004427e0(entity_index);
    else if (callback_address == kCallbackOrganicWasteII && entity_type == 0x6bu) callback_jupiter_missile_0043a4b0(entity_index);
    else if (callback_address == kCallbackOrganicWasteII) callback_organic_waste_ii_0043a4b0(entity_index);
    else if (callback_address == kCallbackNormalFireball) callback_normal_fireball_00441aa0(entity_index);
    else callback_trail_00430480(entity_index);
    trace_callback(sequence++, callback_address, entity_index, before_x, before_y,
                   before_life, before_state, before_guard, rng_before);
    return true;
}

void EntityCallbacks_RemoveAt(int entity_index)
{
    if (entity_index < 0 || entity_index >= g_EntityCount) return;
    uint8_t *removed = entity_at(entity_index);

    if (is_tracked(removed)) {
        const int category = tracking_category(tou_binary::load_u8(removed, 0x21));
        if (category >= 0 && DAT_00487834[category] > 0) {
            const int removed_slot = tou_binary::load_i32(removed, 0x50);
            const int new_count = --DAT_00487834[category];
            int *links = static_cast<int *>(DAT_0048781c) + category * 0x1000;
            const int moved_entity_index = links[new_count];
            tou_binary::store_i32(entity_at(moved_entity_index), 0x50, removed_slot);
            links[removed_slot] = moved_entity_index;
        }
    }

    const int last_index = g_EntityCount - 1;
    uint8_t *last = entity_at(last_index);
    if (entity_index != last_index && is_tracked(last)) {
        const int category = tracking_category(tou_binary::load_u8(last, 0x21));
        if (category >= 0) {
            int *links = static_cast<int *>(DAT_0048781c) + category * 0x1000;
            links[tou_binary::load_i32(last, 0x50)] = entity_index;
        }
    }

    --g_EntityCount;
    copy_entity_fields(removed, entity_at(g_EntityCount));
}
