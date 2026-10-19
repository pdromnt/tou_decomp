#ifndef TOU_NETCODE_EXCHANGE_H
#define TOU_NETCODE_EXCHANGE_H
#include "simulation_reconciliation.h"
#include <deque>

// Experimental one-peer reliable/ordered exchange. No socket or world ownership.
class NetcodeExchange {
public:
    NetcodeExchange() = default;
    NetcodeExchange(const NetcodeExchange &) = delete;
    NetcodeExchange &operator=(const NetcodeExchange &) = delete;
    enum Role { Host, Client };
    enum Phase { Running, NeedsSync, AwaitReady, Prepared, AwaitApplied, AwaitResume, Closing, Closed, Failed,
                 HostReady, HostApplied };
    enum Type { Receipt = 1, ReceiptAck, Revision, RevisionAck, Progress,
                SyncOffer, SyncReady, SyncCommit, SyncApplied, SyncResume, End, EndAck };
    struct Message { Type type = Receipt; std::vector<uint8_t> bytes; };
    bool start(Role role, uint32_t generation, uint32_t epoch, uint8_t players, uint8_t client_slot);
    bool receipt(uint32_t tick, uint8_t actions);
    bool revision(const AuthoritativeInput &input, const SimulationReconciliation &world);
    bool progress(const SimulationReconciliation &world);
    bool begin_sync(const SimulationReconciliation &world, uint32_t epoch, uint32_t now);
    bool finish(uint32_t tick, uint32_t now);
    bool receive(const Message &message, SimulationReconciliation *world, uint32_t now);
    bool poll(uint32_t now);
    bool take(Message *message);
    void disconnect();
    Phase phase() const { return phase_; }
    uint32_t epoch() const { return epoch_; }
    uint32_t peer_confirmed() const { return peer_confirmed_; }
    size_t queued_bytes() const { return queued_bytes_; }
    static bool valid_size(Type type, size_t size);
private:
    friend class NetcodeHostExchange;
    bool offer(const ConfirmedState &state, uint32_t now);
    struct ReceiptSlot { uint32_t tick = 0; uint8_t actions = 0; bool valid = false, acked = false; };
    struct RevisionSlot { AuthoritativeInput input; uint64_t hash = 0; bool acked = false; };
    bool queue(Type type, std::vector<uint8_t> bytes);
    bool stamp(Type type);
    bool same_stamp(const std::vector<uint8_t> &bytes) const;
    bool fail();
    void reset_history(uint32_t base);
    Role role_ = Host;
    Phase phase_ = Failed;
    uint32_t generation_ = 0, epoch_ = 0, peer_confirmed_ = 0, last_progress_ = 0, deadline_start_ = 0;
    uint8_t players_ = 0, slot_ = 0;
    uint64_t peer_hash_ = 0;
    ReceiptSlot receipts_[64];
    RevisionSlot revisions_[64];
    ConfirmedState pending_;
    std::deque<Message> outgoing_;
    size_t queued_bytes_ = 0;
    bool coordinated_ = false;
};
// One shared host world, up to three peers. Failure of any member aborts all.
class NetcodeHostExchange {
public:
    bool start(uint32_t generation, uint32_t epoch, uint8_t players);
    bool begin_sync(const SimulationReconciliation &world, uint32_t epoch, uint32_t now);
    bool poll(SimulationReconciliation *world, uint32_t now);
    bool begin_control(const SimulationReconciliation &world, uint32_t epoch,
                       SimulationReconciliation::Control control, uint32_t now);
    void disconnect();
    bool running() const;
    bool needs_sync() const;
    uint32_t peer_confirmed() const;
    NetcodeExchange *peer(uint8_t slot);
private:
    bool offer_all(const ConfirmedState &state, uint32_t now);
    NetcodeExchange peers_[3];
    ConfirmedState pending_;
    uint8_t count_ = 0;
    bool active_ = false, syncing_ = false, installed_ = false;
};
// Loopback-only developer harness. Never used by live LAN.
int ExchangeDiagnostic_Run(int ticks, int window, bool host, uint16_t port, int players, int slot, int controls);
#endif
