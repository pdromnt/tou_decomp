// Loopback-only multi-process experiment; never the live LAN socket path.
#include "tou.h"
#include "netcode_exchange.h"
#include <algorithm>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET Socket;
const Socket kInvalid = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int Socket;
const Socket kInvalid = -1;
#endif

namespace {
void Close(Socket socket) {
    if (socket == kInvalid) return;
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}
bool WouldBlock() {
#ifdef _WIN32
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}
bool Nonblocking(Socket socket) {
#ifdef _WIN32
    u_long enabled = 1; return ioctlsocket(socket, FIONBIO, &enabled) == 0;
#else
    const int flags = fcntl(socket, F_GETFL, 0); return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}
void Put32(std::vector<uint8_t> *out, uint32_t n) {
    for (int i = 0; i < 4; ++i) out->push_back(static_cast<uint8_t>(n >> (i * 8)));
}
uint32_t Get32(const uint8_t *in) {
    return uint32_t(in[0]) | uint32_t(in[1]) << 8 | uint32_t(in[2]) << 16 | uint32_t(in[3]) << 24;
}
struct Wire {
    Socket socket = kInvalid;
    uint32_t sent_sequence = 0, received_sequence = 0;
    size_t sent = 0;
    uint64_t sent_bytes = 0, received_bytes = 0;
    std::vector<uint8_t> incoming, outgoing;
    ~Wire() { Close(socket); }
    bool flush(NetcodeExchange *exchange) {
        for (int n = 0; n < 16; ++n) {
            if (outgoing.empty()) {
                NetcodeExchange::Message message;
                if (!exchange->take(&message)) return true;
                if (sent_sequence == UINT32_MAX) return false;
                Put32(&outgoing, 0x58454f54); // TOEX, experimental stream version 1.
                Put32(&outgoing, 1u | (static_cast<uint32_t>(message.type) << 16));
                Put32(&outgoing, static_cast<uint32_t>(message.bytes.size()));
                Put32(&outgoing, ++sent_sequence);
                outgoing.insert(outgoing.end(), message.bytes.begin(), message.bytes.end());
            }
            const int count = send(socket, reinterpret_cast<const char *>(outgoing.data() + sent),
                static_cast<int>(std::min(size_t(65536), outgoing.size() - sent)),
#ifdef MSG_NOSIGNAL
                MSG_NOSIGNAL
#else
                0
#endif
            );
            if (count < 0) return WouldBlock();
            if (!count) return false;
            sent += static_cast<size_t>(count); sent_bytes += static_cast<unsigned int>(count);
            if (sent == outgoing.size()) { outgoing.clear(); sent = 0; }
        }
        return true;
    }
    bool receive(NetcodeExchange *exchange, SimulationReconciliation *world, uint32_t now) {
        bool closed = false;
        for (int n = 0; n < 16; ++n) {
            uint8_t bytes[65536];
            const int count = recv(socket, reinterpret_cast<char *>(bytes), sizeof(bytes), 0);
            if (count < 0) { if (!WouldBlock()) return false; break; }
            if (!count) { closed = true; break; }
            if (incoming.size() + static_cast<size_t>(count) > 64u * 1024u * 1024u + 131072u) return false;
            incoming.insert(incoming.end(), bytes, bytes + count); received_bytes += static_cast<unsigned int>(count);
        }
        size_t used = 0;
        for (int n = 0; n < 128 && incoming.size() - used >= 16; ++n) {
            const uint8_t *header = incoming.data() + used;
            const uint32_t combined = Get32(header + 4), length = Get32(header + 8), sequence = Get32(header + 12);
            const NetcodeExchange::Type type = static_cast<NetcodeExchange::Type>(combined >> 16);
            if (Get32(header) != 0x58454f54 || (combined & 65535u) != 1 || !sequence ||
                sequence != received_sequence + 1 || !NetcodeExchange::valid_size(type, length)) return false;
            if (incoming.size() - used - 16 < length) break;
            NetcodeExchange::Message message; message.type = type;
            message.bytes.assign(header + 16, header + 16 + length);
            if (!exchange->receive(message, world, now)) {
                LOG("[EXCHANGE] rejected type=%u phase=%u\n", unsigned(type), unsigned(exchange->phase())); return false;
            }
            received_sequence = sequence; used += 16 + length;
        }
        if (used) incoming.erase(incoming.begin(), incoming.begin() + used);
        return !closed || (exchange->phase() == NetcodeExchange::Closed && incoming.empty());
    }
};
uint8_t Action(uint32_t tick, int player) {
    const uint32_t phase = tick + static_cast<uint32_t>(player) * 37u;
    return static_cast<uint8_t>(GAME_ACTION_FIRE_PRIMARY |
        (phase % 100 < 70 ? GAME_ACTION_THRUST : 0) |
        (phase % 120 < 60 ? GAME_ACTION_TURN_LEFT : GAME_ACTION_TURN_RIGHT) |
        (phase % 80 < 40 ? GAME_ACTION_FIRE_SECONDARY : 0) |
        (phase % 60 == 0 ? GAME_ACTION_DETONATE : 0));
}
Socket Connect(bool host, uint16_t port) {
    sockaddr_in address = {}; address.sin_family = AF_INET; address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (host) {
        const Socket listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == kInvalid) return kInvalid;
        const int enabled = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&enabled), sizeof(enabled));
        if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) || listen(listener, 1)) {
            Close(listener); return kInvalid;
        }
        fd_set read; FD_ZERO(&read); FD_SET(listener, &read); timeval timeout = {5, 0};
        const int ready = select(static_cast<int>(listener + 1), &read, NULL, NULL, &timeout);
        const Socket peer = ready > 0 ? accept(listener, NULL, NULL) : kInvalid;
        Close(listener); return peer;
    }
    const uint32_t start = Platform_GetTicks();
    while (Platform_GetTicks() - start < 5000) {
        const Socket peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (peer == kInvalid) return kInvalid;
        if (!connect(peer, reinterpret_cast<sockaddr *>(&address), sizeof(address))) return peer;
        Close(peer); Platform_Delay(10);
    }
    return kInvalid;
}
}

int ExchangeDiagnostic_Run(int ticks, int window, bool host, uint16_t port, int players, int slot, int controls) {
    if (players < 2 || players > 4 || slot < 1 || slot >= players || controls < 0 || controls > 3 ||
        unsigned(port) + players - 2 > 65535) return 8;
#ifdef _WIN32
    struct SocketScope {
        bool ready;
        SocketScope() { WSADATA data; ready = WSAStartup(MAKEWORD(2, 2), &data) == 0; }
        ~SocketScope() { if (ready) WSACleanup(); }
    } sockets;
    if (!sockets.ready) return 8;
#endif
    Wire wires[3];
    const int peers = host ? players - 1 : 1;
    for (int i = 0; i < peers; ++i) {
        Wire &wire = wires[i];
        wire.socket = Connect(host, static_cast<uint16_t>(port + (host ? i : slot - 1)));
        if (wire.socket == kInvalid || !Nonblocking(wire.socket)) return 8;
        const int enabled = 1;
        setsockopt(wire.socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&enabled), sizeof(enabled));
#ifdef SO_NOSIGPIPE
        setsockopt(wire.socket, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char *>(&enabled), sizeof(enabled));
#endif
    }
    SimulationReconciliation world;
    if (!world.start(1, static_cast<uint8_t>(players), static_cast<uint32_t>(window), 128u * 1024u * 1024u, false,
        host ? SimulationReconciliation::DirectInputs : SimulationReconciliation::HostRevisions)) return 8;
    NetcodeExchange exchange;
    NetcodeHostExchange group;
    if (host ? !group.start(1, 1, static_cast<uint8_t>(players)) :
        !exchange.start(NetcodeExchange::Client, 1, 1, static_cast<uint8_t>(players), static_cast<uint8_t>(slot))) return 8;
    InputAuthorityPublisher publisher;
    if (host && !publisher.reset(1, 1, static_cast<uint8_t>(players))) return 8;
    const uint32_t start = Platform_GetTicks(); uint32_t last = start, sampled = 0, epoch = 1, resyncs = 0, frames = 0;
    bool periodic_sync = false;
    int control_stage = 0;
    uint32_t pause_started = 0, quiesce_started = 0;
    bool pause_seen = false, resume_seen = false, terminal_seen = false;
    struct Delayed { uint32_t tick, due; uint8_t action; };
    std::vector<Delayed> delayed;
    LOG("[EXCHANGE] begin role=%s players=%d slot=%d ticks=%d window=%d port=%u loopback-only=1 dry-run-events=1\n",
        host ? "host" : "client", players, host ? 0 : slot, ticks, window, unsigned(port));
    if (host && !group.begin_sync(world, 2, start)) return 8; // Authoritative tick-zero bootstrap.
    while (Platform_GetTicks() - start < 30000) {
        const uint32_t now = Platform_GetTicks(), elapsed = now - last; last = now;
        for (int i = 0; i < peers; ++i) {
            NetcodeExchange *peer = host ? group.peer(static_cast<uint8_t>(i + 1)) : &exchange;
            if (!peer || !wires[i].receive(peer, &world, now) || !peer->poll(now)) {
                group.disconnect();
                LOG("[EXCHANGE] transport/barrier failed peer=%d tick=%u\n", i + 1, world.simulated_tick()); return 8;
            }
        }
        if (host && !group.poll(&world, now)) return 8;
        if (world.epoch() != epoch) {
            epoch = world.epoch(); sampled = world.simulated_tick(); delayed.clear(); ++resyncs;
            if (host && !publisher.reset(1, epoch, static_cast<uint8_t>(players), world.simulated_tick())) return 8;
            if (controls) {
                if (g_SubState == GAMEPLAY_EXIT_MENU) { pause_seen = true; pause_started = now; }
                if (pause_seen && g_SubState == GAMEPLAY_ACTIVE) resume_seen = true;
                if (g_SubState == GAMEPLAY_LEVEL_ADVANCE) terminal_seen = true;
                LOG("[EXCHANGE] control anchor role=%s epoch=%u tick=%u state=%u level=%u countdown=%u\n",
                    host ? "host" : "client", epoch, world.simulated_tick(), unsigned(g_SubState),
                    unsigned(static_cast<unsigned char>(DAT_0048693c)), unsigned(static_cast<unsigned char>(DAT_004892a5)));
            }
        }
        if (host && group.running() && controls && control_stage == 2 &&
            g_SubState == GAMEPLAY_EXIT_MENU && now - pause_started >= 100) {
            const SimulationReconciliation::Control control = controls == 1 ? SimulationReconciliation::ResumeMatch :
                controls == 2 ? SimulationReconciliation::SkipLevel : SimulationReconciliation::EndMatch;
            if (!group.begin_control(world, epoch + 1, control, now)) return 8;
            control_stage = 3;
        }
        if (host && group.running() && control_stage == 3 && g_SubState == GAMEPLAY_LEVEL_ADVANCE) {
            if (!world.can_finalize()) return 8;
            for (int i = 1; i < players; ++i)
                if (!group.peer(static_cast<uint8_t>(i))->finish(world.simulated_tick(), now)) return 8;
        }
        if ((host ? group.running() : exchange.phase() == NetcodeExchange::Running) && epoch > 1 &&
            g_SubState == GAMEPLAY_ACTIVE) {
            const uint32_t target = static_cast<uint32_t>(ticks);
            if (host && controls && !control_stage && world.simulated_tick() >= target / 4) {
                control_stage = 1;
                quiesce_started = now;
                LOG("[EXCHANGE] quiesce for pause tick=%u confirmed=%u\n", world.simulated_tick(), world.confirmed_tick());
            }
            const bool quiescing = host && control_stage == 1;
            if (quiescing && now - quiesce_started >= 5000) {
                LOG("[EXCHANGE] control quiescence timed out\n"); return 8;
            }
            // Drain already-simulated local input too: a bounded catch-up pump
            // can be ahead of the scripted sampler when quiescence is requested.
            if ((!quiescing || sampled < world.simulated_tick()) && !world.replaying() &&
                sampled < target && sampled < world.simulated_tick() + 1) {
                ++sampled;
                if (host) {
                    if (world.submit(1, sampled, 0, Action(sampled, 0), epoch) != InputTimeline::Accepted) return 8;
                } else {
                    if (delayed.size() >= 64) return 8;
                    delayed.push_back(Delayed{sampled, now + ((sampled + slot) % 7u) * 8u, Action(sampled, slot)});
                }
            }
            for (size_t n = delayed.size(); n > 0; --n)
                if (static_cast<int32_t>(now - delayed[n - 1].due) >= 0) {
                    if (!exchange.receipt(delayed[n - 1].tick, delayed[n - 1].action)) return 8;
                    delayed.erase(delayed.begin() + n - 1);
                }
            const SimulationReconciliation::FrameWork work = world.pump(quiescing ? 0 : elapsed, 3,
                quiescing ? world.simulated_tick() : target);
            if (work.result == SimulationReconciliation::Failed || work.forward_ticks + work.replay_ticks > 3) return 8;
            if (!world.replaying() && !world.dirty_from()) {
                if (host) {
                    for (uint32_t tick = group.peer_confirmed() + 1; tick <= world.simulated_tick(); ++tick) {
                        uint8_t actions[4] = {}; bool sealed = false; AuthoritativeInput input;
                        if (!world.canonical_input(tick, actions, &sealed)) return 8;
                        const InputAuthorityPublisher::Result result = publisher.publish(tick, actions, sealed, &input);
                        if (result != InputAuthorityPublisher::Changed && result != InputAuthorityPublisher::Unchanged) return 8;
                        for (int i = 1; i < players; ++i) {
                            NetcodeExchange *peer = group.peer(static_cast<uint8_t>(i));
                            if (tick > peer->peer_confirmed() && !peer->revision(input, world)) return 8;
                        }
                    }
                }
                if (!world.confirm()) return 8;
                if (!host && !exchange.progress(world)) return 8;
                if (quiescing && world.can_finalize()) {
                    if (!group.begin_control(world, epoch + 1, SimulationReconciliation::OpenPauseMenu, now)) return 8;
                    control_stage = 2;
                } else if (host && !periodic_sync && world.confirmed_tick() >= target / 2) {
                    if (!group.begin_sync(world, epoch + 1, now)) return 8;
                    periodic_sync = true;
                } else if (host && world.confirmed_tick() == target && group.peer_confirmed() == target) {
                    for (int i = 1; i < players; ++i)
                        if (!group.peer(static_cast<uint8_t>(i))->finish(target, now)) return 8;
                }
            }
        } else if (host && group.needs_sync()) {
            if (world.replaying() || world.dirty_from()) {
                if (world.pump(0, 3, world.simulated_tick()).result == SimulationReconciliation::Failed) return 8;
            } else if (world.confirm() && group.begin_sync(world, epoch + 1, now))
                LOG("[EXCHANGE] checksum recovery offered\n");
            else return 8;
        }
        bool closed = true;
        uint64_t sent = 0, received = 0;
        for (int i = 0; i < peers; ++i) {
            NetcodeExchange *peer = host ? group.peer(static_cast<uint8_t>(i + 1)) : &exchange;
            if (!peer || !wires[i].flush(peer)) return 8;
            if (peer->phase() != NetcodeExchange::Closed || !wires[i].outgoing.empty() || peer->queued_bytes()) closed = false;
            sent += wires[i].sent_bytes; received += wires[i].received_bytes;
        }
        ++frames;
        if (closed) {
            if (controls && (!pause_seen || (controls == 1 ? !resume_seen : !terminal_seen))) return 8;
            if (controls > 1 && (g_SubState != GAMEPLAY_LEVEL_ADVANCE || DAT_004892a5 != 0 ||
                static_cast<unsigned char>(DAT_0048693c) != (controls == 3 ? g_GameConfig.values.active_level_count : 0))) return 8;
            if (controls) LOG("[EXCHANGE] controls PASS mode=%d pause=%d resume=%d terminal=%d\n",
                controls, int(pause_seen), int(resume_seen), int(terminal_seen));
            LOG("[EXCHANGE] PASS role=%s tick=%u peer_confirmed=%u epoch=%u resyncs=%u frames=%u corrections=%u replayed=%llu sent=%llu received=%llu\n",
                host ? "host" : "client", world.simulated_tick(), host ? group.peer_confirmed() : exchange.peer_confirmed(), epoch, resyncs, frames,
                world.corrections(), static_cast<unsigned long long>(world.replayed_ticks()),
                static_cast<unsigned long long>(sent), static_cast<unsigned long long>(received));
            return 0;
        }
        Platform_Delay(1);
    }
    LOG("[EXCHANGE] timeout role=%s tick=%u peer=%u\n", host ? "host" : "client",
        world.simulated_tick(), host ? group.peer_confirmed() : exchange.peer_confirmed());
    return 8;
}
