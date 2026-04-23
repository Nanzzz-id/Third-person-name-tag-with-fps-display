/**
 * FPSTagMod - Tambahkan FPS dan Ping di nametag pemain
 *
 * Struktur mengikuti libThirdPersonNametag.so persis:
 *  - Pakai pl/Gloss.h dan pl/Signature.h
 *  - Hook via GlossInit(true) + hookVtable
 *  - __attribute__((constructor)) sebagai entry point
 *
 * Cara kerja:
 *  - Hook fungsi render nametag Minecraft
 *  - Inject teks FPS dan ping ke string nametag
 *  - FPS dihitung dari interval render frame
 *  - Ping diambil dari fungsi MC (atau estimasi)
 *
 * Hasil tampilan: "Nanzzz12 (60fps)" atau "Nanzzz12 (0ms)"
 */

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <chrono>
#include <atomic>
#include <sys/mman.h>

#include <android/log.h>
#include "pl/Gloss.h"
#include "pl/Signature.h"

#define TAG "FPSTagMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static constexpr const char* MCPE_LIB = "libminecraftpe.so";

// ═══════════════════════════════════════════
//  FPS COUNTER
// ═══════════════════════════════════════════
static std::atomic<int>   g_fps{0};
static std::atomic<int>   g_ping{0};

// Hitung FPS dari frame interval
static void updateFPS() {
    using namespace std::chrono;
    static auto lastTime = steady_clock::now();
    static int  frameCount = 0;
    static int  cachedFPS  = 0;

    frameCount++;
    auto now     = steady_clock::now();
    auto elapsed = duration_cast<milliseconds>(now - lastTime).count();

    if (elapsed >= 1000) {
        cachedFPS  = (int)(frameCount * 1000.0f / elapsed);
        frameCount = 0;
        lastTime   = now;
        g_fps.store(cachedFPS);
    }
}

// ═══════════════════════════════════════════
//  HELPER: PATCH MEMORY (sama dengan ThirdPersonNametag)
// ═══════════════════════════════════════════
static bool PatchMemory(void* addr, const void* data, size_t size) {
    uintptr_t page_start = (uintptr_t)addr & ~(4095UL);
    size_t    page_size  = ((uintptr_t)addr + size - page_start + 4095) & ~(4095UL);
    if (mprotect((void*)page_start, page_size,
                 PROT_READ|PROT_WRITE|PROT_EXEC) != 0)
        return false;
    memcpy(addr, data, size);
    __builtin___clear_cache((char*)addr, (char*)addr + size);
    mprotect((void*)page_start, page_size, PROT_READ|PROT_EXEC);
    return true;
}

// ═══════════════════════════════════════════
//  HELPER: HOOK VTABLE (sama dengan ThirdPersonNametag)
// ═══════════════════════════════════════════
static bool hookVtable(const char* cls, int slot,
                        void** outOrig, void* hookFn) {
    size_t rodataSize = 0;
    uintptr_t rodata = GlossGetLibSection(MCPE_LIB, ".rodata", &rodataSize);
    if (!rodata || !rodataSize) {
        LOGE("hookVtable: no .rodata for %s", cls);
        return false;
    }

    auto scan = [](uintptr_t base, size_t sz,
                   const void* pat, size_t plen) -> uintptr_t {
        auto* m = (const uint8_t*)base;
        auto* p = (const uint8_t*)pat;
        for (size_t i = 0; i+plen <= sz; ++i)
            if (memcmp(m+i, p, plen) == 0) return base+i;
        return 0;
    };

    uintptr_t zts = scan(rodata, rodataSize, cls, strlen(cls)+1);
    if (!zts) { LOGE("hookVtable: ZTS not found for %s", cls); return false; }

    size_t drrSize = 0;
    uintptr_t drr = GlossGetLibSection(MCPE_LIB, ".data.rel.ro", &drrSize);
    if (!drr || !drrSize) {
        LOGE("hookVtable: no .data.rel.ro for %s", cls);
        return false;
    }

    uintptr_t zti = 0;
    for (size_t i = 0; i+sizeof(uintptr_t) <= drrSize; i += sizeof(uintptr_t)) {
        if (*reinterpret_cast<uintptr_t*>(drr+i) == zts) {
            zti = drr+i-sizeof(uintptr_t);
            break;
        }
    }
    if (!zti) { LOGE("hookVtable: ZTI not found for %s", cls); return false; }

    uintptr_t vtbl = 0;
    for (size_t i = 0; i+sizeof(uintptr_t) <= drrSize; i += sizeof(uintptr_t)) {
        if (*reinterpret_cast<uintptr_t*>(drr+i) == zti) {
            vtbl = drr+i+sizeof(uintptr_t);
            break;
        }
    }
    if (!vtbl) { LOGE("hookVtable: VTable not found for %s", cls); return false; }

    void** vt = reinterpret_cast<void**>(vtbl);
    *outOrig = vt[slot];
    Unprotect(vtbl + slot*sizeof(void*), sizeof(void*));
    vt[slot] = hookFn;
    __builtin___clear_cache(
        (char*)(vtbl+slot*sizeof(void*)),
        (char*)(vtbl+(slot+1)*sizeof(void*)));
    LOGI("hooked %s slot[%d]", cls, slot);
    return true;
}

// ═══════════════════════════════════════════
//  HOOK 1: getNameTag
//  Intercept fungsi yang mengembalikan string nametag pemain
//  Tambahkan FPS/ping ke string tersebut
// ═══════════════════════════════════════════

// std::string* getNameTag(void* actor)
using getNameTag_t = std::string* (*)(void*);
static void* g_orig_getNameTag = nullptr;

static std::string* hook_getNameTag(void* actor) {
    // Panggil fungsi asli
    std::string* tag = ((getNameTag_t)g_orig_getNameTag)(actor);
    if (!tag || tag->empty()) return tag;

    // Tambahkan info FPS atau ping
    int fps  = g_fps.load();
    int ping = g_ping.load();

    char extra[64];
    if (ping > 0)
        snprintf(extra, sizeof(extra), " (%dms)", ping);
    else
        snprintf(extra, sizeof(extra), " (%dfps)", fps > 0 ? fps : 60);

    // Simpan original, tambahkan suffix
    static thread_local std::string modified;
    modified = *tag + extra;
    return &modified;
}

// ═══════════════════════════════════════════
//  HOOK 2: AppPlatform::getFrameRenderTime
//  Untuk hitung FPS yang akurat
// ═══════════════════════════════════════════
using getFrameTime_t = float (*)(void*);
static void* g_orig_getFrameTime = nullptr;

static float hook_getFrameTime(void* self) {
    updateFPS();
    return ((getFrameTime_t)g_orig_getFrameTime)(self);
}

// ═══════════════════════════════════════════
//  HOOK 3: RakPeer::GetAveragePing (untuk ping)
//  Signature-based hook
// ═══════════════════════════════════════════
using GetPing_t = int (*)(void*, void*);
static void* g_orig_getPing = nullptr;

static int hook_getPing(void* self, void* addr) {
    int ping = ((GetPing_t)g_orig_getPing)(self, addr);
    if (ping >= 0) g_ping.store(ping);
    return ping;
}

// ═══════════════════════════════════════════
//  NAMETAG SIGNATURE HOOKS
//  Sama persis dengan ThirdPersonNametag
// ═══════════════════════════════════════════

// Signature getNameTag - cari di libminecraftpe.so
// Pattern ini untuk fungsi Actor::getNameTag
static const char* NAMETAG_SIG =
    "? ? 00 91 "    // ADD Xn, Xm, #imm
    "? ? ? 97 "    // BL getNameTag internal
    "? ? 40 F9 "   // LDR X?, [X?]
    "? ? 40 F9 "   // LDR X?, [X?]
    "? ? 00 91";   // ADD

// Signature AppPlatform frame time
static const char* FRAME_TIME_SIG =
    "? ? ? ? "
    "F4 4F BE A9 "
    "FD 7B 01 A9 "
    "FD 43 00 91 "
    "? ? ? 94";

// Signature RakPeer::GetAveragePing
static const char* PING_SIG =
    "F4 4F BE A9 "
    "FD 7B 01 A9 "
    "? ? ? ? "
    "? ? 00 B4 "
    "? ? 40 F9 "
    "? 00 40 F9";

// ═══════════════════════════════════════════
//  ENTRY POINT (sama persis dengan ThirdPersonNametag)
// ═══════════════════════════════════════════
__attribute__((constructor))
void FPSTagMod_Init() {
    LOGI("FPSTagMod loading...");

    // Init Gloss - persis sama dengan ThirdPersonNametag
    GlossInit(true);

    // Hook 1: getNameTag via vtable
    // Class Actor, slot getNameTag (biasanya slot 36 atau sekitar)
    // Coba beberapa slot yang umum
    bool tagged = false;

    // Coba via signature dulu (lebih reliable)
    uintptr_t nametagAddr = pl::signature::pl_resolve_signature(
        NAMETAG_SIG, MCPE_LIB);

    if (nametagAddr) {
        if (GlossHook((void*)nametagAddr,
                      (void*)hook_getNameTag,
                      &g_orig_getNameTag) == 0) {
            LOGI("getNameTag hooked via signature");
            tagged = true;
        }
    }

    if (!tagged) {
        // Fallback: coba vtable hook
        // "8Actor" = mangled name class Actor di ARM64
        if (hookVtable("8Actor", 36,
                       &g_orig_getNameTag,
                       (void*)hook_getNameTag)) {
            LOGI("getNameTag hooked via vtable slot 36");
            tagged = true;
        }
    }

    if (!tagged) {
        LOGE("getNameTag hook FAILED - nametag won't show FPS");
    }

    // Hook 2: frame time untuk FPS counter
    uintptr_t frameAddr = pl::signature::pl_resolve_signature(
        FRAME_TIME_SIG, MCPE_LIB);
    if (frameAddr) {
        GlossHook((void*)frameAddr,
                  (void*)hook_getFrameTime,
                  &g_orig_getFrameTime);
        LOGI("FrameTime hooked");
    } else {
        LOGI("FrameTime sig not found (FPS may show 0 initially)");
    }

    // Hook 3: ping
    uintptr_t pingAddr = pl::signature::pl_resolve_signature(
        PING_SIG, MCPE_LIB);
    if (pingAddr) {
        GlossHook((void*)pingAddr,
                  (void*)hook_getPing,
                  &g_orig_getPing);
        LOGI("GetPing hooked");
    } else {
        LOGI("Ping sig not found (will show FPS instead)");
    }

    LOGI("FPSTagMod ready! Format: 'Name (60fps)' or 'Name (12ms)'");
}

__attribute__((destructor))
void FPSTagMod_Shutdown() {
    LOGI("FPSTagMod unloaded");
}
