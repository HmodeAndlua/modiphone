#pragma once
#include "CoreEngine.h"
#include "Theme.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <time.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// KillAround.h — Unified Group/Vehicle Elimination & Ghost Step Engine (iOS arm64)
// Architecture mirrors kill_around (2).h + mainTOW.cpp:
// - Uses real runtime MeleeAttack enums (heightType=0, direction=2, attackType=0/2)
// - Sends via MetaClientWrapper.Send<T> (MakeGenericMethod + MethodBase.Invoke, TCFlyGuard protected)
//   with captured HostCommandWriter.WriteSend fallback only if AOT generic invocation is unavailable
// - Captures client from MetaClientWrapper.Poll, GlobalContextCore.MetaClientWrapper, SimpleDImple, and ElemSendDisp
// - Hooks PredictComboDamage & MeleeWeaponAttackDatagram.Write for 100000 1-Hit knockout damage
// - Fuses PedView + PedSeat + VehicleDisplacementSync + PedDisplacementSync so groups & vehicle occupants never escape

namespace KillAround {

struct Tunables {
    float   meleeReachM        = 4.2f;     // Direct melee reach before Ghost Step engages
    float   stepHopMaxM        = 11.5f;    // Max meters per stepped displacement breadcrumb
    int     maxHopsPerTick     = 14;       // Upper cap on breadcrumbs per tick (no freeze)
    float   standOffsetM       = 0.45f;    // Attack stand distance from target chest
    int32_t oneHitDamage       = 100000;   // Exact mainTOW.cpp kOneHitDamage
    int32_t comboHitsPerBurst  = 6;        // Exact kill_around (2).h Burst(..., 6)
    int64_t auraTickMs         = 70;       // Fast sequential elimination cadence
    int64_t singleTickMs       = 95;       // Single locked target cadence
    int64_t targetStuckMs      = 950;      // Max dwell before rotating past invulnerable/shielded target
    int64_t retryCooldownMs    = 1400;     // Brief retry cooldown so 5th..Nth players in groups are never starved
    int64_t deadHoldMs         = 12000;    // Cooldown after PedDeadDatagram
    int64_t burstWindowMs      = 4500;     // Kill-Everyone wave duration
    int64_t ghostHoldMs        = 280;      // Hold ghost override in PedDisplacementSyncDatagram.Write
};

inline Tunables g_cfg{};

#pragma pack(push, 1)
struct ElementIdModelVal {
    int32_t id;
    int32_t gen;
};

// dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams.MeleeWeaponAttackRequestDatagram (0x38)
struct MeleeRequestDatagramPacket {
    ElementIdModelVal element;   // 0x00
    ElementIdModelVal target;    // 0x08
    int32_t heightType;          // 0x10 (0)
    int32_t direction;           // 0x14 (2)
    int32_t attackType;          // 0x18 (0 or 2)
    float   trace[3];            // 0x1C
    uint8_t isKo;                // 0x28
    uint8_t pad_29[7];           // 0x29..0x2F
    uint64_t timestamp;          // 0x30
};
static_assert(sizeof(MeleeRequestDatagramPacket) == 0x38, "MeleeRequestDatagramPacket must be 0x38");

// dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams.MeleeWeaponAttackDatagram (0x40)
struct MeleeHitDatagramPacket {
    ElementIdModelVal element;   // 0x00
    ElementIdModelVal target;    // 0x08
    int32_t damage;              // 0x10
    int32_t damageToStamina;     // 0x14
    float   trace[3];            // 0x18
    int32_t heightType;          // 0x24 (0)
    int32_t direction;           // 0x28 (2)
    int32_t attackType;          // 0x2C (2)
    uint8_t isKo;                // 0x30
    uint8_t pad_31[7];           // 0x31..0x37
    uint64_t timestamp;          // 0x38
};
static_assert(sizeof(MeleeHitDatagramPacket) == 0x40, "MeleeHitDatagramPacket must be 0x40");

// dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Datagrams.MeleeFinisherDatagram (0x28)
struct FinisherDatagramPacket {
    ElementIdModelVal performer; // 0x00
    ElementIdModelVal target;    // 0x08
    float hitPos[3];             // 0x10
    float dir[3];                // 0x1C
};
static_assert(sizeof(FinisherDatagramPacket) == 0x28, "FinisherDatagramPacket must be 0x28");

// dev.Scripts.Networking.IBNet.IBNet_Datagrams.ElementSync.PedDisplacementSyncDatagram (0x40)
struct PedDispDatagramPacket {
    ElementIdModelVal remoteId;  // 0x00
    float   pos[3];              // 0x08
    float   rot[4];              // 0x14
    float   vel[3];              // 0x24
    double  remoteTime;          // 0x30
    uint8_t isCloseDistance;     // 0x38
    uint8_t pad_39[7];           // 0x39..0x3F
};
static_assert(sizeof(PedDispDatagramPacket) == 0x40, "PedDispDatagramPacket must be 0x40");

// dev.Scripts.Networking.IBNet.IBNet_Datagrams.ElementSync.VehicleDisplacementSyncDatagram (0x64)
struct VehDispDatagramPacket {
    ElementIdModelVal remoteId;  // 0x00
    float   pos[3];              // 0x08
    float   rot[4];              // 0x14
    float   vel[3];              // 0x24
    float   bodyAccel[3];        // 0x30
    float   angVel[3];           // 0x3C
    float   bodyAngAccel[3];     // 0x48
    uint8_t pad_54[4];           // 0x54..0x57 alignment before double 0x58
    double  remoteTime;          // 0x58
    uint8_t isCloseDistance;     // 0x60
    uint8_t pad_61[3];           // 0x61..0x63
};
static_assert(offsetof(VehDispDatagramPacket, remoteTime) == 0x58, "VehDispDatagramPacket remoteTime offset mismatch");
#pragma pack(pop)

struct TrackedElem {
    int32_t id = 0;
    int32_t gen = 1;
    Vec3    pos{};
    Vec3    vel{};
    int32_t hp = 100;
    int64_t seenMs = 0;
    int64_t deadUntilMs = 0;
    bool    inVehicle = false;
    int32_t vehElemId = 0;
};

struct TrackedVeh {
    int32_t id = 0;
    int32_t gen = 1;
    Vec3    pos{};
    Vec3    vel{};
    int64_t seenMs = 0;
};

struct CandidateTarget {
    int32_t     elemId = 0;
    int32_t     elemGen = 1;
    int32_t     playerId = -1;
    uint32_t    digitalId = 0;
    int64_t     pedEnt = 0;
    std::string name;
    Vec3        pos{};
    Vec3        headPos{};
    Vec3        chestPos{};
    Vec3        vel{};
    float       dist = 99999.0f;
    int32_t     hp = 100;
    bool        inVehicle = false;
    bool        isNpc = false;
    bool        isDead = false;
    int64_t     seenMs = 0;
};

inline int64_t GetMonotonicMs() {
    return static_cast<int64_t>(CACurrentMediaTime() * 1000.0);
}

inline uint64_t ComboTimestamp() {
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t ticks = static_cast<uint64_t>(ts.tv_sec) * 10000000ULL +
                     static_cast<uint64_t>(ts.tv_nsec) / 100ULL +
                     621355968000000000ULL;
    return ticks | (1ULL << 62); // DateTimeKind.Utc
}

inline void CopyRawBytes(void* dst, const void* src, size_t n) {
    if (!dst || !src || n == 0) return;
    std::memcpy(dst, src, n);
}

// ============================================================================
// Runtime State & FeatureModules / Tweak Exports
// ============================================================================
inline std::mutex                              g_kaMutex;
inline std::unordered_map<int32_t, TrackedElem> g_kaElems;
inline std::unordered_map<int32_t, TrackedVeh>  g_kaVehElems;
inline std::unordered_map<int32_t, int32_t>     g_kaVehElemToPedElem;
inline std::unordered_map<int32_t, int64_t>     g_kaSkipUntilMs;
inline void*                                    g_kaCachedWorld = nullptr;

inline std::atomic<int32_t> g_kaLocalElemId{0};
inline std::atomic<int32_t> g_kaLocalElemGen{1};
inline std::atomic<int32_t> g_kaSelectedElemId{0};
inline std::atomic<int32_t> g_kaPendingKillElemId{0};
inline std::atomic<int32_t> g_kaPendingKillElemGen{1};
inline std::atomic<int32_t> g_kaPendingKillPlayerId{0};

inline std::atomic<int32_t> g_kaLastSendError{0};
inline std::atomic<int32_t> g_kaSendOk{0};
inline std::atomic<int32_t> g_kaSendFail{0};
inline std::atomic<int32_t> g_kaLastNearbyCount{0};
inline std::atomic<int32_t> g_kaSrvAcceptedCount{0};
inline std::atomic<int32_t> g_kaDeadHitsCount{0};
inline std::atomic<int32_t> g_kaSrvLastDeadId{0};

inline std::atomic<int32_t> g_kaMeleeOursFlag{0};
inline std::atomic<int32_t> g_kaSendingOwnDgram{0};

inline std::atomic<int64_t> g_kaKillAllBurstUntilMs{0};
inline std::atomic<int64_t> g_kaGhostHoldUntilMs{0};
inline std::atomic<bool>    g_kaPendingKillAllReq{false};
inline std::atomic<bool>    g_kaPendingStopAll{false};
inline std::atomic<bool>    g_kaPendingRestoreGhost{false};
inline std::atomic<bool>    g_kaSeqUiActive{false};
inline std::atomic<int32_t> g_kaKillAllSeqIdx{0};
inline std::atomic<int32_t> g_kaKillAllSeqTotal{0};
inline std::atomic<int32_t> g_kaSeqUiElemId{0};
inline std::atomic<int32_t> g_kaSeqUiHp{100};
inline std::atomic<int32_t> g_kaSeqUiStrikes{0};

inline std::atomic<bool>    g_kaHasMeleeTemplate{false};
inline std::atomic<bool>    g_kaHasPedDispTemplate{false};
inline uint8_t              g_kaMeleeTemplate[64]{};
inline uint8_t              g_kaPedDispTemplate[64]{};

// Hook-captured engine / network pointers
inline std::atomic<void*>   g_kaMetaClientPtr{nullptr};
inline std::atomic<void*>   g_kaWeaponProduceMeleeSys{nullptr};
inline std::atomic<void*>   g_kaElemSendDispSys{nullptr};

inline void*                g_kaClsPedDisp = nullptr;
inline void*                g_kaClsVehDisp = nullptr;
inline void*                g_kaClsPedDead = nullptr;
inline void*                g_kaClsMeleeAtk = nullptr;
inline void*                g_kaClsMeleeReq = nullptr;

// Ghost & sequential focus tracking (main thread only)
inline bool                 g_kaHasActiveGhost = false;
inline bool                 g_kaStepActive = false;
inline bool                 g_kaCombatActive = false;
inline double               g_kaLastRemoteTime = 0.0;
inline Vec3                 g_kaGhostOverridePos{};
inline Vec3                 g_kaGhostOverrideVel{};
inline Vec3                 g_kaGhostOriginPos{};
inline bool                 g_kaHasGhostOrigin = false;
inline float                g_kaCurrentTargetDist = 0.0f;
inline int32_t              g_kaFocusElemId = 0;
inline int64_t              g_kaFocusStartedMs = 0;
inline int32_t              g_kaFocusStrikes = 0;
inline int64_t              g_kaLastAuraTickMs = 0;
inline int64_t              g_kaLastSingleTickMs = 0;

inline std::mutex           g_kaUiMutex;
inline std::string          g_kaUiTargetName;
inline std::vector<CandidateTarget> g_kaUiCandidates;
inline Vec3                 g_kaUiLocalPos{};
inline bool                 g_kaUiHasLocalPos = false;

// ============================================================================
// Reflection Cache (MakeGenericMethod -> MethodBase.Invoke, ZERO per-hit alloc churn)
// ============================================================================
struct GenericSenderSlot {
    bool     tried = false;
    bool     ok = false;
    void*    dgramKlass = nullptr;
    uint32_t specMethodGch = 0; // GCHandle to System.Reflection.MethodInfo (MetaClientWrapper.Send<T>)
};

inline std::unordered_map<std::string, GenericSenderSlot> g_kaGenericSenders;
inline void* g_kaMetaSendGenericMI = nullptr;
inline void* g_kaMakeGenericMI = nullptr;
inline void* g_kaMethodBaseInvokeMI = nullptr;
inline void* g_kaSystemTypeCls = nullptr;
inline void* g_kaSystemObjCls = nullptr;

// Fallback non-generic AOT writer cache (only engaged if MakeGenericMethod fails on iOS AOT)
struct WireFallbackMeta {
    bool     ready = false;
    bool     failed = false;
    void*    dgramKlass = nullptr;
    void*    writeMI = nullptr;
    uint64_t packetId = 0;
    uint8_t  netChannel = 16;
};
inline std::unordered_map<std::string, WireFallbackMeta> g_kaWireFallback;
inline void*    g_kaWriterCls = nullptr;
inline void*    g_kaWriterResetMI = nullptr;
inline void*    g_kaWriterToSegMI = nullptr;
inline void*    g_kaWriteULongMI = nullptr;
inline void*    g_kaHostWriteSendMI = nullptr;
inline void*    g_kaPeerIsAssignedMI = nullptr;
inline void*    g_kaPeerGetMetaPeerMI = nullptr;
inline uint32_t g_kaOurWriterGch = 0;

inline void ResetCachesOnWorldChange() {
    void* w = Morpeh::g_stashes.cachedWorld;
    if (!SafeMem::IsPlausiblePtr(w) || w == g_kaCachedWorld) return;
    g_kaCachedWorld = w;
    g_kaLocalElemId.store(0, std::memory_order_release);
    g_kaLocalElemGen.store(1, std::memory_order_release);
    g_kaSelectedElemId.store(0, std::memory_order_release);
    g_kaPendingKillElemId.store(0, std::memory_order_release);
    g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
    g_kaGhostHoldUntilMs.store(0, std::memory_order_release);
    g_kaPendingKillAllReq.store(false, std::memory_order_release);
    g_kaPendingStopAll.store(false, std::memory_order_release);
    g_kaPendingRestoreGhost.store(false, std::memory_order_release);
    g_kaSeqUiActive.store(false, std::memory_order_release);
    g_kaHasActiveGhost = false;
    g_kaStepActive = false;
    g_kaCombatActive = false;
    g_kaHasGhostOrigin = false;
    g_kaFocusElemId = 0;
    g_kaFocusStrikes = 0;
    g_kaMetaClientPtr.store(nullptr, std::memory_order_release);
    g_kaWeaponProduceMeleeSys.store(nullptr, std::memory_order_release);
    g_kaElemSendDispSys.store(nullptr, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        g_kaElems.clear();
        g_kaVehElems.clear();
        g_kaVehElemToPedElem.clear();
        g_kaSkipUntilMs.clear();
    }
    {
        std::lock_guard<std::mutex> lk(g_kaUiMutex);
        g_kaUiCandidates.clear();
        g_kaUiTargetName.clear();
    }
}

inline void* ResolveMetaClient() {
    void* mc = g_kaMetaClientPtr.load(std::memory_order_acquire);
    if (Il2Cpp::IsUnityObjectSafe(mc)) return mc;

    if (void* sys = g_kaElemSendDispSys.load(std::memory_order_acquire)) {
        if (SafeMem::IsPlausiblePtr(sys)) {
            void* cls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(sys) : nullptr;
            size_t off = Il2Cpp::FieldOffset(cls, "_metaClientWrapper", 0x18);
            void* cand = SafeMem::PtrAt(sys, off);
            if (Il2Cpp::IsUnityObjectSafe(cand)) {
                g_kaMetaClientPtr.store(cand, std::memory_order_release);
                return cand;
            }
        }
    }

    if (void* resolved = Morpeh::EnsureMetaClientResolved()) {
        g_kaMetaClientPtr.store(resolved, std::memory_order_release);
        return resolved;
    }
    return nullptr;
}

inline bool EnsureLocalElementId() {
    ResetCachesOnWorldChange();
    int32_t cur = g_kaLocalElemId.load(std::memory_order_acquire);
    if (cur >= 1) return true;

    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped > 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.elementIdStash)) {
        int32_t pedId = Morpeh::UnpackEntityId(ped);
        ElementIdModelVal eid{};
        if (pedId >= 0) {
            if (void* p = Morpeh::Get(Morpeh::g_stashes.elementIdStash, pedId, sizeof(ElementIdModelVal))) {
                if (SafeMem::ReadVal(p, eid) && eid.id >= 1) {
                    g_kaLocalElemId.store(eid.id, std::memory_order_release);
                    g_kaLocalElemGen.store(eid.gen > 0 ? eid.gen : 1, std::memory_order_release);
                    return true;
                }
            }
        }
    }
    if (void* es = Morpeh::EnsureElementStorageResolved()) {
        void* esCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(es) : nullptr;
        size_t offId = Il2Cpp::FieldOffset(esCls, "_lastLocalElementId", Offsets::g_cfg.offElemStorageLastLocalId);
        ElementIdModelVal eid{};
        if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(es) + offId, eid) && eid.id >= 1) {
            g_kaLocalElemId.store(eid.id, std::memory_order_release);
            g_kaLocalElemGen.store(eid.gen > 0 ? eid.gen : 1, std::memory_order_release);
            return true;
        }
    }
    return false;
}

// ============================================================================
// Wire Transport: Primary = MetaClientWrapper.Send<T> (kill_around (2).h)
//                 Fallback = HostCommandWriter.WriteSend (if AOT strips generic)
// ============================================================================
inline bool SendViaMakeGenericMethod(void* client, const char* ns, const char* dgramName, const void* payload, size_t payloadSize) {
    if (!Il2Cpp::Init() || !Il2Cpp::g_api.object_new || !Il2Cpp::g_api.array_new ||
        !Il2Cpp::g_api.method_get_object || !Il2Cpp::g_api.gchandle_new || !Il2Cpp::g_api.gchandle_get_target) {
        return false;
    }

    std::string key = std::string(ns ? ns : "") + "." + dgramName;
    GenericSenderSlot& slot = g_kaGenericSenders[key];

    if (!slot.tried) {
        slot.tried = true;
        slot.dgramKlass = Il2Cpp::FindClass(ns, dgramName);
        if (!SafeMem::IsPlausiblePtr(slot.dgramKlass)) return false;

        if (!g_kaMetaSendGenericMI) {
            void* mcCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(client) : nullptr;
            if (!mcCls) mcCls = Il2Cpp::FindClass("FlakeFlow", "MetaClientWrapper");
            if (mcCls) g_kaMetaSendGenericMI = Il2Cpp::g_api.class_get_method_from_name(mcCls, "Send", 1);
        }
        if (!g_kaSystemTypeCls)  g_kaSystemTypeCls  = Il2Cpp::FindClass("System", "Type");
        if (!g_kaSystemObjCls)   g_kaSystemObjCls   = Il2Cpp::FindClass("System", "Object");
        if (!g_kaMakeGenericMI)  g_kaMakeGenericMI  = Il2Cpp::ResolveMethod("System.Reflection", "MethodInfo", "MakeGenericMethod", 1);
        if (!g_kaMethodBaseInvokeMI) {
            void* mbCls = Il2Cpp::FindClass("System.Reflection", "MethodBase");
            if (mbCls && Il2Cpp::g_api.class_get_method_from_name) {
                g_kaMethodBaseInvokeMI = Il2Cpp::g_api.class_get_method_from_name(mbCls, "Invoke", 2);
            }
        }

        if (!g_kaMetaSendGenericMI || !g_kaSystemTypeCls || !g_kaSystemObjCls ||
            !g_kaMakeGenericMI || !g_kaMethodBaseInvokeMI) {
            return false;
        }

        void* typeObj = Il2Cpp::GetTypeObject(ns, dgramName);
        void* openObj = Il2Cpp::g_api.method_get_object(g_kaMetaSendGenericMI, nullptr);
        if (!SafeMem::IsPlausiblePtr(typeObj) || !SafeMem::IsPlausiblePtr(openObj)) return false;

        void* specObj = nullptr;
        TCFlyGuard::TryRun([&]() {
            void* typeArr = Il2Cpp::g_api.array_new(g_kaSystemTypeCls, 1);
            if (!SafeMem::IsPlausiblePtr(typeArr)) return;
            void** arrData = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(typeArr) + 0x20);
            arrData[0] = typeObj;
            void* mgArgs[1] = { typeArr };
            void* exc = nullptr;
            specObj = Il2Cpp::g_api.runtime_invoke(g_kaMakeGenericMI, openObj, mgArgs, &exc);
            if (exc) specObj = nullptr;
        });

        if (!SafeMem::IsPlausiblePtr(specObj)) return false;
        slot.specMethodGch = Il2Cpp::g_api.gchandle_new(specObj, false);
        slot.ok = (slot.specMethodGch != 0);
    }

    if (!slot.ok || !slot.dgramKlass || slot.specMethodGch == 0) return false;
    void* specObj = Il2Cpp::g_api.gchandle_get_target(slot.specMethodGch);
    if (!Il2Cpp::IsUnityObjectSafe(specObj)) {
        slot.tried = false;
        slot.ok = false;
        slot.specMethodGch = 0;
        return false;
    }

    bool ok = false;
    g_kaSendingOwnDgram.fetch_add(1, std::memory_order_acq_rel);
    TCFlyGuard::TryRun([&]() {
        void* boxed = Il2Cpp::g_api.object_new(slot.dgramKlass);
        if (!SafeMem::IsPlausiblePtr(boxed)) return;
        if (payload && payloadSize > 0) {
            void* unboxed = Il2Cpp::Unbox(boxed);
            if (!SafeMem::IsPlausiblePtr(unboxed)) return;
            std::memcpy(unboxed, payload, payloadSize);
        }
        void* objArr = Il2Cpp::g_api.array_new(g_kaSystemObjCls, 1);
        if (!SafeMem::IsPlausiblePtr(objArr)) return;
        void** objData = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(objArr) + 0x20);
        objData[0] = boxed;
        void* invArgs[2] = { client, objArr };
        void* exc = nullptr;
        Il2Cpp::g_api.runtime_invoke(g_kaMethodBaseInvokeMI, specObj, invArgs, &exc);
        ok = (exc == nullptr);
    });
    g_kaSendingOwnDgram.fetch_sub(1, std::memory_order_acq_rel);
    return ok;
}

inline void* EnsureFallbackWriter() {
    if (!Il2Cpp::Init() || !Il2Cpp::g_api.object_new || !Il2Cpp::g_api.gchandle_new || !Il2Cpp::g_api.gchandle_get_target)
        return nullptr;
    if (g_kaOurWriterGch != 0) {
        void* ex = Il2Cpp::g_api.gchandle_get_target(g_kaOurWriterGch);
        if (Il2Cpp::IsUnityObjectSafe(ex)) return ex;
        g_kaOurWriterGch = 0;
    }
    if (!g_kaWriterCls) g_kaWriterCls = Il2Cpp::FindClass("dev.Scripts.Networking.IBNet", "IBNet_Writer");
    if (!SafeMem::IsPlausiblePtr(g_kaWriterCls)) return nullptr;
    if (!g_kaWriterResetMI) g_kaWriterResetMI = Il2Cpp::g_api.class_get_method_from_name(g_kaWriterCls, "Reset", 0);
    if (!g_kaWriterToSegMI) g_kaWriterToSegMI = Il2Cpp::g_api.class_get_method_from_name(g_kaWriterCls, "ToArraySegment", 0);
    if (!g_kaWriteULongMI)  g_kaWriteULongMI  = Il2Cpp::ResolveMethod("dev.Scripts.Networking.IBNet", "NumberSerializer", "WriteULong", 2);
    void* ctorMI = Il2Cpp::g_api.class_get_method_from_name(g_kaWriterCls, ".ctor", 1);
    if (!SafeMem::IsPlausiblePtr(ctorMI)) return nullptr;

    void* obj = nullptr;
    TCFlyGuard::TryRun([&]() {
        obj = Il2Cpp::g_api.object_new(g_kaWriterCls);
        if (SafeMem::IsPlausiblePtr(obj)) {
            int32_t cap = 512;
            void* args[1] = { &cap };
            void* exc = nullptr;
            Il2Cpp::g_api.runtime_invoke(ctorMI, obj, args, &exc);
            if (exc) obj = nullptr;
        }
    });
    if (Il2Cpp::IsUnityObjectSafe(obj)) {
        g_kaOurWriterGch = Il2Cpp::g_api.gchandle_new(obj, false);
        return obj;
    }
    return nullptr;
}

inline bool SendViaHostCommandWriterFallback(void* client, const char* ns, const char* dgramName, const void* payload, size_t payloadSize) {
    if (!Il2Cpp::Init()) return false;
    std::string key = std::string(ns ? ns : "") + "." + dgramName;
    WireFallbackMeta& wm = g_kaWireFallback[key];
    if (wm.failed) return false;
    if (!wm.ready) {
        wm.dgramKlass = Il2Cpp::FindClass(ns, dgramName);
        if (!SafeMem::IsPlausiblePtr(wm.dgramKlass)) { wm.failed = true; return false; }
        wm.writeMI = Il2Cpp::g_api.class_get_method_from_name(wm.dgramKlass, "Write", 1);
        if (!SafeMem::IsPlausiblePtr(wm.writeMI)) { wm.failed = true; return false; }
        if (void* fnvMI = Il2Cpp::ResolveMethod("FlakeFlow", "HashUtility", "Fnv1A64", 1)) {
            if (void* sObj = Il2Cpp::g_api.string_new ? Il2Cpp::g_api.string_new(key.c_str()) : nullptr) {
                void* args[1] = { sObj };
                void* exc = nullptr;
                if (void* boxed = Il2Cpp::SafeInvoke(fnvMI, nullptr, args, &exc)) {
                    uint64_t h = 0;
                    if (!exc && SafeMem::ReadVal(Il2Cpp::Unbox(boxed), h)) wm.packetId = h;
                }
            }
        }
        wm.netChannel = (std::strcmp(dgramName, "PedDisplacementSyncDatagram") == 0) ? 32 : 16;
        if (wm.packetId == 0) { wm.failed = true; return false; }
        wm.ready = true;
    }

    void* mcCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(client) : nullptr;
    void* sessionObj = SafeMem::PtrAt(client, Il2Cpp::FieldOffset(mcCls, "_session", 0x30));
    if (!Il2Cpp::IsUnityObjectSafe(sessionObj)) return false;

    void* sessCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(sessionObj) : nullptr;
    void* metaPeerObj = nullptr;
    uint8_t peers[8]{};
    if (!g_kaPeerGetMetaPeerMI) g_kaPeerGetMetaPeerMI = Il2Cpp::ResolveMethod("libflakeflow_csharp", "Peer", "GetMetaPeer", 0);
    if (!g_kaHostWriteSendMI)   g_kaHostWriteSendMI   = Il2Cpp::ResolveMethod("FlakeFlow.Serialization", "HostCommandWriter", "WriteSend", 4);
    if (!g_kaHostWriteSendMI || !g_kaWriterResetMI || !g_kaWriterToSegMI || !g_kaWriteULongMI) return false;

    void* peerList = SafeMem::PtrAt(sessionObj, Il2Cpp::FieldOffset(sessCls, "peers", 0x30));
    if (Il2Cpp::IsUnityObjectSafe(peerList)) {
        void* pListCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(peerList) : nullptr;
        void* items = SafeMem::PtrAt(peerList, 0x10);
        int32_t cnt = 0;
        SafeMem::ReadVal(reinterpret_cast<uint8_t*>(peerList) + 0x18, cnt);
        if (SafeMem::IsPlausiblePtr(items) && cnt > 0) {
            void* firstPeer = SafeMem::PtrAt(items, 0x20);
            if (Il2Cpp::IsUnityObjectSafe(firstPeer) && g_kaPeerGetMetaPeerMI) {
                void* exc = nullptr;
                if (void* mpBox = Il2Cpp::SafeInvoke(g_kaPeerGetMetaPeerMI, firstPeer, nullptr, &exc)) {
                    SafeMem::ReadVal(Il2Cpp::Unbox(mpBox), peers);
                    metaPeerObj = firstPeer;
                }
            }
        }
        (void)pListCls;
    }
    if (!metaPeerObj) return false;

    void* writer = EnsureFallbackWriter();
    if (!Il2Cpp::IsUnityObjectSafe(writer)) return false;

    bool sent = false;
    g_kaSendingOwnDgram.fetch_add(1, std::memory_order_acq_rel);
    TCFlyGuard::TryRun([&]() {
        void* exc = nullptr;
        Il2Cpp::g_api.runtime_invoke(g_kaWriterResetMI, writer, nullptr, &exc);
        if (exc) return;
        uint64_t pktId = wm.packetId;
        void* hdrArgs[2] = { writer, &pktId };
        Il2Cpp::g_api.runtime_invoke(g_kaWriteULongMI, nullptr, hdrArgs, &exc);
        if (exc) return;
        if (payload && payloadSize > 0) {
            uint8_t stackBuf[128]{};
            size_t cp = std::min(payloadSize, sizeof(stackBuf));
            std::memcpy(stackBuf, payload, cp);
            using WriteFn = void (*)(void*, void*, const void*);
            WriteFn rawWrite = reinterpret_cast<WriteFn>(SafeMem::PtrAt(wm.writeMI, 0));
            if (SafeMem::IsPlausiblePtr(reinterpret_cast<void*>(rawWrite))) {
                rawWrite(stackBuf, writer, wm.writeMI);
            } else {
                void* wArgs[1] = { writer };
                Il2Cpp::g_api.runtime_invoke(wm.writeMI, stackBuf, wArgs, &exc);
                if (exc) return;
            }
        }
        void* segBox = Il2Cpp::g_api.runtime_invoke(g_kaWriterToSegMI, writer, nullptr, &exc);
        if (exc || !SafeMem::IsPlausiblePtr(segBox)) return;
        void* segUnb = Il2Cpp::Unbox(segBox);
        if (!SafeMem::IsPlausiblePtr(segUnb)) return;
        uint8_t seg[16]{};
        if (!SafeMem::Read(segUnb, seg, 16)) return;
        uint8_t ch = wm.netChannel;
        uint64_t dummyId = 0;
        void* sendArgs[4] = { &peers, &ch, seg, &dummyId };
        Il2Cpp::g_api.runtime_invoke(g_kaHostWriteSendMI, sessionObj, sendArgs, &exc);
        sent = (exc == nullptr);
    });
    g_kaSendingOwnDgram.fetch_sub(1, std::memory_order_acq_rel);
    return sent;
}

inline bool SendDatagram(const char* ns, const char* dgramName, const void* payload, size_t payloadSize) {
    void* client = ResolveMetaClient();
    if (!Il2Cpp::IsUnityObjectSafe(client)) {
        g_kaLastSendError.store(1, std::memory_order_release);
        g_kaSendFail.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    bool ok = SendViaMakeGenericMethod(client, ns, dgramName, payload, payloadSize);
    if (!ok) {
        ok = SendViaHostCommandWriterFallback(client, ns, dgramName, payload, payloadSize);
    }
    if (ok) {
        g_kaLastSendError.store(0, std::memory_order_release);
        g_kaSendOk.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_kaLastSendError.store(2, std::memory_order_release);
        g_kaSendFail.fetch_add(1, std::memory_order_relaxed);
    }
    return ok;
}

// ============================================================================
// Exact kill_around (2).h Combat Packets + Working Enums
// ============================================================================
inline bool SendModeStart() {
    return SendDatagram("dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Datagrams",
                        "MeleeCombatModeStartDatagram", nullptr, 0);
}

inline bool SendOneHitRaw(int32_t targetId, int32_t targetGen, const float dir[3], int32_t dmg) {
    MeleeHitDatagramPacket m{};
    if (g_kaHasMeleeTemplate.load(std::memory_order_acquire)) {
        CopyRawBytes(&m, g_kaMeleeTemplate, sizeof(m));
    }
    m.element         = { g_kaLocalElemId.load(std::memory_order_relaxed),
                          std::max(1, g_kaLocalElemGen.load(std::memory_order_relaxed)) };
    m.target          = { targetId, targetGen > 0 ? targetGen : 1 };
    m.damage          = dmg;
    m.damageToStamina = 0;
    m.trace[0]        = dir[0];
    m.trace[1]        = dir[1];
    m.trace[2]        = dir[2];
    m.heightType      = 0;
    m.direction       = 2;
    m.attackType      = 2;
    m.isKo            = 1;
    m.timestamp       = ComboTimestamp();
    return SendDatagram("dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams",
                        "MeleeWeaponAttackDatagram", &m, sizeof(m));
}

inline bool SendComboHit(int32_t targetId, int32_t targetGen, bool isKo, uint64_t ts, const float dir[3], int32_t attackType) {
    MeleeRequestDatagramPacket r{};
    r.element    = { g_kaLocalElemId.load(std::memory_order_relaxed),
                     std::max(1, g_kaLocalElemGen.load(std::memory_order_relaxed)) };
    r.target     = { targetId, targetGen > 0 ? targetGen : 1 };
    r.heightType = 0;
    r.direction  = 2;
    r.attackType = attackType;
    r.trace[0]   = dir[0];
    r.trace[1]   = dir[1];
    r.trace[2]   = dir[2];
    r.isKo       = isKo ? 1 : 0;
    r.timestamp  = ts;
    g_kaMeleeOursFlag.store(1, std::memory_order_release);
    bool ok = SendDatagram("dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams",
                           "MeleeWeaponAttackRequestDatagram", &r, sizeof(r));
    g_kaMeleeOursFlag.store(0, std::memory_order_release);
    return ok;
}

inline bool SendFinisher(int32_t targetId, int32_t targetGen, const float hitPos[3], const float dir[3]) {
    FinisherDatagramPacket f{};
    f.performer = { g_kaLocalElemId.load(std::memory_order_relaxed),
                    std::max(1, g_kaLocalElemGen.load(std::memory_order_relaxed)) };
    f.target    = { targetId, targetGen > 0 ? targetGen : 1 };
    f.hitPos[0] = hitPos[0]; f.hitPos[1] = hitPos[1]; f.hitPos[2] = hitPos[2];
    f.dir[0]    = dir[0];    f.dir[1]    = dir[1];    f.dir[2]    = dir[2];
    return SendDatagram("dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Datagrams",
                        "MeleeFinisherDatagram", &f, sizeof(f));
}

inline bool SendDisplacement(const Vec3& pos, const Vec3& vel, double remoteTime) {
    PedDispDatagramPacket d{};
    if (g_kaHasPedDispTemplate.load(std::memory_order_acquire)) {
        CopyRawBytes(&d, g_kaPedDispTemplate, sizeof(d));
    }
    d.remoteId = { g_kaLocalElemId.load(std::memory_order_relaxed),
                   std::max(1, g_kaLocalElemGen.load(std::memory_order_relaxed)) };
    d.pos[0] = pos.x; d.pos[1] = pos.y; d.pos[2] = pos.z;
    if (std::fabs(d.rot[0]) < 1e-4f && std::fabs(d.rot[1]) < 1e-4f &&
        std::fabs(d.rot[2]) < 1e-4f && std::fabs(d.rot[3]) < 1e-4f) {
        d.rot[3] = 1.0f;
    }
    d.vel[0] = vel.x; d.vel[1] = vel.y; d.vel[2] = vel.z;
    d.remoteTime = remoteTime;
    d.isCloseDistance = 1;
    return SendDatagram("dev.Scripts.Networking.IBNet.IBNet_Datagrams.ElementSync",
                        "PedDisplacementSyncDatagram", &d, sizeof(d));
}

// ============================================================================
// Stepped Ghost Server Displacement (safe extended range + moving vehicles)
// ============================================================================
inline bool GetLocalPlayerPos(Vec3& outPos) {
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped <= 0) return false;
    int64_t view = Morpeh::GetViewEntity(ped);
    if (view > 0) {
        if (void* tr = Morpeh::GetEntityTransform(view)) {
            if (Il2Cpp::SafeGetTransformPos(tr, outPos) && IsFiniteVec3(outPos) &&
                (std::fabs(outPos.x) > 0.25f || std::fabs(outPos.z) > 0.25f)) {
                return true;
            }
        }
    }
    int32_t pedId = Morpeh::UnpackEntityId(ped);
    if (pedId >= 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.positionStash)) {
        if (void* p = Morpeh::Get(Morpeh::g_stashes.positionStash, pedId, sizeof(Vec3))) {
            if (SafeMem::ReadVal(p, outPos) && IsFiniteVec3(outPos) &&
                (std::fabs(outPos.x) > 0.25f || std::fabs(outPos.z) > 0.25f)) {
                return true;
            }
        }
    }
    return false;
}

inline Vec3 ComputeAttackStandPos(const Vec3& fromPos, const CandidateTarget& tgt) {
    Vec3 base = tgt.pos;
    // Compensate slightly forward along target velocity so moving targets/vehicles don't slip out of range
    base.x += tgt.vel.x * 0.055f;
    base.y += tgt.vel.y * 0.055f;
    base.z += tgt.vel.z * 0.055f;

    if (tgt.inVehicle) {
        // Seated players sit inside car cabin (+0.30..+0.45m above seat root)
        float dx = base.x - fromPos.x, dz = base.z - fromPos.z;
        float h = std::sqrt(dx * dx + dz * dz);
        float off = 0.35f;
        if (h < 0.05f) return { base.x + off, base.y + 0.35f, base.z };
        return { base.x - (dx / h) * off, base.y + 0.35f, base.z - (dz / h) * off };
    }
    float dx = base.x - fromPos.x, dz = base.z - fromPos.z;
    float h = std::sqrt(dx * dx + dz * dz);
    if (h < 0.05f) return { base.x + g_cfg.standOffsetM, base.y, base.z };
    return { base.x - (dx / h) * g_cfg.standOffsetM, base.y, base.z - (dz / h) * g_cfg.standOffsetM };
}

inline void SteppedGhostMove(const Vec3& fromPos, const Vec3& destPos, const Vec3& destVel) {
    double t = (g_kaLastRemoteTime > 0.0) ? g_kaLastRemoteTime : (GetMonotonicMs() / 1000.0);
    float dx = destPos.x - fromPos.x, dy = destPos.y - fromPos.y, dz = destPos.z - fromPos.z;
    float totalDist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(totalDist)) return;

    int hops = 1;
    if (totalDist > g_cfg.stepHopMaxM) {
        hops = std::min(g_cfg.maxHopsPerTick, static_cast<int>(std::ceil(totalDist / g_cfg.stepHopMaxM)));
    }
    for (int i = 1; i <= hops; ++i) {
        float frac = static_cast<float>(i) / static_cast<float>(hops);
        Vec3 p{
            fromPos.x + dx * frac,
            fromPos.y + dy * frac,
            fromPos.z + dz * frac
        };
        t += 0.024;
        Vec3 stepVel = (i == hops) ? destVel : Vec3{ (dx / std::max(totalDist, 0.1f)) * 9.0f, 0.0f, (dz / std::max(totalDist, 0.1f)) * 9.0f };
        SendDisplacement(p, stepVel, t);
    }
    g_kaLastRemoteTime = t;
}

inline void GhostToTarget(const Vec3& me, const CandidateTarget& tgt, int64_t nowMs) {
    if (!g_kaHasActiveGhost) {
        g_kaGhostOriginPos = me;
        g_kaHasGhostOrigin = true;
    }
    Vec3 start = g_kaHasActiveGhost ? g_kaGhostOverridePos : me;
    Vec3 dest = ComputeAttackStandPos(start, tgt);
    SteppedGhostMove(start, dest, tgt.vel);
    g_kaGhostOverridePos = dest;
    g_kaGhostOverrideVel = tgt.vel;
    g_kaHasActiveGhost = true;
    g_kaStepActive = true;
    g_kaGhostHoldUntilMs.store(nowMs + g_cfg.ghostHoldMs, std::memory_order_release);
}

inline void GhostRestore(const Vec3& me) {
    if (!g_kaHasActiveGhost) return;
    Vec3 dest = g_kaHasGhostOrigin ? g_kaGhostOriginPos : me;
    SteppedGhostMove(g_kaGhostOverridePos, dest, Vec3{0, 0, 0});
    g_kaHasActiveGhost = false;
    g_kaStepActive = false;
    g_kaHasGhostOrigin = false;
    g_kaGhostHoldUntilMs.store(0, std::memory_order_release);
}

// ============================================================================
// Burst Attack Engine (OneHitRaw + Combo Requests + Finisher + Vehicle Cabin)
// ============================================================================
inline int BurstTarget(const CandidateTarget& tgt, const Vec3& standOrMe, int hits) {
    if (tgt.elemId < 1 || g_kaLocalElemId.load(std::memory_order_relaxed) < 1) return 0;
    SendModeStart();
    g_kaCombatActive = true;

    Vec3 aimPoint = tgt.chestPos;
    aimPoint.x += tgt.vel.x * 0.055f;
    aimPoint.y += tgt.vel.y * 0.055f;
    aimPoint.z += tgt.vel.z * 0.055f;
    if (tgt.inVehicle && IsFiniteVec3(tgt.headPos)) {
        // Aim at upper chest / head inside cabin
        aimPoint.x = (aimPoint.x + tgt.headPos.x) * 0.5f + tgt.vel.x * 0.055f;
        aimPoint.y = (aimPoint.y + tgt.headPos.y) * 0.5f + tgt.vel.y * 0.055f;
        aimPoint.z = (aimPoint.z + tgt.headPos.z) * 0.5f + tgt.vel.z * 0.055f;
    }

    Vec3 origin = { standOrMe.x, standOrMe.y + 1.0f, standOrMe.z };
    float dx = aimPoint.x - origin.x;
    float dy = aimPoint.y - origin.y;
    float dz = aimPoint.z - origin.z;
    float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    float dir[3] = { 0.0f, 0.0f, 1.0f };
    if (d > 0.005f) {
        dir[0] = dx / d; dir[1] = dy / d; dir[2] = dz / d;
    }

    int sent = 0;
    if (g_cfgState.bKillAroundOneHit) {
        if (SendOneHitRaw(tgt.elemId, tgt.elemGen, dir, g_cfg.oneHitDamage)) ++sent;
    } else {
        if (SendOneHitRaw(tgt.elemId, tgt.elemGen, dir, 1000)) ++sent;
    }

    uint64_t ts = ComboTimestamp();
    for (int i = 0; i < hits; ++i) {
        if (SendComboHit(tgt.elemId, tgt.elemGen, true, ts + static_cast<uint64_t>(i), dir, (i % 3 == 2) ? 2 : 0)) {
            ++sent;
        }
    }
    float hitPos[3] = { aimPoint.x, aimPoint.y, aimPoint.z };
    if (SendFinisher(tgt.elemId, tgt.elemGen, hitPos, dir)) ++sent;
    return sent;
}

// ============================================================================
// Dual-Fused Live Roster (PedView + PedSeat + VehDisp + PedDisp)
// ============================================================================
inline bool IsDeadByElemOrView(int32_t elemId, int32_t viewId, int32_t hp, int64_t nowMs) {
    if (viewId >= 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.pedViewDeadStash)) {
        if (Morpeh::HasNative(Morpeh::g_stashes.pedViewDeadStash, viewId)) return true;
    }
    if (hp <= 0) return true;
    if (elemId >= 1) {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        auto it = g_kaElems.find(elemId);
        if (it != g_kaElems.end() && it->second.deadUntilMs > nowMs) return true;
    }
    return false;
}

inline std::vector<CandidateTarget> GatherCandidates(const Vec3& me, float radius, bool includeNpcs, bool includeVeh, int64_t nowMs, bool keepDead = false) {
    std::vector<CandidateTarget> out;
    int32_t myElemId = g_kaLocalElemId.load(std::memory_order_relaxed);
    int64_t localPedEnt = Morpeh::GetLocalPedEntity();
    int32_t localPedId = (localPedEnt > 0) ? Morpeh::UnpackEntityId(localPedEnt) : -1;

    std::unordered_set<int32_t> seenElemIds;
    std::unordered_map<int32_t, TrackedElem> netElems;
    std::unordered_map<int32_t, TrackedVeh>  netVehs;
    std::unordered_map<int32_t, int32_t>     vehToPedMap;
    {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        netElems = g_kaElems;
        netVehs  = g_kaVehElems;
        vehToPedMap = g_kaVehElemToPedElem;
    }

    // Snapshot Morpeh streamed players (already resolves PedView + PedSeat + Health + Nickname + Vehicle)
    std::vector<ESPPlayer> renderCopy;
    std::vector<ServerPlayerEntry> serverCopy;
    {
        std::lock_guard<std::mutex> lk(::g_espMutex);
        renderCopy = ::g_renderPlayers;
        serverCopy = ::g_allServerPlayers;
    }

    auto readElemIdForPed = [](int32_t pedId, int32_t& outId, int32_t& outGen) -> bool {
        if (pedId < 0 || !SafeMem::IsPlausiblePtr(Morpeh::g_stashes.elementIdStash)) return false;
        ElementIdModelVal eid{};
        if (void* p = Morpeh::Get(Morpeh::g_stashes.elementIdStash, pedId, sizeof(ElementIdModelVal))) {
            if (SafeMem::ReadVal(p, eid) && eid.id >= 1 && eid.id < 2000000) {
                outId = eid.id;
                outGen = (eid.gen > 0 && eid.gen < 100000) ? eid.gen : 1;
                return true;
            }
        }
        return false;
    };

    auto readVehElemForPed = [&](int32_t pedId) -> int32_t {
        if (pedId < 0 || !SafeMem::IsPlausiblePtr(Morpeh::g_stashes.pedSeatStash) ||
            !SafeMem::IsPlausiblePtr(Morpeh::g_stashes.elementIdStash)) return 0;
        if (void* sp = Morpeh::Get(Morpeh::g_stashes.pedSeatStash, pedId, 16)) {
            int64_t vehEnt = 0;
            if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(sp) + 0x8, vehEnt) && vehEnt > 0) {
                int32_t vehId = Morpeh::UnpackEntityId(vehEnt);
                if (vehId >= 0) {
                    ElementIdModelVal vid{};
                    if (void* vp = Morpeh::Get(Morpeh::g_stashes.elementIdStash, vehId, sizeof(ElementIdModelVal))) {
                        if (SafeMem::ReadVal(vp, vid) && vid.id >= 1) return vid.id;
                    }
                }
            }
        }
        return 0;
    };

    // 1) Primary Source: Live Morpeh ECS PedView roster (catches stationary campers & car occupants!)
    for (const auto& pl : renderCopy) {
        if (pl.entityId < 0 || pl.entityId == localPedId) continue;
        bool isNpc = (pl.playerId < 0);
        if (isNpc && !includeNpcs) continue;
        if (pl.inVehicle && !includeVeh) continue;

        int32_t elemId = 0, elemGen = 1;
        if (!readElemIdForPed(pl.entityId, elemId, elemGen)) continue;
        if (elemId == myElemId) continue;

        CandidateTarget c{};
        c.elemId    = elemId;
        c.elemGen   = elemGen;
        c.playerId  = pl.playerId;
        c.digitalId = pl.entityGen;
        c.pedEnt    = pl.pedEntityVal;
        c.name      = pl.name.empty() ? ("#" + std::to_string(elemId)) : pl.name;
        c.pos       = pl.worldPos;
        c.chestPos  = pl.chestPos;
        c.headPos   = pl.headPos;
        c.vel       = pl.velocity;
        c.hp        = pl.health;
        c.inVehicle = pl.inVehicle;
        c.isNpc     = isNpc;
        c.seenMs    = nowMs;

        // If target is inside a vehicle, correlate their vehicle's fresh network displacement & velocity
        int32_t vehElemId = pl.inVehicle ? readVehElemForPed(pl.entityId) : 0;
        if (vehElemId >= 1) {
            vehToPedMap[vehElemId] = elemId;
            auto vIt = netVehs.find(vehElemId);
            if (vIt != netVehs.end() && (nowMs - vIt->second.seenMs) <= 1500) {
                if (IsFiniteVec3(vIt->second.vel)) c.vel = vIt->second.vel;
                if (vIt->second.seenMs >= nowMs - 200 && IsFiniteVec3(vIt->second.pos)) {
                    // Use live View bone if valid, otherwise fresh vehicle position
                    if (std::fabs(c.pos.x) < 0.1f && std::fabs(c.pos.z) < 0.1f) {
                        c.pos = vIt->second.pos;
                        c.chestPos = { c.pos.x, c.pos.y + 0.85f, c.pos.z };
                        c.headPos  = { c.pos.x, c.pos.y + 1.25f, c.pos.z };
                    }
                }
            }
        } else {
            // Merge high-frequency network PedDisp position if fresher than frame scan
            auto nIt = netElems.find(elemId);
            if (nIt != netElems.end()) {
                if (nIt->second.gen > 0) c.elemGen = nIt->second.gen;
                if (IsFiniteVec3(nIt->second.vel)) c.vel = nIt->second.vel;
            }
        }

        // Cache into g_kaElems so UI & single-target lookup always have fresh coords
        {
            std::lock_guard<std::mutex> lk(g_kaMutex);
            TrackedElem& te = g_kaElems[elemId];
            te.id = elemId;
            te.gen = c.elemGen;
            te.pos = c.pos;
            te.vel = c.vel;
            te.hp = c.hp;
            te.seenMs = nowMs;
            te.inVehicle = c.inVehicle;
            te.vehElemId = vehElemId;
            if (vehElemId >= 1) g_kaVehElemToPedElem[vehElemId] = elemId;
        }

        c.isDead = IsDeadByElemOrView(elemId, pl.viewId, c.hp, nowMs);
        if (c.isDead && !keepDead) {
            seenElemIds.insert(elemId);
            continue;
        }

        float dx = c.pos.x - me.x, dy = c.pos.y - me.y, dz = c.pos.z - me.z;
        c.dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        seenElemIds.insert(elemId);
        if (c.dist <= radius) out.push_back(std::move(c));
    }

    // 2) Secondary ECS Source: All streamed controlsEntity / g_allServerPlayers (catches vehicle passengers missing PedView transform)
    for (const auto& sp : serverCopy) {
        if (sp.pedId < 0 || sp.pedId == localPedId) continue;
        int32_t elemId = 0, elemGen = 1;
        if (!readElemIdForPed(sp.pedId, elemId, elemGen)) continue;
        if (elemId == myElemId || seenElemIds.find(elemId) != seenElemIds.end()) continue;

        int32_t vehElemId = readVehElemForPed(sp.pedId);
        bool inVeh = (vehElemId >= 1);
        if (inVeh && !includeVeh) { seenElemIds.insert(elemId); continue; }

        Vec3 p = sp.worldPos;
        Vec3 v{};
        if (inVeh) {
            vehToPedMap[vehElemId] = elemId;
            auto vIt = netVehs.find(vehElemId);
            if (vIt != netVehs.end() && IsFiniteVec3(vIt->second.pos)) {
                p = vIt->second.pos;
                v = vIt->second.vel;
            }
        }
        if (std::fabs(p.x) < 0.1f && std::fabs(p.z) < 0.1f && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.positionStash)) {
            if (void* pp = Morpeh::Get(Morpeh::g_stashes.positionStash, sp.pedId, sizeof(Vec3))) {
                SafeMem::ReadVal(pp, p);
            }
        }
        if (!IsFiniteVec3(p) || (std::fabs(p.x) < 0.1f && std::fabs(p.z) < 0.1f)) continue;

        bool dead = IsDeadByElemOrView(elemId, -1, sp.health, nowMs);
        if (dead && !keepDead) { seenElemIds.insert(elemId); continue; }

        float dx = p.x - me.x, dy = p.y - me.y, dz = p.z - me.z;
        float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        seenElemIds.insert(elemId);
        if (d > radius) continue;

        CandidateTarget c{};
        c.elemId    = elemId;
        c.elemGen   = elemGen;
        c.playerId  = sp.playerId;
        c.digitalId = sp.digitalId;
        c.pedEnt    = sp.pedEntityVal;
        c.name      = sp.name.empty() ? ("#" + std::to_string(elemId)) : sp.name;
        c.pos       = p;
        c.chestPos  = { p.x, p.y + 0.90f, p.z };
        c.headPos   = { p.x, p.y + 1.45f, p.z };
        c.vel       = v;
        c.dist      = d;
        c.hp        = sp.health;
        c.inVehicle = inVeh;
        c.isDead    = dead;
        c.seenMs    = nowMs;
        out.push_back(std::move(c));
    }

    // 3) Tertiary Source: Raw Network displacement entries not matched in ECS
    for (const auto& kv : netElems) {
        int32_t elemId = kv.first;
        const TrackedElem& te = kv.second;
        if (elemId < 1 || elemId == myElemId) continue;
        if (seenElemIds.find(elemId) != seenElemIds.end()) continue;
        if ((nowMs - te.seenMs) > 30000) continue; // Tolerates idle stationary campers!
        if (te.inVehicle && !includeVeh) continue;

        bool dead = (te.deadUntilMs > nowMs) || (te.hp <= 0);
        if (dead && !keepDead) continue;

        Vec3 p = te.pos;
        Vec3 v = te.vel;
        if (te.inVehicle && te.vehElemId >= 1) {
            auto vIt = netVehs.find(te.vehElemId);
            if (vIt != netVehs.end() && vIt->second.seenMs >= te.seenMs && IsFiniteVec3(vIt->second.pos)) {
                p = vIt->second.pos;
                v = vIt->second.vel;
            }
        }
        float dx = p.x - me.x, dy = p.y - me.y, dz = p.z - me.z;
        float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d > radius) continue;

        CandidateTarget c{};
        c.elemId    = elemId;
        c.elemGen   = te.gen > 0 ? te.gen : 1;
        c.name      = "#" + std::to_string(elemId);
        c.pos       = p;
        c.chestPos  = { p.x, p.y + 0.90f, p.z };
        c.headPos   = { p.x, p.y + 1.50f, p.z };
        c.vel       = v;
        c.dist      = d;
        c.hp        = te.hp;
        c.inVehicle = te.inVehicle;
        c.isDead    = dead;
        c.seenMs    = te.seenMs;
        out.push_back(std::move(c));
    }

    std::sort(out.begin(), out.end(), [](const CandidateTarget& a, const CandidateTarget& b) {
        return a.dist < b.dist;
    });
    return out;
}

// ============================================================================
// Public Controls Called by UI
// ============================================================================
inline void RequestKillTarget(int32_t playerId, int32_t elemId, int32_t elemGen, const std::string& name) {
    g_kaSelectedElemId.store(elemId, std::memory_order_release);
    g_kaPendingKillPlayerId.store(playerId, std::memory_order_release);
    g_kaPendingKillElemId.store(elemId, std::memory_order_release);
    g_kaPendingKillElemGen.store(elemGen > 0 ? elemGen : 1, std::memory_order_release);
    std::lock_guard<std::mutex> lk(g_kaUiMutex);
    g_kaUiTargetName = name;
}

inline void RequestKillAllAroundOnce() {
    g_kaPendingKillAllReq.store(true, std::memory_order_release);
}

inline void StopAll() {
    g_cfgState.bKillAroundAura = false;
    g_cfgState.bKillAroundAutoRepeat = false;
    g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
    g_kaPendingKillElemId.store(0, std::memory_order_release);
    g_kaPendingKillAllReq.store(false, std::memory_order_release);
    g_kaPendingStopAll.store(true, std::memory_order_release);
    g_kaPendingRestoreGhost.store(true, std::memory_order_release);
    g_kaSeqUiActive.store(false, std::memory_order_release);
    g_kaFocusElemId = 0;
    g_kaFocusStrikes = 0;
    g_kaSkipUntilMs.clear();
}

// ============================================================================
// Hook Feeders (Invoked from FeatureModules.h)
// ============================================================================
inline const void* NormalizeDatagramPtr(const void* rawPkt, void*& cachedKlass, const char* ns, const char* cls) {
    if (!SafeMem::IsPlausiblePtr(rawPkt)) return nullptr;
    if (!cachedKlass && Il2Cpp::Init()) cachedKlass = Il2Cpp::FindClass(ns, cls);
    if (cachedKlass && SafeMem::PtrAt(rawPkt, 0) == cachedKlass) {
        return reinterpret_cast<const uint8_t*>(rawPkt) + 0x10;
    }
    return rawPkt;
}

inline void* NormalizeDatagramPtrMut(void* rawPkt, void*& cachedKlass, const char* ns, const char* cls) {
    return const_cast<void*>(NormalizeDatagramPtr(rawPkt, cachedKlass, ns, cls));
}

inline void OnMetaClientPoll(void* self) {
    if (SafeMem::IsPlausiblePtr(self)) {
        g_kaMetaClientPtr.store(self, std::memory_order_release);
    }
}

inline int32_t PredictComboDamageOverride(int32_t /*attackType*/, bool isStaminaDamage, bool& outOverride) {
    outOverride = false;
    if (isStaminaDamage || g_cfgState.bMasterDisable || !g_cfgState.bKillAroundOneHit) return 0;
    int64_t nowMs = GetMonotonicMs();
    bool active = g_cfgState.bKillAroundAura || g_cfgState.bKillAroundAutoRepeat ||
                  (nowMs < g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed)) ||
                  (g_kaPendingKillElemId.load(std::memory_order_relaxed) > 0) ||
                  g_kaFocusElemId > 0 || g_kaCombatActive;
    if (active) {
        outOverride = true;
        return g_cfg.oneHitDamage;
    }
    return 0;
}

inline void OnPedDisplacementRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_kaClsPedDisp, Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    PedDispDatagramPacket pkt{};
    if (!SafeMem::ReadVal(ptr, pkt)) return;
    if (pkt.remoteId.id < 1 || pkt.remoteId.id > 2000000) return;
    Vec3 p{ pkt.pos[0], pkt.pos[1], pkt.pos[2] };
    if (!IsFiniteVec3(p) || (std::fabs(p.x) < 0.1f && std::fabs(p.z) < 0.1f)) return;
    Vec3 v{ pkt.vel[0], pkt.vel[1], pkt.vel[2] };
    if (!IsFiniteVec3(v)) v = {};
    int64_t nowMs = GetMonotonicMs();

    std::lock_guard<std::mutex> lk(g_kaMutex);
    TrackedElem& te = g_kaElems[pkt.remoteId.id];
    te.id = pkt.remoteId.id;
    if (pkt.remoteId.gen > 0 && pkt.remoteId.gen < 100000) te.gen = pkt.remoteId.gen;
    te.pos = p;
    te.vel = v;
    te.seenMs = nowMs;
    te.inVehicle = false;
}

inline void OnVehicleDisplacementRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_kaClsVehDisp, Offsets::g_cfg.nsIBNetElemSync.c_str(), "VehicleDisplacementSyncDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    VehDispDatagramPacket pkt{};
    if (!SafeMem::ReadVal(ptr, pkt)) return;
    if (pkt.remoteId.id < 1 || pkt.remoteId.id > 2000000) return;
    Vec3 p{ pkt.pos[0], pkt.pos[1], pkt.pos[2] };
    if (!IsFiniteVec3(p) || (std::fabs(p.x) < 0.1f && std::fabs(p.z) < 0.1f)) return;
    Vec3 v{ pkt.vel[0], pkt.vel[1], pkt.vel[2] };
    if (!IsFiniteVec3(v)) v = {};
    int64_t nowMs = GetMonotonicMs();

    std::lock_guard<std::mutex> lk(g_kaMutex);
    TrackedVeh& tv = g_kaVehElems[pkt.remoteId.id];
    tv.id = pkt.remoteId.id;
    if (pkt.remoteId.gen > 0 && pkt.remoteId.gen < 100000) tv.gen = pkt.remoteId.gen;
    tv.pos = p;
    tv.vel = v;
    tv.seenMs = nowMs;

    auto mIt = g_kaVehElemToPedElem.find(pkt.remoteId.id);
    if (mIt != g_kaVehElemToPedElem.end() && mIt->second >= 1) {
        TrackedElem& te = g_kaElems[mIt->second];
        te.id = mIt->second;
        te.pos = p;
        te.vel = v;
        te.seenMs = nowMs;
        te.inVehicle = true;
        te.vehElemId = pkt.remoteId.id;
    }
}

inline void OnPedDisplacementWrite(void* rawPkt, void* /*writer*/ = nullptr) {
    if (g_kaSendingOwnDgram.load(std::memory_order_acquire) > 0) return;
    void* ptr = NormalizeDatagramPtrMut(rawPkt, g_kaClsPedDisp, Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;

    PedDispDatagramPacket pkt{};
    if (!SafeMem::ReadVal(ptr, pkt)) return;
    if (pkt.remoteId.id >= 1 && pkt.remoteId.id < 2000000) {
        g_kaLocalElemId.store(pkt.remoteId.id, std::memory_order_release);
        if (pkt.remoteId.gen > 0 && pkt.remoteId.gen < 100000) {
            g_kaLocalElemGen.store(pkt.remoteId.gen, std::memory_order_release);
        }
        if (!g_kaHasActiveGhost) {
            CopyRawBytes(g_kaPedDispTemplate, &pkt, sizeof(g_kaPedDispTemplate));
            g_kaHasPedDispTemplate.store(true, std::memory_order_release);
        }
        if (std::isfinite(pkt.remoteTime) && pkt.remoteTime > g_kaLastRemoteTime) {
            g_kaLastRemoteTime = pkt.remoteTime;
        }
    }

    if (g_cfgState.bKillAroundGhostStep && g_kaHasActiveGhost &&
        GetMonotonicMs() <= g_kaGhostHoldUntilMs.load(std::memory_order_acquire) &&
        (std::fabs(g_kaGhostOverridePos.x) > 0.1f || std::fabs(g_kaGhostOverridePos.z) > 0.1f)) {
        g_kaLastRemoteTime += 0.016;
        pkt.pos[0] = g_kaGhostOverridePos.x;
        pkt.pos[1] = g_kaGhostOverridePos.y;
        pkt.pos[2] = g_kaGhostOverridePos.z;
        pkt.vel[0] = g_kaGhostOverrideVel.x;
        pkt.vel[1] = g_kaGhostOverrideVel.y;
        pkt.vel[2] = g_kaGhostOverrideVel.z;
        pkt.remoteTime = g_kaLastRemoteTime;
        pkt.isCloseDistance = 1;
        SafeMem::Write(ptr, &pkt, sizeof(pkt));
    }
}

inline void OnPedDeadRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_kaClsPedDead, Offsets::g_cfg.nsIBNetPedSync.c_str(), "PedDeadDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    ElementIdModelVal eid{};
    if (!SafeMem::ReadVal(ptr, eid) || eid.id < 1 || eid.id > 2000000) return;

    int64_t nowMs = GetMonotonicMs();
    g_kaDeadHitsCount.fetch_add(1, std::memory_order_relaxed);
    g_kaSrvLastDeadId.store(eid.id, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lk(g_kaMutex);
    TrackedElem& te = g_kaElems[eid.id];
    te.id = eid.id;
    if (eid.gen > 0 && eid.gen < 100000) te.gen = eid.gen;
    te.hp = 0;
    te.deadUntilMs = nowMs + g_cfg.deadHoldMs;
    te.seenMs = nowMs;
}

inline void OnMeleeAttackRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_kaClsMeleeAtk, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    MeleeHitDatagramPacket pkt{};
    if (!SafeMem::ReadVal(ptr, pkt)) return;
    if (pkt.element.id < 1 || pkt.element.id > 2000000) return;

    if (g_kaSendingOwnDgram.load(std::memory_order_acquire) == 0) {
        CopyRawBytes(g_kaMeleeTemplate, &pkt, sizeof(g_kaMeleeTemplate));
        g_kaHasMeleeTemplate.store(true, std::memory_order_release);
    }

    int32_t myId = g_kaLocalElemId.load(std::memory_order_relaxed);
    if (myId >= 1 && pkt.element.id == myId) {
        g_kaSrvAcceptedCount.fetch_add(1, std::memory_order_relaxed);
    }
    if (pkt.target.id >= 1 && pkt.target.id < 2000000 && pkt.damage > 0) {
        int64_t nowMs = GetMonotonicMs();
        std::lock_guard<std::mutex> lk(g_kaMutex);
        TrackedElem& te = g_kaElems[pkt.target.id];
        te.id = pkt.target.id;
        te.hp = std::max(0, te.hp - pkt.damage);
        te.seenMs = nowMs;
    }
}

inline void OnMeleeAttackWrite(void* rawPkt, void* /*writer*/ = nullptr) {
    if (g_kaSendingOwnDgram.load(std::memory_order_acquire) > 0) return;
    void* ptr = NormalizeDatagramPtrMut(rawPkt, g_kaClsMeleeAtk, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    MeleeHitDatagramPacket pkt{};
    if (!SafeMem::ReadVal(ptr, pkt)) return;
    if (pkt.element.id < 1 || pkt.element.id > 2000000) return;

    g_kaLocalElemId.store(pkt.element.id, std::memory_order_release);
    if (pkt.element.gen > 0 && pkt.element.gen < 100000) {
        g_kaLocalElemGen.store(pkt.element.gen, std::memory_order_release);
    }
    CopyRawBytes(g_kaMeleeTemplate, &pkt, sizeof(g_kaMeleeTemplate));
    g_kaHasMeleeTemplate.store(true, std::memory_order_release);

    if (!g_cfgState.bMasterDisable && g_cfgState.bKillAroundOneHit &&
        (g_cfgState.bKillAroundAura || g_cfgState.bKillAroundAutoRepeat ||
         GetMonotonicMs() < g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed) ||
         g_kaFocusElemId > 0)) {
        pkt.damage = g_cfg.oneHitDamage;
        pkt.damageToStamina = 0;
        pkt.isKo = 1;
        SafeMem::Write(ptr, &pkt, sizeof(pkt));
    }
}

inline void OnMeleeRequestWrite(void* rawPkt, void* /*writer*/ = nullptr) {
    if (g_kaSendingOwnDgram.load(std::memory_order_acquire) > 0) return;
    void* ptr = NormalizeDatagramPtrMut(rawPkt, g_kaClsMeleeReq, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackRequestDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    MeleeRequestDatagramPacket req{};
    if (!SafeMem::ReadVal(ptr, req)) return;
    if (req.element.id >= 1 && req.element.id < 2000000) {
        g_kaLocalElemId.store(req.element.id, std::memory_order_release);
        if (req.element.gen > 0 && req.element.gen < 100000) {
            g_kaLocalElemGen.store(req.element.gen, std::memory_order_release);
        }
    }
    if (!g_cfgState.bMasterDisable && g_cfgState.bKillAroundOneHit &&
        g_kaMeleeOursFlag.load(std::memory_order_relaxed) == 0 &&
        (g_cfgState.bKillAroundAura || g_cfgState.bKillAroundAutoRepeat ||
         GetMonotonicMs() < g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed) ||
         g_kaFocusElemId > 0)) {
        uint8_t ko = 1;
        SafeMem::Write(reinterpret_cast<uint8_t*>(ptr) + 0x28, &ko, sizeof(ko));
    }
}

// ============================================================================
// Main-Thread Tick (Zero IL2CPP calls from Render Thread!)
// ============================================================================
inline void Tick() {
    ResetCachesOnWorldChange();
    int64_t nowMs = GetMonotonicMs();

    // Pending UI requests
    if (g_kaPendingStopAll.exchange(false, std::memory_order_acq_rel)) {
        Vec3 me{};
        if (g_kaHasActiveGhost && GetLocalPlayerPos(me)) {
            GhostRestore(me);
        }
        g_kaFocusElemId = 0;
        g_kaFocusStrikes = 0;
        g_kaCombatActive = false;
        g_kaSeqUiActive.store(false, std::memory_order_release);
        return;
    }

    if (g_kaPendingKillAllReq.exchange(false, std::memory_order_acq_rel)) {
        g_kaSkipUntilMs.clear();
        g_kaFocusElemId = 0;
        g_kaFocusStrikes = 0;
        g_kaKillAllBurstUntilMs.store(nowMs + g_cfg.burstWindowMs, std::memory_order_release);
    }

    // Throttled ElementId learn even when idle
    {
        static int64_t s_lastIdTryMs = 0;
        if (g_kaLocalElemId.load(std::memory_order_relaxed) < 1 && (nowMs - s_lastIdTryMs) >= 400) {
            s_lastIdTryMs = nowMs;
            EnsureLocalElementId();
        }
    }

    Vec3 me{};
    bool hasMe = GetLocalPlayerPos(me);
    float radius = std::clamp(g_cfgState.killAroundRadius, 5.0f, 150.0f);

    // Refresh UI candidate snapshot at ~8Hz so menu draws 100% lock-light without engine calls
    {
        static int64_t s_lastUiSnapMs = 0;
        if (hasMe && (nowMs - s_lastUiSnapMs) >= 120) {
            s_lastUiSnapMs = nowMs;
            auto uiList = GatherCandidates(me, radius, g_cfgState.bKillAroundIncludeNpcs,
                                           g_cfgState.bKillAroundIncludeVehicles, nowMs, false);
            g_kaLastNearbyCount.store(static_cast<int32_t>(uiList.size()), std::memory_order_release);
            std::lock_guard<std::mutex> lk(g_kaUiMutex);
            g_kaUiCandidates = std::move(uiList);
            g_kaUiLocalPos = me;
            g_kaUiHasLocalPos = true;
        }
    }

    if (g_kaPendingRestoreGhost.exchange(false, std::memory_order_acq_rel) && hasMe) {
        GhostRestore(me);
    }

    bool burstActive = (nowMs < g_kaKillAllBurstUntilMs.load(std::memory_order_acquire));
    bool auraActive  = g_cfgState.bKillAroundAura || g_cfgState.bKillAroundAutoRepeat || burstActive;
    int32_t lockElemId = g_kaPendingKillElemId.load(std::memory_order_acquire);

    if (!auraActive && lockElemId <= 0) {
        if (g_kaHasActiveGhost && hasMe) GhostRestore(me);
        g_kaCombatActive = false;
        g_kaSeqUiActive.store(false, std::memory_order_release);
        g_kaFocusElemId = 0;
        return;
    }

    if (!EnsureLocalElementId() || !hasMe) return;

    // Resolve Client proactively
    ResolveMetaClient();

    // ------------------------------------------------------------------------
    // Mode 1: Confirmed Sequential Elimination (Aura / Kill All Wave)
    // Focuses current victim until confirmed dead (or stuck timeout -> brief retry cooldown)
    // so every single player in a 5+ player group & in cars is eliminated cleanly!
    // ------------------------------------------------------------------------
    if (auraActive) {
        auto cands = GatherCandidates(me, radius, g_cfgState.bKillAroundIncludeNpcs,
                                      g_cfgState.bKillAroundIncludeVehicles, nowMs, false);
        g_kaLastNearbyCount.store(static_cast<int32_t>(cands.size()), std::memory_order_release);
        g_kaKillAllSeqTotal.store(static_cast<int32_t>(cands.size()), std::memory_order_release);

        if (cands.empty()) {
            g_kaFocusElemId = 0;
            g_kaFocusStrikes = 0;
            g_kaSeqUiActive.store(false, std::memory_order_release);
            if (burstActive) {
                g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
            }
            if (g_kaHasActiveGhost) GhostRestore(me);
            return;
        }

        // Extend burst window slightly while live targets remain until wave clears cleanly
        if (burstActive && cands.size() > 0 && (g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed) - nowMs) < 500) {
            g_kaKillAllBurstUntilMs.store(nowMs + 1200, std::memory_order_release);
        }

        if ((nowMs - g_kaLastAuraTickMs) >= g_cfg.auraTickMs) {
            g_kaLastAuraTickMs = nowMs;

            // Find current focus target or select nearest ready target (skipping temporary cooldown)
            const CandidateTarget* activeT = nullptr;
            int activeIdx = 0;

            if (g_kaFocusElemId >= 1) {
                for (int i = 0; i < static_cast<int>(cands.size()); ++i) {
                    if (cands[i].elemId == g_kaFocusElemId) {
                        bool stuck = (nowMs - g_kaFocusStartedMs) > g_cfg.targetStuckMs;
                        if (stuck) {
                            g_kaSkipUntilMs[g_kaFocusElemId] = nowMs + g_cfg.retryCooldownMs;
                            g_kaFocusElemId = 0;
                            g_kaFocusStrikes = 0;
                        } else {
                            activeT = &cands[i];
                            activeIdx = i;
                        }
                        break;
                    }
                }
            }

            if (!activeT) {
                for (int i = 0; i < static_cast<int>(cands.size()); ++i) {
                    auto sIt = g_kaSkipUntilMs.find(cands[i].elemId);
                    if (sIt != g_kaSkipUntilMs.end() && sIt->second > nowMs) continue;
                    activeT = &cands[i];
                    activeIdx = i;
                    break;
                }
                if (!activeT) {
                    // Everyone remaining is on brief retry cooldown -> pick nearest immediately
                    activeT = &cands[0];
                    activeIdx = 0;
                }
                g_kaFocusElemId = activeT->elemId;
                g_kaFocusStartedMs = nowMs;
                g_kaFocusStrikes = 0;
            }

            Vec3 attackOrigin = me;
            if (activeT->dist > g_cfg.meleeReachM || activeT->inVehicle) {
                if (g_cfgState.bKillAroundGhostStep) {
                    GhostToTarget(me, *activeT, nowMs);
                    attackOrigin = g_kaGhostOverridePos;
                } else if (g_kaHasActiveGhost) {
                    GhostRestore(me);
                }
            } else if (g_kaHasActiveGhost) {
                GhostRestore(me);
            }

            if (activeT->dist <= g_cfg.meleeReachM || g_cfgState.bKillAroundGhostStep) {
                int sent = BurstTarget(*activeT, attackOrigin, g_cfg.comboHitsPerBurst);
                g_kaFocusStrikes += sent;
            }

            g_kaCurrentTargetDist = activeT->dist;
            g_kaKillAllSeqIdx.store(activeIdx + 1, std::memory_order_release);
            g_kaSeqUiElemId.store(activeT->elemId, std::memory_order_release);
            g_kaSeqUiHp.store(activeT->hp, std::memory_order_release);
            g_kaSeqUiStrikes.store(g_kaFocusStrikes, std::memory_order_release);
            g_kaSeqUiActive.store(true, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lk(g_kaUiMutex);
                g_kaUiTargetName = activeT->name;
            }
        }
        return;
    }

    // ------------------------------------------------------------------------
    // Mode 2: Single Target Kill Lock
    // ------------------------------------------------------------------------
    if (lockElemId > 0) {
        if ((nowMs - g_kaLastSingleTickMs) < g_cfg.singleTickMs) return;
        g_kaLastSingleTickMs = nowMs;

        auto all = GatherCandidates(me, std::max(radius, 180.0f), true, true, nowMs, true);
        const CandidateTarget* chosen = nullptr;
        for (const auto& c : all) {
            if (c.elemId == lockElemId) { chosen = &c; break; }
        }
        if (!chosen || chosen->isDead) {
            g_kaPendingKillElemId.store(0, std::memory_order_release);
            g_kaSeqUiActive.store(false, std::memory_order_release);
            if (g_kaHasActiveGhost) GhostRestore(me);
            return;
        }

        Vec3 attackOrigin = me;
        if (chosen->dist > g_cfg.meleeReachM || chosen->inVehicle) {
            if (g_cfgState.bKillAroundGhostStep) {
                GhostToTarget(me, *chosen, nowMs);
                attackOrigin = g_kaGhostOverridePos;
            } else {
                if (g_kaHasActiveGhost) GhostRestore(me);
                return;
            }
        } else if (g_kaHasActiveGhost) {
            GhostRestore(me);
        }

        BurstTarget(*chosen, attackOrigin, g_cfg.comboHitsPerBurst);
        g_kaCurrentTargetDist = chosen->dist;
        g_kaSeqUiElemId.store(chosen->elemId, std::memory_order_release);
        g_kaSeqUiHp.store(chosen->hp, std::memory_order_release);
        g_kaSeqUiStrikes.fetch_add(1, std::memory_order_relaxed);
        g_kaSeqUiActive.store(true, std::memory_order_release);
    }
}

// ============================================================================
// UI Panel (Render Thread Only — Reads Cached Snapshot, Zero IL2CPP Calls)
// ============================================================================
inline void DrawPanel(float availW) {
    using Lang::T;
    int32_t myId  = g_kaLocalElemId.load(std::memory_order_relaxed);
    int32_t myGen = g_kaLocalElemGen.load(std::memory_order_relaxed);
    bool hasMc    = Il2Cpp::IsUnityObjectSafe(g_kaMetaClientPtr.load(std::memory_order_relaxed)) ||
                    Il2Cpp::IsUnityObjectSafe(Morpeh::g_stashes.realMetaClient);
    int32_t nearN = g_kaLastNearbyCount.load(std::memory_order_relaxed);
    int32_t srvOk = g_kaSrvAcceptedCount.load(std::memory_order_relaxed);
    int32_t deadN = g_kaDeadHitsCount.load(std::memory_order_relaxed);
    int32_t err   = g_kaLastSendError.load(std::memory_order_relaxed);
    bool burstActive = (GetMonotonicMs() < g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed));
    bool seqActive   = g_kaSeqUiActive.load(std::memory_order_relaxed);

    ImGui::TextColored(
        (myId > 0 && hasMc && err == 0) ? ImVec4(0.28f, 0.98f, 0.65f, 1.0f) : ImVec4(1.0f, 0.76f, 0.30f, 1.0f),
        "Elem=%d/%d | Net=%s | Near=%d | Hits=%d | Kills=%d | E=%d",
        myId, myGen, hasMc ? "OK" : "Wait", nearN, srvOk, deadN, err);

    if (seqActive) {
        int32_t tot = g_kaKillAllSeqTotal.load(std::memory_order_relaxed);
        int32_t idx = g_kaKillAllSeqIdx.load(std::memory_order_relaxed);
        int32_t eid = g_kaSeqUiElemId.load(std::memory_order_relaxed);
        int32_t hp  = g_kaSeqUiHp.load(std::memory_order_relaxed);
        int32_t stk = g_kaSeqUiStrikes.load(std::memory_order_relaxed);
        std::string tName;
        {
            std::lock_guard<std::mutex> lk(g_kaUiMutex);
            tName = g_kaUiTargetName;
        }
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.44f, 1.0f),
                           "%s: %s (%d/%d) | HP=%d | Hits=%d | %.1fm",
                           T("Locking", "الهدف الحالي"),
                           !tName.empty() ? tName.c_str() : ("#" + std::to_string(eid)).c_str(),
                           tot > 0 ? idx : 1, tot > 0 ? tot : 1, hp, stk, g_kaCurrentTargetDist);
    }

    char killAllBtn[160];
    snprintf(killAllBtn, sizeof(killAllBtn), "%s (%d)##ka_all",
             T("KILL EVERYONE AROUND ME", "قتل الجميع حولي الآن"), nearN);
    if (ThemeEngine::NeonActionButton(killAllBtn, std::max(180.0f, availW - 8.0f), 46.0f, (g_cfgState.bKillAroundAura || burstActive))) {
        if (burstActive) {
            StopAll();
        } else {
            RequestKillAllAroundOnce();
        }
    }

    ImGui::Spacing();
    ThemeEngine::ProToggle(T("Continuous Kill Aura (Group Wipe)", "أورا القتل التلقائي (تصفية المجموعات)"), &g_cfgState.bKillAroundAura);
    ThemeEngine::ProToggle(T("Ghost Reach (Extended Range)", "المدى البعيد بالانتقال الخفي (غوست)"), &g_cfgState.bKillAroundGhostStep);
    ThemeEngine::ProToggle(T("Kill Players in Vehicles", "قتل اللاعبين داخل السيارات"), &g_cfgState.bKillAroundIncludeVehicles);
    ThemeEngine::ProToggle(T("One-Hit Knockout (100K)", "ضربة قاضية فورية"), &g_cfgState.bKillAroundOneHit);
    ThemeEngine::ProToggle(T("Include NPCs / Bots", "تضمين البوتات"), &g_cfgState.bKillAroundIncludeNpcs);
    ThemeEngine::ProSlider(T("Kill Radius", "مدى القتل"), &g_cfgState.killAroundRadius, 5.0f, 150.0f, "%.0f m");

    if (g_cfgState.bKillAroundAura || burstActive || seqActive || g_kaPendingKillElemId.load(std::memory_order_relaxed) > 0) {
        if (ThemeEngine::NeonActionButton(T("STOP & RESTORE POSITION", "إيقاف واستعادة الموقع"), std::max(180.0f, availW - 8.0f), 34.0f, false)) {
            StopAll();
        }
    }

    ImGui::Spacing();
    std::vector<CandidateTarget> listSnap;
    {
        std::lock_guard<std::mutex> lk(g_kaUiMutex);
        listSnap = g_kaUiCandidates;
    }
    if (!listSnap.empty()) {
        ImGui::TextDisabled("%s (%zu)", T("Nearby Targets", "الأهداف القريبة"), listSnap.size());
        ImGui::BeginChild("##ka_list", ImVec2(0, 165.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        int32_t selId = g_kaSelectedElemId.load(std::memory_order_relaxed);
        for (const auto& c : listSnap) {
            ImGui::PushID(c.elemId);
            char row[160];
            snprintf(row, sizeof(row), "%s %s [HP:%d | %.0fm]##ka_t",
                     c.name.c_str(),
                     c.inVehicle ? "[CAR]" : "",
                     c.hp, c.dist);
            bool selected = (selId == c.elemId && g_kaPendingKillElemId.load(std::memory_order_relaxed) == c.elemId);
            if (ImGui::Selectable(row, selected)) {
                RequestKillTarget(c.playerId, c.elemId, c.elemGen, c.name);
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    ImGui::Spacing();
}

} // namespace KillAround
