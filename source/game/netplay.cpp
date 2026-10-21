#include "tou.h"
#include "netplay.h"
#include "presentation_events.h"
#include "input_timeline.h"
#include "netcode_exchange.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET TouSocket;
const TouSocket kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int TouSocket;
const TouSocket kInvalidSocket = -1;
#endif

namespace {

const uint32_t kProtocolMagic = 0x4e554f54u; /* TOUN, little endian */
const uint16_t kProtocolVersion = 6;
/* The sim revision is independent of the app/release version. Bump it whenever
 * a gameplay or snapshot change can make equal commands produce different
 * state, even if the packet shapes remain compatible. Cosmetic releases do
 * not by themselves require a compatibility revision. */
const char kSimulationBuildId[] = "tou-sim3-net6-snapshot5";
const uint16_t kDefaultPort = 27015;
const uint32_t kMaximumPayload = 1024u * 1024u;
const uint32_t kMaximumSnapshotPayload = 64u * 1024u * 1024u;
const size_t kMaximumQueuedBytes = static_cast<size_t>(kMaximumSnapshotPayload) +
                                   4u * 1024u * 1024u;
const uint32_t kDefaultInputDelay = 3;
const uint32_t kMaximumInputDelay = 12;
const uint32_t kChecksumInterval = 60;
const size_t kCommandRingSize = 512;

enum Mode { MODE_OFF, MODE_HOST, MODE_CLIENT };
enum MessageType {
    MESSAGE_HELLO = 1,
    MESSAGE_WELCOME = 2,
    MESSAGE_START = 3,
    MESSAGE_COMMAND = 4,
    MESSAGE_COMMAND_BUNDLE = 5,
    MESSAGE_CHECKSUM = 6,
    MESSAGE_DISCONNECT = 7,
    MESSAGE_GAMEPLAY_STATE = 8,
    MESSAGE_REJECT = 9,
    MESSAGE_PING = 10,
    MESSAGE_PONG = 11,
    MESSAGE_LEVEL_READY = 12,
    MESSAGE_INITIAL_SNAPSHOT = 13,
    MESSAGE_ROSTER = 14,
    MESSAGE_STATE_CORRECTION = 15,
    MESSAGE_INPUT_DELAY = 16,
    MESSAGE_CONTROL_EXCHANGE = 17,
    MESSAGE_PREDICTION_WINDOW = 18
};

enum RejectReason {
    REJECT_SESSION_FULL = 1,
    REJECT_INVALID_PROFILE = 2,
    REJECT_LEVEL_CONTENT = 3,
    REJECT_GAMEPLAY_ASSETS = 4,
    REJECT_SIMULATION_BUILD = 5
};

struct Connection {
    TouSocket socket;
    std::vector<uint8_t> received;
    std::vector<uint8_t> outgoing;
    size_t sent;
    uint32_t last_received_sequence;
    uint32_t connect_started;
    uint32_t last_received_at;
    uint32_t last_ping_at;
    bool ping_pending;
    uint32_t rtt_samples;
    uint32_t rtt_x8;
    uint32_t variation_x4;
    uint8_t slot;
    uint8_t ship;
    uint8_t team;
    bool hello;
    uint32_t ready_generation;
    bool connected;
    bool connecting;

    Connection() : socket(kInvalidSocket), sent(0), last_received_sequence(0),
                   connect_started(0), last_received_at(0), last_ping_at(0),
                   ping_pending(false), rtt_samples(0), rtt_x8(0), variation_x4(0),
                   slot(0xff), ship(0), team(0),
                   hello(false), ready_generation(0), connected(false), connecting(false) {}
};

struct TickCommands {
    uint32_t tick;
    uint32_t rng_state;
    uint64_t rng_calls;
    uint8_t actions[4];
    bool present[4];
    bool bundle;
    uint32_t first_arrived_at;
    bool arrival_recorded;

    TickCommands() : tick(0), rng_state(0), rng_calls(0), bundle(false),
                     first_arrived_at(0), arrival_recorded(false)
    {
        memset(actions, 0, sizeof(actions));
        memset(present, 0, sizeof(present));
    }
};

struct LobbyEntry {
    uint8_t slot;
    uint8_t team;
    uint8_t ship;
    uint8_t ready;
};

Mode g_mode = MODE_OFF;
TouSocket g_listen_socket = kInvalidSocket;
Connection g_server;
std::vector<Connection> g_clients;
TickCommands g_commands[kCommandRingSize];
InputTimeline g_inputs;
struct ControlBarrier {
    uint32_t generation, tick;
    uint8_t state, level, winner, countdown;
};
std::vector<ControlBarrier> g_controls;
uint64_t g_host_checksums[kCommandRingSize] = {};
uint32_t g_host_checksum_ticks[kCommandRingSize] = {};
uint32_t g_last_sampled_tick = 0;
uint32_t g_sequence = 1;
uint8_t g_local_slot = 0;
uint8_t g_player_count = 0;
uint8_t g_requested_team = 0;
bool g_match_active = false;
bool g_network_started = false;
GameConfig g_local_config;
bool g_saved_local_config = false;
struct HostSessionConfig {
    uint8_t player_count;
    uint8_t human_player_count;
    int8_t team_count;
    uint8_t player_difficulty[GAME_CONFIG_PLAYER_CAPACITY];
    uint8_t player_enabled[GAME_CONFIG_PLAYER_CAPACITY];
    uint8_t player_team[GAME_CONFIG_PLAYER_CAPACITY];
    uint8_t player_ship[GAME_CONFIG_PLAYER_CAPACITY];
};
HostSessionConfig g_host_session_config = {};
bool g_saved_host_session_config = false;
int g_last_broadcast_substate = -1;
bool g_local_level_ready = false;
bool g_initial_state_ready = false;
bool g_auto_start = false;
uint32_t g_auto_start_players = 2;
bool g_dump_state = false;
uint32_t g_level_generation = 0;
LobbyEntry g_lobby[4] = {};
uint8_t g_lobby_count = 0;
bool g_correction_applied = false;
bool g_abort_to_lobby = false;
uint32_t g_host_input_delay = kDefaultInputDelay;
uint32_t g_input_delay = kDefaultInputDelay;
uint32_t g_wait_started = 0;
bool g_waiting = false;
uint64_t g_wait_total = 0;
uint32_t g_wait_count = 0;
uint32_t g_wait_max = 0;
uint32_t g_arrival_age = 0;
uint32_t g_last_poll = 0;
bool g_polled = false;
uint32_t g_poll_gap_max = 0;
uint32_t g_last_diagnostic_log = 0;
char g_status[160] = "LAN disabled";
// Explicit protocol-7 opt-in: control barriers only, never live prediction.
bool g_confirmed_controls = false;
bool g_predictive = false;
uint32_t g_host_prediction_window = 8, g_prediction_window = 8;
bool g_handoff_active = false, g_handoff_waiting = false;
uint32_t g_control_epoch = 1, g_handoff_started = 0, g_handoff_tick = 0;
SimulationReconciliation g_control_world;
NetcodeHostExchange g_control_host;
NetcodeExchange g_control_client;
NetcodeExchange::Message g_control_offer;
bool g_control_script = false;
bool g_prediction_input_script = false;
int g_control_script_stage = 0;
uint32_t g_control_script_pause = 0;
bool g_prediction_active = false, g_prediction_closing = false, g_prediction_terminal_ready = false;
uint32_t g_prediction_clock = 0, g_prediction_sampled = 0, g_prediction_pending = 0, g_prediction_pending_started = 0;
uint32_t g_prediction_last_present = 0;
uint64_t g_prediction_events = 0, g_prediction_sounds = 0, g_prediction_prev_events = 0, g_prediction_prev_sounds = 0;
InputAuthorityPublisher g_prediction_publisher;
bool EnsurePrediction();
void PollPrediction();
void HandlePredictionMessage(Connection *connection, const NetcodeExchange::Message &message);

uint16_t ProtocolVersion() { return g_predictive ? 8 : g_confirmed_controls ? 7 : kProtocolVersion; }
size_t EpochBytes() { return g_confirmed_controls ? 4u : 0u; }

void ResetDiagnostics(void)
{
    g_waiting = false;
    g_wait_total = 0;
    g_wait_count = g_wait_max = g_arrival_age = g_poll_gap_max = 0;
    g_polled = false;
    g_last_diagnostic_log = Platform_GetTicks();
}

bool TickReady(bool ready)
{
    const uint32_t now = Platform_GetTicks();
    if (!ready && !g_waiting) {
        g_wait_started = now;
        g_waiting = true;
    } else if (ready && g_waiting) {
        const uint32_t elapsed = now - g_wait_started;
        g_wait_total += elapsed;
        if (elapsed != 0) ++g_wait_count;
        if (elapsed > g_wait_max) g_wait_max = elapsed;
        g_waiting = false;
    }
    return ready;
}

void Put16(std::vector<uint8_t> *out, uint16_t value)
{
    out->push_back(static_cast<uint8_t>(value));
    out->push_back(static_cast<uint8_t>(value >> 8));
}

void Put32(std::vector<uint8_t> *out, uint32_t value)
{
    Put16(out, static_cast<uint16_t>(value));
    Put16(out, static_cast<uint16_t>(value >> 16));
}

void Put64(std::vector<uint8_t> *out, uint64_t value)
{
    Put32(out, static_cast<uint32_t>(value));
    Put32(out, static_cast<uint32_t>(value >> 32));
}

uint64_t HashBytes(uint64_t hash, const void *data, size_t size)
{
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

int CompareNames(const void *left, const void *right)
{
    const char *const lhs = *static_cast<char *const *>(left);
    const char *const rhs = *static_cast<char *const *>(right);
    return strcmp(lhs, rhs);
}

uint64_t HashFile(uint64_t hash, const char *path)
{
    hash = HashBytes(hash, path, strlen(path) + 1);
    FILE *file = fopen(path, "rb");
    const uint8_t present = file != NULL ? 1 : 0;
    hash = HashBytes(hash, &present, sizeof(present));
    if (file == NULL)
        return hash;
    uint8_t buffer[16 * 1024];
    for (;;) {
        const size_t count = fread(buffer, 1, sizeof(buffer), file);
        if (count != 0)
            hash = HashBytes(hash, buffer, count);
        if (count != sizeof(buffer))
            break;
    }
    fclose(file);
    return hash;
}

uint64_t HashDirectoryFiles(uint64_t hash, const char *directory,
                            const char *pattern)
{
    int count = 0;
    char **entries = SDL_GlobDirectory(directory, pattern, 0, &count);
    if (entries == NULL)
        return HashBytes(hash, directory, strlen(directory) + 1);
    qsort(entries, static_cast<size_t>(count), sizeof(*entries), CompareNames);
    for (int i = 0; i < count; ++i) {
        char path[512];
        const int written = snprintf(path, sizeof(path), "%s/%s", directory, entries[i]);
        if (written > 0 && static_cast<size_t>(written) < sizeof(path))
            hash = HashFile(hash, path);
    }
    SDL_free(entries);
    return hash;
}

uint64_t LevelCatalogFingerprint(void)
{
    static uint64_t cached = 0;
    if (cached != 0)
        return cached;
    uint64_t hash = 14695981039346656037ull;
    hash = HashBytes(hash, &DAT_00485088, sizeof(DAT_00485088));
    for (int i = 0; i < DAT_00485088; ++i) {
        const char *name = static_cast<const char *>(DAT_00485090[i]);
        if (name != NULL)
            hash = HashBytes(hash, name, strlen(name) + 1);
        hash = HashBytes(hash, &DAT_00485ea0[i], sizeof(DAT_00485ea0[i]));
        if (name != NULL) {
            char path[512];
            if (DAT_00485ea0[i] == 2) {
                const int written = snprintf(path, sizeof(path), "ggstuff/%s", name);
                if (written > 0 && static_cast<size_t>(written) < sizeof(path))
                    hash = HashDirectoryFiles(hash, path, "*");
            } else {
                const int written = snprintf(path, sizeof(path), "levels/%s.lev", name);
                if (written > 0 && static_cast<size_t>(written) < sizeof(path))
                    hash = HashFile(hash, path);
            }
        }
    }
    cached = hash;
    return cached;
}

uint64_t GameplayAssetFingerprint(void)
{
    static uint64_t cached = 0;
    if (cached != 0)
        return cached;
    static const char *const files[] = {
        "data/all3.gfx", "data/explode.gfx", "data/loadtime.dat",
        "data/pal.col", "data/shipal.col", "data/taulu2.tau",
        "data/names.dat"
    };
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i)
        hash = HashFile(hash, files[i]);
    cached = HashDirectoryFiles(hash, "ships", "*.shp");
    return cached;
}

uint64_t SimulationBuildFingerprint(void)
{
    if (g_predictive) {
        const char id[] = "tou-sim3-net8-snapshot5-prediction1";
        return HashBytes(14695981039346656037ull, id, sizeof(id));
    }
    if (g_confirmed_controls) {
        const char id[] = "tou-sim3-net7-snapshot5-controls1";
        return HashBytes(14695981039346656037ull, id, sizeof(id));
    }
    return HashBytes(14695981039346656037ull, kSimulationBuildId,
                     sizeof(kSimulationBuildId));
}

uint16_t Get16(const uint8_t *bytes)
{
    return static_cast<uint16_t>(bytes[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t Get32(const uint8_t *bytes)
{
    return static_cast<uint32_t>(Get16(bytes)) |
           (static_cast<uint32_t>(Get16(bytes + 2)) << 16);
}

uint64_t Get64(const uint8_t *bytes)
{
    return static_cast<uint64_t>(Get32(bytes)) |
           (static_cast<uint64_t>(Get32(bytes + 4)) << 32);
}

void CloseSocket(TouSocket socket)
{
    if (socket == kInvalidSocket)
        return;
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

bool WouldBlock(void)
{
#ifdef _WIN32
    const int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
#else
    return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS;
#endif
}

bool SetNonBlocking(TouSocket socket)
{
#ifdef _WIN32
    u_long enabled = 1;
    return ioctlsocket(socket, FIONBIO, &enabled) == 0;
#else
    const int flags = fcntl(socket, F_GETFL, 0);
    return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

void SetStatus(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_status, sizeof(g_status), format, args);
    va_end(args);
    LOG("[LAN] %s\n", g_status);
    char title[256];
    snprintf(title, sizeof(title), "%s (Multiplayer: %s)", STR_TITLE, g_status);
    Platform_SetWindowTitle(title);
}

const char *FindArgument(int argc, char **argv, const char *name)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], name) == 0)
            return argv[i + 1];
    }
    return NULL;
}

bool HasArgument(int argc, char **argv, const char *name)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], name) == 0)
            return true;
    }
    return false;
}

bool ParsePort(const char *text, uint16_t *port)
{
    if (text == NULL || text[0] == 0)
        return false;
    char *end = NULL;
    const long value = strtol(text, &end, 10);
    if (*end != 0 || value < 1 || value > 65535)
        return false;
    *port = static_cast<uint16_t>(value);
    return true;
}

bool ParseEndpoint(const char *text, char *host, size_t host_size, uint16_t *port)
{
    if (text == NULL || text[0] == 0)
        return false;
    const char *colon = strrchr(text, ':');
    if (colon == NULL) {
        snprintf(host, host_size, "%s", text);
        *port = kDefaultPort;
        return true;
    }
    const size_t length = static_cast<size_t>(colon - text);
    if (length == 0 || length >= host_size || !ParsePort(colon + 1, port))
        return false;
    memcpy(host, text, length);
    host[length] = 0;
    return true;
}

void Disconnect(Connection *connection, const char *reason);
void DisconnectLocalized(Connection *connection, const char *detail,
                          const char *text_key);

void QueueMessage(Connection *connection, MessageType type,
                  const uint8_t *payload, size_t payload_size)
{
    const size_t limit = type == MESSAGE_CONTROL_EXCHANGE ? static_cast<size_t>(kMaximumSnapshotPayload) + 64u :
                         (type == MESSAGE_INITIAL_SNAPSHOT ||
                          type == MESSAGE_STATE_CORRECTION)
        ? static_cast<size_t>(kMaximumSnapshotPayload) + 8u : kMaximumPayload;
    if (connection == NULL || !connection->connected || payload_size > limit)
        return;
    if (connection->sent != 0) {
        connection->outgoing.erase(connection->outgoing.begin(),
                                   connection->outgoing.begin() + connection->sent);
        connection->sent = 0;
    }
    const size_t message_size = 16u + payload_size;
    if (message_size > kMaximumQueuedBytes ||
        connection->outgoing.size() > kMaximumQueuedBytes - message_size) {
        DisconnectLocalized(connection, "outgoing network queue exceeded safety limit",
                            "lan.disconnect.invalid_data");
        return;
    }
    Put32(&connection->outgoing, kProtocolMagic);
    Put16(&connection->outgoing, ProtocolVersion());
    Put16(&connection->outgoing, static_cast<uint16_t>(type));
    Put32(&connection->outgoing, static_cast<uint32_t>(payload_size));
    Put32(&connection->outgoing, g_sequence++);
    if (payload_size != 0)
        connection->outgoing.insert(connection->outgoing.end(), payload, payload + payload_size);
}

void Disconnect(Connection *connection, const char *reason)
{
    if (connection == NULL || !connection->connected)
        return;
    CloseSocket(connection->socket);
    connection->socket = kInvalidSocket;
    connection->connected = false;
    connection->connecting = false;
    connection->received.clear();
    connection->outgoing.clear();
    connection->sent = 0;
    SetStatus(Text_Get("lan.status.disconnected_format"), reason);
    if (g_match_active) {
        g_SubState = GAMEPLAY_PAUSED;
        g_NeedsRedraw = 1;
        g_abort_to_lobby = true;
    }
}

void Flush(Connection *connection)
{
    if (!connection->connected || connection->connecting ||
        connection->sent >= connection->outgoing.size())
        return;
    const uint8_t *data = connection->outgoing.data() + connection->sent;
    const size_t remaining = connection->outgoing.size() - connection->sent;
    const int chunk = static_cast<int>(remaining < 64u * 1024u
        ? remaining : 64u * 1024u);
    const int result = send(connection->socket, reinterpret_cast<const char *>(data), chunk, 0);
    if (result > 0) {
        connection->sent += static_cast<size_t>(result);
        if (connection->sent == connection->outgoing.size()) {
            connection->outgoing.clear();
            connection->sent = 0;
        }
    } else if (result == 0 || !WouldBlock()) {
        DisconnectLocalized(connection, "send failed",
                            "lan.disconnect.connection_lost");
    }
}

void SendHello(Connection *connection)
{
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>(g_GameConfig.values.player_ship[0] % 9));
    payload.push_back(g_requested_team);
    Put64(&payload, LevelCatalogFingerprint());
    Put64(&payload, GameplayAssetFingerprint());
    Put64(&payload, SimulationBuildFingerprint());
    QueueMessage(connection, MESSAGE_HELLO, payload.data(), payload.size());
}

void RestoreLocalPresentationSettings(void)
{
    g_GameConfig.values.music_enabled = g_local_config.values.music_enabled;
    g_GameConfig.values.sound_enabled = g_local_config.values.sound_enabled;
    g_GameConfig.values.music_volume = g_local_config.values.music_volume;
    g_GameConfig.values.sound_volume = g_local_config.values.sound_volume;
    g_GameConfig.values.resolution_index = g_local_config.values.resolution_index;
    g_GameConfig.values.display_reserved = g_local_config.values.display_reserved;
    g_GameConfig.values.display_detail = g_local_config.values.display_detail;
}

TickCommands *CommandsFor(uint32_t tick)
{
    TickCommands *commands = &g_commands[tick % kCommandRingSize];
    if (commands->tick != tick) {
        *commands = TickCommands();
        commands->tick = tick;
    }
    return commands;
}

void ResetTickTransportState(void)
{
    g_inputs = InputTimeline();
    g_controls.clear();
    for (size_t i = 0; i < kCommandRingSize; ++i)
        g_commands[i] = TickCommands();
    memset(g_host_checksums, 0, sizeof(g_host_checksums));
    memset(g_host_checksum_ticks, 0, sizeof(g_host_checksum_ticks));
    g_last_sampled_tick = 0;
    g_waiting = false;
}

void SeedInputDelay(void)
{
    if (!g_inputs.reset(g_level_generation, g_player_count, 0)) {
        g_abort_to_lobby = true; return;
    }
    for (uint32_t tick = 1; tick <= g_input_delay; ++tick) {
        TickCommands *commands = CommandsFor(tick);
        for (uint8_t slot = 0; slot < g_player_count; ++slot) {
            commands->present[slot] = true;
            g_inputs.submit(g_level_generation, tick, slot, 0);
        }
    }
}

bool IsMessageShapeValid(uint16_t type, size_t size)
{
    if (type == MESSAGE_CONTROL_EXCHANGE)
        return g_confirmed_controls && size >= (g_predictive ? 16u : 22u) && size <= static_cast<size_t>(kMaximumSnapshotPayload) + 64u;
    if (type == MESSAGE_DISCONNECT)
        return size == 1;
    if (type == MESSAGE_PING || type == MESSAGE_PONG)
        return size == 4;
    if (g_mode == MODE_HOST) {
        return (type == MESSAGE_HELLO && size == 26) ||
               (type == MESSAGE_COMMAND && size == 9 + EpochBytes()) ||
               (type == MESSAGE_CHECKSUM && size == 16 + EpochBytes()) ||
               (type == MESSAGE_LEVEL_READY && size == 4);
    }
    if (g_mode == MODE_CLIENT) {
        return (type == MESSAGE_WELCOME && size == 1) ||
               (type == MESSAGE_START && size == 17 + GAME_CONFIG_SIZE + (g_predictive ? 1u : 0u)) ||
               (type == MESSAGE_INPUT_DELAY && size == 4) ||
               (g_predictive && type == MESSAGE_PREDICTION_WINDOW && size == 4) ||
               (type == MESSAGE_COMMAND_BUNDLE && size >= 22 + EpochBytes() && size <= 25 + EpochBytes()) ||
               (!g_confirmed_controls && type == MESSAGE_GAMEPLAY_STATE && size == 12) ||
               (type == MESSAGE_ROSTER && size >= 1 && size <= 17 &&
                ((size - 1) % 4) == 0) ||
               (type == MESSAGE_REJECT && size == 1) ||
               (type == MESSAGE_INITIAL_SNAPSHOT && size > 4 &&
                size <= static_cast<size_t>(kMaximumSnapshotPayload) + 4u) ||
               (type == MESSAGE_STATE_CORRECTION && size > 8 &&
                size <= static_cast<size_t>(kMaximumSnapshotPayload) + 8u);
    }
    return false;
}

uint8_t AvailableLobbySlot(const Connection *joining)
{
    bool used[4] = {};
    used[0] = true;
    for (size_t i = 0; i < g_clients.size(); ++i) {
        const Connection &client = g_clients[i];
        if (&client != joining && client.connected && client.hello && client.slot < 4)
            used[client.slot] = true;
    }
    for (uint8_t slot = 1; slot < 4; ++slot) {
        if (!used[slot])
            return slot;
    }
    return 0xff;
}

void DisconnectLocalized(Connection *connection, const char *detail,
                          const char *text_key)
{
    LOG("[LAN] Disconnect detail: %s\n", detail);
    Disconnect(connection, Text_Get(text_key));
}

void RefreshHostRoster(void)
{
    if (g_mode != MODE_HOST)
        return;
    g_lobby_count = 1;
    g_lobby[0] = {0, 0,
                  static_cast<uint8_t>(g_GameConfig.values.player_ship[0] % 9),
                  static_cast<uint8_t>(!g_match_active || g_local_level_ready)};
    for (size_t i = 0; i < g_clients.size() && g_lobby_count < 4; ++i) {
        const Connection &client = g_clients[i];
        if (!client.connected || !client.hello)
            continue;
        const bool ready = !g_match_active ||
            client.ready_generation == g_level_generation;
        g_lobby[g_lobby_count++] = {client.slot, client.team, client.ship,
                                    static_cast<uint8_t>(ready)};
    }
}

void BroadcastRoster(void)
{
    if (g_mode != MODE_HOST)
        return;
    RefreshHostRoster();
    std::vector<uint8_t> payload;
    payload.push_back(g_lobby_count);
    for (uint8_t i = 0; i < g_lobby_count; ++i) {
        payload.push_back(g_lobby[i].slot);
        payload.push_back(g_lobby[i].team);
        payload.push_back(g_lobby[i].ship);
        payload.push_back(g_lobby[i].ready);
    }
    for (size_t i = 0; i < g_clients.size(); ++i) {
        if (g_clients[i].connected && g_clients[i].hello)
            QueueMessage(&g_clients[i], MESSAGE_ROSTER,
                         payload.data(), payload.size());
    }
}

bool NormalizeHostLobbySlots(void)
{
    uint8_t next_slot = 1;
    bool changed = false;
    for (size_t i = 0; i < g_clients.size(); ++i) {
        Connection &client = g_clients[i];
        if (!client.connected || !client.hello)
            continue;
        if (client.slot != next_slot) {
            client.slot = next_slot;
            changed = true;
            const uint8_t response[1] = {client.slot};
            QueueMessage(&client, MESSAGE_WELCOME, response, sizeof(response));
        }
        ++next_slot;
    }
    return changed;
}

void PruneDisconnectedLobbyClients(void)
{
    if (g_mode != MODE_HOST || g_match_active)
        return;
    bool changed = false;
    for (size_t i = 0; i < g_clients.size();) {
        if (!g_clients[i].connected) {
            g_clients.erase(g_clients.begin() + static_cast<ptrdiff_t>(i));
            changed = true;
        } else {
            ++i;
        }
    }
    changed = NormalizeHostLobbySlots() || changed;
    if (changed)
        BroadcastRoster();
}

void DumpTickCommands(uint32_t tick, const TickCommands &commands)
{
    if (!g_dump_state)
        return;
    const char *path = g_mode == MODE_HOST
        ? "lan-host-actions.txt" : "lan-client-actions.txt";
    FILE *file = fopen(path, tick == 1 ? "w" : "a");
    if (file == NULL)
        return;
    fprintf(file, "%u %u %u %u %u %u %llu\n", static_cast<unsigned>(tick),
            commands.actions[0], commands.actions[1], commands.actions[2],
            commands.actions[3], commands.rng_state,
            static_cast<unsigned long long>(commands.rng_calls));
    fclose(file);
}

const char *RejectReasonText(uint8_t reason)
{
    switch (reason) {
    case REJECT_SESSION_FULL: return Text_Get("lan.reject.session_full");
    case REJECT_INVALID_PROFILE: return Text_Get("lan.reject.invalid_profile");
    case REJECT_LEVEL_CONTENT: return Text_Get("lan.reject.level_content");
    case REJECT_GAMEPLAY_ASSETS: return Text_Get("lan.reject.gameplay_assets");
    case REJECT_SIMULATION_BUILD: return Text_Get("lan.reject.simulation_build");
    default: return Text_Get("lan.reject.unknown");
    }
}

void RejectClient(Connection *connection, RejectReason reason)
{
    const uint8_t payload[1] = {static_cast<uint8_t>(reason)};
    QueueMessage(connection, MESSAGE_REJECT, payload, sizeof(payload));
    Flush(connection);
    Disconnect(connection, RejectReasonText(payload[0]));
}

bool AcceptInput(Connection *connection, InputTimeline::Result result)
{
    if (result == InputTimeline::Accepted || result == InputTimeline::Duplicate) return true;
    if (result != InputTimeline::TooOld && result != InputTimeline::WrongGeneration)
        DisconnectLocalized(connection, "conflicting or out-of-window input", "lan.disconnect.invalid_data");
    return false;
}

bool IsControlState(uint8_t state)
{
    return state == GAMEPLAY_ACTIVE || state == GAMEPLAY_PAUSED ||
        state == GAMEPLAY_EXIT_MENU || state == GAMEPLAY_LEVEL_ADVANCE;
}

void ApplyControlBarriers(void)
{
    while (!g_controls.empty()) {
        const ControlBarrier barrier = g_controls.front();
        if (barrier.generation != g_level_generation) {
            g_controls.erase(g_controls.begin()); continue;
        }
        if (!g_initial_state_ready || !g_inputs.can_finalize(barrier.tick)) return;
        if (g_inputs.simulated_tick() != barrier.tick) {
            DisconnectLocalized(&g_server, "control barrier arrived after its tick", "lan.disconnect.sync_failed");
            g_controls.clear(); return;
        }
        g_controls.erase(g_controls.begin());
        g_SubState = static_cast<GameplaySubState>(barrier.state);
        // Pause/resume is a control event, not a score-state correction. Only
        // explicit skip/end changes the target level and clears victory state.
        if (barrier.state == GAMEPLAY_LEVEL_ADVANCE) {
            DAT_0048693c = (DAT_0048693c & ~0xff) | barrier.level;
            DAT_004892a4 = static_cast<char>(barrier.winner);
            DAT_004892a5 = static_cast<char>(barrier.countdown);
        }
        g_TimerStart = g_FrameTimer = Platform_GetTicks();
        g_TimerAux = 0; g_NeedsRedraw = 1;
        LOG("[NET] control applied generation=%u tick=%u state=%u\n",
            barrier.generation, barrier.tick, static_cast<unsigned int>(barrier.state));
    }
}

void ResetHandoff()
{
    g_control_host.disconnect(); g_control_client.disconnect(); g_control_world.stop();
    g_control_offer = NetcodeExchange::Message();
    g_handoff_active = g_handoff_waiting = false;
    g_prediction_active = g_prediction_closing = g_prediction_terminal_ready = false;
    g_prediction_pending = 0;
}

void PollHandoff()
{
    if (g_predictive) { PollPrediction(); return; }
    if (!g_confirmed_controls || !g_match_active || g_abort_to_lobby) return;
    const uint32_t now = Platform_GetTicks();
    if ((g_handoff_active || g_handoff_waiting) && now - g_handoff_started >= 5000) {
        LOG("[NET] confirmed control deadline expired\n"); g_abort_to_lobby = true; return;
    }
    if (g_handoff_waiting && SimulationState_Tick() >= g_handoff_tick) {
        if (SimulationState_Tick() != g_handoff_tick ||
            !g_control_world.start_live_control(g_level_generation, g_player_count, g_control_epoch,
                                               128u * 1024u * 1024u) ||
            !g_control_client.start(NetcodeExchange::Client, g_level_generation, g_control_epoch,
                                    g_player_count, g_local_slot) ||
            !g_control_client.receive(g_control_offer, &g_control_world, now)) {
            g_abort_to_lobby = true; return;
        }
        g_handoff_active = true; g_handoff_waiting = false;
        g_control_offer = NetcodeExchange::Message();
    }
    if (!g_handoff_active) return;
    if (g_mode == MODE_HOST ? !g_control_host.poll(&g_control_world, now) : !g_control_client.poll(now)) {
        g_abort_to_lobby = true; return;
    }
    const size_t count = g_mode == MODE_HOST ? g_clients.size() : 1;
    for (size_t i = 0; i < count; ++i) {
        Connection *connection = g_mode == MODE_HOST ? &g_clients[i] : &g_server;
        NetcodeExchange *exchange = g_mode == MODE_HOST ? g_control_host.peer(connection->slot) : &g_control_client;
        if (!connection->connected || !exchange) { g_abort_to_lobby = true; return; }
        NetcodeExchange::Message message;
        while (exchange->take(&message)) {
            std::vector<uint8_t> bytes;
            Put16(&bytes, static_cast<uint16_t>(message.type));
            bytes.insert(bytes.end(), message.bytes.begin(), message.bytes.end());
            QueueMessage(connection, MESSAGE_CONTROL_EXCHANGE, bytes.data(), bytes.size());
            if (!connection->connected) { g_abort_to_lobby = true; return; }
        }
        Flush(connection);
    }
    const bool complete = g_mode == MODE_HOST ? g_control_host.running() :
        g_control_client.phase() == NetcodeExchange::Running;
    if (!complete) return;
    const uint32_t base = SimulationState_Tick();
    g_control_epoch = g_control_world.epoch();
    ResetHandoff(); // Borrowed control owners never stop/reset presentation.
    ResetTickTransportState();
    if (!g_inputs.reset(g_level_generation, g_player_count, 0, base)) { g_abort_to_lobby = true; return; }
    for (uint32_t tick = base + 1; tick <= base + g_input_delay; ++tick) {
        TickCommands *commands = CommandsFor(tick);
        for (uint8_t player = 0; player < g_player_count; ++player) {
            commands->present[player] = true;
            if (g_inputs.submit(g_level_generation, tick, player, 0) != InputTimeline::Accepted) {
                g_abort_to_lobby = true; return;
            }
        }
    }
    g_last_sampled_tick = base;
    g_TimerStart = g_FrameTimer = now; g_TimerAux = 0; g_NeedsRedraw = 2;
    LOG("[NET] confirmed control released generation=%u epoch=%u tick=%u state=%u level=%u\n",
        g_level_generation, g_control_epoch, base, unsigned(g_SubState), unsigned(static_cast<unsigned char>(DAT_0048693c)));
}

void HandleMessage(Connection *connection, uint16_t type,
                   const uint8_t *payload, size_t size)
{
    if (type == MESSAGE_CONTROL_EXCHANGE) {
        if (!g_confirmed_controls || !g_match_active || !g_initial_state_ready || !connection->hello) {
            g_abort_to_lobby = true; return;
        }
        NetcodeExchange::Message message; message.type = static_cast<NetcodeExchange::Type>(Get16(payload));
        message.bytes.assign(payload + 2, payload + size);
        if (g_predictive) { HandlePredictionMessage(connection, message); return; }
        const bool allowed = g_mode == MODE_HOST ?
            (message.type == NetcodeExchange::SyncReady || message.type == NetcodeExchange::SyncApplied) :
            (message.type == NetcodeExchange::SyncOffer || message.type == NetcodeExchange::SyncCommit ||
             message.type == NetcodeExchange::SyncResume);
        if (!allowed || !NetcodeExchange::valid_size(message.type, message.bytes.size())) {
            g_abort_to_lobby = true; return;
        }
        if (g_mode == MODE_CLIENT && message.type == NetcodeExchange::SyncOffer) {
            ConfirmedState state;
            if (!AuthorityState_Decode(message.bytes.data(), message.bytes.size(), &state)) { g_abort_to_lobby = true; return; }
            if (state.generation != g_level_generation || state.epoch <= g_control_epoch) return;
            if (g_handoff_active) {
                if (!g_control_client.receive(message, &g_control_world, Platform_GetTicks())) g_abort_to_lobby = true;
                return;
            }
            if (g_handoff_waiting) {
                if (message.bytes != g_control_offer.bytes) g_abort_to_lobby = true;
                return;
            }
            if (state.players != g_player_count ||
                state.tick < SimulationState_Tick() || state.tick - SimulationState_Tick() >= kCommandRingSize ||
                !SimulationState_Validate(state.state.data(), state.state.size(), state.tick)) {
                g_abort_to_lobby = true; return;
            }
            g_control_offer = std::move(message); g_handoff_tick = state.tick;
            g_handoff_started = Platform_GetTicks(); g_handoff_waiting = true;
            return;
        }
        if (Get32(message.bytes.data()) != g_level_generation || Get32(message.bytes.data() + 4) < g_control_epoch) return;
        if (!g_handoff_active) {
            if (Get32(message.bytes.data() + 4) == g_control_epoch) return; // Completed duplicate.
            g_abort_to_lobby = true; return;
        }
        NetcodeExchange *exchange = g_mode == MODE_HOST ? g_control_host.peer(connection->slot) : &g_control_client;
        if (!exchange || !exchange->receive(message, &g_control_world, Platform_GetTicks())) g_abort_to_lobby = true;
        if (!g_abort_to_lobby && g_mode == MODE_CLIENT && message.type == NetcodeExchange::SyncResume)
            PollHandoff(); // Reseed before any following new-epoch bundle in this same read.
        return;
    }
    if (g_mode == MODE_HOST && type == MESSAGE_HELLO && !connection->hello) {
        if (payload[0] >= 9 || payload[1] >= 2) {
            RejectClient(connection, REJECT_INVALID_PROFILE);
            return;
        }
        if (Get64(payload + 2) != LevelCatalogFingerprint()) {
            RejectClient(connection, REJECT_LEVEL_CONTENT);
            return;
        }
        if (Get64(payload + 10) != GameplayAssetFingerprint()) {
            RejectClient(connection, REJECT_GAMEPLAY_ASSETS);
            return;
        }
        if (Get64(payload + 18) != SimulationBuildFingerprint()) {
            RejectClient(connection, REJECT_SIMULATION_BUILD);
            return;
        }
        connection->ship = payload[0];
        connection->team = payload[1];
        connection->slot = AvailableLobbySlot(connection);
        if (connection->slot == 0xff) {
            RejectClient(connection, REJECT_SESSION_FULL);
            return;
        }
        connection->hello = true;
        uint8_t response[1] = {connection->slot};
        QueueMessage(connection, MESSAGE_WELCOME, response, sizeof(response));
        std::vector<uint8_t> delay;
        Put32(&delay, g_host_input_delay);
        QueueMessage(connection, MESSAGE_INPUT_DELAY, delay.data(), delay.size());
        if (g_predictive) {
            std::vector<uint8_t> window; Put32(&window, g_host_prediction_window);
            QueueMessage(connection, MESSAGE_PREDICTION_WINDOW, window.data(), window.size());
        }
        BroadcastRoster();
        SetStatus(Text_Get("lan.status.client_joined_format"),
                  static_cast<unsigned>(connection->slot + 1));
        return;
    }
    if (g_mode == MODE_CLIENT && type == MESSAGE_WELCOME) {
        if (g_confirmed_controls && g_match_active) { g_abort_to_lobby = true; return; }
        if (payload[0] == 0 || payload[0] >= 4) {
            DisconnectLocalized(connection, "invalid player slot",
                                "lan.disconnect.invalid_data");
            return;
        }
        g_local_slot = payload[0];
        g_server.hello = true;
        SetStatus(Text_Get("lan.status.connected_format"),
                  static_cast<unsigned>(g_local_slot + 1));
        return;
    }
    if (g_mode == MODE_CLIENT && type == MESSAGE_REJECT) {
        Disconnect(connection, RejectReasonText(payload[0]));
        return;
    }
    if (g_mode == MODE_CLIENT && type == MESSAGE_ROSTER) {
        const uint8_t count = payload[0];
        if (count == 0 || count > 4 || size != static_cast<size_t>(1 + count * 4)) {
            DisconnectLocalized(connection, "invalid lobby roster",
                                "lan.disconnect.invalid_data");
            return;
        }
        bool used[4] = {};
        for (uint8_t i = 0; i < count; ++i) {
            const uint8_t *entry = payload + 1 + i * 4;
            if (entry[0] >= 4 || entry[1] >= 2 || entry[2] >= 9 ||
                entry[3] > 1 || used[entry[0]]) {
                DisconnectLocalized(connection, "invalid lobby roster entry",
                                    "lan.disconnect.invalid_data");
                return;
            }
            used[entry[0]] = true;
            g_lobby[i] = {entry[0], entry[1], entry[2], entry[3]};
        }
        g_lobby_count = count;
        return;
    }
    if (g_mode == MODE_CLIENT && type == MESSAGE_INPUT_DELAY) {
        const uint32_t delay = Get32(payload);
        if (g_match_active || delay < 1 || delay > kMaximumInputDelay) {
            DisconnectLocalized(connection, "invalid input delay", "lan.disconnect.invalid_data");
            return;
        }
        g_input_delay = delay;
        return;
    }
    if (g_mode == MODE_CLIENT && type == MESSAGE_PREDICTION_WINDOW) {
        const uint32_t window = Get32(payload);
        if (g_match_active || !window || window > InputTimeline::MaximumPrediction) { g_abort_to_lobby = true; return; }
        g_prediction_window = window; return;
    }
    if (g_mode == MODE_CLIENT && type == MESSAGE_START &&
        size == 17 + GAME_CONFIG_SIZE + (g_predictive ? 1u : 0u)) {
        if (g_predictive) {
            g_prediction_window = payload[17 + GAME_CONFIG_SIZE];
            if (!g_prediction_window || g_prediction_window > InputTimeline::MaximumPrediction) { g_abort_to_lobby = true; return; }
        }
        const uint32_t delay = Get32(payload + 13 + GAME_CONFIG_SIZE);
        if (delay < 1 || delay > kMaximumInputDelay) {
            DisconnectLocalized(connection, "invalid match input delay", "lan.disconnect.invalid_data");
            return;
        }
        g_input_delay = delay;
        ResetDiagnostics();
        g_player_count = payload[0];
        if (g_local_slot >= g_player_count || g_player_count < 2 || g_player_count > 4) {
            DisconnectLocalized(connection, "invalid player assignment",
                                "lan.disconnect.invalid_data");
            return;
        }
        const uint32_t rng_state = Get32(payload + 1);
        const uint64_t rng_calls = Get64(payload + 5);
        const uint8_t *config = payload + 13;
        if (!g_saved_local_config) {
            g_local_config = g_GameConfig;
            g_saved_local_config = true;
        }
        unsigned char local_keys[8];
        memcpy(local_keys, g_local_config.values.player_keys[0], sizeof(local_keys));
        const uint8_t pause_key = g_local_config.values.pause_key;
        const uint8_t camera_key = g_local_config.values.camera_key;
        memcpy(g_GameConfig.bytes, config, GAME_CONFIG_SIZE);
        RestoreLocalPresentationSettings();
        if (g_local_slot < 4)
            memcpy(g_GameConfig.values.player_keys[g_local_slot], local_keys, sizeof(local_keys));
        g_GameConfig.values.pause_key = pause_key;
        g_GameConfig.values.camera_key = camera_key;
        TOU_RestoreRandState(rng_state, rng_calls);
        g_match_active = true;
        g_local_level_ready = false;
        g_initial_state_ready = false;
        g_level_generation = 0;
        ResetTickTransportState();
        SimulationState_SetChecksumInterval(kChecksumInterval);
        DAT_0048764a = 1;
        GameState_Transition(GAME_STATE_QUICK_RESTART);
        SetStatus("%s", Text_Get("lan.status.match_starting"));
        return;
    }
    if (g_mode == MODE_HOST && type == MESSAGE_COMMAND && connection->hello && g_initial_state_ready) {
        if (g_predictive) { g_abort_to_lobby = true; return; }
        const uint32_t generation = Get32(payload);
        if (g_confirmed_controls && (g_handoff_active || Get32(payload + 4) != g_control_epoch)) return;
        const uint8_t *command = payload + EpochBytes();
        const uint32_t tick = Get32(command + 4);
        if (!AcceptInput(connection, g_inputs.submit(generation, tick, connection->slot, command[8])))
            return;
        if (tick > SimulationState_Tick() && tick - SimulationState_Tick() < kCommandRingSize) {
            TickCommands *commands = CommandsFor(tick);
            if (!commands->arrival_recorded) {
                commands->first_arrived_at = Platform_GetTicks();
                commands->arrival_recorded = true;
            }
            commands->actions[connection->slot] = command[8];
            commands->present[connection->slot] = true;
        }
        return;
    }
    if (g_mode == MODE_CLIENT && type == MESSAGE_COMMAND_BUNDLE && g_initial_state_ready) {
        if (g_predictive) { g_abort_to_lobby = true; return; }
        const uint32_t generation = Get32(payload);
        if (g_confirmed_controls && (g_handoff_active || Get32(payload + 4) != g_control_epoch)) return;
        const uint8_t *bundle = payload + EpochBytes();
        const uint32_t tick = Get32(bundle + 4);
        const uint8_t count = bundle[20];
        if (generation != g_level_generation) return;
        if (count != g_player_count || size != static_cast<size_t>(21 + count) + EpochBytes()) {
            DisconnectLocalized(connection, "invalid input bundle", "lan.disconnect.invalid_data");
            return;
        }
        for (uint8_t slot = 0; slot < count; ++slot)
            if (!Input_IsValidGameActions(bundle[21 + slot])) {
                DisconnectLocalized(connection, "invalid input action", "lan.disconnect.invalid_data");
                return;
            }
        for (uint8_t slot = 0; slot < count; ++slot)
            if (!AcceptInput(connection, g_inputs.submit(generation, tick, slot, bundle[21 + slot])))
                return;
        if (tick <= SimulationState_Tick()) return;
        TickCommands *commands = CommandsFor(tick);
        const uint32_t rng_state = Get32(bundle + 8);
        const uint64_t rng_calls = Get64(bundle + 12);
        if (commands->bundle && (commands->rng_state != rng_state || commands->rng_calls != rng_calls)) {
            DisconnectLocalized(connection, "conflicting bundle RNG", "lan.disconnect.invalid_data");
            return;
        }
        commands->rng_state = rng_state;
        commands->first_arrived_at = Platform_GetTicks();
        commands->arrival_recorded = true;
        commands->rng_calls = rng_calls;
        commands->bundle = true;
        for (uint8_t slot = 0; slot < count; ++slot) {
            commands->actions[slot] = bundle[21 + slot];
            commands->present[slot] = true;
        }
        return;
    }
    if (g_mode == MODE_HOST && type == MESSAGE_CHECKSUM) {
        if (g_predictive) { g_abort_to_lobby = true; return; }
        if (Get32(payload) != g_level_generation) return;
        if (g_confirmed_controls && (g_handoff_active || Get32(payload + 4) != g_control_epoch)) return;
        const uint8_t *checksum = payload + EpochBytes();
        const uint32_t tick = Get32(checksum + 4);
        const uint64_t remote = Get64(checksum + 8);
        const size_t index = tick % kCommandRingSize;
        if (g_host_checksum_ticks[index] == tick && g_host_checksums[index] != remote) {
            if (g_confirmed_controls) {
                LOG("[NET] confirmed-control session checksum mismatch tick=%u; aborting whole session\n", tick);
                g_abort_to_lobby = true; return;
            }
            std::vector<uint8_t> snapshot;
            if (!SimulationState_Capture(&snapshot) || snapshot.empty() ||
                !SimulationState_ValidateRoundTrip(snapshot) ||
                snapshot.size() > kMaximumSnapshotPayload) {
                SetStatus("%s", Text_Get("lan.status.correction_failed"));
                g_SubState = GAMEPLAY_PAUSED;
                g_NeedsRedraw = 1;
                return;
            }
            std::vector<uint8_t> correction;
            Put32(&correction, g_level_generation);
            Put32(&correction, SimulationState_Tick());
            correction.insert(correction.end(), snapshot.begin(), snapshot.end());
            QueueMessage(connection, MESSAGE_STATE_CORRECTION,
                         correction.data(), correction.size());
            SetStatus(Text_Get("lan.status.correcting_format"),
                      static_cast<unsigned>(connection->slot + 1), tick);
        }
        return;
    }
    if (g_mode == MODE_HOST && type == MESSAGE_LEVEL_READY &&
        g_match_active && connection->hello) {
        const uint32_t generation = Get32(payload);
        if (generation == 0 || generation > g_level_generation + 1) {
            DisconnectLocalized(connection, "invalid level synchronization generation",
                                "lan.disconnect.sync_failed");
            return;
        }
        connection->ready_generation = generation;
        BroadcastRoster();
        SetStatus(Text_Get("lan.status.player_loaded_format"),
                  static_cast<unsigned>(connection->slot + 1));
        return;
    }
    if (g_mode == MODE_CLIENT && type == MESSAGE_INITIAL_SNAPSHOT &&
        g_match_active && g_local_level_ready) {
        if (g_confirmed_controls && g_initial_state_ready) { g_abort_to_lobby = true; return; }
        const uint32_t generation = Get32(payload);
        if (generation != g_level_generation) {
            DisconnectLocalized(connection, "host synchronized the wrong level generation",
                                "lan.disconnect.sync_failed");
            return;
        }
        ResetTickTransportState();
        SeedInputDelay();
        if (!SimulationState_Restore(payload + 4, size - 4)) {
            DisconnectLocalized(connection, "host initial state did not match the loaded level",
                                "lan.disconnect.sync_failed");
            return;
        }
        g_initial_state_ready = true;
        SetStatus("%s", Text_Get("lan.status.synchronized"));
        return;
    }
    if (g_mode == MODE_CLIENT && type == MESSAGE_STATE_CORRECTION &&
        g_match_active && g_initial_state_ready) {
        if (g_confirmed_controls) { g_abort_to_lobby = true; return; }
        const uint32_t generation = Get32(payload);
        const uint32_t tick = Get32(payload + 4);
        if (generation != g_level_generation ||
            !SimulationState_Restore(payload + 8, size - 8) ||
            SimulationState_Tick() != tick) {
            DisconnectLocalized(connection, "host state correction was invalid",
                                "lan.disconnect.sync_failed");
            return;
        }
        ResetTickTransportState();
        g_last_sampled_tick = tick;
        if (!g_inputs.reset(g_level_generation, g_player_count, 0, tick)) {
            DisconnectLocalized(connection, "invalid correction frontier", "lan.disconnect.sync_failed");
            return;
        }
        g_correction_applied = true;
        SetStatus(Text_Get("lan.status.corrected_format"), tick);
        return;
    }
    if (g_mode == MODE_CLIENT && type == MESSAGE_GAMEPLAY_STATE && g_match_active) {
        const uint32_t generation = Get32(payload), tick = Get32(payload + 4);
        if (generation != g_level_generation) return;
        if (!IsControlState(payload[8]) || g_controls.size() >= 16 ||
            (tick > SimulationState_Tick() && tick - SimulationState_Tick() >= kCommandRingSize)) {
            DisconnectLocalized(connection, "invalid control barrier", "lan.disconnect.invalid_data");
            return;
        }
        g_controls.push_back(ControlBarrier{generation, tick, payload[8], payload[9], payload[10], payload[11]});
        ApplyControlBarriers();
        return;
    }
    if (type == MESSAGE_DISCONNECT) {
        DisconnectLocalized(connection, "peer closed session",
                            "lan.disconnect.session_ended");
        return;
    }
    if (type == MESSAGE_PING && size == 4) {
        QueueMessage(connection, MESSAGE_PONG, payload, size);
        return;
    }
    if (type == MESSAGE_PONG && size == 4) {
        if (!connection->ping_pending || Get32(payload) != connection->last_ping_at)
            return; // Ignore stale/unsolicited echoes; no clock synchronization needed.
        const uint32_t sample = Platform_GetTicks() - connection->last_ping_at;
        connection->ping_pending = false;
        if (sample > 10000u) return;
        if (connection->rtt_samples == 0) {
            connection->rtt_x8 = sample * 8;
            connection->variation_x4 = sample * 2;
        } else {
            const uint32_t mean = connection->rtt_x8 / 8;
            const uint32_t deviation = sample > mean ? sample - mean : mean - sample;
            connection->variation_x4 = (connection->variation_x4 * 3) / 4 + deviation;
            connection->rtt_x8 = (connection->rtt_x8 * 7) / 8 + sample;
        }
        if (connection->rtt_samples != UINT32_MAX) ++connection->rtt_samples;
        LOG("[NET] peer=%u sample_rtt=%u rtt=%u variation=%u samples=%u queued=%llu poll_gap_max=%u\n",
            connection == &g_server ? 0u : static_cast<unsigned>(connection->slot),
            sample, connection->rtt_x8 / 8,
            connection->variation_x4 / 4, connection->rtt_samples,
            static_cast<unsigned long long>(connection->outgoing.size() - connection->sent),
            g_poll_gap_max);
        return;
    }
}

void Receive(Connection *connection)
{
    if (!connection->connected || connection->connecting)
        return;
    uint8_t chunk[16 * 1024];
    for (;;) {
        const int result = recv(connection->socket, reinterpret_cast<char *>(chunk), sizeof(chunk), 0);
        if (result > 0) {
            const size_t receive_limit = g_mode == MODE_CLIENT
                ? static_cast<size_t>(kMaximumSnapshotPayload) + 1024u * 1024u + 32u
                : static_cast<size_t>(kMaximumPayload) + 32u;
            if (connection->received.size() > receive_limit - static_cast<size_t>(result)) {
                DisconnectLocalized(connection, "incoming network queue exceeded safety limit",
                                    "lan.disconnect.invalid_data");
                return;
            }
            connection->received.insert(connection->received.end(), chunk, chunk + result);
            connection->last_received_at = Platform_GetTicks();
        } else if (result == 0) {
            DisconnectLocalized(connection, "peer closed connection",
                                "lan.disconnect.connection_lost");
            return;
        } else {
            if (!WouldBlock())
                DisconnectLocalized(connection, "receive failed",
                                    "lan.disconnect.connection_lost");
            break;
        }
    }

    size_t consumed = 0;
    while (connection->received.size() - consumed >= 16) {
        const uint8_t *header = connection->received.data() + consumed;
        const uint32_t magic = Get32(header);
        const uint16_t version = Get16(header + 4);
        const uint16_t type = Get16(header + 6);
        const uint32_t payload_size = Get32(header + 8);
        const uint32_t sequence = Get32(header + 12);
        const uint32_t payload_limit =
            (g_mode == MODE_CLIENT && type == MESSAGE_INITIAL_SNAPSHOT)
                ? kMaximumSnapshotPayload + 4u :
            (g_mode == MODE_CLIENT && type == MESSAGE_CONTROL_EXCHANGE)
                ? kMaximumSnapshotPayload + 64u :
            (g_mode == MODE_CLIENT && type == MESSAGE_STATE_CORRECTION)
                ? kMaximumSnapshotPayload + 8u : kMaximumPayload;
        if (magic != kProtocolMagic || version != ProtocolVersion() ||
            payload_size > payload_limit || sequence == 0 ||
            sequence <= connection->last_received_sequence) {
            DisconnectLocalized(connection, "protocol mismatch",
                                "lan.disconnect.protocol_mismatch");
            return;
        }
        if (connection->received.size() - consumed < 16u + payload_size)
            break;
        if (!IsMessageShapeValid(type, payload_size)) {
            DisconnectLocalized(connection, "invalid protocol message",
                                "lan.disconnect.invalid_data");
            return;
        }
        HandleMessage(connection, type, header + 16, payload_size);
        connection->last_received_sequence = sequence;
        consumed += 16u + payload_size;
        if (!connection->connected)
            return;
    }
    if (consumed != 0)
        connection->received.erase(connection->received.begin(), connection->received.begin() + consumed);
}

bool StartSocketLayer(void)
{
    if (g_network_started)
        return true;
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        return false;
#endif
    g_network_started = true;
    return true;
}

bool StartHost(uint16_t port)
{
    g_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen_socket == kInvalidSocket)
        return false;
    int reuse = 1;
    setsockopt(g_listen_socket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&reuse), sizeof(reuse));
    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(g_listen_socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0 ||
        listen(g_listen_socket, 3) != 0 || !SetNonBlocking(g_listen_socket)) {
        CloseSocket(g_listen_socket);
        g_listen_socket = kInvalidSocket;
        return false;
    }
    SetStatus(Text_Get("lan.status.hosting_format"), static_cast<unsigned>(port));
    return true;
}

bool StartClient(const char *host, uint16_t port)
{
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    char port_text[8];
    snprintf(port_text, sizeof(port_text), "%u", static_cast<unsigned>(port));
    addrinfo *results = NULL;
    if (getaddrinfo(host, port_text, &hints, &results) != 0)
        return false;
    bool started = false;
    for (addrinfo *result = results; result != NULL; result = result->ai_next) {
        TouSocket socket_handle = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (socket_handle == kInvalidSocket || !SetNonBlocking(socket_handle)) {
            CloseSocket(socket_handle);
            continue;
        }
        const int connect_result = connect(socket_handle, result->ai_addr,
                                           static_cast<int>(result->ai_addrlen));
        if (connect_result == 0 || WouldBlock()) {
            g_server.socket = socket_handle;
            g_server.connected = true;
            g_server.connecting = connect_result != 0;
            g_server.connect_started = Platform_GetTicks();
            g_server.last_received_at = g_server.connect_started;
            started = true;
            break;
        }
        CloseSocket(socket_handle);
    }
    freeaddrinfo(results);
    if (started && !g_server.connecting) {
        SendHello(&g_server);
    }
    if (started)
        SetStatus(Text_Get("lan.status.connecting_format"), host, static_cast<unsigned>(port));
    return started;
}

void PollClientConnect(void)
{
    if (!g_server.connected || !g_server.connecting)
        return;
    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(g_server.socket, &writable);
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
#ifdef _WIN32
    const int ready = select(0, NULL, &writable, NULL, &timeout);
#else
    const int ready = select(g_server.socket + 1, NULL, &writable, NULL, &timeout);
#endif
    if (ready <= 0) {
        if (Platform_GetTicks() - g_server.connect_started > 10000u)
            DisconnectLocalized(&g_server, "connection timed out",
                                "lan.disconnect.timed_out");
        return;
    }
    int error = 0;
#ifdef _WIN32
    int length = sizeof(error);
#else
    socklen_t length = sizeof(error);
#endif
    if (getsockopt(g_server.socket, SOL_SOCKET, SO_ERROR,
                   reinterpret_cast<char *>(&error), &length) != 0 || error != 0) {
        DisconnectLocalized(&g_server, "connection failed",
                            "lan.disconnect.connection_lost");
        return;
    }
    g_server.connecting = false;
    SendHello(&g_server);
}

void PollHeartbeat(Connection *connection)
{
    if (!connection->connected || connection->connecting)
        return;
    const uint32_t now = Platform_GetTicks();
    if (now - connection->last_received_at > 10000u) {
        DisconnectLocalized(connection, "peer timed out", "lan.disconnect.timed_out");
        return;
    }
    if (connection->hello && now - connection->last_ping_at >=
            (connection->ping_pending ? 4000u : 2000u)) {
        std::vector<uint8_t> probe;
        Put32(&probe, now);
        QueueMessage(connection, MESSAGE_PING, probe.data(), probe.size());
        connection->last_ping_at = now;
        connection->ping_pending = true;
    }
}

void PollHostAccept(void)
{
    if (g_listen_socket == kInvalidSocket || g_match_active)
        return;
    for (;;) {
        TouSocket accepted = accept(g_listen_socket, NULL, NULL);
        if (accepted == kInvalidSocket) {
            if (!WouldBlock())
                SetStatus("%s", Text_Get("lan.status.host_accept_failed"));
            break;
        }
        if (!SetNonBlocking(accepted)) {
            CloseSocket(accepted);
            continue;
        }
        if (g_clients.size() >= 3) {
            Connection rejected;
            rejected.socket = accepted;
            rejected.connected = true;
            const uint8_t reason[1] = {REJECT_SESSION_FULL};
            QueueMessage(&rejected, MESSAGE_REJECT, reason, sizeof(reason));
            Flush(&rejected);
            CloseSocket(accepted);
            continue;
        }
        Connection connection;
        connection.socket = accepted;
        connection.connected = true;
        connection.last_received_at = Platform_GetTicks();
        connection.last_ping_at = connection.last_received_at;
        connection.slot = 0xff;
        g_clients.push_back(connection);
    }
}

uint8_t SampleLocalActions(void)
{
    if (g_local_slot >= DAT_00489240 || DAT_00487810 == NULL)
        return 0;
    // LAN always uses this machine's Player 1 controller configuration.
    return static_cast<uint8_t>(Input_EncodeGameActions(
        g_KeyboardState, Player_Get(g_local_slot)->key_scan_codes) | Gamepad_PlayerActions(0));
}

bool PredictionTerminal()
{
    return g_SubState == GAMEPLAY_MATCH_COMPLETE || g_SubState == GAMEPLAY_LEVEL_ADVANCE;
}

bool ObservePredictionEpoch()
{
    if (g_control_world.epoch() == g_control_epoch) return true;
    g_control_epoch = g_control_world.epoch();
    const uint32_t base = g_control_world.simulated_tick();
    g_prediction_sampled = base + g_input_delay;
    g_prediction_prev_events = g_prediction_prev_sounds = 0;
    g_prediction_terminal_ready = PredictionTerminal();
    if (g_mode == MODE_HOST) {
        if (!g_prediction_publisher.reset(g_level_generation, g_control_epoch, g_player_count, base)) return false;
        for (uint32_t tick = base + 1; tick <= base + g_input_delay; ++tick)
            for (uint8_t slot = 0; slot < g_player_count; ++slot)
                if (g_control_world.submit(g_level_generation, tick, slot, 0, g_control_epoch) != InputTimeline::Accepted) return false;
    }
    LOG("[NET] prediction anchor role=%s generation=%u epoch=%u tick=%u state=%u\n",
        g_mode == MODE_HOST ? "host" : "client", g_level_generation, g_control_epoch, base, unsigned(g_SubState));
    g_TimerStart = g_FrameTimer = Platform_GetTicks(); g_TimerAux = 0; g_NeedsRedraw = 2;
    return true;
}

bool EnsurePrediction()
{
    if (g_prediction_active) return true;
    if (!g_control_world.start_live_prediction(g_level_generation, g_player_count, g_prediction_window,
            128u * 1024u * 1024u,
            g_mode == MODE_HOST ? SimulationReconciliation::DirectInputs : SimulationReconciliation::HostRevisions)) return false;
    g_prediction_active = true; g_prediction_clock = Platform_GetTicks(); g_control_epoch = 1;
    g_prediction_last_present = g_prediction_clock - 16;
    g_prediction_terminal_ready = false; g_prediction_sampled = 0;
    g_prediction_events = g_prediction_sounds = g_prediction_prev_events = g_prediction_prev_sounds = 0;
    if (g_mode == MODE_HOST) {
        if (!g_control_host.start(g_level_generation, 1, g_player_count) ||
            !g_control_host.begin_sync(g_control_world, 2, g_prediction_clock)) return false;
    } else if (!g_control_client.start(NetcodeExchange::Client, g_level_generation, 1, g_player_count, g_local_slot)) return false;
    g_handoff_active = true;
    LOG("[NET] live prediction started role=%s generation=%u window=%u budget=134217728 work=3\n",
        g_mode == MODE_HOST ? "host" : "client", g_level_generation, g_prediction_window);
    return true;
}

void HandlePredictionMessage(Connection *connection, const NetcodeExchange::Message &message)
{
    if (g_prediction_closing) return; // Old traffic may drain before the next level generation.
    const bool allowed = g_mode == MODE_HOST ?
        (message.type == NetcodeExchange::Receipt || message.type == NetcodeExchange::RevisionAck ||
         message.type == NetcodeExchange::Progress || message.type == NetcodeExchange::SyncReady ||
         message.type == NetcodeExchange::SyncApplied) :
        (message.type == NetcodeExchange::Revision || message.type == NetcodeExchange::ReceiptAck ||
         message.type == NetcodeExchange::SyncOffer || message.type == NetcodeExchange::SyncCommit ||
         message.type == NetcodeExchange::SyncResume);
    if (!allowed || !NetcodeExchange::valid_size(message.type, message.bytes.size())) { g_abort_to_lobby = true; return; }
    if (!EnsurePrediction()) { g_abort_to_lobby = true; return; }
    NetcodeExchange *peer = g_mode == MODE_HOST ? g_control_host.peer(connection->slot) : &g_control_client;
    if (!peer || !peer->receive(message, &g_control_world, Platform_GetTicks()) || !ObservePredictionEpoch()) {
        LOG("[NET] prediction rejected exchange=%u\n", unsigned(message.type)); g_abort_to_lobby = true; return;
    }
    g_handoff_active = g_mode == MODE_HOST ? !g_control_host.running() : g_control_client.phase() != NetcodeExchange::Running;
}

void PollPrediction()
{
    if (!g_predictive || !g_match_active || !g_initial_state_ready || !g_local_level_ready ||
        g_prediction_closing || g_abort_to_lobby || g_GameState != GAME_STATE_GAMEPLAY) return;
    if (!EnsurePrediction()) {
        LOG("[NET] live prediction initialization/budget failed\n"); g_abort_to_lobby = true; return;
    }
    const uint32_t now = Platform_GetTicks(), elapsed = now - g_prediction_clock;
    g_prediction_clock = now;
    if (g_mode == MODE_HOST ? !g_control_host.poll(&g_control_world, now) : !g_control_client.poll(now)) {
        LOG("[NET] prediction barrier failed\n"); g_abort_to_lobby = true; return;
    }
    if (!ObservePredictionEpoch()) { g_abort_to_lobby = true; return; }
    const bool running = g_mode == MODE_HOST ? g_control_host.running() :
        (g_control_client.phase() == NetcodeExchange::Running && g_control_epoch > 1);
    g_handoff_active = !running;
    const bool recovery = g_mode == MODE_HOST && g_control_host.needs_sync();
    const bool quiescing = g_prediction_pending || recovery;
    if (g_prediction_pending && now - g_prediction_pending_started >= 5000) {
        LOG("[NET] prediction control quiescence timed out\n"); g_abort_to_lobby = true; return;
    }
    if (running || recovery) {
        if (running && !g_control_world.replaying() && g_SubState == GAMEPLAY_ACTIVE) {
            const uint32_t sample_to = g_control_world.simulated_tick() + (quiescing ? 0 : g_input_delay + 1);
            const uint8_t sampled_action = SampleLocalActions();
            for (uint32_t n = 0; n < 24 && g_prediction_sampled < sample_to; ++n) {
                const uint32_t tick = g_prediction_sampled + 1;
                const uint32_t phase = tick + g_local_slot * 37u;
                const uint8_t action = !g_prediction_input_script ? sampled_action : static_cast<uint8_t>(
                    GAME_ACTION_FIRE_PRIMARY | (phase % 100 < 70 ? GAME_ACTION_THRUST : 0) |
                    (phase % 120 < 60 ? GAME_ACTION_TURN_LEFT : GAME_ACTION_TURN_RIGHT) |
                    (phase % 80 < 40 ? GAME_ACTION_FIRE_SECONDARY : 0) | (phase % 60 == 0 ? GAME_ACTION_DETONATE : 0));
                if (g_mode == MODE_HOST) {
                    const InputTimeline::Result result = g_control_world.submit(g_level_generation, tick, 0, action, g_control_epoch);
                    if (result != InputTimeline::Accepted && result != InputTimeline::Duplicate) { g_abort_to_lobby = true; return; }
                } else if (!g_control_client.receipt(tick, action)) break; // Bounded backpressure; retry the unsent tick.
                g_prediction_sampled = tick;
            }
        }
        uint32_t target = quiescing ? g_control_world.simulated_tick() : UINT32_MAX;
        if (g_mode == MODE_HOST && !quiescing) target = g_control_host.peer_confirmed() + 32;
        const SimulationReconciliation::FrameWork work = g_control_world.pump(quiescing ? 0 : elapsed, 3, target);
        if (work.result == SimulationReconciliation::Failed) { g_abort_to_lobby = true; return; }
        if (work.forward_ticks) g_prediction_terminal_ready = false;
        if (!g_control_world.replaying() && !g_control_world.dirty_from()) {
            if (g_mode == MODE_HOST && running) {
                for (uint32_t tick = g_control_host.peer_confirmed() + 1; tick <= g_control_world.simulated_tick(); ++tick) {
                    uint8_t actions[4] = {}; bool sealed = false; AuthoritativeInput input;
                    if (!g_control_world.canonical_input(tick, actions, &sealed)) { g_abort_to_lobby = true; return; }
                    const InputAuthorityPublisher::Result result = g_prediction_publisher.publish(tick, actions, sealed, &input);
                    if (result != InputAuthorityPublisher::Changed && result != InputAuthorityPublisher::Unchanged) { g_abort_to_lobby = true; return; }
                    for (uint8_t slot = 1; slot < g_player_count; ++slot) {
                        NetcodeExchange *peer = g_control_host.peer(slot);
                        if (!peer || (tick > peer->peer_confirmed() && !peer->revision(input, g_control_world))) { g_abort_to_lobby = true; return; }
                    }
                }
            }
            if (!g_control_world.confirm()) { g_abort_to_lobby = true; return; }
            const uint64_t events = PresentationEvents_CommittedCount(), sounds = PresentationEvents_CommittedKindCount(PRESENT_SOUND);
            g_prediction_events += events - g_prediction_prev_events; g_prediction_sounds += sounds - g_prediction_prev_sounds;
            g_prediction_prev_events = events; g_prediction_prev_sounds = sounds;
            if (g_mode == MODE_CLIENT && running) {
                if (!g_control_client.progress(g_control_world)) { g_abort_to_lobby = true; return; }
            }
            if (recovery) {
                if (!g_control_host.begin_sync(g_control_world, g_control_epoch + 1, now)) { g_abort_to_lobby = true; return; }
                g_handoff_active = true;
            } else if (g_mode == MODE_HOST && running && g_control_world.can_finalize()) {
                if (g_prediction_pending) {
                    const uint32_t control = g_prediction_pending; g_prediction_pending = 0;
                    if (PredictionTerminal()) {
                        // A corrected natural ending takes priority over an in-flight pause request.
                    } else if (!g_control_host.begin_control(g_control_world, g_control_epoch + 1,
                                   static_cast<SimulationReconciliation::Control>(control), now)) {
                        g_abort_to_lobby = true; return;
                    } else g_handoff_active = true;
                }
                if (!g_handoff_active && PredictionTerminal() && !g_prediction_terminal_ready &&
                    g_control_host.peer_confirmed() == g_control_world.simulated_tick()) {
                    if (!g_control_host.begin_sync(g_control_world, g_control_epoch + 1, now)) { g_abort_to_lobby = true; return; }
                    g_handoff_active = true;
                }
            }
        }
    }
    const size_t count = g_mode == MODE_HOST ? g_clients.size() : 1;
    for (size_t i = 0; i < count; ++i) {
        Connection *connection = g_mode == MODE_HOST ? &g_clients[i] : &g_server;
        NetcodeExchange *peer = g_mode == MODE_HOST ? g_control_host.peer(connection->slot) : &g_control_client;
        if (!peer || !connection->connected) { g_abort_to_lobby = true; return; }
        NetcodeExchange::Message message;
        while (peer->take(&message)) {
            std::vector<uint8_t> bytes; Put16(&bytes, static_cast<uint16_t>(message.type));
            bytes.insert(bytes.end(), message.bytes.begin(), message.bytes.end());
            QueueMessage(connection, MESSAGE_CONTROL_EXCHANGE, bytes.data(), bytes.size());
        }
        Flush(connection);
    }
}

} // namespace

bool Netplay_InitializeFromArguments(int argc, char **argv)
{
    // Settings are loaded before CLI parsing. Only explicit flags override them.
    if (HasArgument(argc, argv, "--lan-prediction"))
        Netplay_SetNetworkMode(NETPLAY_PREDICTED);
    else if (HasArgument(argc, argv, "--lan-confirmed-controls"))
        Netplay_SetNetworkMode(NETPLAY_CONFIRMED);
    g_prediction_input_script = HasArgument(argc, argv, "--lan-scripted-input");
    if (g_prediction_input_script && !g_predictive) { SetStatus("--lan-scripted-input requires --lan-prediction"); return false; }
    g_control_script = HasArgument(argc, argv, "--lan-control-script");
    if (g_control_script && !g_confirmed_controls) {
        SetStatus("--lan-control-script requires --lan-confirmed-controls"); return false;
    }
    const bool host_requested = HasArgument(argc, argv, "--lan-host");
    const char *join = FindArgument(argc, argv, "--lan-join");
    if (!host_requested && join == NULL)
        return true;
    if (host_requested && join != NULL) {
        SetStatus("%s", Text_Get("lan.status.choose_host_or_join"));
        return false;
    }
    const char *team = FindArgument(argc, argv, "--team");
    const int requested_team = team != NULL ? atoi(team) : 1;
    g_auto_start = HasArgument(argc, argv, "--lan-auto-start");
    g_dump_state = HasArgument(argc, argv, "--lan-dump-state");

    if (host_requested) {
        const char *players_arg = FindArgument(argc, argv, "--lan-auto-start-players");
        if (players_arg != NULL) {
            char *end = NULL; const unsigned long count = strtoul(players_arg, &end, 10);
            if (!*players_arg || *end || count < 2 || count > 4) {
                SetStatus("--lan-auto-start-players requires 2-4 total players"); return false;
            }
            g_auto_start_players = static_cast<uint32_t>(count);
        }
        const char *delay_arg = FindArgument(argc, argv, "--lan-input-delay");
        if (delay_arg != NULL) {
            char *end = NULL;
            const unsigned long delay = strtoul(delay_arg, &end, 10);
            if (!*delay_arg || *end || delay < 1 || delay > kMaximumInputDelay ||
                !Netplay_SetInputDelay(static_cast<uint32_t>(delay))) {
                SetStatus("%s", Text_Get("lan.error.input_delay"));
                return false;
            }
        }
        uint16_t port = kDefaultPort;
        const char *port_arg = FindArgument(argc, argv, "--port");
        if (port_arg != NULL && !ParsePort(port_arg, &port))
            return false;
        return Netplay_StartHostSession(port, requested_team);
    }

    char host[256];
    uint16_t port;
    if (!ParseEndpoint(join, host, sizeof(host), &port))
        return false;
    char endpoint[272];
    snprintf(endpoint, sizeof(endpoint), "%s:%u", host, static_cast<unsigned>(port));
    return Netplay_StartClientSession(endpoint, requested_team);
}

bool Netplay_StartHostSession(uint16_t port, int team)
{
    if (g_mode != MODE_OFF)
        return false;
    if (!StartSocketLayer()) {
        SetStatus("%s", Text_Get("lan.status.socket_init_failed"));
        return false;
    }
    g_mode = MODE_HOST;
    g_input_delay = g_host_input_delay;
    ResetDiagnostics();
    g_local_slot = 0;
    g_requested_team = static_cast<uint8_t>(team == 2 ? 1 : 0);
    if (StartHost(port))
        return true;
    SetStatus(Text_Get("lan.status.host_failed_format"), static_cast<unsigned>(port));
    Netplay_Shutdown();
    return false;
}

bool Netplay_StartClientSession(const char *endpoint, int team)
{
    if (g_mode != MODE_OFF)
        return false;
    char host[256];
    uint16_t port;
    if (!ParseEndpoint(endpoint, host, sizeof(host), &port)) {
        SetStatus("%s", Text_Get("lan.status.invalid_address"));
        return false;
    }
    if (!StartSocketLayer()) {
        SetStatus("%s", Text_Get("lan.status.socket_init_failed"));
        return false;
    }
    g_mode = MODE_CLIENT;
    g_requested_team = static_cast<uint8_t>(team == 2 ? 1 : 0);
    if (StartClient(host, port))
        return true;
    SetStatus(Text_Get("lan.status.connect_failed_format"), host, static_cast<unsigned>(port));
    Netplay_Shutdown();
    return false;
}

void Netplay_CancelSession(void)
{
    Netplay_Shutdown();
    Platform_SetWindowTitle(STR_TITLE);
}

// Synchronize while the level preview is visible too: waiting for an active
// simulation tick here would deadlock confirmed-only preview controls.
static void SynchronizeInitialState(void)
{
    if (g_mode != MODE_HOST || !g_match_active || !g_local_level_ready ||
        g_initial_state_ready || g_GameState != GAME_STATE_GAMEPLAY) return;
    for (size_t i = 0; i < g_clients.size(); ++i)
        if (!g_clients[i].connected || g_clients[i].ready_generation != g_level_generation)
            return;
    std::vector<uint8_t> snapshot;
    const bool captured = SimulationState_Capture(&snapshot) && !snapshot.empty();
    const bool round_trip = captured && SimulationState_ValidateRoundTrip(snapshot);
    LOG("[NET] initial_snapshot captured=%d round_trip=%d bytes=%llu\n",
        captured ? 1 : 0, round_trip ? 1 : 0, static_cast<unsigned long long>(snapshot.size()));
    if (!captured || !round_trip || snapshot.size() > kMaximumSnapshotPayload) {
        SetStatus("%s", Text_Get("lan.status.snapshot_failed"));
        g_abort_to_lobby = true; return;
    }
    ResetTickTransportState();
    SeedInputDelay();
    std::vector<uint8_t> payload;
    Put32(&payload, g_level_generation);
    payload.insert(payload.end(), snapshot.begin(), snapshot.end());
    for (size_t i = 0; i < g_clients.size(); ++i)
        QueueMessage(&g_clients[i], MESSAGE_INITIAL_SNAPSHOT, payload.data(), payload.size());
    g_initial_state_ready = true;
    SetStatus("%s", Text_Get("lan.status.synchronized"));
}

void Netplay_Poll(void)
{
    if (g_mode != MODE_OFF) {
        const uint32_t now = Platform_GetTicks();
        if (g_polled && now - g_last_poll > g_poll_gap_max)
            g_poll_gap_max = now - g_last_poll;
        g_polled = true;
        g_last_poll = now;
        if (g_match_active && (g_SubState != GAMEPLAY_ACTIVE || g_handoff_active))
            g_waiting = false; // Intentional pauses are not missing-input stalls.
        if (g_match_active && now - g_last_diagnostic_log >= 2000u) {
            LOG("[NET] role=%s tick=%u delay=%u stalls=%u wait_ms=%llu max_wait=%u current_wait=%u arrival_age=%u poll_gap_max=%u simulated=%u confirmed=%u\n",
                g_mode == MODE_HOST ? "host" : "client", SimulationState_Tick(),
                g_input_delay, g_wait_count, static_cast<unsigned long long>(g_wait_total),
                g_wait_max, g_waiting ? now - g_wait_started : 0u,
                g_arrival_age, g_poll_gap_max,
                g_prediction_active ? g_control_world.simulated_tick() : g_inputs.simulated_tick(),
                g_prediction_active ? g_control_world.confirmed_tick() : g_inputs.confirmed_tick());
            g_last_diagnostic_log = now;
        }
    }
    if (g_mode == MODE_HOST) {
        PruneDisconnectedLobbyClients();
        PollHostAccept();
        for (size_t i = 0; i < g_clients.size(); ++i) {
            Receive(&g_clients[i]);
            PollHeartbeat(&g_clients[i]);
            Flush(&g_clients[i]);
        }
        PruneDisconnectedLobbyClients();
        SynchronizeInitialState();
    } else if (g_mode == MODE_CLIENT) {
        PollClientConnect();
        Receive(&g_server);
        PollHeartbeat(&g_server);
        Flush(&g_server);
        ApplyControlBarriers();
    }
    PollHandoff();
    // Developer-only route probe: actual control API, scoring and level loader;
    // no keyboard injection and never selected by ordinary menu sessions.
    if (g_control_script && g_mode == MODE_HOST && g_match_active && !g_abort_to_lobby &&
        !g_handoff_active && !g_handoff_waiting) {
        const uint32_t now = Platform_GetTicks(), tick = SimulationState_Tick();
        if (g_control_script_stage == 7 && g_GameState == GAME_STATE_GAME_OVER) {
            LOG("[NET] live control script PASS generations=%u level=%u\n", g_level_generation,
                unsigned(static_cast<unsigned char>(DAT_0048693c)));
            g_control_script_stage = 8;
        } else if (g_GameState == GAME_STATE_GAMEPLAY &&
                   (g_predictive ? Netplay_LocalSessionControlsAllowed() : Netplay_ControlAnchorReady(tick))) {
            NetplayControl request = NETPLAY_PAUSE; bool send = false;
            if (!g_control_script_stage && g_SubState == GAMEPLAY_ACTIVE && tick >= 30) {
                g_control_script_stage = 1; g_control_script_pause = now; send = true;
            } else if (g_control_script_stage == 1 && g_SubState == GAMEPLAY_EXIT_MENU && now - g_control_script_pause >= 250) {
                request = NETPLAY_RESUME; g_control_script_stage = 2; send = true;
            } else if (g_control_script_stage == 2 && g_SubState == GAMEPLAY_ACTIVE && tick >= 90) {
                g_control_script_stage = 3; send = true;
            } else if (g_control_script_stage == 3 && g_SubState == GAMEPLAY_EXIT_MENU) {
                request = NETPLAY_SKIP; g_control_script_stage = 4; send = true;
            } else if (g_control_script_stage == 4 && g_level_generation >= 2 && g_SubState == GAMEPLAY_LEVEL_PREVIEW) {
                request = NETPLAY_RESUME; g_control_script_stage = 5; send = true;
            } else if (g_control_script_stage == 5 && g_SubState == GAMEPLAY_ACTIVE && tick >= 60) {
                g_control_script_stage = 6; send = true;
            } else if (g_control_script_stage == 6 && g_SubState == GAMEPLAY_EXIT_MENU) {
                request = NETPLAY_END; g_control_script_stage = 7; send = true;
            }
            if (send) Netplay_RequestControl(request);
        }
    }
    if (g_mode == MODE_HOST && g_match_active && g_SubState != g_last_broadcast_substate) {
        Netplay_BroadcastGameplayStateNow();
        g_last_broadcast_substate = g_SubState;
    }
    if (g_mode == MODE_HOST && g_auto_start && !g_match_active && !g_clients.empty()) {
        bool all_ready = true;
        for (size_t i = 0; i < g_clients.size(); ++i)
            all_ready = all_ready && g_clients[i].connected && g_clients[i].hello;
        if (all_ready && Netplay_ConnectedPlayerCount() >= static_cast<int>(g_auto_start_players) && Netplay_HostStartMatch()) {
            DAT_0048764a = 1;
            GameState_Transition(GAME_STATE_QUICK_RESTART);
        }
    }
    if (g_abort_to_lobby) {
        /* A lockstep match cannot continue after losing a peer. Tear down the
         * whole session so every remaining peer receives DISCONNECT and lands
         * on a usable LAN screen instead of waiting on a missing command
         * forever. This is deferred until polling completes because receive
         * and flush loops hold references into the connection arrays. */
        g_abort_to_lobby = false;
        LOG("[NET] aborting whole session; returning to LAN screen\n");
        g_match_active = false;
        Netplay_Shutdown();
        DAT_004877a4 = 0xF9;
        GameState_Transition(GAME_STATE_RETURN_TO_MENU);
    }
}

void Netplay_Shutdown(void)
{
    ResetHandoff(); g_control_epoch = 1;
    PresentationEvents_Stop();
    if (g_mode == MODE_HOST) {
        for (size_t i = 0; i < g_clients.size(); ++i) {
            uint8_t reason = 0;
            QueueMessage(&g_clients[i], MESSAGE_DISCONNECT, &reason, 1);
            Flush(&g_clients[i]);
            CloseSocket(g_clients[i].socket);
        }
    } else if (g_mode == MODE_CLIENT) {
        uint8_t reason = 0;
        QueueMessage(&g_server, MESSAGE_DISCONNECT, &reason, 1);
        Flush(&g_server);
        CloseSocket(g_server.socket);
    }
    CloseSocket(g_listen_socket);
    g_listen_socket = kInvalidSocket;
    g_clients.clear();
    if (g_saved_local_config && g_mode == MODE_CLIENT)
        g_GameConfig = g_local_config;
    if (g_saved_host_session_config && g_mode == MODE_HOST) {
        g_GameConfig.values.player_count = g_host_session_config.player_count;
        g_GameConfig.values.human_player_count = g_host_session_config.human_player_count;
        g_GameConfig.values.team_count = g_host_session_config.team_count;
        memcpy(g_GameConfig.values.player_difficulty, g_host_session_config.player_difficulty,
               sizeof(g_host_session_config.player_difficulty));
        memcpy(g_GameConfig.values.player_enabled, g_host_session_config.player_enabled,
               sizeof(g_host_session_config.player_enabled));
        memcpy(g_GameConfig.values.player_team, g_host_session_config.player_team,
               sizeof(g_host_session_config.player_team));
        memcpy(g_GameConfig.values.player_ship, g_host_session_config.player_ship,
               sizeof(g_host_session_config.player_ship));
    }
#ifdef _WIN32
    if (g_network_started)
        WSACleanup();
#endif
    g_network_started = false;
    g_mode = MODE_OFF;
    g_server = Connection();
    for (size_t i = 0; i < kCommandRingSize; ++i)
        g_commands[i] = TickCommands();
    memset(g_host_checksums, 0, sizeof(g_host_checksums));
    memset(g_host_checksum_ticks, 0, sizeof(g_host_checksum_ticks));
    g_sequence = 1;
    g_player_count = 0;
    g_match_active = false;
    g_saved_local_config = false;
    g_saved_host_session_config = false;
    g_local_level_ready = false;
    g_initial_state_ready = false;
    g_last_broadcast_substate = -1;
    g_auto_start = false;
    g_auto_start_players = 2;
    g_dump_state = false;
    g_level_generation = 0;
    g_lobby_count = 0;
    memset(g_lobby, 0, sizeof(g_lobby));
    g_correction_applied = false;
    g_abort_to_lobby = false;
    g_input_delay = kDefaultInputDelay;
    ResetDiagnostics();
}

bool Netplay_IsEnabled(void) { return g_mode != MODE_OFF; }
bool Netplay_IsHost(void) { return g_mode == MODE_HOST; }
bool Netplay_IsClient(void) { return g_mode == MODE_CLIENT; }
bool Netplay_IsMatchActive(void) { return g_match_active; }
const char *Netplay_Status(void) { return g_status; }

uint32_t Netplay_InputDelay(void)
{
    return g_mode == MODE_CLIENT ? g_input_delay : g_host_input_delay;
}

bool Netplay_SetInputDelay(uint32_t ticks)
{
    if (g_match_active || g_mode == MODE_CLIENT || ticks < 1 || ticks > kMaximumInputDelay)
        return false;
    g_host_input_delay = g_input_delay = ticks;
    if (g_mode == MODE_HOST) {
        std::vector<uint8_t> payload;
        Put32(&payload, ticks);
        for (size_t i = 0; i < g_clients.size(); ++i)
            if (g_clients[i].hello)
                QueueMessage(&g_clients[i], MESSAGE_INPUT_DELAY, payload.data(), payload.size());
    }
    return true;
}

void Netplay_FormatDiagnostics(char *out, size_t out_size)
{
    uint32_t rtt = 0, variation = 0;
    bool measured = false;
    const size_t count = g_mode == MODE_HOST ? g_clients.size() : 1;
    for (size_t i = 0; i < count && g_mode != MODE_OFF; ++i) {
        const Connection &peer = g_mode == MODE_HOST ? g_clients[i] : g_server;
        if (!peer.connected || !peer.hello || peer.rtt_samples == 0) continue;
        measured = true;
        if (peer.rtt_x8 / 8 > rtt) rtt = peer.rtt_x8 / 8;
        if (peer.variation_x4 / 4 > variation) variation = peer.variation_x4 / 4;
    }
    if (out == NULL || out_size == 0) return;
    if (measured)
        snprintf(out, out_size, Text_Get("lan.diagnostics_format"), rtt, variation);
    else
        snprintf(out, out_size, "%s", Text_Get("lan.diagnostics_pending"));
}

void Netplay_BroadcastGameplayStateNow(void)
{
    if (g_confirmed_controls) return;
    if (g_mode != MODE_HOST || !g_match_active || !g_initial_state_ready ||
        g_GameState != GAME_STATE_GAMEPLAY ||
        !IsControlState(static_cast<uint8_t>(g_SubState)) ||
        !Netplay_CanFinalizeSimulationTick(SimulationState_Tick()))
        return;
    std::vector<uint8_t> payload;
    Put32(&payload, g_level_generation);
    Put32(&payload, SimulationState_Tick());
    payload.push_back(static_cast<uint8_t>(g_SubState));
    payload.push_back(static_cast<uint8_t>(DAT_0048693c));
    payload.push_back(static_cast<uint8_t>(DAT_004892a4));
    payload.push_back(static_cast<uint8_t>(DAT_004892a5));
    for (size_t i = 0; i < g_clients.size(); ++i) {
        if (!g_clients[i].connected || !g_clients[i].hello)
            continue;
        QueueMessage(&g_clients[i], MESSAGE_GAMEPLAY_STATE, payload.data(), payload.size());
        Flush(&g_clients[i]);
    }
    g_last_broadcast_substate = g_SubState;
}

int Netplay_ConnectedPlayerCount(void)
{
    if (g_mode == MODE_OFF)
        return 0;
    int count = 1;
    if (g_mode == MODE_HOST) {
        for (size_t i = 0; i < g_clients.size(); ++i) {
            if (g_clients[i].connected && g_clients[i].hello)
                ++count;
        }
    } else if (g_lobby_count != 0) {
        count = g_lobby_count;
    }
    return count;
}

void Netplay_FormatRoster(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
        return;
    out[0] = 0;
    if (g_mode == MODE_OFF)
        return;
    if (g_mode == MODE_HOST)
        RefreshHostRoster();
    for (uint8_t i = 0; i < g_lobby_count; ++i) {
        const LobbyEntry &entry = g_lobby[i];
        const size_t used = strlen(out);
        if (used >= out_size)
            break;
        snprintf(out + used, out_size - used, Text_Get("lan.roster_entry_format"),
                 used == 0 ? "" : " | ",
                 static_cast<unsigned>(entry.slot + 1),
                 static_cast<unsigned>(entry.team + 1),
                 Ship_DisplayName(entry.ship),
                 Text_Get(entry.ready ? "lan.ready" : "lan.loading"));
    }
}

bool Netplay_HostStartMatch(void)
{
    if (g_mode == MODE_OFF)
        return true;
    if (g_mode != MODE_HOST)
        return false;

    std::vector<Connection *> ready;
    for (size_t i = 0; i < g_clients.size(); ++i) {
        if (!g_clients[i].connected)
            continue;
        if (!g_clients[i].hello) {
            Platform_ShowError(Text_Get("lan.error.client_not_ready"));
            return false;
        }
        ready.push_back(&g_clients[i]);
    }
    if (ready.empty()) {
        Platform_ShowError(Text_Get("lan.error.no_clients"));
        return false;
    }

    if (!g_saved_host_session_config) {
        g_host_session_config.player_count = g_GameConfig.values.player_count;
        g_host_session_config.human_player_count = g_GameConfig.values.human_player_count;
        g_host_session_config.team_count = g_GameConfig.values.team_count;
        memcpy(g_host_session_config.player_difficulty, g_GameConfig.values.player_difficulty,
               sizeof(g_host_session_config.player_difficulty));
        memcpy(g_host_session_config.player_enabled, g_GameConfig.values.player_enabled,
               sizeof(g_host_session_config.player_enabled));
        memcpy(g_host_session_config.player_team, g_GameConfig.values.player_team,
               sizeof(g_host_session_config.player_team));
        memcpy(g_host_session_config.player_ship, g_GameConfig.values.player_ship,
               sizeof(g_host_session_config.player_ship));
        g_saved_host_session_config = true;
    }

    g_player_count = static_cast<uint8_t>(ready.size() + 1);
    g_GameConfig.values.player_count = g_player_count;
    g_GameConfig.values.human_player_count = g_player_count;
    g_GameConfig.values.team_count = 2;
    for (uint8_t slot = 0; slot < g_player_count; ++slot) {
        g_GameConfig.values.player_enabled[slot] = 1;
        g_GameConfig.values.player_difficulty[slot] = 0;
    }
    g_GameConfig.values.player_team[0] = 0;
    for (size_t i = 0; i < ready.size(); ++i) {
        Connection *client = ready[i];
        client->slot = static_cast<uint8_t>(i + 1);
        g_GameConfig.values.player_ship[client->slot] = client->ship;
        g_GameConfig.values.player_team[client->slot] = client->team;
    }

    std::vector<uint8_t> payload;
    g_input_delay = g_host_input_delay;
    ResetDiagnostics();
    payload.reserve(17 + GAME_CONFIG_SIZE);
    payload.push_back(g_player_count);
    Put32(&payload, TOU_RandState());
    Put64(&payload, TOU_RandCallCount());
    payload.insert(payload.end(), g_GameConfig.bytes, g_GameConfig.bytes + GAME_CONFIG_SIZE);
    Put32(&payload, g_input_delay);
    if (g_predictive) {
        g_prediction_window = g_host_prediction_window;
        payload.push_back(static_cast<uint8_t>(g_prediction_window));
    }
    for (size_t i = 0; i < ready.size(); ++i)
        QueueMessage(ready[i], MESSAGE_START, payload.data(), payload.size());

    g_match_active = true;
    g_control_script_stage = 0;
    g_last_broadcast_substate = -1;
    g_last_sampled_tick = 0;
    g_local_level_ready = false;
    g_initial_state_ready = false;
    g_level_generation = 0;
    for (size_t i = 0; i < ready.size(); ++i)
        ready[i]->ready_generation = 0;
    ResetTickTransportState();
    SimulationState_SetChecksumInterval(kChecksumInterval);
    BroadcastRoster();
    SetStatus(Text_Get("lan.status.starting_players_format"),
              static_cast<unsigned>(g_player_count));
    return true;
}

void Netplay_PreparePlayersBeforeInit(void)
{
    if (!g_match_active || DAT_00487810 == NULL)
        return;
    for (int i = 0; i < DAT_00489240; ++i)
        Player_Get(i)->human_controlled = (i == g_local_slot) ? 1 : 0;
}

void Netplay_FinalizePlayersAfterInit(void)
{
    if (!g_match_active || DAT_00487810 == NULL)
        return;
    for (int i = 0; i < DAT_00489240; ++i) {
        Player_Get(i)->ai_level = 0;
        Player_Get(i)->human_controlled = (i == g_local_slot) ? 1 : 0;
    }
}

void Netplay_OnLevelLoaded(void)
{
    if (!g_match_active)
        return;
    ResetHandoff(); g_control_epoch = 1;
    ++g_level_generation;
    LOG("[NET] level loaded role=%s generation=%u level=%u\n", g_mode == MODE_HOST ? "host" : "client",
        g_level_generation, unsigned(static_cast<unsigned char>(DAT_0048693c)));
    g_last_broadcast_substate = -1;
    ResetDiagnostics();
    g_local_level_ready = true;
    g_initial_state_ready = false;
    ResetTickTransportState();
    if (g_mode == MODE_CLIENT) {
        std::vector<uint8_t> payload;
        Put32(&payload, g_level_generation);
        QueueMessage(&g_server, MESSAGE_LEVEL_READY, payload.data(), payload.size());
        Flush(&g_server);
        SetStatus("%s", Text_Get("lan.status.level_loaded_client"));
    } else {
        BroadcastRoster();
        SetStatus("%s", Text_Get("lan.status.level_loaded_host"));
    }
}

bool Netplay_PrepareSimulationTick(uint32_t tick)
{
    if (g_predictive && g_match_active) return false; // The owner is the sole live tick driver.
    if (!g_match_active)
        return true;
    Netplay_Poll();

    if (g_abort_to_lobby || g_mode == MODE_OFF || g_handoff_active ||
        (g_handoff_waiting && tick > g_handoff_tick)) return false;

    if (g_SubState != GAMEPLAY_ACTIVE) return false;

    if (g_correction_applied) {
        g_correction_applied = false;
        return false;
    }

    if (!g_local_level_ready)
        return false;
    if (!g_initial_state_ready) return false;

    const uint32_t future_tick = tick + g_input_delay;
    if (future_tick > g_last_sampled_tick) {
        const uint8_t actions = SampleLocalActions();
        if (g_mode == MODE_HOST) {
            const InputTimeline::Result result = g_inputs.submit(g_level_generation, future_tick, g_local_slot, actions);
            if (result != InputTimeline::Accepted && result != InputTimeline::Duplicate) {
                LOG("[NET] local input ledger failed\n"); g_abort_to_lobby = true; return false;
            }
            TickCommands *future = CommandsFor(future_tick);
            future->actions[g_local_slot] = actions;
            future->present[g_local_slot] = true;
        } else {
            std::vector<uint8_t> payload;
            Put32(&payload, g_level_generation);
            if (g_confirmed_controls) Put32(&payload, g_control_epoch);
            Put32(&payload, future_tick);
            payload.push_back(actions);
            QueueMessage(&g_server, MESSAGE_COMMAND, payload.data(), payload.size());
            Flush(&g_server);
        }
        g_last_sampled_tick = future_tick;
    }

    TickCommands *commands = CommandsFor(tick);
    if (g_mode == MODE_HOST) {
        if (!g_inputs.prepare(tick, commands->actions)) return TickReady(false);
        commands->rng_state = TOU_RandState();
        commands->rng_calls = TOU_RandCallCount();
        commands->bundle = true;
        std::vector<uint8_t> payload;
        Put32(&payload, g_level_generation);
        if (g_confirmed_controls) Put32(&payload, g_control_epoch);
        Put32(&payload, tick);
        Put32(&payload, commands->rng_state);
        Put64(&payload, commands->rng_calls);
        payload.push_back(g_player_count);
        payload.insert(payload.end(), commands->actions, commands->actions + g_player_count);
        for (size_t i = 0; i < g_clients.size(); ++i)
            QueueMessage(&g_clients[i], MESSAGE_COMMAND_BUNDLE, payload.data(), payload.size());
        DumpTickCommands(tick, *commands);
        g_arrival_age = commands->arrival_recorded
            ? Platform_GetTicks() - commands->first_arrived_at : 0;
        return TickReady(true);
    }

    if (!commands->bundle)
        return TickReady(false);
    if (!g_inputs.prepare(tick, commands->actions)) return TickReady(false);
    TOU_RestoreRandState(commands->rng_state, commands->rng_calls);
    DumpTickCommands(tick, *commands);
    g_arrival_age = commands->arrival_recorded
        ? Platform_GetTicks() - commands->first_arrived_at : 0;
    return TickReady(true);
}

uint8_t Netplay_PlayerActions(int player, uint8_t local_fallback)
{
    if (!g_match_active || player < 0 || player >= g_player_count)
        return local_fallback;
    TickCommands *commands = CommandsFor(SimulationState_Tick() + 1);
    return commands->actions[player];
}

bool Netplay_LocalSessionControlsAllowed(void)
{
    if (g_predictive && g_match_active)
        return g_mode == MODE_HOST && g_prediction_active && !g_prediction_closing &&
            !g_handoff_active && !g_prediction_pending && !g_control_world.replaying() && !g_control_world.dirty_from();
    return !g_match_active || (g_mode != MODE_CLIENT &&
        Netplay_CanFinalizeSimulationTick(SimulationState_Tick()));
}

bool Netplay_UsesConfirmedControls(void) { return g_confirmed_controls && g_match_active; }
NetplayNetworkMode Netplay_NetworkMode(void) {
    return g_predictive ? NETPLAY_PREDICTED : g_confirmed_controls ? NETPLAY_CONFIRMED : NETPLAY_LOCKSTEP;
}
bool Netplay_SetNetworkMode(NetplayNetworkMode mode) {
    if (g_mode != MODE_OFF || mode < NETPLAY_LOCKSTEP || mode > NETPLAY_PREDICTED) return false;
    g_predictive = mode == NETPLAY_PREDICTED; g_confirmed_controls = mode != NETPLAY_LOCKSTEP; return true;
}
uint32_t Netplay_PredictionWindow(void) { return g_mode == MODE_CLIENT ? g_prediction_window : g_host_prediction_window; }
uint32_t Netplay_PreferredInputDelay(void) { return g_host_input_delay; }
uint32_t Netplay_PreferredPredictionWindow(void) { return g_host_prediction_window; }
void Netplay_ResetPreferences(void) {
    if (g_mode != MODE_OFF) Netplay_CancelSession();
    Netplay_SetNetworkMode(NETPLAY_LOCKSTEP);
    Netplay_SetInputDelay(kDefaultInputDelay);
    Netplay_SetPredictionWindow(8);
}
bool Netplay_SetPredictionWindow(uint32_t ticks) {
    if (g_match_active || g_mode == MODE_CLIENT || !ticks || ticks > InputTimeline::MaximumPrediction) return false;
    g_host_prediction_window = ticks;
    if (g_mode == MODE_HOST && g_predictive) {
        std::vector<uint8_t> window; Put32(&window, ticks);
        for (size_t i = 0; i < g_clients.size(); ++i) if (g_clients[i].hello)
            QueueMessage(&g_clients[i], MESSAGE_PREDICTION_WINDOW, window.data(), window.size());
    }
    return true;
}
bool Netplay_ControlAnchorReady(uint32_t tick)
{
    return !g_predictive && Netplay_UsesConfirmedControls() && g_GameState == GAME_STATE_GAMEPLAY && !g_abort_to_lobby &&
        g_local_level_ready && g_initial_state_ready && !g_handoff_active &&
        tick == SimulationState_Tick() && tick == g_inputs.simulated_tick() && g_inputs.can_finalize(tick);
}
bool Netplay_IsPredicting(void) { return g_predictive && g_match_active; }
bool Netplay_PredictionCanStart(void) {
    return Netplay_IsPredicting() && !g_prediction_active && !g_prediction_closing && !g_abort_to_lobby &&
        g_GameState == GAME_STATE_GAMEPLAY && g_local_level_ready && g_initial_state_ready && SimulationState_Tick() == 0;
}
bool Netplay_RenderAllowed(void) {
    if (g_handoff_active) return false;
    if (g_prediction_active && !g_prediction_closing)
        return !g_control_world.replaying() && !g_control_world.dirty_from() &&
            Platform_GetTicks() - g_prediction_last_present >= 16;
    return true;
}
void Netplay_FramePresented(void) { if (g_prediction_active) g_prediction_last_present = Platform_GetTicks(); }
void Netplay_ReleaseTransition(void) {
    if (!g_predictive || !g_prediction_active) return;
    LOG("[NET] prediction released for scoring generation=%u epoch=%u tick=%u corrections=%u replayed=%llu events=%llu sounds=%llu\n",
        g_level_generation, g_control_epoch, SimulationState_Tick(), g_control_world.corrections(),
        static_cast<unsigned long long>(g_control_world.replayed_ticks()),
        static_cast<unsigned long long>(g_prediction_events), static_cast<unsigned long long>(g_prediction_sounds));
    g_control_world.stop(); g_control_host.disconnect(); g_control_client.disconnect();
    g_prediction_active = false; g_prediction_closing = true;
}
bool Netplay_RequestControl(NetplayControl control)
{
    if (!Netplay_UsesConfirmedControls()) return false;
    if (g_predictive) {
        if (g_mode == MODE_HOST && g_prediction_active && !g_prediction_pending && control >= NETPLAY_PAUSE && control <= NETPLAY_END) {
            g_prediction_pending = control; g_prediction_pending_started = Platform_GetTicks();
            LOG("[NET] prediction quiesce control=%u tick=%u confirmed=%u\n", unsigned(control),
                g_control_world.simulated_tick(), g_control_world.confirmed_tick());
        }
        return true;
    }
    if (g_mode != MODE_HOST || g_handoff_active || g_handoff_waiting) return true;
    const uint32_t now = Platform_GetTicks();
    if (!Netplay_ControlAnchorReady(SimulationState_Tick()) || control < NETPLAY_PAUSE || control > NETPLAY_END ||
        !g_control_world.start_live_control(g_level_generation, g_player_count, g_control_epoch, 128u * 1024u * 1024u) ||
        !g_control_host.start(g_level_generation, g_control_epoch, g_player_count) ||
        !g_control_host.begin_control(g_control_world, g_control_epoch + 1,
                                     static_cast<SimulationReconciliation::Control>(control), now)) {
        LOG("[NET] could not prepare confirmed control=%u\n", unsigned(control));
        g_abort_to_lobby = true; return true;
    }
    g_handoff_active = true; g_handoff_started = now;
    LOG("[NET] confirmed control requested=%u generation=%u epoch=%u tick=%u\n",
        unsigned(control), g_level_generation, g_control_epoch, SimulationState_Tick());
    PollHandoff();
    return true;
}

bool Netplay_CanFinalizeSimulationTick(uint32_t tick)
{
    if (g_predictive && g_match_active) {
        if (g_prediction_closing) return true;
        return g_prediction_active && !g_handoff_active && !g_prediction_pending &&
            tick == g_control_world.simulated_tick() && g_control_world.can_finalize() &&
            (!PredictionTerminal() || g_prediction_terminal_ready);
    }
    return !g_match_active || (g_initial_state_ready && !g_handoff_active && !g_handoff_waiting && g_inputs.can_finalize(tick));
}

void Netplay_AfterSimulationTick(uint32_t tick, uint64_t checksum)
{
    if (!g_match_active) return;
    if (!g_inputs.simulated(tick)) {
        LOG("[NET] simulated tick is outside the prepared input frontier\n");
        g_abort_to_lobby = true; return;
    }
    if (g_mode == MODE_CLIENT) ApplyControlBarriers();
    if (checksum == 0) return;
    if ((tick % kChecksumInterval) != 0)
        return;
    if (g_dump_state) {
        std::vector<uint8_t> snapshot;
        if (SimulationState_Capture(&snapshot)) {
            char path[64];
            snprintf(path, sizeof(path), "lan-%s-%u.snap",
                     g_mode == MODE_HOST ? "host" : "client",
                     static_cast<unsigned>(tick));
            FILE *file = fopen(path, "wb");
            if (file != NULL) {
                fwrite(snapshot.data(), 1, snapshot.size(), file);
                fclose(file);
            }
        }
    }
    if (g_mode == MODE_HOST) {
        const size_t index = tick % kCommandRingSize;
        g_host_checksum_ticks[index] = tick;
        g_host_checksums[index] = checksum;
    } else {
        std::vector<uint8_t> payload;
        Put32(&payload, g_level_generation);
        if (g_confirmed_controls) Put32(&payload, g_control_epoch);
        Put32(&payload, tick);
        Put64(&payload, checksum);
        QueueMessage(&g_server, MESSAGE_CHECKSUM, payload.data(), payload.size());
    }
}
