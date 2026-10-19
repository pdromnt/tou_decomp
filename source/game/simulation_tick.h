#ifndef TOU_SIMULATION_TICK_H
#define TOU_SIMULATION_TICK_H
#include <stddef.h>
#include <stdint.h>

// Advance exactly one tick with supplied actions, without pacing or net/replay
// completion hooks. NULL retains the live input path at its original call site.
// False means the original trooper path ended the enclosing catch-up batch.
// Caller owns flag resets: legacy batch policy locally, canonical per-tick in LAN.
// Tick count advances here. The optional event timeline is experimental and is
// not enabled in live multiplayer yet.
bool Gameplay_AdvanceSimulation(const uint8_t *actions, size_t count);
uint8_t Gameplay_PlayerActions(int player, const unsigned char *bindings);

// Developer-only local rewind experiment; never active in ordinary play.
int SimulationRewind_Run(int argc, char **argv, const char *level);
#endif
