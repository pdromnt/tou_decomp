#include "tou.h"
#include "simulation_state.h"

#include <stddef.h>
#include <string.h>

namespace {

const uint8_t kSnapshotMagic[8] = {'T', 'O', 'U', 'S', 'N', 'A', 'P', 0};
const uint32_t kSnapshotVersion = 3;
const int kWaypointRecordLimit = 1400000 / 0x1c;
const int kSpawnPointLimit = 0xc00 / 0x0c;
const int kDecorationLimit = 0x800 / 0x10;
const int kStaticEntityLimit = 0x40 / 0x10;
const size_t kEntityTrackingBytes = 0x30000;
uint32_t g_simulation_tick = 0;
uint64_t g_last_checksum = 0;
uint32_t g_checksum_interval = 0;
std::vector<uint8_t> g_checksum_snapshot;

class Writer {
public:
    explicit Writer(std::vector<uint8_t> *bytes) : bytes_(bytes) {}

    void raw(const void *data, size_t size)
    {
        if (size == 0)
            return;
        const uint8_t *first = static_cast<const uint8_t *>(data);
        bytes_->insert(bytes_->end(), first, first + size);
    }

    void u32(uint32_t value)
    {
        uint8_t bytes[4] = {
            static_cast<uint8_t>(value),
            static_cast<uint8_t>(value >> 8),
            static_cast<uint8_t>(value >> 16),
            static_cast<uint8_t>(value >> 24)
        };
        raw(bytes, sizeof(bytes));
    }

    void u64(uint64_t value)
    {
        u32(static_cast<uint32_t>(value));
        u32(static_cast<uint32_t>(value >> 32));
    }

private:
    std::vector<uint8_t> *bytes_;
};

class Reader {
public:
    Reader(const uint8_t *bytes, size_t size) : cursor_(bytes), remaining_(size) {}

    bool raw(void *output, size_t size)
    {
        if (size == 0)
            return true;
        if (size > remaining_)
            return false;
        memcpy(output, cursor_, size);
        cursor_ += size;
        remaining_ -= size;
        return true;
    }

    bool u32(uint32_t *value)
    {
        uint8_t bytes[4];
        if (!raw(bytes, sizeof(bytes)))
            return false;
        *value = static_cast<uint32_t>(bytes[0]) |
                 (static_cast<uint32_t>(bytes[1]) << 8) |
                 (static_cast<uint32_t>(bytes[2]) << 16) |
                 (static_cast<uint32_t>(bytes[3]) << 24);
        return true;
    }

    bool u64(uint64_t *value)
    {
        uint32_t low;
        uint32_t high;
        if (!u32(&low) || !u32(&high))
            return false;
        *value = static_cast<uint64_t>(low) | (static_cast<uint64_t>(high) << 32);
        return true;
    }

    size_t remaining() const { return remaining_; }

private:
    const uint8_t *cursor_;
    size_t remaining_;
};

bool ValidCount(int count, int capacity)
{
    return count >= 0 && count <= capacity;
}

void WritePlayer(Writer *writer, const PlayerData &player)
{
    /* Physical key bindings, local-human/camera ownership, viewport geometry,
     * and audio-channel ownership are presentation/input state. */
    writer->raw(&player, offsetof(PlayerData, key_scan_codes));
    writer->raw(reinterpret_cast<const uint8_t *>(&player) +
                    offsetof(PlayerData, unknown_0b3),
                offsetof(PlayerData, human_controlled) - offsetof(PlayerData, unknown_0b3));
    writer->raw(reinterpret_cast<const uint8_t *>(&player) +
                    offsetof(PlayerData, frag_count),
                offsetof(PlayerData, sound_timer) - offsetof(PlayerData, frag_count));
}

bool ReadPlayer(Reader *reader, PlayerData *player)
{
    return reader->raw(player, offsetof(PlayerData, key_scan_codes)) &&
           reader->raw(reinterpret_cast<uint8_t *>(player) +
                           offsetof(PlayerData, unknown_0b3),
                       offsetof(PlayerData, human_controlled) - offsetof(PlayerData, unknown_0b3)) &&
           reader->raw(reinterpret_cast<uint8_t *>(player) +
                           offsetof(PlayerData, frag_count),
                       offsetof(PlayerData, sound_timer) - offsetof(PlayerData, frag_count));
}

void WriteCount(Writer *writer, int count) { writer->u32(static_cast<uint32_t>(count)); }

uint64_t Fnv1a64(const uint8_t *bytes, size_t size)
{
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace

void SimulationState_Reset(void)
{
    g_simulation_tick = 0;
    g_last_checksum = 0;
}

void SimulationState_SetChecksumInterval(uint32_t ticks)
{
    g_checksum_interval = ticks;
}

uint32_t SimulationState_Tick(void) { return g_simulation_tick; }
uint64_t SimulationState_Checksum(void) { return g_last_checksum; }

bool SimulationState_Capture(std::vector<uint8_t> *snapshot)
{
    if (snapshot == NULL || DAT_00481f50 == NULL || DAT_0048782c == NULL ||
        DAT_00487810 == NULL || DAT_0048781c == NULL || DAT_00487a00 <= 0 ||
        DAT_004879f4 == 0 || DAT_00481f48 == NULL || DAT_00487820 == NULL ||
        DAT_00489e84 == NULL || DAT_004876a0 == NULL || DAT_00487aa0 == NULL ||
        DAT_00489e98 == NULL ||
        !ValidCount(DAT_00489240, GAMEPLAY_PLAYER_CAPACITY) ||
        !ValidCount(g_EntityCount, ENTITY_ACTIVE_CAPACITY) ||
        !ValidCount(g_TrooperCount, TROOPER_CAPACITY) ||
        !ValidCount(g_ProjectileCount, PROJECTILE_CAPACITY) ||
        !ValidCount(g_ParticleCount, PARTICLE_CAPACITY) ||
        !ValidCount(g_FireParticleCount, EDGE_RECORD_CAPACITY) ||
        !ValidCount(g_DebrisItemCount, DEBRIS_ITEM_CAPACITY) ||
        !ValidCount(DAT_00489264, 25) || !ValidCount(DAT_0048926c, 100) ||
        !ValidCount(g_FluidSourceCount, FLUID_SOURCE_CAPACITY) ||
        !ValidCount(DAT_00489270, 16) ||
        DAT_00489280 < 0 || !ValidCount(DAT_0048927c, DAT_00489280) ||
        !ValidCount(DAT_004892c8, kWaypointRecordLimit) ||
        !ValidCount(g_MapEdgeCount, MAP_EDGE_CAPACITY) ||
        !ValidCount(DAT_004892d4, kSpawnPointLimit) ||
        !ValidCount(DAT_004892d8, kDecorationLimit) ||
        !ValidCount(DAT_00489274, kStaticEntityLimit)) {
        return false;
    }

    snapshot->clear();
    Writer writer(snapshot);
    writer.raw(kSnapshotMagic, sizeof(kSnapshotMagic));
    writer.u32(kSnapshotVersion);
    writer.u32(g_simulation_tick);
    writer.u32(TOU_RandState());
    writer.u64(TOU_RandCallCount());
    writer.u32(DAT_004879f0);
    writer.u32(DAT_004879f4);
    writer.u32(static_cast<uint32_t>(DAT_00487a00));

    const int counts[] = {
        DAT_00489240, g_EntityCount, g_TrooperCount, g_ProjectileCount,
        g_ParticleCount, g_FireParticleCount, g_DebrisItemCount,
        DAT_00489264, DAT_0048926c, g_FluidSourceCount, DAT_00489270,
        DAT_0048927c, DAT_004892c8, g_MapEdgeCount, DAT_004892d4,
        DAT_004892d8, DAT_00489274
    };
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i)
        WriteCount(&writer, counts[i]);

    writer.raw(&DAT_00489288, sizeof(DAT_00489288));
    writer.raw(&DAT_004892a4, sizeof(DAT_004892a4));
    writer.raw(&DAT_004892a5, sizeof(DAT_004892a5));
    /* DAT_004892bc is elapsed wall-clock time derived from Platform_GetTicks.
     * It is scoreboard presentation state, not deterministic simulation state,
     * and naturally differs by a few milliseconds between LAN peers. */
    writer.raw(&DAT_004892d0, sizeof(DAT_004892d0));
    writer.raw(g_TeamWins, sizeof(g_TeamWins));
    writer.raw(&g_SubState, sizeof(g_SubState));
    writer.raw(&DAT_004892a8, sizeof(DAT_004892a8));
    writer.raw(&DAT_004892ac, sizeof(DAT_004892ac));
    writer.raw(&DAT_0048693c, sizeof(DAT_0048693c));
    writer.raw(&DAT_004892e4, sizeof(DAT_004892e4));
    writer.raw(&DAT_004892e5, sizeof(DAT_004892e5));
    writer.raw(&DAT_0048764a, sizeof(DAT_0048764a));
    writer.raw(&DAT_00487640[0], sizeof(DAT_00487640[0]));
    writer.raw(&DAT_00489284, sizeof(DAT_00489284));
    writer.raw(&DAT_004892cc, sizeof(DAT_004892cc));
    writer.raw(&DAT_0048929c, sizeof(DAT_0048929c));
    writer.raw(&DAT_004892c0, sizeof(DAT_004892c0));
    writer.raw(&DAT_004892dc, sizeof(DAT_004892dc));
    writer.raw(&DAT_004892e0, sizeof(DAT_004892e0));
    writer.raw(DAT_00487834, sizeof(DAT_00487834));
    writer.raw(DAT_00486944, sizeof(DAT_00486944));
    writer.raw(DAT_00486954, sizeof(DAT_00486954));
    writer.raw(&DAT_00486964, sizeof(DAT_00486964));
    writer.raw(DAT_00486968, sizeof(DAT_00486968));
    writer.raw(DAT_00486aa8, sizeof(DAT_00486aa8));
    writer.raw(DAT_00486be8, sizeof(DAT_00486be8));
    writer.raw(DAT_00486d28, sizeof(DAT_00486d28));
    writer.raw(DAT_00486e68, sizeof(DAT_00486e68));
    writer.raw(DAT_00486fa8, sizeof(DAT_00486fa8));
    writer.raw(DAT_004870e8, sizeof(DAT_004870e8));
    writer.raw(DAT_00487228, sizeof(DAT_00487228));

    for (int i = 0; i < DAT_00489240; ++i)
        WritePlayer(&writer, *Player_Get(i));

    writer.raw(g_EntityPool, static_cast<size_t>(g_EntityCount) * sizeof(Entity));
    writer.raw(g_TrooperPool, static_cast<size_t>(g_TrooperCount) * sizeof(TrooperRecord));
    writer.raw(g_ProjectilePool, static_cast<size_t>(g_ProjectileCount) * sizeof(ProjectileRecord));
    writer.raw(g_ParticlePool, static_cast<size_t>(g_ParticleCount) * sizeof(ParticleRecord));
    writer.raw(DAT_00481f2c, static_cast<size_t>(g_FireParticleCount) * 0x20u);
    writer.raw(g_DebrisItemPool, static_cast<size_t>(g_DebrisItemCount) * sizeof(DebrisItemRecord));
    writer.raw(DAT_00487780, static_cast<size_t>(DAT_00489264) * 0x20u);
    writer.raw(DAT_00487a9c, static_cast<size_t>(DAT_0048926c) * 0x20u);
    writer.raw(DAT_00489e7c, static_cast<size_t>(g_FluidSourceCount) * 0x20u);
    writer.raw(DAT_00489e80, static_cast<size_t>(DAT_00489270) * 0x20u);
    writer.raw(DAT_00481f48, static_cast<size_t>(DAT_0048927c) * 0x08u);
    writer.raw(DAT_00487820, static_cast<size_t>(DAT_004892c8) * 0x1cu);
    writer.raw(DAT_00489e84, static_cast<size_t>(g_MapEdgeCount) * 0x10u);
    writer.raw(DAT_004876a0, static_cast<size_t>(DAT_004892d4) * 0x0cu);
    writer.raw(DAT_00487aa0, static_cast<size_t>(DAT_004892d8) * 0x10u);
    writer.raw(DAT_00489e98, static_cast<size_t>(DAT_00489274) * 0x10u);
    writer.raw(DAT_0048781c, kEntityTrackingBytes);

    const size_t cells = static_cast<size_t>(DAT_00487a00) * DAT_004879f4;
    writer.raw(DAT_00481f50, cells * sizeof(uint16_t));
    writer.raw(DAT_0048782c, cells);
    return true;
}

bool SimulationState_Restore(const uint8_t *snapshot, size_t size)
{
    if (snapshot == NULL || DAT_00481f50 == NULL || DAT_0048782c == NULL ||
        DAT_0048781c == NULL || DAT_00481f48 == NULL || DAT_00487820 == NULL ||
        DAT_00489e84 == NULL || DAT_004876a0 == NULL || DAT_00487aa0 == NULL ||
        DAT_00489e98 == NULL)
        return false;

    Reader reader(snapshot, size);
    uint8_t magic[sizeof(kSnapshotMagic)];
    uint32_t version;
    uint32_t tick;
    uint32_t rng_state;
    uint64_t rng_calls;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    if (!reader.raw(magic, sizeof(magic)) || memcmp(magic, kSnapshotMagic, sizeof(magic)) != 0 ||
        !reader.u32(&version) || version != kSnapshotVersion || !reader.u32(&tick) ||
        !reader.u32(&rng_state) || !reader.u64(&rng_calls) ||
        !reader.u32(&width) || !reader.u32(&height) || !reader.u32(&stride) ||
        width != DAT_004879f0 || height != DAT_004879f4 || stride != static_cast<uint32_t>(DAT_00487a00)) {
        return false;
    }

    uint32_t raw_counts[17];
    for (size_t i = 0; i < sizeof(raw_counts) / sizeof(raw_counts[0]); ++i) {
        if (!reader.u32(&raw_counts[i]))
            return false;
    }
    const int player_count = static_cast<int>(raw_counts[0]);
    const int entity_count = static_cast<int>(raw_counts[1]);
    const int trooper_count = static_cast<int>(raw_counts[2]);
    const int projectile_count = static_cast<int>(raw_counts[3]);
    const int particle_count = static_cast<int>(raw_counts[4]);
    const int fire_count = static_cast<int>(raw_counts[5]);
    const int debris_count = static_cast<int>(raw_counts[6]);
    const int misc_count = static_cast<int>(raw_counts[7]);
    const int explosion_count = static_cast<int>(raw_counts[8]);
    const int fluid_count = static_cast<int>(raw_counts[9]);
    const int wall_count = static_cast<int>(raw_counts[10]);
    const int turret_count = static_cast<int>(raw_counts[11]);
    const int waypoint_count = static_cast<int>(raw_counts[12]);
    const int map_edge_count = static_cast<int>(raw_counts[13]);
    const int spawn_count = static_cast<int>(raw_counts[14]);
    const int decoration_count = static_cast<int>(raw_counts[15]);
    const int static_entity_count = static_cast<int>(raw_counts[16]);
    if (!ValidCount(player_count, GAMEPLAY_PLAYER_CAPACITY) ||
        !ValidCount(entity_count, ENTITY_ACTIVE_CAPACITY) ||
        !ValidCount(trooper_count, TROOPER_CAPACITY) ||
        !ValidCount(projectile_count, PROJECTILE_CAPACITY) ||
        !ValidCount(particle_count, PARTICLE_CAPACITY) ||
        !ValidCount(fire_count, EDGE_RECORD_CAPACITY) ||
        !ValidCount(debris_count, DEBRIS_ITEM_CAPACITY) ||
        !ValidCount(misc_count, 25) || !ValidCount(explosion_count, 100) ||
        !ValidCount(fluid_count, FLUID_SOURCE_CAPACITY) || !ValidCount(wall_count, 16) ||
        DAT_00489280 < 0 || !ValidCount(turret_count, DAT_00489280) ||
        !ValidCount(waypoint_count, kWaypointRecordLimit) ||
        !ValidCount(map_edge_count, MAP_EDGE_CAPACITY) ||
        !ValidCount(spawn_count, kSpawnPointLimit) ||
        !ValidCount(decoration_count, kDecorationLimit) ||
        !ValidCount(static_entity_count, kStaticEntityLimit) ||
        player_count != DAT_00489240) {
        return false;
    }

    const size_t player_bytes = offsetof(PlayerData, key_scan_codes) +
        (offsetof(PlayerData, human_controlled) - offsetof(PlayerData, unknown_0b3)) +
        (offsetof(PlayerData, sound_timer) - offsetof(PlayerData, frag_count));
    const size_t cells = static_cast<size_t>(stride) * height;
    const size_t expected_body =
        sizeof(DAT_00489288) + sizeof(DAT_004892a4) + sizeof(DAT_004892a5) +
        sizeof(DAT_004892d0) + sizeof(g_TeamWins) +
        sizeof(g_SubState) + sizeof(DAT_004892a8) +
        sizeof(DAT_004892ac) + sizeof(DAT_0048693c) +
        sizeof(DAT_004892e4) + sizeof(DAT_004892e5) +
        sizeof(DAT_0048764a) + sizeof(DAT_00487640[0]) +
        sizeof(DAT_00489284) + sizeof(DAT_004892cc) +
        sizeof(DAT_0048929c) + sizeof(DAT_004892c0) +
        sizeof(DAT_004892dc) + sizeof(DAT_004892e0) +
        sizeof(DAT_00487834) + sizeof(DAT_00486944) +
        sizeof(DAT_00486954) + sizeof(DAT_00486964) +
        sizeof(DAT_00486968) + sizeof(DAT_00486aa8) +
        sizeof(DAT_00486be8) + sizeof(DAT_00486d28) +
        sizeof(DAT_00486e68) + sizeof(DAT_00486fa8) +
        sizeof(DAT_004870e8) + sizeof(DAT_00487228) +
        static_cast<size_t>(player_count) * player_bytes +
        static_cast<size_t>(entity_count) * sizeof(Entity) +
        static_cast<size_t>(trooper_count) * sizeof(TrooperRecord) +
        static_cast<size_t>(projectile_count) * sizeof(ProjectileRecord) +
        static_cast<size_t>(particle_count) * sizeof(ParticleRecord) +
        static_cast<size_t>(fire_count) * 0x20u +
        static_cast<size_t>(debris_count) * sizeof(DebrisItemRecord) +
        static_cast<size_t>(misc_count) * 0x20u +
        static_cast<size_t>(explosion_count) * 0x20u +
        static_cast<size_t>(fluid_count) * 0x20u +
        static_cast<size_t>(wall_count) * 0x20u +
        static_cast<size_t>(turret_count) * 0x08u +
        static_cast<size_t>(waypoint_count) * 0x1cu +
        static_cast<size_t>(map_edge_count) * 0x10u +
        static_cast<size_t>(spawn_count) * 0x0cu +
        static_cast<size_t>(decoration_count) * 0x10u +
        static_cast<size_t>(static_entity_count) * 0x10u +
        kEntityTrackingBytes + cells * (sizeof(uint16_t) + 1u);
    if (reader.remaining() != expected_body)
        return false;

    if (!reader.raw(&DAT_00489288, sizeof(DAT_00489288)) ||
        !reader.raw(&DAT_004892a4, sizeof(DAT_004892a4)) ||
        !reader.raw(&DAT_004892a5, sizeof(DAT_004892a5)) ||
        !reader.raw(&DAT_004892d0, sizeof(DAT_004892d0)) ||
        !reader.raw(g_TeamWins, sizeof(g_TeamWins)) ||
        !reader.raw(&g_SubState, sizeof(g_SubState)) ||
        !reader.raw(&DAT_004892a8, sizeof(DAT_004892a8)) ||
        !reader.raw(&DAT_004892ac, sizeof(DAT_004892ac)) ||
        !reader.raw(&DAT_0048693c, sizeof(DAT_0048693c)) ||
        !reader.raw(&DAT_004892e4, sizeof(DAT_004892e4)) ||
        !reader.raw(&DAT_004892e5, sizeof(DAT_004892e5)) ||
        !reader.raw(&DAT_0048764a, sizeof(DAT_0048764a)) ||
        !reader.raw(&DAT_00487640[0], sizeof(DAT_00487640[0])) ||
        !reader.raw(&DAT_00489284, sizeof(DAT_00489284)) ||
        !reader.raw(&DAT_004892cc, sizeof(DAT_004892cc)) ||
        !reader.raw(&DAT_0048929c, sizeof(DAT_0048929c)) ||
        !reader.raw(&DAT_004892c0, sizeof(DAT_004892c0)) ||
        !reader.raw(&DAT_004892dc, sizeof(DAT_004892dc)) ||
        !reader.raw(&DAT_004892e0, sizeof(DAT_004892e0)) ||
        !reader.raw(DAT_00487834, sizeof(DAT_00487834)) ||
        !reader.raw(DAT_00486944, sizeof(DAT_00486944)) ||
        !reader.raw(DAT_00486954, sizeof(DAT_00486954)) ||
        !reader.raw(&DAT_00486964, sizeof(DAT_00486964)) ||
        !reader.raw(DAT_00486968, sizeof(DAT_00486968)) ||
        !reader.raw(DAT_00486aa8, sizeof(DAT_00486aa8)) ||
        !reader.raw(DAT_00486be8, sizeof(DAT_00486be8)) ||
        !reader.raw(DAT_00486d28, sizeof(DAT_00486d28)) ||
        !reader.raw(DAT_00486e68, sizeof(DAT_00486e68)) ||
        !reader.raw(DAT_00486fa8, sizeof(DAT_00486fa8)) ||
        !reader.raw(DAT_004870e8, sizeof(DAT_004870e8)) ||
        !reader.raw(DAT_00487228, sizeof(DAT_00487228))) {
        return false;
    }

    for (int i = 0; i < player_count; ++i) {
        if (!ReadPlayer(&reader, Player_Get(i)))
            return false;
    }
    if (!reader.raw(g_EntityPool, static_cast<size_t>(entity_count) * sizeof(Entity)) ||
        !reader.raw(g_TrooperPool, static_cast<size_t>(trooper_count) * sizeof(TrooperRecord)) ||
        !reader.raw(g_ProjectilePool, static_cast<size_t>(projectile_count) * sizeof(ProjectileRecord)) ||
        !reader.raw(g_ParticlePool, static_cast<size_t>(particle_count) * sizeof(ParticleRecord)) ||
        !reader.raw(DAT_00481f2c, static_cast<size_t>(fire_count) * 0x20u) ||
        !reader.raw(g_DebrisItemPool, static_cast<size_t>(debris_count) * sizeof(DebrisItemRecord)) ||
        !reader.raw(DAT_00487780, static_cast<size_t>(misc_count) * 0x20u) ||
        !reader.raw(DAT_00487a9c, static_cast<size_t>(explosion_count) * 0x20u) ||
        !reader.raw(DAT_00489e7c, static_cast<size_t>(fluid_count) * 0x20u) ||
        !reader.raw(DAT_00489e80, static_cast<size_t>(wall_count) * 0x20u) ||
        !reader.raw(DAT_00481f48, static_cast<size_t>(turret_count) * 0x08u) ||
        !reader.raw(DAT_00487820, static_cast<size_t>(waypoint_count) * 0x1cu) ||
        !reader.raw(DAT_00489e84, static_cast<size_t>(map_edge_count) * 0x10u) ||
        !reader.raw(DAT_004876a0, static_cast<size_t>(spawn_count) * 0x0cu) ||
        !reader.raw(DAT_00487aa0, static_cast<size_t>(decoration_count) * 0x10u) ||
        !reader.raw(DAT_00489e98, static_cast<size_t>(static_entity_count) * 0x10u) ||
        !reader.raw(DAT_0048781c, kEntityTrackingBytes)) {
        return false;
    }
    if (!reader.raw(DAT_00481f50, cells * sizeof(uint16_t)) ||
        !reader.raw(DAT_0048782c, cells) || reader.remaining() != 0) {
        return false;
    }

    g_EntityCount = entity_count;
    g_TrooperCount = trooper_count;
    g_ProjectileCount = projectile_count;
    g_ParticleCount = particle_count;
    g_FireParticleCount = fire_count;
    g_DebrisItemCount = debris_count;
    DAT_00489264 = misc_count;
    DAT_0048926c = explosion_count;
    g_FluidSourceCount = fluid_count;
    DAT_00489270 = wall_count;
    DAT_0048927c = turret_count;
    DAT_004892c8 = waypoint_count;
    g_MapEdgeCount = map_edge_count;
    DAT_004892d4 = spawn_count;
    DAT_004892d8 = decoration_count;
    DAT_00489274 = static_entity_count;
    g_simulation_tick = tick;
    TOU_RestoreRandState(rng_state, rng_calls);
    g_last_checksum = Fnv1a64(snapshot, size);
    return true;
}

bool SimulationState_ValidateRoundTrip(const std::vector<uint8_t> &snapshot)
{
    if (snapshot.empty() ||
        !SimulationState_Restore(snapshot.data(), snapshot.size()))
        return false;
    std::vector<uint8_t> restored;
    return SimulationState_Capture(&restored) && restored == snapshot;
}

uint64_t SimulationState_OnTickComplete(void)
{
    ++g_simulation_tick;
    if (g_checksum_interval == 0 ||
        (g_simulation_tick % g_checksum_interval) != 0)
        return g_last_checksum;
    g_last_checksum = SimulationState_Capture(&g_checksum_snapshot)
        ? Fnv1a64(g_checksum_snapshot.data(), g_checksum_snapshot.size()) : 0;
    return g_last_checksum;
}
