#ifndef TOU_SIMULATION_TICK_H
#define TOU_SIMULATION_TICK_H
#include <stddef.h>
#include <stdint.h>

// Advance exactly one tick with supplied actions, without pacing or net/replay
// completion hooks. NULL retains the live input path at its original call site.
// False means the original trooper path ended the enclosing catch-up batch.
// Caller owns the legacy once-per-batch flag reset. Tick count advances here.
// Audio/VFX side effects are not yet reconciled for live rollback.
bool Gameplay_AdvanceSimulation(const uint8_t *actions, size_t count);
uint8_t Gameplay_PlayerActions(int player, const unsigned char *bindings);

// Developer-only local rewind experiment; never active in ordinary play.
int SimulationRewind_Run(int argc, char **argv, const char *level);
#endif
