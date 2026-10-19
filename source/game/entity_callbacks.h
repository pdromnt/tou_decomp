#ifndef TOU_ENTITY_CALLBACKS_H
#define TOU_ENTITY_CALLBACKS_H

#include <stdint.h>

void EntityCallbacks_Init(void);
void EntityCallbacks_InitNucleusMarkII(void *entity);
void EntityCallbacks_ApplyStickyWasteSlowdown(int32_t *velocity_x, int32_t *velocity_y);
bool EntityCallbacks_Dispatch(uint32_t callback_address, int entity_index);
void EntityCallbacks_RemoveAt(int entity_index);

#endif
