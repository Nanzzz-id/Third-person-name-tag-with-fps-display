#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void      GlossInit(bool earlyInit = false);
void*     GlossOpen(const char* lib_name);
void*     GlossSymbol(void* handle, const char* sym_name);
int       GlossHook(void* target, void* hook, void** orig);
uintptr_t GlossGetLibSection(const char* lib, const char* section, size_t* out_size);
bool      Unprotect(uintptr_t addr, size_t size);

#ifdef __cplusplus
}
#endif
