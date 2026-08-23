#ifndef TOU_ACCURACY_RUNTIME_H
#define TOU_ACCURACY_RUNTIME_H

#include <stdint.h>

void Accuracy_InitEntityCallbackTable(void);
void Accuracy_InitNucleusMarkIIEntity(void *entity);
bool Accuracy_DispatchEntityCallback(uint32_t callback_address, int entity_index);
void Accuracy_RemoveEntityAt(int entity_index);

#endif
