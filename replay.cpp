#include "tou.h"
#include "replay.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <vector>

namespace {

const uint8_t kReplayMagic[8] = {'T', 'O', 'U', 'R', 'P', 'L', 'Y', 0};
const uint32_t kReplayVersion = 2;
const size_t kMaximumReplayFrames = 10000000u;

enum ReplayMode { REPLAY_OFF, REPLAY_RECORD, REPLAY_PLAYBACK };

struct ReplayFrame {
    uint32_t tick;
    uint8_t player_count;
    uint8_t actions[4];
    uint64_t checksum;
};

ReplayMode g_replay_mode = REPLAY_OFF;
FILE *g_replay_file = NULL;
std::vector<uint8_t> g_initial_snapshot;
std::vector<ReplayFrame> g_frames;
size_t g_frame_index = 0;
ReplayFrame g_pending_frame = {};
bool g_header_written = false;
bool g_initial_restored = false;
GameConfig g_saved_config;
bool g_have_saved_config = false;
char g_replay_status[192] = "Replay disabled";

void Put32(FILE *file, uint32_t value)
{
    const uint8_t bytes[4] = {
        static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
        static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)
    };
    fwrite(bytes, 1, sizeof(bytes), file);
}

void Put64(FILE *file, uint64_t value)
{
    Put32(file, static_cast<uint32_t>(value));
    Put32(file, static_cast<uint32_t>(value >> 32));
}

bool Get32(FILE *file, uint32_t *value)
{
    uint8_t bytes[4];
    if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes))
        return false;
    *value = static_cast<uint32_t>(bytes[0]) |
             (static_cast<uint32_t>(bytes[1]) << 8) |
             (static_cast<uint32_t>(bytes[2]) << 16) |
             (static_cast<uint32_t>(bytes[3]) << 24);
    return true;
}

bool Get64(FILE *file, uint64_t *value)
{
    uint32_t low;
    uint32_t high;
    if (!Get32(file, &low) || !Get32(file, &high))
        return false;
    *value = static_cast<uint64_t>(low) | (static_cast<uint64_t>(high) << 32);
    return true;
}

const char *ArgumentValue(int argc, char **argv, const char *name)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], name) == 0)
            return argv[i + 1];
    }
    return NULL;
}

void SetReplayStatus(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_replay_status, sizeof(g_replay_status), format, args);
    va_end(args);
    LOG("[REPLAY] %s\n", g_replay_status);
    char title[256];
    snprintf(title, sizeof(title), "%s - %s", STR_TITLE, g_replay_status);
    Platform_SetWindowTitle(title);
}

bool ReadReplay(FILE *file)
{
    uint8_t magic[sizeof(kReplayMagic)];
    uint32_t version;
    uint32_t config_size;
    uint32_t snapshot_size;
    if (fread(magic, 1, sizeof(magic), file) != sizeof(magic) ||
        memcmp(magic, kReplayMagic, sizeof(magic)) != 0 ||
        !Get32(file, &version) || version != kReplayVersion ||
        !Get32(file, &config_size) || config_size != GAME_CONFIG_SIZE ||
        !Get32(file, &snapshot_size) || snapshot_size > 512u * 1024u * 1024u ||
        fread(g_GameConfig.bytes, 1, GAME_CONFIG_SIZE, file) != GAME_CONFIG_SIZE) {
        return false;
    }
    g_initial_snapshot.resize(snapshot_size);
    if (snapshot_size != 0 &&
        fread(g_initial_snapshot.data(), 1, snapshot_size, file) != snapshot_size)
        return false;

    uint32_t previous_tick = 0;
    for (;;) {
        ReplayFrame frame = {};
        uint32_t tick;
        if (!Get32(file, &tick)) {
            if (feof(file))
                break;
            return false;
        }
        if (tick == 0 || tick <= previous_tick || g_frames.size() >= kMaximumReplayFrames)
            return false;
        frame.tick = tick;
        if (fread(&frame.player_count, 1, 1, file) != 1 ||
            frame.player_count == 0 || frame.player_count > 4 ||
            fread(frame.actions, 1, sizeof(frame.actions), file) != sizeof(frame.actions) ||
            !Get64(file, &frame.checksum)) {
            return false;
        }
        g_frames.push_back(frame);
        previous_tick = tick;
    }
    return !g_frames.empty();
}

bool WriteHeader(void)
{
    if (g_header_written)
        return true;
    if (!SimulationState_Capture(&g_initial_snapshot) ||
        !SimulationState_ValidateRoundTrip(g_initial_snapshot)) {
        SetReplayStatus("Could not capture initial simulation state");
        return false;
    }
    fwrite(kReplayMagic, 1, sizeof(kReplayMagic), g_replay_file);
    Put32(g_replay_file, kReplayVersion);
    Put32(g_replay_file, GAME_CONFIG_SIZE);
    Put32(g_replay_file, static_cast<uint32_t>(g_initial_snapshot.size()));
    fwrite(g_GameConfig.bytes, 1, GAME_CONFIG_SIZE, g_replay_file);
    if (!g_initial_snapshot.empty())
        fwrite(g_initial_snapshot.data(), 1, g_initial_snapshot.size(), g_replay_file);
    g_header_written = ferror(g_replay_file) == 0;
    return g_header_written;
}

} // namespace

bool Replay_InitializeFromArguments(int argc, char **argv)
{
    const char *record_path = ArgumentValue(argc, argv, "--record-replay");
    const char *playback_path = ArgumentValue(argc, argv, "--replay");
    if (record_path == NULL && playback_path == NULL)
        return true;
    if (record_path != NULL && playback_path != NULL) {
        SetReplayStatus("Choose either --record-replay or --replay");
        return false;
    }
    if (Netplay_IsEnabled()) {
        SetReplayStatus("Replay recording/playback cannot be combined with LAN beta yet");
        return false;
    }
    g_saved_config = g_GameConfig;
    g_have_saved_config = true;
    if (record_path != NULL) {
        g_replay_file = fopen(record_path, "wb");
        if (g_replay_file == NULL) {
            SetReplayStatus("Could not create replay file: %s", record_path);
            return false;
        }
        g_replay_mode = REPLAY_RECORD;
        SetReplayStatus("Recording replay to %s", record_path);
    } else {
        g_replay_file = fopen(playback_path, "rb");
        if (g_replay_file == NULL || !ReadReplay(g_replay_file)) {
            if (g_replay_file != NULL)
                fclose(g_replay_file);
            g_replay_file = NULL;
            g_GameConfig = g_saved_config;
            SetReplayStatus("Invalid replay file: %s", playback_path);
            return false;
        }
        fclose(g_replay_file);
        g_replay_file = NULL;
        g_replay_mode = REPLAY_PLAYBACK;
        SetReplayStatus("Loaded %u replay ticks; press Start", static_cast<unsigned>(g_frames.size()));
    }
    SimulationState_SetChecksumInterval(1);
    return true;
}

void Replay_Shutdown(void)
{
    if (g_replay_file != NULL) {
        fflush(g_replay_file);
        fclose(g_replay_file);
        g_replay_file = NULL;
    }
    if (g_have_saved_config && g_replay_mode == REPLAY_PLAYBACK)
        g_GameConfig = g_saved_config;
    g_replay_mode = REPLAY_OFF;
}

bool Replay_IsActive(void) { return g_replay_mode != REPLAY_OFF; }
const char *Replay_Status(void) { return g_replay_status; }

bool Replay_PrepareSimulationTick(uint32_t tick)
{
    if (g_replay_mode == REPLAY_OFF)
        return true;
    if (g_replay_mode == REPLAY_RECORD) {
        if (!WriteHeader())
            return false;
        g_pending_frame = ReplayFrame();
        g_pending_frame.tick = tick;
        g_pending_frame.player_count = static_cast<uint8_t>(DAT_00489240 > 4 ? 4 : DAT_00489240);
        return true;
    }

    if (!g_initial_restored) {
        if (!SimulationState_Restore(g_initial_snapshot.data(), g_initial_snapshot.size())) {
            SetReplayStatus("Initial snapshot does not match the loaded level");
            return false;
        }
        g_initial_restored = true;
        tick = SimulationState_Tick() + 1;
    }
    if (g_frame_index >= g_frames.size()) {
        SetReplayStatus("Replay complete: %u ticks verified", static_cast<unsigned>(g_frames.size()));
        g_SubState = GAMEPLAY_PAUSED;
        return false;
    }
    if (g_frames[g_frame_index].tick != tick) {
        SetReplayStatus("Replay tick mismatch: expected %u, runtime requested %u",
                        g_frames[g_frame_index].tick, tick);
        return false;
    }
    g_pending_frame = g_frames[g_frame_index];
    return true;
}

uint8_t Replay_PlayerActions(int player, uint8_t fallback)
{
    if (g_replay_mode == REPLAY_OFF || player < 0 || player >= 4)
        return fallback;
    if (g_replay_mode == REPLAY_RECORD) {
        g_pending_frame.actions[player] = fallback;
        return fallback;
    }
    if (player >= g_pending_frame.player_count)
        return 0;
    return g_pending_frame.actions[player];
}

void Replay_AfterSimulationTick(uint32_t tick, uint64_t checksum)
{
    if (g_replay_mode == REPLAY_OFF)
        return;
    if (checksum == 0) {
        SetReplayStatus("Could not checksum authoritative state at tick %u", tick);
        g_SubState = GAMEPLAY_PAUSED;
        return;
    }
    if (g_replay_mode == REPLAY_RECORD) {
        Put32(g_replay_file, tick);
        fwrite(&g_pending_frame.player_count, 1, 1, g_replay_file);
        fwrite(g_pending_frame.actions, 1, sizeof(g_pending_frame.actions), g_replay_file);
        Put64(g_replay_file, checksum);
        if (ferror(g_replay_file) != 0) {
            SetReplayStatus("Replay write failed at tick %u", tick);
            g_SubState = GAMEPLAY_PAUSED;
        }
        return;
    }
    if (checksum != g_pending_frame.checksum) {
        SetReplayStatus("DIVERGENCE at tick %u: expected %llx, got %llx", tick,
                        static_cast<unsigned long long>(g_pending_frame.checksum),
                        static_cast<unsigned long long>(checksum));
        g_SubState = GAMEPLAY_PAUSED;
        return;
    }
    ++g_frame_index;
}
