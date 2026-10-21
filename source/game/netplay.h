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
enum NetplayControl { NETPLAY_PAUSE = 1, NETPLAY_RESUME, NETPLAY_SKIP, NETPLAY_END };
bool Netplay_UsesConfirmedControls(void);
// True means the opt-in path consumed the request; never fall back on failure.
bool Netplay_RequestControl(NetplayControl control);
bool Netplay_RenderAllowed(void);
bool Netplay_ControlAnchorReady(uint32_t tick);
enum NetplayNetworkMode { NETPLAY_LOCKSTEP = 0, NETPLAY_CONFIRMED, NETPLAY_PREDICTED };
NetplayNetworkMode Netplay_NetworkMode(void);
bool Netplay_SetNetworkMode(NetplayNetworkMode mode);
uint32_t Netplay_PredictionWindow(void);
bool Netplay_SetPredictionWindow(uint32_t ticks);
// Saved local host preferences, even while connected to a different host.
uint32_t Netplay_PreferredInputDelay(void);
uint32_t Netplay_PreferredPredictionWindow(void);
void Netplay_ResetPreferences(void);
bool Netplay_IsPredicting(void);
bool Netplay_PredictionCanStart(void);
void Netplay_ReleaseTransition(void);
void Netplay_FramePresented(void);
// True through the confirmed input frontier and only after a control handoff.
bool Netplay_CanFinalizeSimulationTick(uint32_t tick);
const char *Netplay_Status(void);
int Netplay_ConnectedPlayerCount(void);
void Netplay_FormatRoster(char *out, size_t out_size);
// Persistent host preference. Fixed for the entire match, in simulation ticks.
bool Netplay_SetInputDelay(uint32_t ticks);
uint32_t Netplay_InputDelay(void);
void Netplay_FormatDiagnostics(char *out, size_t out_size);

#endif
