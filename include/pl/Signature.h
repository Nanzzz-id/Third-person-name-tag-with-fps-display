#pragma once
#include <stdint.h>

namespace pl {
namespace signature {

// Cari fungsi Minecraft pakai byte pattern
// lib_name: nama library (misal "libminecraftpe.so")
// signature: string hex pattern, "?" = wildcard
// return: alamat fungsi, atau 0 jika tidak ditemukan
uintptr_t pl_resolve_signature(const char* signature,
                                const char* lib_name);

} // namespace signature
} // namespace pl
