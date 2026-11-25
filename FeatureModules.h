#pragma once
#include "CoreEngine.h"
#include "Theme.h"
#include "KillAround.h"
#include <unordered_map>
#if defined(__has_feature)
#if __has_feature(ptrauth_intrinsics)
#include <ptrauth.h>
#endif
#endif

// FeatureModules.h — Complete Managed Hooks, ECS Systems, World/Vehicle/Wardrobe/Automation & Overlay Draw
namespace Hooks {

using SystemUpdateFn    = void (*)(void*, float, void*);
using FallDmgFn         = void (*)(void*, int64_t, float, void*);
using WriteDatagramFn   = void (*)(void*, void*, void*);
using InitPartsFn       = void (*)(void*, int64_t, void*);
using StreamConstructFn = bool (*)(void*, int64_t, void*);
using NickValidateFn    = bool (*)(void*, void*, void*, void*);
using NickPassFn        = void* (*)(void*, void*);
using VoiceRunFn        = void (*)(void*, void*);
using TempWeapSetFn     = void (*)(void*, int32_t, int32_t, bool, void*);
using ItemUseLocalFn    = bool (*)(void*, int64_t, int32_t, void*);
using GetUniqueIdFn     = void* (*)(void*);
// WeaponStorage::RayCastAttackRanged overloads (7-arg & 8-arg) + Penetration & Map FindPath
using RayCastRanged7Fn  = int32_t (*)(void*, int64_t, Vec3, Vec3, float, int32_t, void*, int64_t*, void*);
using RayCastRanged8Fn  = int32_t (*)(void*, int64_t, Vec3, Vec3, float, int32_t, int32_t, void*, int64_t*, void*);
using TryGetPenDistFn   = bool (*)(void*, int32_t, int64_t, void*, void*, float*, void*);
using CanDmgSecondaryFn = bool (*)(void*, int32_t, int64_t, int64_t, void*);
using NavFindPathFn     = int32_t (*)(void*, Vec3, Vec3, void*, void*);
// FMOD / Whisper Voice / Camera / Ghost Proxy Environment Immunity
using StudioListenerUpdFn = void (*)(void*, void*);
using SetListenerLoc2Fn   = void (*)(void*, void*, void*);
using SetListenerLoc3IdxFn= void (*)(int32_t, void*, void*, void*);
using SetListenerLoc3RbFn = void (*)(void*, void*, void*, void*);
using SetListenerLoc4Fn   = void (*)(int32_t, void*, void*, void*, void*);
using SpatialCtorFn       = void (*)(void*, void*);
using VoiceSetMutedFn     = void (*)(void*, int32_t, bool, void*);
using VoiceSetAllMutedFn  = void (*)(void*, bool, void*);
using WaterTriggerFn      = void (*)(void*, void*, void*);

inline SystemUpdateFn    orig_GZ_Sync = nullptr, orig_GZ_Fire = nullptr, orig_GZ_Melee = nullptr;
inline FallDmgFn         orig_ApplyLocalFallDamage = nullptr;
inline WriteDatagramFn   orig_ParkingSpawnWrite = nullptr;
inline InitPartsFn       orig_VehFactoryInitParts = nullptr;
inline StreamConstructFn orig_PedSP_Construct = nullptr, orig_VehSP_Construct = nullptr;
inline NickValidateFn    orig_NickTryValidate = nullptr;
inline NickPassFn        orig_NickGetFormat = nullptr, orig_NickReplaceRegex = nullptr;
inline VoiceRunFn        orig_VoiceWorkerRun = nullptr;
inline TempWeapSetFn     orig_TempWeaponSetLocal = nullptr;
inline ItemUseLocalFn    orig_ItemTryUseLocal = nullptr;
inline GetUniqueIdFn     orig_SystemInfoDeviceUnique = nullptr;
inline RayCastRanged7Fn  orig_RayCastRanged7 = nullptr;
inline RayCastRanged8Fn  orig_RayCastRanged8 = nullptr;
inline TryGetPenDistFn   orig_TryGetPenDist = nullptr;
inline CanDmgSecondaryFn orig_CanDmgSecondary = nullptr;
inline NavFindPathFn     orig_NavFindPath = nullptr;
inline StudioListenerUpdFn orig_StudioListenerUpdate = nullptr;
inline SetListenerLoc2Fn   orig_SetListenerLoc2 = nullptr;
inline SetListenerLoc3IdxFn orig_SetListenerLoc3Idx = nullptr;
inline SetListenerLoc3RbFn orig_SetListenerLoc3Rb = nullptr;
inline SetListenerLoc4Fn   orig_SetListenerLoc4 = nullptr;
inline SpatialCtorFn       orig_SpatialCtor = nullptr;
inline VoiceSetMutedFn     orig_VoiceSetMutedChat = nullptr, orig_VoiceSetMutedBus = nullptr;
inline VoiceSetAllMutedFn  orig_VoiceSetAllMuted = nullptr;
inline SystemUpdateFn      orig_CamPlacementUpdate = nullptr, orig_UniCamApplyUpdate = nullptr;
inline SystemUpdateFn      orig_OffMapUpdate = nullptr, orig_PedKillUpdate = nullptr;
inline SystemUpdateFn      orig_PedOxygenUpdate = nullptr, orig_PedGravityUpdate = nullptr;
inline SystemUpdateFn      orig_PedKoWaterUpdate = nullptr, orig_SubmergeUpdate = nullptr;
inline SystemUpdateFn      orig_UnderwaterReactUpdate = nullptr;
inline WaterTriggerFn      orig_WaterAreaTriggerStay = nullptr;
inline WriteDatagramFn     orig_PedDispRead = nullptr, orig_PedDispWrite = nullptr;
inline WriteDatagramFn     orig_PedDeadRead = nullptr, orig_MeleeAtkRead = nullptr, orig_MeleeReqWrite = nullptr;

inline double g_vehSpawnCaptureDeadline = 0.0;
inline bool   g_hooksInstalled = false;
inline bool   g_gzHooksInstalled = false;
inline bool   g_noFallHookInstalled = false;
inline bool   g_silentAimHooksInstalled = false;
inline bool   g_nickHooksInstalled = false;
inline bool   g_voiceHookInstalled = false;
inline bool   g_vehSpawnHooksInstalled = false;
inline bool   g_deviceIdHookInstalled = false;
inline bool   g_freeCamPatchesInstalled = false;
inline bool   g_freeCamVoiceHooksInstalled = false;
inline bool   g_killAroundHooksInstalled = false;
inline bool   g_mapHooksInstalled = false;

// Shared Silent Aim & Wallbang state published per frame by ApplyAimbotStep
inline std::atomic<bool>     g_silentAimHasTarget{false};
inline std::atomic<float>    g_saTargetX{0.0f}, g_saTargetY{0.0f}, g_saTargetZ{0.0f};
inline std::atomic<int32_t>  g_throughWallMask{0};
inline std::atomic<int32_t>  g_groundLayerMask{-1};
inline std::atomic<uint32_t> g_hwBpHitCount{0};
inline Vec3                  g_silentAimTargetPos{};
inline std::mutex            g_silentAimMutex;

// Captured Map Marker GPS Destination (NavigationGraph.FindPath & _lastGoal)
inline std::atomic<void*>    g_navGraphInst{nullptr};
inline std::atomic<bool>     g_hasCapturedMapGoal{false};
inline std::atomic<float>    g_mapGoalX{0.0f}, g_mapGoalY{0.0f}, g_mapGoalZ{0.0f};
inline std::atomic<uint32_t> g_mapGoalSeq{0};

// Ghost Proxy Underground Anchor & Surface Audio Listener State
inline std::atomic<bool>     g_fcGhostAnchorActive{false};
inline std::atomic<double>   g_fcReturnGraceUntil{0.0};
inline std::atomic<bool>     g_fcHasSurfaceAudio{false};
inline std::atomic<float>    g_fcAudioX{0.0f}, g_fcAudioY{0.0f}, g_fcAudioZ{0.0f};
inline std::atomic<bool>     g_fcCamLockActive{false};
inline std::atomic<float>    g_fcCamPosX{0.0f}, g_fcCamPosY{0.0f}, g_fcCamPosZ{0.0f};
inline std::atomic<float>    g_fcCamRotX{0.0f}, g_fcCamRotY{0.0f}, g_fcCamRotZ{0.0f}, g_fcCamRotW{1.0f};
inline std::atomic<void*>    g_pedPhysicsParamsInst{nullptr};
inline int32_t               g_savedPedGravityInAir = 0;
inline bool                  g_hasSavedPedGravity = false;

// ============================================================================
// ARM64 Mach Hardware Breakpoint Silent Aim + Wallbang Engine
// Hooks WeaponStorage::RayCastAttackRanged (7-arg & 8-arg) directly on native AOT BL
// calls with ZERO __TEXT page writes (works on both jailed & jailbroken iOS).
// ============================================================================
namespace SilentAimHWBP {

inline std::atomic<uint64_t> g_entry7{0};
inline std::atomic<uint64_t> g_entry8{0};
inline std::atomic<uint64_t> g_entryFindPath{0};
inline std::atomic<void*>    g_m7Info{nullptr};
inline std::atomic<void*>    g_m8Info{nullptr};
inline std::atomic<void*>    g_mFindPathInfo{nullptr};
inline std::atomic<bool>     g_portReady{false};
inline std::atomic<bool>     g_portTried{false};
inline std::atomic<bool>     g_armed{false};
inline std::atomic<bool>     g_combatArmed{false};
inline std::atomic<bool>     g_mapArmed{false};
inline std::atomic<uint32_t> g_excThreadMachPort{0};
inline mach_port_t           g_excPort = MACH_PORT_NULL;
inline pthread_t             g_excThread{};

inline uint64_t StripPac(uint64_t addr) {
#if defined(__PTRAUTH_INTRINSICS__)
    addr = reinterpret_cast<uint64_t>(ptrauth_strip(reinterpret_cast<void*>(addr), ptrauth_key_function_pointer));
#endif
    return addr & 0x0000007FFFFFFFFFULL;
}

inline float NeonGetS(const arm_neon_state64_t& neon, int idx) {
    union { uint32_t u; float f; } cvt;
    cvt.u = static_cast<uint32_t>(neon.__v[idx] & 0xFFFFFFFFULL);
    return cvt.f;
}

inline void NeonSetS(arm_neon_state64_t& neon, int idx, float val) {
    union { uint32_t u; float f; } cvt;
    cvt.f = val;
    neon.__v[idx] = static_cast<__uint128_t>(cvt.u);
}

inline int32_t ComputeWallbangLayerMask() {
    int32_t mask = g_throughWallMask.load(std::memory_order_acquire);
    if (mask != 0) return mask;
    int32_t fallback = 0;
    for (int l = 8; l <= 19; ++l) fallback |= (1 << l);
    return fallback;
}

inline void ResolveLayerMasksOnce() {
    static std::atomic<bool> s_done{false};
    if (s_done.load(std::memory_order_acquire)) return;
    const char* ns = Offsets::g_cfg.nsGlobalContexts.c_str();
    const char* cls = Offsets::g_cfg.clsLayersMask.c_str();
    int32_t pedComb = 0, pedHead = 0, pedBody = 0, pedCap = 0, motoHead = 0, normWorld = 0;
    bool anyPed = false;
    if (Il2Cpp::ReadStaticIntField(ns, cls, "PedCombination", pedComb)) anyPed = true;
    if (Il2Cpp::ReadStaticIntField(ns, cls, "PedHead", pedHead)) anyPed = true;
    if (Il2Cpp::ReadStaticIntField(ns, cls, "PedBody", pedBody)) anyPed = true;
    if (Il2Cpp::ReadStaticIntField(ns, cls, "PedCapsule", pedCap)) anyPed = true;
    if (Il2Cpp::ReadStaticIntField(ns, cls, "MotoHeadCollisionCombination", motoHead)) anyPed = true;
    if (anyPed) {
        int32_t mask = pedComb | pedHead | pedBody | pedCap | motoHead;
        if (mask != 0) g_throughWallMask.store(mask, std::memory_order_release);
    }
    if (Il2Cpp::ReadStaticIntField(ns, cls, "NormalWorldCombination", normWorld) && normWorld != 0) {
        g_groundLayerMask.store(normWorld, std::memory_order_release);
    }
    if (anyPed || normWorld != 0) s_done.store(true, std::memory_order_release);
}

inline void ResolveEntriesOnce() {
    const auto& c = Offsets::g_cfg;
    if (g_entry7.load(std::memory_order_acquire) == 0) {
        if (void* m7 = Il2Cpp::ResolveMethod(c.nsWeaponSys.c_str(), c.clsWeaponStorage.c_str(), c.mthRayCastRanged.c_str(), 7)) {
            g_m7Info.store(m7, std::memory_order_release);
            void* fn = SafeMem::PtrAt(m7, 0);
            if (SafeMem::IsPlausiblePtr(fn)) g_entry7.store(StripPac(reinterpret_cast<uint64_t>(fn)), std::memory_order_release);
        }
    }
    if (g_entry8.load(std::memory_order_acquire) == 0) {
        if (void* m8 = Il2Cpp::ResolveMethod(c.nsWeaponSys.c_str(), c.clsWeaponStorage.c_str(), c.mthRayCastRanged.c_str(), 8)) {
            g_m8Info.store(m8, std::memory_order_release);
            void* fn = SafeMem::PtrAt(m8, 0);
            if (SafeMem::IsPlausiblePtr(fn)) g_entry8.store(StripPac(reinterpret_cast<uint64_t>(fn)), std::memory_order_release);
        }
    }
    if (g_entryFindPath.load(std::memory_order_acquire) == 0) {
        if (void* mf = Il2Cpp::ResolveMethod(c.nsMap.c_str(), c.clsNavigationGraph.c_str(), c.mthFindPath.c_str(), 3)) {
            g_mFindPathInfo.store(mf, std::memory_order_release);
            void* fn = SafeMem::PtrAt(mf, 0);
            if (SafeMem::IsPlausiblePtr(fn)) g_entryFindPath.store(StripPac(reinterpret_cast<uint64_t>(fn)), std::memory_order_release);
        }
    }
}

inline void PopulateDebugBreakpoints(arm_debug_state64_t& dbg, bool wantCombat, bool wantMap) {
    uint64_t e7 = wantCombat ? g_entry7.load(std::memory_order_acquire) : 0;
    uint64_t e8 = wantCombat ? g_entry8.load(std::memory_order_acquire) : 0;
    uint64_t ef = wantMap ? g_entryFindPath.load(std::memory_order_acquire) : 0;
    for (int i = 0; i < 4; ++i) {
        dbg.__bvr[i] = 0;
        dbg.__bcr[i] = 0;
    }
    int slot = 0;
    if (e7 != 0) {
        dbg.__bvr[slot] = e7;
        dbg.__bcr[slot] = 0x1E7ULL;
        ++slot;
    }
    if (e8 != 0 && e8 != e7) {
        dbg.__bvr[slot] = e8;
        dbg.__bcr[slot] = 0x1E7ULL;
        ++slot;
    }
    if (ef != 0 && ef != e7 && ef != e8 && slot < 4) {
        dbg.__bvr[slot] = ef;
        dbg.__bcr[slot] = 0x1E7ULL;
    }
}

inline void SetHwbpOnThread(thread_t thr, bool wantCombat, bool wantMap) {
    if (thr == MACH_PORT_NULL) return;
    uint32_t excThr = g_excThreadMachPort.load(std::memory_order_acquire);
    if (excThr != 0 && thr == excThr) return;
    arm_debug_state64_t dbg{};
    mach_msg_type_number_t cnt = ARM_DEBUG_STATE64_COUNT;
    if (thread_get_state(thr, ARM_DEBUG_STATE64, reinterpret_cast<thread_state_t>(&dbg), &cnt) != KERN_SUCCESS)
        return;
    // Never interrupt an active single-step trap on a worker thread
    if ((dbg.__mdscr_el1 & 1ULL) != 0) return;
    if (wantCombat || wantMap) {
        PopulateDebugBreakpoints(dbg, wantCombat, wantMap);
    } else {
        for (int i = 0; i < 4; ++i) {
            dbg.__bvr[i] = 0;
            dbg.__bcr[i] = 0;
        }
    }
    if (thread_set_state(thr, ARM_DEBUG_STATE64, reinterpret_cast<thread_state_t>(&dbg), ARM_DEBUG_STATE64_COUNT) != KERN_SUCCESS && wantMap && wantCombat) {
        // Fallback to 2 slots if hardware implements only 2 HW breakpoints
        PopulateDebugBreakpoints(dbg, true, false);
        thread_set_state(thr, ARM_DEBUG_STATE64, reinterpret_cast<thread_state_t>(&dbg), ARM_DEBUG_STATE64_COUNT);
    }
}

inline void ApplyAllThreads(bool wantCombat, bool wantMap) {
    thread_act_array_t list = nullptr;
    mach_msg_type_number_t count = 0;
    if (task_threads(mach_task_self(), &list, &count) != KERN_SUCCESS || !list) return;
    for (mach_msg_type_number_t i = 0; i < count; ++i) {
        SetHwbpOnThread(list[i], wantCombat, wantMap);
        mach_port_deallocate(mach_task_self(), list[i]);
    }
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(list), count * sizeof(thread_t));
}

inline void HandleBreakpoint(thread_t thr) {
    arm_thread_state64_t ts{};
    mach_msg_type_number_t tsCnt = ARM_THREAD_STATE64_COUNT;
    if (thread_get_state(thr, ARM_THREAD_STATE64, reinterpret_cast<thread_state_t>(&ts), &tsCnt) != KERN_SUCCESS)
        return;

#if defined(__darwin_arm_thread_state64_get_pc)
    uint64_t pc = StripPac(static_cast<uint64_t>(__darwin_arm_thread_state64_get_pc(ts)));
#else
    uint64_t pc = StripPac(static_cast<uint64_t>(ts.__pc));
#endif
    uint64_t e7 = g_entry7.load(std::memory_order_acquire);
    uint64_t e8 = g_entry8.load(std::memory_order_acquire);
    uint64_t ef = g_entryFindPath.load(std::memory_order_acquire);
    bool isEntry7   = (e7 != 0 && pc == e7);
    bool isEntry8   = (e8 != 0 && pc == e8);
    bool isFindPath = (ef != 0 && pc == ef);

    arm_debug_state64_t dbg{};
    mach_msg_type_number_t dbgCnt = ARM_DEBUG_STATE64_COUNT;
    if (thread_get_state(thr, ARM_DEBUG_STATE64, reinterpret_cast<thread_state_t>(&dbg), &dbgCnt) != KERN_SUCCESS)
        return;

    if (isEntry7 || isEntry8 || isFindPath) {
        if (isFindPath) {
            if (ts.__x[0] > 0x100000000ULL) {
                g_navGraphInst.store(reinterpret_cast<void*>(ts.__x[0]), std::memory_order_release);
            }
            arm_neon_state64_t neon{};
            mach_msg_type_number_t neonCnt = ARM_NEON_STATE64_COUNT;
            if (thread_get_state(thr, ARM_NEON_STATE64, reinterpret_cast<thread_state_t>(&neon), &neonCnt) == KERN_SUCCESS) {
                Vec3 goal{ NeonGetS(neon, 3), NeonGetS(neon, 4), NeonGetS(neon, 5) };
                if (IsFiniteVec3(goal) && (std::fabs(goal.x) > 0.25f || std::fabs(goal.z) > 0.25f) &&
                    std::fabs(goal.x) < 25000.0f && std::fabs(goal.z) < 25000.0f) {
                    g_mapGoalX.store(goal.x, std::memory_order_relaxed);
                    g_mapGoalY.store(goal.y, std::memory_order_relaxed);
                    g_mapGoalZ.store(goal.z, std::memory_order_relaxed);
                    g_hasCapturedMapGoal.store(true, std::memory_order_release);
                    g_mapGoalSeq.fetch_add(1, std::memory_order_release);
                }
            }
        } else {
            g_hwBpHitCount.fetch_add(1, std::memory_order_relaxed);
            if (!Morpeh::g_stashes.realWeaponStorage && ts.__x[0] > 0x100000000ULL) {
                Morpeh::g_stashes.realWeaponStorage = reinterpret_cast<void*>(ts.__x[0]);
            }
            if (!g_cfgState.bMasterDisable) {
                int64_t localPed = g_localPedEntityAtomic.load(std::memory_order_acquire);
                int64_t dealerPed = static_cast<int64_t>(ts.__x[1]);
                bool isLocalShooter = (localPed <= 0) ||
                                      (Morpeh::UnpackEntityId(dealerPed) == Morpeh::UnpackEntityId(localPed));
                if (isLocalShooter) {
                    // 1) Silent Aim bullet direction + range override (when target locked)
                    if (g_cfgState.bSilentAim && g_silentAimHasTarget.load(std::memory_order_acquire)) {
                        arm_neon_state64_t neon{};
                        mach_msg_type_number_t neonCnt = ARM_NEON_STATE64_COUNT;
                        if (thread_get_state(thr, ARM_NEON_STATE64, reinterpret_cast<thread_state_t>(&neon), &neonCnt) == KERN_SUCCESS) {
                            Vec3 startPos{ NeonGetS(neon, 0), NeonGetS(neon, 1), NeonGetS(neon, 2) };
                            Vec3 tgt{ g_saTargetX.load(std::memory_order_relaxed),
                                      g_saTargetY.load(std::memory_order_relaxed),
                                      g_saTargetZ.load(std::memory_order_relaxed) };
                            if (IsFiniteVec3(startPos) && IsFiniteVec3(tgt)) {
                                Vec3 delta{ tgt.x - startPos.x, tgt.y - startPos.y, tgt.z - startPos.z };
                                float distToTgt = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                                if (distToTgt > 0.05f && distToTgt < 600.0f) {
                                    Vec3 newDir{ delta.x / distToTgt, delta.y / distToTgt, delta.z / distToTgt };
                                    NeonSetS(neon, 3, newDir.x);
                                    NeonSetS(neon, 4, newDir.y);
                                    NeonSetS(neon, 5, newDir.z);
                                    float curDist = NeonGetS(neon, 6);
                                    float wantDist = std::max(curDist, std::max(distToTgt + 25.0f, g_cfgState.saRadius));
                                    NeonSetS(neon, 6, wantDist);
                                    thread_set_state(thr, ARM_NEON_STATE64, reinterpret_cast<thread_state_t>(&neon), ARM_NEON_STATE64_COUNT);
                                }
                            }
                        }
                    }

                    // 2) Bullet Wallbang: works BOTH with Silent Aim AND standalone when shooting through walls!
                    if (g_cfgState.bSilentWallbang) {
                        ts.__x[2] = static_cast<uint64_t>(static_cast<uint32_t>(ComputeWallbangLayerMask()));
                        if (isEntry8) {
                            ts.__x[3] = 4ULL; // WeaponHitTargets::Peds
                        }
                        thread_set_state(thr, ARM_THREAD_STATE64, reinterpret_cast<thread_state_t>(&ts), ARM_THREAD_STATE64_COUNT);
                    }
                }
            }
        }
        // Single-step 1 instruction past entry, then re-arm HWBP on the next trap
        for (int i = 0; i < 4; ++i) dbg.__bcr[i] = 0;
        dbg.__mdscr_el1 |= 1ULL;
        thread_set_state(thr, ARM_DEBUG_STATE64, reinterpret_cast<thread_state_t>(&dbg), ARM_DEBUG_STATE64_COUNT);
    } else {
        // Single-step trap: clear SS bit and re-arm active breakpoints
        dbg.__mdscr_el1 &= ~1ULL;
        bool wc = g_combatArmed.load(std::memory_order_acquire);
        bool wm = g_mapArmed.load(std::memory_order_acquire);
        if (wc || wm) {
            PopulateDebugBreakpoints(dbg, wc, wm);
        } else {
            for (int i = 0; i < 4; ++i) {
                dbg.__bvr[i] = 0;
                dbg.__bcr[i] = 0;
            }
        }
        thread_set_state(thr, ARM_DEBUG_STATE64, reinterpret_cast<thread_state_t>(&dbg), ARM_DEBUG_STATE64_COUNT);
    }
}

#pragma pack(push, 4)
struct MachExcReq {
    mach_msg_header_t Head;
    mach_msg_body_t msgh_body;
    mach_msg_port_descriptor_t thread;
    mach_msg_port_descriptor_t task;
    NDR_record_t NDR;
    exception_type_t exception;
    mach_msg_type_number_t codeCnt;
    int64_t code[2];
    uint8_t pad[128];
};
struct MachExcReply {
    mach_msg_header_t Head;
    NDR_record_t NDR;
    kern_return_t RetCode;
};
#pragma pack(pop)

inline void* ExceptionServerLoop(void*) {
    g_excThreadMachPort.store(mach_thread_self(), std::memory_order_release);
    for (;;) {
        MachExcReq req{};
        kern_return_t kr = mach_msg(&req.Head, MACH_RCV_MSG | MACH_RCV_LARGE, 0,
                                    sizeof(req), g_excPort, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
        if (kr != KERN_SUCCESS) continue;

        kern_return_t retCode = KERN_FAILURE;
        if (req.Head.msgh_id == 2405) {
            thread_t faultingThr = req.thread.name;
            HandleBreakpoint(faultingThr);
            retCode = KERN_SUCCESS;
            if (req.thread.name != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), req.thread.name);
            if (req.task.name != MACH_PORT_NULL)   mach_port_deallocate(mach_task_self(), req.task.name);
        }

        MachExcReply rep{};
        rep.Head.msgh_bits        = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(req.Head.msgh_bits), 0);
        rep.Head.msgh_size        = sizeof(rep);
        rep.Head.msgh_remote_port = req.Head.msgh_remote_port;
        rep.Head.msgh_local_port  = MACH_PORT_NULL;
        rep.Head.msgh_id          = req.Head.msgh_id + 100;
        rep.NDR                   = NDR_record;
        rep.RetCode               = retCode;
        mach_msg(&rep.Head, MACH_SEND_MSG, sizeof(rep), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    }
    return nullptr;
}

inline void EnsurePortInstalled() {
    if (!g_portReady.load(std::memory_order_acquire)) {
        if (g_excPort == MACH_PORT_NULL) {
            if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &g_excPort) != KERN_SUCCESS) return;
            if (mach_port_insert_right(mach_task_self(), g_excPort, g_excPort, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) return;
        }
        kern_return_t kr = task_set_exception_ports(mach_task_self(), EXC_MASK_BREAKPOINT, g_excPort,
                                                    EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, THREAD_STATE_NONE);
        if (kr != KERN_SUCCESS) {
            g_portTried.store(true, std::memory_order_release);
            return;
        }
        if (pthread_create(&g_excThread, nullptr, ExceptionServerLoop, nullptr) == 0) {
            pthread_detach(g_excThread);
            g_portReady.store(true, std::memory_order_release);
        }
        g_portTried.store(true, std::memory_order_release);
        return;
    }
    // Periodically re-verify EXC_MASK_BREAKPOINT wasn't hijacked later by Crashlytics/PLCrashReporter
    static double s_lastPortCheck = 0.0;
    double now = CACurrentMediaTime();
    if ((now - s_lastPortCheck) >= 2.5) {
        s_lastPortCheck = now;
        static constexpr mach_msg_type_number_t kMaxExcPorts = 16;
        exception_mask_t masks[kMaxExcPorts]{};
        mach_port_t ports[kMaxExcPorts]{};
        exception_behavior_t behaviors[kMaxExcPorts]{};
        thread_state_flavor_t flavors[kMaxExcPorts]{};
        mach_msg_type_number_t cnt = kMaxExcPorts;
        bool oursActive = false;
        if (task_get_exception_ports(mach_task_self(), EXC_MASK_BREAKPOINT, masks, &cnt, ports, behaviors, flavors) == KERN_SUCCESS) {
            for (mach_msg_type_number_t i = 0; i < cnt; ++i) {
                if ((masks[i] & EXC_MASK_BREAKPOINT) && ports[i] == g_excPort) oursActive = true;
                if (ports[i] != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), ports[i]);
            }
        }
        if (!oursActive) {
            task_set_exception_ports(mach_task_self(), EXC_MASK_BREAKPOINT, g_excPort,
                                     EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, THREAD_STATE_NONE);
        }
    }
}

inline void Tick() {
    if (!Morpeh::g_stashes.worldReady) return;
    ResolveLayerMasksOnce();

    bool wantCombat = (!g_cfgState.bMasterDisable && (g_cfgState.bSilentAim || g_cfgState.bSilentWallbang));
    if (wantCombat) {
        ResolveEntriesOnce();
        if (g_entry7.load(std::memory_order_acquire) != 0 || g_entry8.load(std::memory_order_acquire) != 0) {
            EnsurePortInstalled();
            if (!g_portReady.load(std::memory_order_acquire)) return;

            bool prevCombat = g_combatArmed.exchange(true, std::memory_order_acq_rel);
            g_mapArmed.store(false, std::memory_order_release);
            g_armed.store(true, std::memory_order_release);

            static double s_lastReapply = 0.0;
            double now = CACurrentMediaTime();
            if (!prevCombat || (now - s_lastReapply) >= 0.85) {
                ApplyAllThreads(true, false);
                s_lastReapply = now;
            }
            return;
        }
    }

    bool wasCombat = g_combatArmed.exchange(false, std::memory_order_acq_rel);
    bool wasMap    = g_mapArmed.exchange(false, std::memory_order_acq_rel);
    g_armed.store(false, std::memory_order_release);
    if (wasCombat || wasMap) {
        ApplyAllThreads(false, false);
    }
}

} // namespace SilentAimHWBP

static void GZ_PurgeSystemField(void* sysInstance, const char* fieldName, size_t fallbackOffset) {
    if (g_cfgState.bMasterDisable) return;
    if ((!g_cfgState.bGreenZoneBypass && !g_cfgState.bZoneGuard) || !SafeMem::IsPlausiblePtr(sysInstance)) return;
    void* k = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(sysInstance) : nullptr;
    size_t off = Il2Cpp::FieldOffset(k, fieldName, fallbackOffset);
    if (void* stash = SafeMem::PtrAt(sysInstance, off)) {
        Morpeh::RemoveAll(stash);
    }
}

static void Hook_GZ_Sync(void* self, float dt, void* method) {
    if (SafeMem::IsPlausiblePtr(self) && !Morpeh::g_stashes.realPlayerStorage) {
        void* k = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(self) : nullptr;
        size_t psOff = Il2Cpp::FieldOffset(k, "_playerStorage", 0);
        if (psOff != 0) {
            if (void* ps = SafeMem::PtrAt(self, psOff)) Morpeh::g_stashes.realPlayerStorage = ps;
        }
    }
    if (!g_cfgState.bMasterDisable && (g_cfgState.bGreenZoneBypass || g_cfgState.bZoneGuard))
        GZ_PurgeSystemField(self, "_weaponAttackNotAllowed", Offsets::GreenZoneOffsets::kSyncSystemStashOffset);
    if (orig_GZ_Sync) orig_GZ_Sync(self, dt, method);
    if (!g_cfgState.bMasterDisable && (g_cfgState.bGreenZoneBypass || g_cfgState.bZoneGuard))
        GZ_PurgeSystemField(self, "_weaponAttackNotAllowed", Offsets::GreenZoneOffsets::kSyncSystemStashOffset);
}

static void Hook_GZ_Fire(void* self, float dt, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) {
        void* k = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(self) : nullptr;
        if (!Morpeh::g_stashes.realPlayerStorage) {
            size_t psOff = Il2Cpp::FieldOffset(k, "_playerStorage", 0);
            if (psOff != 0) {
                if (void* ps = SafeMem::PtrAt(self, psOff)) Morpeh::g_stashes.realPlayerStorage = ps;
            }
        }
        if (!Morpeh::g_stashes.realCtrlElemStorage) {
            size_t ceOff = Il2Cpp::FieldOffset(k, "_controlledElementStorage", 0);
            if (ceOff != 0) {
                if (void* ce = SafeMem::PtrAt(self, ceOff)) Morpeh::g_stashes.realCtrlElemStorage = ce;
            }
        }
    }
    if (!g_cfgState.bMasterDisable && (g_cfgState.bGreenZoneBypass || g_cfgState.bZoneGuard))
        GZ_PurgeSystemField(self, "_weaponAttackNotAllowed", 0x30);
    if (orig_GZ_Fire) orig_GZ_Fire(self, dt, method);
    if (!g_cfgState.bMasterDisable && (g_cfgState.bGreenZoneBypass || g_cfgState.bZoneGuard))
        GZ_PurgeSystemField(self, "_weaponAttackNotAllowed", 0x30);
}

static void Hook_GZ_Melee(void* self, float dt, void* method) {
    if (!g_cfgState.bMasterDisable && (g_cfgState.bGreenZoneBypass || g_cfgState.bZoneGuard))
        GZ_PurgeSystemField(self, "_weaponAttackNotAllowed", 0xC8);
    if (orig_GZ_Melee) orig_GZ_Melee(self, dt, method);
    if (!g_cfgState.bMasterDisable && (g_cfgState.bGreenZoneBypass || g_cfgState.bZoneGuard))
        GZ_PurgeSystemField(self, "_weaponAttackNotAllowed", 0xC8);
}

static int32_t Hook_RayCastRanged7(void* self, int64_t dealerPed, Vec3 startPos, Vec3 dir, float dist,
                                   int32_t layerMask, void* outHit, int64_t* outHitEnt, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) Morpeh::g_stashes.realWeaponStorage = self;
    if (!g_cfgState.bMasterDisable) {
        int64_t localPed = g_localPedEntityAtomic.load(std::memory_order_acquire);
        bool isLocal = (localPed <= 0 || Morpeh::UnpackEntityId(dealerPed) == Morpeh::UnpackEntityId(localPed));
        if (isLocal) {
            if (g_cfgState.bSilentAim && g_silentAimHasTarget.load(std::memory_order_acquire)) {
                Vec3 tgt{ g_saTargetX.load(std::memory_order_relaxed),
                          g_saTargetY.load(std::memory_order_relaxed),
                          g_saTargetZ.load(std::memory_order_relaxed) };
                if (IsFiniteVec3(tgt) && IsFiniteVec3(startPos)) {
                    float dToTgt = Vec3Dist(startPos, tgt);
                    dir = Vec3Norm({ tgt.x - startPos.x, tgt.y - startPos.y, tgt.z - startPos.z });
                    dist = std::max(dist, std::max(dToTgt + 25.0f, g_cfgState.saRadius));
                }
            }
            if (g_cfgState.bSilentWallbang) {
                layerMask = SilentAimHWBP::ComputeWallbangLayerMask();
            }
        }
    }
    return orig_RayCastRanged7 ? orig_RayCastRanged7(self, dealerPed, startPos, dir, dist, layerMask, outHit, outHitEnt, method) : 0;
}

static int32_t Hook_RayCastRanged8(void* self, int64_t dealerPed, Vec3 startPos, Vec3 dir, float dist,
                                   int32_t layerMask, int32_t hitTargets, void* outHit, int64_t* outHitEnt, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) Morpeh::g_stashes.realWeaponStorage = self;
    if (!g_cfgState.bMasterDisable) {
        int64_t localPed = g_localPedEntityAtomic.load(std::memory_order_acquire);
        bool isLocal = (localPed <= 0 || Morpeh::UnpackEntityId(dealerPed) == Morpeh::UnpackEntityId(localPed));
        if (isLocal) {
            if (g_cfgState.bSilentAim && g_silentAimHasTarget.load(std::memory_order_acquire)) {
                Vec3 tgt{ g_saTargetX.load(std::memory_order_relaxed),
                          g_saTargetY.load(std::memory_order_relaxed),
                          g_saTargetZ.load(std::memory_order_relaxed) };
                if (IsFiniteVec3(tgt) && IsFiniteVec3(startPos)) {
                    float dToTgt = Vec3Dist(startPos, tgt);
                    dir = Vec3Norm({ tgt.x - startPos.x, tgt.y - startPos.y, tgt.z - startPos.z });
                    dist = std::max(dist, std::max(dToTgt + 25.0f, g_cfgState.saRadius));
                }
            }
            if (g_cfgState.bSilentWallbang) {
                layerMask = SilentAimHWBP::ComputeWallbangLayerMask();
                hitTargets = 4; // WeaponHitTargets::Peds
            }
        }
    }
    return orig_RayCastRanged8 ? orig_RayCastRanged8(self, dealerPed, startPos, dir, dist, layerMask, hitTargets, outHit, outHitEnt, method) : 0;
}

static bool Hook_TryGetPenDist(void* self, int32_t hitType, int64_t firstHitEnt, void* hit,
                               void* weaponParams, float* maxPenDist, void* method) {
    if (!g_cfgState.bMasterDisable && g_cfgState.bSilentWallbang && SafeMem::IsPlausiblePtr(maxPenDist)) {
        float pen = 500.0f;
        SafeMem::Write(maxPenDist, &pen, sizeof(pen));
        return true;
    }
    return orig_TryGetPenDist ? orig_TryGetPenDist(self, hitType, firstHitEnt, hit, weaponParams, maxPenDist, method) : false;
}

static bool Hook_CanDmgSecondary(void* self, int32_t firstHitType, int64_t firstEnt, int64_t secEnt, void* method) {
    if (!g_cfgState.bMasterDisable && g_cfgState.bSilentWallbang) return true;
    return orig_CanDmgSecondary ? orig_CanDmgSecondary(self, firstHitType, firstEnt, secEnt, method) : false;
}

static int32_t Hook_NavFindPath(void* self, Vec3 start, Vec3 end, void* path, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) g_navGraphInst.store(self, std::memory_order_release);
    if (IsFiniteVec3(end) && (std::fabs(end.x) > 0.25f || std::fabs(end.z) > 0.25f) &&
        std::fabs(end.x) < 25000.0f && std::fabs(end.z) < 25000.0f) {
        g_mapGoalX.store(end.x, std::memory_order_relaxed);
        g_mapGoalY.store(end.y, std::memory_order_relaxed);
        g_mapGoalZ.store(end.z, std::memory_order_relaxed);
        g_hasCapturedMapGoal.store(true, std::memory_order_release);
        g_mapGoalSeq.fetch_add(1, std::memory_order_release);
    }
    return orig_NavFindPath ? orig_NavFindPath(self, start, end, path, method) : 0;
}

// FMOD StudioListener + RuntimeManager + Whisper SpatialParameters + VoicePlaybackService
static void Hook_StudioListenerUpdate(void* self, void* method) {
    bool active = !g_cfgState.bMasterDisable && (g_cfgState.bSpectateActive || (g_cfgState.bFreeCam && g_cfgState.bFreeCamGhostVoice));
    if (active && Il2Cpp::IsUnityNativeAlive(self)) {
        const auto& cfg = Offsets::g_cfg;
        void* cls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(self) : nullptr;
        size_t offAtt = Il2Cpp::FieldOffset(cls, "attenuationObject", cfg.offStudioListenerAttenObj);
        size_t offRb  = Il2Cpp::FieldOffset(cls, "rigidBody", cfg.offStudioListenerRb);

        void* savedAtt = SafeMem::PtrAt(self, offAtt);
        void* savedRb  = SafeMem::PtrAt(self, offRb);
        void* nullPtr  = nullptr;
        SafeMem::Write(reinterpret_cast<uint8_t*>(self) + offAtt, &nullPtr, sizeof(void*));
        SafeMem::Write(reinterpret_cast<uint8_t*>(self) + offRb,  &nullPtr, sizeof(void*));

        if (g_fcHasSurfaceAudio.load(std::memory_order_acquire)) {
            Vec3 audioPos{ g_fcAudioX.load(std::memory_order_relaxed),
                           g_fcAudioY.load(std::memory_order_relaxed),
                           g_fcAudioZ.load(std::memory_order_relaxed) };
            if (IsFiniteVec3(audioPos)) {
                if (void* lstTr = Il2Cpp::SafeGetTransform(self)) Il2Cpp::SafeSetTransformPos(lstTr, audioPos);
                if (void* cam = Il2Cpp::SafeCameraGetMain()) {
                    if (void* camTr = Il2Cpp::SafeGetTransform(cam)) Il2Cpp::SafeSetTransformPos(camTr, audioPos);
                }
            }
        }
        if (orig_StudioListenerUpdate) orig_StudioListenerUpdate(self, method);
        SafeMem::Write(reinterpret_cast<uint8_t*>(self) + offAtt, &savedAtt, sizeof(void*));
        SafeMem::Write(reinterpret_cast<uint8_t*>(self) + offRb,  &savedRb,  sizeof(void*));
        return;
    }
    if (orig_StudioListenerUpdate) orig_StudioListenerUpdate(self, method);
}

static void ResolveSurfaceCamGo(void*& ioGo, void*& ioAtt) {
    bool active = !g_cfgState.bMasterDisable && (g_cfgState.bSpectateActive || (g_cfgState.bFreeCam && g_cfgState.bFreeCamGhostVoice));
    if (!active) return;
    if (void* cam = Il2Cpp::SafeCameraGetMain()) {
        if (void* camGo = Il2Cpp::SafeGetGameObject(cam)) ioGo = camGo;
    }
    ioAtt = nullptr;
}

static void Hook_SetListenerLoc2(void* go, void* att, void* method) {
    ResolveSurfaceCamGo(go, att);
    if (orig_SetListenerLoc2) orig_SetListenerLoc2(go, att, method);
}
static void Hook_SetListenerLoc3Idx(int32_t idx, void* go, void* att, void* method) {
    ResolveSurfaceCamGo(go, att);
    if (orig_SetListenerLoc3Idx) orig_SetListenerLoc3Idx(idx, go, att, method);
}
static void Hook_SetListenerLoc3Rb(void* go, void* rb, void* att, void* method) {
    bool active = !g_cfgState.bMasterDisable && (g_cfgState.bSpectateActive || (g_cfgState.bFreeCam && g_cfgState.bFreeCamGhostVoice));
    if (active) { ResolveSurfaceCamGo(go, att); rb = nullptr; }
    if (orig_SetListenerLoc3Rb) orig_SetListenerLoc3Rb(go, rb, att, method);
}
static void Hook_SetListenerLoc4(int32_t idx, void* go, void* rb, void* att, void* method) {
    bool active = !g_cfgState.bMasterDisable && (g_cfgState.bSpectateActive || (g_cfgState.bFreeCam && g_cfgState.bFreeCamGhostVoice));
    if (active) { ResolveSurfaceCamGo(go, att); rb = nullptr; }
    if (orig_SetListenerLoc4) orig_SetListenerLoc4(idx, go, rb, att, method);
}

static void Hook_SpatialCtor(void* self, void* method) {
    if (orig_SpatialCtor) orig_SpatialCtor(self, method);
    bool active = !g_cfgState.bMasterDisable && (g_cfgState.bSpectateActive || (g_cfgState.bFreeCam && g_cfgState.bFreeCamGhostVoice));
    if (active && SafeMem::IsPlausiblePtr(self)) {
        const auto& cfg = Offsets::g_cfg;
        uint32_t mode2D = cfg.kFmodMode2D; // WhisperRuntime.Playback.FmodMode::Mode2D (281022472)
        float minD = 150.0f, maxD = 500.0f;
        SafeMem::Write(reinterpret_cast<uint8_t*>(self) + cfg.offSpatialMode,    &mode2D, sizeof(mode2D));
        SafeMem::Write(reinterpret_cast<uint8_t*>(self) + cfg.offSpatialMinDist, &minD,   sizeof(minD));
        SafeMem::Write(reinterpret_cast<uint8_t*>(self) + cfg.offSpatialMaxDist, &maxD,   sizeof(maxD));
    }
}

static void Hook_VoiceSetMutedChat(void* self, int32_t chatType, bool state, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) Morpeh::g_stashes.realVoicePlayback = self;
    if (!g_cfgState.bMasterDisable && (g_cfgState.bSpectateActive || (g_cfgState.bFreeCam && g_cfgState.bFreeCamGhostVoice)))
        state = false;
    if (orig_VoiceSetMutedChat) orig_VoiceSetMutedChat(self, chatType, state, method);
}
static void Hook_VoiceSetMutedBus(void* self, int32_t busType, bool state, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) Morpeh::g_stashes.realVoicePlayback = self;
    if (!g_cfgState.bMasterDisable && (g_cfgState.bSpectateActive || (g_cfgState.bFreeCam && g_cfgState.bFreeCamGhostVoice)))
        state = false;
    if (orig_VoiceSetMutedBus) orig_VoiceSetMutedBus(self, busType, state, method);
}
static void Hook_VoiceSetAllMuted(void* self, bool muted, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) Morpeh::g_stashes.realVoicePlayback = self;
    if (!g_cfgState.bMasterDisable && (g_cfgState.bSpectateActive || (g_cfgState.bFreeCam && g_cfgState.bFreeCamGhostVoice)))
        muted = false;
    if (orig_VoiceSetAllMuted) orig_VoiceSetAllMuted(self, muted, method);
}

static void EnforceLockedSurfaceCameraIfNeeded() {
    if (!g_fcCamLockActive.load(std::memory_order_acquire)) return;
    if (void* cam = Il2Cpp::SafeCameraGetMain()) {
        if (void* camTr = Il2Cpp::SafeGetTransform(cam)) {
            Vec3 pos{ g_fcCamPosX.load(std::memory_order_relaxed),
                      g_fcCamPosY.load(std::memory_order_relaxed),
                      g_fcCamPosZ.load(std::memory_order_relaxed) };
            Quat rot{ g_fcCamRotX.load(std::memory_order_relaxed),
                      g_fcCamRotY.load(std::memory_order_relaxed),
                      g_fcCamRotZ.load(std::memory_order_relaxed),
                      g_fcCamRotW.load(std::memory_order_relaxed) };
            if (IsFiniteVec3(pos)) {
                Il2Cpp::SafeSetTransformPos(camTr, pos);
                Il2Cpp::SafeSetTransformRot(camTr, rot);
            }
        }
    }
}

static void Hook_CamPlacementUpdate(void* self, float dt, void* method) {
    if (!g_cfgState.bMasterDisable && g_fcCamLockActive.load(std::memory_order_acquire)) {
        EnforceLockedSurfaceCameraIfNeeded();
        return;
    }
    if (orig_CamPlacementUpdate) orig_CamPlacementUpdate(self, dt, method);
}
static void Hook_UniCamApplyUpdate(void* self, float dt, void* method) {
    if (!g_cfgState.bMasterDisable && g_fcCamLockActive.load(std::memory_order_acquire)) {
        EnforceLockedSurfaceCameraIfNeeded();
        return;
    }
    if (orig_UniCamApplyUpdate) orig_UniCamApplyUpdate(self, dt, method);
}

static bool IsGhostProtectedNow() {
    if (g_cfgState.bMasterDisable) return false;
    if (g_fcGhostAnchorActive.load(std::memory_order_acquire)) return true;
    return CACurrentMediaTime() < g_fcReturnGraceUntil.load(std::memory_order_acquire);
}
static void Hook_OffMapUpdate(void* self, float dt, void* method) {
    if (IsGhostProtectedNow()) return;
    if (orig_OffMapUpdate) orig_OffMapUpdate(self, dt, method);
}
static void Hook_PedKillUpdate(void* self, float dt, void* method) {
    if (IsGhostProtectedNow()) return;
    if (orig_PedKillUpdate) orig_PedKillUpdate(self, dt, method);
}
static void Hook_PedOxygenUpdate(void* self, float dt, void* method) {
    if (IsGhostProtectedNow()) return;
    if (orig_PedOxygenUpdate) orig_PedOxygenUpdate(self, dt, method);
}
static void Hook_PedKoWaterUpdate(void* self, float dt, void* method) {
    if (IsGhostProtectedNow()) return;
    if (orig_PedKoWaterUpdate) orig_PedKoWaterUpdate(self, dt, method);
}
static void Hook_SubmergeUpdate(void* self, float dt, void* method) {
    if (IsGhostProtectedNow()) return;
    if (orig_SubmergeUpdate) orig_SubmergeUpdate(self, dt, method);
}
static void Hook_UnderwaterReactUpdate(void* self, float dt, void* method) {
    if (IsGhostProtectedNow()) return;
    if (orig_UnderwaterReactUpdate) orig_UnderwaterReactUpdate(self, dt, method);
}
static void Hook_WaterAreaTriggerStay(void* self, void* other, void* method) {
    if (IsGhostProtectedNow()) return;
    if (orig_WaterAreaTriggerStay) orig_WaterAreaTriggerStay(self, other, method);
}
static void Hook_PedGravityUpdate(void* self, float dt, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) {
        if (void* pp = SafeMem::PtrAt(self, Offsets::g_cfg.offPedViewGravParams)) {
            g_pedPhysicsParamsInst.store(pp, std::memory_order_release);
            if (!g_hasSavedPedGravity) {
                int32_t g = 0;
                if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(pp) + Offsets::g_cfg.offPedPhysicsGravInAir, g)) {
                    g_savedPedGravityInAir = g;
                    g_hasSavedPedGravity = true;
                }
            }
        }
    }
    if (IsGhostProtectedNow()) return;
    if (orig_PedGravityUpdate) orig_PedGravityUpdate(self, dt, method);
}

// KillAround Datagram hooks (managed interface / vtable fallbacks)
static void Hook_PedDispRead(void* self, void* reader, void* method) {
    if (orig_PedDispRead) orig_PedDispRead(self, reader, method);
    KillAround::OnPedDisplacementRead(self);
}
static void Hook_PedDispWrite(void* self, void* writer, void* method) {
    KillAround::OnPedDisplacementWrite(self);
    if (orig_PedDispWrite) orig_PedDispWrite(self, writer, method);
}
static void Hook_PedDeadRead(void* self, void* reader, void* method) {
    if (orig_PedDeadRead) orig_PedDeadRead(self, reader, method);
    KillAround::OnPedDeadRead(self);
}
static void Hook_MeleeAtkRead(void* self, void* reader, void* method) {
    if (orig_MeleeAtkRead) orig_MeleeAtkRead(self, reader, method);
    KillAround::OnMeleeAttackRead(self);
}
static void Hook_MeleeReqWrite(void* self, void* writer, void* method) {
    KillAround::OnMeleeRequestWrite(self);
    if (orig_MeleeReqWrite) orig_MeleeReqWrite(self, writer, method);
}

static void Hook_TempWeaponSetLocal(void* self, int32_t weaponType, int32_t startAmmo, bool isInf, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) Morpeh::g_stashes.realTempWeaponSvc = self;
    if (orig_TempWeaponSetLocal) orig_TempWeaponSetLocal(self, weaponType, startAmmo, isInf, method);
}

static bool Hook_ItemTryUseLocal(void* self, int64_t itemEnt, int32_t dragSource, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) Morpeh::g_stashes.realItemSvc = self;
    return orig_ItemTryUseLocal ? orig_ItemTryUseLocal(self, itemEnt, dragSource, method) : false;
}

static void Hook_ApplyLocalFallDamage(void* self, int64_t pedViewEntity, float fallHeight, void* method) {
    if (!g_cfgState.bMasterDisable && (g_cfgState.bNoFall || IsGhostProtectedNow())) return;
    if (orig_ApplyLocalFallDamage) orig_ApplyLocalFallDamage(self, pedViewEntity, fallHeight, method);
}

static void Hook_ParkingSpawnWrite(void* self, void* writer, void* method) {
    if (!g_cfgState.bMasterDisable && g_cfgState.bVehSpawnCapture)
        g_vehSpawnCaptureDeadline = CACurrentMediaTime() + 10.0;
    if (orig_ParkingSpawnWrite) orig_ParkingSpawnWrite(self, writer, method);
}

static void Hook_VehFactoryInitParts(void* self, int64_t vehEntity, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) Morpeh::g_stashes.realVehFactorySys = self;
    if (!g_cfgState.bMasterDisable && g_cfgState.bVehSpawnCapture &&
        CACurrentMediaTime() <= g_vehSpawnCaptureDeadline && g_cfgState.pendingCarId > 0 && vehEntity > 0) {
        int32_t vIdx = Morpeh::UnpackEntityId(vehEntity);
        if (vIdx >= 0) {
            if (void* ptr = Morpeh::Get(Morpeh::g_stashes.pedSkinStash, vIdx, 4)) {
                int32_t carId = g_cfgState.pendingCarId;
                SafeMem::Write(ptr, &carId, 4);
                g_cfgState.lastStatusBanner = "Replaced spawned car with ID " + std::to_string(carId);
                g_vehSpawnCaptureDeadline = 0.0;
            }
        }
    }
    if (orig_VehFactoryInitParts) orig_VehFactoryInitParts(self, vehEntity, method);
}

static bool Hook_PedSP_Construct(void* self, int64_t ent, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) Morpeh::g_stashes.realPedSP = self;
    return orig_PedSP_Construct ? orig_PedSP_Construct(self, ent, method) : false;
}

static bool Hook_VehSP_Construct(void* self, int64_t ent, void* method) {
    if (SafeMem::IsPlausiblePtr(self)) Morpeh::g_stashes.realVehSP = self;
    return orig_VehSP_Construct ? orig_VehSP_Construct(self, ent, method) : false;
}

static bool Hook_NickTryValidate(void* firstNameStr, void* lastNameStr, void* errorsList, void* method) {
    if (!g_cfgState.bMasterDisable && g_cfgState.bNickBypass) return true;
    return orig_NickTryValidate ? orig_NickTryValidate(firstNameStr, lastNameStr, errorsList, method) : true;
}

static void* Hook_NickGetFormat(void* str, void* method) {
    return (!g_cfgState.bMasterDisable && g_cfgState.bNickBypass) ? str : (orig_NickGetFormat ? orig_NickGetFormat(str, method) : str);
}

static void* Hook_NickReplaceRegex(void* str, void* method) {
    return (!g_cfgState.bMasterDisable && g_cfgState.bNickBypass) ? str : (orig_NickReplaceRegex ? orig_NickReplaceRegex(str, method) : str);
}

static void Hook_VoiceWorkerRun(void* self, void* method) {
    if (!g_cfgState.bMasterDisable && g_cfgState.bVoiceGuard) {
        usleep(50000);
        return;
    }
    if (orig_VoiceWorkerRun) orig_VoiceWorkerRun(self, method);
}

static void* Hook_SystemInfoDeviceUnique(void* method) {
    if (!g_cfgState.bMasterDisable && g_cfgState.bDeviceIdSpoof && Il2Cpp::g_api.string_new) {
        static std::string s_spoofId;
        if (s_spoofId.empty()) {
            NSString* uuid = [[NSUUID UUID] UUIDString];
            s_spoofId = uuid ? uuid.UTF8String : "A1B2C3D4-E5F6-4789-ABCD-0123456789AB";
        }
        return Il2Cpp::g_api.string_new(s_spoofId.c_str());
    }
    return orig_SystemInfoDeviceUnique ? orig_SystemInfoDeviceUnique(method) : nullptr;
}

static bool Hook_ReturnTrue(void*, void*) {
    return true;
}

inline void EnsureFreeCamSystemPatches() {
    if (g_freeCamPatchesInstalled || !Il2Cpp::Init()) return;
    const auto& c = Offsets::g_cfg;
    const char* ns = c.nsFreeCamSystems.c_str();
    const char* sysNames[4] = {
        c.clsFreeCamActivateSys.c_str(),
        c.clsFreeCamDeactSys.c_str(),
        c.clsFreeCamSeatAddSys.c_str(),
        c.clsFreeCamSeatRemSys.c_str()
    };
    int patched = 0;
    for (int i = 0; i < 4; ++i) {
        if (void* m = Il2Cpp::ResolveMethod(ns, sysNames[i], "IsEnabled", 0)) {
            void* orig = nullptr;
            if (Il2Cpp::SwapMethodPointer(m, (void*)Hook_ReturnTrue, &orig)) {
                ++patched;
            }
        }
    }
    if (patched > 0) g_freeCamPatchesInstalled = true;
}

inline void EnsureFreeCamVoiceAndImmunityHooks() {
    if (g_freeCamVoiceHooksInstalled || !Il2Cpp::Init()) return;
    g_freeCamVoiceHooksInstalled = true;
    const auto& c = Offsets::g_cfg;

    if (void* m = Il2Cpp::ResolveMethod(c.nsFmodUnity.c_str(), c.clsStudioListener.c_str(), "Update", 0))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_StudioListenerUpdate, (void**)&orig_StudioListenerUpdate);
    if (void* m = Il2Cpp::ResolveMethod(c.nsWhisperPlayback.c_str(), c.clsSpatialParameters.c_str(), ".ctor", 0))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_SpatialCtor, (void**)&orig_SpatialCtor);
    if (void* m = Il2Cpp::ResolveMethod(c.nsVoiceSystems.c_str(), c.clsVoicePlaybackSvc.c_str(), "SetAllChatTypesMuted", 1))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_VoiceSetAllMuted, (void**)&orig_VoiceSetAllMuted);

    // Resolve all SetMuted(2) & RuntimeManager::SetListenerLocation overloads safely (TCFlyGuard + il2cpp_free, NEVER libc std::free)
    TCFlyGuard::TryRun([&]() {
        using IterFn       = void* (*)(void*, void**);
        using ParamCntFn   = uint32_t (*)(const void*);
        using ParamTypFn   = void* (*)(const void*, uint32_t);
        using TypeNameFn   = char* (*)(const void*);
        using MethNameFn   = const char* (*)(const void*);
        using Il2CppFreeFn = void (*)(void*);
        static IterFn       s_iter = (IterFn)dlsym(RTLD_DEFAULT, "il2cpp_class_get_methods");
        static ParamCntFn   s_pcnt = (ParamCntFn)dlsym(RTLD_DEFAULT, "il2cpp_method_get_param_count");
        static ParamTypFn   s_ptyp = (ParamTypFn)dlsym(RTLD_DEFAULT, "il2cpp_method_get_param");
        static TypeNameFn   s_tnam = (TypeNameFn)dlsym(RTLD_DEFAULT, "il2cpp_type_get_name");
        static MethNameFn   s_mnam = (MethNameFn)dlsym(RTLD_DEFAULT, "il2cpp_method_get_name");
        static Il2CppFreeFn s_free = (Il2CppFreeFn)dlsym(RTLD_DEFAULT, "il2cpp_free");
        if (!s_iter || !s_pcnt || !s_ptyp || !s_tnam || !s_mnam) return;

        if (void* vc = Il2Cpp::FindClass(c.nsVoiceSystems.c_str(), c.clsVoicePlaybackSvc.c_str())) {
            void* iter = nullptr;
            while (void* m = s_iter(vc, &iter)) {
                const char* mn = s_mnam(m);
                if (!mn || std::strcmp(mn, "SetMuted") != 0 || s_pcnt(m) != 2) continue;
                void* pt0 = s_ptyp(m, 0);
                char* tn0 = pt0 ? s_tnam(pt0) : nullptr;
                if (tn0) {
                    if (std::strstr(tn0, "VoiceChatType"))
                        Il2Cpp::SwapMethodPointer(m, (void*)Hook_VoiceSetMutedChat, (void**)&orig_VoiceSetMutedChat);
                    else if (std::strstr(tn0, "VoiceFmodBusType"))
                        Il2Cpp::SwapMethodPointer(m, (void*)Hook_VoiceSetMutedBus, (void**)&orig_VoiceSetMutedBus);
                    if (s_free) s_free(tn0);
                }
            }
        }
        if (void* rm = Il2Cpp::FindClass(c.nsFmodUnity.c_str(), c.clsRuntimeManager.c_str())) {
            void* rIter = nullptr;
            while (void* m = s_iter(rm, &rIter)) {
                const char* mn = s_mnam(m);
                if (!mn || std::strcmp(mn, "SetListenerLocation") != 0) continue;
                uint32_t pc = s_pcnt(m);
                void* pt0 = s_ptyp(m, 0);
                void* pt1 = (pc >= 2) ? s_ptyp(m, 1) : nullptr;
                char* tn0 = pt0 ? s_tnam(pt0) : nullptr;
                char* tn1 = pt1 ? s_tnam(pt1) : nullptr;
                if (pc == 2 && tn0 && std::strstr(tn0, "GameObject")) {
                    Il2Cpp::SwapMethodPointer(m, (void*)Hook_SetListenerLoc2, (void**)&orig_SetListenerLoc2);
                } else if (pc == 3 && tn0 && std::strstr(tn0, "Int32")) {
                    Il2Cpp::SwapMethodPointer(m, (void*)Hook_SetListenerLoc3Idx, (void**)&orig_SetListenerLoc3Idx);
                } else if (pc == 3 && tn1 && std::strstr(tn1, "Rigidbody")) {
                    Il2Cpp::SwapMethodPointer(m, (void*)Hook_SetListenerLoc3Rb, (void**)&orig_SetListenerLoc3Rb);
                } else if (pc == 4 && tn0 && std::strstr(tn0, "Int32") && tn1 && std::strstr(tn1, "GameObject")) {
                    Il2Cpp::SwapMethodPointer(m, (void*)Hook_SetListenerLoc4, (void**)&orig_SetListenerLoc4);
                }
                if (tn0 && s_free) s_free(tn0);
                if (tn1 && s_free) s_free(tn1);
            }
        }
    });

    if (void* m = Il2Cpp::ResolveMethod(c.nsMainCamSys.c_str(), c.clsCameraPlacementSys.c_str(), "OnUpdate", 1))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_CamPlacementUpdate, (void**)&orig_CamPlacementUpdate);
    if (void* m = Il2Cpp::ResolveMethod(c.nsUniversalCamSys.c_str(), c.clsUniversalCamApply.c_str(), "OnUpdate", 1))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_UniCamApplyUpdate, (void**)&orig_UniCamApplyUpdate);
    if (void* m = Il2Cpp::ResolveMethod(c.nsPedsSys.c_str(), c.clsPedPosOffMapSys.c_str(), "OnUpdate", 1))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_OffMapUpdate, (void**)&orig_OffMapUpdate);
    if (void* m = Il2Cpp::ResolveMethod(c.nsPedsSys.c_str(), c.clsPedKillSys.c_str(), "OnUpdate", 1))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_PedKillUpdate, (void**)&orig_PedKillUpdate);
    if (void* m = Il2Cpp::ResolveMethod(c.nsPedsValSys.c_str(), c.clsPedOxygenSys.c_str(), "OnUpdate", 1))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_PedOxygenUpdate, (void**)&orig_PedOxygenUpdate);
    if (void* m = Il2Cpp::ResolveMethod(c.nsPedViewSys.c_str(), c.clsPedViewGravitySys.c_str(), "OnUpdate", 1))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_PedGravityUpdate, (void**)&orig_PedGravityUpdate);
    if (void* m = Il2Cpp::ResolveMethod(c.nsPedViewSys.c_str(), c.clsPedViewKoWaterSys.c_str(), "OnUpdate", 1))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_PedKoWaterUpdate, (void**)&orig_PedKoWaterUpdate);
    if (void* m = Il2Cpp::ResolveMethod(c.nsElementsSys.c_str(), c.clsElemSubmergeSys.c_str(), "OnUpdate", 1))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_SubmergeUpdate, (void**)&orig_SubmergeUpdate);
    if (void* m = Il2Cpp::ResolveMethod(c.nsElementsSyncSys.c_str(), c.clsElemUnderwaterSys.c_str(), "OnUpdate", 1))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_UnderwaterReactUpdate, (void**)&orig_UnderwaterReactUpdate);
    if (void* m = Il2Cpp::ResolveMethod(c.nsWater.c_str(), c.clsWaterArea.c_str(), "OnTriggerStay", 1))
        Il2Cpp::SwapMethodPointer(m, (void*)Hook_WaterAreaTriggerStay, (void**)&orig_WaterAreaTriggerStay);
}

inline void InstallLazyFeatureHooks() {
    if (!g_hooksInstalled || !Il2Cpp::Init()) return;
    const auto& c = Offsets::g_cfg;

    if (g_cfgState.bFreeCam || g_cfgState.bSpectateActive || g_cfgState.bTalkFreeCam) {
        if (!g_freeCamPatchesInstalled) EnsureFreeCamSystemPatches();
        if (!g_freeCamVoiceHooksInstalled) EnsureFreeCamVoiceAndImmunityHooks();
    }

    bool wantMapHook = g_cfgState.bAutoMapMarkerTp ||
                       (g_menuIsOpenAtomic.load(std::memory_order_relaxed) &&
                        g_activeTabAtomic.load(std::memory_order_relaxed) == 2 &&
                        g_activeSubTabAtomic.load(std::memory_order_relaxed) == 1);
    if (wantMapHook && !g_mapHooksInstalled && Morpeh::g_stashes.worldReady) {
        g_mapHooksInstalled = true;
        if (void* m = Il2Cpp::ResolveMethod(c.nsMap.c_str(), c.clsNavigationGraph.c_str(), c.mthFindPath.c_str(), 3))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_NavFindPath, (void**)&orig_NavFindPath);
    }

    if ((g_cfgState.bGreenZoneBypass || g_cfgState.bZoneGuard) && !g_gzHooksInstalled) {
        g_gzHooksInstalled = true;
        if (void* m = Il2Cpp::ResolveMethod(c.nsGreenZoneSys.c_str(), c.clsGreenZoneSys.c_str(), "OnUpdate", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_GZ_Sync, (void**)&orig_GZ_Sync);
        if (void* m = Il2Cpp::ResolveMethod(c.nsInputFireSys.c_str(), c.clsInputFireSys.c_str(), "OnUpdate", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_GZ_Fire, (void**)&orig_GZ_Fire);
        if (void* m = Il2Cpp::ResolveMethod(c.nsMeleeConfirm.c_str(), c.clsMeleeConfirm.c_str(), "OnUpdate", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_GZ_Melee, (void**)&orig_GZ_Melee);
    }
    if (g_cfgState.bNoFall && !g_noFallHookInstalled) {
        g_noFallHookInstalled = true;
        if (void* m = Il2Cpp::ResolveMethod(c.nsFallDamageSys.c_str(), c.clsFallDamageSys.c_str(), "ApplyLocalFallDamage", 2))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_ApplyLocalFallDamage, (void**)&orig_ApplyLocalFallDamage);
    }
    if ((g_cfgState.bSilentAim || g_cfgState.bSilentWallbang) && !g_silentAimHooksInstalled &&
        Morpeh::g_stashes.worldReady && SilentAimHWBP::g_portTried.load(std::memory_order_acquire)) {
        g_silentAimHooksInstalled = true;
        if (void* m = Il2Cpp::ResolveMethod(c.nsWeaponSys.c_str(), c.clsWeapProduceRanged.c_str(), c.mthTryGetPenDist.c_str(), 5))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_TryGetPenDist, (void**)&orig_TryGetPenDist);
        if (void* m = Il2Cpp::ResolveMethod(c.nsWeaponSys.c_str(), c.clsWeapProduceRanged.c_str(), c.mthCanDmgSecondary.c_str(), 3))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_CanDmgSecondary, (void**)&orig_CanDmgSecondary);
        // Only swap RayCastRanged pointers if HWBP port failed to arm (never overwrite g_entry7/g_entry8 targets while HWBP owns them!)
        if (!SilentAimHWBP::g_portReady.load(std::memory_order_acquire)) {
            if (void* m = Il2Cpp::ResolveMethod(c.nsWeaponSys.c_str(), c.clsWeaponStorage.c_str(), c.mthRayCastRanged.c_str(), 7))
                Il2Cpp::SwapMethodPointer(m, (void*)Hook_RayCastRanged7, (void**)&orig_RayCastRanged7);
            if (void* m = Il2Cpp::ResolveMethod(c.nsWeaponSys.c_str(), c.clsWeaponStorage.c_str(), c.mthRayCastRanged.c_str(), 8))
                Il2Cpp::SwapMethodPointer(m, (void*)Hook_RayCastRanged8, (void**)&orig_RayCastRanged8);
        }
    }
    bool wantKillAround = g_cfgState.bKillAroundAura || g_cfgState.bKillAroundAutoRepeat ||
                          KillAround::g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed) > 0 ||
                          (g_menuIsOpenAtomic.load(std::memory_order_relaxed) &&
                           g_activeTabAtomic.load(std::memory_order_relaxed) == 0 &&
                           g_activeSubTabAtomic.load(std::memory_order_relaxed) == 3);
    if (wantKillAround && !g_killAroundHooksInstalled && Morpeh::g_stashes.worldReady) {
        g_killAroundHooksInstalled = true;
        if (void* m = Il2Cpp::ResolveMethod(c.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram", "Read", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_PedDispRead, (void**)&orig_PedDispRead);
        if (void* m = Il2Cpp::ResolveMethod(c.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram", "Write", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_PedDispWrite, (void**)&orig_PedDispWrite);
        if (void* m = Il2Cpp::ResolveMethod(c.nsIBNetPedSync.c_str(), "PedDeadDatagram", "Read", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_PedDeadRead, (void**)&orig_PedDeadRead);
        if (void* m = Il2Cpp::ResolveMethod(c.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackDatagram", "Read", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_MeleeAtkRead, (void**)&orig_MeleeAtkRead);
        if (void* m = Il2Cpp::ResolveMethod(c.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackRequestDatagram", "Write", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_MeleeReqWrite, (void**)&orig_MeleeReqWrite);
    }
    if (g_cfgState.bVehSpawnCapture && !g_vehSpawnHooksInstalled) {
        g_vehSpawnHooksInstalled = true;
        if (void* m = Il2Cpp::ResolveMethod(c.nsParkSpawnDg.c_str(), c.clsParkSpawnDg.c_str(), "Write", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_ParkingSpawnWrite, (void**)&orig_ParkingSpawnWrite);
        if (void* m = Il2Cpp::ResolveMethod(c.nsVehFactorySys.c_str(), c.clsVehFactorySys.c_str(), "InitializeParts", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_VehFactoryInitParts, (void**)&orig_VehFactoryInitParts);
    }
    if (g_cfgState.bNickBypass && !g_nickHooksInstalled) {
        g_nickHooksInstalled = true;
        if (void* m = Il2Cpp::ResolveMethod(c.nsNickUtils.c_str(), c.clsNickUtils.c_str(), "TryValidateNickname", 3))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_NickTryValidate, (void**)&orig_NickTryValidate);
        if (void* m = Il2Cpp::ResolveMethod(c.nsNickUtils.c_str(), c.clsNickUtils.c_str(), "GetNameFormat", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_NickGetFormat, (void**)&orig_NickGetFormat);
        if (void* m = Il2Cpp::ResolveMethod(c.nsNickUtils.c_str(), c.clsNickUtils.c_str(), "ReplaceUsingRegex", 1))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_NickReplaceRegex, (void**)&orig_NickReplaceRegex);
    }
    if (g_cfgState.bVoiceGuard && !g_voiceHookInstalled) {
        g_voiceHookInstalled = true;
        if (void* m = Il2Cpp::ResolveMethod(c.nsVoiceWorker.c_str(), c.clsVoiceWorker.c_str(), "Run", 0))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_VoiceWorkerRun, (void**)&orig_VoiceWorkerRun);
    }
    if (g_cfgState.bDeviceIdSpoof && !g_deviceIdHookInstalled) {
        g_deviceIdHookInstalled = true;
        if (void* m = Il2Cpp::ResolveMethod(c.nsUnity.c_str(), "SystemInfo", "get_deviceUniqueIdentifier", 0))
            Il2Cpp::SwapMethodPointer(m, (void*)Hook_SystemInfoDeviceUnique, (void**)&orig_SystemInfoDeviceUnique);
    }
}

inline void InstallAllOnce() {
    if (!Il2Cpp::Init()) return;
    if (!g_hooksInstalled) {
        const auto& c = Offsets::g_cfg;
        if (!Il2Cpp::FindClass(c.nsGreenZoneSys.c_str(), c.clsGreenZoneSys.c_str())) return; // IL2CPP game assemblies not ready yet
        // ZERO startup hooks: all 14 live game services are resolved cleanly via Initializer._container (SimpleDImple).
        // Feature hooks are installed lazily ONLY when the user turns that feature ON.
        g_hooksInstalled = true;
    }
    InstallLazyFeatureHooks();
}

} // namespace Hooks

// ============================================================================
// Feature Runtime Operations (ECS / Vehicle / Wardrobe / World / Aim / ESP / Auto)
// ============================================================================
namespace Features {

inline double g_selfTeleportGraceUntil = 0.0;
inline Vec3   g_antiTpPedAnchor{};
inline bool   g_antiTpPedAnchored = false;
inline Vec3   g_antiTpCarAnchor{};
inline bool   g_antiTpCarAnchored = false;
inline Vec3   g_killFlyReturnPos{};
inline bool   g_killFlyActive = false;
inline std::unordered_map<void*, Vec3> g_scaledTransforms;
inline std::vector<void*> g_hiddenCrosshairObjs;
inline bool   g_crosshairWasHidden = false;
inline bool   g_freeCamWasActive = false;

struct FloatingDmgPopup {
    Vec3 worldPos{};
    int  delta = 0;
    double bornAt = 0.0;
};
inline std::unordered_map<int32_t, int> g_prevPlayerHealth;
inline std::vector<FloatingDmgPopup>    g_dmgPopups;

inline void NotifySelfTeleport() {
    g_selfTeleportGraceUntil = CACurrentMediaTime() + 1.25;
    g_antiTpPedAnchored = false;
    g_antiTpCarAnchored = false;
}

inline bool ReadTransformPosition(void* tr, Vec3& outPos) {
    return Il2Cpp::SafeGetTransformPos(tr, outPos);
}

inline bool WorldToScreenViewport(void* cam, const Vec3& world, float screenW, float screenH, Vec2& out) {
    return Il2Cpp::SafeWorldToViewport(cam, world, screenW, screenH, out);
}

inline Quat YawToQuat(float yawRad) {
    float half = yawRad * 0.5f;
    return { 0.0f, std::sin(half), 0.0f, std::cos(half) };
}

inline void RepositionTarget(void* rigidbody, void* transform, const Vec3& targetPos, const Vec3& kickVel, bool restoreGravity = true) {
    if (!IsFiniteVec3(targetPos)) return;
    Vec3 zero{0, 0, 0};

    if (Il2Cpp::IsUnityNativeAlive(rigidbody)) {
        Il2Cpp::SafeSetRbKinematic(rigidbody, true);
        Il2Cpp::SafeSetRbVelocity(rigidbody, zero);
        Il2Cpp::SafeSetRbPosition(rigidbody, targetPos);
    }
    if (Il2Cpp::IsUnityNativeAlive(transform)) {
        Il2Cpp::SafeSetTransformPos(transform, targetPos);
    }
    Il2Cpp::SafePhysicsSyncTransforms();
    if (Il2Cpp::IsUnityNativeAlive(rigidbody)) {
        Il2Cpp::SafeSetRbVelocity(rigidbody, zero);
        Il2Cpp::SafeSetRbGravity(rigidbody, restoreGravity);
        Il2Cpp::SafeSetRbKinematic(rigidbody, false);
        Il2Cpp::SafeSetRbVelocity(rigidbody, kickVel);
    }
}

inline int64_t ResolveTargetVehicleLogic(int64_t pedOrVehEntity) {
    Morpeh::RefreshStoragesIfNeeded();
    if (pedOrVehEntity <= 0) pedOrVehEntity = Morpeh::GetLocalPedEntity();
    if (pedOrVehEntity <= 0) return -1;
    int64_t seated = Morpeh::GetLocalVehicleLogicEntity(pedOrVehEntity);
    if (seated > 0) return seated;
    return Morpeh::Has(Morpeh::g_stashes.vehicleStash, pedOrVehEntity) ? pedOrVehEntity : -1;
}

inline bool GetLocalPedViewData(int64_t& outPed, int64_t& outView, void*& outPvPtr) {
    outPed = Morpeh::GetLocalPedEntity();
    outView = (outPed > 0) ? Morpeh::GetViewEntity(outPed) : 0;
    outPvPtr = nullptr;
    if (outView <= 0) return false;
    int32_t vid = Morpeh::UnpackEntityId(outView);
    if (vid < 0) return false;
    outPvPtr = Morpeh::Get(Morpeh::g_stashes.pedViewStash, vid, 384);
    return (outPvPtr != nullptr);
}

inline bool GetLocalPosition(Vec3& outPos) {
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped > 0) {
        int32_t pid = Morpeh::UnpackEntityId(ped);
        if (pid >= 0) {
            if (void* p = Morpeh::Get(Morpeh::g_stashes.positionStash, pid, sizeof(Vec3))) {
                if (SafeMem::ReadVal(p, outPos) && IsFiniteVec3(outPos)) return true;
            }
        }
        int64_t view = Morpeh::GetViewEntity(ped);
        if (void* tr = Morpeh::GetEntityTransform(view)) {
            if (ReadTransformPosition(tr, outPos)) return true;
        }
    }
    return false;
}

// Instant non-sweeping Ped Warp: updates PhysicsActor interpolation buffers (0x40/0x4C) + calls
// PhysicsActor.Teleport(Vector3, Quaternion) + set_Position + ResetInterpolationPosition so walls never block!
inline void WarpCharacterActorInstant(void* actor, void* rb, void* tr, const Vec3& dest,
                                      const Vec3& velAfter = {0, 0, 0}, bool holdKinematic = false, bool restoreGravity = true) {
    if (!IsFiniteVec3(dest)) return;
    const auto& cfg = Offsets::g_cfg;
    Vec3 zero{0, 0, 0};

    if (actor && Il2Cpp::IsUnityNativeAlive(actor)) {
        Vec3 d = dest;
        Quat q{0, 0, 0, 1};
        SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offPhysActorStartPos,  &d, sizeof(Vec3));
        SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offPhysActorTargetPos, &d, sizeof(Vec3));
        SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offCharActorInVel,     &zero, sizeof(Vec3));
        SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offCharActorPreVel,    &zero, sizeof(Vec3));
        SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offCharActorPostVel,   &zero, sizeof(Vec3));
        SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offCharActorExtVel,    &zero, sizeof(Vec3));

        if (!holdKinematic) {
            Il2Cpp::SafeSetBehaviourEnabled(actor, true);
            if (Il2Cpp::g_unity.mPhysActor_Teleport2) {
                void* args[2] = { &d, &q };
                Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPhysActor_Teleport2, actor, args);
            } else if (Il2Cpp::g_unity.mPhysActor_SetPos) {
                void* args[1] = { &d };
                Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPhysActor_SetPos, actor, args);
            }
            if (Il2Cpp::g_unity.mPhysActor_SetVel) {
                Vec3 v = velAfter;
                void* va[1] = { &v };
                Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPhysActor_SetVel, actor, va);
            }
            if (Il2Cpp::g_unity.mPhysActor_ResetInterp) {
                Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPhysActor_ResetInterp, actor, nullptr);
            }
        } else {
            Il2Cpp::SafeSetBehaviourEnabled(actor, false);
        }
    }

    if (rb && Il2Cpp::IsRigidbodyAlive(rb)) {
        Il2Cpp::SafeSetRbKinematic(rb, true);
        Il2Cpp::SafeSetRbGravity(rb, false);
        Il2Cpp::SafeSetRbVelocity(rb, zero);
        Il2Cpp::SafeSetRbPosition(rb, dest);
    }
    if (tr && Il2Cpp::IsTransformAlive(tr)) {
        Il2Cpp::SafeSetTransformPos(tr, dest);
    }
    Il2Cpp::SafePhysicsSyncTransforms();

    if (!holdKinematic && rb && Il2Cpp::IsRigidbodyAlive(rb)) {
        Il2Cpp::SafeSetRbVelocity(rb, zero);
        Il2Cpp::SafeSetRbGravity(rb, restoreGravity);
        Il2Cpp::SafeSetRbKinematic(rb, false);
        Il2Cpp::SafeSetRbVelocity(rb, velAfter);
    }
}

inline bool TeleportLocalTo(const Vec3& dest, const Vec3& kickVel = {0, 0, 0}, bool restoreGravity = true) {
    if (!IsFiniteVec3(dest)) return false;
    NotifySelfTeleport();
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped <= 0) return false;

    int64_t veh = Morpeh::GetLocalVehicleLogicEntity(ped);
    if (veh > 0) {
        int64_t vehView = Morpeh::GetViewEntity(veh);
        void* vRb = Morpeh::GetEntityRigidbody(vehView);
        void* vTr = Morpeh::GetEntityTransform(vehView);
        if (vRb || vTr) {
            RepositionTarget(vRb, vTr, dest, kickVel, restoreGravity);
            Morpeh::SetComponent(Morpeh::g_stashes.positionStash, veh, &dest, sizeof(Vec3));
            Morpeh::SetComponent(Morpeh::g_stashes.positionStash, ped, &dest, sizeof(Vec3));
            return true;
        }
    }

    int64_t view = Morpeh::GetViewEntity(ped);
    int32_t vid = (view > 0) ? Morpeh::UnpackEntityId(view) : -1;
    void* pv = (vid >= 0) ? Morpeh::Get(Morpeh::g_stashes.pedViewStash, vid, 384) : nullptr;
    if (pv) {
        void* actor = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewActor);
        void* rb    = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewRigidbody);
        void* tr    = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewRoot);
        WarpCharacterActorInstant(actor, rb, tr, dest, kickVel, false, restoreGravity);
    }
    Morpeh::SetComponent(Morpeh::g_stashes.positionStash, ped, &dest, sizeof(Vec3));
    if (view > 0) Morpeh::SetComponent(Morpeh::g_stashes.positionStash, view, &dest, sizeof(Vec3));
    if (Morpeh::g_stashes.setPosRotStash) Morpeh::SetEmpty(Morpeh::g_stashes.setPosRotStash, ped);
    return true;
}

// ============================================================================
// Vehicle Tuning, Paint, Tint, Rims, Bodykit & Auto Repair
// ============================================================================
inline void ApplyCarTuningTick(int64_t pedOrVehEntity) {
    if (!g_cfgState.bCarHack) return;
    int64_t logicVeh = ResolveTargetVehicleLogic(pedOrVehEntity);
    if (logicVeh <= 0) return;

    int64_t viewVeh = Morpeh::GetViewEntity(logicVeh);
    int64_t targetEnt = (viewVeh > 0) ? viewVeh : logicVeh;
    int32_t entId = Morpeh::UnpackEntityId(targetEnt);
    if (entId < 0) return;

    float mult = std::max(1.0f, g_cfgState.carSpeed);
    int32_t internalBoost = static_cast<int32_t>(std::lround(std::clamp((mult - 1.0f) * 10.0f, 0.0f, Offsets::CarTuningLimits::kInternalMax)));
    float addTorque       = std::clamp((mult - 1.0f) * 800.0f, 0.0f, Offsets::CarTuningLimits::kMaxTorqueBoost);
    float speedRestrict   = Offsets::CarTuningLimits::kSpeedRestrictMax;

    if (void* p = Morpeh::Get(Morpeh::g_stashes.vehInternalsStash, entId, 24)) {
        SafeMem::Write(p, &internalBoost, 4);
        SafeMem::Write(reinterpret_cast<uint8_t*>(p) + 4, &internalBoost, 4);
    }
    if (void* p = Morpeh::Get(Morpeh::g_stashes.vehAddTorqueStash, entId, 4))
        SafeMem::Write(p, &addTorque, 4);
    if (void* p = Morpeh::Get(Morpeh::g_stashes.vehSpeedRestrictStash, entId, 4))
        SafeMem::Write(p, &speedRestrict, 4);
}

inline void ApplyCarPaintColor(int64_t pedOrVehEnt, const float rgba[4]) {
    int64_t logicVeh = ResolveTargetVehicleLogic(pedOrVehEnt);
    if (logicVeh <= 0) {
        g_cfgState.lastStatusBanner = "Enter a vehicle first to apply paint";
        return;
    }
    Morpeh::SetComponent(Morpeh::g_stashes.vehPaintStash, logicVeh, rgba, 16);
    Morpeh::SetEmpty(Morpeh::g_stashes.vehPaintDirtyStash, logicVeh);
    g_cfgState.lastStatusBanner = "Applied vehicle body paint";
}

inline void ApplyCarWindowTint(int64_t pedOrVehEnt, const float rgba[4]) {
    int64_t logicVeh = ResolveTargetVehicleLogic(pedOrVehEnt);
    if (logicVeh <= 0) {
        g_cfgState.lastStatusBanner = "Enter a vehicle first to apply window tint";
        return;
    }
    Morpeh::SetComponent(Morpeh::g_stashes.vehWinStash, logicVeh, rgba, 16);
    Morpeh::SetEmpty(Morpeh::g_stashes.vehWinDirtyStash, logicVeh);
    g_cfgState.lastStatusBanner = "Applied window tint";
}

inline void ApplyRimIdAndColor(int64_t pedOrVehEnt, int32_t rimId, const float rgba[4]) {
    int64_t logicVeh = ResolveTargetVehicleLogic(pedOrVehEnt);
    if (logicVeh <= 0) {
        g_cfgState.lastStatusBanner = "Enter a vehicle first to update rims";
        return;
    }
    Morpeh::SetComponent(Morpeh::g_stashes.vehWheelsStash, logicVeh, &rimId, 4);
    Morpeh::SetComponent(Morpeh::g_stashes.vehRimColStash, logicVeh, rgba, 16);
    int64_t viewVeh = Morpeh::GetViewEntity(logicVeh);
    if (viewVeh > 0) Morpeh::SetComponent(Morpeh::g_stashes.vehRimIdStash, viewVeh, &rimId, 4);
    Morpeh::SetEmpty(Morpeh::g_stashes.vehRimDirtyStash, logicVeh);
    Morpeh::SetEmpty(Morpeh::g_stashes.vehRimColDirtyStash, logicVeh);
    g_cfgState.lastStatusBanner = "Updated rims #" + std::to_string(rimId);
}

inline void ApplyVehTuneExternal(int64_t pedOrVehEnt, const int parts[10]) {
    int64_t logicVeh = ResolveTargetVehicleLogic(pedOrVehEnt);
    if (logicVeh <= 0) {
        g_cfgState.lastStatusBanner = "Enter a vehicle first to apply bodykit";
        return;
    }
    int32_t buf[10];
    for (int i = 0; i < 10; ++i) buf[i] = parts[i];
    Morpeh::SetComponent(Morpeh::g_stashes.vehTuneExtStash, logicVeh, buf, sizeof(buf));
    Morpeh::SetEmpty(Morpeh::g_stashes.vehTuneExtDirtyStash, logicVeh);
    g_cfgState.lastStatusBanner = "Applied exterior vehicle tuning";
}

// ============================================================================
// Wardrobe, Accessories, Cars, Emotes, Weapons & Outfit Copy
// ============================================================================
inline void ApplySkinLocal(int32_t skinId, uint16_t gender) {
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped <= 0 || skinId < 0) {
        g_cfgState.lastStatusBanner = "Skin apply skipped: join world first";
        return;
    }
    int32_t pedIdx = Morpeh::UnpackEntityId(ped);
    int64_t viewEnt = Morpeh::GetViewEntity(ped);
    void* pIdArg[1] = { &ped };
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.realPedSP) && Il2Cpp::g_unity.mPedSP_Deconstruct)
        Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPedSP_Deconstruct, Morpeh::g_stashes.realPedSP, pIdArg);
    if (void* ptr = Morpeh::Get(Morpeh::g_stashes.pedSkinStash, pedIdx, 4))
        SafeMem::Write(ptr, &skinId, 4);
    if (viewEnt > 0) {
        int32_t vIdx = Morpeh::UnpackEntityId(viewEnt);
        if (void* pvPtr = Morpeh::Get(Morpeh::g_stashes.pedViewStash, vIdx, 384)) {
            int32_t gEnum = (gender != 0) ? 134479874 : 67109376; // PedGender::Female vs Male
            void* pvCls = Il2Cpp::FindClass(Offsets::g_cfg.nsPedViewComponents.c_str(), Offsets::g_cfg.compPedView.c_str());
            size_t gOff = Il2Cpp::StructFieldOffset(pvCls, "pedGender", Offsets::g_cfg.offPedViewGender);
            SafeMem::Write(reinterpret_cast<uint8_t*>(pvPtr) + gOff, &gEnum, 4);
        }
    }
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.realPedSP) && Il2Cpp::g_unity.mPedSP_Construct)
        Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPedSP_Construct, Morpeh::g_stashes.realPedSP, pIdArg);
    g_cfgState.lastStatusBanner = "Applied Ped Skin ID " + std::to_string(skinId);
}

inline void ApplyClothesLocal(uint16_t clothId, int32_t genderFlag) {
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped <= 0) {
        g_cfgState.lastStatusBanner = "Clothes apply skipped: join world first";
        return;
    }
    void* pIdArg[1] = { &ped };
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.realPedSP) && Il2Cpp::g_unity.mPedSP_Deconstruct)
        Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPedSP_Deconstruct, Morpeh::g_stashes.realPedSP, pIdArg);
    Morpeh::SetComponent(Morpeh::g_stashes.pedClothesStash, ped, &clothId, 2);
    Morpeh::SetEmpty(Morpeh::g_stashes.pedClothesChangedStash, ped);
    (void)genderFlag;
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.realPedSP) && Il2Cpp::g_unity.mPedSP_Construct)
        Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPedSP_Construct, Morpeh::g_stashes.realPedSP, pIdArg);
    g_cfgState.lastStatusBanner = "Applied Clothes ID " + std::to_string(clothId);
}

inline void ApplyAccessoryLocal(int32_t accId, int32_t slot) {
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped <= 0) {
        g_cfgState.lastStatusBanner = "Accessory apply skipped: join world first";
        return;
    }
    if (void* accStash = Morpeh::FetchComponentStash(Morpeh::GetWorld(), "dev.Scripts.ECS.Morpeh.Features.PedCustomizationFeature.Components.Ped", "PedCustomizationActiveAccessories")) {
        int32_t pedId = Morpeh::UnpackEntityId(ped);
        if (void* dictPtr = Morpeh::Get(accStash, pedId, 8)) {
            void* dictObj = SafeMem::PtrAt(dictPtr, 0);
            if (SafeMem::IsPlausiblePtr(dictObj)) {
                void* dictCls = Il2Cpp::g_api.object_get_class(dictObj);
                void* setItem = dictCls ? Il2Cpp::g_api.class_get_method_from_name(dictCls, "set_Item", 2) : nullptr;
                if (setItem) {
                    void* args[2] = { &slot, &accId };
                    Il2Cpp::SafeInvoke(setItem, dictObj, args);
                }
            }
        }
    }
    if (void* loadStash = Morpeh::FetchComponentStash(Morpeh::GetWorld(), "dev.Scripts.ECS.Morpeh.Features.PedCustomizationFeature.Components.Ped", "PedCustomizationLoadAsset")) {
        Morpeh::SetEmpty(loadStash, ped);
    }
    g_cfgState.lastStatusBanner = "Applied Accessory ID " + std::to_string(accId);
}

inline void StripAllAccessoriesLocal() {
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped <= 0) return;
    if (void* accStash = Morpeh::FetchComponentStash(Morpeh::GetWorld(), "dev.Scripts.ECS.Morpeh.Features.PedCustomizationFeature.Components.Ped", "PedCustomizationActiveAccessories")) {
        int32_t pedId = Morpeh::UnpackEntityId(ped);
        if (void* dictPtr = Morpeh::Get(accStash, pedId, 8)) {
            void* dictObj = SafeMem::PtrAt(dictPtr, 0);
            if (SafeMem::IsPlausiblePtr(dictObj)) {
                void* dictCls = Il2Cpp::g_api.object_get_class(dictObj);
                void* clearM = dictCls ? Il2Cpp::g_api.class_get_method_from_name(dictCls, "Clear", 0) : nullptr;
                if (clearM) Il2Cpp::SafeInvoke(clearM, dictObj, nullptr);
            }
        }
    }
    if (void* loadStash = Morpeh::FetchComponentStash(Morpeh::GetWorld(), "dev.Scripts.ECS.Morpeh.Features.PedCustomizationFeature.Components.Ped", "PedCustomizationLoadAsset")) {
        Morpeh::SetEmpty(loadStash, ped);
    }
    g_cfgState.lastStatusBanner = "Stripped all local accessories";
}

inline void ApplyCarLocal(int32_t carId) {
    int64_t logicVeh = ResolveTargetVehicleLogic(-1);
    if (logicVeh <= 0) {
        g_cfgState.lastStatusBanner = "Enter a vehicle first to apply model ID";
        return;
    }
    int32_t vIdx = Morpeh::UnpackEntityId(logicVeh);
    void* pIdArg[1] = { &logicVeh };
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.realVehSP) && Il2Cpp::g_unity.mVehSP_Deconstruct)
        Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mVehSP_Deconstruct, Morpeh::g_stashes.realVehSP, pIdArg);
    if (void* ptr = Morpeh::Get(Morpeh::g_stashes.pedSkinStash, vIdx, 4))
        SafeMem::Write(ptr, &carId, 4);
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.realVehSP) && Il2Cpp::g_unity.mVehSP_Construct)
        Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mVehSP_Construct, Morpeh::g_stashes.realVehSP, pIdArg);
    g_cfgState.lastStatusBanner = "Applied Car ID " + std::to_string(carId);
}

inline void ApplyEmoteLocal(int32_t emoteId) {
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped <= 0) {
        g_cfgState.lastStatusBanner = "Emote skipped: join world first";
        return;
    }
    int64_t viewEnt = Morpeh::GetViewEntity(ped);
    int32_t vId = (viewEnt > 0) ? Morpeh::UnpackEntityId(viewEnt) : -1;
    if (vId >= 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.pedViewFsmStash) && Il2Cpp::g_unity.mPedFSM_ForceEnter) {
        if (void* fsmPtr = Morpeh::Get(Morpeh::g_stashes.pedViewFsmStash, vId, 8)) {
            void* fsm = SafeMem::PtrAt(fsmPtr, 0);
            if (SafeMem::IsPlausiblePtr(fsm)) {
                bool forceReEnter = true;
                void* args[2] = { &emoteId, &forceReEnter };
                Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPedFSM_ForceEnter, fsm, args);
            }
        }
    }
    g_cfgState.lastStatusBanner = "Triggered Emote ID " + std::to_string(emoteId);
}

inline void ApplyWeaponLocal(int32_t weaponItemId) {
    Morpeh::RefreshStoragesIfNeeded();
    if (weaponItemId <= 0) return;
    void* svc = Morpeh::g_stashes.realTempWeaponSvc;
    if (!SafeMem::IsPlausiblePtr(svc)) {
        svc = Il2Cpp::GetSingletonStaticInstance(Offsets::g_cfg.nsWeaponSys.c_str(), Offsets::g_cfg.clsTempWeapSvc.c_str());
    }
    if (SafeMem::IsPlausiblePtr(svc) && Il2Cpp::g_unity.mTempWeapon_SetLocal) {
        int32_t ammo = 999;
        bool infAmmo = true;
        void* args[3] = { &weaponItemId, &ammo, &infAmmo };
        Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mTempWeapon_SetLocal, svc, args);
        g_cfgState.lastStatusBanner = "Equipped Temp Weapon #" + std::to_string(weaponItemId);
        return;
    }
    // ECS fallback: write PedWeapon item id directly on local ped
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped > 0) {
        int32_t pedId = Morpeh::UnpackEntityId(ped);
        if (void* wPtr = Morpeh::Get(Morpeh::g_stashes.pedWeaponStash, pedId, 32)) {
            SafeMem::Write(wPtr, &weaponItemId, 4);
            g_cfgState.lastStatusBanner = "Wrote Local PedWeapon #" + std::to_string(weaponItemId);
            return;
        }
    }
    g_cfgState.lastStatusBanner = "Equip a weapon once so TempWeaponService binds";
}

inline void CopyPlayerOutfit(const ESPPlayer& pl) {
    if (pl.skinId > 0) ApplySkinLocal(pl.skinId, 0);
    if (pl.clothId > 0) ApplyClothesLocal(pl.clothId, 0);
    g_cfgState.lastStatusBanner = "Copied outfit from " + (pl.name.empty() ? "Player" : pl.name);
}

inline bool MatchConsumableToken(const char* itemName, bool wantHeal) {
    if (!itemName) return false;
    if (wantHeal) {
        for (int i = 0; i < Catalogs::kHealStrongTokensCount; ++i)
            if (ThemeEngine::CaseInsensitiveContains(itemName, Catalogs::kHealStrongTokens[i])) return true;
        for (int i = 0; i < Catalogs::kHealFoodTokensCount; ++i)
            if (ThemeEngine::CaseInsensitiveContains(itemName, Catalogs::kHealFoodTokens[i])) return true;
        return false;
    }
    for (int i = 0; i < Catalogs::kArmorTokenRawCount; ++i)
        if (ThemeEngine::CaseInsensitiveContains(itemName, Catalogs::kArmorTokenRaw[i])) return true;
    for (int i = 0; i < Catalogs::kArmorTokenNormCount; ++i)
        if (ThemeEngine::CaseInsensitiveContains(itemName, Catalogs::kArmorTokenNorm[i])) return true;
    return false;
}

// ============================================================================
// Auto Heal, Auto Vest, ZoneGuard, Weapon Mods, Movement & Scale
// ============================================================================
inline bool UseInventoryConsumable(bool wantHeal) {
    void* svc = Morpeh::g_stashes.realItemSvc;
    if (!SafeMem::IsPlausiblePtr(svc)) {
        svc = Il2Cpp::GetSingletonStaticInstance(Offsets::g_cfg.nsItemsSvc.c_str(), Offsets::g_cfg.clsItemSvc.c_str());
    }
    if (!SafeMem::IsPlausiblePtr(svc) || !Il2Cpp::g_unity.mItemSvc_TryUseLocal) return false;
    if (!SafeMem::IsPlausiblePtr(Morpeh::g_stashes.localItemStash)) return false;

    int64_t foundItem = 0;
    Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.localItemStash, [&](int32_t itemId, int) {
        if (foundItem != 0) return;
        int32_t cnt = 1;
        if (void* cPtr = Morpeh::Get(Morpeh::g_stashes.itemCountStash, itemId, 4))
            SafeMem::ReadVal(cPtr, cnt);
        if (cnt <= 0) return;
        if (void* nPtr = Morpeh::Get(Morpeh::g_stashes.itemNameStash, itemId, 8)) {
            std::string name = Il2Cpp::ReadManagedString(SafeMem::PtrAt(nPtr, 0));
            if (MatchConsumableToken(name.c_str(), wantHeal)) {
                foundItem = Morpeh::GetEntityById(itemId);
            }
        }
    });
    if (foundItem == 0) return false;
    int32_t dragSrc = 0;
    void* args[2] = { &foundItem, &dragSrc };
    Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mItemSvc_TryUseLocal, svc, args);
    return true;
}

inline void TickAutoHealAndVest(int64_t localPed) {
    if (localPed <= 0) return;
    int32_t pid = Morpeh::UnpackEntityId(localPed);
    if (pid < 0) return;
    double now = CACurrentMediaTime();

    if (g_cfgState.bAutoHeal) {
        static double s_lastHeal = 0.0;
        if (now - s_lastHeal >= 1.8) {
            int32_t hp = 100;
            if (void* hPtr = Morpeh::Get(Morpeh::g_stashes.healthStash, pid, 4))
                SafeMem::ReadVal(hPtr, hp);
            if (hp > 0 && hp <= g_cfgState.autoHealUnderHp) {
                if (UseInventoryConsumable(true)) {
                    s_lastHeal = now;
                    g_cfgState.lastStatusBanner = "AutoHeal triggered at " + std::to_string(hp) + " HP";
                }
            }
        }
    }

    if (g_cfgState.bAutoVest) {
        static double s_lastVest = 0.0;
        if (now - s_lastVest >= 2.0) {
            int32_t aVal = 0; uint8_t aType = 0;
            if (void* aPtr = Morpeh::Get(Morpeh::g_stashes.armorStash, pid, 8)) {
                SafeMem::ReadVal(aPtr, aVal);
                SafeMem::ReadVal(reinterpret_cast<uint8_t*>(aPtr) + 4, aType);
            }
            int maxA = std::max(1, Offsets::ArmorMaxForType(aType));
            int pct = (aVal * 100) / maxA;
            if (pct <= g_cfgState.autoVestUnderPct) {
                if (UseInventoryConsumable(false)) {
                    s_lastVest = now;
                    g_cfgState.lastStatusBanner = "AutoVest triggered at " + std::to_string(pct) + "%";
                }
            }
        }
    }
}

inline void TickZoneGuard(int64_t localPed) {
    if (!g_cfgState.bZoneGuard && !g_cfgState.bGreenZoneBypass && !g_cfgState.bFlyHack && !g_cfgState.bCarFly) return;
    if (Morpeh::g_stashes.weaponNotAllowedStash) Morpeh::RemoveAll(Morpeh::g_stashes.weaponNotAllowedStash);
    if (localPed > 0 && (g_cfgState.bZoneGuard || g_cfgState.bFlyHack || g_cfgState.bCarFly)) {
        if (Morpeh::g_stashes.pedInZoneStash)       Morpeh::Remove(Morpeh::g_stashes.pedInZoneStash, localPed);
        if (Morpeh::g_stashes.pedBlockedInputStash) Morpeh::Remove(Morpeh::g_stashes.pedBlockedInputStash, localPed);
        if (Morpeh::g_stashes.elementFrozenStash)   Morpeh::Remove(Morpeh::g_stashes.elementFrozenStash, localPed);
    }
}

inline void TickWeaponMods(int64_t localPed) {
    if (!g_cfgState.bInstantReload && !g_cfgState.bNoRecoil && !g_cfgState.bRapidFire && !g_cfgState.bSilentAim) return;
    if (localPed <= 0) return;

    if (g_cfgState.bRapidFire && Morpeh::g_stashes.weaponAttackCdStash) {
        Morpeh::RemoveAll(Morpeh::g_stashes.weaponAttackCdStash);
    }
    if (g_cfgState.bNoRecoil) {
        if (Morpeh::g_stashes.weaponRecoilStash)    Morpeh::RemoveAll(Morpeh::g_stashes.weaponRecoilStash);
        if (Morpeh::g_stashes.camWeaponRecoilStash) Morpeh::RemoveAll(Morpeh::g_stashes.camWeaponRecoilStash);
    }

    int64_t viewEnt = Morpeh::GetViewEntity(localPed);
    int32_t vid = (viewEnt > 0) ? Morpeh::UnpackEntityId(viewEnt) : -1;
    if (vid < 0 || !Morpeh::g_stashes.pedViewWeaponLinkStash || !Morpeh::g_stashes.weaponViewParamsStash) return;

    if (void* linkPtr = Morpeh::Get(Morpeh::g_stashes.pedViewWeaponLinkStash, vid, 8)) {
        int64_t weapEnt = 0;
        if (SafeMem::ReadVal(linkPtr, weapEnt) && weapEnt != 0) {
            int32_t wid = Morpeh::UnpackEntityId(weapEnt);
            if (wid >= 0) {
                if (void* wvp = Morpeh::Get(Morpeh::g_stashes.weaponViewParamsStash, wid, 16)) {
                    void* lvlParams = SafeMem::PtrAt(wvp, 8);
                    if (SafeMem::IsPlausiblePtr(lvlParams)) {
                        if (g_cfgState.bInstantReload) {
                            float fastReload = 0.05f;
                            SafeMem::Write(reinterpret_cast<uint8_t*>(lvlParams) + Offsets::g_cfg.offWeapLvlReloadTime, &fastReload, 4);
                        }
                        if (g_cfgState.bRapidFire) {
                            float curSpd = 0.0f;
                            if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(lvlParams) + Offsets::g_cfg.offWeapLvlAttackSpeed, curSpd) &&
                                curSpd > 0.01f && curSpd < 20.0f) {
                                float boosted = std::max(curSpd, g_cfgState.rapidFireMult * 4.0f);
                                SafeMem::Write(reinterpret_cast<uint8_t*>(lvlParams) + Offsets::g_cfg.offWeapLvlAttackSpeed, &boosted, 4);
                            }
                        }
                        if (g_cfgState.bSilentAim) {
                            float curDist = 0.0f;
                            if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(lvlParams) + Offsets::g_cfg.offWeapLvlDistance, curDist) &&
                                curDist < g_cfgState.saRadius) {
                                float d = g_cfgState.saRadius;
                                SafeMem::Write(reinterpret_cast<uint8_t*>(lvlParams) + Offsets::g_cfg.offWeapLvlDistance, &d, 4);
                            }
                        }
                    }
                }
            }
        }
    }
}

inline void TickMovementMods(float dt) {
    int64_t ped = 0, view = 0;
    void* pv = nullptr;
    bool hasPv = GetLocalPedViewData(ped, view, pv);
    if (!hasPv || !pv) return;

    void* rb = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewRigidbody);
    void* tr = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewRoot);
    void* actor = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewActor);

    // Super Jump (PhysicsActor::VerticalVelocity primary + Rigidbody fallback)
    if (g_cfgState.bSuperJump) {
        static double s_lastJumpBoost = 0.0;
        double now = CACurrentMediaTime();
        bool boosted = false;
        if (SafeMem::IsPlausiblePtr(actor) && Il2Cpp::g_unity.mPhysActor_GetVertVel && Il2Cpp::g_unity.mPhysActor_SetVertVel) {
            Vec3 vvel{};
            if (void* u = Il2Cpp::Unbox(Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPhysActor_GetVertVel, actor, nullptr))) {
                if (SafeMem::ReadVal(u, vvel) && IsFiniteVec3(vvel) &&
                    vvel.y > 0.8f && vvel.y < g_cfgState.superJumpForce * 0.85f && (now - s_lastJumpBoost) > 0.35) {
                    if (Il2Cpp::g_unity.mCharActor_ForceNotGrounded) {
                        int32_t frames = 3;
                        void* fa[1] = { &frames };
                        Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mCharActor_ForceNotGrounded, actor, fa);
                    }
                    vvel.y = g_cfgState.superJumpForce;
                    void* va[1] = { &vvel };
                    Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPhysActor_SetVertVel, actor, va);
                    s_lastJumpBoost = now;
                    boosted = true;
                }
            }
        }
        if (!boosted && Il2Cpp::IsUnityNativeAlive(rb)) {
            Vec3 vel{};
            if (Il2Cpp::SafeGetRbVelocity(rb, vel) &&
                vel.y > 0.8f && vel.y < g_cfgState.superJumpForce * 0.85f && (now - s_lastJumpBoost) > 0.35) {
                vel.y = g_cfgState.superJumpForce;
                Il2Cpp::SafeSetRbVelocity(rb, vel);
                s_lastJumpBoost = now;
            }
        }
    }

    // Super Punch / SpeedHack Animator multiplier
    if (Il2Cpp::IsUnityObjectSafe(actor)) {
        static float s_lastAnimSpd = 1.0f;
        float desiredAnim = 1.0f;
        if (g_cfgState.bSuperPunch) desiredAnim = std::max(desiredAnim, g_cfgState.superPunchSpeed);
        if (g_cfgState.bSpeedHack)  desiredAnim = std::max(desiredAnim, g_cfgState.speedMultiplier);
        if (std::fabs(desiredAnim - s_lastAnimSpd) > 0.05f || desiredAnim > 1.05f) {
            if (void* anim = SafeMem::PtrAt(actor, Offsets::g_cfg.offPhysicsActorAnimator)) {
                if (Il2Cpp::IsAnimatorAlive(anim)) {
                    Il2Cpp::SafeSetAnimatorSpeed(anim, desiredAnim);
                    s_lastAnimSpd = desiredAnim;
                }
            }
        }
    }

    // =========================================================================
    // Real Invisible-Floor Skywalk (Ped walks/aims/shoots 100% normally on a
    // solid invisible sky platform!) + Smooth Game-Controlled Car Fly
    // =========================================================================
    static bool  s_skyFloorActive = false;
    static void* s_skyFloorGo     = nullptr;
    static void* s_skyFloorTr     = nullptr;
    static float s_skyFloorY      = 0.0f;

    static bool  s_carFlyActive = false;
    static Vec3  s_carFlyPos{};
    static float s_carFlyYaw    = 0.0f;
    static void* s_carFlyRb     = nullptr;

    struct GameInputSnapshot {
        float pedH = 0.0f;      // InputsData.KeyHorizontal (+0x04)
        float pedV = 0.0f;      // InputsData.KeyVertical   (+0x08)
        bool  jump = false;     // InputsData.KeyJump       (+0x1C)
        bool  crouch = false;   // InputsData.KeyCrouch     (+0x15)
        bool  sprint = false;   // InputsData.KeySprint     (+0x1D)
        float carGas = 0.0f;    // VehicleViewInputs.gas / KeyVehicleGas / KeyVehicleVertical > 0
        float carBrake = 0.0f;  // VehicleViewInputs.brake / KeyVehicleReverse / KeyVehicleVertical < 0
        float carSteer = 0.0f;  // VehicleViewInputs.steer / KeyVehicleHorizontal
        bool  carHandbrake = false;
    };

    auto readLocalGameControls = [&](int64_t vehLogic) -> GameInputSnapshot {
        GameInputSnapshot in{};
        auto readInputsSlot = [&](void* inp) {
            if (!inp) return;
            const uint8_t* b = reinterpret_cast<const uint8_t*>(inp);
            float h = 0.0f, v = 0.0f, vv = 0.0f, vh = 0.0f;
            uint8_t cr = 0, jp = 0, sp = 0, hb = 0, vg = 0, vr = 0;
            if (SafeMem::ReadVal(b + 0x04, h) && std::isfinite(h) && std::fabs(h) <= 1.5f && std::fabs(h) > std::fabs(in.pedH)) in.pedH = std::clamp(h, -1.0f, 1.0f);
            if (SafeMem::ReadVal(b + 0x08, v) && std::isfinite(v) && std::fabs(v) <= 1.5f && std::fabs(v) > std::fabs(in.pedV)) in.pedV = std::clamp(v, -1.0f, 1.0f);
            if (SafeMem::ReadVal(b + 0x15, cr) && cr != 0) in.crouch = true;
            if (SafeMem::ReadVal(b + 0x1C, jp) && jp != 0) in.jump   = true;
            if (SafeMem::ReadVal(b + 0x1D, sp) && sp != 0) in.sprint = true;
            if (SafeMem::ReadVal(b + 0x20, hb) && hb != 0) in.carHandbrake = true;
            if (SafeMem::ReadVal(b + 0x21, vg) && vg != 0) in.carGas   = std::max(in.carGas, 1.0f);
            if (SafeMem::ReadVal(b + 0x22, vr) && vr != 0) in.carBrake = std::max(in.carBrake, 1.0f);
            if (SafeMem::ReadVal(b + 0x24, vv) && std::isfinite(vv) && std::fabs(vv) <= 1.5f) {
                if (vv > 0.10f) in.carGas   = std::max(in.carGas, std::clamp(vv, 0.0f, 1.0f));
                if (vv < -0.10f) in.carBrake = std::max(in.carBrake, std::clamp(-vv, 0.0f, 1.0f));
            }
            if (SafeMem::ReadVal(b + 0x28, vh) && std::isfinite(vh) && std::fabs(vh) <= 1.5f && std::fabs(vh) > std::fabs(in.carSteer)) {
                in.carSteer = std::clamp(vh, -1.0f, 1.0f);
            }
        };

        bool gotLocalInp = false;
        if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.inputsDataStash)) {
            int32_t localPlayerId = Morpeh::GetLocalPlayerId();
            int32_t localPedId = Morpeh::UnpackEntityId(ped);
            if (localPlayerId >= 0) {
                if (void* p = Morpeh::Get(Morpeh::g_stashes.inputsDataStash, localPlayerId, 88)) {
                    readInputsSlot(p);
                    gotLocalInp = true;
                }
            }
            if (!gotLocalInp && localPedId >= 0) {
                if (void* p = Morpeh::Get(Morpeh::g_stashes.inputsDataStash, localPedId, 88)) {
                    readInputsSlot(p);
                    gotLocalInp = true;
                }
            }
            if (!gotLocalInp) {
                int seen = 0;
                Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.inputsDataStash, [&](int32_t id, int) {
                    if (seen >= 2) return;
                    if (void* p = Morpeh::Get(Morpeh::g_stashes.inputsDataStash, id, 88)) {
                        readInputsSlot(p);
                        ++seen;
                    }
                });
            }
        }

        if (vehLogic > 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.vehViewInputsStash)) {
            auto readVehInp = [&](int32_t eid) {
                if (eid < 0) return false;
                void* vi = Morpeh::Get(Morpeh::g_stashes.vehViewInputsStash, eid, 24);
                if (!vi) return false;
                const uint8_t* b = reinterpret_cast<const uint8_t*>(vi);
                float gas = 0.0f, steer = 0.0f, brake = 0.0f;
                uint8_t gPr = 0, bPr = 0, hbPr = 0;
                if (SafeMem::ReadVal(b + 0x00, gas) && std::isfinite(gas))     in.carGas   = std::max(in.carGas, std::clamp(gas, 0.0f, 1.0f));
                if (SafeMem::ReadVal(b + 0x04, steer) && std::isfinite(steer) && std::fabs(steer) > std::fabs(in.carSteer))
                    in.carSteer = std::clamp(steer, -1.0f, 1.0f);
                if (SafeMem::ReadVal(b + 0x0C, brake) && std::isfinite(brake)) in.carBrake = std::max(in.carBrake, std::clamp(brake, 0.0f, 1.0f));
                if (SafeMem::ReadVal(b + 0x14, gPr) && gPr != 0)  in.carGas   = std::max(in.carGas, 1.0f);
                if (SafeMem::ReadVal(b + 0x15, bPr) && bPr != 0)  in.carBrake = std::max(in.carBrake, 1.0f);
                if (SafeMem::ReadVal(b + 0x16, hbPr) && hbPr != 0) in.carHandbrake = true;
                return true;
            };
            int64_t vv = Morpeh::GetViewEntity(vehLogic);
            if (!readVehInp(vv > 0 ? Morpeh::UnpackEntityId(vv) : -1)) {
                readVehInp(Morpeh::UnpackEntityId(vehLogic));
            }
        }

        if (std::fabs(in.pedH) < 0.12f) in.pedH = 0.0f;
        if (std::fabs(in.pedV) < 0.12f) in.pedV = 0.0f;
        if (in.carGas < 0.08f)          in.carGas = 0.0f;
        if (in.carBrake < 0.08f)        in.carBrake = 0.0f;
        if (std::fabs(in.carSteer) < 0.10f) in.carSteer = 0.0f;
        return in;
    };

    auto disableSkyFloor = [&]() {
        if (!s_skyFloorActive) return;
        if (s_skyFloorGo && Il2Cpp::IsGameObjectAlive(s_skyFloorGo)) {
            Il2Cpp::SafeSetGameObjectActive(s_skyFloorGo, false);
        }
        // Grant brief fall-damage grace when stepping off the sky floor
        Hooks::g_fcReturnGraceUntil.store(CACurrentMediaTime() + 3.5, std::memory_order_release);
        s_skyFloorActive = false;
    };

    auto restoreCarFlyPhysics = [&]() {
        if (!s_carFlyActive) return;
        if (s_carFlyRb && Il2Cpp::IsRigidbodyAlive(s_carFlyRb)) {
            Il2Cpp::SafeSetRbVelocity(s_carFlyRb, {0, 0, 0});
            Il2Cpp::SafeSetRbGravity(s_carFlyRb, true);
            Il2Cpp::SafeSetRbKinematic(s_carFlyRb, false);
        }
        s_carFlyActive = false;
        s_carFlyRb = nullptr;
    };

    bool pedFly = g_cfgState.bFlyHack && !g_cfgState.bFreeCam && !g_cfgState.bSpectateActive;
    bool carFly = g_cfgState.bCarFly && !g_cfgState.bFreeCam && !g_cfgState.bSpectateActive;
    int64_t veh = Morpeh::GetLocalVehicleLogicEntity(ped);

    if (!pedFly || veh > 0) disableSkyFloor();
    if (!carFly || veh <= 0) restoreCarFlyPhysics();

    if (pedFly || carFly) {
        void* cam = Il2Cpp::SafeCameraGetMain();
        void* camTr = cam ? Il2Cpp::SafeGetTransform(cam) : nullptr;
        Vec3 fwd{0, 0, 1}, right{1, 0, 0};
        if (camTr) {
            Il2Cpp::SafeGetTransformForward(camTr, fwd);
            Il2Cpp::SafeGetTransformRight(camTr, right);
        }
        GameInputSnapshot gi = readLocalGameControls(veh);

        // --- CAR FLY (100% controlled by Game Gas / Brake / Steer + Camera Pitch!) ---
        if (carFly && veh > 0) {
            int64_t vehView = Morpeh::GetViewEntity(veh);
            void* vRb = Morpeh::GetEntityRigidbody(vehView);
            void* vTr = Morpeh::GetEntityTransform(vehView);
            if (!vTr && vRb) vTr = Il2Cpp::SafeGetTransform(vRb);

            if (vTr && Il2Cpp::IsTransformAlive(vTr)) {
                if (!s_carFlyActive) {
                    Vec3 p0{};
                    if (ReadTransformPosition(vTr, p0)) {
                        s_carFlyPos = { p0.x, p0.y + 0.35f, p0.z };
                        s_carFlyYaw = std::atan2(fwd.x, fwd.z);
                        s_carFlyRb  = vRb;
                        s_carFlyActive = true;
                    }
                }
                if (s_carFlyActive) {
                    s_carFlyRb = vRb;
                    float throttle = gi.carHandbrake ? 0.0f : (gi.carGas - gi.carBrake);
                    float steer = gi.carSteer;

                    s_carFlyYaw += steer * 2.15f * dt;
                    if (std::fabs(throttle) > 0.05f && std::fabs(steer) < 0.05f) {
                        float camYaw = std::atan2(fwd.x, fwd.z);
                        float diff = std::atan2(std::sin(camYaw - s_carFlyYaw), std::cos(camYaw - s_carFlyYaw));
                        s_carFlyYaw += diff * std::clamp(dt * 4.5f, 0.0f, 1.0f);
                    }

                    float spd = std::clamp(g_cfgState.carFlySpeed, 5.0f, 120.0f);
                    float pitchY = std::clamp(fwd.y, -0.85f, 0.85f);
                    float horizScale = std::sqrt(std::max(0.05f, 1.0f - pitchY * pitchY));
                    Vec3 flyDir{
                        std::sin(s_carFlyYaw) * horizScale,
                        pitchY,
                        std::cos(s_carFlyYaw) * horizScale
                    };

                    if (std::fabs(throttle) > 0.05f) {
                        s_carFlyPos.x += flyDir.x * throttle * spd * dt;
                        s_carFlyPos.y += flyDir.y * throttle * spd * dt;
                        s_carFlyPos.z += flyDir.z * throttle * spd * dt;
                    }

                    NotifySelfTeleport();
                    Quat carRot = YawToQuat(s_carFlyYaw);
                    if (Il2Cpp::IsRigidbodyAlive(vRb)) {
                        Il2Cpp::SafeSetRbGravity(vRb, false);
                        Il2Cpp::SafeSetRbKinematic(vRb, true);
                        Il2Cpp::SafeSetRbVelocity(vRb, {0, 0, 0});
                        Il2Cpp::SafeSetRbPosition(vRb, s_carFlyPos);
                        Il2Cpp::SafeSetRbRotation(vRb, carRot);
                    }
                    Il2Cpp::SafeSetTransformPos(vTr, s_carFlyPos);
                    Il2Cpp::SafeSetTransformRot(vTr, carRot);
                    Il2Cpp::SafePhysicsSyncTransforms();
                    Morpeh::SetComponent(Morpeh::g_stashes.positionStash, veh, &s_carFlyPos, sizeof(Vec3));
                    Morpeh::SetComponent(Morpeh::g_stashes.positionStash, ped, &s_carFlyPos, sizeof(Vec3));
                }
            }
        }
        // --- TRUE INVISIBLE-FLOOR SKYWALK (Player stands/walks/aims/shoots 100% normally in the sky!) ---
        else if (pedFly && tr && Il2Cpp::IsTransformAlive(tr)) {
            Vec3 footPos{};
            if (ReadTransformPosition(tr, footPos) && IsFiniteVec3(footPos)) {
                // Lazily spawn or reactivate the invisible solid sky floor collider
                if (!s_skyFloorGo || !Il2Cpp::IsGameObjectAlive(s_skyFloorGo) || !Il2Cpp::IsTransformAlive(s_skyFloorTr)) {
                    int32_t groundLayer = 0;
                    uint32_t gMask = static_cast<uint32_t>(Hooks::g_groundLayerMask.load(std::memory_order_acquire));
                    if (gMask != 0 && gMask != 0xFFFFFFFFU) {
                        for (int bit = 0; bit < 32; ++bit) {
                            if (gMask & (1u << bit)) { groundLayer = bit; break; }
                        }
                    }
                    s_skyFloorGo = Il2Cpp::SafeCreateInvisibleSkyFloor(groundLayer, s_skyFloorTr);
                }

                if (!s_skyFloorActive) {
                    s_skyFloorY = footPos.y - 0.02f;
                    if (s_skyFloorGo && Il2Cpp::IsGameObjectAlive(s_skyFloorGo)) {
                        Il2Cpp::SafeSetGameObjectActive(s_skyFloorGo, true);
                    }
                    s_skyFloorActive = true;
                }

                float climbRate = std::clamp(g_cfgState.flySpeed * 0.45f, 2.0f, 18.0f);
                if (gi.jump) {
                    // Holding Jump smoothly lifts the invisible sky floor upward!
                    s_skyFloorY += climbRate * dt;
                } else if (gi.crouch) {
                    // Holding Crouch smoothly lowers the invisible sky floor downward!
                    s_skyFloorY -= climbRate * dt;
                } else if (std::fabs(gi.pedV) > 0.15f && std::fabs(fwd.y) > 0.24f) {
                    // Walking while looking clearly up or down slopes the sky floor smoothly like invisible stairs!
                    s_skyFloorY += fwd.y * gi.pedV * climbRate * 0.80f * dt;
                }

                // If the player naturally jumped higher than the sky floor, raise the floor to catch them at the peak!
                if (footPos.y > s_skyFloorY + 0.35f && footPos.y < s_skyFloorY + 3.2f) {
                    s_skyFloorY = footPos.y - 0.05f;
                }
                // Safety net: never allow the player to fall below their own sky floor!
                if (footPos.y < s_skyFloorY - 0.55f) {
                    s_skyFloorY = footPos.y - 0.03f;
                }

                // Slide the 16x0.4x16 invisible solid floor directly beneath the player's feet (top surface = s_skyFloorY)
                if (s_skyFloorTr && Il2Cpp::IsTransformAlive(s_skyFloorTr)) {
                    Vec3 floorCenter{ footPos.x, s_skyFloorY - 0.20f, footPos.z };
                    Il2Cpp::SafeSetTransformPos(s_skyFloorTr, floorCenter);
                    Il2Cpp::SafePhysicsSyncTransforms();
                }

                // If player is rising via Jump hold and still below new floor top, lift feet smoothly onto floor top
                if (gi.jump && footPos.y < s_skyFloorY - 0.02f) {
                    Vec3 lifted{ footPos.x, s_skyFloorY + 0.02f, footPos.z };
                    if (actor && Il2Cpp::IsUnityNativeAlive(actor) && Il2Cpp::g_unity.mPhysActor_SetPos) {
                        void* a[1] = { &lifted };
                        Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPhysActor_SetPos, actor, a);
                    } else {
                        Il2Cpp::SafeSetTransformPos(tr, lifted);
                    }
                }
            }
        }
    }

    // Anti-TP Guard (Ped & Vehicle)
    double now = CACurrentMediaTime();
    if (now > g_selfTeleportGraceUntil && !pedFly && !carFly) {
        if (g_cfgState.bAntiTp && tr) {
            Vec3 p{};
            if (ReadTransformPosition(tr, p)) {
                if (g_antiTpPedAnchored && Vec3Dist(p, g_antiTpPedAnchor) > 25.0f && Vec3Dist(p, g_antiTpPedAnchor) < 800.0f) {
                    RepositionTarget(rb, tr, g_antiTpPedAnchor, {0, 0, 0}, true);
                    g_cfgState.lastStatusBanner = "Anti-TP blocked ped snap";
                } else {
                    g_antiTpPedAnchor = p;
                    g_antiTpPedAnchored = true;
                }
            }
        } else {
            g_antiTpPedAnchored = false;
        }

        if (g_cfgState.bAntiTpCar) {
            int64_t veh = Morpeh::GetLocalVehicleLogicEntity(ped);
            if (veh > 0) {
                int64_t vv = Morpeh::GetViewEntity(veh);
                void* vTr = Morpeh::GetEntityTransform(vv);
                void* vRb = Morpeh::GetEntityRigidbody(vv);
                Vec3 vp{};
                if (vTr && ReadTransformPosition(vTr, vp)) {
                    if (g_antiTpCarAnchored && Vec3Dist(vp, g_antiTpCarAnchor) > 35.0f && Vec3Dist(vp, g_antiTpCarAnchor) < 800.0f) {
                        RepositionTarget(vRb, vTr, g_antiTpCarAnchor, {0, 0, 0}, true);
                        g_cfgState.lastStatusBanner = "Anti-TP blocked vehicle snap";
                    } else {
                        g_antiTpCarAnchor = vp;
                        g_antiTpCarAnchored = true;
                    }
                }
            } else {
                g_antiTpCarAnchored = false;
            }
        } else {
            g_antiTpCarAnchored = false;
        }
    }

    // Anti-AFK pulse every 25s
    if (g_cfgState.bAntiAfk && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.inputsDataStash)) {
        static double s_lastAfk = 0.0;
        if (now - s_lastAfk >= 25.0) {
            s_lastAfk = now;
            Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.inputsDataStash, [&](int32_t id, int) {
                if (void* inp = Morpeh::Get(Morpeh::g_stashes.inputsDataStash, id, 88)) {
                    float pulse = 0.08f;
                    SafeMem::Write(reinterpret_cast<uint8_t*>(inp) + 0x4, &pulse, 4);
                }
            });
        }
    }
}

inline void RestoreAllScaledBones() {
    if (g_scaledTransforms.empty()) return;
    Vec3 one{1.0f, 1.0f, 1.0f};
    for (auto& kv : g_scaledTransforms) {
        if (Il2Cpp::IsUnityNativeAlive(kv.first))
            Il2Cpp::SafeSetTransformScale(kv.first, one);
    }
    g_scaledTransforms.clear();
}

inline void TickPlayerScale() {
    if (!g_cfgState.bScaleEnable) {
        RestoreAllScaledBones();
        return;
    }
    std::vector<ESPPlayer> snapshot;
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        snapshot = g_renderPlayers;
    }
    float sc = std::clamp(g_cfgState.playerScale, 1.0f, 8.0f);
    Vec3 sVec{sc, sc, sc};
    Vec3 one{1.0f, 1.0f, 1.0f};

    for (const auto& pl : snapshot) {
        bool allow = !pl.dead && !pl.inVehicle;
        auto applyBone = [&](void* boneTr, bool active) {
            if (!Il2Cpp::IsUnityNativeAlive(boneTr)) return;
            if (allow && active) {
                g_scaledTransforms[boneTr] = one;
                Il2Cpp::SafeSetTransformScale(boneTr, sVec);
            } else if (g_scaledTransforms.count(boneTr)) {
                Il2Cpp::SafeSetTransformScale(boneTr, one);
                g_scaledTransforms.erase(boneTr);
            }
        };
        applyBone(pl.headTransform,  g_cfgState.bBigHead);
        applyBone(pl.chestTransform, g_cfgState.bBigChest);
        applyBone(pl.rootTransform,  g_cfgState.bBigBody);
    }
}

// ============================================================================
// World Time, Camera FOV/Aspect, Crosshair Hider, Radio, FreeCam & FX
// ============================================================================
inline void ApplyDayNightTick() {
    if (!g_cfgState.bDayNightEnable) return;
    Morpeh::RefreshStoragesIfNeeded();
    float hr = std::fmod(std::max(0.0f, g_cfgState.dayNightTime), 24.0f);
    int64_t ticks = static_cast<int64_t>(hr * 36000000000.0);

    Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.timeWorldStash, [&](int32_t entId, int) {
        if (void* wPtr = Morpeh::Get(Morpeh::g_stashes.timeWorldStash, entId, 8)) {
            SafeMem::Write(wPtr, &ticks, 8);
        }
    });

    static float s_lastPushedHr = -1.0f;
    static bool  s_lastFreeze   = false;
    if (std::fabs(hr - s_lastPushedHr) > 0.05f || g_cfgState.bDayNightFreeze != s_lastFreeze) {
        int64_t reqEnt = Morpeh::CreateEntity();
        if (reqEnt != 0) {
            if (Morpeh::g_stashes.timeSetReqStash)
                Morpeh::SetComponent(Morpeh::g_stashes.timeSetReqStash, reqEnt, &ticks, 8);
            if (Morpeh::g_stashes.timeFrozenReqStash) {
                uint8_t frz = g_cfgState.bDayNightFreeze ? 1 : 0;
                Morpeh::SetComponent(Morpeh::g_stashes.timeFrozenReqStash, reqEnt, &frz, 1);
            }
        }
        s_lastPushedHr = hr;
        s_lastFreeze = g_cfgState.bDayNightFreeze;
    }

    if (void* env = Il2Cpp::GetSingletonStaticInstance(Offsets::g_cfg.nsEnviro.c_str(), Offsets::g_cfg.clsEnviroMgr.c_str())) {
        void* envCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(env) : nullptr;
        size_t offSolar = Il2Cpp::FieldOffset(envCls, "solarTime", Offsets::g_cfg.offEnviroSolarTime);
        size_t offLunar = Il2Cpp::FieldOffset(envCls, "lunarTime", Offsets::g_cfg.offEnviroLunarTime);
        size_t offNight = Il2Cpp::FieldOffset(envCls, "isNight", Offsets::g_cfg.offEnviroIsNight);
        float solar01 = hr / 24.0f;
        float lunar01 = std::fmod(solar01 + 0.5f, 1.0f);
        uint8_t isNight = (hr < 6.0f || hr >= 20.0f) ? 1 : 0;
        SafeMem::Write(reinterpret_cast<uint8_t*>(env) + offSolar, &solar01, 4);
        SafeMem::Write(reinterpret_cast<uint8_t*>(env) + offLunar, &lunar01, 4);
        SafeMem::Write(reinterpret_cast<uint8_t*>(env) + offNight, &isNight, 1);
    }
}

inline void TickCameraFovAndAspect() {
    if (!g_cfgState.bFovEnable && !g_cfgState.bResolutionEnable) return;
    void* cam = Il2Cpp::SafeCameraGetMain();
    if (!cam) return;

    static bool s_capturedAspect = false;
    if (!s_capturedAspect && Il2Cpp::g_icall.Camera_get_aspect) {
        float asp = 1.0f;
        if (TCFlyGuard::TryRun([&]() { asp = Il2Cpp::g_icall.Camera_get_aspect(cam); }) &&
            std::isfinite(asp) && asp > 0.3f && asp < 4.0f) {
            g_cfgState.nativeAspect = asp;
            s_capturedAspect = true;
        }
    }

    if (g_cfgState.bFovEnable) {
        float fov = g_cfgState.cameraFov;
        if (Il2Cpp::g_icall.Camera_set_fov) {
            TCFlyGuard::TryRun([&]() { Il2Cpp::g_icall.Camera_set_fov(cam, fov); });
        } else if (Il2Cpp::g_unity.Camera_set_fov) {
            void* a[1] = { &fov };
            Il2Cpp::SafeInvoke(Il2Cpp::g_unity.Camera_set_fov, cam, a);
        }
        // FOVPersist: patch CameraSettings.HorizontalFieldOfView (+0x60)
        if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.camSettingsStash)) {
            Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.camSettingsStash, [&](int32_t id, int) {
                if (void* csPtr = Morpeh::Get(Morpeh::g_stashes.camSettingsStash, id, 8)) {
                    if (void* csObj = SafeMem::PtrAt(csPtr, 0)) {
                        SafeMem::Write(reinterpret_cast<uint8_t*>(csObj) + Offsets::g_cfg.offCamSettingsHFov, &fov, 4);
                    }
                }
            });
        }
    }

    if (g_cfgState.bResolutionEnable) {
        float asp = std::clamp(g_cfgState.nativeAspect * g_cfgState.aspectScale, 0.4f, 4.0f);
        if (Il2Cpp::g_icall.Camera_set_aspect) {
            TCFlyGuard::TryRun([&]() { Il2Cpp::g_icall.Camera_set_aspect(cam, asp); });
        } else if (Il2Cpp::g_unity.Camera_set_aspect) {
            void* a[1] = { &asp };
            Il2Cpp::SafeInvoke(Il2Cpp::g_unity.Camera_set_aspect, cam, a);
        }
    }
}

inline void RestoreGameCrosshair() {
    if (!g_crosshairWasHidden) return;
    for (void* go : g_hiddenCrosshairObjs) {
        Il2Cpp::SafeSetGameObjectActive(go, true);
    }
    g_hiddenCrosshairObjs.clear();
    g_crosshairWasHidden = false;
}

inline void TickRemoveGameCrosshair() {
    if (!g_cfgState.bRemoveGameCrosshair) {
        RestoreGameCrosshair();
        return;
    }
    if (g_crosshairWasHidden || !Il2Cpp::g_unity.Resources_FindObjectsOfTypeAll) return;
    static double s_lastScan = 0.0;
    double now = CACurrentMediaTime();
    if (now - s_lastScan < 2.0) return;
    s_lastScan = now;

    void* tObj = Il2Cpp::GetTypeObject(Offsets::g_cfg.nsGuiHudWeaponCtrls.c_str(), Offsets::g_cfg.clsGuiHudWeaponCtrls.c_str());
    if (!tObj) return;
    void* args[1] = { tObj };
    void* arr = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.Resources_FindObjectsOfTypeAll, nullptr, args);
    if (!SafeMem::IsPlausiblePtr(arr)) return;
    int32_t len = 0;
    if (!SafeMem::ReadVal(reinterpret_cast<uint8_t*>(arr) + Offsets::Morpeh::kIl2CppArrayLengthOff, len) || len <= 0) return;
    uint8_t* base = reinterpret_cast<uint8_t*>(arr) + Offsets::Morpeh::kIl2CppArrayDataOff;
    for (int i = 0; i < std::min(len, 16); ++i) {
        if (void* comp = SafeMem::PtrAt(base, static_cast<size_t>(i) * sizeof(void*))) {
            if (void* go = Il2Cpp::SafeGetGameObject(comp)) {
                Il2Cpp::SafeSetGameObjectActive(go, false);
                g_hiddenCrosshairObjs.push_back(go);
                g_crosshairWasHidden = true;
            }
        }
    }
}

inline void ComputeShotPartsColor(double timeSec, float& outR, float& outG, float& outB) {
    if (g_cfgState.shotPalSel >= 0 && g_cfgState.shotPalSel < Catalogs::kShotPartStyleCount) {
        const auto& st = Catalogs::kShotPartStyles[g_cfgState.shotPalSel];
        float phase = 0.5f + 0.5f * std::sin(static_cast<float>(timeSec) * st.speed);
        float h = st.hueMin + st.hueMax * phase;
        ImVec4 c = ThemeEngine::HsvToVec4(h, st.sat, st.val, 1.0f);
        outR = c.x * g_cfgState.cfTint[0];
        outG = c.y * g_cfgState.cfTint[1];
        outB = c.z * g_cfgState.cfTint[2];
    } else {
        outR = (g_cfgState.shotColR / 255.0f) * g_cfgState.cfTint[0];
        outG = (g_cfgState.shotColG / 255.0f) * g_cfgState.cfTint[1];
        outB = (g_cfgState.shotColB / 255.0f) * g_cfgState.cfTint[2];
    }
}

inline void TickRadioOverride(int64_t localPlayer) {
    if (!g_cfgState.bRadioEnable || localPlayer <= 0) return;
    static int s_lastChan = -1;
    if (g_cfgState.radioChannel == s_lastChan) return;
    int16_t ch = static_cast<int16_t>(g_cfgState.radioChannel);
    if (Morpeh::g_stashes.voiceChanStash) {
        Morpeh::SetComponent(Morpeh::g_stashes.voiceChanStash, localPlayer, &ch, 2);
    }
    if (Morpeh::g_stashes.voiceChanReqStash) {
        struct VoiceChanReq { int16_t changeType; int16_t channelValue; uint8_t manual; } req{ 1, ch, 1 };
        Morpeh::SetComponent(Morpeh::g_stashes.voiceChanReqStash, localPlayer, &req, sizeof(req));
    }
    s_lastChan = g_cfgState.radioChannel;
    g_cfgState.lastStatusBanner = "Radio channel set to #" + std::to_string(ch);
}

// ============================================================================
// FreeCam (Free-Fly Camera) & Far-Player Spectate + Ghost Proxy Voice (FMOD/AoI)
// + Map Marker / Waypoint Teleport (NavigationGraph + HudMap + Smart Ground-Settle)
// ============================================================================
inline Vec3     g_freeCamPos{};
inline bool     g_freeCamHasPos = false;
inline int64_t  g_activeSpectateEnt = -1;
inline int32_t  g_spectateTargetPlayerId = -1;
inline uint32_t g_spectateTargetDigitalId = 0;
inline Vec3     g_spectateTargetPos{};
inline bool     g_hasSpectateTargetPos = false;
inline double   g_spectateStartedAt = 0.0;
inline Vec3     g_fcGhostOriginPos{};
inline bool     g_fcHasGhostOrigin = false;
inline void*    g_fcSavedRemovePenAction = nullptr;
inline bool     g_fcSavedRemovePenValid = false;
inline Vec3     g_lastAutoMapTpPos{};
inline bool     g_hasLastAutoMapTpPos = false;

inline void ForceUnmuteAllVoiceChannels() {
    if (void* vp = Morpeh::EnsureVoicePlaybackResolved()) {
        if (Il2Cpp::g_unity.mVoiceSvc_SetAllMuted) {
            bool m = false;
            void* a[1] = { &m };
            Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mVoiceSvc_SetAllMuted, vp, a);
        }
        if (Il2Cpp::g_unity.mVoiceSvc_SetAllVolume) {
            float vol = 1.0f;
            void* a[1] = { &vol };
            Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mVoiceSvc_SetAllVolume, vp, a);
        }
    }
}

inline void ApplyGhostUndergroundAnchorToLocal(int64_t localPed, const Vec3& anchorPos) {
    if (localPed <= 0 || !IsFiniteVec3(anchorPos)) return;
    int64_t view = Morpeh::GetViewEntity(localPed);
    int32_t vid  = (view > 0) ? Morpeh::UnpackEntityId(view) : -1;
    void* pv     = (vid >= 0) ? Morpeh::Get(Morpeh::g_stashes.pedViewStash, vid, 384) : nullptr;
    const auto& cfg = Offsets::g_cfg;

    if (pv) {
        void* actor = SafeMem::PtrAt(pv, cfg.offPedViewActor);
        void* rb    = SafeMem::PtrAt(pv, cfg.offPedViewRigidbody);
        void* tr    = SafeMem::PtrAt(pv, cfg.offPedViewRoot);

        if (actor && Il2Cpp::IsUnityNativeAlive(actor)) {
            if (!g_fcSavedRemovePenValid) {
                g_fcSavedRemovePenAction = SafeMem::PtrAt(actor, cfg.offCharActorRemovePen);
                g_fcSavedRemovePenValid = true;
            }
            void* nullPtr = nullptr;
            uint8_t one = 1;
            Vec3 zero{};
            SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offCharActorRemovePen, &nullPtr, sizeof(void*));
            SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offCharActorInVel,     &zero, sizeof(Vec3));
            SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offCharActorPreVel,    &zero, sizeof(Vec3));
            SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offCharActorPostVel,   &zero, sizeof(Vec3));
            SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offCharActorExtVel,    &zero, sizeof(Vec3));
            SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offCharActorGrounded,  &one, 1);
            SafeMem::Write(reinterpret_cast<uint8_t*>(actor) + cfg.offCharActorStable,    &one, 1);
        }
        if (rb && Il2Cpp::IsRigidbodyAlive(rb)) {
            Il2Cpp::SafeSetRbKinematic(rb, true);
            Il2Cpp::SafeSetRbGravity(rb, false);
            Il2Cpp::SafeSetRbDetectCollisions(rb, false);
            Il2Cpp::SafeSetRbVelocity(rb, {0, 0, 0});
            Il2Cpp::SafeSetRbPosition(rb, anchorPos);
        }
        if (tr && Il2Cpp::IsTransformAlive(tr)) {
            Il2Cpp::SafeSetTransformPos(tr, anchorPos);
        }
        Il2Cpp::SafePhysicsSyncTransforms();
    }
    Morpeh::SetComponent(Morpeh::g_stashes.positionStash, localPed, &anchorPos, sizeof(Vec3));

    // Disable PedPhysicsParams gravityInAir while ghost proxy rests underground
    if (void* pp = Hooks::g_pedPhysicsParamsInst.load(std::memory_order_acquire)) {
        int32_t zeroGrav = 0;
        SafeMem::Write(reinterpret_cast<uint8_t*>(pp) + cfg.offPedPhysicsGravInAir, &zeroGrav, sizeof(zeroGrav));
    }
}

struct MapTpLandingState {
    bool    active      = false;
    bool    inVehicle   = false;
    int64_t vehLogicEnt = -1;
    Vec3    targetRaw{};
    Vec3    hoverPos{};
    double  startedAt   = 0.0;
    void*   savedActor  = nullptr;
    void*   savedRb     = nullptr;
};
inline MapTpLandingState g_mapTpLanding{};

inline bool TryFindGroundY(float x, float z, float& outY);
inline void ApplyDirectEntityTeleport(int64_t ped, int64_t veh, const Vec3& dst, bool holdKinematic);

inline void ReleaseGhostUndergroundAnchor(int64_t localPed) {
    Hooks::g_fcGhostAnchorActive.store(false, std::memory_order_release);
    Hooks::g_fcCamLockActive.store(false, std::memory_order_release);
    Hooks::g_fcHasSurfaceAudio.store(false, std::memory_order_release);
    // Extended 4.5s grace period protects ped from fall/off-map/void kills while terrain streams back in!
    Hooks::g_fcReturnGraceUntil.store(CACurrentMediaTime() + 4.5, std::memory_order_release);

    const auto& cfg = Offsets::g_cfg;
    if (Hooks::g_hasSavedPedGravity) {
        if (void* pp = Hooks::g_pedPhysicsParamsInst.load(std::memory_order_acquire)) {
            SafeMem::Write(reinterpret_cast<uint8_t*>(pp) + cfg.offPedPhysicsGravInAir,
                           &Hooks::g_savedPedGravityInAir, sizeof(int32_t));
        }
    }

    int64_t ped = (localPed > 0) ? localPed : Morpeh::GetLocalPedEntity();
    void* savedActor = nullptr;
    void* savedRb = nullptr;
    if (ped > 0) {
        int64_t view = Morpeh::GetViewEntity(ped);
        int32_t vid  = (view > 0) ? Morpeh::UnpackEntityId(view) : -1;
        if (void* pv = (vid >= 0) ? Morpeh::Get(Morpeh::g_stashes.pedViewStash, vid, 384) : nullptr) {
            savedActor = SafeMem::PtrAt(pv, cfg.offPedViewActor);
            savedRb    = SafeMem::PtrAt(pv, cfg.offPedViewRigidbody);
            if (savedActor && Il2Cpp::IsUnityNativeAlive(savedActor)) {
                if (g_fcSavedRemovePenValid) {
                    SafeMem::Write(reinterpret_cast<uint8_t*>(savedActor) + cfg.offCharActorRemovePen,
                                   &g_fcSavedRemovePenAction, sizeof(void*));
                }
                Vec3 zero{};
                uint8_t one = 1;
                SafeMem::Write(reinterpret_cast<uint8_t*>(savedActor) + cfg.offCharActorInVel,    &zero, sizeof(Vec3));
                SafeMem::Write(reinterpret_cast<uint8_t*>(savedActor) + cfg.offCharActorPreVel,   &zero, sizeof(Vec3));
                SafeMem::Write(reinterpret_cast<uint8_t*>(savedActor) + cfg.offCharActorPostVel,  &zero, sizeof(Vec3));
                SafeMem::Write(reinterpret_cast<uint8_t*>(savedActor) + cfg.offCharActorExtVel,   &zero, sizeof(Vec3));
                SafeMem::Write(reinterpret_cast<uint8_t*>(savedActor) + cfg.offCharActorGrounded, &one, 1);
                SafeMem::Write(reinterpret_cast<uint8_t*>(savedActor) + cfg.offCharActorStable,   &one, 1);
            }
            // Restore collision detection now, but DO NOT re-enable gravity while still underground!
            if (savedRb && Il2Cpp::IsRigidbodyAlive(savedRb)) {
                Il2Cpp::SafeSetRbDetectCollisions(savedRb, true);
                Il2Cpp::SafeSetRbVelocity(savedRb, {0, 0, 0});
            }
        }
    }
    g_fcSavedRemovePenAction = nullptr;
    g_fcSavedRemovePenValid = false;

    if (g_fcHasGhostOrigin && IsFiniteVec3(g_fcGhostOriginPos)) {
        Vec3 retPos = g_fcGhostOriginPos;
        g_fcHasGhostOrigin = false;
        // Use ground-verified safe return: if terrain at origin is unloaded after far spectate/freecam,
        // hold ped safely in kinematic hover above origin until terrain collider streams back in!
        float groundY = 0.0f;
        if (TryFindGroundY(retPos.x, retPos.z, groundY)) {
            float safeY = std::max(retPos.y, groundY + 0.35f) + 0.85f;
            TeleportLocalTo({ retPos.x, safeY, retPos.z }, {0, 0, 0}, true);
        } else {
            // Far origin terrain unloaded -> engage hover-settle until terrain raycast confirms solid ground!
            Vec3 hover{ retPos.x, retPos.y + 3.5f, retPos.z };
            int64_t veh = (ped > 0) ? Morpeh::GetLocalVehicleLogicEntity(ped) : -1;
            g_mapTpLanding = MapTpLandingState{};
            g_mapTpLanding.active      = true;
            g_mapTpLanding.inVehicle   = (veh > 0);
            g_mapTpLanding.vehLogicEnt = veh;
            g_mapTpLanding.targetRaw   = { retPos.x, retPos.y + 1.0f, retPos.z };
            g_mapTpLanding.hoverPos    = hover;
            g_mapTpLanding.startedAt   = CACurrentMediaTime();
            g_mapTpLanding.savedActor  = savedActor;
            g_mapTpLanding.savedRb     = savedRb;
            ApplyDirectEntityTeleport(ped, veh, hover, true);
        }
    } else if (savedRb && Il2Cpp::IsRigidbodyAlive(savedRb)) {
        Il2Cpp::SafeSetRbGravity(savedRb, true);
        Il2Cpp::SafeSetRbKinematic(savedRb, false);
    }
}

inline void StopSpectate() {
    Hooks::EnsureFreeCamSystemPatches();
    Hooks::EnsureFreeCamVoiceAndImmunityHooks();
    if (g_activeSpectateEnt > 0) {
        if (Morpeh::g_stashes.freeCamDeactivateStash) Morpeh::SetEmpty(Morpeh::g_stashes.freeCamDeactivateStash, g_activeSpectateEnt);
        if (Morpeh::g_stashes.freeCamTargetStash)     Morpeh::Remove(Morpeh::g_stashes.freeCamTargetStash, g_activeSpectateEnt);
        if (Morpeh::g_stashes.freeCamActiveStash)     Morpeh::Remove(Morpeh::g_stashes.freeCamActiveStash, g_activeSpectateEnt);
    }
    int64_t lp = Morpeh::GetLocalPedEntity();
    if (lp > 0) {
        if (Morpeh::g_stashes.freeCamDeactivateStash) Morpeh::SetEmpty(Morpeh::g_stashes.freeCamDeactivateStash, lp);
        if (Morpeh::g_stashes.freeCamActiveStash)     Morpeh::Remove(Morpeh::g_stashes.freeCamActiveStash, lp);
    }
    ReleaseGhostUndergroundAnchor(lp);

    g_activeSpectateEnt = -1;
    g_spectateTargetPlayerId = -1;
    g_spectateTargetDigitalId = 0;
    g_hasSpectateTargetPos = false;
    g_spectateStartedAt = 0.0;
    g_cfgState.bSpectateActive = false;
    g_cfgState.spectateTargetPedEnt = -1;
    g_cfgState.spectateTargetName.clear();
    g_cfgState.lastStatusBanner = "Spectate stopped — returned safely";
}

inline void StartSpectatePlayer(int64_t targetPedEnt, const std::string& name) {
    if (targetPedEnt <= 0) {
        g_cfgState.lastStatusBanner = "Spectate skipped: invalid target ped entity";
        return;
    }
    Morpeh::RefreshStoragesIfNeeded();
    Hooks::EnsureFreeCamSystemPatches();
    Hooks::EnsureFreeCamVoiceAndImmunityHooks();
    ForceUnmuteAllVoiceChannels();

    if (g_cfgState.bFreeCam) {
        g_cfgState.bFreeCam = false;
        g_freeCamHasPos = false;
    }

    // Save local origin ONCE before activating Ghost Proxy underground anchor
    if (!g_fcHasGhostOrigin) {
        Vec3 origPos{};
        if (GetLocalPosition(origPos) && IsFiniteVec3(origPos)) {
            g_fcGhostOriginPos = origPos;
            g_fcHasGhostOrigin = true;
        }
    }

    // Resolve owning playerId, digitalId, and initial position from server roster / streamed list
    int32_t foundPid = -1;
    uint32_t foundDid = 0;
    Vec3 initPos{};
    bool hasInitPos = false;
    int32_t tgtPedId = Morpeh::UnpackEntityId(targetPedEnt);
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        for (const auto& sp : g_allServerPlayers) {
            if (sp.pedEntityVal == targetPedEnt || (tgtPedId >= 0 && sp.pedId == tgtPedId)) {
                foundPid = sp.playerId;
                foundDid = sp.digitalId;
                if (IsFiniteVec3(sp.worldPos) && (std::fabs(sp.worldPos.x) > 0.25f || std::fabs(sp.worldPos.z) > 0.25f)) {
                    initPos = sp.worldPos;
                    hasInitPos = true;
                }
                break;
            }
        }
        if (!hasInitPos) {
            for (const auto& pl : g_renderPlayers) {
                if (pl.pedEntityVal == targetPedEnt || (tgtPedId >= 0 && pl.entityId == tgtPedId)) {
                    foundPid = pl.playerId;
                    foundDid = pl.entityGen;
                    initPos = pl.worldPos;
                    hasInitPos = true;
                    break;
                }
            }
        }
    }

    // Also try PlayerDistanceUtils immediately for far targets whose position isn't in local stash yet
    if (!hasInitPos) {
        int32_t dim = 0; float dist = 0.0f;
        hasInitPos = Morpeh::QueryRemotePlayerPosition(foundPid, foundDid, targetPedEnt, initPos, dim, dist);
    }

    if (g_activeSpectateEnt > 0 && g_activeSpectateEnt != targetPedEnt) {
        if (Morpeh::g_stashes.freeCamDeactivateStash) Morpeh::SetEmpty(Morpeh::g_stashes.freeCamDeactivateStash, g_activeSpectateEnt);
        if (Morpeh::g_stashes.freeCamActiveStash)     Morpeh::Remove(Morpeh::g_stashes.freeCamActiveStash, g_activeSpectateEnt);
        if (Morpeh::g_stashes.freeCamTargetStash)     Morpeh::Remove(Morpeh::g_stashes.freeCamTargetStash, g_activeSpectateEnt);
    }
    if (Morpeh::g_stashes.freeCamDeactivateStash) Morpeh::Remove(Morpeh::g_stashes.freeCamDeactivateStash, targetPedEnt);
    if (Morpeh::g_stashes.freeCamTargetStash)     Morpeh::SetEmpty(Morpeh::g_stashes.freeCamTargetStash, targetPedEnt);
    if (Morpeh::g_stashes.freeCamActivateStash)   Morpeh::SetEmpty(Morpeh::g_stashes.freeCamActivateStash, targetPedEnt);

    g_activeSpectateEnt       = targetPedEnt;
    g_spectateTargetPlayerId  = foundPid;
    g_spectateTargetDigitalId = foundDid;
    g_spectateTargetPos       = initPos;
    g_hasSpectateTargetPos    = hasInitPos;
    g_spectateStartedAt       = CACurrentMediaTime();
    g_cfgState.bSpectateActive = true;
    g_cfgState.spectateTargetPedEnt = targetPedEnt;
    g_cfgState.spectateTargetName = name.empty() ? "Player" : name;
    g_cfgState.lastStatusBanner = "Ghost Spectate + Voice: " + g_cfgState.spectateTargetName;
}

inline void TeleportLocalToFreeCam() {
    if (!g_freeCamHasPos || !IsFiniteVec3(g_freeCamPos)) return;
    Vec3 dst = g_freeCamPos;
    g_cfgState.bFreeCam = false;
    g_freeCamHasPos = false;
    TeleportLocalTo(dst);
    g_cfgState.lastStatusBanner = "Teleported to FreeCam position";
}

inline bool TryFindGroundY(float x, float z, float& outY) {
    int32_t mask = Hooks::g_groundLayerMask.load(std::memory_order_acquire);
    UnityRaycastHit hit{};
    if (Il2Cpp::SafePhysicsRaycast({ x, 450.0f, z }, { 0.0f, -1.0f, 0.0f }, 750.0f, mask, hit) &&
        std::isfinite(hit.point.y) && hit.point.y > -80.0f && hit.point.y < 440.0f) {
        outY = hit.point.y;
        return true;
    }
    if (Il2Cpp::SafePhysicsRaycast({ x, 220.0f, z }, { 0.0f, -1.0f, 0.0f }, 420.0f, -1, hit) &&
        std::isfinite(hit.point.y) && hit.point.y > -80.0f && hit.point.y < 218.0f) {
        outY = hit.point.y;
        return true;
    }
    return false;
}

inline float FindSafeGroundY(float x, float z, float fallbackY) {
    float hitY = 0.0f;
    if (TryFindGroundY(x, z, hitY)) return hitY + 1.45f;
    return (fallbackY > 3.0f && fallbackY < 400.0f) ? (fallbackY + 2.0f) : 45.0f;
}

inline void ApplyDirectEntityTeleport(int64_t ped, int64_t veh, const Vec3& dst, bool holdKinematic) {
    NotifySelfTeleport();
    if (veh > 0) {
        int64_t vv = Morpeh::GetViewEntity(veh);
        void* vRb = Morpeh::GetEntityRigidbody(vv);
        void* vTr = Morpeh::GetEntityTransform(vv);
        if (!vTr && vRb) vTr = Il2Cpp::SafeGetTransform(vRb);

        if (Il2Cpp::IsRigidbodyAlive(vRb)) {
            Il2Cpp::SafeSetRbKinematic(vRb, true);
            Il2Cpp::SafeSetRbVelocity(vRb, {0, 0, 0});
            Il2Cpp::SafeSetRbPosition(vRb, dst);
            g_mapTpLanding.savedRb = vRb;
        }
        if (Il2Cpp::IsTransformAlive(vTr)) {
            Il2Cpp::SafeSetTransformPos(vTr, dst);
        }
        Il2Cpp::SafePhysicsSyncTransforms();
        if (!holdKinematic && Il2Cpp::IsRigidbodyAlive(vRb)) {
            Il2Cpp::SafeSetRbVelocity(vRb, {0, 0, 0});
            Il2Cpp::SafeSetRbGravity(vRb, true);
            Il2Cpp::SafeSetRbKinematic(vRb, false);
        } else if (holdKinematic && Il2Cpp::IsRigidbodyAlive(vRb)) {
            Il2Cpp::SafeSetRbGravity(vRb, false);
            Il2Cpp::SafeSetRbKinematic(vRb, true);
        }
        // Sync BOTH vehicle and seated ped Morpeh Position stashes together
        Morpeh::SetComponent(Morpeh::g_stashes.positionStash, veh, &dst, sizeof(Vec3));
        if (ped > 0) Morpeh::SetComponent(Morpeh::g_stashes.positionStash, ped, &dst, sizeof(Vec3));
        return;
    }

    if (ped <= 0) return;
    int64_t view = Morpeh::GetViewEntity(ped);
    int32_t vid = (view > 0) ? Morpeh::UnpackEntityId(view) : -1;
    void* pv = (vid >= 0) ? Morpeh::Get(Morpeh::g_stashes.pedViewStash, vid, 384) : nullptr;
    if (pv) {
        void* actor = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewActor);
        void* rb    = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewRigidbody);
        void* tr    = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewRoot);

        if (holdKinematic) {
            if (actor && Il2Cpp::IsUnityNativeAlive(actor)) g_mapTpLanding.savedActor = actor;
            if (rb && Il2Cpp::IsRigidbodyAlive(rb))        g_mapTpLanding.savedRb = rb;
            WarpCharacterActorInstant(actor, rb, tr, dst, {0, 0, 0}, true, false);
        } else {
            WarpCharacterActorInstant(actor, rb, tr, dst, {0, 0, 0}, false, true);
        }
    }
    Morpeh::SetComponent(Morpeh::g_stashes.positionStash, ped, &dst, sizeof(Vec3));
    if (view > 0) Morpeh::SetComponent(Morpeh::g_stashes.positionStash, view, &dst, sizeof(Vec3));
    if (Morpeh::g_stashes.setPosRotStash) Morpeh::SetEmpty(Morpeh::g_stashes.setPosRotStash, ped);
}

inline void FinishMapTpLanding(int64_t localPed, float finalY) {
    int64_t veh = g_mapTpLanding.inVehicle ? g_mapTpLanding.vehLogicEnt : -1;
    if (localPed > 0 && veh <= 0) {
        int64_t curVeh = Morpeh::GetLocalVehicleLogicEntity(localPed);
        if (curVeh > 0) veh = curVeh;
    }
    Vec3 landPos{ g_mapTpLanding.targetRaw.x, finalY, g_mapTpLanding.targetRaw.z };
    if (g_mapTpLanding.savedActor && Il2Cpp::IsUnityNativeAlive(g_mapTpLanding.savedActor)) {
        Il2Cpp::SafeSetBehaviourEnabled(g_mapTpLanding.savedActor, true);
    }
    ApplyDirectEntityTeleport(localPed, veh, landPos, false);
    if (g_mapTpLanding.savedRb && Il2Cpp::IsRigidbodyAlive(g_mapTpLanding.savedRb)) {
        Il2Cpp::SafeSetRbGravity(g_mapTpLanding.savedRb, true);
        Il2Cpp::SafeSetRbKinematic(g_mapTpLanding.savedRb, false);
        Il2Cpp::SafeSetRbVelocity(g_mapTpLanding.savedRb, {0, 0, 0});
    }
    g_mapTpLanding = MapTpLandingState{};
}

inline void TickPendingMapTeleport(int64_t localPed) {
    if (!g_mapTpLanding.active) return;
    double now = CACurrentMediaTime();
    double elapsed = now - g_mapTpLanding.startedAt;

    float groundY = 0.0f;
    bool hitGround = TryFindGroundY(g_mapTpLanding.targetRaw.x, g_mapTpLanding.targetRaw.z, groundY);
    if (hitGround && elapsed >= 0.12) {
        float clearance = g_mapTpLanding.inVehicle ? 1.85f : 1.40f;
        FinishMapTpLanding(localPed, groundY + clearance);
        return;
    }
    if (elapsed >= 1.45) {
        float fallbackY = hitGround
            ? (groundY + (g_mapTpLanding.inVehicle ? 1.85f : 1.40f))
            : ((g_mapTpLanding.targetRaw.y > 2.0f && g_mapTpLanding.targetRaw.y < 350.0f)
                ? (g_mapTpLanding.targetRaw.y + 2.0f)
                : 35.0f);
        FinishMapTpLanding(localPed, fallbackY);
        return;
    }
    // Hold steadily above target while far terrain streams in so player/car never fall under map
    int64_t veh = g_mapTpLanding.inVehicle ? g_mapTpLanding.vehLogicEnt : -1;
    ApplyDirectEntityTeleport(localPed, veh, g_mapTpLanding.hoverPos, true);
}

inline bool TeleportToMapMarker(const Vec3& rawPos, const std::string& label = "") {
    if (!IsFiniteVec3(rawPos)) return false;
    Morpeh::RefreshStoragesIfNeeded();

    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped <= 0) {
        g_cfgState.lastStatusBanner = "Map TP skipped: join world first";
        return false;
    }
    int64_t veh = Morpeh::GetLocalVehicleLogicEntity(ped);
    bool inVeh = (veh > 0);
    float clearance = inVeh ? 1.85f : 1.40f;

    float groundY = 0.0f;
    if (TryFindGroundY(rawPos.x, rawPos.z, groundY)) {
        // Terrain already loaded at destination -> teleport directly & cleanly
        g_mapTpLanding = MapTpLandingState{};
        Vec3 dst{ rawPos.x, groundY + clearance, rawPos.z };
        ApplyDirectEntityTeleport(ped, veh, dst, false);
        char buf[96];
        snprintf(buf, sizeof(buf), "Map TP (%s): %.0f, %.0f, %.0f",
                 inVeh ? "Car" : "Foot", dst.x, dst.y, dst.z);
        g_cfgState.lastStatusBanner = buf;
        return true;
    }

    // Far destination whose ground mesh isn't streamed yet -> hover-settle until terrain raycast hits!
    float hoverY = (rawPos.y > 6.0f && rawPos.y < 320.0f) ? (rawPos.y + 10.0f) : 65.0f;
    Vec3 hover{ rawPos.x, hoverY, rawPos.z };
    g_mapTpLanding = MapTpLandingState{};
    g_mapTpLanding.active      = true;
    g_mapTpLanding.inVehicle   = inVeh;
    g_mapTpLanding.vehLogicEnt = veh;
    g_mapTpLanding.targetRaw   = rawPos;
    g_mapTpLanding.hoverPos    = hover;
    g_mapTpLanding.startedAt   = CACurrentMediaTime();

    ApplyDirectEntityTeleport(ped, veh, hover, true);

    char buf[112];
    snprintf(buf, sizeof(buf), "Map TP (%s%s): %.0f, %.0f (landing...)",
             inVeh ? "Car" : "Foot",
             label.empty() ? "" : (" " + label).c_str(),
             rawPos.x, rawPos.z);
    g_cfgState.lastStatusBanner = buf;
    return true;
}

inline void ScanMapMarkers(const Vec3& refPos, bool hasRefPos, bool allowAutoTp = true) {
    std::vector<MapMarkerEntry> next;
    std::vector<int32_t> userPinEntityIds;
    std::vector<Vec3> jobCheckpointPositions;

    // Collect active JobCheckpoint positions so a mission/job auto-GPS route never masquerades as a user map pin
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.jobCpStash) && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.positionStash)) {
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.jobCpStash, [&](int32_t jId, int) {
            if (void* pPtr = Morpeh::Get(Morpeh::g_stashes.positionStash, jId, sizeof(Vec3))) {
                Vec3 jp{};
                if (SafeMem::ReadVal(pPtr, jp) && IsFiniteVec3(jp) &&
                    (std::fabs(jp.x) > 0.25f || std::fabs(jp.z) > 0.25f)) {
                    jobCheckpointPositions.push_back(jp);
                }
            }
        });
    }
    auto isNearJobCheckpoint = [&](const Vec3& p) -> bool {
        for (const auto& jp : jobCheckpointPositions) {
            if (std::hypot(jp.x - p.x, jp.z - p.z) < 8.5f) return true;
        }
        return false;
    };

    auto pushUnique = [&](MapMarkerEntry&& e) {
        if (!IsFiniteVec3(e.worldPos)) return;
        if (std::fabs(e.worldPos.x) < 0.25f && std::fabs(e.worldPos.z) < 0.25f) return;
        if (std::fabs(e.worldPos.x) > 25000.0f || std::fabs(e.worldPos.z) > 25000.0f) return;
        for (auto& ex : next) {
            if (std::hypot(ex.worldPos.x - e.worldPos.x, ex.worldPos.z - e.worldPos.z) < 3.5f) {
                if (e.priority > ex.priority) {
                    ex.priority      = e.priority;
                    ex.isUserPin     = e.isUserPin;
                    ex.isExactMapPin = e.isExactMapPin;
                    if (!e.label.empty()) ex.label = e.label;
                } else {
                    if (e.isUserPin && !ex.isUserPin) ex.isUserPin = true;
                    if (e.isExactMapPin && !ex.isExactMapPin) ex.isExactMapPin = true;
                    if (ex.label.empty() && !e.label.empty()) ex.label = e.label;
                }
                return;
            }
        }
        next.push_back(std::move(e));
    };

    // Ensure HudMapNavigationService & hudMapCheckpointStash are resolved lazily
    Morpeh::EnsureHudMapNavServiceResolved();

    // =========================================================================
    // TIER 1 (Priority 100): EXACT USER-PLACED MAP PIN -> HudMapCheckpoint
    // Verified dump1.4.1.cpp L542739 & L537021 (HudMapCreateCheckpointSystem):
    // When the player places a pin on the big map, HudMapCheckpoint.Value holds
    // the persistent Morpeh Entity whose Position / NavMarkerBlip is the pin!
    // =========================================================================
    bool foundExactHudMapPin = false;
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.hudMapCheckpointStash)) {
        const auto& cfg = Offsets::g_cfg;
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.hudMapCheckpointStash, [&](int32_t slotId, int) {
            void* slotPtr = Morpeh::Get(Morpeh::g_stashes.hudMapCheckpointStash, slotId, sizeof(int64_t));
            if (!slotPtr) return;
            int64_t cpEnt = 0;
            if (!SafeMem::ReadVal(slotPtr, cpEnt) || cpEnt == 0) return;
            int32_t cpId = Morpeh::UnpackEntityId(cpEnt);
            if (cpId < 0) return;
            userPinEntityIds.push_back(cpId);

            Vec3 pos{};
            bool hasPos = false;

            // 1a) Direct Position stash on the HudMapCheckpoint.Value entity (exact HudMapCacheNavMarkerSystem path)
            if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.positionStash)) {
                if (void* pPtr = Morpeh::Get(Morpeh::g_stashes.positionStash, cpId, sizeof(Vec3))) {
                    if (SafeMem::ReadVal(pPtr, pos) && IsFiniteVec3(pos) &&
                        (std::fabs(pos.x) > 0.25f || std::fabs(pos.z) > 0.25f)) {
                        hasPos = true;
                    }
                }
            }
            // 1b) Fallback: NavMarkerBlip on cpId -> MapElement._position (+0x28)
            if (!hasPos && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.navMarkerBlipStash)) {
                if (void* bSlot = Morpeh::Get(Morpeh::g_stashes.navMarkerBlipStash, cpId, 8)) {
                    void* blipObj = SafeMem::PtrAt(bSlot, 0);
                    if (SafeMem::IsPlausiblePtr(blipObj) &&
                        SafeMem::ReadVal(reinterpret_cast<uint8_t*>(blipObj) + cfg.offMapElementPosition, pos) &&
                        IsFiniteVec3(pos) && (std::fabs(pos.x) > 0.25f || std::fabs(pos.z) > 0.25f)) {
                        hasPos = true;
                    }
                }
            }
            // 1c) Fallback: UnityTransform on cpId
            if (!hasPos && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.unityTransformStash)) {
                if (void* trSlot = Morpeh::Get(Morpeh::g_stashes.unityTransformStash, cpId, 8)) {
                    void* tr = SafeMem::PtrAt(trSlot, 0);
                    if (Il2Cpp::IsTransformAlive(tr) && Il2Cpp::SafeGetTransformPos(tr, pos) &&
                        (std::fabs(pos.x) > 0.25f || std::fabs(pos.z) > 0.25f)) {
                        hasPos = true;
                    }
                }
            }

            if (hasPos) {
                foundExactHudMapPin = true;
                MapMarkerEntry e{};
                e.entityId      = cpId;
                e.worldPos      = pos;
                e.distance      = hasRefPos ? Vec3Dist(refPos, pos) : 0.0f;
                e.priority      = 100;
                e.isUserPin     = true;
                e.isExactMapPin = true;
                e.label         = "Map Pin (User)";
                pushUnique(std::move(e));
            }
        });
    }

    // TIER 1B (Priority 98): HudMapCreateCheckpoint (if caught on the exact placement frame)
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.hudMapCreateCpStash)) {
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.hudMapCreateCpStash, [&](int32_t id, int) {
            if (void* ptr = Morpeh::Get(Morpeh::g_stashes.hudMapCreateCpStash, id, sizeof(Vec3))) {
                Vec3 pos{};
                if (SafeMem::ReadVal(ptr, pos) && IsFiniteVec3(pos) &&
                    (std::fabs(pos.x) > 0.25f || std::fabs(pos.z) > 0.25f)) {
                    foundExactHudMapPin = true;
                    MapMarkerEntry e{};
                    e.entityId      = id;
                    e.worldPos      = pos;
                    e.distance      = hasRefPos ? Vec3Dist(refPos, pos) : 0.0f;
                    e.priority      = 98;
                    e.isUserPin     = true;
                    e.isExactMapPin = true;
                    e.label         = "Map Pin (Placed)";
                    pushUnique(std::move(e));
                }
            }
        });
    }

    // =========================================================================
    // TIER 2 (Priority 80): Live GPS Route Goal via HudMapNavigationService->NavigationGraph._lastGoal,
    // HudMapNavigationSystem._points.back(), and Hook_NavigationGraph_FindPath.
    // Demoted to non-user-pin if it matches a JobCheckpoint so job GPS never hijacks User Pin TP!
    // =========================================================================
    auto pushGpsCandidate = [&](const Vec3& goal, int32_t eid, const char* lbl) {
        if (!IsFiniteVec3(goal) || (std::fabs(goal.x) < 0.25f && std::fabs(goal.z) < 0.25f)) return;
        if (std::fabs(goal.x) > 25000.0f || std::fabs(goal.z) > 25000.0f) return;
        bool jobGoal = isNearJobCheckpoint(goal);
        MapMarkerEntry e{};
        e.entityId      = eid;
        e.worldPos      = goal;
        e.distance      = hasRefPos ? Vec3Dist(refPos, goal) : 0.0f;
        e.priority      = jobGoal ? 30 : 80;
        e.isUserPin     = !jobGoal;
        e.isExactMapPin = false;
        e.label         = jobGoal ? "Job GPS Route" : lbl;
        pushUnique(std::move(e));
    };

    void* nav = Hooks::g_navGraphInst.load(std::memory_order_acquire);
    if (!SafeMem::IsPlausiblePtr(nav)) {
        if (void* diNav = Morpeh::EnsureNavGraphResolved()) {
            Hooks::g_navGraphInst.store(diNav, std::memory_order_release);
            nav = diNav;
        }
    }
    if (SafeMem::IsPlausiblePtr(nav)) {
        void* navCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(nav) : nullptr;
        size_t offGoal = Il2Cpp::FieldOffset(navCls, "_lastGoal", Offsets::g_cfg.offNavGraphLastGoal);
        Vec3 lastGoal{};
        if (SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(nav) + offGoal, lastGoal)) {
            pushGpsCandidate(lastGoal, 0, "GPS Route Target");
        }
    }
    Vec3 routeEnd{};
    if (Morpeh::TryReadHudMapRouteEnd(routeEnd)) {
        pushGpsCandidate(routeEnd, -2, "GPS Route End");
    }
    if (Hooks::g_hasCapturedMapGoal.load(std::memory_order_acquire)) {
        Vec3 goal{ Hooks::g_mapGoalX.load(std::memory_order_relaxed),
                   Hooks::g_mapGoalY.load(std::memory_order_relaxed),
                   Hooks::g_mapGoalZ.load(std::memory_order_relaxed) };
        pushGpsCandidate(goal, -1, "GPS Captured Target");
    }

    // =========================================================================
    // TIER 3 (Priority 40, NOT a user pin): HudMapCachedNavMarker (cached nav/tutorial waypoint)
    // =========================================================================
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.hudMapCachedMarkerStash)) {
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.hudMapCachedMarkerStash, [&](int32_t id, int) {
            if (void* ptr = Morpeh::Get(Morpeh::g_stashes.hudMapCachedMarkerStash, id, 20)) {
                Vec3 pos{};
                if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(ptr) + 0x08, pos) && IsFiniteVec3(pos)) {
                    MapMarkerEntry e{};
                    e.entityId      = id;
                    e.worldPos      = pos;
                    e.distance      = hasRefPos ? Vec3Dist(refPos, pos) : 0.0f;
                    e.priority      = 40;
                    e.isUserPin     = false;
                    e.isExactMapPin = false;
                    e.label         = "Cached Waypoint";
                    pushUnique(std::move(e));
                }
            }
        });
    }

    // =========================================================================
    // TIER 4 (Priority 15, NOT a user pin): NavMarkerBlip & NavMarkerWithBlip
    // Mission/Job/Shop/World waypoints are listed for optional manual TP below,
    // NEVER promoted to isUserPin unless their entityId matches HudMapCheckpoint.Value!
    // =========================================================================
    auto isUserPinId = [&](int32_t eid) -> bool {
        for (int32_t uid : userPinEntityIds) {
            if (uid == eid) return true;
        }
        return false;
    };

    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.navMarkerBlipStash)) {
        const auto& cfg = Offsets::g_cfg;
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.navMarkerBlipStash, [&](int32_t id, int) {
            void* slotPtr = Morpeh::Get(Morpeh::g_stashes.navMarkerBlipStash, id, 8);
            if (!slotPtr) return;
            void* blipObj = SafeMem::PtrAt(slotPtr, 0);
            Vec3 pos{};
            bool hasPos = false;
            std::string label;
            std::string sprite;

            if (SafeMem::IsPlausiblePtr(blipObj)) {
                uint8_t hidden = 0;
                if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(blipObj) + cfg.offMapElementHidden, hidden) && hidden != 0)
                    return;
                if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(blipObj) + cfg.offMapElementPosition, pos) &&
                    IsFiniteVec3(pos) && (std::fabs(pos.x) > 0.25f || std::fabs(pos.z) > 0.25f)) {
                    hasPos = true;
                }
                label  = Il2Cpp::ReadManagedString(SafeMem::PtrAt(blipObj, cfg.offBlipLabel));
                if (label.empty()) label = Il2Cpp::ReadManagedString(SafeMem::PtrAt(blipObj, cfg.offMapElementLegendTitle));
                sprite = Il2Cpp::ReadManagedString(SafeMem::PtrAt(blipObj, cfg.offBlipSpriteName));
            }
            if (!hasPos && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.positionStash)) {
                if (void* posPtr = Morpeh::Get(Morpeh::g_stashes.positionStash, id, sizeof(Vec3))) {
                    if (SafeMem::ReadVal(posPtr, pos) && IsFiniteVec3(pos)) hasPos = true;
                }
            }
            if (!hasPos) return;

            bool exactPin = isUserPinId(id);
            MapMarkerEntry e{};
            e.entityId      = id;
            e.worldPos      = pos;
            e.distance      = hasRefPos ? Vec3Dist(refPos, pos) : 0.0f;
            e.priority      = exactPin ? 100 : 15;
            e.isUserPin     = exactPin;
            e.isExactMapPin = exactPin;
            e.label         = exactPin ? "Map Pin (User)"
                              : (!label.empty() ? label : (!sprite.empty() ? sprite : ("Waypoint #" + std::to_string(id))));
            pushUnique(std::move(e));
        });
    }

    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.navMarkerWithBlipStash) &&
        SafeMem::IsPlausiblePtr(Morpeh::g_stashes.positionStash)) {
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.navMarkerWithBlipStash, [&](int32_t id, int) {
            if (void* posPtr = Morpeh::Get(Morpeh::g_stashes.positionStash, id, sizeof(Vec3))) {
                Vec3 pos{};
                if (!SafeMem::ReadVal(posPtr, pos) || !IsFiniteVec3(pos)) return;
                std::string sprite;
                if (void* wb = Morpeh::Get(Morpeh::g_stashes.navMarkerWithBlipStash, id, 16)) {
                    sprite = Il2Cpp::ReadManagedString(SafeMem::PtrAt(wb, 0));
                }
                bool exactPin = isUserPinId(id);
                MapMarkerEntry e{};
                e.entityId      = id;
                e.worldPos      = pos;
                e.distance      = hasRefPos ? Vec3Dist(refPos, pos) : 0.0f;
                e.priority      = exactPin ? 100 : 10;
                e.isUserPin     = exactPin;
                e.isExactMapPin = exactPin;
                e.label         = exactPin ? "Map Pin (User)"
                                  : (!sprite.empty() ? sprite : ("Waypoint #" + std::to_string(id)));
                pushUnique(std::move(e));
            }
        });
    }

    // Sort: highest priority first (100 User Pin -> 80 GPS Route -> 40 Cached -> 15 Waypoints), then nearest
    std::sort(next.begin(), next.end(), [](const MapMarkerEntry& a, const MapMarkerEntry& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.distance < b.distance;
    });

    // Auto-TP ONLY when a genuine User Map Pin (HudMapCheckpoint or active non-job GPS pin) is placed/moved
    if (allowAutoTp && g_cfgState.bAutoMapMarkerTp) {
        const MapMarkerEntry* chosenPin = nullptr;
        for (const auto& m : next) {
            if (m.isExactMapPin) { chosenPin = &m; break; }
        }
        if (!chosenPin && !foundExactHudMapPin) {
            for (const auto& m : next) {
                if (m.isUserPin && m.priority >= 80) { chosenPin = &m; break; }
            }
        }
        if (chosenPin) {
            Vec3 pinPos = chosenPin->worldPos;
            if (!g_hasLastAutoMapTpPos ||
                std::hypot(pinPos.x - g_lastAutoMapTpPos.x, pinPos.z - g_lastAutoMapTpPos.z) > 4.0f) {
                g_lastAutoMapTpPos = pinPos;
                g_hasLastAutoMapTpPos = true;
                TeleportToMapMarker(pinPos, chosenPin->label);
            }
        } else {
            // Pin was cleared/removed -> arm for the next placement immediately
            g_hasLastAutoMapTpPos = false;
        }
    } else if (!g_cfgState.bAutoMapMarkerTp) {
        g_hasLastAutoMapTpPos = false;
    }

    std::lock_guard<std::mutex> lk(g_espMutex);
    g_mapMarkers = std::move(next);
}

inline void ForceRefreshMapMarkers() {
    Morpeh::RefreshStoragesIfNeeded();
    Vec3 refPos{};
    bool hasRef = GetLocalPosition(refPos);
    ScanMapMarkers(refPos, hasRef, false);
}

inline bool TeleportToNearestUserMapPin() {
    // Always run a fresh on-demand scan right before TP so newly placed pins work on the very first tap!
    ForceRefreshMapMarkers();
    MapMarkerEntry chosen{};
    bool found = false;
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        // 1) Prefer exact user-placed HudMapCheckpoint pin (priority 100)
        for (const auto& m : g_mapMarkers) {
            if (m.isExactMapPin) { chosen = m; found = true; break; }
        }
        // 2) Fallback ONLY to active non-job GPS route goal (priority >= 80), NEVER random mission/job waypoints!
        if (!found) {
            for (const auto& m : g_mapMarkers) {
                if (m.isUserPin && m.priority >= 80) { chosen = m; found = true; break; }
            }
        }
    }
    if (!found) {
        g_cfgState.lastStatusBanner = "No user map pin placed — open map and drop a pin first";
        return false;
    }
    return TeleportToMapMarker(chosen.worldPos, chosen.label);
}

// Server-wide player scanner (NEAR + FAR players across the entire server)
// Reads ControlsEntity (playerId -> logic ped Entity) + NicknameText + DigitalId
inline void ScanAllServerPlayers(int32_t localPlayerId, const Vec3& refPos, bool hasRefPos) {
    if (!SafeMem::IsPlausiblePtr(Morpeh::g_stashes.controlsEntityStash) &&
        !SafeMem::IsPlausiblePtr(Morpeh::g_stashes.nickTextStash)) return;

    // Build quick lookup from currently streamed nearby ESP players
    std::unordered_map<int32_t, ESPPlayer> streamedByPed;
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        streamedByPed.reserve(g_renderPlayers.size() + 8);
        for (const auto& pl : g_renderPlayers) {
            if (pl.entityId >= 0) streamedByPed[pl.entityId] = pl;
        }
    }

    std::unordered_map<int32_t, ServerPlayerEntry> byPlayerId;
    byPlayerId.reserve(128);

    auto readPlayerName = [](int32_t playerId) -> std::string {
        if (playerId < 0 || !SafeMem::IsPlausiblePtr(Morpeh::g_stashes.nickTextStash)) return "";
        if (void* ntPtr = Morpeh::Get(Morpeh::g_stashes.nickTextStash, playerId, 16)) {
            std::string fn = Il2Cpp::ReadManagedString(SafeMem::PtrAt(ntPtr, 0));
            std::string ln = Il2Cpp::ReadManagedString(SafeMem::PtrAt(ntPtr, 8));
            if (!fn.empty() && !ln.empty()) return fn + " " + ln;
            return !fn.empty() ? fn : ln;
        }
        return "";
    };

    auto readDigitalId = [](int32_t playerId) -> uint32_t {
        if (playerId < 0 || !SafeMem::IsPlausiblePtr(Morpeh::g_stashes.digitalIdStash)) return 0;
        uint32_t dig = 0;
        if (void* didPtr = Morpeh::Get(Morpeh::g_stashes.digitalIdStash, playerId, 4))
            SafeMem::ReadVal(didPtr, dig);
        return dig;
    };

    // Pass 1: ControlsEntity has ALL active players on the server (Near + Far) with their logic Ped Entity!
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.controlsEntityStash)) {
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.controlsEntityStash, [&](int32_t playerId, int) {
            if (playerId < 0 || (localPlayerId >= 0 && playerId == localPlayerId)) return;
            int64_t pedEnt = 0;
            if (void* ctrlPtr = Morpeh::Get(Morpeh::g_stashes.controlsEntityStash, playerId, 8)) {
                SafeMem::ReadVal(ctrlPtr, pedEnt);
            }
            if (pedEnt <= 0) return;
            int32_t pedId = Morpeh::UnpackEntityId(pedEnt);
            if (pedId < 0) return;

            ServerPlayerEntry sp{};
            sp.playerId     = playerId;
            sp.pedId        = pedId;
            sp.pedEntityVal = pedEnt;
            sp.digitalId    = readDigitalId(playerId);
            sp.name         = readPlayerName(playerId);

            auto sIt = streamedByPed.find(pedId);
            if (sIt != streamedByPed.end()) {
                sp.isStreamed = true;
                sp.worldPos   = sIt->second.worldPos;
                sp.health     = sIt->second.health;
                sp.distance   = hasRefPos ? Vec3Dist(refPos, sp.worldPos) : 0.0f;
                if (sp.name.empty() && !sIt->second.name.empty()) sp.name = sIt->second.name;
                if (sp.digitalId == 0 && sIt->second.entityGen > 0) sp.digitalId = sIt->second.entityGen;
            } else {
                sp.isStreamed = false;
                sp.health     = 100;
                if (void* hPtr = Morpeh::Get(Morpeh::g_stashes.healthStash, pedId, 4)) {
                    int32_t hp = 100;
                    if (SafeMem::ReadVal(hPtr, hp) && hp >= 0 && hp <= 1000) sp.health = hp;
                }
                if (void* pPtr = Morpeh::Get(Morpeh::g_stashes.positionStash, pedId, sizeof(Vec3))) {
                    Vec3 p{};
                    if (SafeMem::ReadVal(pPtr, p) && IsFiniteVec3(p) &&
                        (std::fabs(p.x) > 0.5f || std::fabs(p.z) > 0.5f)) {
                        sp.worldPos = p;
                        sp.distance = hasRefPos ? Vec3Dist(refPos, p) : -1.0f;
                    }
                }
            }
            if (sp.name.empty()) {
                sp.name = (sp.digitalId > 0) ? ("Player #" + std::to_string(sp.digitalId)) : ("Player #" + std::to_string(playerId));
            }
            byPlayerId[playerId] = std::move(sp);
        });
    }

    // Pass 2: Include any remaining players visible in NicknameText (fallback if ControlsEntity missed one)
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.nickTextStash)) {
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.nickTextStash, [&](int32_t playerId, int) {
            if (playerId < 0 || (localPlayerId >= 0 && playerId == localPlayerId)) return;
            if (byPlayerId.find(playerId) != byPlayerId.end()) return;
            std::string nm = readPlayerName(playerId);
            if (nm.empty()) return;

            ServerPlayerEntry sp{};
            sp.playerId  = playerId;
            sp.digitalId = readDigitalId(playerId);
            sp.name      = std::move(nm);
            sp.isStreamed = false;
            sp.distance   = -1.0f;
            byPlayerId[playerId] = std::move(sp);
        });
    }

    std::vector<ServerPlayerEntry> out;
    out.reserve(byPlayerId.size());
    for (auto& kv : byPlayerId) out.push_back(std::move(kv.second));

    // Sort: streamed nearby players first (closest first), then far players alphabetically
    std::sort(out.begin(), out.end(), [](const ServerPlayerEntry& a, const ServerPlayerEntry& b) {
        if (a.isStreamed != b.isStreamed) return a.isStreamed > b.isStreamed;
        if (a.isStreamed && b.isStreamed) return a.distance < b.distance;
        return a.name < b.name;
    });

    std::lock_guard<std::mutex> lk(g_espMutex);
    g_allServerPlayers = std::move(out);
}

inline void TickFreeCam(int64_t localPed, float dt) {
    // 1) Player Spectate mode (Near & Far via Ghost Proxy Underground Anchor + FMOD 2D/Surface Voice + Surface Orbit Cam)
    if (g_cfgState.bSpectateActive && g_cfgState.spectateTargetPedEnt > 0) {
        Hooks::EnsureFreeCamSystemPatches();
        Hooks::EnsureFreeCamVoiceAndImmunityHooks();

        // Keep voice bus unmuted while spectating
        static double s_lastVoiceEnforce = 0.0;
        double now = CACurrentMediaTime();
        if (now - s_lastVoiceEnforce >= 1.0) {
            s_lastVoiceEnforce = now;
            ForceUnmuteAllVoiceChannels();
        }

        // Resolve live 3D position: (A) Streamed 60fps ped head/root -> (B) PlayerDistanceUtils/PositionStash (FAR players)
        Vec3 liveTargetPos = g_spectateTargetPos;
        bool gotTarget = g_hasSpectateTargetPos;
        bool streamedNow = false;
        int32_t tgtPedId = Morpeh::UnpackEntityId(g_cfgState.spectateTargetPedEnt);
        {
            std::lock_guard<std::mutex> lk(g_espMutex);
            for (const auto& pl : g_renderPlayers) {
                if (pl.pedEntityVal == g_cfgState.spectateTargetPedEnt ||
                    (tgtPedId >= 0 && pl.entityId == tgtPedId)) {
                    Vec3 p = pl.worldPos;
                    if (pl.rootTransform) ReadTransformPosition(pl.rootTransform, p);
                    if (IsFiniteVec3(p) && (std::fabs(p.x) > 0.25f || std::fabs(p.z) > 0.25f)) {
                        liveTargetPos = p;
                        gotTarget = true;
                        streamedNow = true;
                    }
                    break;
                }
            }
        }
        if (!streamedNow) {
            static double s_lastDistQuery = 0.0;
            if (!gotTarget || (now - s_lastDistQuery) >= 0.08) {
                s_lastDistQuery = now;
                Vec3 farPos{};
                int32_t farDim = 0;
                float farDist = 0.0f;
                if (Morpeh::QueryRemotePlayerPosition(g_spectateTargetPlayerId, g_spectateTargetDigitalId,
                                                      g_cfgState.spectateTargetPedEnt, farPos, farDim, farDist)) {
                    liveTargetPos = farPos;
                    gotTarget = true;
                }
            }
        }
        if (gotTarget) {
            g_spectateTargetPos = liveTargetPos;
            g_hasSpectateTargetPos = true;
        }

        if (gotTarget) {
            // Ghost Proxy Underground Anchor: places local ped ~19.5m underneath target (<= 25m voice bubble)
            if (g_cfgState.bFreeCamGhostVoice && localPed > 0) {
                if (!g_fcHasGhostOrigin) {
                    Vec3 origPos{};
                    if (GetLocalPosition(origPos) && IsFiniteVec3(origPos)) {
                        g_fcGhostOriginPos = origPos;
                        g_fcHasGhostOrigin = true;
                    }
                }
                float depthY = std::clamp(g_cfgState.freeCamGhostDepthY, -55.0f, -12.0f);
                Vec3 anchor{ liveTargetPos.x, liveTargetPos.y + depthY, liveTargetPos.z };
                Hooks::g_fcGhostAnchorActive.store(true, std::memory_order_release);
                ApplyGhostUndergroundAnchorToLocal(localPed, anchor);
            } else if (Hooks::g_fcGhostAnchorActive.load(std::memory_order_acquire)) {
                ReleaseGhostUndergroundAnchor(localPed);
            }

            // Surface Orbit Camera & FMOD Listener Position above ground
            if (g_cfgState.bFreeCamAutoOrbit) {
                g_cfgState.freeCamOrbitYaw += 35.0f * dt;
                if (g_cfgState.freeCamOrbitYaw > 360.0f) g_cfgState.freeCamOrbitYaw -= 360.0f;
            }
            float radYaw   = g_cfgState.freeCamOrbitYaw * 3.14159265f / 180.0f;
            float radPitch = std::clamp(g_cfgState.freeCamOrbitPitch, -30.0f, 80.0f) * 3.14159265f / 180.0f;
            float orbitDist   = g_cfgState.bFreeCamHeadCam ? 1.35f : std::clamp(g_cfgState.freeCamOrbitDist, 1.2f, 35.0f);
            float pivotHeight = g_cfgState.bFreeCamHeadCam ? 1.72f : 1.60f;
            float lookHeight  = g_cfgState.bFreeCamHeadCam ? 1.65f : 1.20f;

            Vec3 camPos{
                liveTargetPos.x - std::sin(radYaw) * std::cos(radPitch) * orbitDist,
                liveTargetPos.y + pivotHeight + std::sin(radPitch) * orbitDist,
                liveTargetPos.z - std::cos(radYaw) * std::cos(radPitch) * orbitDist
            };
            Vec3 lookAt{ liveTargetPos.x, liveTargetPos.y + lookHeight, liveTargetPos.z };
            Vec3 lookDir{ lookAt.x - camPos.x, lookAt.y - camPos.y, lookAt.z - camPos.z };
            Quat camRot = LookRotationQuat(lookDir);

            // Publish surface audio + camera lock for FMOD StudioListener & CameraPlacement/UniversalCamera systems
            Hooks::g_fcAudioX.store(camPos.x, std::memory_order_relaxed);
            Hooks::g_fcAudioY.store(camPos.y, std::memory_order_relaxed);
            Hooks::g_fcAudioZ.store(camPos.z, std::memory_order_relaxed);
            Hooks::g_fcHasSurfaceAudio.store(true, std::memory_order_release);

            Hooks::g_fcCamPosX.store(camPos.x, std::memory_order_relaxed);
            Hooks::g_fcCamPosY.store(camPos.y, std::memory_order_relaxed);
            Hooks::g_fcCamPosZ.store(camPos.z, std::memory_order_relaxed);
            Hooks::g_fcCamRotX.store(camRot.x, std::memory_order_relaxed);
            Hooks::g_fcCamRotY.store(camRot.y, std::memory_order_relaxed);
            Hooks::g_fcCamRotZ.store(camRot.z, std::memory_order_relaxed);
            Hooks::g_fcCamRotW.store(camRot.w, std::memory_order_relaxed);
            Hooks::g_fcCamLockActive.store(true, std::memory_order_release);

            if (void* cam = Il2Cpp::SafeCameraGetMain()) {
                if (void* camTr = Il2Cpp::SafeGetTransform(cam)) {
                    Il2Cpp::SafeSetTransformPos(camTr, camPos);
                    Il2Cpp::SafeSetTransformRot(camTr, camRot);
                }
            }
        }
        return;
    }

    // 2) FreeCam Free-Fly mode (with optional Ghost Voice Underground Anchor)
    bool want = g_cfgState.bFreeCam || g_cfgState.bTalkFreeCam;
    if (want) {
        Hooks::EnsureFreeCamSystemPatches();
        Hooks::EnsureFreeCamVoiceAndImmunityHooks();
        if (!g_freeCamWasActive) {
            // ALWAYS save local ped origin on FreeCam start so the player is protected from falling & returned safely!
            if (!g_fcHasGhostOrigin) {
                Vec3 origPos{};
                if (GetLocalPosition(origPos) && IsFiniteVec3(origPos)) {
                    g_fcGhostOriginPos = { origPos.x, origPos.y + 0.35f, origPos.z };
                    g_fcHasGhostOrigin = true;
                }
            }
            if (localPed > 0) {
                if (Morpeh::g_stashes.freeCamActivateStash) Morpeh::SetEmpty(Morpeh::g_stashes.freeCamActivateStash, localPed);
                if (Morpeh::g_stashes.freeCamActiveStash)   Morpeh::SetEmpty(Morpeh::g_stashes.freeCamActiveStash, localPed);
            }
            if (void* cam = Il2Cpp::SafeCameraGetMain()) {
                if (void* camTr = Il2Cpp::SafeGetTransform(cam)) {
                    g_freeCamHasPos = Il2Cpp::SafeGetTransformPos(camTr, g_freeCamPos);
                }
            }
            g_freeCamWasActive = true;
        }
        if (g_cfgState.bFreeCam) {
            if (void* cam = Il2Cpp::SafeCameraGetMain()) {
                if (void* camTr = Il2Cpp::SafeGetTransform(cam)) {
                    if (!g_freeCamHasPos) g_freeCamHasPos = Il2Cpp::SafeGetTransformPos(camTr, g_freeCamPos);
                    if (g_freeCamHasPos) {
                        Vec3 fwd{0, 0, 1}, right{1, 0, 0};
                        Il2Cpp::SafeGetTransformForward(camTr, fwd);
                        Il2Cpp::SafeGetTransformRight(camTr, right);

                        float inH = 0.0f, inV = 0.0f;
                        bool inJump = false, inCrouch = false;
                        if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.inputsDataStash)) {
                            Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.inputsDataStash, [&](int32_t id, int) {
                                if (void* inp = Morpeh::Get(Morpeh::g_stashes.inputsDataStash, id, 88)) {
                                    float h = 0.0f, v = 0.0f; uint8_t cr = 0, jp = 0;
                                    if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(inp) + 0x04, h) && std::isfinite(h) && std::fabs(h) > std::fabs(inH)) inH = h;
                                    if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(inp) + 0x08, v) && std::isfinite(v) && std::fabs(v) > std::fabs(inV)) inV = v;
                                    if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(inp) + 0x15, cr) && cr) inCrouch = true;
                                    if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(inp) + 0x1C, jp) && jp) inJump = true;
                                }
                            });
                        }
                        if (std::fabs(inH) < 0.12f) inH = 0.0f;
                        if (std::fabs(inV) < 0.12f) inV = 0.0f;
                        float jx = inH;
                        float jy = inV;
                        float jz = inJump ? 1.0f : (inCrouch ? -1.0f : 0.0f);
                        float spd = std::clamp(g_cfgState.freeCamSpeed, 3.0f, 120.0f);
                        g_freeCamPos.x += (fwd.x * jy + right.x * jx) * spd * dt;
                        g_freeCamPos.y += (fwd.y * jy + jz) * spd * dt;
                        g_freeCamPos.z += (fwd.z * jy + right.z * jx) * spd * dt;
                        Il2Cpp::SafeSetTransformPos(camTr, g_freeCamPos);

                        Hooks::g_fcAudioX.store(g_freeCamPos.x, std::memory_order_relaxed);
                        Hooks::g_fcAudioY.store(g_freeCamPos.y, std::memory_order_relaxed);
                        Hooks::g_fcAudioZ.store(g_freeCamPos.z, std::memory_order_relaxed);
                        Hooks::g_fcHasSurfaceAudio.store(true, std::memory_order_release);

                        if (g_cfgState.bFreeCamGhostVoice && localPed > 0) {
                            float depthY = std::clamp(g_cfgState.freeCamGhostDepthY, -55.0f, -12.0f);
                            Vec3 anchor{ g_freeCamPos.x, g_freeCamPos.y + depthY, g_freeCamPos.z };
                            Hooks::g_fcGhostAnchorActive.store(true, std::memory_order_release);
                            ApplyGhostUndergroundAnchorToLocal(localPed, anchor);
                        } else if (localPed > 0 && g_fcHasGhostOrigin) {
                            // Even without Ghost Voice, hold local ped safely anchored at saved origin so it NEVER falls!
                            Hooks::g_fcGhostAnchorActive.store(true, std::memory_order_release);
                            ApplyGhostUndergroundAnchorToLocal(localPed, g_fcGhostOriginPos);
                        }
                    }
                }
            }
        }
    } else if (g_freeCamWasActive) {
        if (localPed > 0) {
            if (Morpeh::g_stashes.freeCamDeactivateStash) Morpeh::SetEmpty(Morpeh::g_stashes.freeCamDeactivateStash, localPed);
            if (Morpeh::g_stashes.freeCamActiveStash)     Morpeh::Remove(Morpeh::g_stashes.freeCamActiveStash, localPed);
        }
        if (Hooks::g_fcGhostAnchorActive.load(std::memory_order_acquire) || g_fcHasGhostOrigin) {
            ReleaseGhostUndergroundAnchor(localPed);
        }
        Hooks::g_fcCamLockActive.store(false, std::memory_order_release);
        Hooks::g_fcHasSurfaceAudio.store(false, std::memory_order_release);
        g_freeCamWasActive = false;
        g_freeCamHasPos = false;
    }
}

inline void TickAutoDanse() {
    if (!g_cfgState.bAutoDanse || g_cfgState.autoDanseId <= 0) return;
    static double s_lastDance = 0.0;
    double now = CACurrentMediaTime();
    if (now - s_lastDance >= 4.0) {
        ApplyEmoteLocal(g_cfgState.autoDanseId);
        s_lastDance = now;
    }
}

// ============================================================================
// Camera Matrix, Weapon Lookup, Role Categorizer & Live World Snapshot
// ============================================================================
inline bool GetMainCameraVPMatrix(float outM[16]) {
    if (!Il2Cpp::g_unity.Camera_get_main ||
        !Il2Cpp::g_unity.Camera_get_worldToCameraMatrix ||
        !Il2Cpp::g_unity.Camera_get_projectionMatrix) return false;
    void* cam = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.Camera_get_main, nullptr, nullptr);
    if (!SafeMem::IsPlausiblePtr(cam)) return false;
    void* vRet = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.Camera_get_worldToCameraMatrix, cam, nullptr);
    void* pRet = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.Camera_get_projectionMatrix, cam, nullptr);
    void* vUnb = Il2Cpp::Unbox(vRet);
    void* pUnb = Il2Cpp::Unbox(pRet);
    if (!vUnb || !pUnb) return false;

    float v[16]{}, p[16]{};
    if (!SafeMem::Read(vUnb, v, sizeof(v)) || !SafeMem::Read(pUnb, p, sizeof(p))) return false;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += p[k * 4 + row] * v[col * 4 + k];
            if (!std::isfinite(sum)) return false;
            outM[col * 4 + row] = sum;
        }
    }
    return true;
}

inline const char* LookupWeaponName(int32_t invItemId) {
    if (invItemId <= 0) return "";
    const int n = static_cast<int>(sizeof(Catalogs::kWeapons) / sizeof(Catalogs::kWeapons[0]));
    for (int i = 0; i < n; ++i) {
        if (static_cast<int32_t>(Catalogs::kWeapons[i].id) == invItemId)
            return Catalogs::kWeapons[i].displayName;
    }
    return "";
}

inline int ResolvePlayerRoleCategory(int32_t playerId, const std::string& localClanId) {
    // Role map: 0=Mates, 1=Factions(Mayor/Other), 2=Civil, 3=Police, 4=Army, 5=Medic, 6=Gang, 7=War, 8=Combat, 9=Alliance, 10=Admin
    if (playerId < 0) return 2;

    if (void* facPtr = Morpeh::Get(Morpeh::g_stashes.factionResStash, playerId, 8)) {
        uint16_t facType = 0;
        if (SafeMem::ReadVal(facPtr, facType)) {
            if (facType == 1) return 3; // Police
            if (facType == 2) return 4; // Army
            if (facType == 3) return 5; // Medic
            if (facType == 4) return 1; // Mayor / Faction
        }
    }
    if (void* clanPtr = Morpeh::Get(Morpeh::g_stashes.playerClanStash, playerId, 16)) {
        std::string cid = Il2Cpp::ReadManagedString(SafeMem::PtrAt(clanPtr, 0));
        if (!cid.empty()) {
            if (!localClanId.empty() && cid == localClanId) return 0; // Mates
            return 6; // Gang
        }
    }
    return 2; // Civil
}

inline void ResolvePedViewFieldOffsets(size_t& offRoot, size_t& offHips, size_t& offChest, size_t& offHeadBone,
                                       size_t& offAnimancer, size_t& offActor,
                                       size_t& offRH, size_t& offLH, size_t& offRL, size_t& offLL) {
    static bool s_resolved = false;
    static size_t s_root = 0, s_hips = 0, s_chest = 0, s_head = 0, s_anim = 0, s_actor = 0;
    static size_t s_rh = 0, s_lh = 0, s_rl = 0, s_ll = 0;
    if (!s_resolved) {
        void* pvCls = Il2Cpp::FindClass(Offsets::g_cfg.nsPedViewComponents.c_str(), Offsets::g_cfg.compPedView.c_str());
        // PedView is an unboxed C# struct inside Stash<PedView>.data (PedView[]) -> MUST strip +0x10 boxed header!
        s_root  = Il2Cpp::StructFieldOffset(pvCls, "root", Offsets::g_cfg.offPedViewRoot);
        s_hips  = Il2Cpp::StructFieldOffset(pvCls, "hips", Offsets::g_cfg.offPedViewHips);
        s_chest = Il2Cpp::StructFieldOffset(pvCls, "chest", Offsets::g_cfg.offPedViewChest);
        s_head  = Il2Cpp::StructFieldOffset(pvCls, "headBone", Offsets::g_cfg.offPedViewHeadBone);
        s_anim  = Il2Cpp::StructFieldOffset(pvCls, "animancer", Offsets::g_cfg.offPedViewAnimancer);
        s_actor = Il2Cpp::StructFieldOffset(pvCls, "actor", Offsets::g_cfg.offPedViewActor);
        s_rh    = Il2Cpp::StructFieldOffset(pvCls, "RightHandIkBones", Offsets::g_cfg.offPedViewRightHandIk);
        s_lh    = Il2Cpp::StructFieldOffset(pvCls, "LeftHandIkBones", Offsets::g_cfg.offPedViewLeftHandIk);
        s_rl    = Il2Cpp::StructFieldOffset(pvCls, "RightLegIkBones", Offsets::g_cfg.offPedViewRightLegIk);
        s_ll    = Il2Cpp::StructFieldOffset(pvCls, "LeftLegIkBones", Offsets::g_cfg.offPedViewLeftLegIk);
        s_resolved = true;
    }
    offRoot = s_root; offHips = s_hips; offChest = s_chest; offHeadBone = s_head;
    offAnimancer = s_anim; offActor = s_actor;
    offRH = s_rh; offLH = s_lh; offRL = s_rl; offLL = s_ll;
}

inline void* ResolveAnimatorFromPedView(void* pvPtr, size_t offAnimancer, size_t offActor) {
    if (!SafeMem::IsPlausiblePtr(pvPtr)) return nullptr;
    if (void* animancer = SafeMem::PtrAt(pvPtr, offAnimancer)) {
        if (Il2Cpp::IsUnityObjectSafe(animancer)) {
            if (void* anim = SafeMem::PtrAt(animancer, 0x20)) {
                if (Il2Cpp::IsAnimatorAlive(anim)) return anim;
            }
        }
    }
    if (void* actor = SafeMem::PtrAt(pvPtr, offActor)) {
        if (Il2Cpp::IsUnityObjectSafe(actor)) {
            if (void* anim = SafeMem::PtrAt(actor, Offsets::g_cfg.offPhysicsActorAnimator)) {
                if (Il2Cpp::IsAnimatorAlive(anim)) return anim;
            }
        }
    }
    return nullptr;
}

inline void RefreshWorldSnapshot(float dt) {
    Morpeh::RefreshStoragesIfNeeded();
    int32_t localPlayerId = Morpeh::GetLocalPlayerId();
    int32_t localViewId   = Morpeh::GetLocalViewId();
    int64_t localPlayer   = Morpeh::GetLocalPlayerEntity();
    int64_t localPed      = Morpeh::GetLocalPedEntity();
    int32_t localPedId    = (localPed > 0) ? Morpeh::UnpackEntityId(localPed) : -1;
    g_localPedEntityAtomic.store(localPed, std::memory_order_release);

    // Local Player / Vehicle Runtime Mods (run at full tick rate)
    if (localPed > 0) {
        if (g_cfgState.bInfStamina) {
            Morpeh::SetEmpty(Morpeh::g_stashes.staminaInfStash, localPed);
            Morpeh::SetEmpty(Morpeh::g_stashes.ignoreStaminaLossStash, localPed);
        }
        if (g_cfgState.bCarHack) ApplyCarTuningTick(localPed);
        if (g_cfgState.bAutoRepair) {
            int64_t veh = Morpeh::GetLocalVehicleLogicEntity(localPed);
            if (veh > 0) {
                int32_t vid = Morpeh::UnpackEntityId(veh);
                if (void* hpPtr = Morpeh::Get(Morpeh::g_stashes.healthStash, vid, 4)) {
                    int32_t hp = 0;
                    if (SafeMem::ReadVal(hpPtr, hp) && hp > 0 && hp < 1000) {
                        int32_t fullHp = 1000;
                        SafeMem::Write(hpPtr, &fullHp, 4);
                    }
                }
            }
        }
        TickZoneGuard(localPed);
        TickAutoHealAndVest(localPed);
        TickWeaponMods(localPed);
    }
    TickPendingMapTeleport(localPed);
    TickFreeCam(localPed, dt);
    if (localPlayer > 0) TickRadioOverride(localPlayer);

    // Adaptive ECS scan cadence: 8Hz normally, ~6Hz in crowded areas (>16 players) to keep 60-120fps silky smooth
    static double s_lastEcsScan = 0.0;
    double now = CACurrentMediaTime();
    double ecsInterval = (g_renderPlayers.size() > 16) ? 0.165 : 0.125;
    if (now - s_lastEcsScan < ecsInterval) return;
    s_lastEcsScan = now;

    Vec3 camPos{};
    bool hasCamPos = false;
    if (void* cam = Il2Cpp::SafeCameraGetMain()) {
        if (void* camTr = Il2Cpp::SafeGetTransform(cam)) {
            hasCamPos = Il2Cpp::SafeGetTransformPos(camTr, camPos);
        }
    }

    std::string localClanId;
    if (localPlayerId >= 0) {
        if (void* cp = Morpeh::Get(Morpeh::g_stashes.playerClanStash, localPlayerId, 16))
            localClanId = Il2Cpp::ReadManagedString(SafeMem::PtrAt(cp, 0));
    }

    // Populate ESP / Aimbot / Spectate / KillAround / Player-List Snapshot
    bool killAroundActive = g_cfgState.bKillAroundAura || g_cfgState.bKillAroundAutoRepeat ||
                            (KillAround::GetMonotonicMs() < KillAround::g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed)) ||
                            (g_menuIsOpenAtomic.load(std::memory_order_relaxed) &&
                             g_activeTabAtomic.load(std::memory_order_relaxed) == 0 &&
                             g_activeSubTabAtomic.load(std::memory_order_relaxed) == 3);
    bool needPlayers = g_cfgState.bEnableESP || g_cfgState.bMiniMapEsp || g_cfgState.bEnableAimbot || g_cfgState.bSilentAim ||
                       g_cfgState.bScaleEnable || g_cfgState.bFollowTarget || g_cfgState.bKillFly ||
                       g_cfgState.bCarKill || g_cfgState.bDmgNumEnable || g_cfgState.bSpectateActive || g_cfgState.bFreeCam || killAroundActive;
    if (needPlayers && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.pedViewStash)) {
        size_t offRoot = 0, offHips = 0, offChest = 0, offHeadBone = 0;
        size_t offAnimancer = 0, offActor = 0, offRH = 0, offLH = 0, offRL = 0, offLL = 0;
        ResolvePedViewFieldOffsets(offRoot, offHips, offChest, offHeadBone,
                                   offAnimancer, offActor, offRH, offLH, offRL, offLL);

        // Build pedId -> playerId map from ControlsEntity (exact StateScript.dylib ped2player)
        std::unordered_map<int32_t, int32_t> ped2player;
        if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.controlsEntityStash)) {
            ped2player.reserve(128);
            Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.controlsEntityStash, [&](int32_t playerId, int) {
                if (playerId < 0) return;
                if (void* ctrlPtr = Morpeh::Get(Morpeh::g_stashes.controlsEntityStash, playerId, 8)) {
                    int64_t ctrlEnt = 0;
                    if (SafeMem::ReadVal(ctrlPtr, ctrlEnt) && ctrlEnt != 0) {
                        int32_t cPedId = Morpeh::UnpackEntityId(ctrlEnt);
                        if (cPedId >= 0) ped2player[cPedId] = playerId;
                    }
                }
            });
        }

        // Resolve local root transform & position for bulletproof self-exclusion
        void* localRootTr = nullptr;
        Vec3 localPos{};
        bool hasLocalPos = false;
        if (localViewId >= 0) {
            if (void* lpv = Morpeh::Get(Morpeh::g_stashes.pedViewStash, localViewId, 384)) {
                void* cand = SafeMem::PtrAt(lpv, offRoot);
                if (Il2Cpp::IsTransformAlive(cand)) {
                    localRootTr = cand;
                    hasLocalPos = Il2Cpp::SafeGetTransformPos(localRootTr, localPos);
                }
            }
        }
        if (!hasLocalPos && localPedId >= 0) {
            if (void* lpPtr = Morpeh::Get(Morpeh::g_stashes.positionStash, localPedId, sizeof(Vec3)))
                hasLocalPos = SafeMem::ReadVal(lpPtr, localPos) && IsFiniteVec3(localPos);
        }
        if (!hasLocalPos && hasCamPos) { localPos = camPos; hasLocalPos = true; }

        int32_t localDim = 0;
        bool hasLocalDim = false;
        if (localPedId >= 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.dimensionStash)) {
            if (void* dp = Morpeh::Get(Morpeh::g_stashes.dimensionStash, localPedId, 4))
                hasLocalDim = SafeMem::ReadVal(dp, localDim);
        }

        // Unity HumanBodyBones IDs matching kBoneEdges[14] (0=Head..14=LeftFoot)
        static constexpr int32_t kHumanBoneIds[15] = {
            10, // 0: Head
            8,  // 1: Chest
            0,  // 2: Hips
            14, // 3: RightUpperArm
            16, // 4: RightLowerArm
            18, // 5: RightHand
            13, // 6: LeftUpperArm
            15, // 7: LeftLowerArm
            17, // 8: LeftHand
            2,  // 9: RightUpperLeg
            4,  // 10: RightLowerLeg
            6,  // 11: RightFoot
            1,  // 12: LeftUpperLeg
            3,  // 13: LeftLowerLeg
            5   // 14: LeftFoot
        };

        std::vector<ESPPlayer> nextPlayers;
        nextPlayers.reserve(64);

        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.pedViewStash, [&](int32_t viewId, int) {
            if (viewId < 0) return;

            // 6-layer local-player & dead/knockout exclusion (100% native, ZERO GC)
            if (viewId == localViewId) return;
            if (Morpeh::HasNative(Morpeh::g_stashes.pedViewLocalStash, viewId)) return;
            if (Morpeh::HasNative(Morpeh::g_stashes.pedViewDeadStash, viewId)) return;
            if (Morpeh::HasNative(Morpeh::g_stashes.pedViewKoStash, viewId)) return;

            void* pvPtr = Morpeh::Get(Morpeh::g_stashes.pedViewStash, viewId, 384);
            if (!pvPtr) return;

            void* rootTr  = SafeMem::PtrAt(pvPtr, offRoot);
            if (!Il2Cpp::IsTransformAlive(rootTr)) return;
            if (localRootTr && rootTr == localRootTr) return;

            // Follow LogicEntityLink -> logic ped entity (ZERO il2cpp_runtime_invoke!)
            int64_t pedEnt = 0;
            int32_t pedId  = -1;
            if (void* linkPtr = Morpeh::Get(Morpeh::g_stashes.pedToLogicLinkStash, viewId, 8)) {
                if (SafeMem::ReadVal(linkPtr, pedEnt) && pedEnt != 0) {
                    pedId = Morpeh::UnpackEntityId(pedEnt);
                }
            }
            if (pedId < 0) return;
            if (localPedId >= 0 && pedId == localPedId) return;

            // Resolve owning Player entity via ped2player (ControlsEntity) OR ControlledBy fallback
            int32_t playerId = -1;
            int64_t playerEnt = 0;
            auto pIt = ped2player.find(pedId);
            if (pIt != ped2player.end()) {
                playerId = pIt->second;
            } else if (void* cbPtr = Morpeh::Get(Morpeh::g_stashes.controlledByStash, pedId, 8)) {
                if (SafeMem::ReadVal(cbPtr, playerEnt) && playerEnt != 0) {
                    playerId = Morpeh::UnpackEntityId(playerEnt);
                }
            }
            if (playerId >= 0 && localPlayerId >= 0 && playerId == localPlayerId) return;
            if (playerId < 0 && !g_cfgState.bShowNpcs) return;

            // Filter out entities in other dimensions / interiors
            if (hasLocalDim && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.dimensionStash)) {
                int32_t pedDim = localDim;
                if (void* dp = Morpeh::Get(Morpeh::g_stashes.dimensionStash, pedId, 4)) {
                    if (SafeMem::ReadVal(dp, pedDim) && pedDim != localDim) return;
                }
            }

            // Read LIVE Unity Transform foot position (zero GC, TCFlyGuard-protected)
            Vec3 footPos{};
            if (!Il2Cpp::SafeGetTransformPos(rootTr, footPos)) return;
            if (std::fabs(footPos.x) < 0.01f && std::fabs(footPos.y) < 0.01f && std::fabs(footPos.z) < 0.01f) return;

            // Final proximity safety net ONLY if local IDs couldn't be resolved yet
            if (localPedId < 0 && localViewId < 0 && hasLocalPos && Vec3Dist(localPos, footPos) < 0.45f) return;

            float maxScanDist = std::max(g_cfgState.espRadiusValue, 260.0f);
            if (hasLocalPos && Vec3Dist(localPos, footPos) > maxScanDist) return;

            void* headTr  = SafeMem::PtrAt(pvPtr, offHeadBone);
            void* chestTr = SafeMem::PtrAt(pvPtr, offChest);

            ESPPlayer pl{};
            pl.entityId        = pedId;
            pl.viewId          = viewId;
            pl.playerId        = playerId;
            pl.pedEntityVal    = pedEnt;
            pl.playerEntityVal = playerEnt;
            pl.rootTransform   = rootTr;
            pl.chestTransform  = Il2Cpp::IsTransformAlive(chestTr) ? chestTr : nullptr;
            pl.headTransform   = Il2Cpp::IsTransformAlive(headTr) ? headTr : nullptr;
            pl.worldPos = footPos;
            pl.headPos  = { footPos.x, footPos.y + Offsets::g_cfg.offPedHeadBoneYOffset, footPos.z };
            pl.chestPos = { footPos.x, footPos.y + 1.15f, footPos.z };

            if (pl.headTransform)  Il2Cpp::SafeGetTransformPos(pl.headTransform, pl.headPos);
            if (pl.chestTransform) Il2Cpp::SafeGetTransformPos(pl.chestTransform, pl.chestPos);

            if (void* vPtr = Morpeh::Get(Morpeh::g_stashes.velocityStash, pedId, sizeof(Vec3)))
                SafeMem::ReadVal(vPtr, pl.velocity);

            pl.health = 100;
            if (void* hPtr = Morpeh::Get(Morpeh::g_stashes.healthStash, pedId, 4)) {
                int32_t hp = 100;
                if (SafeMem::ReadVal(hPtr, hp)) pl.health = hp;
            }

            if (void* skPtr = Morpeh::Get(Morpeh::g_stashes.pedSkinStash, pedId, 4))
                SafeMem::ReadVal(skPtr, pl.skinId);

            if (void* clPtr = Morpeh::Get(Morpeh::g_stashes.pedClothesStash, pedId, 2))
                SafeMem::ReadVal(clPtr, pl.clothId);

            if (void* aPtr = Morpeh::Get(Morpeh::g_stashes.armorStash, pedId, 8)) {
                int32_t aVal = 0; uint8_t aType = 0;
                SafeMem::ReadVal(aPtr, aVal);
                SafeMem::ReadVal(reinterpret_cast<uint8_t*>(aPtr) + 4, aType);
                pl.armor = aVal;
                pl.armorMax = Offsets::ArmorMaxForType(aType);
            }

            if (void* sPtr = Morpeh::Get(Morpeh::g_stashes.pedSeatStash, pedId, 16)) {
                int64_t veh = 0;
                if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(sPtr) + 0x8, veh)) pl.inVehicle = (veh != 0);
            }

            // Skeleton Bone Transform resolution (PedView IK bones + Animator::GetBoneTransformInternal fallback)
            if (g_cfgState.bSkeletonESP && (!hasLocalPos || Vec3Dist(localPos, footPos) <= 105.0f)) {
                void* rawTr[15] = {
                    pl.headTransform,
                    pl.chestTransform,
                    SafeMem::PtrAt(pvPtr, offHips),
                    SafeMem::PtrAt(pvPtr, offRH + 0x0),
                    SafeMem::PtrAt(pvPtr, offRH + 0x8),
                    SafeMem::PtrAt(pvPtr, offRH + 0x10),
                    SafeMem::PtrAt(pvPtr, offLH + 0x0),
                    SafeMem::PtrAt(pvPtr, offLH + 0x8),
                    SafeMem::PtrAt(pvPtr, offLH + 0x10),
                    SafeMem::PtrAt(pvPtr, offRL + 0x0),
                    SafeMem::PtrAt(pvPtr, offRL + 0x8),
                    SafeMem::PtrAt(pvPtr, offRL + 0x10),
                    SafeMem::PtrAt(pvPtr, offLL + 0x0),
                    SafeMem::PtrAt(pvPtr, offLL + 0x8),
                    SafeMem::PtrAt(pvPtr, offLL + 0x10)
                };
                void* anim = nullptr;
                bool triedAnim = false;
                int validCount = 0;
                for (int bi = 0; bi < 15; ++bi) {
                    void* tr = Il2Cpp::IsTransformAlive(rawTr[bi]) ? rawTr[bi] : nullptr;
                    if (!tr) {
                        if (!triedAnim) {
                            triedAnim = true;
                            anim = ResolveAnimatorFromPedView(pvPtr, offAnimancer, offActor);
                        }
                        if (anim) tr = Il2Cpp::SafeGetAnimatorBone(anim, kHumanBoneIds[bi]);
                    }
                    pl.boneTransforms[bi] = tr;
                    Vec3 bp{};
                    if (tr && Il2Cpp::SafeGetTransformPos(tr, bp) && Vec3Dist(bp, footPos) < 2.8f) {
                        pl.bonePoints[bi] = bp;
                        pl.boneValid[bi]  = true;
                        ++validCount;
                    } else {
                        pl.boneValid[bi]  = false;
                    }
                }
                pl.hasBoneTransforms = (validCount >= 5);
                pl.hasBones = (validCount >= 5);
            }

            if (playerId >= 0) {
                if (void* didPtr = Morpeh::Get(Morpeh::g_stashes.digitalIdStash, playerId, 4)) {
                    uint32_t dig = 0;
                    if (SafeMem::ReadVal(didPtr, dig)) pl.entityGen = dig;
                }
                if (void* ntPtr = Morpeh::Get(Morpeh::g_stashes.nickTextStash, playerId, 16)) {
                    std::string fn = Il2Cpp::ReadManagedString(SafeMem::PtrAt(ntPtr, 0));
                    std::string ln = Il2Cpp::ReadManagedString(SafeMem::PtrAt(ntPtr, 8));
                    if (!fn.empty() && !ln.empty()) pl.name = fn + " " + ln;
                    else pl.name = !fn.empty() ? fn : ln;
                }
                pl.roleCategory = ResolvePlayerRoleCategory(playerId, localClanId);
            } else {
                pl.name = "NPC";
                pl.roleCategory = 2;
            }

            if (void* wPtr = Morpeh::Get(Morpeh::g_stashes.pedWeaponStash, pedId, 32)) {
                int32_t wId = 0;
                if (SafeMem::ReadVal(wPtr, wId)) pl.weaponName = LookupWeaponName(wId);
            }

            if (g_cfgState.bDmgNumEnable) {
                auto it = g_prevPlayerHealth.find(pedId);
                if (it != g_prevPlayerHealth.end() && pl.health < it->second && (it->second - pl.health) <= 200) {
                    FloatingDmgPopup dp{};
                    dp.worldPos = pl.headPos;
                    dp.delta = it->second - pl.health;
                    dp.bornAt = now;
                    g_dmgPopups.push_back(dp);
                }
                g_prevPlayerHealth[pedId] = pl.health;
            }

            nextPlayers.push_back(std::move(pl));
        });

        size_t foundPlayers = nextPlayers.size();
        {
            std::lock_guard<std::mutex> lk(g_espMutex);
            g_renderPlayers = std::move(nextPlayers);
        }
        if (g_cfgState.lastStatusBanner == "Ready" || g_cfgState.lastStatusBanner.rfind("Bound:", 0) == 0) {
            char st[96];
            snprintf(st, sizeof(st), "Bound: W:%s DI:%s PV:%s | Players:%zu",
                     Morpeh::g_stashes.cachedWorld ? "OK" : "-",
                     Morpeh::g_stashes.realContainer ? "OK" : "-",
                     Morpeh::g_stashes.pedViewStash ? "OK" : "-",
                     foundPlayers);
            g_cfgState.lastStatusBanner = st;
        }
    } else if (g_cfgState.lastStatusBanner == "Ready" || g_cfgState.lastStatusBanner.rfind("Bound:", 0) == 0) {
        char st[96];
        snprintf(st, sizeof(st), "Bound: W:%s DI:%s PV:%s",
                 Morpeh::g_stashes.cachedWorld ? "OK" : "-",
                 Morpeh::g_stashes.realContainer ? "OK" : "-",
                 Morpeh::g_stashes.pedViewStash ? "OK" : "-");
        g_cfgState.lastStatusBanner = st;
    }

    // Populate Spray / Clan Graffiti Nodes when Spray ESP or Spray Auto-TP is enabled
    if ((g_cfgState.bSprayESP || g_cfgState.bSprayAutoTp) && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.graffitiCpStash)) {
        std::vector<SprayNode> nextSprays;
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.graffitiCpStash, [&](int32_t gId, int) {
            if (void* pPtr = Morpeh::Get(Morpeh::g_stashes.positionStash, gId, sizeof(Vec3))) {
                Vec3 pos{};
                if (!SafeMem::ReadVal(pPtr, pos) || !IsFiniteVec3(pos)) return;
                SprayNode sn{};
                sn.worldPos = pos;
                sn.distance = hasCamPos ? Vec3Dist(camPos, pos) : 0.0f;
                int16_t state = 1;
                if (void* stPtr = Morpeh::Get(Morpeh::g_stashes.graffitiStateStash, gId, 2))
                    SafeMem::ReadVal(stPtr, state);
                sn.sprayable = (state == 1 || state == 2);
                sn.clash     = (state == 2);
                sn.nearby    = (sn.distance <= 55.0f);
                sn.district  = true;
                if (void* idPtr = Morpeh::Get(Morpeh::g_stashes.graffitiCpStash, gId, 8))
                    sn.label = Il2Cpp::ReadManagedString(SafeMem::PtrAt(idPtr, 0));
                if (sn.label.empty()) sn.label = "Spray #" + std::to_string(gId);
                nextSprays.push_back(std::move(sn));
            }
        });
        std::lock_guard<std::mutex> lk(g_espMutex);
        g_sprayNodes = std::move(nextSprays);
    }

    // Populate Checkpoints list when job/automation toggles need it
    if ((g_cfgState.bJobTp || g_cfgState.bMoneyGlitch || g_cfgState.bWalkCp ||
         g_cfgState.bVehAiDrive || g_cfgState.bStealEvent || g_cfgState.bGemsFarm) &&
        SafeMem::IsPlausiblePtr(Morpeh::g_stashes.checkpointTagStash)) {
        std::vector<CheckpointEntry> nextCps;
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.checkpointTagStash, [&](int32_t cpId, int) {
            if (void* posPtr = Morpeh::Get(Morpeh::g_stashes.checkpointPosStash, cpId, sizeof(Vec3))) {
                Vec3 cpPos{};
                if (SafeMem::ReadVal(posPtr, cpPos) && IsFiniteVec3(cpPos)) {
                    CheckpointEntry e{};
                    e.entityId = cpId;
                    e.pos = cpPos;
                    e.distance = hasCamPos ? Vec3Dist(camPos, cpPos) : 0.0f;
                    e.isJob = Morpeh::HasNative(Morpeh::g_stashes.jobCpStash, cpId);
                    e.isLivraison = e.isJob;
                    nextCps.push_back(e);
                }
            }
        });
        std::lock_guard<std::mutex> lk(g_espMutex);
        g_checkpoints = std::move(nextCps);
    }

    // Gated Map Markers scan (~2.5Hz) — ONLY runs when Map TP tab is open OR Auto-TP is enabled (zero gameplay lag!)
    bool menuOpen = g_menuIsOpenAtomic.load(std::memory_order_relaxed);
    int  uiTab    = g_activeTabAtomic.load(std::memory_order_relaxed);
    int  uiSub    = g_activeSubTabAtomic.load(std::memory_order_relaxed);

    if (g_cfgState.bAutoMapMarkerTp || (menuOpen && uiTab == 2 && uiSub == 1)) {
        static double s_lastMapScan = 0.0;
        double scanInterval = g_cfgState.bAutoMapMarkerTp ? 0.20 : 0.40;
        if (now - s_lastMapScan >= scanInterval) {
            s_lastMapScan = now;
            ScanMapMarkers(camPos, hasCamPos, true);
        }
    }

    // Gated Server-Wide All Players scan (~2Hz) — runs when FreeCam, Live Players, or Kill Around is open/active!
    if ((menuOpen && ((uiTab == 1 && uiSub == 1) || (uiTab == 3 && uiSub == 2) || (uiTab == 0 && uiSub == 3))) ||
        g_cfgState.bKillAroundAura || g_cfgState.bKillAroundAutoRepeat ||
        KillAround::GetMonotonicMs() < KillAround::g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed)) {
        static double s_lastServerPlScan = 0.0;
        if (now - s_lastServerPlScan >= 0.45) {
            s_lastServerPlScan = now;
            ScanAllServerPlayers(localPlayerId, camPos, hasCamPos);
        }
    }
}

// ============================================================================
// Automation: JobTP, MoneyGlitch, StealEvent, WalkCp, VehAi, Gems, SprayTP,
// FollowTarget, KillFly & CarKill
// ============================================================================
inline bool FindTargetPlayerByPid(int64_t pid, ESPPlayer& outPl) {
    if (pid <= 0) return false;
    std::lock_guard<std::mutex> lk(g_espMutex);
    for (const auto& pl : g_renderPlayers) {
        if ((int64_t)pl.entityGen == pid || (int64_t)pl.entityId == pid) {
            outPl = pl;
            return true;
        }
    }
    return false;
}

inline void TickAutomation(float dt) {
    double now = CACurrentMediaTime();
    Vec3 localPos{};
    bool hasLocal = GetLocalPosition(localPos);

    // 1) Auto-accept TemporaryMini popups during MoneyGlitch
    if (g_cfgState.bMoneyGlitch && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.tempMiniStash) &&
        SafeMem::IsPlausiblePtr(Morpeh::g_stashes.acceptPressedStash)) {
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.tempMiniStash, [&](int32_t miniId, int) {
            int64_t mEnt = Morpeh::GetEntityById(miniId);
            if (mEnt != 0) Morpeh::SetEmpty(Morpeh::g_stashes.acceptPressedStash, mEnt);
        });
    }

    // 2) Pick nearest checkpoint snapshot
    std::vector<CheckpointEntry> cps;
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        cps = g_checkpoints;
    }
    const CheckpointEntry* nearestJob = nullptr;
    const CheckpointEntry* nearestAny = nullptr;
    float bestJobDist = 1e9f, bestAnyDist = 1e9f;
    for (const auto& cp : cps) {
        float d = hasLocal ? Vec3Dist(localPos, cp.pos) : cp.distance;
        if (d < 2.0f) continue;
        if (d < bestAnyDist) { bestAnyDist = d; nearestAny = &cp; }
        if (cp.isJob && d < bestJobDist) { bestJobDist = d; nearestJob = &cp; }
    }
    const CheckpointEntry* chosenCp = nearestJob ? nearestJob : nearestAny;

    // 3) Job Checkpoint Pull / MoneyGlitch / StealEvent
    if ((g_cfgState.bJobTp || g_cfgState.bMoneyGlitch || g_cfgState.bStealEvent) && hasLocal && chosenCp) {
        static double s_lastPull = 0.0;
        double interval = g_cfgState.bMoneyGlitch ? std::max(1.5, (double)g_cfgState.moneyGlitchSpeed) : 1.0;
        if (now - s_lastPull >= interval) {
            if (void* cpPosPtr = Morpeh::Get(Morpeh::g_stashes.positionStash, chosenCp->entityId, sizeof(Vec3))) {
                SafeMem::Write(cpPosPtr, &localPos, sizeof(Vec3));
                s_lastPull = now;
                g_cfgState.lastStatusBanner = "Pulled checkpoint #" + std::to_string(chosenCp->entityId);
            }
        }
    }

    // 4) Walk Checkpoints (writes InputsData KeyHorizontal/KeyVertical/KeySprint)
    if (g_cfgState.bWalkCp && hasLocal && chosenCp && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.inputsDataStash)) {
        Vec3 dir = Vec3Norm({ chosenCp->pos.x - localPos.x, 0.0f, chosenCp->pos.z - localPos.z });
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.inputsDataStash, [&](int32_t id, int) {
            if (void* inp = Morpeh::Get(Morpeh::g_stashes.inputsDataStash, id, 88)) {
                uint8_t sprint = 1;
                SafeMem::Write(reinterpret_cast<uint8_t*>(inp) + 0x4, &dir.x, 4);
                SafeMem::Write(reinterpret_cast<uint8_t*>(inp) + 0x8, &dir.z, 4);
                SafeMem::Write(reinterpret_cast<uint8_t*>(inp) + 0x1D, &sprint, 1);
            }
        });
    }

    // 5) VehAi Auto Drive (writes VehicleViewInputs gas/steer + gentle velocity toward checkpoint)
    if (g_cfgState.bVehAiDrive && hasLocal && chosenCp) {
        int64_t veh = ResolveTargetVehicleLogic(-1);
        if (veh > 0) {
            int64_t vv = Morpeh::GetViewEntity(veh);
            int32_t vvid = (vv > 0) ? Morpeh::UnpackEntityId(vv) : -1;
            Vec3 toCp = Vec3Norm({ chosenCp->pos.x - localPos.x, 0.0f, chosenCp->pos.z - localPos.z });
            if (vvid >= 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.vehViewInputsStash)) {
                if (void* vi = Morpeh::Get(Morpeh::g_stashes.vehViewInputsStash, vvid, 24)) {
                    float gas = 1.0f;
                    uint8_t gasOn = 1;
                    SafeMem::Write(reinterpret_cast<uint8_t*>(vi) + 0x0, &gas, 4);
                    SafeMem::Write(reinterpret_cast<uint8_t*>(vi) + 0x14, &gasOn, 1);
                }
            }
            if (void* vRb = Morpeh::GetEntityRigidbody(vv)) {
                Vec3 driveVel{ toCp.x * 22.0f, -0.5f, toCp.z * 22.0f };
                Il2Cpp::SafeSetRbVelocity(vRb, driveVel);
            }
        }
    }

    // 6) Gems Route Farm (teleports across checkpoints on timer)
    if (g_cfgState.bGemsFarm && !cps.empty()) {
        static double s_lastGemTp = 0.0;
        static size_t s_gemIdx = 0;
        if (now - s_lastGemTp >= 3.0) {
            const auto& cp = cps[s_gemIdx % cps.size()];
            TeleportLocalTo({ cp.pos.x, cp.pos.y + 1.0f, cp.pos.z });
            ++s_gemIdx;
            s_lastGemTp = now;
            g_cfgState.lastStatusBanner = "GemsFarm -> CP #" + std::to_string(cp.entityId);
        }
    }

    // 7) Spray Auto-TP
    if (g_cfgState.bSprayAutoTp) {
        static double s_lastSprayTp = 0.0;
        static size_t s_sprayIdx = 0;
        if (now - s_lastSprayTp >= std::max(1.0, (double)g_cfgState.sprayAutoTpDelay)) {
            std::vector<SprayNode> sprays;
            {
                std::lock_guard<std::mutex> lk(g_espMutex);
                sprays = g_sprayNodes;
            }
            for (size_t i = 0; i < sprays.size(); ++i) {
                const auto& sn = sprays[(s_sprayIdx + i) % sprays.size()];
                if (g_cfgState.bSprayEspOnlySprayable && !sn.sprayable) continue;
                TeleportLocalTo({ sn.worldPos.x, sn.worldPos.y + 1.2f, sn.worldPos.z });
                s_sprayIdx = (s_sprayIdx + i + 1) % std::max<size_t>(1, sprays.size());
                s_lastSprayTp = now;
                g_cfgState.lastStatusBanner = "SprayTP -> " + sn.label;
                break;
            }
        }
    }

    // 8) Follow Target / KillFly / CarKill
    if (g_cfgState.bFollowTarget || g_cfgState.bKillFly || g_cfgState.bCarKill) {
        ESPPlayer tgt{};
        if (FindTargetPlayerByPid(g_cfgState.targetPlayerPid, tgt) && !tgt.dead) {
            if (g_cfgState.bKillFly) {
                if (!g_killFlyActive && hasLocal) {
                    g_killFlyReturnPos = localPos;
                    g_killFlyActive = true;
                }
                Vec3 hover{ tgt.worldPos.x, tgt.worldPos.y + 1.8f, tgt.worldPos.z - 1.6f };
                TeleportLocalTo(hover, {0, 0, 0}, false);
            } else if (g_cfgState.bCarKill) {
                Vec3 dropPos{ tgt.worldPos.x, tgt.worldPos.y + 0.6f, tgt.worldPos.z };
                TeleportLocalTo(dropPos, {0, -18.0f, 0}, true);
            } else if (g_cfgState.bFollowTarget) {
                static double s_lastFollow = 0.0;
                if (now - s_lastFollow >= 0.15) {
                    Vec3 behind{ tgt.worldPos.x - 2.0f, tgt.worldPos.y + 0.4f, tgt.worldPos.z - 2.0f };
                    TeleportLocalTo(behind);
                    s_lastFollow = now;
                }
            }
        } else if (g_killFlyActive && !g_cfgState.bKillFly) {
            TeleportLocalTo(g_killFlyReturnPos, {0, 0, 0}, true);
            g_killFlyActive = false;
        }
    } else if (g_killFlyActive) {
        TeleportLocalTo(g_killFlyReturnPos, {0, 0, 0}, true);
        g_killFlyActive = false;
    }

    (void)dt;
}

// ============================================================================
// Aimbot + Silent Aim Target Selection & Overlay Visual Rendering
// ============================================================================
inline void ApplyAimbotStep(void* cam, float w, float h, ImDrawList* bg = nullptr) {
    if ((!g_cfgState.bEnableAimbot && !g_cfgState.bSilentAim) || !Il2Cpp::IsCameraAlive(cam)) {
        Hooks::g_silentAimHasTarget.store(false, std::memory_order_release);
        return;
    }
    std::vector<ESPPlayer> snapshot;
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        snapshot = g_renderPlayers;
    }

    ImVec2 center(w * 0.5f, h * 0.5f);
    float bestDist = g_cfgState.aimFov;
    const ESPPlayer* bestTarget = nullptr;
    Vec3 bestWorld{};
    Vec2 bestScreen{};
    bool ignoreWalls = (g_cfgState.bSilentAim && g_cfgState.bSilentWallbang);

    for (const ESPPlayer& pl : snapshot) {
        if (pl.dead || pl.roleCategory < 0 || pl.roleCategory > 10) continue;
        if (!g_cfgState.bLockRole[pl.roleCategory]) continue;
        if (g_cfgState.bVisCheck && !ignoreWalls && !pl.visible) continue;

        Vec3 aimPt = (g_cfgState.aimBoneMode == 0) ? pl.headPos : pl.chestPos;
        if (g_cfgState.aimBoneMode == 0 && pl.headTransform) ReadTransformPosition(pl.headTransform, aimPt);
        else if (g_cfgState.aimBoneMode != 0 && pl.chestTransform) ReadTransformPosition(pl.chestTransform, aimPt);

        if (g_cfgState.bPrediction && g_cfgState.bulletSpeed > 1.0f) {
            float leadDt = g_cfgState.predictionFactor / g_cfgState.bulletSpeed;
            aimPt.x += pl.velocity.x * leadDt;
            aimPt.y += pl.velocity.y * leadDt;
            aimPt.z += pl.velocity.z * leadDt;
        }
        Vec2 sPos{};
        if (!WorldToScreenViewport(cam, aimPt, w, h, sPos)) continue;
        float d = std::hypot(sPos.x - center.x, sPos.y - center.y);
        if (d < bestDist) {
            bestDist = d;
            bestTarget = &pl;
            bestWorld = aimPt;
            bestScreen = sPos;
        }
    }

    if (bestTarget) {
        // Lock-free atomic publication for the ARM64 Mach HWBP exception thread
        Hooks::g_saTargetX.store(bestWorld.x, std::memory_order_relaxed);
        Hooks::g_saTargetY.store(bestWorld.y, std::memory_order_relaxed);
        Hooks::g_saTargetZ.store(bestWorld.z, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lk(Hooks::g_silentAimMutex);
            Hooks::g_silentAimTargetPos = bestWorld;
        }
        Hooks::g_silentAimHasTarget.store(true, std::memory_order_release);

        if (bg && g_cfgState.bSilentAim && g_cfgState.bAimIndicator) {
            ImU32 lockCol = g_cfgState.bSilentWallbang ? IM_COL32(0, 240, 255, 225) : IM_COL32(255, 80, 110, 225);
            bg->AddLine(center, ImVec2(bestScreen.x, bestScreen.y), IM_COL32(0, 240, 255, 110), 1.2f);
            bg->AddCircle(ImVec2(bestScreen.x, bestScreen.y), 6.5f, lockCol, 16, 1.6f);
            bg->AddCircleFilled(ImVec2(bestScreen.x, bestScreen.y), 2.2f, IM_COL32(255, 255, 255, 245));
        }
    } else {
        Hooks::g_silentAimHasTarget.store(false, std::memory_order_release);
    }

    if (bestTarget && g_cfgState.bEnableAimbot && g_cfgState.bSmoothCamAim) {
        if (void* camTr = Il2Cpp::SafeGetTransform(cam)) {
            Vec3 camPos{}, curFwd{0, 0, 1};
            if (Il2Cpp::SafeGetTransformPos(camTr, camPos) && Il2Cpp::SafeGetTransformForward(camTr, curFwd)) {
                Vec3 desired = Vec3Norm({ bestWorld.x - camPos.x, bestWorld.y - camPos.y, bestWorld.z - camPos.z });
                float t = std::clamp(g_cfgState.aimSmoothing, 0.05f, 1.0f);
                Vec3 blended = Vec3Norm({
                    curFwd.x + (desired.x - curFwd.x) * t,
                    curFwd.y + (desired.y - curFwd.y) * t,
                    curFwd.z + (desired.z - curFwd.z) * t
                });
                // Primary: exact StateScript.dylib MainCameraStorage::SetCameraLookDirection(Vector3)
                if (Il2Cpp::IsUnityObjectSafe(Morpeh::g_stashes.realMainCamStorage) && Il2Cpp::g_unity.mMainCam_SetLookDir) {
                    Vec3 dirArg = blended;
                    void* a[1] = { &dirArg };
                    Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mMainCam_SetLookDir, Morpeh::g_stashes.realMainCamStorage, a);
                } else {
                    Il2Cpp::SafeSetTransformRot(camTr, LookRotationQuat(blended));
                }
            }
        }
    }
}

inline void DrawCornerBox(ImDrawList* bg, float x0, float y0, float w, float h, ImU32 col, float thick) {
    float cw = std::min(w * 0.28f, 16.0f);
    float ch = std::min(h * 0.22f, 18.0f);
    ImU32 blk = IM_COL32(0, 0, 0, 190);
    auto seg = [&](ImVec2 a, ImVec2 b) {
        bg->AddLine(a, b, blk, thick + 1.6f);
        bg->AddLine(a, b, col, thick);
    };
    seg({x0, y0}, {x0 + cw, y0});             seg({x0, y0}, {x0, y0 + ch});
    seg({x0 + w - cw, y0}, {x0 + w, y0});     seg({x0 + w, y0}, {x0 + w, y0 + ch});
    seg({x0, y0 + h}, {x0 + cw, y0 + h});     seg({x0, y0 + h - ch}, {x0, y0 + h});
    seg({x0 + w - cw, y0 + h}, {x0 + w, y0 + h}); seg({x0 + w, y0 + h - ch}, {x0 + w, y0 + h});
}

inline void DrawOverlayVisuals(ImDrawList* bg, float w, float h) {
    if (!AuthSystem::IsVerifiedFast()) return;
    if (g_cfgState.bMasterDisable) return;

    if ((g_cfgState.bEnableAimbot || g_cfgState.bSilentAim) && g_cfgState.bDrawFov) {
        ImU32 ringCol = Hooks::g_silentAimHasTarget.load() ? IM_COL32(255, 75, 95, 220) : ThemeEngine::GetAccent(0.68f);
        bg->AddCircle(ImVec2(w * 0.5f, h * 0.5f), g_cfgState.aimFov, ringCol, 64, 1.4f);
    }

    if (g_cfgState.bCrosshairEnable) {
        ImVec2 c(w * 0.5f, h * 0.5f);
        float sz  = g_cfgState.crosshairSize;
        float th  = std::max(1.0f, g_cfgState.crosshairThickness * 0.25f);
        float gap = g_cfgState.crosshairGap;
        int alpha = static_cast<int>(std::clamp(g_cfgState.crosshairOpacity, 0.0f, 255.0f));
        ImU32 col = IM_COL32((int)(g_cfgState.crosshairColor[0] * 255),
                             (int)(g_cfgState.crosshairColor[1] * 255),
                             (int)(g_cfgState.crosshairColor[2] * 255), alpha);
        ImU32 out = IM_COL32(0, 0, 0, static_cast<int>(alpha * 0.8f));
        auto arm = [&](ImVec2 a, ImVec2 b) {
            if (g_cfgState.crosshairOutline) bg->AddLine(a, b, out, th + 2.0f);
            bg->AddLine(a, b, col, th);
        };
        if (g_cfgState.crosshairType == 0) {
            if (g_cfgState.crosshairOutline) bg->AddCircleFilled(c, th + 1.5f, out);
            bg->AddCircleFilled(c, th, col);
        } else {
            arm(ImVec2(c.x - sz - gap, c.y), ImVec2(c.x - gap, c.y));
            arm(ImVec2(c.x + gap, c.y), ImVec2(c.x + sz + gap, c.y));
            arm(ImVec2(c.x, c.y - sz - gap), ImVec2(c.x, c.y - gap));
            arm(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + sz + gap));
        }
    }

    if (g_cfgState.bShotPartsEnable) {
        float sr = 1, sg = 1, sb = 1;
        ComputeShotPartsColor(CACurrentMediaTime(), sr, sg, sb);
        ImU32 fxCol = IM_COL32((int)(std::clamp(sr, 0.f, 1.f) * 255),
                               (int)(std::clamp(sg, 0.f, 1.f) * 255),
                               (int)(std::clamp(sb, 0.f, 1.f) * 255), 140);
        bg->AddCircle(ImVec2(w * 0.5f, h * 0.5f), 5.0f, fxCol, 16, 1.5f);
    }

    void* cam = Il2Cpp::SafeCameraGetMain();
    if (!cam && Il2Cpp::IsUnityObjectSafe(Morpeh::g_stashes.realMainCamStorage) && Il2Cpp::g_unity.mMainCam_GetCamera) {
        void* mc = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mMainCam_GetCamera, Morpeh::g_stashes.realMainCamStorage, nullptr);
        if (Il2Cpp::IsCameraAlive(mc)) cam = mc;
    }
    bool hasCam = (cam != nullptr);
    if (hasCam) ApplyAimbotStep(cam, w, h, bg);

    // Floating Damage Numbers FX
    if (g_cfgState.bDmgNumEnable && hasCam && !g_dmgPopups.empty()) {
        double now = CACurrentMediaTime();
        float dr = 1, dg = 0.25f, db = 0.25f;
        ComputeShotPartsColor(now, dr, dg, db);
        for (auto it = g_dmgPopups.begin(); it != g_dmgPopups.end(); ) {
            double age = now - it->bornAt;
            if (age > 1.2) { it = g_dmgPopups.erase(it); continue; }
            Vec3 wp = it->worldPos;
            wp.y += static_cast<float>(age) * 1.1f;
            Vec2 sp{};
            if (WorldToScreenViewport(cam, wp, w, h, sp)) {
                int alpha = static_cast<int>(std::clamp((1.0 - age / 1.2) * 255.0, 0.0, 255.0));
                char dbuf[24];
                snprintf(dbuf, sizeof(dbuf), "-%d", it->delta);
                ImU32 outCol = IM_COL32(0, 0, 0, alpha);
                ImU32 fgCol  = IM_COL32((int)(std::clamp(dr, 0.f, 1.f) * 255),
                                        (int)(std::clamp(dg, 0.f, 1.f) * 255),
                                        (int)(std::clamp(db, 0.f, 1.f) * 255), alpha);
                bg->AddText(ImVec2(sp.x + 1, sp.y + 1), outCol, dbuf);
                bg->AddText(ImVec2(sp.x, sp.y), fgCol, dbuf);
            }
            ++it;
        }
    }

    // Spray Nodes ESP
    if (g_cfgState.bSprayESP && hasCam) {
        std::vector<SprayNode> sprays;
        {
            std::lock_guard<std::mutex> lk(g_espMutex);
            sprays = g_sprayNodes;
        }
        ImU32 sCol = IM_COL32((int)(g_cfgState.sprayEspCol[0] * 255),
                              (int)(g_cfgState.sprayEspCol[1] * 255),
                              (int)(g_cfgState.sprayEspCol[2] * 255), 220);
        for (const auto& sn : sprays) {
            if (sn.distance > g_cfgState.sprayEspDistance) continue;
            if (g_cfgState.bSprayEspOnlySprayable && !sn.sprayable) continue;
            if (g_cfgState.bSprayEspOnlyClash && !sn.clash) continue;
            if (g_cfgState.bSprayEspOnlyNear && !sn.nearby) continue;
            if (g_cfgState.bSprayEspOnlyDistrict && !sn.district) continue;
            Vec2 sp{};
            if (!WorldToScreenViewport(cam, sn.worldPos, w, h, sp)) continue;
            if (g_cfgState.bSprayEspBox) bg->AddRect(ImVec2(sp.x - 10, sp.y - 10), ImVec2(sp.x + 10, sp.y + 10), sCol, 4.0f, 0, 1.5f);
            if (g_cfgState.bSprayEspLines) bg->AddLine(ImVec2(w * 0.5f, h), ImVec2(sp.x, sp.y), sCol, 1.2f);
            char sb[64];
            snprintf(sb, sizeof(sb), "%s (%.0fm)", sn.label.c_str(), sn.distance);
            bg->AddText(ImVec2(sp.x - 24, sp.y - 24), sCol, sb);
        }
    }

    if (!g_cfgState.bEnableESP && !g_cfgState.bMiniMapEsp) return;
    std::vector<ESPPlayer> snapshot;
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        snapshot = g_renderPlayers;
    }

    Vec3 camPos{}, camFwd{0, 0, 1};
    bool hasCamPos = false;
    if (hasCam) {
        if (void* camTr = Il2Cpp::SafeGetTransform(cam)) {
            hasCamPos = Il2Cpp::SafeGetTransformPos(camTr, camPos);
            Il2Cpp::SafeGetTransformForward(camTr, camFwd);
        }
    }

    // MiniMap Radar (Top-Right corner)
    if (g_cfgState.bMiniMapEsp && hasCamPos) {
        float rSize = 112.0f;
        ImVec2 rMin(w - rSize - 18.0f, 52.0f);
        ImVec2 rCtr(rMin.x + rSize * 0.5f, rMin.y + rSize * 0.5f);
        bg->AddRectFilled(rMin, ImVec2(rMin.x + rSize, rMin.y + rSize), IM_COL32(20, 22, 29, 210), 10.0f);
        bg->AddRect(rMin, ImVec2(rMin.x + rSize, rMin.y + rSize), ThemeEngine::GetAccent(0.55f), 10.0f, 0, 1.2f);
        bg->AddLine(ImVec2(rCtr.x, rMin.y + 6), ImVec2(rCtr.x, rMin.y + rSize - 6), IM_COL32(255, 255, 255, 28), 1.0f);
        bg->AddLine(ImVec2(rMin.x + 6, rCtr.y), ImVec2(rMin.x + rSize - 6, rCtr.y), IM_COL32(255, 255, 255, 28), 1.0f);
        bg->AddCircleFilled(rCtr, 3.2f, IM_COL32(255, 255, 255, 240));
        float yaw = std::atan2(camFwd.x, camFwd.z);
        float cs = std::cos(yaw), sn = std::sin(yaw);
        float scale = (rSize * 0.45f) / std::max(25.0f, g_cfgState.espRadiusValue);
        for (const auto& pl : snapshot) {
            if (pl.dead || pl.roleCategory < 0 || pl.roleCategory > 10) continue;
            if (!g_cfgState.bShowRole[pl.roleCategory] || g_cfgState.bHideRole[pl.roleCategory]) continue;
            float dx = pl.worldPos.x - camPos.x;
            float dz = pl.worldPos.z - camPos.z;
            float rx = (dx * cs - dz * sn) * scale;
            float ry = (dx * sn + dz * cs) * scale;
            float lim = rSize * 0.44f;
            rx = std::clamp(rx, -lim, lim);
            ry = std::clamp(ry, -lim, lim);
            ImU32 dCol = IM_COL32(g_cfgState.roleR[pl.roleCategory], g_cfgState.roleG[pl.roleCategory], g_cfgState.roleB[pl.roleCategory], 255);
            bg->AddCircleFilled(ImVec2(rCtr.x + rx, rCtr.y - ry), 3.0f, dCol);
        }
    }

    if (!g_cfgState.bEnableESP || !hasCam) return;

    // ZERO-LAG MATRIX W2S: fetch View*Projection ONCE per frame -> pure C++ math for ALL players & bones!
    Il2Cpp::FrameCameraVP fvp = Il2Cpp::BuildFrameCameraVP(cam);

    static constexpr int kBoneEdges[14][2] = {
        {0, 1}, {1, 2},           // Head -> Chest -> Hips
        {1, 3}, {3, 4}, {4, 5},   // Right Arm
        {1, 6}, {6, 7}, {7, 8},   // Left Arm
        {2, 9}, {9, 10}, {10, 11},// Right Leg
        {2, 12}, {12, 13}, {13, 14} // Left Leg
    };

    int count = 0;
    int liveSkelBudget = 6; // Refresh live 15-bone transforms only for closest/first 6 visible players; shift cached bones for crowd!
    for (const ESPPlayer& pl : snapshot) {
        if (pl.dead || pl.roleCategory < 0 || pl.roleCategory > 10) continue;
        if (!g_cfgState.bShowRole[pl.roleCategory] || g_cfgState.bHideRole[pl.roleCategory]) continue;

        // Fast 1-transform read per player; derive head using cached head-to-foot offset (cuts 50% of transform icalls!)
        Vec3 liveFoot = pl.worldPos;
        if (pl.rootTransform) ReadTransformPosition(pl.rootTransform, liveFoot);
        Vec3 delta{ liveFoot.x - pl.worldPos.x, liveFoot.y - pl.worldPos.y, liveFoot.z - pl.worldPos.z };
        Vec3 liveHead{ pl.headPos.x + delta.x, pl.headPos.y + delta.y, pl.headPos.z + delta.z };

        float dist = hasCamPos ? Vec3Dist(camPos, liveFoot) : 0.0f;
        if (hasCamPos && dist > g_cfgState.espRadiusValue) continue;

        // Top of skull = headBone + 0.22m, bottom of shoes = root - 0.04m
        Vec3 topWorld{ liveHead.x, liveHead.y + 0.22f, liveHead.z };
        Vec3 botWorld{ liveFoot.x, liveFoot.y - 0.04f, liveFoot.z };

        Vec2 footS{}, headS{};
        if (!Il2Cpp::ProjectWorldToScreenFast(fvp, cam, botWorld, w, h, footS)) continue;
        if (!Il2Cpp::ProjectWorldToScreenFast(fvp, cam, topWorld, w, h, headS)) continue;

        // Fast off-screen frustum cull before allocating strings or drawing primitives
        if (footS.x < -140.0f || footS.x > w + 140.0f || headS.y > h + 140.0f || footS.y < -140.0f) continue;

        float topY = std::min(headS.y, footS.y);
        float botY = std::max(headS.y, footS.y);
        float boxH = botY - topY;
        if (boxH < 6.0f || boxH > h * 1.4f) continue;

        float boxW = boxH * 0.48f;
        float midX = (headS.x + footS.x) * 0.5f;
        float x0 = midX - boxW * 0.5f;
        float y0 = topY;

        ImU32 rCol = IM_COL32(g_cfgState.roleR[pl.roleCategory],
                              g_cfgState.roleG[pl.roleCategory],
                              g_cfgState.roleB[pl.roleCategory], 255);
        ImU32 skelCol = IM_COL32((int)(g_cfgState.espSkelCol[0] * 255),
                                 (int)(g_cfgState.espSkelCol[1] * 255),
                                 (int)(g_cfgState.espSkelCol[2] * 255), 240);
        ++count;

        if (g_cfgState.bEspBox) {
            if (g_cfgState.espBoxStyle == 1) {
                DrawCornerBox(bg, x0, y0, boxW, boxH, rCol, 1.5f);
            } else {
                bg->AddRect(ImVec2(x0 - 1, y0 - 1), ImVec2(x0 + boxW + 1, y0 + boxH + 1), IM_COL32(0, 0, 0, 185), 2.0f);
                bg->AddRect(ImVec2(x0, y0), ImVec2(x0 + boxW, y0 + boxH), rCol, 2.0f, 0, 1.35f);
            }
        }
        if (g_cfgState.bEspHealth) {
            float hp = std::clamp(pl.health / 100.0f, 0.0f, 1.0f);
            ImU32 hpCol = IM_COL32((int)((1.0f - hp) * 240.0f), (int)(hp * 235.0f), 65, 245);
            bg->AddRectFilled(ImVec2(x0 - 6, y0 - 1), ImVec2(x0 - 2, y0 + boxH + 1), IM_COL32(10, 12, 16, 200));
            bg->AddRectFilled(ImVec2(x0 - 5, y0 + boxH * (1.0f - hp)), ImVec2(x0 - 3, y0 + boxH), hpCol);
        }
        if (g_cfgState.bEspArmor && pl.armor > 0) {
            float ar = std::clamp(pl.armor / static_cast<float>(std::max(1, pl.armorMax)), 0.0f, 1.0f);
            bg->AddRectFilled(ImVec2(x0, y0 + boxH + 2), ImVec2(x0 + boxW, y0 + boxH + 5), IM_COL32(10, 12, 16, 195));
            bg->AddRectFilled(ImVec2(x0, y0 + boxH + 2.5f), ImVec2(x0 + boxW * ar, y0 + boxH + 4.5f), IM_COL32(60, 165, 255, 245));
        }
        if (g_cfgState.bSkeletonESP && pl.hasBones && (!hasCamPos || dist <= 95.0f)) {
            Vec2 boneScr[15]{};
            bool boneOk[15]{};
            bool readLiveBones = (liveSkelBudget > 0 && dist <= 55.0f);
            if (readLiveBones) --liveSkelBudget;
            for (int bi = 0; bi < 15; ++bi) {
                Vec3 bp{};
                bool gotBp = false;
                if (readLiveBones && pl.boneTransforms[bi] && Il2Cpp::SafeGetTransformPos(pl.boneTransforms[bi], bp) &&
                    Vec3Dist(bp, liveFoot) < 2.8f) {
                    gotBp = true;
                } else if (pl.boneValid[bi]) {
                    bp = { pl.bonePoints[bi].x + delta.x, pl.bonePoints[bi].y + delta.y, pl.bonePoints[bi].z + delta.z };
                    gotBp = true;
                }
                if (gotBp && Il2Cpp::ProjectWorldToScreenFast(fvp, cam, bp, w, h, boneScr[bi])) {
                    boneOk[bi] = true;
                }
            }
            for (int e = 0; e < 14; ++e) {
                int i0 = kBoneEdges[e][0], i1 = kBoneEdges[e][1];
                if (boneOk[i0] && boneOk[i1]) {
                    bg->AddLine(ImVec2(boneScr[i0].x, boneScr[i0].y), ImVec2(boneScr[i1].x, boneScr[i1].y), skelCol, 1.35f);
                }
            }
        }
        if (g_cfgState.bEspLine) {
            ImVec2 org(w * 0.5f, g_cfgState.lineOrigin == 0 ? 0.0f : (g_cfgState.lineOrigin == 1 ? h : h * 0.5f));
            Vec2 dst = (g_cfgState.lineTarget == 0) ? headS : footS;
            bg->AddLine(org, ImVec2(dst.x, dst.y), rCol, g_cfgState.lineThickness);
        }
        std::string topTag;
        if (g_cfgState.bEspName && !pl.name.empty()) topTag += pl.name;
        if (g_cfgState.bEspPlayerId && pl.entityGen > 0) {
            if (!topTag.empty()) topTag += " ";
            topTag += "[" + std::to_string(pl.entityGen) + "]";
        }
        if (!topTag.empty()) {
            ImVec2 ts = ImGui::CalcTextSize(topTag.c_str());
            float tx = midX - ts.x * 0.5f;
            bg->AddText(ImVec2(tx + 1, y0 - 16.0f), IM_COL32(0, 0, 0, 210), topTag.c_str());
            bg->AddText(ImVec2(tx, y0 - 17.0f), IM_COL32(255, 255, 255, 245), topTag.c_str());
        }
        std::string botTag;
        if (g_cfgState.bEspWeapon && !pl.weaponName.empty()) botTag += pl.weaponName;
        if (g_cfgState.bEspDistance && hasCamPos) {
            if (!botTag.empty()) botTag += " · ";
            botTag += std::to_string(static_cast<int>(std::lround(Vec3Dist(camPos, liveFoot)))) + "m";
        }
        if (!botTag.empty()) {
            ImVec2 bs = ImGui::CalcTextSize(botTag.c_str());
            float bx = midX - bs.x * 0.5f;
            float by = y0 + boxH + (pl.armor > 0 && g_cfgState.bEspArmor ? 7.0f : 3.0f);
            bg->AddText(ImVec2(bx + 1, by + 1), IM_COL32(0, 0, 0, 200), botTag.c_str());
            bg->AddText(ImVec2(bx, by), IM_COL32(215, 225, 245, 240), botTag.c_str());
        }
    }

    if (g_cfgState.bEspCount) {
        char buf[64];
        snprintf(buf, sizeof(buf), "PLAYERS: %d", count);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        float px = (w - ts.x) * 0.5f;
        bg->AddRectFilled(ImVec2(px - 14, 16), ImVec2(px + ts.x + 14, 40), ThemeEngine::GetBg(0.92f), 10.0f);
        bg->AddRect(ImVec2(px - 14, 16), ImVec2(px + ts.x + 14, 40), ThemeEngine::GetAccent(0.55f), 10.0f);
        bg->AddText(ImVec2(px, 21), ThemeEngine::GetText(), buf);
    }
}

inline void TickRuntime() {
    if (!AuthSystem::IsVerifiedFast()) return;
    TCFlyGuard::InstallOnce();
    Il2Cpp::EnsureThreadAttached();
    Il2Cpp::ResolveUnityAndGameMethods();
    Hooks::InstallAllOnce();

    static double s_prevTime = 0.0;
    double now = CACurrentMediaTime();
    float dt = (s_prevTime > 0.0) ? static_cast<float>(std::clamp(now - s_prevTime, 0.001, 0.1)) : 0.016f;
    s_prevTime = now;

    if (g_cfgState.bMasterDisable) {
        if (Il2Cpp::g_icall.Time_set_timeScale) {
            TCFlyGuard::TryRun([&]() { Il2Cpp::g_icall.Time_set_timeScale(1.0f); });
        } else if (Il2Cpp::g_unity.Time_set_timeScale) {
            float one = 1.0f;
            void* a[1] = { &one };
            Il2Cpp::SafeInvoke(Il2Cpp::g_unity.Time_set_timeScale, nullptr, a);
        }
        RestoreAllScaledBones();
        RestoreGameCrosshair();
        if (g_cfgState.bSpectateActive) StopSpectate();
        if (Hooks::g_fcGhostAnchorActive.load(std::memory_order_acquire) || g_fcHasGhostOrigin) {
            ReleaseGhostUndergroundAnchor(Morpeh::GetLocalPedEntity());
        }
        Hooks::g_silentAimHasTarget.store(false);
        Hooks::SilentAimHWBP::Tick();
        return;
    }

    static float s_lastFps = -1.0f;
    if (std::fabs(g_cfgState.targetFPS - s_lastFps) > 0.5f) {
        int32_t fps = static_cast<int32_t>(std::lround(g_cfgState.targetFPS));
        if (Il2Cpp::g_icall.App_set_targetFrameRate) {
            TCFlyGuard::TryRun([&]() { Il2Cpp::g_icall.App_set_targetFrameRate(fps); });
            s_lastFps = g_cfgState.targetFPS;
        } else if (Il2Cpp::g_unity.App_set_targetFrameRate) {
            void* args[1] = { &fps };
            Il2Cpp::SafeInvoke(Il2Cpp::g_unity.App_set_targetFrameRate, nullptr, args);
            s_lastFps = g_cfgState.targetFPS;
        }
    }

    {
        static float s_lastScale = -1.0f;
        float desired = g_cfgState.bSpeedHack ? std::clamp(g_cfgState.speedMultiplier, 0.5f, 8.0f) : 1.0f;
        if (std::fabs(desired - s_lastScale) > 0.01f) {
            if (Il2Cpp::g_icall.Time_set_timeScale) {
                TCFlyGuard::TryRun([&]() { Il2Cpp::g_icall.Time_set_timeScale(desired); });
                s_lastScale = desired;
            } else if (Il2Cpp::g_unity.Time_set_timeScale) {
                void* args[1] = { &desired };
                Il2Cpp::SafeInvoke(Il2Cpp::g_unity.Time_set_timeScale, nullptr, args);
                s_lastScale = desired;
            }
        }
    }

    RefreshWorldSnapshot(dt);
    Hooks::SilentAimHWBP::Tick();
    if (!Morpeh::g_stashes.worldReady) return;
    if (g_cfgState.bKillAroundAura || g_cfgState.bKillAroundAutoRepeat ||
        KillAround::g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed) > 0 ||
        KillAround::g_kaPendingKillElemId.load(std::memory_order_relaxed) > 0 ||
        KillAround::g_kaPendingRestoreGhost.load(std::memory_order_relaxed) ||
        KillAround::g_kaHasActiveGhost || KillAround::g_kaStepActive || KillAround::g_kaCombatActive ||
        (g_menuIsOpenAtomic.load(std::memory_order_relaxed) &&
         g_activeTabAtomic.load(std::memory_order_relaxed) == 0 &&
         g_activeSubTabAtomic.load(std::memory_order_relaxed) == 3)) {
        KillAround::Tick();
    }
    TickCameraFovAndAspect();
    TickRemoveGameCrosshair();
    TickMovementMods(dt);
    TickPlayerScale();
    TickAutomation(dt);
    TickAutoDanse();
    ApplyDayNightTick();
}

} // namespace Features
