#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// GlossInit - inisialisasi framework
// pass true agar Gloss siap terima hook sebelum MC load
void GlossInit(bool earlyInit = false);

// GlossOpen - buka library
void* GlossOpen(const char* lib_name);

// GlossSymbol - cari simbol
void* GlossSymbol(void* handle, const char* sym_name);

// GlossHook - hook fungsi
int GlossHook(void* target, void* hook, void** orig);

// GlossGetLibSection - ambil alamat section dari library
// Dipakai untuk scan vtable (seperti di ThirdPersonNametag)
uintptr_t GlossGetLibSection(const char* lib_name,
                              const char* section_name,
                              size_t* out_size);

// Unprotect - buat memory writable (untuk patch vtable)
bool Unprotect(uintptr_t addr, size_t size);

#ifdef __cplusplus
}
#endif
