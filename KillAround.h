#pragma once
// KillAround.h — iOS 1.4.1 Sequential Close + Remote Fist Combat Engine
// Verified against dump1.4.1.cpp:
// - Non-generic AOT-safe FlakeFlow wire sender (MetaTypeInfo.IdToInfo + IBNet_Writer + HostCommandWriter.WriteSend)
// - Full Fist Combat Mode lifecycle (MeleeCombatModeStartDatagram + MeleeCombatMode + InMeleeBattle + MeleeCombo)
// - 2-Phase Standing Combo -> Knockout -> Ground Finisher (kAtkFinisher) sequential 1-by-1 elimination
// - Stepped Ghost Displacement (1.9m..80m) with live remote_time monotonicity & clean origin restore.
#include <time.h>
#include <unordered_set>
#include "imgui.h"
#include "CoreEngine.h"
#include "Lang.h"
#include "Theme.h"

namespace KillAround {

struct Tunables {
    float   directMeleeRangeM      = 1.90f; // true point-blank fist hit radius (fixes 2.0m..4.2m server range deadzone)
    float   ghostStepChunkM        = 8.50f; // smooth per-hop displacement step
    float   ghostHitOffsetM        = 0.85f; // optimal stand distance in front of target
    float   stepApproachOffsetM    = 0.85f;
    int32_t strikeIntervalMs       = 90;    // optimal fist strike cadence (~11 Hz)
    int32_t koStrikeIntervalMs     = 115;   // finisher cadence during KnockoutFinisherDelay window
    int32_t ghostSettleLeadMs      = 55;    // server arrival lead after >1.9m ghost step before strike #0
    int32_t maxStrikesPerTarget    = 48;    // enough for 100 HP -> KO -> KnockoutFinisherDelay -> Finisher -> Dead
    int32_t maxMsPerTarget         = 5200;  // full standing + KO + finisher window per player
    int32_t skipUnkillableMs       = 1600;
    int32_t singleFollowIntervalMs = 95;
    int32_t killAllBurstMs         = 35000; // ceiling for clearing multi-player groups sequentially 1-by-1
    int32_t autoFinishWindowMs     = 5200;
    int32_t deadCooldownMs         = 5500;
    int32_t pedStaleMs             = 10000;
    int32_t ghostHoldMs            = 750;   // keeps server position locked at target between strikes
    int32_t stepHoldMs             = 350;
    int32_t maxGhostHopsPerStrike  = 10;    // covers up to 85m cleanly in <=8.5m increments
};

inline Tunables g_cfg{};

#pragma pack(push, 1)
// dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams.MeleeWeaponAttackRequestDatagram (0x38 = 56 bytes, dump1.4.1.cpp L101744)
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

// dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Components.InMeleeBattle (0x10 = 16 bytes, dump1.4.1.cpp L426406)
struct InMeleeBattleComponent {
    int64_t currentTarget;     // 0x00 Scellecs.Morpeh.Entity
    float   battleTimer;       // 0x08
    uint8_t pad_0c[4];
};
static_assert(sizeof(InMeleeBattleComponent) == 0x10, "InMeleeBattleComponent must be 0x10");

// FlakeFlow.MetaPeer (0x08 bytes, dump1.4.1.cpp L1435300)
struct FlakeMetaPeerVal {
    int32_t id;
    int32_t gen;
};
static_assert(sizeof(FlakeMetaPeerVal) == 0x08, "FlakeMetaPeerVal must be 0x08");

// System.ArraySegment<byte> unboxed layout (0x10 bytes, dump1.4.1.cpp L890276)
struct ByteArraySegmentVal {
    void*   array;
    int32_t offset;
    int32_t count;
};
static_assert(sizeof(ByteArraySegmentVal) == 0x10, "ByteArraySegmentVal must be 0x10");

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
    int64_t     koEnteredMs = 0;
    uint64_t    comboTimestamp = 0;
    int32_t     strikesSent = 0;
    int32_t     koStrikesSent = 0;
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
inline std::atomic<int32_t>  g_kaSendingOwnDgram{0};
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
inline std::atomic<int32_t>  g_kaSeqUiHp{100};
inline std::mutex            g_kaUiNameMutex;
inline std::string           g_kaUiTargetName;
inline std::string           g_kaReqTargetName;

// Live game system instances resolved from FeatureRunner / OnUpdate hooks
inline std::atomic<void*>    g_kaWeaponProduceMeleeSys{nullptr};
inline std::atomic<void*>    g_kaElemSendDispSys{nullptr};
inline std::atomic<void*>    g_kaMeleeReqSendSys{nullptr};
inline std::atomic<void*>    g_kaMeleeModeActSys{nullptr};

// Cached per-world / per-runtime IL2CPP handles & stashes
inline void*                 g_kaMeleeReqSendStash = nullptr;
inline void*                 g_kaMeleeCombatModeStash = nullptr;
inline void*                 g_kaInMeleeBattleStash = nullptr;
inline void*                 g_kaMeleeComboStash = nullptr;
inline void*                 g_kaWeaponNotAllowedStash = nullptr;
inline void*                 g_kaPedDeadTagStash = nullptr;
inline void*                 g_kaPedViewMeleeKoStash = nullptr;
inline void*                 g_kaMeleeKoHitStash = nullptr;
inline void*                 g_kaSendAttackRequestMI = nullptr;
inline void*                 g_kaSendPedDispMI = nullptr;
inline void*                 g_kaGetServerTimeMI = nullptr;
inline void*                 g_kaClsPedDisp = nullptr;
inline void*                 g_kaClsPedDead = nullptr;
inline void*                 g_kaClsMeleeAtk = nullptr;
inline void*                 g_kaClsMeleeReq = nullptr;

// Non-generic FlakeFlow AOT wire sender handles
enum class WireHeaderMode : int32_t {
    kAutoWriteULong = 0,
    kVerifiedWriteULong = 1,
    kRaw8ByteLE = 2,
    kPrefixPlus8ByteLE = 3
};

struct DatagramSenderEntry {
    const char* name = nullptr;
    void*       dgramKlass = nullptr;
    void*       writeMI = nullptr;
    uint64_t    packetId = 0;
    uint8_t     netChannel = 16; // 16 = Reliable, 32 = Unreliable
    bool        metaReady = false;
    int64_t     lastFailMs = 0;
};

inline DatagramSenderEntry   g_kaSenders[16]{};
inline int                   g_kaSenderCount = 0;
inline bool                  g_kaMetaWarmupCalled = false;
inline uint32_t              g_kaOurWriterGch = 0;
inline void*                 g_kaWriterCls = nullptr;
inline void*                 g_kaWriterResetMI = nullptr;
inline void*                 g_kaWriterToSegMI = nullptr;
inline void*                 g_kaWriteULongMI = nullptr;
inline void*                 g_kaHostWriteSendMI = nullptr;
inline void*                 g_kaPeerIsAssignedMI = nullptr;
inline void*                 g_kaPeerGetMetaPeerMI = nullptr;
inline std::atomic<int32_t>  g_kaWireHdrMode{static_cast<int32_t>(WireHeaderMode::kAutoWriteULong)};
inline uint8_t               g_kaWirePrefixBytes[8]{};
inline int32_t               g_kaWirePrefixLen = 0;

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
inline int64_t     g_kaCombatModeLastSentMs = 0;
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
    g_kaSeqUiHp.store(100, std::memory_order_release);
    g_kaStepActive = false;
    g_kaStepReturnAtMs = 0;
    g_kaCombatActive = false;
    g_kaCombatModeLastSentMs = 0;
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
    g_kaMeleeModeActSys.store(nullptr, std::memory_order_release);
    g_kaMeleeReqSendStash = nullptr;
    g_kaMeleeCombatModeStash = nullptr;
    g_kaInMeleeBattleStash = nullptr;
    g_kaMeleeComboStash = nullptr;
    g_kaWeaponNotAllowedStash = nullptr;
    g_kaPedDeadTagStash = nullptr;
    g_kaPedViewMeleeKoStash = nullptr;
    g_kaMeleeKoHitStash = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        g_kaTracked.clear();
    }
}

inline void ResolveSystemsFromFeatureRunner() {
    if (!Il2Cpp::Init()) return;
    if (!Il2Cpp::IsUnityNativeAlive(Morpeh::g_stashes.realInitializer)) {
        Morpeh::RefreshStoragesIfNeeded();
    }
    if (!Il2Cpp::IsUnityNativeAlive(Morpeh::g_stashes.realInitializer)) return;
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
    if (void* mfCls = Il2Cpp::FindClass("dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature", "MeleeCombatFeature")) {
        size_t offFeat = Il2Cpp::FieldOffset(frCls, "_meleeCombatFeature", 0x3228);
        if (!SafeMem::IsPlausiblePtr(g_kaMeleeReqSendSys.load(std::memory_order_relaxed))) {
            size_t offSys = Il2Cpp::StructFieldOffset(mfCls, "_meleeCombatRequestSendSystem", 0xC0);
            void* sys = SafeMem::PtrAt(runner, offFeat + offSys);
            void* sysCls = Il2Cpp::FindClass("dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Systems", "MeleeCombatAttackRequestSendSystem");
            if (SafeMem::IsPlausiblePtr(sys) && (!sysCls || SafeMem::PtrAt(sys, 0) == sysCls)) {
                g_kaMeleeReqSendSys.store(sys, std::memory_order_release);
            }
        }
        if (!SafeMem::IsPlausiblePtr(g_kaMeleeModeActSys.load(std::memory_order_relaxed))) {
            size_t offAct = Il2Cpp::StructFieldOffset(mfCls, "_meleeCombatModeActivateSystem", 0x30);
            void* sys = SafeMem::PtrAt(runner, offFeat + offAct);
            void* sysCls = Il2Cpp::FindClass("dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Systems", "MeleeCombatModeActivateSystem");
            if (SafeMem::IsPlausiblePtr(sys) && (!sysCls || SafeMem::PtrAt(sys, 0) == sysCls)) {
                g_kaMeleeModeActSys.store(sys, std::memory_order_release);
            }
        }
    }
}

inline void EnsureMeleeStashesResolved() {
    ResolveSystemsFromFeatureRunner();
    void* w = Morpeh::g_stashes.cachedWorld;
    if (!SafeMem::IsPlausiblePtr(w)) return;
    const auto& cfg = Offsets::g_cfg;

    if (!g_kaMeleeReqSendStash)
        g_kaMeleeReqSendStash = Morpeh::FetchComponentStash(w, cfg.nsMeleeCombatComps.c_str(), "MeleeAttackRequestSend");
    if (!g_kaMeleeCombatModeStash)
        g_kaMeleeCombatModeStash = Morpeh::FetchComponentStash(w, cfg.nsMeleeCombatComps.c_str(), "MeleeCombatMode");
    if (!g_kaInMeleeBattleStash)
        g_kaInMeleeBattleStash = Morpeh::FetchComponentStash(w, cfg.nsMeleeCombatComps.c_str(), "InMeleeBattle");
    if (!g_kaMeleeComboStash)
        g_kaMeleeComboStash = Morpeh::FetchComponentStash(w, cfg.nsMeleeCombatComps.c_str(), "MeleeCombo");
    if (!g_kaMeleeKoHitStash)
        g_kaMeleeKoHitStash = Morpeh::FetchComponentStash(w, cfg.nsMeleeCombatComps.c_str(), "MeleeKnockoutHit");
    if (!g_kaPedDeadTagStash)
        g_kaPedDeadTagStash = Morpeh::FetchComponentStash(w, cfg.nsPedStatesComponents.c_str(), "PedDead");
    if (!g_kaPedViewMeleeKoStash)
        g_kaPedViewMeleeKoStash = Morpeh::FetchComponentStash(w, cfg.nsMeleeCombatComps.c_str(), cfg.compPedViewMeleeKo.c_str());
    if (!g_kaWeaponNotAllowedStash)
        g_kaWeaponNotAllowedStash = Morpeh::FetchComponentStash(w, "dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Components", "WeaponAttackNotAllowed");

    if (void* meleeSys = g_kaWeaponProduceMeleeSys.load(std::memory_order_acquire)) {
        void* cls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(meleeSys) : nullptr;
        if (!g_kaMeleeReqSendStash) {
            void* st = SafeMem::PtrAt(meleeSys, Il2Cpp::FieldOffset(cls, "_meleeAttackRequestSend", 0xE0));
            if (Il2Cpp::IsUnityObjectSafe(st)) g_kaMeleeReqSendStash = st;
        }
        if (!g_kaMeleeComboStash) {
            void* st = SafeMem::PtrAt(meleeSys, Il2Cpp::FieldOffset(cls, "_meleeCombo", 0xD0));
            if (Il2Cpp::IsUnityObjectSafe(st)) g_kaMeleeComboStash = st;
        }
        if (!g_kaInMeleeBattleStash) {
            void* st = SafeMem::PtrAt(meleeSys, Il2Cpp::FieldOffset(cls, "_inMeleeBattle", 0x90));
            if (Il2Cpp::IsUnityObjectSafe(st)) g_kaInMeleeBattleStash = st;
        }
        if (!g_kaWeaponNotAllowedStash) {
            void* st = SafeMem::PtrAt(meleeSys, Il2Cpp::FieldOffset(cls, "_weaponAttackNotAllowed", 0xE8));
            if (Il2Cpp::IsUnityObjectSafe(st)) g_kaWeaponNotAllowedStash = st;
        }
        if (!g_kaPedDeadTagStash) {
            void* st = SafeMem::PtrAt(meleeSys, Il2Cpp::FieldOffset(cls, "_pedDead", 0x108));
            if (Il2Cpp::IsUnityObjectSafe(st)) g_kaPedDeadTagStash = st;
        }
    }
    if (void* actSys = g_kaMeleeModeActSys.load(std::memory_order_acquire)) {
        void* cls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(actSys) : nullptr;
        if (!g_kaMeleeCombatModeStash) {
            void* st = SafeMem::PtrAt(actSys, Il2Cpp::FieldOffset(cls, "_meleeCombatMode", 0x28));
            if (Il2Cpp::IsUnityObjectSafe(st)) g_kaMeleeCombatModeStash = st;
        }
        if (!g_kaInMeleeBattleStash) {
            void* st = SafeMem::PtrAt(actSys, Il2Cpp::FieldOffset(cls, "_inMeleeBattle", 0x30));
            if (Il2Cpp::IsUnityObjectSafe(st)) g_kaInMeleeBattleStash = st;
        }
        if (!g_kaMeleeComboStash) {
            void* st = SafeMem::PtrAt(actSys, Il2Cpp::FieldOffset(cls, "_meleeCombo", 0x38));
            if (Il2Cpp::IsUnityObjectSafe(st)) g_kaMeleeComboStash = st;
        }
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

// ============================================================================
// Non-Generic AOT-Safe FlakeFlow Wire Sender (Zero MakeGenericMethod!)
// ============================================================================
inline void* EnsureOurIBNetWriter() {
    if (!Il2Cpp::Init() || !Il2Cpp::g_api.object_new || !Il2Cpp::g_api.gchandle_new || !Il2Cpp::g_api.gchandle_get_target)
        return nullptr;
    if (g_kaOurWriterGch != 0) {
        void* existing = Il2Cpp::g_api.gchandle_get_target(g_kaOurWriterGch);
        if (Il2Cpp::IsUnityObjectSafe(existing)) return existing;
        g_kaOurWriterGch = 0;
    }
    if (!g_kaWriterCls) {
        g_kaWriterCls = Il2Cpp::FindClass("dev.Scripts.Networking.IBNet", "IBNet_Writer");
    }
    if (!SafeMem::IsPlausiblePtr(g_kaWriterCls)) return nullptr;
    if (!g_kaWriterResetMI)
        g_kaWriterResetMI = Il2Cpp::g_api.class_get_method_from_name(g_kaWriterCls, "Reset", 0);
    if (!g_kaWriterToSegMI)
        g_kaWriterToSegMI = Il2Cpp::g_api.class_get_method_from_name(g_kaWriterCls, "ToArraySegment", 0);
    if (!g_kaWriteULongMI)
        g_kaWriteULongMI = Il2Cpp::ResolveMethod("dev.Scripts.Networking.IBNet", "NumberSerializer", "WriteULong", 2);

    void* ctorMI = Il2Cpp::g_api.class_get_method_from_name(g_kaWriterCls, ".ctor", 1);
    if (!SafeMem::IsPlausiblePtr(ctorMI)) return nullptr;

    void* obj = nullptr;
    TCFlyGuard::TryRun([&]() {
        obj = Il2Cpp::g_api.object_new(g_kaWriterCls);
        if (SafeMem::IsPlausiblePtr(obj)) {
            int32_t cap = 512;
            void* args[1] = { &cap };
            void* exc = nullptr;
            Il2Cpp::SafeInvoke(ctorMI, obj, args, &exc);
            if (!exc) {
                g_kaOurWriterGch = Il2Cpp::g_api.gchandle_new(obj, false);
            } else {
                obj = nullptr;
            }
        }
    });
    return (g_kaOurWriterGch != 0) ? obj : nullptr;
}

inline bool ResolveDatagramMetaInfo(const char* ns, const char* dgramName, DatagramSenderEntry& entry) {
    if (entry.metaReady && entry.packetId != 0 && SafeMem::IsPlausiblePtr(entry.dgramKlass)) return true;
    if (!Il2Cpp::Init() || !dgramName) return false;

    if (!entry.dgramKlass) entry.dgramKlass = Il2Cpp::FindClass(ns, dgramName);
    if (!SafeMem::IsPlausiblePtr(entry.dgramKlass)) return false;
    if (!entry.writeMI) entry.writeMI = Il2Cpp::g_api.class_get_method_from_name(entry.dgramKlass, "Write", 1);
    if (!SafeMem::IsPlausiblePtr(entry.writeMI)) return false;

    void* mtiCls = Il2Cpp::FindClass("FlakeFlow", "MetaTypeInfo");
    if (!SafeMem::IsPlausiblePtr(mtiCls)) return false;

    void* dictObj = Il2Cpp::ResolveStaticClassFieldPtr("FlakeFlow", "MetaTypeInfo", "IdToInfo");
    if (!Il2Cpp::IsUnityObjectSafe(dictObj) && !g_kaMetaWarmupCalled) {
        g_kaMetaWarmupCalled = true;
        if (void* warmMI = Il2Cpp::g_api.class_get_method_from_name(mtiCls, "Warmup", 0)) {
            void* exc = nullptr;
            Il2Cpp::SafeInvoke(warmMI, nullptr, nullptr, &exc);
        }
        dictObj = Il2Cpp::ResolveStaticClassFieldPtr("FlakeFlow", "MetaTypeInfo", "IdToInfo");
    }
    if (!Il2Cpp::IsUnityObjectSafe(dictObj)) return false;

    void* dictCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(dictObj) : nullptr;
    size_t offEntries = Il2Cpp::FieldOffset(dictCls, "_entries", 0x18);
    size_t offCount   = Il2Cpp::FieldOffset(dictCls, "_count", 0x20);

    void* entriesArr = SafeMem::PtrAt(dictObj, offEntries);
    int32_t count = 0;
    if ((!SafeMem::IsPlausiblePtr(entriesArr) || !SafeMem::ReadVal(reinterpret_cast<uint8_t*>(dictObj) + offCount, count) || count <= 0) &&
        !g_kaMetaWarmupCalled) {
        g_kaMetaWarmupCalled = true;
        if (void* warmMI = Il2Cpp::g_api.class_get_method_from_name(mtiCls, "Warmup", 0)) {
            void* exc = nullptr;
            Il2Cpp::SafeInvoke(warmMI, nullptr, nullptr, &exc);
        }
        entriesArr = SafeMem::PtrAt(dictObj, offEntries);
        SafeMem::ReadVal(reinterpret_cast<uint8_t*>(dictObj) + offCount, count);
    }
    if (!SafeMem::IsPlausiblePtr(entriesArr) || count <= 0)
        return false;

    int32_t arrLen = 0;
    if (!SafeMem::ReadVal(reinterpret_cast<uint8_t*>(entriesArr) + Offsets::Morpeh::kIl2CppArrayLengthOff, arrLen) || arrLen <= 0)
        return false;

    int32_t elemStride = Il2Cpp::g_api.array_element_size ? Il2Cpp::g_api.array_element_size(entriesArr) : 24;
    if (elemStride < 16 || elemStride > 64) elemStride = 24;

    void* targetTypeObj = Il2Cpp::GetTypeObject(ns, dgramName);
    size_t offType       = Il2Cpp::FieldOffset(mtiCls, "type", 0x10);
    size_t offTypeName   = Il2Cpp::FieldOffset(mtiCls, "TypeName", 0x18);
    size_t offPacketId   = Il2Cpp::FieldOffset(mtiCls, "PacketId", 0x20);
    size_t offNetChannel = Il2Cpp::FieldOffset(mtiCls, "NetChannel", 0x28);

    int32_t limit = std::min(count, std::min(arrLen, 4096));
    uint8_t* dataBase = reinterpret_cast<uint8_t*>(entriesArr) + Offsets::Morpeh::kIl2CppArrayDataOff;
    std::string suffixDot = std::string(".") + dgramName;

    for (int32_t i = 0; i < limit; ++i) {
        uint8_t* slot = dataBase + static_cast<size_t>(i) * static_cast<size_t>(elemStride);
        int32_t hashCode = -1;
        if (!SafeMem::ReadVal(slot, hashCode) || hashCode < 0) continue;
        void* mtiObj = nullptr;
        for (size_t wOff = 0; wOff + sizeof(void*) <= static_cast<size_t>(elemStride); wOff += sizeof(void*)) {
            void* cand = SafeMem::PtrAt(slot, wOff);
            if (SafeMem::IsPlausiblePtr(cand) && SafeMem::PtrAt(cand, 0) == mtiCls) {
                mtiObj = cand;
                break;
            }
        }
        if (!mtiObj) continue;

        bool matched = false;
        void* entryTypeObj = SafeMem::PtrAt(mtiObj, offType);
        if (SafeMem::IsPlausiblePtr(targetTypeObj) && entryTypeObj == targetTypeObj) {
            matched = true;
        } else {
            std::string tName = Il2Cpp::ReadManagedString(SafeMem::PtrAt(mtiObj, offTypeName));
            if (!tName.empty()) {
                if (tName == dgramName ||
                    (tName.size() >= suffixDot.size() &&
                     tName.compare(tName.size() - suffixDot.size(), suffixDot.size(), suffixDot) == 0)) {
                    matched = true;
                }
            }
        }
        if (matched) {
            uint64_t pktId = 0;
            uint8_t  chan  = 16;
            SafeMem::ReadVal(reinterpret_cast<uint8_t*>(mtiObj) + offPacketId, pktId);
            SafeMem::ReadVal(reinterpret_cast<uint8_t*>(mtiObj) + offNetChannel, chan);
            if (pktId != 0) {
                entry.packetId   = pktId;
                entry.netChannel = (chan == 16 || chan == 32) ? chan : 16;
                entry.metaReady  = true;
                return true;
            }
        }
    }

    // Secondary Fnv1A64 fallback if dictionary scan missed
    if (void* fnvMI = Il2Cpp::ResolveMethod("FlakeFlow", "HashUtility", "Fnv1A64", 1)) {
        std::string full = (ns && ns[0]) ? (std::string(ns) + "." + dgramName) : std::string(dgramName);
        if (void* sObj = Il2Cpp::g_api.string_new ? Il2Cpp::g_api.string_new(full.c_str()) : nullptr) {
            void* args[1] = { sObj };
            void* exc = nullptr;
            if (void* boxed = Il2Cpp::SafeInvoke(fnvMI, nullptr, args, &exc)) {
                if (!exc) {
                    uint64_t h = 0;
                    if (SafeMem::ReadVal(Il2Cpp::Unbox(boxed), h) && h != 0) {
                        entry.packetId   = h;
                        entry.netChannel = (std::strcmp(dgramName, "PedDisplacementSyncDatagram") == 0) ? 32 : 16;
                        entry.metaReady  = true;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// Auto-verifies exact FlakeFlow header framing from live game IBNet_Writer on first outgoing packet.
inline void ObserveLiveWriterHeader(void* liveWriter, const char* ns, const char* dgramName) {
    if (g_kaSendingOwnDgram.load(std::memory_order_relaxed) > 0) return;
    if (g_kaWireHdrMode.load(std::memory_order_relaxed) != static_cast<int32_t>(WireHeaderMode::kAutoWriteULong)) return;
    if (!Il2Cpp::IsUnityObjectSafe(liveWriter) || !Il2Cpp::Init()) return;

    DatagramSenderEntry temp{};
    temp.name = dgramName;
    if (!ResolveDatagramMetaInfo(ns, dgramName, temp) || temp.packetId == 0) return;

    void* wCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(liveWriter) : nullptr;
    size_t offBuf = Il2Cpp::FieldOffset(wCls, "buffer", 0x10);
    size_t offPos = Il2Cpp::FieldOffset(wCls, "position", 0x18);

    void* bufArr = SafeMem::PtrAt(liveWriter, offBuf);
    int32_t livePos = 0;
    if (!SafeMem::IsPlausiblePtr(bufArr) || !SafeMem::ReadVal(reinterpret_cast<uint8_t*>(liveWriter) + offPos, livePos)) return;
    if (livePos <= 0 || livePos > 16) return;

    uint8_t liveBytes[16]{};
    if (!SafeMem::Read(reinterpret_cast<uint8_t*>(bufArr) + Offsets::Morpeh::kIl2CppArrayDataOff, liveBytes, static_cast<size_t>(livePos)))
        return;

    // Check raw 8-byte little-endian PacketId
    if (livePos == 8) {
        uint64_t rawId = 0;
        CopyRawBytes(&rawId, liveBytes, 8);
        if (rawId == temp.packetId) {
            g_kaWireHdrMode.store(static_cast<int32_t>(WireHeaderMode::kRaw8ByteLE), std::memory_order_release);
            return;
        }
    }
    // Check prefix + 8-byte little-endian PacketId
    if (livePos > 8 && livePos <= 16) {
        uint64_t tailId = 0;
        CopyRawBytes(&tailId, liveBytes + (livePos - 8), 8);
        if (tailId == temp.packetId) {
            g_kaWirePrefixLen = livePos - 8;
            CopyRawBytes(g_kaWirePrefixBytes, liveBytes, static_cast<size_t>(g_kaWirePrefixLen));
            g_kaWireHdrMode.store(static_cast<int32_t>(WireHeaderMode::kPrefixPlus8ByteLE), std::memory_order_release);
            return;
        }
    }
    // Otherwise NumberSerializer.WriteULong (varint / custom IBNet ulong encoding)
    g_kaWireHdrMode.store(static_cast<int32_t>(WireHeaderMode::kVerifiedWriteULong), std::memory_order_release);
}

inline bool WriteWirePacketHeader(void* writer, uint64_t packetId) {
    if (!Il2Cpp::IsUnityObjectSafe(writer)) return false;
    size_t offBuf = Il2Cpp::FieldOffset(g_kaWriterCls, "buffer", 0x10);
    size_t offPos = Il2Cpp::FieldOffset(g_kaWriterCls, "position", 0x18);

    WireHeaderMode mode = static_cast<WireHeaderMode>(g_kaWireHdrMode.load(std::memory_order_acquire));
    if (mode == WireHeaderMode::kRaw8ByteLE || (mode == WireHeaderMode::kAutoWriteULong && !g_kaWriteULongMI)) {
        void* bufArr = SafeMem::PtrAt(writer, offBuf);
        if (!SafeMem::IsPlausiblePtr(bufArr)) return false;
        int32_t newPos = 8;
        if (!SafeMem::Write(reinterpret_cast<uint8_t*>(bufArr) + Offsets::Morpeh::kIl2CppArrayDataOff, &packetId, 8)) return false;
        return SafeMem::Write(reinterpret_cast<uint8_t*>(writer) + offPos, &newPos, sizeof(int32_t));
    }
    if (mode == WireHeaderMode::kPrefixPlus8ByteLE && g_kaWirePrefixLen > 0 && g_kaWirePrefixLen <= 8) {
        void* bufArr = SafeMem::PtrAt(writer, offBuf);
        if (!SafeMem::IsPlausiblePtr(bufArr)) return false;
        uint8_t* dst = reinterpret_cast<uint8_t*>(bufArr) + Offsets::Morpeh::kIl2CppArrayDataOff;
        if (!SafeMem::Write(dst, g_kaWirePrefixBytes, static_cast<size_t>(g_kaWirePrefixLen))) return false;
        if (!SafeMem::Write(dst + g_kaWirePrefixLen, &packetId, 8)) return false;
        int32_t newPos = g_kaWirePrefixLen + 8;
        return SafeMem::Write(reinterpret_cast<uint8_t*>(writer) + offPos, &newPos, sizeof(int32_t));
    }
    if (SafeMem::IsPlausiblePtr(g_kaWriteULongMI)) {
        uint64_t idArg = packetId;
        void* args[2] = { writer, &idArg };
        void* exc = nullptr;
        Il2Cpp::SafeInvoke(g_kaWriteULongMI, nullptr, args, &exc);
        return (exc == nullptr);
    }
    return false;
}

inline bool SendDatagram(const char* ns, const char* dgramName, const void* payload, size_t payloadSize) {
    if (!Il2Cpp::Init() || !dgramName || payloadSize > 0x200) {
        g_kaLastSendError.store(1, std::memory_order_relaxed);
        return false;
    }
    void* mcw = ResolveMetaClientWrapper();
    if (!Il2Cpp::IsUnityObjectSafe(mcw)) {
        g_kaLastSendError.store(2, std::memory_order_relaxed);
        return false;
    }
    if (!Il2Cpp::g_api.value_box) {
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
    if (!entry->metaReady && entry->lastFailMs > 0 && (nowMs - entry->lastFailMs) < 1000) {
        return false;
    }

    uint8_t boxBuf[0x200]{};
    if (payload && payloadSize > 0) CopyRawBytes(boxBuf, payload, payloadSize);

    int32_t errCode = 0;
    bool sendOk = false;

    TCFlyGuard::TryRun([&]() {
        Il2Cpp::EnsureThreadAttached();

        if (!ResolveDatagramMetaInfo(ns, dgramName, *entry)) {
            errCode = 3;
            return;
        }

        // Navigate MetaClientWrapper -> _client (FlakeFlowClient) -> _peerProvider & _commandWriter
        void* mcwCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(mcw) : nullptr;
        void* ffClient = SafeMem::PtrAt(mcw, Il2Cpp::FieldOffset(mcwCls, "_client", 0x38));
        if (!Il2Cpp::IsUnityObjectSafe(ffClient)) {
            errCode = 2;
            return;
        }
        void* ffcCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(ffClient) : nullptr;
        void* peerProv  = SafeMem::PtrAt(ffClient, Il2Cpp::FieldOffset(ffcCls, "_peerProvider", 0x20));
        void* cmdWriter = SafeMem::PtrAt(ffClient, Il2Cpp::FieldOffset(ffcCls, "_commandWriter", 0x30));
        if (!Il2Cpp::IsUnityObjectSafe(peerProv) || !Il2Cpp::IsUnityObjectSafe(cmdWriter)) {
            errCode = 2;
            return;
        }

        void* ppCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(peerProv) : nullptr;
        if (!g_kaPeerIsAssignedMI && ppCls)
            g_kaPeerIsAssignedMI = Il2Cpp::g_api.class_get_method_from_name(ppCls, "IsAssigned", 0);
        if (!g_kaPeerGetMetaPeerMI && ppCls)
            g_kaPeerGetMetaPeerMI = Il2Cpp::g_api.class_get_method_from_name(ppCls, "get_MetaPeer", 0);

        uint8_t assigned = 0;
        SafeMem::ReadVal(reinterpret_cast<uint8_t*>(peerProv) + Il2Cpp::FieldOffset(ppCls, "_isAssigned", 0x10), assigned);
        if (!assigned && g_kaPeerIsAssignedMI) {
            void* exc = nullptr;
            if (void* b = Il2Cpp::SafeInvoke(g_kaPeerIsAssignedMI, peerProv, nullptr, &exc)) {
                if (!exc) SafeMem::ReadVal(Il2Cpp::Unbox(b), assigned);
            }
        }
        if (!assigned) {
            errCode = 2;
            return;
        }

        FlakeMetaPeerVal metaPeer{};
        bool gotPeer = false;
        if (g_kaPeerGetMetaPeerMI) {
            void* exc = nullptr;
            if (void* b = Il2Cpp::SafeInvoke(g_kaPeerGetMetaPeerMI, peerProv, nullptr, &exc)) {
                if (!exc && SafeMem::ReadVal(Il2Cpp::Unbox(b), metaPeer)) gotPeer = true;
            }
        }
        if (!gotPeer) {
            gotPeer = SafeMem::ReadVal(
                reinterpret_cast<uint8_t*>(peerProv) + Il2Cpp::FieldOffset(ppCls, "<MetaPeer>k__BackingField", 0x14),
                metaPeer);
        }
        if (!gotPeer) {
            errCode = 2;
            return;
        }

        if (!g_kaHostWriteSendMI) {
            void* cwCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(cmdWriter) : nullptr;
            if (cwCls) g_kaHostWriteSendMI = Il2Cpp::g_api.class_get_method_from_name(cwCls, "WriteSend", 3);
        }
        void* ourWriter = EnsureOurIBNetWriter();
        if (!SafeMem::IsPlausiblePtr(g_kaHostWriteSendMI) || !Il2Cpp::IsUnityObjectSafe(ourWriter) || !g_kaWriterResetMI) {
            errCode = 6;
            return;
        }

        // 1. Reset writer & write PacketId header
        void* exc = nullptr;
        Il2Cpp::SafeInvoke(g_kaWriterResetMI, ourWriter, nullptr, &exc);
        if (exc || !WriteWirePacketHeader(ourWriter, entry->packetId)) {
            errCode = 4;
            return;
        }

        size_t offPos = Il2Cpp::FieldOffset(g_kaWriterCls, "position", 0x18);
        size_t offBuf = Il2Cpp::FieldOffset(g_kaWriterCls, "buffer", 0x10);
        int32_t posBefore = 0;
        SafeMem::ReadVal(reinterpret_cast<uint8_t*>(ourWriter) + offPos, posBefore);

        // 2. Serialize datagram payload via non-generic Datagram.Write(IBNet_Writer)
        void* boxedDgram = Il2Cpp::g_api.value_box(entry->dgramKlass, boxBuf);
        if (!SafeMem::IsPlausiblePtr(boxedDgram)) {
            errCode = 4;
            return;
        }
        void* writeArgs[1] = { ourWriter };
        exc = nullptr;
        g_kaSendingOwnDgram.fetch_add(1, std::memory_order_acq_rel);
        Il2Cpp::SafeInvoke(entry->writeMI, boxedDgram, writeArgs, &exc);
        g_kaSendingOwnDgram.fetch_sub(1, std::memory_order_acq_rel);
        if (exc) {
            errCode = 4;
            return;
        }

        int32_t posAfter = 0;
        SafeMem::ReadVal(reinterpret_cast<uint8_t*>(ourWriter) + offPos, posAfter);
        if (posAfter <= 0 || (payloadSize > 0 && posAfter <= posBefore)) {
            errCode = 5;
            return;
        }

        // 3. Build ArraySegment<byte> & dispatch HostCommandWriter.WriteSend(metaPeer, segment, netChannel)
        ByteArraySegmentVal segVal{};
        void* segArgPtr = nullptr;
        if (g_kaWriterToSegMI) {
            exc = nullptr;
            if (void* boxedSeg = Il2Cpp::SafeInvoke(g_kaWriterToSegMI, ourWriter, nullptr, &exc)) {
                void* unboxed = !exc ? Il2Cpp::Unbox(boxedSeg) : nullptr;
                if (SafeMem::IsPlausiblePtr(unboxed) && SafeMem::IsPlausiblePtr(SafeMem::PtrAt(unboxed, 0))) {
                    segArgPtr = unboxed;
                }
            }
        }
        if (!segArgPtr) {
            segVal.array  = SafeMem::PtrAt(ourWriter, offBuf);
            segVal.offset = 0;
            segVal.count  = posAfter;
            if (SafeMem::IsPlausiblePtr(segVal.array)) segArgPtr = &segVal;
        }
        if (!segArgPtr) {
            errCode = 5;
            return;
        }

        uint8_t chanVal = entry->netChannel;
        void* sendArgs[3] = { &metaPeer, segArgPtr, &chanVal };
        exc = nullptr;
        Il2Cpp::SafeInvoke(g_kaHostWriteSendMI, cmdWriter, sendArgs, &exc);
        if (exc) {
            errCode = 4;
            return;
        }
        sendOk = true;
    });

    if (!sendOk && errCode == 0) errCode = 5;
    if (!sendOk && errCode == 3) entry->lastFailMs = nowMs;
    else if (sendOk) entry->lastFailMs = 0;
    g_kaLastSendError.store(errCode, std::memory_order_relaxed);
    return sendOk;
}

inline int32_t GetTargetLiveHealth(int32_t elemId, int32_t playerId, int64_t pedEnt) {
    if (pedEnt > 0) {
        int32_t pedId = Morpeh::UnpackEntityId(pedEnt);
        if (pedId >= 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.healthStash)) {
            int32_t hp = 100;
            if (void* hpPtr = Morpeh::Get(Morpeh::g_stashes.healthStash, pedId, sizeof(int32_t))) {
                if (SafeMem::ReadVal(hpPtr, hp)) return hp;
            }
        }
    }
    if (elemId >= 1) {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        auto it = g_kaTracked.find(elemId);
        if (it != g_kaTracked.end() && (GetMonotonicMs() - it->second.lastSeenMs) < 5000) {
            return it->second.hp;
        }
    }
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        for (const auto& rp : g_renderPlayers) {
            if (playerId > 0 && rp.playerId == playerId) return rp.health;
        }
        for (const auto& sp : g_allServerPlayers) {
            if (playerId > 0 && sp.playerId == playerId && sp.isStreamed) return sp.health;
        }
    }
    return 100;
}

inline bool IsTargetConfirmedDead(int32_t elemId, int32_t playerId, int64_t pedEnt = 0) {
    EnsureMeleeStashesResolved();
    int64_t nowMs = GetMonotonicMs();
    if (elemId >= 1) {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        auto it = g_kaTracked.find(elemId);
        if (it != g_kaTracked.end()) {
            if (it->second.deadUntilMs > nowMs) return true;
        }
    }
    if (pedEnt > 0) {
        if (g_kaPedDeadTagStash && Morpeh::Has(g_kaPedDeadTagStash, pedEnt)) return true;
        int64_t viewEnt = Morpeh::GetViewEntity(pedEnt);
        if (viewEnt > 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.pedViewDeadStash) &&
            Morpeh::Has(Morpeh::g_stashes.pedViewDeadStash, viewEnt)) {
            return true;
        }
        int32_t pedId = Morpeh::UnpackEntityId(pedEnt);
        if (pedId >= 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.healthStash)) {
            int32_t hp = 100;
            if (void* hpPtr = Morpeh::Get(Morpeh::g_stashes.healthStash, pedId, sizeof(int32_t))) {
                // Note: In OneState RP, knocked-out players may still need a Finisher until PedDead/PedViewDead is set;
                // only treat hp <= 0 as dead if NOT currently waiting in melee knockout!
                if (SafeMem::ReadVal(hpPtr, hp) && hp <= 0) {
                    bool inMeleeKo = (viewEnt > 0 && SafeMem::IsPlausiblePtr(g_kaPedViewMeleeKoStash) &&
                                      Morpeh::Has(g_kaPedViewMeleeKoStash, viewEnt));
                    if (!inMeleeKo) return true;
                }
            }
        }
    }
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        for (const auto& rp : g_renderPlayers) {
            if (playerId > 0 && rp.playerId == playerId && rp.dead) return true;
        }
    }
    return false;
}

inline bool IsTargetInKnockout(int64_t pedEnt) {
    if (pedEnt <= 0) return false;
    EnsureMeleeStashesResolved();
    if (SafeMem::IsPlausiblePtr(g_kaMeleeKoHitStash) && Morpeh::Has(g_kaMeleeKoHitStash, pedEnt)) {
        return true;
    }
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
    if (SafeMem::IsPlausiblePtr(g_kaMeleeKoHitStash) &&
        Morpeh::Has(g_kaMeleeKoHitStash, viewEnt)) {
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

    out.reserve(srvSnap.size() + renderSnap.size() + 16);
    std::unordered_set<int32_t> seenPeds;
    std::unordered_set<int32_t> seenElems;
    seenPeds.reserve(srvSnap.size() + 32);
    seenElems.reserve(srvSnap.size() + 32);

    for (const auto& sp : srvSnap) {
        NamedTargetCandidate c{};
        c.playerId  = sp.playerId;
        c.digitalId = sp.digitalId;
        c.pedEnt    = sp.pedEntityVal;
        c.name      = sp.name;
        c.hp        = sp.health;
        c.pos       = sp.worldPos;
        c.hasPos    = (sp.isStreamed || (IsFiniteVec3(sp.worldPos) && (std::fabs(sp.worldPos.x) > 0.25f || std::fabs(sp.worldPos.z) > 0.25f)));
        c.isDead    = false;
        if (sp.pedEntityVal > 0) {
            ReadPedElementId(sp.pedEntityVal, c.elemId, c.elemGen);
            int32_t pid = Morpeh::UnpackEntityId(sp.pedEntityVal);
            if (pid >= 0) seenPeds.insert(pid);
            if (c.pedEnt == localPed) continue;
            Vec3 liveP{};
            int64_t view = Morpeh::GetViewEntity(sp.pedEntityVal);
            if (view > 0) {
                if (void* tr = Morpeh::GetEntityTransform(view)) {
                    if (Il2Cpp::SafeGetTransformPos(tr, liveP) && IsFiniteVec3(liveP) &&
                        (std::fabs(liveP.x) > 0.25f || std::fabs(liveP.z) > 0.25f)) {
                        c.pos = liveP;
                        c.hasPos = true;
                    }
                }
            }
        }
        if (myElemId >= 1 && c.elemId == myElemId) continue;

        if (c.elemId >= 1) {
            seenElems.insert(c.elemId);
            auto it = elemSnap.find(c.elemId);
            if (it != elemSnap.end()) {
                if (it->second.deadUntilMs > nowMs) c.isDead = true;
                if (it->second.isPed && !c.hasPos && IsFiniteVec3(it->second.pos) &&
                    (std::fabs(it->second.pos.x) > 0.25f || std::fabs(it->second.pos.z) > 0.25f)) {
                    c.pos = it->second.pos;
                    c.hasPos = true;
                }
            }
        }
        if (!c.isDead && IsTargetConfirmedDead(c.elemId, c.playerId, c.pedEnt)) {
            c.isDead = true;
            c.hp = 0;
        }
        if (hasMyPos && c.hasPos) c.dist = Vec3Dist(myPos, c.pos);
        else if (sp.distance > 0.0f) c.dist = sp.distance;
        out.push_back(std::move(c));
    }

    for (const auto& pl : renderSnap) {
        if (pl.entityId < 0 || pl.entityId == localPedId || pl.dead) continue;
        if (seenPeds.count(pl.entityId)) continue;
        int32_t eId = 0, eGen = 1;
        if (!ReadPedElementId(pl.pedEntityVal, eId, eGen) || eId < 1) continue;
        if (myElemId >= 1 && eId == myElemId) continue;
        if (seenElems.count(eId)) continue;
        seenElems.insert(eId);
        seenPeds.insert(pl.entityId);

        NamedTargetCandidate c{};
        c.playerId = (pl.playerId >= 0) ? pl.playerId : -eId;
        c.elemId   = eId;
        c.elemGen  = eGen;
        c.pedEnt   = pl.pedEntityVal;
        c.name     = !pl.name.empty() ? pl.name : ("Player #" + std::to_string(eId));
        c.hp       = pl.health;
        c.pos      = pl.worldPos;
        c.hasPos   = IsFiniteVec3(pl.worldPos) && (std::fabs(pl.worldPos.x) > 0.25f || std::fabs(pl.worldPos.z) > 0.25f);
        c.isNpc    = (pl.playerId < 0);
        c.isDead   = IsTargetConfirmedDead(eId, c.playerId, c.pedEnt);
        c.dist     = (hasMyPos && c.hasPos) ? Vec3Dist(myPos, pl.worldPos) : 99999.0f;
        out.push_back(std::move(c));
    }

    // Direct Morpeh ECS scan fallback: guarantees nearby players/NPCs are found even when ESP is OFF!
    if (hasMyPos && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.elementIdStash) &&
        SafeMem::IsPlausiblePtr(Morpeh::g_stashes.pedStash)) {
        std::unordered_map<int32_t, int32_t> pedToPlayer;
        if (SafeMem::IsPlausiblePtr(Morpeh::g_stashes.controlsEntityStash)) {
            Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.controlsEntityStash, [&](int32_t pSlot, int32_t) {
                if (pSlot < 0) return;
                int64_t ctrlEnt = 0;
                if (void* cp = Morpeh::Get(Morpeh::g_stashes.controlsEntityStash, pSlot, sizeof(int64_t))) {
                    if (SafeMem::ReadVal(cp, ctrlEnt) && ctrlEnt > 0) {
                        int32_t pid = Morpeh::UnpackEntityId(ctrlEnt);
                        if (pid >= 0) pedToPlayer[pid] = pSlot;
                    }
                }
            });
        }
        Morpeh::ForEachSlotEntityId(Morpeh::g_stashes.elementIdStash, [&](int32_t slotId, int32_t) {
            if (slotId < 0 || slotId == localPedId || seenPeds.count(slotId)) return;
            int64_t ent = Morpeh::GetEntityById(slotId);
            if (ent <= 0 || ent == localPed || !Morpeh::Has(Morpeh::g_stashes.pedStash, ent)) return;

            ElementIdModelVal eid{};
            if (void* ep = Morpeh::Get(Morpeh::g_stashes.elementIdStash, slotId, sizeof(ElementIdModelVal))) {
                if (!SafeMem::ReadVal(ep, eid) || eid.id < 1) return;
            } else {
                return;
            }
            if ((myElemId >= 1 && eid.id == myElemId) || seenElems.count(eid.id)) return;

            Vec3 p{};
            bool okPos = false;
            int64_t view = Morpeh::GetViewEntity(ent);
            if (view > 0) {
                if (void* tr = Morpeh::GetEntityTransform(view)) {
                    if (Il2Cpp::SafeGetTransformPos(tr, p) && IsFiniteVec3(p) &&
                        (std::fabs(p.x) > 0.25f || std::fabs(p.z) > 0.25f)) {
                        okPos = true;
                    }
                }
            }
            if (!okPos && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.positionStash)) {
                if (void* pp = Morpeh::Get(Morpeh::g_stashes.positionStash, slotId, sizeof(Vec3))) {
                    if (SafeMem::ReadVal(pp, p) && IsFiniteVec3(p) &&
                        (std::fabs(p.x) > 0.25f || std::fabs(p.z) > 0.25f)) {
                        okPos = true;
                    }
                }
            }
            if (!okPos) return;

            auto pit = pedToPlayer.find(slotId);
            int32_t plId = (pit != pedToPlayer.end()) ? pit->second : -eid.id;
            if (IsTargetConfirmedDead(eid.id, plId, ent)) return;

            seenElems.insert(eid.id);
            seenPeds.insert(slotId);

            NamedTargetCandidate c{};
            c.playerId = plId;
            c.elemId   = eid.id;
            c.elemGen  = (eid.gen > 0) ? eid.gen : 1;
            c.pedEnt   = ent;
            c.name     = (plId > 0) ? ("Player #" + std::to_string(eid.id)) : ("NPC #" + std::to_string(eid.id));
            c.hp       = GetTargetLiveHealth(eid.id, plId, ent);
            c.pos      = p;
            c.hasPos   = true;
            c.isNpc    = (plId <= 0);
            c.isDead   = false;
            c.dist     = Vec3Dist(myPos, p);
            out.push_back(std::move(c));
        });
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
        if (c.isDead) continue;
        out.push_back(std::move(c));
    }
    g_kaLastNearbyCount.store(static_cast<int32_t>(out.size()), std::memory_order_relaxed);
    return out;
}

inline bool LookupTargetPosition(int32_t elemId, int32_t playerId, int64_t pedEnt, Vec3& outPos) {
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
    if (elemId >= 1) {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        auto it = g_kaTracked.find(elemId);
        if (it != g_kaTracked.end() && IsFiniteVec3(it->second.pos) &&
            (std::fabs(it->second.pos.x) > 0.25f || std::fabs(it->second.pos.z) > 0.25f)) {
            outPos = it->second.pos;
            return true;
        }
    }
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

    // Face toward target cleanly in packet rotation
    float faceDx = targetPos.x - realPos.x;
    float faceDz = targetPos.z - realPos.z;
    if (faceDx * faceDx + faceDz * faceDz > 0.01f) {
        float yaw = std::atan2(faceDx, faceDz);
        pkt.rot_x = 0.0f;
        pkt.rot_y = std::sin(yaw * 0.5f);
        pkt.rot_z = 0.0f;
        pkt.rot_w = std::cos(yaw * 0.5f);
    }

    double baseTime = g_kaLastRemoteTime > 0.0 ? g_kaLastRemoteTime : GetMonotonicSec();
    float chunk = std::max(2.0f, g_cfg.ghostStepChunkM);
    int64_t localPed = Morpeh::GetLocalPedEntity();

    auto emitHop = [&](const Vec3& hopPos, double hopTime) {
        pkt.pos_x = hopPos.x;
        pkt.pos_y = hopPos.y;
        pkt.pos_z = hopPos.z;
        pkt.remote_time = hopTime;
        g_kaGhostOverridePos = hopPos;
        g_kaLastRemoteTime   = hopTime;
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
        int hops = std::clamp(static_cast<int>(std::ceil(backDist / chunk)), 1, g_cfg.maxGhostHopsPerStrike);
        for (int i = 1; i <= hops; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(hops);
            baseTime += 0.022;
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
        baseTime += 0.022;
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

// Ensures both server (MeleeCombatModeStartDatagram) and local ECS (MeleeCombatMode + InMeleeBattle + MeleeCombo)
// are in active Fist Combat Mode before strikes are sent.
inline void EnsureLocalMeleeCombatMode(int64_t localPed, int64_t targetPed, uint64_t comboTs, int64_t nowMs) {
    if (!g_kaCombatActive || (nowMs - g_kaCombatModeLastSentMs) >= 3500) {
        SendDatagram(Offsets::g_cfg.nsMeleeDatagrams.c_str(), "MeleeCombatModeStartDatagram", nullptr, 0);
        g_kaCombatActive         = true;
        g_kaCombatModeLastSentMs = nowMs;
    }
    g_kaCombatFinishAtMs = nowMs + 1500;

    if (localPed > 0 && SafeMem::IsPlausiblePtr(g_kaWeaponNotAllowedStash) &&
        Morpeh::Has(g_kaWeaponNotAllowedStash, localPed)) {
        Morpeh::Remove(g_kaWeaponNotAllowedStash, localPed);
    }

    int64_t localView  = (localPed > 0) ? Morpeh::GetViewEntity(localPed) : 0;
    int64_t targetView = (targetPed > 0) ? Morpeh::GetViewEntity(targetPed) : 0;
    if (localView <= 0) return;

    if (SafeMem::IsPlausiblePtr(g_kaMeleeCombatModeStash) &&
        !Morpeh::Has(g_kaMeleeCombatModeStash, localView)) {
        uint8_t dummy = 1;
        Morpeh::SetComponent(g_kaMeleeCombatModeStash, localView, &dummy, sizeof(dummy));
    }

    if (SafeMem::IsPlausiblePtr(g_kaInMeleeBattleStash)) {
        InMeleeBattleComponent battle{};
        battle.currentTarget = (targetView > 0) ? targetView : targetPed;
        battle.battleTimer   = 5.0f;
        Morpeh::SetComponent(g_kaInMeleeBattleStash, localView, &battle, sizeof(battle));
    }

    int32_t lvId = Morpeh::UnpackEntityId(localView);
    if (lvId >= 0 && SafeMem::IsPlausiblePtr(g_kaMeleeComboStash) &&
        Morpeh::Has(g_kaMeleeComboStash, localView)) {
        // MeleeCombo.FirstHitTimeStamp is at 0x28 (dump1.4.1.cpp L426645)
        if (void* comboPtr = Morpeh::Get(g_kaMeleeComboStash, lvId, 0x38)) {
            SafeMem::Write(reinterpret_cast<uint8_t*>(comboPtr) + 0x28, &comboTs, sizeof(uint64_t));
        }
    }
}

inline void EndCombatModeIfNeeded(int64_t nowMs, bool forceNow) {
    if (!g_kaCombatActive) return;
    if (!forceNow && nowMs < g_kaCombatFinishAtMs) return;
    g_kaCombatActive = false;
    g_kaCombatModeLastSentMs = 0;
    g_kaCombatFinishAtMs = 0;
    SendDatagram(Offsets::g_cfg.nsMeleeDatagrams.c_str(), "MeleeCombatFinishDatagram", nullptr, 0);
}

// Dispatches ONE lethal fist strike against the resolved target:
// - Enters MeleeCombatMode on server & client
// - Standing phase: authentic 3-hit Light/Heavy/Ender combo (never invalid Finisher while standing!)
// - Knockout phase: immediate kAtkFinisher ground finisher to turn KO into permanent kill (PedDead)
// - Sends directly via AOT-safe FlakeFlow wire sender, with native ECS fallback if wire unavailable.
inline bool StrikeTargetOnce(int32_t& ioElemId, int32_t& ioElemGen, int32_t targetPlayerId, int64_t hintPedEnt, int32_t strikeIdx) {
    if (!ResolveLocalElementId() || !Il2Cpp::Init()) return false;
    EnsureMeleeStashesResolved();

    int32_t myId  = g_kaLocalElemId.load(std::memory_order_acquire);
    int32_t myGen = g_kaLocalElemGen.load(std::memory_order_acquire);
    if (myId < 1 || (ioElemId >= 1 && ioElemId == myId)) return false;

    int64_t localPed  = Morpeh::GetLocalPedEntity();
    int64_t targetPed = ResolveTargetPedEntity(ioElemId, ioElemGen, targetPlayerId, hintPedEnt);
    if (localPed <= 0 || targetPed == localPed) return false;

    if (ioElemId < 1 && targetPed > 0) {
        ReadPedElementId(targetPed, ioElemId, ioElemGen);
    }
    if (ioElemId < 1 && targetPed <= 0) return false;

    Vec3 myPos{}, tPos{};
    if (!GetLocalPlayerPos(myPos) || !LookupTargetPosition(ioElemId, targetPlayerId, targetPed, tPos)) return false;

    float realDist = Vec3Dist(myPos, tPos);
    if (realDist > g_cfgState.killAroundRadius) return false;

    int64_t nowMs = GetMonotonicMs();
    if (g_kaActiveSeq.comboTimestamp == 0 || (strikeIdx % 3) == 0) {
        g_kaActiveSeq.comboTimestamp = GetValidServerTimestamp();
    }
    uint64_t comboTs = g_kaActiveSeq.comboTimestamp;

    EnsureLocalMeleeCombatMode(localPed, targetPed, comboTs, nowMs);

    Vec3 effMyPos = (g_kaHasActiveGhost && IsFiniteVec3(g_kaLastGhostSentPos))
                        ? g_kaLastGhostSentPos
                        : myPos;
    Vec3 hitPosUnity = { tPos.x, tPos.y + 0.90f, tPos.z };
    Vec3 delta       = { tPos.x - effMyPos.x, (tPos.y + 0.90f) - (effMyPos.y + 0.90f), tPos.z - effMyPos.z };
    float dLenSq     = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    Vec3 dirUnity    = (dLenSq > 1e-5f) ? Vec3Norm(delta) : Vec3{ 0.0f, 0.0f, 1.0f };

    int32_t liveHp = GetTargetLiveHealth(ioElemId, targetPlayerId, targetPed);
    g_kaSeqUiHp.store(liveHp, std::memory_order_relaxed);
    bool koState = IsTargetInKnockout(targetPed) || (liveHp <= 0);

    int32_t meleeDir   = MeleeEnum::kDirFront;
    int32_t heightTy   = MeleeEnum::kHeightMiddle;
    int32_t attackTy   = MeleeEnum::kAtkHeavy;
    bool    isKnockout = false;

    if (koState) {
        if (g_kaActiveSeq.koEnteredMs == 0) {
            g_kaActiveSeq.koEnteredMs = nowMs;
            g_kaActiveSeq.comboTimestamp = GetValidServerTimestamp();
            comboTs = g_kaActiveSeq.comboTimestamp;
        }
        int32_t koIdx = g_kaActiveSeq.koStrikesSent++;
        // Primary ground execution: kAtkFinisher; every 4th KO strike mix Heavy low as fallback
        if ((koIdx % 4) == 3) {
            attackTy = MeleeEnum::kAtkHeavy;
            heightTy = MeleeEnum::kHeightLow;
            meleeDir = MeleeEnum::kDirFront;
        } else {
            attackTy = MeleeEnum::kAtkFinisher;
            heightTy = MeleeEnum::kHeightLow;
            meleeDir = MeleeEnum::kDirFront;
        }
        isKnockout = true;
    } else {
        // Authentic 3-hit standing combo: Light -> Heavy -> Heavy(ComboEnder + KO)
        int32_t step = strikeIdx % 3;
        if (step == 0) {
            attackTy   = MeleeEnum::kAtkLight;
            heightTy   = MeleeEnum::kHeightHigh;
            meleeDir   = MeleeEnum::kDirLeft;
            isKnockout = false;
        } else if (step == 1) {
            attackTy   = MeleeEnum::kAtkHeavy;
            heightTy   = MeleeEnum::kHeightMiddle;
            meleeDir   = MeleeEnum::kDirRight;
            isKnockout = g_cfgState.bKillAroundOneHit && (liveHp <= 45 || strikeIdx >= 3);
        } else {
            attackTy   = MeleeEnum::kAtkHeavy;
            heightTy   = MeleeEnum::kHeightHigh;
            meleeDir   = MeleeEnum::kDirFront;
            isKnockout = true;
        }
    }

    if (ioElemId >= 1) g_kaRecentHits[(g_kaRecentWriteIdx++) & 31] = { ioElemId, nowMs };

    bool dispatched = false;

    // Tier 1 (Primary Guaranteed Path): Non-generic AOT-safe FlakeFlow wire send (0x38 bytes)
    if (ioElemId >= 1) {
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
        dg.timestamp    = comboTs;

        g_kaMeleeOursFlag.fetch_add(1, std::memory_order_acq_rel);
        bool sent = SendDatagram(Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackRequestDatagram", &dg, sizeof(dg));
        g_kaMeleeOursFlag.fetch_sub(1, std::memory_order_acq_rel);
        if (sent) dispatched = true;
    }

    // Tier 2 (ECS Fallback if wire sender not yet warmed up): WeaponProduceAttackMeleeSystem.SendAttackRequest + Stash
    if (!dispatched && targetPed > 0) {
        void* meleeSys = g_kaWeaponProduceMeleeSys.load(std::memory_order_acquire);
        if (SafeMem::IsPlausiblePtr(meleeSys)) {
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
        if (!dispatched && SafeMem::IsPlausiblePtr(g_kaMeleeReqSendStash)) {
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
    }

    if (dispatched) {
        g_kaLastSendError.store(0, std::memory_order_relaxed);
        g_kaTargetSentHits += 1;
    }
    return dispatched;
}

inline int KillSingleTargetBurst(int32_t targetElemId, int32_t targetGen, int32_t targetPlayerId, int /*hitsCount*/) {
    int32_t eId = targetElemId;
    int32_t eGen = (targetGen > 0) ? targetGen : 1;
    return StrikeTargetOnce(eId, eGen, targetPlayerId, 0, g_kaTargetSentHits) ? 1 : 0;
}

inline bool EngageRangeForTargetIfNeeded(int32_t elemId, int32_t playerId, int64_t pedEnt, int64_t nowMs, bool* outJumpedFromFar = nullptr) {
    if (outJumpedFromFar) *outJumpedFromFar = false;
    Vec3 myPos{}, tPos{};
    if (!GetLocalPlayerPos(myPos) || !LookupTargetPosition(elemId, playerId, pedEnt, tPos)) return false;

    float realDist = Vec3Dist(myPos, tPos);
    g_kaCurrentTargetDist = realDist;
    if (realDist > g_cfgState.killAroundRadius) return false;

    // Point-blank range (<= 1.90m) with no active ghost: strike directly with zero displacement
    if (realDist <= g_cfg.directMeleeRangeM && !g_kaHasActiveGhost && !g_kaStepActive) {
        return false;
    }

    if (!g_cfgState.bKillAroundGhostStep && !g_cfgState.bKillAroundMicroStep) {
        return false;
    }

    bool activeAssist = false;
    if (g_cfgState.bKillAroundMicroStep) {
        if (realDist > g_cfg.directMeleeRangeM) {
            PerformPhysicalStepTo(tPos, nowMs);
            if (outJumpedFromFar) *outJumpedFromFar = true;
        }
        activeAssist = true;
    }
    if (g_cfgState.bKillAroundGhostStep) {
        Vec3 anchor = g_kaHasActiveGhost ? g_kaLastGhostSentPos : myPos;
        float anchorDist = Vec3Dist(anchor, tPos);
        if (!g_kaHasActiveGhost || anchorDist > 1.75f || std::fabs(anchor.y - tPos.y) > 1.1f) {
            bool wasGhost = g_kaHasActiveGhost;
            float preJumpDist = anchorDist;
            SendGhostPedDisplacement(tPos, false);
            if (outJumpedFromFar && (!wasGhost || preJumpDist > 2.2f)) {
                *outJumpedFromFar = true;
            }
        } else {
            Vec3 standPos = ComputeMeleeStandPosition(anchor, tPos, g_cfg.ghostHitOffsetM);
            g_kaGhostOverridePos = standPos;
            g_kaLastGhostSentPos = standPos;
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

inline void OnPedDisplacementWrite(void* rawPkt, void* writer = nullptr) {
    if (writer) ObserveLiveWriterHeader(writer, Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram");
    if (g_kaSendingOwnDgram.load(std::memory_order_acquire) > 0) return;

    void* ptr = NormalizeDatagramPtrMut(rawPkt, g_kaClsPedDisp, Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    PedDisplacementSyncDatagramPacket inPkt{};
    if (!SafeMem::ReadVal(ptr, inPkt)) return;
    if (inPkt.remote_id >= 1 && inPkt.remote_id < 2000000) {
        if (!g_kaHasActiveGhost) {
            CopyRawBytes(g_kaPedDispTemplate, &inPkt, sizeof(g_kaPedDispTemplate));
            g_kaHasPedDispTemplate.store(true, std::memory_order_release);
        }
        if (std::isfinite(inPkt.remote_time) && inPkt.remote_time > g_kaLastRemoteTime) {
            g_kaLastRemoteTime = inPkt.remote_time;
        }
    }
    if (g_cfgState.bKillAroundGhostStep && g_kaGhostHoldUntilMs.load(std::memory_order_acquire) > GetMonotonicMs() &&
        (std::fabs(g_kaGhostOverridePos.x) > 0.1f || std::fabs(g_kaGhostOverridePos.z) > 0.1f)) {
        g_kaLastRemoteTime += 0.020;
        inPkt.pos_x = g_kaGhostOverridePos.x;
        inPkt.pos_y = g_kaGhostOverridePos.y;
        inPkt.pos_z = g_kaGhostOverridePos.z;
        inPkt.vel_x = inPkt.vel_y = inPkt.vel_z = 0.0f;
        inPkt.remote_time = g_kaLastRemoteTime;
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
    if (pkt.target_id >= 1 && pkt.target_id < 2000000 && pkt.damage > 0) {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        TrackedElement& te = g_kaTracked[pkt.target_id];
        te.id = pkt.target_id;
        te.hp = std::max(0, te.hp - pkt.damage);
        te.lastSeenMs = GetMonotonicMs();
        te.isPed = true;
    }
}

inline void OnMeleeRequestWrite(void* rawPkt, void* writer = nullptr) {
    if (writer) ObserveLiveWriterHeader(writer, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackRequestDatagram");
    if (g_kaSendingOwnDgram.load(std::memory_order_acquire) > 0) return;

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
        int64_t maxDur = std::max<int64_t>(g_cfg.killAllBurstMs, static_cast<int64_t>(g_kaWaveQueue.size()) * 5200LL + 2000LL);
        g_kaKillAllBurstUntilMs.store(nowMs + maxDur, std::memory_order_release);
    } else {
        g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
    }
    SyncUiSeqSnapshot();
}

inline void BeginSequentialTarget(const NamedTargetCandidate& c, int64_t nowMs) {
    g_kaActiveSeq = SequentialActiveTarget{};
    g_kaActiveSeq.active         = true;
    g_kaActiveSeq.playerId       = c.playerId;
    g_kaActiveSeq.elemId         = c.elemId;
    g_kaActiveSeq.elemGen        = (c.elemGen > 0) ? c.elemGen : 1;
    g_kaActiveSeq.pedEnt         = ResolveTargetPedEntity(c.elemId, g_kaActiveSeq.elemGen, c.playerId, c.pedEnt);
    g_kaActiveSeq.name           = c.name;
    g_kaActiveSeq.startedMs      = nowMs;
    g_kaActiveSeq.lastStrikeMs   = 0;
    g_kaActiveSeq.koEnteredMs    = 0;
    g_kaActiveSeq.comboTimestamp = GetValidServerTimestamp();
    g_kaActiveSeq.strikesSent    = 0;
    g_kaActiveSeq.koStrikesSent  = 0;

    g_kaCurrentTargetId          = c.elemId;
    g_kaCurrentTargetGen         = g_kaActiveSeq.elemGen;
    g_kaSelectedName             = c.name;
    g_kaTargetSentHits           = 0;
    g_kaTargetStartSrvAcc        = g_kaSrvAcceptedCount;
    g_kaSeqUiHp.store(GetTargetLiveHealth(c.elemId, c.playerId, g_kaActiveSeq.pedEnt), std::memory_order_relaxed);

    // Pre-enter fist combat mode immediately so server is ready before Strike #0 arrives
    EnsureLocalMeleeCombatMode(Morpeh::GetLocalPedEntity(), g_kaActiveSeq.pedEnt, g_kaActiveSeq.comboTimestamp, nowMs);

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

    static int64_t s_lastUiNearMs = 0;
    if (kaTabVisible && !killAllWave && !auraOn && (nowMs - s_lastUiNearMs) >= 500) {
        s_lastUiNearMs = nowMs;
        CollectNearbyAuraTargets(g_cfgState.killAroundRadius, g_cfgState.bKillAroundIncludeNpcs);
    }

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
            if (g_kaActiveSeq.pedEnt <= 0) {
                g_kaActiveSeq.pedEnt = ResolveTargetPedEntity(g_kaActiveSeq.elemId, g_kaActiveSeq.elemGen, g_kaActiveSeq.playerId, 0);
            }
            bool dead = IsTargetConfirmedDead(g_kaActiveSeq.elemId, g_kaActiveSeq.playerId, g_kaActiveSeq.pedEnt);
            Vec3 myPos{}, tPos{};
            bool hasLivePos = GetLocalPlayerPos(myPos) &&
                              LookupTargetPosition(g_kaActiveSeq.elemId, g_kaActiveSeq.playerId, g_kaActiveSeq.pedEnt, tPos);
            bool outOfRange = !hasLivePos || (Vec3Dist(myPos, tPos) > (g_cfgState.killAroundRadius + 3.0f));
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

        // 2. Pick NEXT single target sequentially (only after previous target died or expired)
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
                if ((nowMs - s_lastAuraScanMs) >= 85) {
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

        // 3. Maintain range lock & dispatch sequential strikes on the active target
        if (g_kaActiveSeq.active) {
            bool jumped = false;
            EngageRangeForTargetIfNeeded(g_kaActiveSeq.elemId, g_kaActiveSeq.playerId, g_kaActiveSeq.pedEnt, nowMs, &jumped);
            if (jumped && g_kaActiveSeq.strikesSent == 0) {
                g_kaActiveSeq.readyToStrikeAtMs = std::max(g_kaActiveSeq.readyToStrikeAtMs, nowMs + g_cfg.ghostSettleLeadMs);
            }
            bool inKo = IsTargetInKnockout(g_kaActiveSeq.pedEnt);
            int32_t interval = inKo ? g_cfg.koStrikeIntervalMs
                                    : (singleFollow && !killAllWave && !auraOn ? g_cfg.singleFollowIntervalMs : g_cfg.strikeIntervalMs);
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
        "Elem=%d/%d | Net=%s | Melee=%s | Near=%d | SrvHits=%d | E=%d",
        myId, myGen, hasMcw ? "AOT-OK" : "Wait", hasSys ? "OK" : "Wire", nearN, g_kaSrvAcceptedCount, err);

    if (g_kaSeqUiActive.load(std::memory_order_acquire)) {
        int32_t tot = g_kaKillAllSeqTotal.load(std::memory_order_relaxed);
        int32_t cur = g_kaKillAllSeqIdx.load(std::memory_order_relaxed);
        int32_t eid = g_kaSeqUiElemId.load(std::memory_order_relaxed);
        int32_t stk = g_kaSeqUiStrikes.load(std::memory_order_relaxed);
        int32_t hp  = g_kaSeqUiHp.load(std::memory_order_relaxed);
        std::string tName;
        {
            std::lock_guard<std::mutex> lk(g_kaUiNameMutex);
            tName = g_kaUiTargetName;
        }
        ImGui::TextColored(ImVec4(0.98f, 0.45f, 0.45f, 1.0f),
                           "%s: %s (%d/%d) | HP=%d | Hits=%d | %.1fm",
                           T("Target", "الهدف"),
                           !tName.empty() ? tName.c_str() : ("#" + std::to_string(eid)).c_str(),
                           tot > 0 ? cur : 1, tot > 0 ? tot : 1,
                           hp, stk, g_kaCurrentTargetDist);
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
