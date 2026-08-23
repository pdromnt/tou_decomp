#ifndef TOU_ACCURACY_RUNTIME_H
#define TOU_ACCURACY_RUNTIME_H

#include <stdint.h>

void Accuracy_InitEntityCallbackTable(void);
void Accuracy_InitNucleusMarkIIEntity(void *entity);
void Accuracy_ApplyStickyWasteSlowdown(int32_t *velocity_x, int32_t *velocity_y);
bool Accuracy_DispatchEntityCallback(uint32_t callback_address, int entity_index);
void Accuracy_RemoveEntityAt(int entity_index);

#endif
