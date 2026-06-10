#ifndef API_MANIFOLD_H
#define API_MANIFOLD_H

#include "tartarus_pe.h"

extern char GlobalLastAPI[128];

void InitApiManifold();
void BuildUniversalExportTable(UINT8* fake_dll_base, const char* dll_internal_name);

UINT64 UniversalApiTrap(UINT32 api_index);

#endif