#pragma once
// KillAround.h — iOS 1.4.1 Kill Everyone Around + Single-Target Instant Finisher Engine
// Ported cleanly to CoreEngine.h (SimpleDImple + Morpeh + Il2Cpp generic Invoke + SafeMem/TCFlyGuard).
#include <time.h>
#include <unordered_set>
#include "imgui.h"
#include "CoreEngine.h"
#include "Lang.h"
#include "Theme.h"

namespace KillAround {

struct Tunables {
    float   directMeleeRangeM      = 4.2f;
    float   ghostStepChunkM        = 14.0f;
    float   ghostHitOffsetM        = 0.45f;
    float   stepApproachOffsetM    = 0.85f;
    int32_t singleBurstHits        = 15;
    int32_t singleFollowHits       = 6;
    int32_t auraHitsPerTarget      = 8;
    int32_t auraMaxClosePerTick    = 4;
    int32_t auraSweepIntervalMs    = 110;
    int32_t singleFollowIntervalMs = 250;
    int32_t killAllBurstMs         = 3200;
    int32_t autoFinishWindowMs     = 2500;
    int32_t deadCooldownMs         = 5500;
    int32_t pedStaleMs             = 10000;
    int32_t ghostHoldMs            = 220;
    int32_t stepHoldMs             = 240;
    int32_t maxGhostHopsPerStrike  = 4;
};

inline Tunables g_cfg{};

#pragma pack(push, 1)
// dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams.MeleeWeaponAttackRequestDatagram (0x40 = 64 bytes, dump1.4.1.cpp L101744)
struct MeleeWeaponAttackRequestDatagram {
    int32_t  attacker_id;
    int32_t  attacker_gen;
    int32_t  target_id;
    int32_t  target_gen;
    int32_t  hit_height;
    int32_t  hit_dir;
    int32_t  attack_type;
    float    trace_x;
    float    trace_y;
    float    trace_z;
    uint8_t  is_ko;
    uint8_t  pad_29[7];
    uint64_t ts30;
    uint64_t ts38;
};
static_assert(sizeof(MeleeWeaponAttackRequestDatagram) == 0x40, "MeleeWeaponAttackRequestDatagram must be 0x40");

// dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams.MeleeWeaponAttackDatagram (0x40 = 64 bytes, dump1.4.1.cpp L101718)
struct MeleeWeaponAttackDatagramPacket {
    int32_t  attacker_id;
    int32_t  attacker_gen;
    int32_t  target_id;
    int32_t  target_gen;
    int32_t  damage;
    int32_t  damage_to_stamina;
    float    trace_x;
    float    trace_y;
    float    trace_z;
    int32_t  attack_height;
    int32_t  attack_dir;
    int32_t  attack_type;
    uint8_t  is_ko;
    uint8_t  pad_31[7];
    uint64_t timestamp;
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

struct ElementIdModelVal {
    int32_t id;
    int32_t gen;
};
#pragma pack(pop)

struct TrackedElement {
    int32_t id = 0;
    int32_t gen = 1;
    Vec3    pos{};
    float   hp = 100.0f;
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
inline std::mutex                             g_kaMutex;
inline std::unordered_map<int32_t, TrackedElement> g_kaTracked;
inline void*                                  g_kaCachedWorld = nullptr;

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
inline std::atomic<int64_t>  g_kaAutoFinishUntilMs{0};
inline std::atomic<int64_t>  g_kaKillAllBurstUntilMs{0};
inline std::atomic<int64_t>  g_kaGhostHoldUntilMs{0};
inline std::atomic<bool>     g_kaPendingRestoreGhost{false};
inline std::atomic<bool>     g_kaHasPedDispTemplate{false};
inline std::atomic<bool>     g_kaHasMeleeTemplate{false};

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
inline size_t      g_kaAuraCursor = 0;
inline std::string g_kaSelectedName;
inline char        g_kaSearchBuf[64]{};
inline SentHitRecord g_kaRecentHits[32]{};
inline int32_t     g_kaRecentWriteIdx = 0;

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
    g_kaStepActive = false;
    g_kaStepReturnAtMs = 0;
    g_kaCombatActive = false;
    g_kaCombatFinishAtMs = 0;
    g_kaCurrentTargetId = 0;
    g_kaAutoFinishUntilMs.store(0, std::memory_order_release);
    g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        g_kaTracked.clear();
    }
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

inline bool IsTargetConfirmedDead(int32_t elemId, int32_t playerId) {
    int64_t nowMs = GetMonotonicMs();
    if (elemId >= 1) {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        auto it = g_kaTracked.find(elemId);
        if (it != g_kaTracked.end()) {
            if (it->second.deadUntilMs > nowMs) return true;
            if (it->second.hp <= 0.0f && (nowMs - it->second.lastSeenMs) < 3000) return true;
        }
    }
    {
        std::lock_guard<std::mutex> lk(g_espMutex);
        for (const auto& sp : g_allServerPlayers) {
            if (playerId > 0 && sp.playerId == playerId) {
                if (sp.health <= 0 && sp.isStreamed) return true;
            }
        }
    }
    return false;
}

inline std::vector<NamedTargetCandidate> CollectNamedPlayersSnapshot() {
    std::vector<NamedTargetCandidate> out;
    int64_t nowMs = GetMonotonicMs();
    Vec3 myPos{};
    bool hasMyPos = false;
    int32_t myElemId = g_kaLocalElemId.load(std::memory_order_relaxed);
    int64_t localPed = Morpeh::GetLocalPedEntity();
    int32_t localPedId = (localPed > 0) ? Morpeh::UnpackEntityId(localPed) : -1;
    // Fast zero-icall local position read from Morpeh positionStash (avoids Transform icall stutter in UI)
    if (localPedId >= 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.positionStash)) {
        if (void* p = Morpeh::Get(Morpeh::g_stashes.positionStash, localPedId, sizeof(Vec3))) {
            if (SafeMem::ReadVal(p, myPos) && IsFiniteVec3(myPos) &&
                (std::fabs(myPos.x) > 0.25f || std::fabs(myPos.z) > 0.25f)) {
                hasMyPos = true;
            }
        }
    }
    if (!hasMyPos) hasMyPos = GetLocalPlayerPos(myPos);

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
    seenPeds.reserve(srvSnap.size() + 16);
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
        }
        if (myElemId >= 1 && c.elemId == myElemId) continue;

        if (c.elemId >= 1) {
            auto it = elemSnap.find(c.elemId);
            if (it != elemSnap.end()) {
                if (it->second.deadUntilMs > nowMs || (it->second.hp <= 0.0f && (nowMs - it->second.lastSeenMs) < 3000)) {
                    c.isDead = true;
                }
                if (it->second.isPed) {
                    if (!c.hasPos && IsFiniteVec3(it->second.pos) && (std::fabs(it->second.pos.x) > 0.25f || std::fabs(it->second.pos.z) > 0.25f)) {
                        c.pos = it->second.pos;
                        c.hasPos = true;
                    }
                    if (it->second.hp <= 0.0f) { c.hp = 0; c.isDead = true; }
                }
            }
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

        NamedTargetCandidate c{};
        c.playerId = (pl.playerId >= 0) ? pl.playerId : -eId;
        c.elemId   = eId;
        c.elemGen  = eGen;
        c.pedEnt   = pl.pedEntityVal;
        c.name     = !pl.name.empty() ? pl.name : ("Ped #" + std::to_string(eId));
        c.hp       = pl.health;
        c.pos      = pl.worldPos;
        c.hasPos   = true;
        c.isNpc    = (pl.playerId < 0);
        c.isDead   = (pl.dead || pl.health <= 0);
        auto it = elemSnap.find(eId);
        if (it != elemSnap.end() && it->second.deadUntilMs > nowMs) c.isDead = true;
        c.dist     = hasMyPos ? Vec3Dist(myPos, pl.worldPos) : 99999.0f;
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

inline bool LookupTargetPosition(int32_t elemId, int32_t playerId, Vec3& outPos) {
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
            if (playerId > 0 && rp.playerId == playerId) {
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

    if (isRestore) {
        if (!g_kaHasActiveGhost) return;
        g_kaGhostHoldUntilMs.store(0, std::memory_order_release);
        g_kaHasActiveGhost = false;
        g_kaPendingRestoreGhost.store(false, std::memory_order_release);

        float rdx = realPos.x - g_kaLastGhostSentPos.x;
        float rdy = realPos.y - g_kaLastGhostSentPos.y;
        float rdz = realPos.z - g_kaLastGhostSentPos.z;
        float backDist = std::sqrt(rdx * rdx + rdy * rdy + rdz * rdz);
        int hops = std::clamp(static_cast<int>(std::ceil(backDist / chunk)), 1, 8);
        for (int i = 1; i <= hops; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(hops);
            baseTime += 0.025;
            pkt.pos_x = g_kaLastGhostSentPos.x + rdx * t;
            pkt.pos_y = g_kaLastGhostSentPos.y + rdy * t;
            pkt.pos_z = g_kaLastGhostSentPos.z + rdz * t;
            pkt.remote_time = baseTime;
            SendDatagram(Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram", &pkt, sizeof(pkt));
        }
        g_kaLastRemoteTime = baseTime;
        g_kaLastGhostSentPos = realPos;
        g_kaGhostOverridePos = realPos;
        return;
    }

    Vec3 destPos = ComputeMeleeStandPosition(realPos, targetPos, g_cfg.ghostHitOffsetM);
    Vec3 startPos = g_kaHasActiveGhost ? g_kaLastGhostSentPos : realPos;
    float dx = destPos.x - startPos.x;
    float dy = destPos.y - startPos.y;
    float dz = destPos.z - startPos.z;
    float segDist = std::sqrt(dx * dx + dy * dy + dz * dz);
    int hops = std::clamp(static_cast<int>(std::ceil(segDist / chunk)), 1, g_cfg.maxGhostHopsPerStrike);
    for (int i = 1; i <= hops; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(hops);
        baseTime += 0.025;
        pkt.pos_x = startPos.x + dx * t;
        pkt.pos_y = startPos.y + dy * t;
        pkt.pos_z = startPos.z + dz * t;
        pkt.remote_time = baseTime;
        SendDatagram(Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram", &pkt, sizeof(pkt));
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
    Vec3 standPos = ComputeMeleeStandPosition(g_kaStepOriginPos, targetPos, g_cfg.stepApproachOffsetM);
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

inline void ComputeAimDirection(int32_t targetElemId, int32_t targetPlayerId, Vec3& outDir, Vec3& outHitPos) {
    outDir = { 0.0f, 0.0f, 1.0f };
    outHitPos = {};
    Vec3 myPos{}, tPos{};
    if (LookupTargetPosition(targetElemId, targetPlayerId, tPos)) {
        outHitPos = { tPos.x, tPos.y + 0.9f, tPos.z };
        if (GetLocalPlayerPos(myPos)) {
            outDir = Vec3Norm({ tPos.x - myPos.x, (tPos.y + 0.9f) - (myPos.y + 1.0f), tPos.z - myPos.z });
        }
    }
}

inline void EndCombatModeIfNeeded(int64_t nowMs, bool forceNow) {
    if (!g_kaCombatActive) return;
    if (!forceNow && nowMs < g_kaCombatFinishAtMs) return;
    g_kaCombatActive = false;
    g_kaCombatFinishAtMs = 0;
    SendDatagram(Offsets::g_cfg.nsMeleeDatagrams.c_str(), "MeleeCombatFinishDatagram", nullptr, 0);
}

inline int KillSingleTargetBurst(int32_t targetElemId, int32_t targetGen, int32_t targetPlayerId, int hitsCount) {
    if (targetElemId < 1 || !ResolveLocalElementId()) return 0;
    int32_t myId  = g_kaLocalElemId.load(std::memory_order_acquire);
    int32_t myGen = g_kaLocalElemGen.load(std::memory_order_acquire);
    if (myId < 1 || targetElemId == myId) return 0;

    int64_t nowMs = GetMonotonicMs();
    g_kaRecentHits[(g_kaRecentWriteIdx++) & 31] = { targetElemId, nowMs };

    if (!g_kaCombatActive) {
        if (SendDatagram(Offsets::g_cfg.nsMeleeDatagrams.c_str(), "MeleeCombatModeStartDatagram", nullptr, 0)) {
            g_kaCombatActive = true;
        }
    }
    g_kaCombatFinishAtMs = nowMs + 450;

    Vec3 dir{}, hitPos{};
    ComputeAimDirection(targetElemId, targetPlayerId, dir, hitPos);

    int sent = 0;
    {
        MeleeWeaponAttackDatagramPacket pkt{};
        if (g_kaHasMeleeTemplate.load(std::memory_order_acquire)) {
            CopyRawBytes(&pkt, g_kaMeleeTemplate, sizeof(pkt));
        }
        pkt.attacker_id  = myId;
        pkt.attacker_gen = myGen;
        pkt.target_id    = targetElemId;
        pkt.target_gen   = targetGen > 0 ? targetGen : 1;
        pkt.damage       = 1000;
        pkt.trace_x      = dir.x;
        pkt.trace_y      = dir.y;
        pkt.trace_z      = dir.z;
        pkt.attack_type  = 2;
        pkt.is_ko        = 1;
        pkt.timestamp    = GenerateComboTimestamp();
        if (SendDatagram(Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackDatagram", &pkt, sizeof(pkt))) {
            ++sent;
        }
    }

    for (int i = 0; i < hitsCount; ++i) {
        MeleeWeaponAttackRequestDatagram req{};
        req.attacker_id  = myId;
        req.attacker_gen = myGen;
        req.target_id    = targetElemId;
        req.target_gen   = targetGen > 0 ? targetGen : 1;
        req.hit_height   = 0;
        req.hit_dir      = 2;
        req.attack_type  = (i % 3 == 2) ? 2 : 0;
        req.trace_x      = dir.x;
        req.trace_y      = dir.y;
        req.trace_z      = dir.z;
        req.is_ko        = 1;
        req.ts30         = GenerateComboTimestamp();
        req.ts38         = 0;

        g_kaMeleeOursFlag.store(1, std::memory_order_release);
        if (SendDatagram(Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackRequestDatagram", &req, sizeof(req))) {
            ++sent;
        }
        g_kaMeleeOursFlag.store(0, std::memory_order_release);
    }

    {
        MeleeFinisherDatagram fin{};
        fin.attacker_id  = myId;
        fin.attacker_gen = myGen;
        fin.target_id    = targetElemId;
        fin.target_gen   = targetGen > 0 ? targetGen : 1;
        fin.pos_x        = hitPos.x;
        fin.pos_y        = hitPos.y;
        fin.pos_z        = hitPos.z;
        fin.dir_x        = dir.x;
        fin.dir_y        = dir.y;
        fin.dir_z        = dir.z;
        if (SendDatagram(Offsets::g_cfg.nsMeleeDatagrams.c_str(), "MeleeFinisherDatagram", &fin, sizeof(fin))) {
            ++sent;
        }
    }

    g_kaTargetSentHits += sent;
    return sent;
}

inline bool EngageRangeForTargetIfNeeded(int32_t elemId, int32_t playerId, int64_t nowMs) {
    if (!g_cfgState.bKillAroundGhostStep && !g_cfgState.bKillAroundMicroStep) return false;
    Vec3 myPos{}, tPos{};
    if (!GetLocalPlayerPos(myPos) || !LookupTargetPosition(elemId, playerId, tPos)) return false;
    float dist = Vec3Dist(myPos, tPos);
    g_kaCurrentTargetDist = dist;
    if (dist <= g_cfg.directMeleeRangeM || dist > g_cfgState.killAroundRadius) return false;
    if (g_cfgState.bKillAroundMicroStep) PerformPhysicalStepTo(tPos, nowMs);
    if (g_cfgState.bKillAroundGhostStep) SendGhostPedDisplacement(tPos, false);
    return true;
}

inline void RequestKillTarget(int32_t playerId, int32_t elemId, int32_t elemGen, const std::string& name) {
    g_kaSelectedPlayerId.store(playerId, std::memory_order_release);
    g_kaCurrentTargetId = elemId;
    g_kaCurrentTargetGen = elemGen > 0 ? elemGen : 1;
    g_kaTargetSentHits = 0;
    g_kaTargetStartSrvAcc = g_kaSrvAcceptedCount;
    g_kaSelectedName = name;

    g_kaPendingKillPlayerId.store(playerId, std::memory_order_release);
    g_kaPendingKillElemGen.store(elemGen > 0 ? elemGen : 1, std::memory_order_release);
    g_kaPendingKillElemId.store(elemId, std::memory_order_release);
    g_kaAutoFinishUntilMs.store(GetMonotonicMs() + g_cfg.autoFinishWindowMs, std::memory_order_release);
}

inline void RequestKillAllAroundOnce() {
    g_kaKillAllBurstUntilMs.store(GetMonotonicMs() + g_cfg.killAllBurstMs, std::memory_order_release);
}

inline void StopAll() {
    g_cfgState.bKillAroundAutoRepeat = false;
    g_cfgState.bKillAroundAura = false;
    g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
    g_kaAutoFinishUntilMs.store(0, std::memory_order_release);
    g_kaGhostHoldUntilMs.store(0, std::memory_order_release);
    g_kaPendingRestoreGhost.store(true, std::memory_order_release);
    g_kaCurrentTargetId = 0;
    g_kaSelectedPlayerId.store(0, std::memory_order_release);
}

// Datagram feeders invoked by FeatureModules hooks
inline void OnPedDisplacementRead(const void* rawPkt) {
    if (!SafeMem::IsPlausiblePtr(rawPkt)) return;
    PedDisplacementSyncDatagramPacket pkt{};
    if (!SafeMem::ReadVal(rawPkt, pkt) || pkt.remote_id < 1) return;
    Vec3 p{ pkt.pos_x, pkt.pos_y, pkt.pos_z };
    if (!IsFiniteVec3(p) || (std::fabs(p.x) < 0.1f && std::fabs(p.z) < 0.1f)) return;
    int64_t nowMs = GetMonotonicMs();
    std::lock_guard<std::mutex> lk(g_kaMutex);
    TrackedElement& te = g_kaTracked[pkt.remote_id];
    te.id = pkt.remote_id;
    if (pkt.remote_gen > 0) te.gen = pkt.remote_gen;
    te.pos = p;
    te.lastSeenMs = nowMs;
    te.isPed = true;
}

inline void OnPedDisplacementWrite(void* rawPkt) {
    if (!SafeMem::IsPlausiblePtr(rawPkt)) return;
    PedDisplacementSyncDatagramPacket inPkt{};
    if (!SafeMem::ReadVal(rawPkt, inPkt)) return;
    if (inPkt.remote_id >= 1) {
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
        SafeMem::Write(rawPkt, &inPkt, sizeof(inPkt));
    }
}

inline void OnPedDeadRead(const void* rawPkt) {
    if (!SafeMem::IsPlausiblePtr(rawPkt)) return;
    ElementIdModelVal eid{};
    if (!SafeMem::ReadVal(rawPkt, eid) || eid.id < 1) return;
    int64_t nowMs = GetMonotonicMs();
    {
        std::lock_guard<std::mutex> lk(g_kaMutex);
        TrackedElement& te = g_kaTracked[eid.id];
        te.id = eid.id;
        te.hp = 0.0f;
        te.lastDeadMs = nowMs;
        te.deadUntilMs = nowMs + g_cfg.deadCooldownMs;
    }
    g_kaSrvLastDeadId = eid.id;
    g_kaSrvLastDeadMs = nowMs;
}

inline void OnMeleeAttackRead(const void* rawPkt) {
    if (!SafeMem::IsPlausiblePtr(rawPkt)) return;
    MeleeWeaponAttackDatagramPacket pkt{};
    if (!SafeMem::ReadVal(rawPkt, pkt)) return;
    CopyRawBytes(g_kaMeleeTemplate, &pkt, sizeof(g_kaMeleeTemplate));
    g_kaHasMeleeTemplate.store(true, std::memory_order_release);
    int32_t myId = g_kaLocalElemId.load(std::memory_order_relaxed);
    if (myId >= 1 && pkt.attacker_id == myId) ++g_kaSrvAcceptedCount;
}

inline void OnMeleeRequestWrite(void* rawPkt) {
    if (!SafeMem::IsPlausiblePtr(rawPkt)) return;
    MeleeWeaponAttackRequestDatagram req{};
    if (!SafeMem::ReadVal(rawPkt, req)) return;
    if (req.attacker_id >= 1) {
        g_kaLocalElemId.store(req.attacker_id, std::memory_order_release);
        if (req.attacker_gen > 0) g_kaLocalElemGen.store(req.attacker_gen, std::memory_order_release);
    }
    if (g_cfgState.bKillAroundOneHit && g_kaMeleeOursFlag.load(std::memory_order_relaxed) == 0 &&
        (g_cfgState.bKillAroundAutoRepeat || g_cfgState.bKillAroundAura || g_kaCurrentTargetId > 0)) {
        req.is_ko = 1;
        SafeMem::Write(rawPkt, &req, sizeof(req));
    }
}

inline void Tick() {
    if (g_cfgState.bMasterDisable || !Morpeh::g_stashes.worldReady) return;
    EnsureWorldFreshness();
    ResolveLocalElementId();

    int64_t nowMs = GetMonotonicMs();

    if (g_kaPendingRestoreGhost.exchange(false, std::memory_order_acq_rel)) {
        Vec3 myPos{};
        if (GetLocalPlayerPos(myPos)) SendGhostPedDisplacement(myPos, true);
        else {
            g_kaHasActiveGhost = false;
            g_kaGhostHoldUntilMs.store(0, std::memory_order_release);
        }
        PerformStepReturnIfFinished(nowMs, true);
        EndCombatModeIfNeeded(nowMs, true);
    }

    int32_t reqElem = g_kaPendingKillElemId.exchange(0, std::memory_order_acq_rel);
    int32_t reqPid  = g_kaPendingKillPlayerId.load(std::memory_order_acquire);
    int32_t reqGen  = g_kaPendingKillElemGen.load(std::memory_order_acquire);
    if (reqElem >= 1) {
        EngageRangeForTargetIfNeeded(reqElem, reqPid, nowMs);
        KillSingleTargetBurst(reqElem, reqGen, reqPid, g_cfg.singleBurstHits);
        return;
    }

    bool killAllBurst   = (nowMs < g_kaKillAllBurstUntilMs.load(std::memory_order_acquire));
    bool auraActive     = g_cfgState.bKillAroundAura || killAllBurst;
    bool autoFinishWnd  = (nowMs < g_kaAutoFinishUntilMs.load(std::memory_order_acquire));
    bool singleFollowOn = (g_cfgState.bKillAroundAutoRepeat || autoFinishWnd) && (g_kaCurrentTargetId >= 1);
    bool kaTabVisible   = g_menuIsOpenAtomic.load(std::memory_order_relaxed) &&
                          g_activeTabAtomic.load(std::memory_order_relaxed) == 0 &&
                          g_activeSubTabAtomic.load(std::memory_order_relaxed) == 3;

    if (!auraActive && !singleFollowOn && !g_kaHasActiveGhost && !g_kaStepActive && !g_kaCombatActive && !kaTabVisible) {
        return;
    }

    static int64_t s_lastUiNearMs = 0;
    std::vector<NamedTargetCandidate> auraTargets;
    if (auraActive || (kaTabVisible && (nowMs - s_lastUiNearMs) >= 700)) {
        s_lastUiNearMs = nowMs;
        auraTargets = CollectNearbyAuraTargets(g_cfgState.killAroundRadius, g_cfgState.bKillAroundIncludeNpcs);
    }

    bool usedRangeAssist = false;
    if (auraActive) {
        if (!auraTargets.empty()) {
            static int64_t s_lastSweepMs = 0;
            if ((nowMs - s_lastSweepMs) >= g_cfg.auraSweepIntervalMs) {
                s_lastSweepMs = nowMs;
                int closeHits = 0;
                bool sentRanged = false;
                size_t count = auraTargets.size();
                if (g_kaAuraCursor >= count) g_kaAuraCursor = 0;

                for (size_t step = 0; step < count; ++step) {
                    size_t idx = (g_kaAuraCursor + step) % count;
                    const auto& t = auraTargets[idx];
                    if (t.dist <= g_cfg.directMeleeRangeM) {
                        if (closeHits < g_cfg.auraMaxClosePerTick) {
                            KillSingleTargetBurst(t.elemId, t.elemGen, t.playerId, g_cfg.auraHitsPerTarget);
                            ++closeHits;
                        }
                    } else if (!sentRanged && (g_cfgState.bKillAroundGhostStep || g_cfgState.bKillAroundMicroStep)) {
                        if (g_cfgState.bKillAroundMicroStep) {
                            PerformPhysicalStepTo(t.pos, nowMs);
                            usedRangeAssist = true;
                        }
                        if (g_cfgState.bKillAroundGhostStep) {
                            SendGhostPedDisplacement(t.pos, false);
                            usedRangeAssist = true;
                        }
                        KillSingleTargetBurst(t.elemId, t.elemGen, t.playerId, g_cfg.singleBurstHits);
                        sentRanged = true;
                        g_kaAuraCursor = (idx + 1) % count;
                    }
                }
                if (!sentRanged && count > 0) g_kaAuraCursor = (g_kaAuraCursor + 1) % count;
                g_kaAuraSweepCount.fetch_add(1, std::memory_order_relaxed);
            } else if (g_kaHasActiveGhost || g_kaStepActive) {
                usedRangeAssist = true;
            }
        } else if (killAllBurst) {
            g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
        }
    }

    if (singleFollowOn) {
        int32_t activeElem = g_kaCurrentTargetId;
        int32_t activePid  = g_kaSelectedPlayerId.load(std::memory_order_acquire);
        if (IsTargetConfirmedDead(activeElem, activePid)) {
            g_kaAutoFinishUntilMs.store(0, std::memory_order_release);
        } else {
            static int64_t s_lastFollowMs = 0;
            if ((nowMs - s_lastFollowMs) >= g_cfg.singleFollowIntervalMs) {
                s_lastFollowMs = nowMs;
                if (EngageRangeForTargetIfNeeded(activeElem, activePid, nowMs)) usedRangeAssist = true;
                KillSingleTargetBurst(activeElem, g_kaCurrentTargetGen, activePid, g_cfg.singleFollowHits);
            } else if (g_kaHasActiveGhost || g_kaStepActive) {
                usedRangeAssist = true;
            }
        }
    }

    if (!usedRangeAssist) {
        if (g_kaHasActiveGhost && nowMs >= g_kaGhostHoldUntilMs.load(std::memory_order_acquire)) {
            Vec3 myPos{};
            if (GetLocalPlayerPos(myPos)) SendGhostPedDisplacement(myPos, true);
            else {
                g_kaHasActiveGhost = false;
                g_kaGhostHoldUntilMs.store(0, std::memory_order_release);
            }
        }
        PerformStepReturnIfFinished(nowMs, !auraActive && !singleFollowOn);
    }
    EndCombatModeIfNeeded(nowMs, !auraActive && !singleFollowOn && nowMs >= g_kaCombatFinishAtMs);
}

inline void DrawPanel(float availW) {
    using Lang::T;
    int32_t myId  = g_kaLocalElemId.load(std::memory_order_relaxed);
    int32_t myGen = g_kaLocalElemGen.load(std::memory_order_relaxed);
    // Pure pointer check (ZERO IL2CPP reflection / SimpleDImple calls inside UI draw loop!)
    bool hasMcw   = (Morpeh::g_stashes.worldReady && Il2Cpp::IsUnityObjectSafe(Morpeh::g_stashes.realMetaClient));
    int32_t err   = g_kaLastSendError.load(std::memory_order_relaxed);
    int32_t nearN = g_kaLastNearbyCount.load(std::memory_order_relaxed);
    int32_t sweeps = g_kaAuraSweepCount.load(std::memory_order_relaxed);
    bool burstActive = (GetMonotonicMs() < g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed));

    ImGui::TextColored(
        (myId > 0 && hasMcw && err == 0) ? ImVec4(0.28f, 0.98f, 0.65f, 1.0f) : ImVec4(1.0f, 0.72f, 0.28f, 1.0f),
        "Elem=%d/%d | Net=%s | Near=%d | Sweeps=%d | E=%d",
        myId, myGen, hasMcw ? "OK" : "Wait", nearN, sweeps, err);

    char killAllBtn[160]{};
    snprintf(killAllBtn, sizeof(killAllBtn), "%s (%d)##ka_all",
             T("KILL EVERYONE AROUND ME", "قتل الجميع حولي الآن"), nearN);
    if (ThemeEngine::NeonActionButton(killAllBtn, std::max(180.0f, availW - 8.0f), 38.0f, (g_cfgState.bKillAroundAura || burstActive))) {
        RequestKillAllAroundOnce();
    }
    ImGui::Spacing();

    ThemeEngine::ProToggle(T("Auto Kill Aura (Continuous Range Sweep)", "أورا قتل تلقائي مستمر للجميع"), &g_cfgState.bKillAroundAura);
    ThemeEngine::ProToggle(T("Ghost Server-Step (Kill In Range Without Walking)", "غوست سيرفر — القتل داخل المدى بدون مشي"), &g_cfgState.bKillAroundGhostStep);
    ThemeEngine::ProToggle(T("Physical Micro-Step + Auto Return", "تقرب فيزيائي مؤقت ثم الرجوع"), &g_cfgState.bKillAroundMicroStep);
    ThemeEngine::ProToggle(T("Include Nearby NPCs / Unmapped Peds", "شمل القريبين والبوتات بالمدى"), &g_cfgState.bKillAroundIncludeNpcs);
    ThemeEngine::ProToggle(T("Auto-Repeat Until Selected Target Dies", "متابعة الضرب حتى موت الهدف المحدد"), &g_cfgState.bKillAroundAutoRepeat);
    ThemeEngine::ProToggle(T("One-Hit + Finisher Combo", "ضربة قاتلة + فينشر فوري"), &g_cfgState.bKillAroundOneHit);
    ThemeEngine::ProToggle(T("Show Nearby Players Only", "إظهار القريبين فقط في القائمة"), &g_cfgState.bKillAroundFilterNear);
    ThemeEngine::ProSlider(T("Kill / Filter Range", "مدى القتل والفلتر"), &g_cfgState.killAroundRadius, 5.0f, 120.0f, "%.0f m");

    int kaSrchAct = ThemeEngine::SearchPillButton("##ka_search", T("Tap to search player name or ID...", "اضغط للبحث باسم اللاعب أو الأيدي..."), g_kaSearchBuf);
    if (kaSrchAct == 1) {
        dispatch_async(dispatch_get_main_queue(), ^{
            UIViewController* hostVC = nil;
            for (UIScene* s in UIApplication.sharedApplication.connectedScenes) {
                if ([s isKindOfClass:[UIWindowScene class]]) {
                    for (UIWindow* w in ((UIWindowScene*)s).windows) {
                        if (!w.hidden && w.rootViewController && w.windowLevel >= UIWindowLevelAlert) {
                            hostVC = w.rootViewController;
                            break;
                        }
                    }
                }
                if (hostVC) break;
            }
            if (!hostVC || hostVC.presentedViewController) return;
            bool ar = Lang::IsArabic();
            UIAlertController* ac = [UIAlertController alertControllerWithTitle:(ar ? @"بحث عن لاعب" : @"Search Players")
                                                                        message:nil
                                                                 preferredStyle:UIAlertControllerStyleAlert];
            if (@available(iOS 13.0, *)) ac.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
            [ac addTextFieldWithConfigurationHandler:^(UITextField* tf) {
                tf.placeholder = ar ? @"اكتب اسم اللاعب أو الأيدي..." : @"Enter player name or ID...";
                tf.text = [NSString stringWithUTF8String:g_kaSearchBuf] ?: @"";
                tf.autocapitalizationType = UITextAutocapitalizationTypeNone;
                tf.autocorrectionType = UITextAutocorrectionTypeNo;
                tf.clearButtonMode = UITextFieldViewModeWhileEditing;
            }];
            [ac addAction:[UIAlertAction actionWithTitle:(ar ? @"إلغاء" : @"Cancel") style:UIAlertActionStyleCancel handler:nil]];
            [ac addAction:[UIAlertAction actionWithTitle:(ar ? @"مسح" : @"Clear") style:UIAlertActionStyleDestructive handler:^(UIAlertAction*) {
                g_kaSearchBuf[0] = '\0';
            }]];
            [ac addAction:[UIAlertAction actionWithTitle:(ar ? @"تطبيق" : @"Apply") style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
                NSString* txt = ac.textFields.firstObject.text ?: @"";
                const char* utf = txt.UTF8String;
                std::snprintf(g_kaSearchBuf, sizeof(g_kaSearchBuf), "%s", utf ? utf : "");
            }]];
            [hostVC presentViewController:ac animated:YES completion:nil];
        });
    } else if (kaSrchAct == 2) {
        g_kaSearchBuf[0] = '\0';
    }

    static std::vector<NamedTargetCandidate> s_cached;
    static std::vector<size_t> s_visibleIdx;
    static bool s_hasCachedOnce = false;
    static int64_t s_lastUiMs = 0;
    static std::string s_lastFilterKey;
    static bool s_lastNearOnly = false;
    static float s_lastRadius = -1.0f;

    int64_t nowUi = GetMonotonicMs();
    bool refreshed = false;
    if (!s_hasCachedOnce || (nowUi - s_lastUiMs) >= 650) {
        s_hasCachedOnce = true;
        s_lastUiMs = nowUi;
        s_cached = CollectNamedPlayersSnapshot();
        refreshed = true;
    }

    if (refreshed ||
        s_lastNearOnly != g_cfgState.bKillAroundFilterNear ||
        std::fabs(s_lastRadius - g_cfgState.killAroundRadius) > 0.05f ||
        s_lastFilterKey != g_kaSearchBuf) {
        s_lastNearOnly = g_cfgState.bKillAroundFilterNear;
        s_lastRadius   = g_cfgState.killAroundRadius;
        s_lastFilterKey = g_kaSearchBuf;
        s_visibleIdx.clear();
        s_visibleIdx.reserve(s_cached.size());
        for (size_t i = 0; i < s_cached.size(); ++i) {
            const auto& c = s_cached[i];
            if (g_cfgState.bKillAroundFilterNear && (!c.hasPos || c.dist > g_cfgState.killAroundRadius)) continue;
            if (g_kaSearchBuf[0] != '\0') {
                char pidBuf[24];
                snprintf(pidBuf, sizeof(pidBuf), "%d", c.playerId);
                if (!ThemeEngine::CaseInsensitiveContains(c.name.c_str(), g_kaSearchBuf) &&
                    !ThemeEngine::CaseInsensitiveContains(pidBuf, g_kaSearchBuf)) {
                    continue;
                }
            }
            s_visibleIdx.push_back(i);
        }
    }

    int32_t selPid = g_kaSelectedPlayerId.load(std::memory_order_relaxed);
    ImGui::BeginChild("##ka_list", ImVec2(0.0f, 168.0f), true);
    if (s_visibleIdx.empty()) {
        ImGui::TextDisabled("%s", T("No players in list — stand near players or turn off Nearby filter.",
                                    "لا يوجد لاعبون الآن — اقترب أو أوقف فلتر القريبين."));
    } else {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(s_visibleIdx.size()), 28.0f);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto& c = s_cached[s_visibleIdx[static_cast<size_t>(row)]];
                ImGui::PushID(row);
                bool isSel = (selPid != 0 && selPid == c.playerId);
                bool dead = c.isDead || (c.hp <= 0);
                ImVec4 rowCol = isSel ? ImVec4(1.0f, 0.42f, 0.48f, 1.0f)
                                      : (c.elemId > 0 ? ImVec4(0.94f, 0.96f, 1.0f, 1.0f) : ImVec4(0.70f, 0.72f, 0.78f, 1.0f));
                if (c.hasPos) ImGui::TextColored(rowCol, "%s [%.1fm]%s", c.name.c_str(), c.dist, dead ? " [DEAD]" : "");
                else          ImGui::TextColored(rowCol, "%s [#%d]%s", c.name.c_str(), c.playerId, dead ? " [DEAD]" : "");
                ImGui::SameLine(std::max(190.0f, availW - 104.0f));
                if (c.elemId >= 1) {
                    if (ImGui::Button(T("KILL##ka_k", "قتل##ka_k"), ImVec2(84.0f, 24.0f))) {
                        RequestKillTarget(c.playerId, c.elemId, c.elemGen, c.name);
                    }
                } else {
                    ImGui::TextDisabled("-");
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();

    if (g_kaCurrentTargetId > 0 || g_cfgState.bKillAroundAura || burstActive) {
        ImGui::Spacing();
        if (g_kaCurrentTargetId > 0) {
            bool deadNow = IsTargetConfirmedDead(g_kaCurrentTargetId, selPid);
            ImGui::TextColored(deadNow ? ImVec4(0.35f, 1.0f, 0.55f, 1.0f) : ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
                               "%s (#%d) | %.1fm | Hits=%d | Acc=%d | %s",
                               g_kaSelectedName.empty() ? "Target" : g_kaSelectedName.c_str(),
                               g_kaCurrentTargetId, g_kaCurrentTargetDist,
                               g_kaTargetSentHits, g_kaSrvAcceptedCount - g_kaTargetStartSrvAcc,
                               deadNow ? "DEAD" : "TARGETED");
            if (ThemeEngine::NeonActionButton(T("Kill Selected Again##ka_again", "قتل المحدد مرة أخرى##ka_again"), availW * 0.60f, 30.0f, false)) {
                RequestKillTarget(selPid, g_kaCurrentTargetId, g_kaCurrentTargetGen, g_kaSelectedName);
            }
            ImGui::SameLine();
        }
        if (ThemeEngine::NeonActionButton(T("Stop All##ka_stop", "إيقاف الكل##ka_stop"), availW * 0.34f, 30.0f, true)) {
            StopAll();
        }
    }
}

} // namespace KillAround
