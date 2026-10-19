// Loopback-only two-process experiment; never the live LAN socket path.
#include "tou.h"
#include "netcode_exchange.h"
#include <algorithm>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
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

int ExchangeDiagnostic_Run(int ticks, int window, bool host, uint16_t port) {
#ifdef _WIN32
    struct SocketScope {
        bool ready;
        SocketScope() { WSADATA data; ready = WSAStartup(MAKEWORD(2, 2), &data) == 0; }
        ~SocketScope() { if (ready) WSACleanup(); }
    } sockets;
    if (!sockets.ready) return 8;
#endif
    Wire wire; wire.socket = Connect(host, port);
    if (wire.socket == kInvalid || !Nonblocking(wire.socket)) return 8;
    const int enabled = 1;
    setsockopt(wire.socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&enabled), sizeof(enabled));
#ifdef SO_NOSIGPIPE
    setsockopt(wire.socket, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char *>(&enabled), sizeof(enabled));
#endif
    SimulationReconciliation world;
    if (!world.start(1, 2, static_cast<uint32_t>(window), 128u * 1024u * 1024u, false,
        host ? SimulationReconciliation::DirectInputs : SimulationReconciliation::HostRevisions)) return 8;
    NetcodeExchange exchange;
    if (!exchange.start(host ? NetcodeExchange::Host : NetcodeExchange::Client, 1, 1, 2, 1)) return 8;
    InputAuthorityPublisher publisher;
    if (host && !publisher.reset(1, 1, 2)) return 8;
    const uint32_t start = Platform_GetTicks(); uint32_t last = start, sampled = 0, epoch = 1, resyncs = 0, frames = 0;
    bool periodic_sync = false;
    struct Delayed { uint32_t tick, due; uint8_t action; };
    std::vector<Delayed> delayed;
    LOG("[EXCHANGE] begin role=%s ticks=%d window=%d port=%u loopback-only=1 dry-run-events=1\n",
        host ? "host" : "client", ticks, window, unsigned(port));
    if (host && !exchange.begin_sync(world, 2, start)) return 8; // Authoritative tick-zero bootstrap.
    while (Platform_GetTicks() - start < 30000) {
        const uint32_t now = Platform_GetTicks(), elapsed = now - last; last = now;
        if (!wire.receive(&exchange, &world, now) || !exchange.poll(now)) {
            LOG("[EXCHANGE] transport/barrier failed phase=%u tick=%u\n", unsigned(exchange.phase()), world.simulated_tick()); return 8;
        }
        if (world.epoch() != epoch) {
            epoch = world.epoch(); sampled = world.simulated_tick(); delayed.clear(); ++resyncs;
            if (host && !publisher.reset(1, epoch, 2, world.simulated_tick())) return 8;
        }
        if (exchange.phase() == NetcodeExchange::Running && epoch > 1) {
            const uint32_t target = static_cast<uint32_t>(ticks);
            if (!world.replaying() && sampled < target && sampled < world.simulated_tick() + 1) {
                ++sampled;
                if (host) {
                    if (world.submit(1, sampled, 0, Action(sampled, 0), epoch) != InputTimeline::Accepted) return 8;
                } else {
                    if (delayed.size() >= 64) return 8;
                    delayed.push_back(Delayed{sampled, now + (sampled % 7u) * 8u, Action(sampled, 1)});
                }
            }
            for (size_t n = delayed.size(); n > 0; --n)
                if (static_cast<int32_t>(now - delayed[n - 1].due) >= 0) {
                    if (!exchange.receipt(delayed[n - 1].tick, delayed[n - 1].action)) return 8;
                    delayed.erase(delayed.begin() + n - 1);
                }
            const SimulationReconciliation::FrameWork work = world.pump(elapsed, 3, target);
            if (work.result == SimulationReconciliation::Failed || work.forward_ticks + work.replay_ticks > 3) return 8;
            if (!world.replaying() && !world.dirty_from()) {
                if (host) {
                    for (uint32_t tick = exchange.peer_confirmed() + 1; tick <= world.simulated_tick(); ++tick) {
                        uint8_t actions[4] = {}; bool sealed = false; AuthoritativeInput input;
                        if (!world.canonical_input(tick, actions, &sealed)) return 8;
                        const InputAuthorityPublisher::Result result = publisher.publish(tick, actions, sealed, &input);
                        if ((result != InputAuthorityPublisher::Changed && result != InputAuthorityPublisher::Unchanged) ||
                            !exchange.revision(input, world)) return 8;
                    }
                }
                if (!world.confirm()) return 8;
                if (!host && !exchange.progress(world)) return 8;
                if (host && !periodic_sync && world.confirmed_tick() >= target / 2) {
                    if (!exchange.begin_sync(world, epoch + 1, now)) return 8;
                    periodic_sync = true;
                } else if (host && world.confirmed_tick() == target && exchange.peer_confirmed() == target) {
                    if (!exchange.finish(target, now)) return 8;
                }
            }
        } else if (exchange.phase() == NetcodeExchange::NeedsSync) {
            if (world.replaying() || world.dirty_from()) {
                if (world.pump(0, 3, world.simulated_tick()).result == SimulationReconciliation::Failed) return 8;
            } else if (world.confirm() && exchange.begin_sync(world, epoch + 1, now))
                LOG("[EXCHANGE] checksum recovery offered\n");
            else return 8;
        }
        if (!wire.flush(&exchange)) return 8;
        ++frames;
        if (exchange.phase() == NetcodeExchange::Closed && wire.outgoing.empty() && !exchange.queued_bytes()) {
            LOG("[EXCHANGE] PASS role=%s tick=%u peer_confirmed=%u epoch=%u resyncs=%u frames=%u corrections=%u replayed=%llu sent=%llu received=%llu\n",
                host ? "host" : "client", world.simulated_tick(), exchange.peer_confirmed(), epoch, resyncs, frames,
                world.corrections(), static_cast<unsigned long long>(world.replayed_ticks()),
                static_cast<unsigned long long>(wire.sent_bytes), static_cast<unsigned long long>(wire.received_bytes));
            return 0;
        }
        Platform_Delay(1);
    }
    LOG("[EXCHANGE] timeout phase=%u tick=%u peer=%u\n", unsigned(exchange.phase()), world.simulated_tick(), exchange.peer_confirmed());
    return 8;
}
