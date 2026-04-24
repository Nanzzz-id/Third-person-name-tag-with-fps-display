#pragma once
#include <stdint.h>

namespace pl {
namespace signature {
uintptr_t pl_resolve_signature(const char* signature, const char* lib_name);
} // namespace signature
} // namespace pl
