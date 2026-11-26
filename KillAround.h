#pragma once
// KillAround.h — iOS 1.4.1 Sequential Close + Remote Fist Combat Engine
// Verified against dump1.4.1.cpp (MetaClientWrapper, WeaponProduceAttackMeleeSystem,
// MeleeCombatAttackRequestSendSystem, ElementSendDisplacementSystem, PedDead, Health).
#include <time.h>
#include <unordered_set>
#include "imgui.h"
#include "CoreEngine.h"
#include "Lang.h"
#include "Theme.h"

namespace KillAround {

struct Tunables {
    float   directMeleeRangeM      = 4.2f;
    float   ghostStepChunkM        = 12.0f;
    float   ghostHitOffsetM        = 0.50f;
    float   stepApproachOffsetM    = 0.85f;
    int32_t strikeIntervalMs       = 85;    // >=1 game tick so MeleeCombatAttackRequestSendSystem drains each request
    int32_t ghostSettleLeadMs      = 45;    // brief sync lead after >directRange ghost jump so strike #0 lands
    int32_t maxStrikesPerTarget    = 14;
    int32_t maxMsPerTarget         = 1600;
    int32_t skipUnkillableMs       = 1400;
    int32_t singleFollowIntervalMs = 110;
    int32_t killAllBurstMs         = 18000; // hard safety ceiling for clearing large groups sequentially
    int32_t autoFinishWindowMs     = 2800;
    int32_t deadCooldownMs         = 5500;
    int32_t pedStaleMs             = 10000;
    int32_t ghostHoldMs            = 260;
    int32_t stepHoldMs             = 260;
    int32_t maxGhostHopsPerStrike  = 6;
};

inline Tunables g_cfg{};

#pragma pack(push, 1)
// dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams.MeleeWeaponAttackRequestDatagram (0x38 = 56 bytes, dump1.4.1.cpp L101744)
// Note: System.DateTime has a single 8-byte ulong field (_dateData @ 0x0, L892139), so unboxed size is 0x38.
struct MeleeWeaponAttackRequestDatagram {
    int32_t  attacker_id;   // 0x00 EntityIdModel ElementId.Id
    int32_t  attacker_gen;  // 0x04 EntityIdModel ElementId.Gen
    int32_t  target_id;     // 0x08 EntityIdModel TargetId.Id
    int32_t  target_gen;    // 0x0C EntityIdModel TargetId.Gen
    int32_t  hit_height;    // 0x10 MeleeAttackHeightType
    int32_t  hit_dir;       // 0x14 MeleeAttackDirection
    int32_t  attack_type;   // 0x18 MeleeAttackType
    float    trace_x;       // 0x1C Vector3.x
    float    trace_y;       // 0x20 Vector3.y
    float    trace_z;       // 0x24 Vector3.z
    uint8_t  is_ko;         // 0x28 bool IsKnockout
    uint8_t  pad_29[7];     // 0x29..0x2F alignment padding
    uint64_t timestamp;     // 0x30 System.DateTime (_dateData)
};
static_assert(sizeof(MeleeWeaponAttackRequestDatagram) == 0x38, "MeleeWeaponAttackRequestDatagram must be 0x38");

// dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams.MeleeWeaponAttackDatagram (0x40 = 64 bytes, dump1.4.1.cpp L101718)
struct MeleeWeaponAttackDatagramPacket {
    int32_t  attacker_id;       // 0x00
    int32_t  attacker_gen;      // 0x04
    int32_t  target_id;         // 0x08
    int32_t  target_gen;        // 0x0C
    int32_t  damage;            // 0x10
    int32_t  damage_to_stamina; // 0x14
    float    trace_x;           // 0x18
    float    trace_y;           // 0x1C
    float    trace_z;           // 0x20
    int32_t  attack_height;     // 0x24
    int32_t  attack_dir;        // 0x28
    int32_t  attack_type;       // 0x2C
    uint8_t  is_ko;             // 0x30
    uint8_t  pad_31[7];         // 0x31..0x37
    uint64_t timestamp;         // 0x38 System.DateTime
};
static_assert(sizeof(MeleeWeaponAttackDatagramPacket) == 0x40, "MeleeWeaponAttackDatagramPacket must be 0x40");

// dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Datagrams.MeleeFinisherDatagram (0x28 = 40 bytes, dump1.4.1.cpp L426271)
struct MeleeFinisherDatagram {
    int32_t attacker_id;
    int32_t attacker_gen;
    int32_t target_id;
    int32_t target_gen;
    float   pos_x;
    float   pos_y;
    float   pos_z;
    float   dir_x;
    float   dir_y;
    float   dir_z;
};
static_assert(sizeof(MeleeFinisherDatagram) == 0x28, "MeleeFinisherDatagram must be 0x28");

// dev.Scripts.Networking.IBNet.IBNet_Datagrams.ElementSync.PedDisplacementSyncDatagram (0x40 = 64 bytes, dump1.4.1.cpp L47804)
struct PedDisplacementSyncDatagramPacket {
    int32_t remote_id;
    int32_t remote_gen;
    float   pos_x;
    float   pos_y;
    float   pos_z;
    float   rot_x;
    float   rot_y;
    float   rot_z;
    float   rot_w;
    float   vel_x;
    float   vel_y;
    float   vel_z;
    double  remote_time;
    uint8_t is_close_distance;
    uint8_t pad_39[7];
};
static_assert(sizeof(PedDisplacementSyncDatagramPacket) == 0x40, "PedDisplacementSyncDatagramPacket must be 0x40");

// dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Components.MeleeAttackRequestSend (0x30 = 48 bytes, dump1.4.1.cpp L426425)
struct MeleeAttackRequestSendComponent {
    int64_t target;            // 0x00 Scellecs.Morpeh.Entity
    int32_t attackHeightType;  // 0x08
    int32_t attackDirection;   // 0x0C
    int32_t attackType;        // 0x10
    float   dir_x;             // 0x14 Vector3 Direction
    float   dir_y;             // 0x18
    float   dir_z;             // 0x1C
    float   hit_x;             // 0x20 Vector3 HitPosition
    float   hit_y;             // 0x24
    float   hit_z;             // 0x28
    uint8_t isKnockout;        // 0x2C
    uint8_t pad_2d[3];         // 0x2D..0x2F
};
static_assert(sizeof(MeleeAttackRequestSendComponent) == 0x30, "MeleeAttackRequestSendComponent must be 0x30");

struct ElementIdModelVal {
    int32_t id;
    int32_t gen;
};
#pragma pack(pop)

// 1.4.1 VERIFIED MeleeCombat enums (dump1.4.1.cpp L426188..426229)
namespace MeleeEnum {
    inline constexpr int32_t kAtkLight    = 100925952; // 0x06040200
    inline constexpr int32_t kAtkHeavy    = 394242;    // 0x00060402
    inline constexpr int32_t kAtkFinisher = 33555972;  // 0x02000604
    inline constexpr int32_t kAtkParry    = 67239942;  // 0x04020006

    inline constexpr int32_t kDirLeft     = 100925952; // 0x06040200
    inline constexpr int32_t kDirRight    = 394242;    // 0x00060402
    inline constexpr int32_t kDirFront    = 33555972;  // 0x02000604
    inline constexpr int32_t kDirBack     = 67239942;  // 0x04020006

    inline constexpr int32_t kHeightHigh   = 262656;   // 0x00040200
    inline constexpr int32_t kHeightMiddle = 33555458; // 0x02000402
    inline constexpr int32_t kHeightLow    = 67239940; // 0x04020004
}

struct TrackedElement {
    int32_t id = 0;
    int32_t gen = 1;
    Vec3    pos{};
    int32_t hp = 100;
    int64_t lastSeenMs = 0;
    int64_t lastDeadMs = 0;
    int64_t deadUntilMs = 0;
    bool    isPed = false;
    bool    isLocal = false;
};

struct NamedTargetCandidate {
    int32_t     playerId = 0;
    int32_t     elemId = 0;
    int32_t     elemGen = 1;
    uint32_t    digitalId = 0;
    int64_t     pedEnt = 0;
    std::string name;
    Vec3        pos{};
    float       dist = 99999.0f;
    int32_t     hp = 100;
    bool        hasPos = false;
    bool        isNpc = false;
    bool        isDead = false;
};

struct SentHitRecord {
    int32_t targetId = 0;
    int64_t sentMs = 0;
};

struct SequentialActiveTarget {
    bool        active = false;
    int32_t     playerId = 0;
    int32_t     elemId = 0;
    int32_t     elemGen = 1;
    int64_t     pedEnt = 0;
    std::string name;
    int64_t     startedMs = 0;
    int64_t     lastStrikeMs = 0;
    int64_t     readyToStrikeAtMs = 0;
    int32_t     strikesSent = 0;
    bool        ghostAligned = false;
};

inline int64_t GetMonotonicMs() {
    return static_cast<int64_t>(CACurrentMediaTime() * 1000.0);
}

inline double GetMonotonicSec() {
    return CACurrentMediaTime();
}

inline void CopyRawBytes(void* dst, const void* src, size_t count) {
    uint8_t* d = reinterpret_cast<uint8_t*>(dst);
    const uint8_t* s = reinterpret_cast<const uint8_t*>(src);
    for (size_t i = 0; i < count; ++i) d[i] = s[i];
}

// Runtime state
inline std::mutex                                  g_kaMutex;
inline std::unordered_map<int32_t, TrackedElement> g_kaTracked;
inline void*                                       g_kaCachedWorld = nullptr;

inline std::atomic<int32_t>  g_kaLocalElemId{0};
inline std::atomic<int32_t>  g_kaLocalElemGen{1};
inline std::atomic<int32_t>  g_kaSelectedPlayerId{0};
inline std::atomic<int32_t>  g_kaPendingKillPlayerId{0};
inline std::atomic<int32_t>  g_kaPendingKillElemId{0};
inline std::atomic<int32_t>  g_kaPendingKillElemGen{1};
inline std::atomic<int32_t>  g_kaLastSendError{0};
inline std::atomic<int32_t>  g_kaLastNearbyCount{0};
inline std::atomic<int32_t>  g_kaAuraSweepCount{0};
inline std::atomic<int32_t>  g_kaMeleeOursFlag{0};
inline std::atomic<int32_t>  g_kaKillAllSeqIdx{0};
inline std::atomic<int64_t>  g_kaKillAllSeqMs{0};
inline std::atomic<int32_t>  g_kaKillAllSeqTotal{0};
inline std::atomic<int64_t>  g_kaAutoFinishUntilMs{0};
inline std::atomic<int64_t>  g_kaKillAllBurstUntilMs{0};
inline std::atomic<int64_t>  g_kaGhostHoldUntilMs{0};
inline std::atomic<bool>     g_kaPendingRestoreGhost{false};
inline std::atomic<bool>     g_kaPendingKillAllReq{false};
inline std::atomic<bool>     g_kaPendingStopAll{false};
inline std::atomic<bool>     g_kaHasPedDispTemplate{false};
inline std::atomic<bool>     g_kaHasMeleeTemplate{false};
inline std::atomic<bool>     g_kaSeqUiActive{false};
inline std::atomic<int32_t>  g_kaSeqUiStrikes{0};
inline std::atomic<int32_t>  g_kaSeqUiElemId{0};
inline std::mutex            g_kaUiNameMutex;
inline std::string           g_kaUiTargetName;
inline std::string           g_kaReqTargetName;

// Live game system instances captured cleanly via OnUpdate hooks in FeatureModules.h
inline std::atomic<void*>    g_kaWeaponProduceMeleeSys{nullptr};
inline std::atomic<void*>    g_kaElemSendDispSys{nullptr};
inline std::atomic<void*>    g_kaMeleeReqSendSys{nullptr};

// Cached per-world / per-runtime IL2CPP handles & stashes
inline void*                 g_kaMeleeReqSendStash = nullptr;
inline void*                 g_kaPedDeadTagStash = nullptr;
inline void*                 g_kaPedViewMeleeKoStash = nullptr;
inline void*                 g_kaSendAttackRequestMI = nullptr;
inline void*                 g_kaSendPedDispMI = nullptr;
inline void*                 g_kaGetServerTimeMI = nullptr;
inline void*                 g_kaClsPedDisp = nullptr;
inline void*                 g_kaClsPedDead = nullptr;
inline void*                 g_kaClsMeleeAtk = nullptr;
inline void*                 g_kaClsMeleeReq = nullptr;

inline uint8_t     g_kaPedDispTemplate[64]{};
inline uint8_t     g_kaMeleeTemplate[64]{};
inline Vec3        g_kaGhostOverridePos{};
inline Vec3        g_kaLastGhostSentPos{};
inline bool        g_kaHasActiveGhost = false;
inline double      g_kaLastRemoteTime = 0.0;
inline bool        g_kaStepActive = false;
inline int64_t     g_kaStepReturnAtMs = 0;
inline Vec3        g_kaStepOriginPos{};
inline bool        g_kaCombatActive = false;
inline int64_t     g_kaCombatFinishAtMs = 0;
inline int32_t     g_kaCurrentTargetId = 0;
inline int32_t     g_kaCurrentTargetGen = 1;
inline float       g_kaCurrentTargetDist = 0.0f;
inline int32_t     g_kaTargetSentHits = 0;
inline int32_t     g_kaSrvAcceptedCount = 0;
inline int32_t     g_kaTargetStartSrvAcc = 0;
inline int32_t     g_kaSrvLastDeadId = 0;
inline int64_t     g_kaSrvLastDeadMs = 0;
inline int32_t     g_kaKilledThisWave = 0;
inline std::string g_kaSelectedName;
inline char        g_kaSearchBuf[64]{};
inline SentHitRecord g_kaRecentHits[32]{};
inline int32_t     g_kaRecentWriteIdx = 0;

inline std::vector<NamedTargetCandidate> g_kaWaveQueue;
inline SequentialActiveTarget            g_kaActiveSeq{};
inline int64_t                           g_kaWaveSessionStartMs = 0;

struct DatagramSenderEntry {
    const char* name = nullptr;
    uint32_t    methodGch = 0;
    void*       dgramKlass = nullptr;
    int64_t     lastFailMs = 0;
};

inline DatagramSenderEntry g_kaSenders[16]{};
inline int                 g_kaSenderCount = 0;
inline void*               g_kaSysObjKlass = nullptr;
inline void*               g_kaMethodBaseInvoke = nullptr;

inline void* ResolveMetaClientWrapper() {
    Morpeh::RefreshStoragesIfNeeded();
    return Morpeh::EnsureMetaClientResolved();
}

inline uint64_t GenerateComboTimestamp() {
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t ticksPerSec = 10000000ULL;
    uint64_t ticks = static_cast<uint64_t>(ts.tv_sec) * ticksPerSec +
                     static_cast<uint64_t>(ts.tv_nsec) / 100ULL +
                     621355968000000000ULL;
    return ticks | (1ULL << 62);
}

inline uint64_t GetValidServerTimestamp() {
    void* mc = ResolveMetaClientWrapper();
    if (SafeMem::IsPlausiblePtr(mc) && Il2Cpp::Init()) {
        if (!g_kaGetServerTimeMI) {
            const auto& cfg = Offsets::g_cfg;
            g_kaGetServerTimeMI = Il2Cpp::ResolveMethod(cfg.nsFlakeFlow.c_str(), cfg.clsMetaClientWrapper.c_str(), "GetServerTime", 0);
        }
        if (SafeMem::IsPlausiblePtr(g_kaGetServerTimeMI)) {
            void* exc = nullptr;
            void* boxed = Il2Cpp::SafeInvoke(g_kaGetServerTimeMI, mc, nullptr, &exc);
            if (!exc && SafeMem::IsPlausiblePtr(boxed)) {
                if (void* raw = Il2Cpp::Unbox(boxed)) {
                    uint64_t dtVal = 0;
                    if (SafeMem::ReadVal(raw, dtVal) && dtVal != 0) return dtVal;
                }
            }
        }
    }
    return GenerateComboTimestamp();
}

inline void EnsureWorldFreshness() {
    void* cur = Morpeh::g_stashes.cachedWorld;
    if (!SafeMem::IsPlausiblePtr(cur)) return;
    if (g_kaCachedWorld == cur) return;
    g_kaCachedWorld = cur;
    g_kaLocalElemId.store(0, std::memory_order_release);
    g_kaLocalElemGen.store(1, std::memory_order_release);
    g_kaGhostHoldUntilMs.store(0, std::memory_order_release);
    g_kaHasActiveGhost = false;
    g_kaPendingRestoreGhost.store(false, std::memory_order_release);
    g_kaPendingKillAllReq.store(false, std::memory_order_release);
    g_kaPendingStopAll.store(false, std::memory_order_release);
    g_kaSeqUiActive.store(false, std::memory_order_release);
    g_kaSeqUiStrikes.store(0, std::memory_order_release);
    g_kaSeqUiElemId.store(0, std::memory_order_release);
    g_kaStepActive = false;
    g_kaStepReturnAtMs = 0;
    g_kaCombatActive = false;
    g_kaCombatFinishAtMs = 0;
    g_kaCurrentTargetId = 0;
    g_kaAutoFinishUntilMs.store(0, std::memory_order_release);
    g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
    g_kaKillAllSeqIdx.store(0, std::memory_order_release);
    g_kaKillAllSeqMs.store(0, std::memory_order_release);
    g_kaKillAllSeqTotal.store(0, std::memory_order_release);
    g_kaKilledThisWave = 0;
    g_kaWaveQueue.clear();
    g_kaActiveSeq = SequentialActiveTarget{};
    g_kaWeaponProduceMeleeSys.store(nullptr, std::memory_order_release);
    g_kaElemSendDispSys.store(nullptr, std::memory_order_release);
    g_kaMeleeReqSendSys.store(nullptr, std::memory_order_release);
    g_kaMeleeReqSendStash = nullptr;
    g_kaPedDeadTagStash = nullptr;
    g_kaPedViewMeleeKoStash = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        g_kaTracked.clear();
    }
}

inline void ResolveSystemsFromFeatureRunner() {
    if (!Il2Cpp::Init() || !Il2Cpp::IsUnityNativeAlive(Morpeh::g_stashes.realInitializer)) return;
    void* frCls = Il2Cpp::FindClass("dev.Scripts.ECS.Morpeh", "FeatureRunner");
    if (!SafeMem::IsPlausiblePtr(frCls)) return;

    void* initObj = Morpeh::g_stashes.realInitializer;
    void* initCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(initObj) : nullptr;
    size_t offRunner = Il2Cpp::FieldOffset(initCls, "_featureRunner", 0x30);
    void* runner = SafeMem::PtrAt(initObj, offRunner);
    if (!SafeMem::IsPlausiblePtr(runner) || SafeMem::PtrAt(runner, 0) != frCls) return;

    const auto& cfg = Offsets::g_cfg;
    if (!SafeMem::IsPlausiblePtr(g_kaWeaponProduceMeleeSys.load(std::memory_order_relaxed))) {
        if (void* wfCls = Il2Cpp::FindClass("dev.Scripts.ECS.Morpeh.Features.WeaponFeature", "WeaponFeature")) {
            size_t offFeat = Il2Cpp::FieldOffset(frCls, "_weaponFeature", 0x3128);
            size_t offSys  = Il2Cpp::StructFieldOffset(wfCls, "_weaponProduceAttackMeleeSystem", 0x80);
            void* sys = SafeMem::PtrAt(runner, offFeat + offSys);
            void* sysCls = Il2Cpp::FindClass(cfg.nsWeaponSys.c_str(), "WeaponProduceAttackMeleeSystem");
            if (SafeMem::IsPlausiblePtr(sys) && (!sysCls || SafeMem::PtrAt(sys, 0) == sysCls)) {
                g_kaWeaponProduceMeleeSys.store(sys, std::memory_order_release);
            }
        }
    }
    if (!SafeMem::IsPlausiblePtr(g_kaElemSendDispSys.load(std::memory_order_relaxed))) {
        if (void* efCls = Il2Cpp::FindClass("dev.Scripts.ECS.Morpeh.Features.ElementsFeature", "ElementsFeature")) {
            size_t offFeat = Il2Cpp::FieldOffset(frCls, "_elementsFeature", 0x1E60);
            size_t offSys  = Il2Cpp::StructFieldOffset(efCls, "_elementSendDisplacementSystem", 0xA8);
            void* sys = SafeMem::PtrAt(runner, offFeat + offSys);
            void* sysCls = Il2Cpp::FindClass(cfg.nsElementsSys.c_str(), "ElementSendDisplacementSystem");
            if (SafeMem::IsPlausiblePtr(sys) && (!sysCls || SafeMem::PtrAt(sys, 0) == sysCls)) {
                g_kaElemSendDispSys.store(sys, std::memory_order_release);
            }
        }
    }
    if (!SafeMem::IsPlausiblePtr(g_kaMeleeReqSendSys.load(std::memory_order_relaxed))) {
        if (void* mfCls = Il2Cpp::FindClass("dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature", "MeleeCombatFeature")) {
            size_t offFeat = Il2Cpp::FieldOffset(frCls, "_meleeCombatFeature", 0x3228);
            size_t offSys  = Il2Cpp::StructFieldOffset(mfCls, "_meleeCombatRequestSendSystem", 0xC0);
            void* sys = SafeMem::PtrAt(runner, offFeat + offSys);
            void* sysCls = Il2Cpp::FindClass("dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Systems", "MeleeCombatAttackRequestSendSystem");
            if (SafeMem::IsPlausiblePtr(sys) && (!sysCls || SafeMem::PtrAt(sys, 0) == sysCls)) {
                g_kaMeleeReqSendSys.store(sys, std::memory_order_release);
            }
        }
    }
}

inline void EnsureMeleeStashesResolved() {
    ResolveSystemsFromFeatureRunner();
    void* w = Morpeh::g_stashes.cachedWorld;
    if (!SafeMem::IsPlausiblePtr(w)) return;
    const auto& cfg = Offsets::g_cfg;
    if (!g_kaMeleeReqSendStash) {
        g_kaMeleeReqSendStash = Morpeh::FetchComponentStash(w, cfg.nsMeleeCombatComps.c_str(), "MeleeAttackRequestSend");
    }
    if (!g_kaMeleeReqSendStash) {
        if (void* meleeSys = g_kaWeaponProduceMeleeSys.load(std::memory_order_acquire)) {
            void* cls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(meleeSys) : nullptr;
            void* st = SafeMem::PtrAt(meleeSys, Il2Cpp::FieldOffset(cls, "_meleeAttackRequestSend", 0xE0));
            if (Il2Cpp::IsUnityObjectSafe(st)) g_kaMeleeReqSendStash = st;
        }
    }
    if (!g_kaMeleeReqSendStash) {
        if (void* reqSys = g_kaMeleeReqSendSys.load(std::memory_order_acquire)) {
            void* cls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(reqSys) : nullptr;
            void* st = SafeMem::PtrAt(reqSys, Il2Cpp::FieldOffset(cls, "_meleeAttackRequestSend", 0x28));
            if (Il2Cpp::IsUnityObjectSafe(st)) g_kaMeleeReqSendStash = st;
        }
    }
    if (!g_kaPedDeadTagStash) {
        g_kaPedDeadTagStash = Morpeh::FetchComponentStash(w, cfg.nsPedStatesComponents.c_str(), "PedDead");
    }
    if (!g_kaPedDeadTagStash) {
        if (void* meleeSys = g_kaWeaponProduceMeleeSys.load(std::memory_order_acquire)) {
            void* cls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(meleeSys) : nullptr;
            void* st = SafeMem::PtrAt(meleeSys, Il2Cpp::FieldOffset(cls, "_pedDead", 0x108));
            if (Il2Cpp::IsUnityObjectSafe(st)) g_kaPedDeadTagStash = st;
        }
    }
    if (!g_kaPedViewMeleeKoStash) {
        g_kaPedViewMeleeKoStash = Morpeh::FetchComponentStash(w, cfg.nsMeleeCombatComps.c_str(), cfg.compPedViewMeleeKo.c_str());
    }
}

inline bool ReadPedElementId(int64_t pedEnt, int32_t& outElemId, int32_t& outElemGen) {
    if (pedEnt <= 0 || !SafeMem::IsPlausiblePtr(Morpeh::g_stashes.elementIdStash)) return false;
    int32_t pedId = Morpeh::UnpackEntityId(pedEnt);
    if (pedId < 0) return false;
    ElementIdModelVal eid{};
    if (void* p = Morpeh::Get(Morpeh::g_stashes.elementIdStash, pedId, sizeof(ElementIdModelVal))) {
        if (SafeMem::ReadVal(p, eid) && eid.id >= 1) {
            outElemId = eid.id;
            outElemGen = (eid.gen > 0) ? eid.gen : 1;
            return true;
        }
    }
    return false;
}

inline int64_t ResolveTargetPedEntity(int32_t elemId, int32_t& ioElemGen, int32_t playerId, int64_t hintPedEnt) {
    int32_t testId = 0, testGen = 1;
    if (hintPedEnt > 0 && Morpeh::UnpackEntityId(hintPedEnt) >= 0) {
        if (elemId < 1 || (ReadPedElementId(hintPedEnt, testId, testGen) && testId == elemId)) {
            if (testGen > 0) ioElemGen = testGen;
            return hintPedEnt;
        }
    }
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        for (const auto& sp : g_allServerPlayers) {
            if (sp.pedEntityVal <= 0 || Morpeh::UnpackEntityId(sp.pedEntityVal) < 0) continue;
            if (playerId > 0 && sp.playerId == playerId) {
                if (elemId < 1 || (ReadPedElementId(sp.pedEntityVal, testId, testGen) && testId == elemId)) {
                    if (testGen > 0) ioElemGen = testGen;
                    return sp.pedEntityVal;
                }
            }
            if (elemId >= 1 && ReadPedElementId(sp.pedEntityVal, testId, testGen) && testId == elemId) {
                if (testGen > 0) ioElemGen = testGen;
                return sp.pedEntityVal;
            }
        }
        for (const auto& rp : g_renderPlayers) {
            if (rp.pedEntityVal <= 0 || Morpeh::UnpackEntityId(rp.pedEntityVal) < 0) continue;
            if (playerId > 0 && rp.playerId == playerId) {
                if (elemId < 1 || (ReadPedElementId(rp.pedEntityVal, testId, testGen) && testId == elemId)) {
                    if (testGen > 0) ioElemGen = testGen;
                    return rp.pedEntityVal;
                }
            }
            if (elemId >= 1 && ReadPedElementId(rp.pedEntityVal, testId, testGen) && testId == elemId) {
                if (testGen > 0) ioElemGen = testGen;
                return rp.pedEntityVal;
            }
        }
    }
    if (playerId > 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.controlsEntityStash)) {
        if (void* ctrlPtr = Morpeh::Get(Morpeh::g_stashes.controlsEntityStash, playerId, sizeof(int64_t))) {
            int64_t ctrlEnt = 0;
            if (SafeMem::ReadVal(ctrlPtr, ctrlEnt) && ctrlEnt > 0 && Morpeh::UnpackEntityId(ctrlEnt) >= 0) {
                if (elemId < 1 || (ReadPedElementId(ctrlEnt, testId, testGen) && testId == elemId)) {
                    if (testGen > 0) ioElemGen = testGen;
                    return ctrlEnt;
                }
            }
        }
    }
    // Native ECS scan fallback over ElementId stash (covers streamed NPCs and unmapped peds)
    if (elemId >= 1 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.elementIdStash)) {
        int64_t matchedEnt = 0;
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.elementIdStash, [&](int32_t slotId, int32_t) {
            if (matchedEnt != 0 || slotId < 0) return;
            ElementIdModelVal eid{};
            if (void* p = Morpeh::Get(Morpeh::g_stashes.elementIdStash, slotId, sizeof(ElementIdModelVal))) {
                if (SafeMem::ReadVal(p, eid) && eid.id == elemId) {
                    int64_t ent = Morpeh::GetEntityById(slotId);
                    if (ent > 0 && (!SafeMem::IsPlausiblePtr(Morpeh::g_stashes.pedStash) || Morpeh::Has(Morpeh::g_stashes.pedStash, ent))) {
                        matchedEnt = ent;
                        if (eid.gen > 0) ioElemGen = eid.gen;
                    }
                }
            }
        });
        if (matchedEnt > 0) return matchedEnt;
    }
    return 0;
}

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

inline bool ResolveLocalElementId() {
    EnsureWorldFreshness();
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

inline bool SendDatagram(const char* ns, const char* dgramName, const void* payload, size_t payloadSize) {
    if (!Il2Cpp::Init() || !dgramName || payloadSize > 0x200) {
        g_kaLastSendError.store(1, std::memory_order_relaxed);
        return false;
    }
    void* clientWrapper = ResolveMetaClientWrapper();
    if (!Il2Cpp::IsUnityObjectSafe(clientWrapper)) {
        g_kaLastSendError.store(2, std::memory_order_relaxed);
        return false;
    }
    if (!Il2Cpp::g_api.method_get_object || !Il2Cpp::g_api.array_new ||
        !Il2Cpp::g_api.value_box || !Il2Cpp::g_api.gchandle_new || !Il2Cpp::g_api.gchandle_get_target) {
        g_kaLastSendError.store(6, std::memory_order_relaxed);
        return false;
    }

    DatagramSenderEntry* entry = nullptr;
    for (int i = 0; i < g_kaSenderCount; ++i) {
        if (g_kaSenders[i].name && std::strcmp(g_kaSenders[i].name, dgramName) == 0) {
            entry = &g_kaSenders[i];
            break;
        }
    }
    if (!entry) {
        if (g_kaSenderCount >= 16) return false;
        entry = &g_kaSenders[g_kaSenderCount++];
        entry->name = dgramName;
    }
    int64_t nowMs = GetMonotonicMs();
    if (entry->methodGch == 0 && entry->lastFailMs > 0 && (nowMs - entry->lastFailMs) < 1500) {
        return false;
    }

    uint8_t boxBuf[0x200]{};
    if (payload && payloadSize > 0) CopyRawBytes(boxBuf, payload, payloadSize);

    int32_t errCode = 0;
    bool sendOk = false;

    TCFlyGuard::TryRun([&]() {
        Il2Cpp::EnsureThreadAttached();

        if (entry->methodGch == 0) {
            const auto& cfg = Offsets::g_cfg;
            void* wrapperKlass = Il2Cpp::FindClass(cfg.nsFlakeFlow.c_str(), cfg.clsMetaClientWrapper.c_str());
            entry->dgramKlass = Il2Cpp::FindClass(ns, dgramName);
            void* sysTypeKlass = Il2Cpp::FindClass("System", "Type");
            if (!g_kaSysObjKlass) g_kaSysObjKlass = Il2Cpp::FindClass("System", "Object");
            void* methodInfoKlass = Il2Cpp::FindClass("System.Reflection", "MethodInfo");
            void* runtimeMethodKlass = Il2Cpp::FindClass("System.Reflection", "RuntimeMethodInfo");
            void* methodBaseKlass = Il2Cpp::FindClass("System.Reflection", "MethodBase");

            if (!wrapperKlass || !entry->dgramKlass || !sysTypeKlass || !g_kaSysObjKlass || !methodInfoKlass || !methodBaseKlass) {
                errCode = 3;
                return;
            }
            const void* sendMethod = Il2Cpp::g_api.class_get_method_from_name(wrapperKlass, "Send", 1);
            void* sendMethodObj = sendMethod ? Il2Cpp::g_api.method_get_object(sendMethod, wrapperKlass) : nullptr;
            void* makeGeneric = runtimeMethodKlass ? Il2Cpp::g_api.class_get_method_from_name(runtimeMethodKlass, "MakeGenericMethod", 1) : nullptr;
            if (!makeGeneric) makeGeneric = Il2Cpp::g_api.class_get_method_from_name(methodInfoKlass, "MakeGenericMethod", 1);

            void* typeArr = (sendMethodObj && makeGeneric) ? Il2Cpp::g_api.array_new(sysTypeKlass, 1) : nullptr;
            void* dgramTypeObj = typeArr ? Il2Cpp::g_api.type_get_object(Il2Cpp::g_api.class_get_type(entry->dgramKlass)) : nullptr;
            if (!typeArr || !dgramTypeObj) {
                errCode = 3;
                return;
            }
            void* slotAddr = reinterpret_cast<uint8_t*>(typeArr) + Offsets::Morpeh::kIl2CppArrayDataOff;
            SafeMem::Write(slotAddr, &dgramTypeObj, sizeof(void*));
            void* args[1] = { typeArr };
            void* exc = nullptr;
            void* concrete = Il2Cpp::g_api.runtime_invoke(makeGeneric, sendMethodObj, args, &exc);
            if (!concrete || exc) {
                errCode = 3;
                return;
            }
            if (!g_kaMethodBaseInvoke) {
                g_kaMethodBaseInvoke = Il2Cpp::g_api.class_get_method_from_name(methodBaseKlass, "Invoke", 2);
            }
            entry->methodGch = Il2Cpp::g_api.gchandle_new(concrete, false);
            if (!entry->methodGch || !g_kaMethodBaseInvoke) {
                errCode = 3;
                return;
            }
        }

        void* boxedDgram = Il2Cpp::g_api.value_box(entry->dgramKlass, boxBuf);
        void* objArr = boxedDgram ? Il2Cpp::g_api.array_new(g_kaSysObjKlass, 1) : nullptr;
        void* methodObj = objArr ? Il2Cpp::g_api.gchandle_get_target(entry->methodGch) : nullptr;
        if (!boxedDgram || !objArr || !methodObj) {
            errCode = 3;
            return;
        }
        void* objSlotAddr = reinterpret_cast<uint8_t*>(objArr) + Offsets::Morpeh::kIl2CppArrayDataOff;
        SafeMem::Write(objSlotAddr, &boxedDgram, sizeof(void*));
        void* invokeArgs[2] = { clientWrapper, objArr };
        void* exc = nullptr;
        Il2Cpp::g_api.runtime_invoke(g_kaMethodBaseInvoke, methodObj, invokeArgs, &exc);
        if (exc != nullptr) {
            errCode = 4;
        } else {
            sendOk = true;
        }
    });

    if (!sendOk && errCode == 0) errCode = 5;
    if (!sendOk && errCode == 3) entry->lastFailMs = nowMs;
    else if (sendOk) entry->lastFailMs = 0;
    g_kaLastSendError.store(errCode, std::memory_order_relaxed);
    return sendOk;
}

inline bool IsTargetConfirmedDead(int32_t elemId, int32_t playerId, int64_t pedEnt = 0) {
    EnsureMeleeStashesResolved();
    int64_t nowMs = GetMonotonicMs();
    if (elemId >= 1) {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        auto it = g_kaTracked.find(elemId);
        if (it != g_kaTracked.end()) {
            if (it->second.deadUntilMs > nowMs) return true;
            if (it->second.hp <= 0 && (nowMs - it->second.lastSeenMs) < 3000) return true;
        }
    }
    if (pedEnt > 0) {
        if (g_kaPedDeadTagStash && Morpeh::Has(g_kaPedDeadTagStash, pedEnt)) return true;
        int32_t pedId = Morpeh::UnpackEntityId(pedEnt);
        if (pedId >= 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.healthStash)) {
            int32_t hp = 0;
            if (void* hpPtr = Morpeh::Get(Morpeh::g_stashes.healthStash, pedId, sizeof(int32_t))) {
                if (SafeMem::ReadVal(hpPtr, hp) && hp <= 0) return true;
            }
        }
        int64_t viewEnt = Morpeh::GetViewEntity(pedEnt);
        if (viewEnt > 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.pedViewDeadStash) &&
            Morpeh::Has(Morpeh::g_stashes.pedViewDeadStash, viewEnt)) {
            return true;
        }
    }
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        for (const auto& sp : g_allServerPlayers) {
            if (playerId > 0 && sp.playerId == playerId) {
                if (sp.isStreamed && sp.health <= 0) return true;
            }
        }
        for (const auto& rp : g_renderPlayers) {
            if (playerId > 0 && rp.playerId == playerId) {
                if (rp.dead || rp.health <= 0) return true;
            }
        }
    }
    return false;
}

inline bool IsTargetInKnockout(int64_t pedEnt) {
    if (pedEnt <= 0) return false;
    EnsureMeleeStashesResolved();
    int64_t viewEnt = Morpeh::GetViewEntity(pedEnt);
    if (viewEnt <= 0) return false;
    if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.pedViewKoStash) &&
        Morpeh::Has(Morpeh::g_stashes.pedViewKoStash, viewEnt)) {
        return true;
    }
    if (SafeMem::IsPlausiblePtr(g_kaPedViewMeleeKoStash) &&
        Morpeh::Has(g_kaPedViewMeleeKoStash, viewEnt)) {
        return true;
    }
    return false;
}

inline void MarkTargetCooldown(int32_t elemId, int64_t durationMs, bool setZeroHp) {
    if (elemId < 1) return;
    int64_t nowMs = GetMonotonicMs();
    std::lock_guard<std::mutex> lk(g_kaMutex);
    TrackedElement& te = g_kaTracked[elemId];
    te.id = elemId;
    if (setZeroHp) te.hp = 0;
    te.lastDeadMs = nowMs;
    if (nowMs + durationMs > te.deadUntilMs) te.deadUntilMs = nowMs + durationMs;
}

inline std::vector<NamedTargetCandidate> CollectNamedPlayersSnapshot() {
    std::vector<NamedTargetCandidate> out;
    int64_t nowMs = GetMonotonicMs();
    Vec3 myPos{};
    bool hasMyPos = GetLocalPlayerPos(myPos);
    int32_t myElemId = g_kaLocalElemId.load(std::memory_order_relaxed);
    int64_t localPed = Morpeh::GetLocalPedEntity();
    int32_t localPedId = (localPed > 0) ? Morpeh::UnpackEntityId(localPed) : -1;

    std::vector<ServerPlayerEntry> srvSnap;
    std::vector<ESPPlayer> renderSnap;
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        srvSnap = g_allServerPlayers;
        renderSnap = g_renderPlayers;
    }

    std::unordered_map<int32_t, TrackedElement> elemSnap;
    {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        elemSnap = g_kaTracked;
    }

    out.reserve(srvSnap.size() + renderSnap.size());
    std::unordered_set<int32_t> seenPeds;
    std::unordered_set<int32_t> seenElems;
    seenPeds.reserve(srvSnap.size() + 16);
    seenElems.reserve(srvSnap.size() + 16);

    for (const auto& sp : srvSnap) {
        NamedTargetCandidate c{};
        c.playerId  = sp.playerId;
        c.digitalId = sp.digitalId;
        c.pedEnt    = sp.pedEntityVal;
        c.name      = sp.name;
        c.hp        = sp.health;
        c.pos       = sp.worldPos;
        c.hasPos    = (sp.isStreamed || (IsFiniteVec3(sp.worldPos) && (std::fabs(sp.worldPos.x) > 0.25f || std::fabs(sp.worldPos.z) > 0.25f)));
        c.isDead    = (sp.isStreamed && sp.health <= 0);
        if (sp.pedEntityVal > 0) {
            ReadPedElementId(sp.pedEntityVal, c.elemId, c.elemGen);
            if (sp.pedId >= 0) seenPeds.insert(sp.pedId);
            if (c.pedEnt == localPed) continue;
        }
        if (myElemId >= 1 && c.elemId == myElemId) continue;

        if (c.elemId >= 1) {
            seenElems.insert(c.elemId);
            auto it = elemSnap.find(c.elemId);
            if (it != elemSnap.end()) {
                if (it->second.deadUntilMs > nowMs || (it->second.hp <= 0 && (nowMs - it->second.lastSeenMs) < 3000)) {
                    c.isDead = true;
                }
                if (it->second.isPed) {
                    if (IsFiniteVec3(it->second.pos) && (std::fabs(it->second.pos.x) > 0.25f || std::fabs(it->second.pos.z) > 0.25f)) {
                        c.pos = it->second.pos;
                        c.hasPos = true;
                    }
                    if (it->second.hp <= 0) { c.hp = 0; c.isDead = true; }
                }
            }
        }
        if (!c.isDead && c.pedEnt > 0 && IsTargetConfirmedDead(c.elemId, c.playerId, c.pedEnt)) {
            c.isDead = true;
            c.hp = 0;
        }
        if (hasMyPos && c.hasPos) c.dist = Vec3Dist(myPos, c.pos);
        else if (sp.distance > 0.0f) c.dist = sp.distance;
        out.push_back(std::move(c));
    }

    // Add unmapped nearby peds / NPCs / streamed players missing from srvSnap
    for (const auto& pl : renderSnap) {
        if (pl.entityId < 0 || pl.entityId == localPedId || pl.dead) continue;
        if (seenPeds.count(pl.entityId)) continue;
        int32_t eId = 0, eGen = 1;
        if (!ReadPedElementId(pl.pedEntityVal, eId, eGen) || eId < 1) continue;
        if (myElemId >= 1 && eId == myElemId) continue;
        if (seenElems.count(eId)) continue;
        seenElems.insert(eId);

        NamedTargetCandidate c{};
        c.playerId = (pl.playerId >= 0) ? pl.playerId : -eId;
        c.elemId   = eId;
        c.elemGen  = eGen;
        c.pedEnt   = pl.pedEntityVal;
        c.name     = !pl.name.empty() ? pl.name : ("Ped #" + std::to_string(eId));
        c.hp       = pl.health;
        c.pos      = pl.worldPos;
        c.hasPos   = IsFiniteVec3(pl.worldPos) && (std::fabs(pl.worldPos.x) > 0.25f || std::fabs(pl.worldPos.z) > 0.25f);
        c.isNpc    = (pl.playerId < 0);
        c.isDead   = (pl.dead || pl.health <= 0);
        auto it = elemSnap.find(eId);
        if (it != elemSnap.end() && it->second.deadUntilMs > nowMs) c.isDead = true;
        if (!c.isDead && c.pedEnt > 0 && IsTargetConfirmedDead(eId, c.playerId, c.pedEnt)) c.isDead = true;
        c.dist     = (hasMyPos && c.hasPos) ? Vec3Dist(myPos, pl.worldPos) : 99999.0f;
        out.push_back(std::move(c));
    }

    std::sort(out.begin(), out.end(), [](const NamedTargetCandidate& a, const NamedTargetCandidate& b) {
        if (a.hasPos != b.hasPos) return a.hasPos > b.hasPos;
        if (a.hasPos && b.hasPos && std::fabs(a.dist - b.dist) > 0.05f) return a.dist < b.dist;
        return a.name < b.name;
    });
    return out;
}

inline std::vector<NamedTargetCandidate> CollectNearbyAuraTargets(float radiusM, bool includeNpcs) {
    std::vector<NamedTargetCandidate> out;
    auto all = CollectNamedPlayersSnapshot();
    int32_t myElemId = g_kaLocalElemId.load(std::memory_order_relaxed);
    for (auto& c : all) {
        if (c.elemId < 1 || (myElemId >= 1 && c.elemId == myElemId)) continue;
        if (!includeNpcs && c.isNpc) continue;
        if (!c.hasPos || c.dist > radiusM) continue;
        if (c.hp <= 0 || c.isDead) continue;
        out.push_back(std::move(c));
    }
    g_kaLastNearbyCount.store(static_cast<int32_t>(out.size()), std::memory_order_relaxed);
    return out;
}

inline bool LookupTargetPosition(int32_t elemId, int32_t playerId, int64_t pedEnt, Vec3& outPos) {
    // 1. Real-time zero-lag view Transform or Morpeh Position on live ped entity
    if (pedEnt > 0) {
        int64_t view = Morpeh::GetViewEntity(pedEnt);
        if (view > 0) {
            if (void* tr = Morpeh::GetEntityTransform(view)) {
                if (Il2Cpp::SafeGetTransformPos(tr, outPos) && IsFiniteVec3(outPos) &&
                    (std::fabs(outPos.x) > 0.25f || std::fabs(outPos.z) > 0.25f)) {
                    return true;
                }
            }
        }
        int32_t pedId = Morpeh::UnpackEntityId(pedEnt);
        if (pedId >= 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.positionStash)) {
            if (void* p = Morpeh::Get(Morpeh::g_stashes.positionStash, pedId, sizeof(Vec3))) {
                if (SafeMem::ReadVal(p, outPos) && IsFiniteVec3(outPos) &&
                    (std::fabs(outPos.x) > 0.25f || std::fabs(outPos.z) > 0.25f)) {
                    return true;
                }
            }
        }
    }
    // 2. Network displacement tracker (tracks even streamed/moving remote players)
    if (elemId >= 1) {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        auto it = g_kaTracked.find(elemId);
        if (it != g_kaTracked.end() && IsFiniteVec3(it->second.pos) &&
            (std::fabs(it->second.pos.x) > 0.25f || std::fabs(it->second.pos.z) > 0.25f)) {
            outPos = it->second.pos;
            return true;
        }
    }
    // 3. Cached ESP/server player lists
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        for (const auto& sp : g_allServerPlayers) {
            if (playerId > 0 && sp.playerId == playerId && IsFiniteVec3(sp.worldPos) &&
                (std::fabs(sp.worldPos.x) > 0.25f || std::fabs(sp.worldPos.z) > 0.25f)) {
                outPos = sp.worldPos;
                return true;
            }
        }
        for (const auto& rp : g_renderPlayers) {
            if (playerId > 0 && rp.playerId == playerId && IsFiniteVec3(rp.worldPos) &&
                (std::fabs(rp.worldPos.x) > 0.25f || std::fabs(rp.worldPos.z) > 0.25f)) {
                outPos = rp.worldPos;
                return true;
            }
        }
    }
    return false;
}

inline Vec3 ComputeMeleeStandPosition(const Vec3& myPos, const Vec3& targetPos, float standOffset) {
    float dx = targetPos.x - myPos.x;
    float dz = targetPos.z - myPos.z;
    float horiz = std::sqrt(dx * dx + dz * dz);
    if (horiz < 0.05f) return { targetPos.x + standOffset, targetPos.y, targetPos.z };
    return { targetPos.x - (dx / horiz) * standOffset, targetPos.y, targetPos.z - (dz / horiz) * standOffset };
}

// Native fallback: trigger ElementSendDisplacementSystem.SendPedDisplacement(localPed) while g_kaGhostOverridePos
// is active and temporarily override local positionStash during the invoke so native packet builder encodes hopPos.
inline bool InvokeNativeSendPedDisplacement(int64_t localPed, const Vec3* hopPosOverride = nullptr) {
    ResolveSystemsFromFeatureRunner();
    void* dispSys = g_kaElemSendDispSys.load(std::memory_order_acquire);
    if (!SafeMem::IsPlausiblePtr(dispSys) || localPed <= 0 || !Il2Cpp::Init()) return false;
    if (!g_kaSendPedDispMI) {
        const auto& cfg = Offsets::g_cfg;
        g_kaSendPedDispMI = Il2Cpp::ResolveMethod(cfg.nsElementsSys.c_str(), "ElementSendDisplacementSystem", "SendPedDisplacement", 1);
    }
    if (!SafeMem::IsPlausiblePtr(g_kaSendPedDispMI)) return false;

    void* posPtr = nullptr;
    Vec3 savedPos{};
    bool swappedPos = false;
    if (hopPosOverride && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.positionStash)) {
        int32_t pedId = Morpeh::UnpackEntityId(localPed);
        if (pedId >= 0) {
            posPtr = Morpeh::Get(Morpeh::g_stashes.positionStash, pedId, sizeof(Vec3));
            if (posPtr && SafeMem::ReadVal(posPtr, savedPos)) {
                swappedPos = SafeMem::Write(posPtr, hopPosOverride, sizeof(Vec3));
            }
        }
    }

    void* exc = nullptr;
    void* args[1] = { &localPed };
    Il2Cpp::SafeInvoke(g_kaSendPedDispMI, dispSys, args, &exc);

    if (swappedPos && posPtr) {
        SafeMem::Write(posPtr, &savedPos, sizeof(Vec3));
    }
    return (exc == nullptr);
}

inline void SendGhostPedDisplacement(const Vec3& targetPos, bool isRestore) {
    if (!ResolveLocalElementId()) return;
    int32_t myId  = g_kaLocalElemId.load(std::memory_order_acquire);
    int32_t myGen = g_kaLocalElemGen.load(std::memory_order_acquire);
    if (myId < 1) return;

    Vec3 realPos{};
    if (!GetLocalPlayerPos(realPos) || !IsFiniteVec3(targetPos)) return;
    if (std::fabs(targetPos.x) < 0.1f && std::fabs(targetPos.z) < 0.1f) return;

    PedDisplacementSyncDatagramPacket pkt{};
    if (g_kaHasPedDispTemplate.load(std::memory_order_acquire)) {
        CopyRawBytes(&pkt, g_kaPedDispTemplate, sizeof(pkt));
    } else {
        pkt.rot_w = 1.0f;
    }
    pkt.remote_id         = myId;
    pkt.remote_gen        = myGen;
    pkt.vel_x = pkt.vel_y = pkt.vel_z = 0.0f;
    pkt.is_close_distance = 1;

    double baseTime = g_kaLastRemoteTime > 0.0 ? g_kaLastRemoteTime : GetMonotonicSec();
    float chunk = std::max(2.0f, g_cfg.ghostStepChunkM);
    int64_t localPed = Morpeh::GetLocalPedEntity();

    auto emitHop = [&](const Vec3& hopPos, double hopTime) {
        pkt.pos_x = hopPos.x;
        pkt.pos_y = hopPos.y;
        pkt.pos_z = hopPos.z;
        pkt.remote_time = hopTime;
        g_kaGhostOverridePos = hopPos;
        g_kaGhostHoldUntilMs.store(GetMonotonicMs() + g_cfg.ghostHoldMs, std::memory_order_release);
        if (!SendDatagram(Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram", &pkt, sizeof(pkt))) {
            InvokeNativeSendPedDisplacement(localPed, &hopPos);
        }
    };

    if (isRestore) {
        if (!g_kaHasActiveGhost) return;
        float rdx = realPos.x - g_kaLastGhostSentPos.x;
        float rdy = realPos.y - g_kaLastGhostSentPos.y;
        float rdz = realPos.z - g_kaLastGhostSentPos.z;
        float backDist = std::sqrt(rdx * rdx + rdy * rdy + rdz * rdz);
        int hops = std::clamp(static_cast<int>(std::ceil(backDist / chunk)), 1, 8);
        for (int i = 1; i <= hops; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(hops);
            baseTime += 0.025;
            emitHop({ g_kaLastGhostSentPos.x + rdx * t, g_kaLastGhostSentPos.y + rdy * t, g_kaLastGhostSentPos.z + rdz * t }, baseTime);
        }
        g_kaGhostHoldUntilMs.store(0, std::memory_order_release);
        g_kaHasActiveGhost = false;
        g_kaPendingRestoreGhost.store(false, std::memory_order_release);
        g_kaLastRemoteTime = baseTime;
        g_kaLastGhostSentPos = realPos;
        g_kaGhostOverridePos = realPos;
        return;
    }

    Vec3 anchorPos = g_kaHasActiveGhost ? g_kaLastGhostSentPos : realPos;
    Vec3 destPos   = ComputeMeleeStandPosition(anchorPos, targetPos, g_cfg.ghostHitOffsetM);
    float dx = destPos.x - anchorPos.x;
    float dy = destPos.y - anchorPos.y;
    float dz = destPos.z - anchorPos.z;
    float segDist = std::sqrt(dx * dx + dy * dy + dz * dz);
    int hops = std::clamp(static_cast<int>(std::ceil(segDist / chunk)), 1, g_cfg.maxGhostHopsPerStrike);
    for (int i = 1; i <= hops; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(hops);
        baseTime += 0.025;
        emitHop({ anchorPos.x + dx * t, anchorPos.y + dy * t, anchorPos.z + dz * t }, baseTime);
    }
    g_kaLastRemoteTime   = baseTime;
    g_kaLastGhostSentPos = destPos;
    g_kaGhostOverridePos = destPos;
    g_kaHasActiveGhost   = true;
    g_kaGhostHoldUntilMs.store(GetMonotonicMs() + g_cfg.ghostHoldMs, std::memory_order_release);
}

inline void PerformPhysicalStepTo(const Vec3& targetPos, int64_t nowMs) {
    if (!g_cfgState.bKillAroundMicroStep) return;
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped <= 0) return;
    Vec3 curPos{};
    if (!GetLocalPlayerPos(curPos)) return;
    if (!g_kaStepActive) {
        g_kaStepOriginPos = curPos;
        g_kaStepActive = true;
    }
    Vec3 standPos = ComputeMeleeStandPosition(curPos, targetPos, g_cfg.stepApproachOffsetM);
    standPos.y += 0.05f;

    int64_t view = Morpeh::GetViewEntity(ped);
    int32_t vid = (view > 0) ? Morpeh::UnpackEntityId(view) : -1;
    if (void* pv = (vid >= 0) ? Morpeh::Get(Morpeh::g_stashes.pedViewStash, vid, 384) : nullptr) {
        void* actor = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewActor);
        void* tr = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewRoot);
        if (actor && Il2Cpp::IsUnityNativeAlive(actor) && Il2Cpp::g_unity.mCharActor_SweepAndTeleport) {
            Vec3 p = standPos;
            void* a[1] = { &p };
            Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mCharActor_SweepAndTeleport, actor, a);
        } else if (tr && Il2Cpp::IsTransformAlive(tr)) {
            Il2Cpp::SafeSetTransformPos(tr, standPos);
        }
    }
    g_kaStepReturnAtMs = nowMs + g_cfg.stepHoldMs;
}

inline void PerformStepReturnIfFinished(int64_t nowMs, bool forceReturn) {
    if (!g_kaStepActive) return;
    if (!forceReturn && nowMs < g_kaStepReturnAtMs) return;
    int64_t ped = Morpeh::GetLocalPedEntity();
    int64_t view = (ped > 0) ? Morpeh::GetViewEntity(ped) : 0;
    int32_t vid = (view > 0) ? Morpeh::UnpackEntityId(view) : -1;
    if (void* pv = (vid >= 0) ? Morpeh::Get(Morpeh::g_stashes.pedViewStash, vid, 384) : nullptr) {
        void* actor = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewActor);
        void* tr = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewRoot);
        if (actor && Il2Cpp::IsUnityNativeAlive(actor) && Il2Cpp::g_unity.mCharActor_SweepAndTeleport) {
            Vec3 p = g_kaStepOriginPos;
            void* a[1] = { &p };
            Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mCharActor_SweepAndTeleport, actor, a);
        } else if (tr && Il2Cpp::IsTransformAlive(tr)) {
            Il2Cpp::SafeSetTransformPos(tr, g_kaStepOriginPos);
        }
    }
    g_kaStepActive = false;
    g_kaStepReturnAtMs = 0;
}

inline void EndCombatModeIfNeeded(int64_t nowMs, bool forceNow) {
    if (!g_kaCombatActive) return;
    if (!forceNow && nowMs < g_kaCombatFinishAtMs) return;
    g_kaCombatActive = false;
    g_kaCombatFinishAtMs = 0;
    SendDatagram(Offsets::g_cfg.nsMeleeDatagrams.c_str(), "MeleeCombatFinishDatagram", nullptr, 0);
}

// Dispatches ONE clean fist strike against the resolved target using:
// 1. Live captured WeaponProduceAttackMeleeSystem.SendAttackRequest(liveInstance, localPed, targetPed, ...)
// 2. Post-call check & fallback to Stash<MeleeAttackRequestSend> (picked up by MeleeCombatAttackRequestSendSystem)
// 3. Direct AOT MetaClientWrapper.Send<MeleeWeaponAttackRequestDatagram> (0x38 bytes) if ECS stashes unavailable
inline bool StrikeTargetOnce(int32_t& ioElemId, int32_t& ioElemGen, int32_t targetPlayerId, int64_t hintPedEnt, int32_t strikeIdx) {
    if (!ResolveLocalElementId() || !Il2Cpp::Init()) return false;
    EnsureMeleeStashesResolved();

    int32_t myId  = g_kaLocalElemId.load(std::memory_order_acquire);
    int32_t myGen = g_kaLocalElemGen.load(std::memory_order_acquire);
    if (myId < 1 || (ioElemId >= 1 && ioElemId == myId)) return false;

    int64_t localPed  = Morpeh::GetLocalPedEntity();
    int64_t targetPed = ResolveTargetPedEntity(ioElemId, ioElemGen, targetPlayerId, hintPedEnt);
    if (localPed <= 0) return false;
    if (targetPed == localPed) return false;

    if (ioElemId < 1 && targetPed > 0) {
        ReadPedElementId(targetPed, ioElemId, ioElemGen);
    }
    if (ioElemId < 1 && targetPed <= 0) return false;

    Vec3 myPos{}, tPos{};
    if (!GetLocalPlayerPos(myPos) || !LookupTargetPosition(ioElemId, targetPlayerId, targetPed, tPos)) return false;

    float realDist = Vec3Dist(myPos, tPos);
    if (realDist > g_cfgState.killAroundRadius) return false;

    Vec3 effMyPos = (g_kaHasActiveGhost && realDist > g_cfg.directMeleeRangeM && IsFiniteVec3(g_kaLastGhostSentPos))
                        ? g_kaLastGhostSentPos
                        : myPos;
    Vec3 hitPosUnity = { tPos.x, tPos.y + 0.95f, tPos.z };
    Vec3 delta       = { tPos.x - effMyPos.x, (tPos.y + 0.95f) - (effMyPos.y + 0.95f), tPos.z - effMyPos.z };
    float dLenSq     = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    Vec3 dirUnity    = (dLenSq > 1e-5f) ? Vec3Norm(delta) : Vec3{ 0.0f, 0.0f, 1.0f };

    // Choose smart attack type: immediately Finisher if knocked out, else cycle Heavy(KO) -> Finisher(KO) -> Light(KO)
    bool koState     = IsTargetInKnockout(targetPed);
    bool isKnockout  = koState || g_cfgState.bKillAroundOneHit || ((strikeIdx % 3) != 2);
    int32_t meleeDir = MeleeEnum::kDirFront;
    int32_t heightTy = MeleeEnum::kHeightMiddle;
    int32_t attackTy = MeleeEnum::kAtkHeavy;
    if (koState || (strikeIdx % 3) == 1) {
        attackTy = MeleeEnum::kAtkFinisher;
        heightTy = koState ? MeleeEnum::kHeightLow : MeleeEnum::kHeightMiddle;
    } else if ((strikeIdx % 3) == 0) {
        attackTy = MeleeEnum::kAtkHeavy;
        heightTy = MeleeEnum::kHeightHigh;
    } else {
        attackTy = MeleeEnum::kAtkLight;
        heightTy = MeleeEnum::kHeightMiddle;
    }

    int64_t nowMs = GetMonotonicMs();
    if (ioElemId >= 1) g_kaRecentHits[(g_kaRecentWriteIdx++) & 31] = { ioElemId, nowMs };

    bool dispatched = false;
    bool hadPendingBefore = SafeMem::IsPlausiblePtr(g_kaMeleeReqSendStash) && Morpeh::Has(g_kaMeleeReqSendStash, localPed);
    bool ecsUnconfirmed = (strikeIdx >= 1 && g_kaSrvAcceptedCount == g_kaTargetStartSrvAcc);
    void* meleeSys = g_kaWeaponProduceMeleeSys.load(std::memory_order_acquire);

    // Path 1: Native WeaponProduceAttackMeleeSystem.SendAttackRequest with live `this` instance + valid `targetPed`
    if (!hadPendingBefore && SafeMem::IsPlausiblePtr(meleeSys) && targetPed > 0) {
        if (!g_kaSendAttackRequestMI) {
            g_kaSendAttackRequestMI = Il2Cpp::ResolveMethod(
                Offsets::g_cfg.nsWeaponSys.c_str(), "WeaponProduceAttackMeleeSystem", "SendAttackRequest", 8);
        }
        if (SafeMem::IsPlausiblePtr(g_kaSendAttackRequestMI)) {
            void* params[8] = {
                &localPed, &targetPed, &hitPosUnity, &dirUnity,
                &meleeDir, &heightTy, &attackTy, &isKnockout
            };
            void* exc = nullptr;
            Il2Cpp::SafeInvoke(g_kaSendAttackRequestMI, meleeSys, params, &exc);
            if (!exc && SafeMem::IsPlausiblePtr(g_kaMeleeReqSendStash)) {
                dispatched = Morpeh::Has(g_kaMeleeReqSendStash, localPed);
            }
        }
    }

    // Path 2: Stage MeleeAttackRequestSend directly into Morpeh (drained by MeleeCombatAttackRequestSendSystem.OnUpdate)
    if (!dispatched && !hadPendingBefore && SafeMem::IsPlausiblePtr(g_kaMeleeReqSendStash) && targetPed > 0) {
        MeleeAttackRequestSendComponent comp{};
        comp.target           = targetPed;
        comp.attackHeightType = heightTy;
        comp.attackDirection  = meleeDir;
        comp.attackType       = attackTy;
        comp.dir_x            = dirUnity.x;
        comp.dir_y            = dirUnity.y;
        comp.dir_z            = dirUnity.z;
        comp.hit_x            = hitPosUnity.x;
        comp.hit_y            = hitPosUnity.y;
        comp.hit_z            = hitPosUnity.z;
        comp.isKnockout       = isKnockout ? 1 : 0;
        if (Morpeh::SetComponent(g_kaMeleeReqSendStash, localPed, &comp, sizeof(comp))) {
            dispatched = true;
        }
    }

    // Path 3: Direct wire datagram fallback (exact 0x38 bytes + server-synced DateTime)
    // Fires if ECS paths did not stage, or if previous staged component wasn't drained / server didn't echo yet.
    if ((!dispatched || hadPendingBefore || ecsUnconfirmed) && ioElemId >= 1) {
        MeleeWeaponAttackRequestDatagram dg{};
        dg.attacker_id  = myId;
        dg.attacker_gen = myGen;
        dg.target_id    = ioElemId;
        dg.target_gen   = (ioElemGen > 0) ? ioElemGen : 1;
        dg.hit_height   = heightTy;
        dg.hit_dir      = meleeDir;
        dg.attack_type  = attackTy;
        dg.trace_x      = dirUnity.x;
        dg.trace_y      = dirUnity.y;
        dg.trace_z      = dirUnity.z;
        dg.is_ko        = isKnockout ? 1 : 0;
        dg.timestamp    = GetValidServerTimestamp();
        g_kaMeleeOursFlag.fetch_add(1, std::memory_order_acq_rel);
        bool sent = SendDatagram(Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackRequestDatagram", &dg, sizeof(dg));
        g_kaMeleeOursFlag.fetch_sub(1, std::memory_order_acq_rel);
        if (sent) dispatched = true;
    }

    if (dispatched) {
        g_kaLastSendError.store(0, std::memory_order_relaxed);
        g_kaCombatActive     = true;
        g_kaCombatFinishAtMs = nowMs + 600;
        g_kaTargetSentHits  += 1;
    }
    return dispatched;
}

inline int KillSingleTargetBurst(int32_t targetElemId, int32_t targetGen, int32_t targetPlayerId, int /*hitsCount*/) {
    int32_t eId = targetElemId;
    int32_t eGen = (targetGen > 0) ? targetGen : 1;
    return StrikeTargetOnce(eId, eGen, targetPlayerId, 0, g_kaTargetSentHits) ? 1 : 0;
}

// Ensures attacker server/client position is within directMeleeRangeM of `targetPos`.
// Returns true if a new jump/displacement step occurred from >directMeleeRangeM.
inline bool EngageRangeForTargetIfNeeded(int32_t elemId, int32_t playerId, int64_t pedEnt, int64_t nowMs, bool* outJumpedFromFar = nullptr) {
    if (outJumpedFromFar) *outJumpedFromFar = false;
    Vec3 myPos{}, tPos{};
    if (!GetLocalPlayerPos(myPos) || !LookupTargetPosition(elemId, playerId, pedEnt, tPos)) return false;

    float realDist = Vec3Dist(myPos, tPos);
    g_kaCurrentTargetDist = realDist;
    if (realDist > g_cfgState.killAroundRadius) return false;

    // Close range: no displacement assist needed
    if (realDist <= g_cfg.directMeleeRangeM && !g_kaHasActiveGhost && !g_kaStepActive) {
        return false;
    }

    if (!g_cfgState.bKillAroundGhostStep && !g_cfgState.bKillAroundMicroStep) {
        return false;
    }

    bool activeAssist = false;
    if (g_cfgState.bKillAroundMicroStep) {
        float effDist = Vec3Dist(myPos, tPos);
        if (effDist > (g_cfg.directMeleeRangeM * 0.85f)) {
            PerformPhysicalStepTo(tPos, nowMs);
            if (outJumpedFromFar) *outJumpedFromFar = true;
        }
        activeAssist = true;
    }
    if (g_cfgState.bKillAroundGhostStep) {
        Vec3 anchor = g_kaHasActiveGhost ? g_kaLastGhostSentPos : myPos;
        float anchorDist = Vec3Dist(anchor, tPos);
        if (!g_kaHasActiveGhost || anchorDist > (g_cfg.directMeleeRangeM * 0.78f)) {
            bool wasGhost = g_kaHasActiveGhost;
            float preJumpDist = anchorDist;
            SendGhostPedDisplacement(tPos, false);
            if (outJumpedFromFar && (!wasGhost || preJumpDist > g_cfg.directMeleeRangeM)) {
                *outJumpedFromFar = true;
            }
        } else {
            // Keep ghost override alive while swinging sequentially
            g_kaGhostOverridePos = ComputeMeleeStandPosition(anchor, tPos, g_cfg.ghostHitOffsetM);
            g_kaGhostHoldUntilMs.store(nowMs + g_cfg.ghostHoldMs, std::memory_order_release);
        }
        activeAssist = true;
    }
    return activeAssist;
}

inline void RequestKillTarget(int32_t playerId, int32_t elemId, int32_t elemGen, const std::string& name) {
    g_kaPendingKillAllReq.store(false, std::memory_order_release);
    g_kaPendingStopAll.store(false, std::memory_order_release);
    g_kaSelectedPlayerId.store(playerId, std::memory_order_release);
    g_kaCurrentTargetId = elemId;
    g_kaCurrentTargetGen = (elemGen > 0) ? elemGen : 1;
    g_kaTargetSentHits = 0;
    g_kaTargetStartSrvAcc = g_kaSrvAcceptedCount;
    {
        std::lock_guard<std::mutex> lk(g_kaUiNameMutex);
        g_kaReqTargetName = name;
        g_kaUiTargetName  = name;
    }

    g_kaPendingKillPlayerId.store(playerId, std::memory_order_release);
    g_kaPendingKillElemGen.store(elemGen > 0 ? elemGen : 1, std::memory_order_release);
    g_kaPendingKillElemId.store(elemId, std::memory_order_release);
    g_kaAutoFinishUntilMs.store(GetMonotonicMs() + g_cfg.autoFinishWindowMs, std::memory_order_release);
}

inline void RequestKillAllAroundOnce() {
    g_kaPendingStopAll.store(false, std::memory_order_release);
    g_kaPendingKillAllReq.store(true, std::memory_order_release);
}

inline void StopAll() {
    g_cfgState.bKillAroundAutoRepeat = false;
    g_cfgState.bKillAroundAura = false;
    g_kaPendingKillAllReq.store(false, std::memory_order_release);
    g_kaPendingKillElemId.store(0, std::memory_order_release);
    g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
    g_kaAutoFinishUntilMs.store(0, std::memory_order_release);
    g_kaGhostHoldUntilMs.store(0, std::memory_order_release);
    g_kaKillAllSeqIdx.store(0, std::memory_order_release);
    g_kaKillAllSeqTotal.store(0, std::memory_order_release);
    g_kaSelectedPlayerId.store(0, std::memory_order_release);
    g_kaSeqUiActive.store(false, std::memory_order_release);
    g_kaPendingStopAll.store(true, std::memory_order_release);
    g_kaPendingRestoreGhost.store(true, std::memory_order_release);
}

// Normalizes datagram pointer whether IL2CPP invoked it unboxed (`T*`) or boxed (`Il2CppObject*`).
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

// Datagram feeders invoked by FeatureModules hooks
inline void OnPedDisplacementRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_kaClsPedDisp, Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    PedDisplacementSyncDatagramPacket pkt{};
    if (!SafeMem::ReadVal(ptr, pkt) || pkt.remote_id < 1 || pkt.remote_id > 2000000) return;
    Vec3 p{ pkt.pos_x, pkt.pos_y, pkt.pos_z };
    if (!IsFiniteVec3(p) || (std::fabs(p.x) < 0.1f && std::fabs(p.z) < 0.1f)) return;
    int64_t nowMs = GetMonotonicMs();
    std::lock_guard<std::mutex> lk(g_kaMutex);
    TrackedElement& te = g_kaTracked[pkt.remote_id];
    te.id = pkt.remote_id;
    if (pkt.remote_gen > 0 && pkt.remote_gen < 100000) te.gen = pkt.remote_gen;
    te.pos = p;
    te.lastSeenMs = nowMs;
    te.isPed = true;
}

inline void OnPedDisplacementWrite(void* rawPkt) {
    void* ptr = NormalizeDatagramPtrMut(rawPkt, g_kaClsPedDisp, Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    PedDisplacementSyncDatagramPacket inPkt{};
    if (!SafeMem::ReadVal(ptr, inPkt)) return;
    if (inPkt.remote_id >= 1 && inPkt.remote_id < 2000000) {
        CopyRawBytes(g_kaPedDispTemplate, &inPkt, sizeof(g_kaPedDispTemplate));
        g_kaHasPedDispTemplate.store(true, std::memory_order_release);
        if (std::isfinite(inPkt.remote_time) && inPkt.remote_time > g_kaLastRemoteTime) {
            g_kaLastRemoteTime = inPkt.remote_time;
        }
    }
    if (g_cfgState.bKillAroundGhostStep && g_kaGhostHoldUntilMs.load(std::memory_order_acquire) > GetMonotonicMs() &&
        (std::fabs(g_kaGhostOverridePos.x) > 0.1f || std::fabs(g_kaGhostOverridePos.z) > 0.1f)) {
        inPkt.pos_x = g_kaGhostOverridePos.x;
        inPkt.pos_y = g_kaGhostOverridePos.y;
        inPkt.pos_z = g_kaGhostOverridePos.z;
        inPkt.vel_x = inPkt.vel_y = inPkt.vel_z = 0.0f;
        inPkt.is_close_distance = 1;
        SafeMem::Write(ptr, &inPkt, sizeof(inPkt));
    }
}

inline void OnPedDeadRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_kaClsPedDead, Offsets::g_cfg.nsIBNetPedSync.c_str(), "PedDeadDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    ElementIdModelVal eid{};
    if (!SafeMem::ReadVal(ptr, eid) || eid.id < 1 || eid.id > 2000000) return;
    MarkTargetCooldown(eid.id, g_cfg.deadCooldownMs, true);
    g_kaSrvLastDeadId = eid.id;
    g_kaSrvLastDeadMs = GetMonotonicMs();
}

inline void OnMeleeAttackRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_kaClsMeleeAtk, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    MeleeWeaponAttackDatagramPacket pkt{};
    if (!SafeMem::ReadVal(ptr, pkt)) return;
    if (pkt.attacker_id < 1 || pkt.attacker_id > 2000000) return;
    CopyRawBytes(g_kaMeleeTemplate, &pkt, sizeof(g_kaMeleeTemplate));
    g_kaHasMeleeTemplate.store(true, std::memory_order_release);
    int32_t myId = g_kaLocalElemId.load(std::memory_order_relaxed);
    if (myId >= 1 && pkt.attacker_id == myId) ++g_kaSrvAcceptedCount;
}

inline void OnMeleeRequestWrite(void* rawPkt) {
    void* ptr = NormalizeDatagramPtrMut(rawPkt, g_kaClsMeleeReq, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackRequestDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    MeleeWeaponAttackRequestDatagram req{};
    if (!SafeMem::ReadVal(ptr, req)) return;
    if (req.attacker_id >= 1 && req.attacker_id < 2000000) {
        g_kaLocalElemId.store(req.attacker_id, std::memory_order_release);
        if (req.attacker_gen > 0 && req.attacker_gen < 100000) {
            g_kaLocalElemGen.store(req.attacker_gen, std::memory_order_release);
        }
    }
    if (g_cfgState.bKillAroundOneHit && g_kaMeleeOursFlag.load(std::memory_order_relaxed) == 0 &&
        (g_cfgState.bKillAroundAutoRepeat || g_cfgState.bKillAroundAura ||
         g_kaActiveSeq.active || g_kaCurrentTargetId > 0 ||
         GetMonotonicMs() < g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed))) {
        uint8_t ko = 1;
        // Write ONLY the 1-byte IsKnockout field at 0x28 (never overwrite past the 0x38 struct)
        SafeMem::Write(reinterpret_cast<uint8_t*>(ptr) + 0x28, &ko, sizeof(ko));
    }
}

inline void SyncUiSeqSnapshot() {
    bool act = g_kaActiveSeq.active;
    g_kaSeqUiActive.store(act, std::memory_order_release);
    g_kaSeqUiStrikes.store(act ? g_kaActiveSeq.strikesSent : 0, std::memory_order_release);
    g_kaSeqUiElemId.store(act ? g_kaActiveSeq.elemId : 0, std::memory_order_release);
    if (act) {
        std::lock_guard<std::mutex> lk(g_kaUiNameMutex);
        g_kaUiTargetName = g_kaActiveSeq.name;
    }
}

inline void StartKillAllWaveOnMainThread(int64_t nowMs) {
    auto queue = CollectNearbyAuraTargets(g_cfgState.killAroundRadius, g_cfgState.bKillAroundIncludeNpcs);
    g_kaWaveQueue = std::move(queue);
    g_kaActiveSeq = SequentialActiveTarget{};
    g_kaWaveSessionStartMs = nowMs;
    g_kaKilledThisWave = 0;
    g_kaKillAllSeqIdx.store(0, std::memory_order_release);
    g_kaKillAllSeqMs.store(nowMs, std::memory_order_release);
    g_kaKillAllSeqTotal.store(static_cast<int32_t>(g_kaWaveQueue.size()), std::memory_order_release);
    if (!g_kaWaveQueue.empty()) {
        int64_t maxDur = std::max<int64_t>(g_cfg.killAllBurstMs, static_cast<int64_t>(g_kaWaveQueue.size()) * 1800LL + 1500LL);
        g_kaKillAllBurstUntilMs.store(nowMs + maxDur, std::memory_order_release);
    } else {
        g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
    }
    SyncUiSeqSnapshot();
}

inline void BeginSequentialTarget(const NamedTargetCandidate& c, int64_t nowMs) {
    g_kaActiveSeq = SequentialActiveTarget{};
    g_kaActiveSeq.active       = true;
    g_kaActiveSeq.playerId     = c.playerId;
    g_kaActiveSeq.elemId       = c.elemId;
    g_kaActiveSeq.elemGen      = (c.elemGen > 0) ? c.elemGen : 1;
    g_kaActiveSeq.pedEnt       = ResolveTargetPedEntity(c.elemId, g_kaActiveSeq.elemGen, c.playerId, c.pedEnt);
    g_kaActiveSeq.name         = c.name;
    g_kaActiveSeq.startedMs    = nowMs;
    g_kaActiveSeq.lastStrikeMs = 0;
    g_kaActiveSeq.strikesSent  = 0;

    g_kaCurrentTargetId        = c.elemId;
    g_kaCurrentTargetGen       = g_kaActiveSeq.elemGen;
    g_kaSelectedName           = c.name;
    g_kaTargetSentHits         = 0;
    g_kaTargetStartSrvAcc      = g_kaSrvAcceptedCount;

    bool jumped = false;
    bool assist = EngageRangeForTargetIfNeeded(g_kaActiveSeq.elemId, g_kaActiveSeq.playerId, g_kaActiveSeq.pedEnt, nowMs, &jumped);
    g_kaActiveSeq.ghostAligned      = assist;
    g_kaActiveSeq.readyToStrikeAtMs = jumped ? (nowMs + g_cfg.ghostSettleLeadMs) : nowMs;
    SyncUiSeqSnapshot();
}

inline void FinishActiveWaveOrIdle(int64_t nowMs) {
    g_kaActiveSeq = SequentialActiveTarget{};
    g_kaWaveQueue.clear();
    g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
    g_kaCurrentTargetId = 0;
    SyncUiSeqSnapshot();
    if (g_kaHasActiveGhost) {
        Vec3 myPos{};
        if (GetLocalPlayerPos(myPos)) SendGhostPedDisplacement(myPos, true);
        else {
            g_kaHasActiveGhost = false;
            g_kaGhostHoldUntilMs.store(0, std::memory_order_release);
        }
    }
    PerformStepReturnIfFinished(nowMs, true);
    EndCombatModeIfNeeded(nowMs, true);
}

inline void Tick() {
    if (g_cfgState.bMasterDisable || !Morpeh::g_stashes.worldReady) return;
    EnsureWorldFreshness();
    EnsureMeleeStashesResolved();
    ResolveLocalElementId();

    int64_t nowMs = GetMonotonicMs();

    if (g_kaPendingStopAll.exchange(false, std::memory_order_acq_rel) ||
        g_kaPendingRestoreGhost.exchange(false, std::memory_order_acq_rel)) {
        FinishActiveWaveOrIdle(nowMs);
        return;
    }

    if (g_kaPendingKillAllReq.exchange(false, std::memory_order_acq_rel)) {
        StartKillAllWaveOnMainThread(nowMs);
    }

    // Immediate single-target kill request (e.g., from player list)
    int32_t reqElem = g_kaPendingKillElemId.exchange(0, std::memory_order_acq_rel);
    int32_t reqPid  = g_kaPendingKillPlayerId.load(std::memory_order_acquire);
    int32_t reqGen  = g_kaPendingKillElemGen.load(std::memory_order_acquire);
    if (reqElem >= 1) {
        NamedTargetCandidate single{};
        single.elemId   = reqElem;
        single.elemGen  = (reqGen > 0) ? reqGen : 1;
        single.playerId = reqPid;
        {
            std::lock_guard<std::mutex> lk(g_kaUiNameMutex);
            single.name = g_kaReqTargetName;
        }
        BeginSequentialTarget(single, nowMs);
        if (nowMs >= g_kaActiveSeq.readyToStrikeAtMs) {
            if (StrikeTargetOnce(g_kaActiveSeq.elemId, g_kaActiveSeq.elemGen, g_kaActiveSeq.playerId, g_kaActiveSeq.pedEnt, 0)) {
                g_kaActiveSeq.lastStrikeMs = nowMs;
                g_kaActiveSeq.strikesSent  = 1;
                SyncUiSeqSnapshot();
            }
        }
        return;
    }

    bool killAllWave   = (nowMs < g_kaKillAllBurstUntilMs.load(std::memory_order_acquire));
    bool auraOn        = g_cfgState.bKillAroundAura;
    bool autoFinishWnd = (nowMs < g_kaAutoFinishUntilMs.load(std::memory_order_acquire));
    bool singleFollow  = (g_cfgState.bKillAroundAutoRepeat || autoFinishWnd) && (g_kaCurrentTargetId >= 1);
    bool kaTabVisible  = g_menuIsOpenAtomic.load(std::memory_order_relaxed) &&
                         g_activeTabAtomic.load(std::memory_order_relaxed) == 0 &&
                         g_activeSubTabAtomic.load(std::memory_order_relaxed) == 3;

    if (!killAllWave && !auraOn && !singleFollow && !g_kaHasActiveGhost && !g_kaStepActive && !g_kaCombatActive && !kaTabVisible) {
        return;
    }

    // Periodic UI nearby count refresh while menu subtab is open
    static int64_t s_lastUiNearMs = 0;
    if (kaTabVisible && !killAllWave && !auraOn && (nowMs - s_lastUiNearMs) >= 650) {
        s_lastUiNearMs = nowMs;
        CollectNearbyAuraTargets(g_cfgState.killAroundRadius, g_cfgState.bKillAroundIncludeNpcs);
    }

    // Cleanup timed-out Kill-All wave if burst timer expired mid-wave
    if (!killAllWave && (!g_kaWaveQueue.empty() || g_kaKillAllSeqTotal.load(std::memory_order_relaxed) > 0)) {
        g_kaWaveQueue.clear();
        g_kaKillAllSeqTotal.store(0, std::memory_order_release);
        if (!auraOn && !singleFollow) {
            FinishActiveWaveOrIdle(nowMs);
            return;
        }
    }

    bool seqModeActive = (killAllWave || auraOn || singleFollow);
    if (seqModeActive) {
        // 1. Validate / advance active sequential target
        if (g_kaActiveSeq.active) {
            bool dead = IsTargetConfirmedDead(g_kaActiveSeq.elemId, g_kaActiveSeq.playerId, g_kaActiveSeq.pedEnt);
            Vec3 myPos{}, tPos{};
            bool hasLivePos = GetLocalPlayerPos(myPos) &&
                              LookupTargetPosition(g_kaActiveSeq.elemId, g_kaActiveSeq.playerId, g_kaActiveSeq.pedEnt, tPos);
            bool outOfRange = !hasLivePos || (Vec3Dist(myPos, tPos) > (g_cfgState.killAroundRadius + 2.5f));
            bool expired    = (g_kaActiveSeq.strikesSent >= g_cfg.maxStrikesPerTarget) ||
                              ((nowMs - g_kaActiveSeq.startedMs) >= g_cfg.maxMsPerTarget);

            if (dead || outOfRange || expired) {
                if (dead) {
                    MarkTargetCooldown(g_kaActiveSeq.elemId, g_cfg.deadCooldownMs, true);
                    ++g_kaKilledThisWave;
                } else if (expired) {
                    MarkTargetCooldown(g_kaActiveSeq.elemId, g_cfg.skipUnkillableMs, false);
                }
                g_kaActiveSeq.active = false;
                g_kaCurrentTargetId  = 0;
                SyncUiSeqSnapshot();
                if (singleFollow && (dead || expired)) {
                    g_kaAutoFinishUntilMs.store(0, std::memory_order_release);
                }
            }
        }

        // 2. Pick NEXT single target sequentially (only when previous target finished/died)
        if (!g_kaActiveSeq.active) {
            if (killAllWave) {
                int32_t idx = g_kaKillAllSeqIdx.load(std::memory_order_acquire);
                bool foundNext = false;
                while (idx < static_cast<int32_t>(g_kaWaveQueue.size())) {
                    const auto& cand = g_kaWaveQueue[static_cast<size_t>(idx)];
                    ++idx;
                    int32_t gen = cand.elemGen;
                    int64_t ped = ResolveTargetPedEntity(cand.elemId, gen, cand.playerId, cand.pedEnt);
                    if (IsTargetConfirmedDead(cand.elemId, cand.playerId, ped)) {
                        MarkTargetCooldown(cand.elemId, g_cfg.deadCooldownMs, true);
                        continue;
                    }
                    Vec3 myPos{}, tPos{};
                    if (!GetLocalPlayerPos(myPos) || !LookupTargetPosition(cand.elemId, cand.playerId, ped, tPos)) continue;
                    if (Vec3Dist(myPos, tPos) > g_cfgState.killAroundRadius) continue;

                    NamedTargetCandidate nextCand = cand;
                    nextCand.elemGen = gen;
                    nextCand.pedEnt  = ped;
                    BeginSequentialTarget(nextCand, nowMs);
                    g_kaKillAllSeqIdx.store(idx, std::memory_order_release);
                    g_kaKillAllSeqMs.store(nowMs, std::memory_order_release);
                    foundNext = true;
                    break;
                }
                if (!foundNext) {
                    g_kaKillAllSeqIdx.store(idx, std::memory_order_release);
                    if (!auraOn) {
                        FinishActiveWaveOrIdle(nowMs);
                        return;
                    }
                }
            } else if (auraOn) {
                static int64_t s_lastAuraScanMs = 0;
                if ((nowMs - s_lastAuraScanMs) >= 90) {
                    s_lastAuraScanMs = nowMs;
                    auto nearList = CollectNearbyAuraTargets(g_cfgState.killAroundRadius, g_cfgState.bKillAroundIncludeNpcs);
                    for (auto& cand : nearList) {
                        int32_t gen = cand.elemGen;
                        int64_t ped = ResolveTargetPedEntity(cand.elemId, gen, cand.playerId, cand.pedEnt);
                        if (IsTargetConfirmedDead(cand.elemId, cand.playerId, ped)) continue;
                        cand.elemGen = gen;
                        cand.pedEnt  = ped;
                        BeginSequentialTarget(cand, nowMs);
                        g_kaAuraSweepCount.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                }
            }
        }

        // 3. Keep range assist & dispatch ONE strike at a time on active target
        if (g_kaActiveSeq.active) {
            bool jumped = false;
            EngageRangeForTargetIfNeeded(g_kaActiveSeq.elemId, g_kaActiveSeq.playerId, g_kaActiveSeq.pedEnt, nowMs, &jumped);
            if (jumped && g_kaActiveSeq.strikesSent == 0) {
                g_kaActiveSeq.readyToStrikeAtMs = std::max(g_kaActiveSeq.readyToStrikeAtMs, nowMs + g_cfg.ghostSettleLeadMs);
            }
            int32_t interval = singleFollow && !killAllWave && !auraOn ? g_cfg.singleFollowIntervalMs : g_cfg.strikeIntervalMs;
            if (nowMs >= g_kaActiveSeq.readyToStrikeAtMs &&
                (g_kaActiveSeq.lastStrikeMs == 0 || (nowMs - g_kaActiveSeq.lastStrikeMs) >= interval)) {
                g_kaActiveSeq.lastStrikeMs = nowMs;
                if (StrikeTargetOnce(g_kaActiveSeq.elemId, g_kaActiveSeq.elemGen, g_kaActiveSeq.playerId, g_kaActiveSeq.pedEnt, g_kaActiveSeq.strikesSent)) {
                    ++g_kaActiveSeq.strikesSent;
                    g_kaSeqUiStrikes.store(g_kaActiveSeq.strikesSent, std::memory_order_release);
                }
            }
            return;
        }
    }

    // Idle cleanup when no active sequential target remains
    if (g_kaHasActiveGhost && nowMs >= g_kaGhostHoldUntilMs.load(std::memory_order_acquire)) {
        Vec3 myPos{};
        if (GetLocalPlayerPos(myPos)) SendGhostPedDisplacement(myPos, true);
        else {
            g_kaHasActiveGhost = false;
            g_kaGhostHoldUntilMs.store(0, std::memory_order_release);
        }
    }
    PerformStepReturnIfFinished(nowMs, !seqModeActive);
    EndCombatModeIfNeeded(nowMs, !seqModeActive && nowMs >= g_kaCombatFinishAtMs);
}

inline void DrawPanel(float availW) {
    using Lang::T;
    int32_t myId   = g_kaLocalElemId.load(std::memory_order_relaxed);
    int32_t myGen  = g_kaLocalElemGen.load(std::memory_order_relaxed);
    bool hasMcw    = (Morpeh::g_stashes.worldReady && Il2Cpp::IsUnityObjectSafe(Morpeh::g_stashes.realMetaClient));
    bool hasSys    = SafeMem::IsPlausiblePtr(g_kaWeaponProduceMeleeSys.load(std::memory_order_relaxed));
    int32_t err    = g_kaLastSendError.load(std::memory_order_relaxed);
    int32_t nearN  = g_kaLastNearbyCount.load(std::memory_order_relaxed);
    bool burstActive = (GetMonotonicMs() < g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed));

    ImGui::TextColored(
        (myId > 0 && hasMcw && err == 0) ? ImVec4(0.28f, 0.98f, 0.65f, 1.0f) : ImVec4(1.0f, 0.72f, 0.28f, 1.0f),
        "Elem=%d/%d | Net=%s | MeleeSys=%s | Near=%d | SrvHits=%d | E=%d",
        myId, myGen, hasMcw ? "OK" : "Wait", hasSys ? "OK" : "Fallback", nearN, g_kaSrvAcceptedCount, err);

    if (g_kaSeqUiActive.load(std::memory_order_acquire)) {
        int32_t tot = g_kaKillAllSeqTotal.load(std::memory_order_relaxed);
        int32_t cur = g_kaKillAllSeqIdx.load(std::memory_order_relaxed);
        int32_t eid = g_kaSeqUiElemId.load(std::memory_order_relaxed);
        int32_t stk = g_kaSeqUiStrikes.load(std::memory_order_relaxed);
        std::string tName;
        {
            std::lock_guard<std::mutex> lk(g_kaUiNameMutex);
            tName = g_kaUiTargetName;
        }
        ImGui::TextColored(ImVec4(0.98f, 0.45f, 0.45f, 1.0f),
                           "%s: %s (%d/%d) | Strikes=%d | Dist=%.1fm",
                           T("Target", "الهدف"),
                           !tName.empty() ? tName.c_str() : ("#" + std::to_string(eid)).c_str(),
                           tot > 0 ? cur : 1, tot > 0 ? tot : 1,
                           stk, g_kaCurrentTargetDist);
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
    ThemeEngine::ProToggle(T("Continuous Kill Aura (1-by-1)", "أورا القتل التلقائي (بالترتيب)"), &g_cfgState.bKillAroundAura);
    ThemeEngine::ProToggle(T("Remote Ghost Reach", "القتل عن بعد (غوست)"), &g_cfgState.bKillAroundGhostStep);
    ThemeEngine::ProToggle(T("One-Hit Knockout", "ضربة قاضية فورية"), &g_cfgState.bKillAroundOneHit);
    ThemeEngine::ProToggle(T("Include NPCs / Bots", "تضمين البوتات NPC"), &g_cfgState.bKillAroundIncludeNpcs);
    ThemeEngine::ProSlider(T("Kill Radius", "مدى القتل"), &g_cfgState.killAroundRadius, 3.0f, 80.0f, "%.0f m");
    if (g_cfgState.bKillAroundAura || burstActive || g_kaSeqUiActive.load(std::memory_order_acquire)) {
        if (ThemeEngine::NeonActionButton(T("STOP & RESTORE POSITION", "إيقاف واستعادة الموقع"), std::max(180.0f, availW - 8.0f), 34.0f, false)) {
            StopAll();
        }
    }
    ImGui::Spacing();
}

} // namespace KillAround
