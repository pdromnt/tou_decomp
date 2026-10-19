#ifndef TOU_SIMULATION_STATE_H
#define TOU_SIMULATION_STATE_H

#include <stdint.h>
#include <stddef.h>
#include <vector>

/* Canonical, presentation-free state used by replay diagnostics and LAN
 * correction. Snapshot payloads are only accepted for an already-loaded copy
 * of the same level dimensions; resource and asset loading stays outside the
 * simulation boundary. */
void SimulationState_Reset(void);
void SimulationState_SetChecksumInterval(uint32_t ticks);
uint32_t SimulationState_Tick(void);
uint64_t SimulationState_Checksum(void);
uint64_t SimulationState_OnTickComplete(void);
bool SimulationState_Capture(std::vector<uint8_t> *snapshot);
bool SimulationState_Restore(const uint8_t *snapshot, size_t size);
bool SimulationState_ValidateRoundTrip(const std::vector<uint8_t> &snapshot);

#endif
