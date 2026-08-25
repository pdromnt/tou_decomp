#ifndef TOU_NETPLAY_H
#define TOU_NETPLAY_H

#include <stdint.h>
#include <stddef.h>

bool Netplay_InitializeFromArguments(int argc, char **argv);
bool Netplay_StartHostSession(uint16_t port, int team);
bool Netplay_StartClientSession(const char *endpoint, int team);
void Netplay_CancelSession(void);
void Netplay_Poll(void);
void Netplay_Shutdown(void);
bool Netplay_IsEnabled(void);
bool Netplay_IsHost(void);
bool Netplay_IsClient(void);
bool Netplay_IsMatchActive(void);
void Netplay_BroadcastGameplayStateNow(void);
bool Netplay_HostStartMatch(void);
void Netplay_PreparePlayersBeforeInit(void);
void Netplay_FinalizePlayersAfterInit(void);
void Netplay_OnLevelLoaded(void);
bool Netplay_PrepareSimulationTick(uint32_t tick);
uint8_t Netplay_PlayerActions(int player, uint8_t local_fallback);
void Netplay_AfterSimulationTick(uint32_t tick, uint64_t checksum);
bool Netplay_LocalSessionControlsAllowed(void);
const char *Netplay_Status(void);
int Netplay_ConnectedPlayerCount(void);
void Netplay_FormatRoster(char *out, size_t out_size);

#endif
