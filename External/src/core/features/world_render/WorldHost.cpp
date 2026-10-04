#include "WorldRender.h"
#include "../../../memory/memory.h"
void* world_render::host_get_process_handle(){return memory->GetHandle();}
uint64_t world_render::host_get_module_base(){return memory->get_module_address();}
