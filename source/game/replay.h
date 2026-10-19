#ifndef TOU_REPLAY_H
#define TOU_REPLAY_H

#include <stdint.h>

bool Replay_InitializeFromArguments(int argc, char **argv);
void Replay_Shutdown(void);
bool Replay_PrepareSimulationTick(uint32_t tick);
uint8_t Replay_PlayerActions(int player, uint8_t fallback);
void Replay_AfterSimulationTick(uint32_t tick, uint64_t checksum);
bool Replay_IsActive(void);
const char *Replay_Status(void);

#endif
