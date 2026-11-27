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
#include <vector>

// KillAround.h — Direct iOS arm64 port of kill_around (2).h + mainTOW.cpp
// Runs all engine/IL2CPP operations strictly on UnityMain (Tick), protected by TCFlyGuard + SafeMem.
// Menu (DrawPanel) reads only cached game-thread snapshots.

namespace KillAround {

static constexpr float   kMeleeReach     = 4.2f;
static constexpr int64_t kDeadHoldMs     = 15000;
static constexpr int32_t kOneHitDamage   = 100000;

inline bool                 g_OutOfReach    = false;
inline int32_t              g_SelectedId    = 0;
inline std::atomic<int64_t> g_kaKillAllBurstUntilMs{0}; // g_BurstUntilMs

struct EntityId {
    int32_t Id;
    int32_t Gen;
};

#pragma pack(push, 1)
// MeleeWeaponAttackRequestDatagram (0x38)
struct RequestDatagram {
    EntityId element;
    EntityId target;
    int32_t  heightType;
    int32_t  direction;
    int32_t  attackType;
    float    trace[3];
    uint8_t  isKo;
    uint8_t  pad[7];
    uint64_t timestamp;
};
static_assert(sizeof(RequestDatagram) == 0x38, "RequestDatagram layout");

// MeleeWeaponAttackDatagram (0x40)
struct MeleeHitDatagram {
    EntityId element;
    EntityId target;
    int32_t  damage;
    int32_t  damageToStamina;
    float    trace[3];
    int32_t  heightType;
    int32_t  direction;
    int32_t  attackType;
    uint8_t  isKo;
    uint8_t  pad[7];
    uint64_t timestamp;
};
static_assert(sizeof(MeleeHitDatagram) == 0x40, "MeleeHitDatagram layout");

// MeleeFinisherDatagram (0x28)
struct FinisherDatagram {
    EntityId performer;
    EntityId target;
    float    hitPos[3];
    float    dir[3];
};
static_assert(sizeof(FinisherDatagram) == 0x28, "FinisherDatagram layout");

// PedDisplacementSyncDatagram (0x40)
struct DisplacementDatagram {
    EntityId remoteId;
    float    pos[3];
    float    rot[4];
    float    vel[3];
    double   remoteTime;
    uint8_t  isClose;
    uint8_t  pad[7];
};
static_assert(sizeof(DisplacementDatagram) == 0x40, "DisplacementDatagram layout");
#pragma pack(pop)

inline int64_t GetMonotonicMs() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000LL + static_cast<int64_t>(ts.tv_nsec / 1000000LL);
}

inline uint64_t ComboTimestamp() {
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t ticks = static_cast<uint64_t>(ts.tv_sec) * 10000000ULL +
                     static_cast<uint64_t>(ts.tv_nsec) / 100ULL +
                     0x089f7ff5f7b58000ULL;
    return (ticks & 0x3fffffffffffffffULL) | 0x4000000000000000ULL;
}

struct Tracked {
    Vec3    pos{0.0f, 0.0f, 0.0f};
    int32_t gen = 1;
    int64_t seenMs = 0;
    int64_t deadUntilMs = 0;
};

inline std::mutex                              g_Mtx;
inline std::unordered_map<int32_t, Tracked>    g_Elems;
inline std::unordered_map<int32_t, std::string> g_ElemNames;

inline std::atomic<int32_t> g_LocalId{0};
inline std::atomic<int32_t> g_LocalGen{1};
inline std::atomic<int32_t> g_MeleeOwnFlag{0};
inline int64_t              g_LastDeadMs = 0;
inline int32_t              g_LastDeadId = 0;
inline std::atomic<int32_t> g_AcceptedHits{0};
inline std::atomic<int32_t> g_DispHits{0};
inline std::atomic<int32_t> g_DeadHits{0};
inline std::atomic<int32_t> g_WriteHits{0};
inline std::atomic<int32_t> g_AttackWrites{0};
inline std::atomic<int32_t> g_SendCalls{0};
inline std::atomic<int32_t> g_SendOk{0};
inline std::atomic<int32_t> g_SendFail{0};

inline uint8_t              g_MeleeTemplate[0x40]{};
inline std::atomic<bool>    g_HasMeleeTemplate{false};

// Client capture (Hook_MetaPoll + GlobalContextCore.MetaClientWrapper + SimpleDImple)
inline std::atomic<void*>   g_metaClient{nullptr};
inline void*                g_KaClient = nullptr;
inline int64_t              g_KaClientTryMs = 0;

// Cached datagram Il2CppClass* for unbox normalization
inline void*                g_kaClsPedDisp  = nullptr;
inline void*                g_kaClsPedDead  = nullptr;
inline void*                g_kaClsMeleeAtk = nullptr;
inline void*                g_kaClsMeleeReq = nullptr;

struct SendEntry {
    const char* name = nullptr;
    uint32_t    methodGch = 0;
    void*       dgramKlass = nullptr;
    bool        failed = false;
};
inline SendEntry g_Senders[8]{};
inline int       g_SenderCount = 0;
inline void*     g_ObjectKlass = nullptr;
inline void*     g_MethodBaseInvoke = nullptr;

inline double    g_LastRemoteTime = 0.0;
inline bool      g_kaHasActiveGhost = false; // g_GhostActive
inline int64_t   g_GhostHoldUntil = 0;

inline int64_t   g_LastAuraMs = 0;
inline int64_t   g_LastFollowMs = 0;
inline std::unordered_map<int32_t, int64_t> g_AuraLast;
inline size_t    g_AuraCursor = 0;
inline int32_t   g_CurTargetGen = 1;
inline float     g_CurTargetDist = 0.0f;

inline std::mutex g_LocalPosMtx;
inline bool       g_LocalPosOk = false;
inline Vec3       g_LocalPosCache{};
inline int64_t    g_LocalPosMs = 0;

inline bool LookupName(int32_t elemId, std::string* out) {
    std::lock_guard<std::mutex> lk(g_Mtx);
    auto it = g_ElemNames.find(elemId);
    if (it == g_ElemNames.end() || it->second.empty()) return false;
    if (out) *out = it->second;
    return true;
}

inline void* ResolveMetaClientFromGlobals() {
    if (!Il2Cpp::Init()) return nullptr;
    if (void* c = Il2Cpp::ResolveStaticClassFieldPtr("dev.Scripts.GlobalContexts", "GlobalContextCore", "MetaClientWrapper")) {
        if (Il2Cpp::IsUnityObjectSafe(c)) return c;
    }
    return Morpeh::EnsureMetaClientResolved();
}

inline void* ClientForSend() {
    if (Il2Cpp::IsUnityObjectSafe(g_KaClient)) return g_KaClient;
    void* polled = g_metaClient.load(std::memory_order_acquire);
    if (Il2Cpp::IsUnityObjectSafe(polled)) {
        g_KaClient = polled;
        return g_KaClient;
    }
    const int64_t now = GetMonotonicMs();
    if (now - g_KaClientTryMs < 500) return nullptr;
    g_KaClientTryMs = now;
    void* c = ResolveMetaClientFromGlobals();
    if (Il2Cpp::IsUnityObjectSafe(c)) g_KaClient = c;
    return g_KaClient;
}

// Exact kill_around (2).h SendDatagramImpl using value_box + RuntimeMethodInfo.MakeGenericMethod + MethodBase.Invoke
inline bool SendDatagramImpl(const char* ns, const char* dgramName, const void* payload, size_t payloadSize) {
    void* client = ClientForSend();
    if (!Il2Cpp::IsUnityObjectSafe(client) || payloadSize > 0x200) return false;
    if (!Il2Cpp::Init() || !Il2Cpp::g_api.value_box || !Il2Cpp::g_api.array_new ||
        !Il2Cpp::g_api.method_get_object || !Il2Cpp::g_api.gchandle_new || !Il2Cpp::g_api.gchandle_get_target) {
        return false;
    }

    SendEntry* e = nullptr;
    for (int i = 0; i < g_SenderCount; ++i) {
        if (g_Senders[i].name && std::strcmp(g_Senders[i].name, dgramName) == 0) {
            e = &g_Senders[i];
            break;
        }
    }
    if (!e) {
        if (g_SenderCount >= 8) return false;
        e = &g_Senders[g_SenderCount++];
        e->name = dgramName;
    }
    if (e->failed) return false;

    uint8_t box[0x200]{};
    if (payload && payloadSize > 0) std::memcpy(box, payload, payloadSize);

    bool ok = false;
    TCFlyGuard::TryRun([&]() {
        Il2Cpp::EnsureThreadAttached();
        void* exc = nullptr;
        if (e->methodGch == 0) {
            void* wrapperK = Il2Cpp::FindClass("FlakeFlow", "MetaClientWrapper");
            e->dgramKlass  = Il2Cpp::FindClass(ns, dgramName);
            void* typeK    = Il2Cpp::FindClass("System", "Type");
            if (!g_ObjectKlass) g_ObjectKlass = Il2Cpp::FindClass("System", "Object");
            void* miK      = Il2Cpp::FindClass("System.Reflection", "MethodInfo");
            void* rmiK     = Il2Cpp::FindClass("System.Reflection", "RuntimeMethodInfo");
            void* mbK      = Il2Cpp::FindClass("System.Reflection", "MethodBase");
            if (!wrapperK || !e->dgramKlass || !typeK || !g_ObjectKlass || !miK || !mbK) {
                e->failed = true;
                return;
            }

            const void* sendM = Il2Cpp::g_api.class_get_method_from_name(wrapperK, "Send", 1);
            void* sendObj     = sendM ? Il2Cpp::g_api.method_get_object(sendM, wrapperK) : nullptr;
            void* makeGen     = rmiK ? Il2Cpp::g_api.class_get_method_from_name(rmiK, "MakeGenericMethod", 1) : nullptr;
            if (!makeGen) makeGen = Il2Cpp::g_api.class_get_method_from_name(miK, "MakeGenericMethod", 1);
            void* typeArr     = (sendObj && makeGen) ? Il2Cpp::g_api.array_new(typeK, 1) : nullptr;
            void* dgType      = (typeArr && Il2Cpp::g_api.class_get_type) ? Il2Cpp::g_api.class_get_type(e->dgramKlass) : nullptr;
            void* dgTypeObj   = (dgType && Il2Cpp::g_api.type_get_object) ? Il2Cpp::g_api.type_get_object(dgType) : nullptr;
            if (!typeArr || !dgTypeObj) {
                e->failed = true;
                return;
            }

            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(typeArr) + 0x20) = dgTypeObj;
            void* args[1] = { typeArr };
            void* concrete = Il2Cpp::g_api.runtime_invoke(makeGen, sendObj, args, &exc);
            if (!SafeMem::IsPlausiblePtr(concrete) || exc) {
                e->failed = true;
                return;
            }
            if (!g_MethodBaseInvoke) {
                g_MethodBaseInvoke = Il2Cpp::g_api.class_get_method_from_name(mbK, "Invoke", 2);
            }
            e->methodGch = Il2Cpp::g_api.gchandle_new(concrete, false);
            if (!e->methodGch || !g_MethodBaseInvoke) {
                e->failed = true;
                return;
            }
        }

        void* mObj = Il2Cpp::g_api.gchandle_get_target(e->methodGch);
        if (!Il2Cpp::IsUnityObjectSafe(mObj)) {
            e->methodGch = 0;
            return;
        }
        void* boxed  = Il2Cpp::g_api.value_box(e->dgramKlass, box);
        void* objArr = SafeMem::IsPlausiblePtr(boxed) ? Il2Cpp::g_api.array_new(g_ObjectKlass, 1) : nullptr;
        if (SafeMem::IsPlausiblePtr(boxed) && SafeMem::IsPlausiblePtr(objArr)) {
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(objArr) + 0x20) = boxed;
            void* invArgs[2] = { client, objArr };
            exc = nullptr;
            Il2Cpp::g_api.runtime_invoke(g_MethodBaseInvoke, mObj, invArgs, &exc);
            ok = (exc == nullptr);
        }
    });
    return ok;
}

inline bool SendDatagram(const char* ns, const char* dgramName, const void* payload, size_t payloadSize) {
    g_SendCalls.fetch_add(1, std::memory_order_relaxed);
    const bool ok = SendDatagramImpl(ns, dgramName, payload, payloadSize);
    (ok ? g_SendOk : g_SendFail).fetch_add(1, std::memory_order_relaxed);
    return ok;
}

inline bool SendComboHit(int32_t targetId, int32_t targetGen, bool isKo, uint64_t ts, const float* dir, int32_t attackType) {
    RequestDatagram r{};
    r.element    = { g_LocalId.load(std::memory_order_relaxed), g_LocalGen.load(std::memory_order_relaxed) };
    r.target     = { targetId, targetGen > 0 ? targetGen : 1 };
    r.heightType = 0;
    r.direction  = 2;
    r.attackType = attackType;
    r.trace[0]   = dir[0];
    r.trace[1]   = dir[1];
    r.trace[2]   = dir[2];
    r.isKo       = isKo ? 1 : 0;
    r.timestamp  = ts;
    g_MeleeOwnFlag.store(1, std::memory_order_release);
    bool ok = SendDatagram("dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams",
                           "MeleeWeaponAttackRequestDatagram", &r, sizeof(r));
    g_MeleeOwnFlag.store(0, std::memory_order_release);
    return ok;
}

inline bool SendFinisher(int32_t targetId, int32_t targetGen, const float* hitPos, const float* dir) {
    FinisherDatagram f{};
    f.performer = { g_LocalId.load(std::memory_order_relaxed), g_LocalGen.load(std::memory_order_relaxed) };
    f.target    = { targetId, targetGen > 0 ? targetGen : 1 };
    std::memcpy(f.hitPos, hitPos, sizeof(f.hitPos));
    std::memcpy(f.dir, dir, sizeof(f.dir));
    return SendDatagram("dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Datagrams",
                        "MeleeFinisherDatagram", &f, sizeof(f));
}

inline bool SendModeStart() {
    return SendDatagram("dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Datagrams",
                        "MeleeCombatModeStartDatagram", nullptr, 0);
}

inline bool SendOneHitRaw(int32_t targetId, int32_t targetGen, const float* dir) {
    MeleeHitDatagram m{};
    if (g_HasMeleeTemplate.load(std::memory_order_acquire)) {
        std::memcpy(&m, g_MeleeTemplate, sizeof(m));
    }
    m.element         = { g_LocalId.load(std::memory_order_relaxed), g_LocalGen.load(std::memory_order_relaxed) };
    m.target          = { targetId, targetGen > 0 ? targetGen : 1 };
    m.damage          = 1000;
    m.damageToStamina = 0;
    m.trace[0] = dir[0]; m.trace[1] = dir[1]; m.trace[2] = dir[2];
    m.heightType      = 0;
    m.direction       = 2;
    m.attackType      = 2;
    m.isKo            = 1;
    m.timestamp       = ComboTimestamp();
    return SendDatagram("dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams",
                        "MeleeWeaponAttackDatagram", &m, sizeof(m));
}

inline bool SendDisplacement(const Vec3& at, double remoteTime) {
    DisplacementDatagram d{};
    d.remoteId   = { g_LocalId.load(std::memory_order_relaxed), g_LocalGen.load(std::memory_order_relaxed) };
    d.pos[0] = at.x; d.pos[1] = at.y; d.pos[2] = at.z;
    d.rot[3] = 1.0f;
    d.remoteTime = remoteTime;
    d.isClose    = 1;
    return SendDatagram("dev.Scripts.Networking.IBNet.IBNet_Datagrams.ElementSync",
                        "PedDisplacementSyncDatagram", &d, sizeof(d));
}

inline bool LocalPos(Vec3& out) {
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped <= 0) return false;
    int64_t view = Morpeh::GetViewEntity(ped);
    if (view > 0) {
        if (void* tr = Morpeh::GetEntityTransform(view)) {
            if (Il2Cpp::SafeGetTransformPos(tr, out) && IsFiniteVec3(out) &&
                (std::fabs(out.x) > 0.1f || std::fabs(out.z) > 0.1f)) {
                return true;
            }
        }
    }
    int32_t pedId = Morpeh::UnpackEntityId(ped);
    if (pedId >= 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.positionStash)) {
        if (void* p = Morpeh::Get(Morpeh::g_stashes.positionStash, pedId, sizeof(Vec3))) {
            if (SafeMem::ReadVal(p, out) && IsFiniteVec3(out) &&
                (std::fabs(out.x) > 0.1f || std::fabs(out.z) > 0.1f)) {
                return true;
            }
        }
    }
    return false;
}

inline bool ElemPos(int32_t id, Vec3& out) {
    std::lock_guard<std::mutex> lk(g_Mtx);
    auto it = g_Elems.find(id);
    if (it == g_Elems.end()) return false;
    out = it->second.pos;
    return true;
}

inline bool IsDeadNow(int32_t id, int64_t now) {
    std::lock_guard<std::mutex> lk(g_Mtx);
    auto it = g_Elems.find(id);
    return it != g_Elems.end() && it->second.deadUntilMs > now;
}

struct Candidate {
    int32_t id;
    int32_t gen;
    float   dist;
    Vec3    pos;
    bool    dead;
};

inline std::vector<Candidate> Candidates(const Vec3& me, float radius, int64_t now, bool withDead = false) {
    std::vector<Candidate> out;
    int32_t myId = g_LocalId.load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(g_Mtx);
    for (const auto& kv : g_Elems) {
        const int32_t id = kv.first;
        const Tracked& t = kv.second;
        if (id < 1 || id == myId) continue;
        if ((now - t.seenMs) > 10000) continue;
        const bool dead = t.deadUntilMs > now;
        if (dead && !withDead) continue;
        const float dx = t.pos.x - me.x, dy = t.pos.y - me.y, dz = t.pos.z - me.z;
        const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d > radius) continue;
        out.push_back({ id, t.gen, d, t.pos, dead });
    }
    std::sort(out.begin(), out.end(), [](const Candidate& a, const Candidate& b) { return a.dist < b.dist; });
    return out;
}

inline Vec3 StandPos(const Vec3& me, const Vec3& tgt, float offset) {
    const float dx = tgt.x - me.x, dz = tgt.z - me.z;
    const float h = std::sqrt(dx * dx + dz * dz);
    if (h < 0.05f) return { tgt.x + offset, tgt.y, tgt.z };
    return { tgt.x - dx / h * offset, tgt.y, tgt.z - dz / h * offset };
}

inline void GhostTo(const Vec3& tgt, const Vec3& me) {
    Vec3 dest = StandPos(me, tgt, 0.45f);
    double base = g_LastRemoteTime > 0.0 ? g_LastRemoteTime : (GetMonotonicMs() / 1000.0);
    base += 0.025;
    g_LastRemoteTime = base;
    SendDisplacement(dest, base);
    g_kaHasActiveGhost = true;
    g_GhostHoldUntil = GetMonotonicMs() + 200;
}

inline void GhostRestore(const Vec3& me) {
    if (!g_kaHasActiveGhost) return;
    double base = g_LastRemoteTime + 0.025;
    g_LastRemoteTime = base;
    SendDisplacement(me, base);
    g_kaHasActiveGhost = false;
}

inline int Burst(int32_t id, int32_t gen, int hits) {
    if (id < 1 || g_LocalId.load(std::memory_order_relaxed) < 1) return 0;
    SendModeStart();
    Vec3 me{}, tp{};
    float dir[3] = { 0.0f, 0.0f, 1.0f };
    if (LocalPos(me) && ElemPos(id, tp)) {
        float dx = tp.x - me.x, dy = (tp.y + 0.9f) - (me.y + 1.0f), dz = tp.z - me.z;
        float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d > 0.01f) { dir[0] = dx / d; dir[1] = dy / d; dir[2] = dz / d; }
    }
    int sent = 0;
    if (g_cfgState.bKillAroundOneHit && SendOneHitRaw(id, gen, dir)) ++sent;
    for (int i = 0; i < hits; ++i) {
        if (SendComboHit(id, gen, true, ComboTimestamp(), dir, (i % 3 == 2) ? 2 : 0)) ++sent;
    }
    float hitPos[3] = { tp.x, tp.y + 0.9f, tp.z };
    if (SendFinisher(id, gen, hitPos, dir)) ++sent;
    return sent;
}

inline bool EnsureLocalIdCached() {
    if (g_LocalId.load(std::memory_order_relaxed) >= 1) return true;
    int64_t ped = Morpeh::GetLocalPedEntity();
    if (ped > 0 && SafeMem::IsPlausiblePtr(Morpeh::g_stashes.elementIdStash)) {
        int32_t pedId = Morpeh::UnpackEntityId(ped);
        EntityId eid{};
        if (pedId >= 0) {
            if (void* p = Morpeh::Get(Morpeh::g_stashes.elementIdStash, pedId, sizeof(EntityId))) {
                if (SafeMem::ReadVal(p, eid) && eid.Id >= 1) {
                    g_LocalId.store(eid.Id, std::memory_order_release);
                    g_LocalGen.store(eid.Gen > 0 ? eid.Gen : 1, std::memory_order_release);
                    return true;
                }
            }
        }
    }
    if (void* es = Morpeh::EnsureElementStorageResolved()) {
        void* esCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(es) : nullptr;
        size_t offId = Il2Cpp::FieldOffset(esCls, "_lastLocalElementId", Offsets::g_cfg.offElemStorageLastLocalId);
        EntityId eid{};
        if (SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(es) + offId, eid) && eid.Id >= 1) {
            g_LocalId.store(eid.Id, std::memory_order_release);
            g_LocalGen.store(eid.Gen > 0 ? eid.Gen : 1, std::memory_order_release);
            return true;
        }
    }
    return false;
}

// Syncs streamed Morpeh players into g_Elems + g_ElemNames on UnityMain (~6.5Hz)
// Guarantees targets & nicknames stay fresh on iOS even alongside network Read hooks.
inline void SyncStreamedRosterOnGameThread(int64_t now) {
    static int64_t s_lastSyncMs = 0;
    if (now - s_lastSyncMs < 150) return;
    s_lastSyncMs = now;
    if (!SafeMem::IsPlausiblePtr(Morpeh::g_stashes.elementIdStash)) return;

    int64_t localPed = Morpeh::GetLocalPedEntity();
    int32_t localPedId = (localPed > 0) ? Morpeh::UnpackEntityId(localPed) : -1;

    std::vector<ESPPlayer> snap;
    {
        std::lock_guard<std::mutex> lk(::g_espMutex);
        snap = ::g_renderPlayers;
    }
    if (snap.empty()) return;

    std::lock_guard<std::mutex> lk(g_Mtx);
    for (const auto& pl : snap) {
        if (pl.entityId < 0 || pl.entityId == localPedId) continue;
        if (pl.playerId < 0 && !g_cfgState.bKillAroundIncludeNpcs) continue;
        EntityId eid{};
        void* ep = Morpeh::Get(Morpeh::g_stashes.elementIdStash, pl.entityId, sizeof(EntityId));
        if (!ep || !SafeMem::ReadVal(ep, eid) || eid.Id < 1) continue;

        Tracked& t = g_Elems[eid.Id];
        t.pos = pl.worldPos;
        t.gen = eid.Gen > 0 ? eid.Gen : 1;
        t.seenMs = now;
        if (pl.health <= 0 && t.deadUntilMs < now + 3000) {
            t.deadUntilMs = now + 3000;
        }
        if (!pl.name.empty()) {
            g_ElemNames[eid.Id] = pl.name;
        }
    }
}

// ---------------- Datagram & System Hook Handlers ----------------
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
    if (SafeMem::IsPlausiblePtr(self) && !g_metaClient.load(std::memory_order_relaxed)) {
        g_metaClient.store(self, std::memory_order_release);
    }
}

inline int32_t PredictComboDamageOverride(int32_t /*attackType*/, bool isStaminaDamage, bool& outOverride) {
    outOverride = false;
    if (isStaminaDamage || g_cfgState.bMasterDisable) return 0;
    bool active = g_cfgState.bKillAroundOneHit &&
                  (g_cfgState.bKillAroundAutoRepeat || g_cfgState.bKillAroundAura ||
                   g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed) > GetMonotonicMs());
    if (active) {
        outOverride = true;
        return kOneHitDamage;
    }
    return 0;
}

inline void OnPedDisplacementRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_kaClsPedDisp, Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    DisplacementDatagram d{};
    if (!SafeMem::ReadVal(ptr, d) || d.remoteId.Id < 1) return;
    Vec3 p{ d.pos[0], d.pos[1], d.pos[2] };
    if (!IsFiniteVec3(p)) return;
    g_DispHits.fetch_add(1, std::memory_order_relaxed);
    if (std::isfinite(d.remoteTime) && d.remoteTime > g_LastRemoteTime) {
        g_LastRemoteTime = d.remoteTime;
    }
    const int64_t now = GetMonotonicMs();
    std::lock_guard<std::mutex> lk(g_Mtx);
    Tracked& t = g_Elems[d.remoteId.Id];
    t.pos    = p;
    t.gen    = d.remoteId.Gen > 0 ? d.remoteId.Gen : 1;
    t.seenMs = now;
}

inline void OnPedDeadRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_kaClsPedDead, Offsets::g_cfg.nsIBNetPedSync.c_str(), "PedDeadDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    int32_t id = 0;
    if (!SafeMem::ReadVal(ptr, id)) return;
    g_DeadHits.fetch_add(1, std::memory_order_relaxed);
    if (id < 1) return;
    const int64_t now = GetMonotonicMs();
    {
        std::lock_guard<std::mutex> lk(g_Mtx);
        Tracked& t = g_Elems[id];
        t.deadUntilMs = now + kDeadHoldMs;
    }
    g_LastDeadId = id;
    g_LastDeadMs = now;
}

inline void OnMeleeAttackRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_kaClsMeleeAtk, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    MeleeHitDatagram m{};
    if (!SafeMem::ReadVal(ptr, m)) return;
    std::memcpy(g_MeleeTemplate, &m, sizeof(g_MeleeTemplate));
    g_HasMeleeTemplate.store(true, std::memory_order_release);
    if (m.target.Id < 1) return;
    int32_t myId = g_LocalId.load(std::memory_order_relaxed);
    if (myId >= 1 && m.element.Id == myId) {
        g_AcceptedHits.fetch_add(1, std::memory_order_relaxed);
    }
}

inline void OnMeleeRequestWrite(void* rawPkt, void* /*writer*/ = nullptr) {
    void* ptr = NormalizeDatagramPtrMut(rawPkt, g_kaClsMeleeReq, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackRequestDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    RequestDatagram r{};
    if (!SafeMem::ReadVal(ptr, r)) return;
    g_WriteHits.fetch_add(1, std::memory_order_relaxed);
    if (r.element.Id >= 1) {
        g_LocalId.store(r.element.Id, std::memory_order_release);
        g_LocalGen.store(r.element.Gen > 0 ? r.element.Gen : 1, std::memory_order_release);
    }
    if (g_cfgState.bKillAroundOneHit && g_MeleeOwnFlag.load(std::memory_order_relaxed) == 0 &&
        (g_cfgState.bKillAroundAutoRepeat || g_cfgState.bKillAroundAura ||
         g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed) > 0)) {
        uint8_t ko = 1;
        SafeMem::Write(reinterpret_cast<uint8_t*>(ptr) + offsetof(RequestDatagram, isKo), &ko, 1);
    }
}

inline void OnMeleeAttackWrite(void* rawPkt, void* /*writer*/ = nullptr) {
    void* ptr = NormalizeDatagramPtrMut(rawPkt, g_kaClsMeleeAtk, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    MeleeHitDatagram m{};
    if (!SafeMem::ReadVal(ptr, m)) return;
    g_AttackWrites.fetch_add(1, std::memory_order_relaxed);
    if (!g_HasMeleeTemplate.load(std::memory_order_relaxed)) {
        std::memcpy(g_MeleeTemplate, &m, sizeof(g_MeleeTemplate));
        g_HasMeleeTemplate.store(true, std::memory_order_release);
    }
    if (m.element.Id >= 1) {
        g_LocalId.store(m.element.Id, std::memory_order_release);
        g_LocalGen.store(m.element.Gen > 0 ? m.element.Gen : 1, std::memory_order_release);
    }
}

// ---------------- UnityMain Tick (exact kill_around (2).h) ----------------
inline void Tick() {
    const int64_t tNow = GetMonotonicMs();

    // Refresh cached local position at 4 Hz for the menu (menu never touches engine)
    if (tNow - g_LocalPosMs >= 250) {
        g_LocalPosMs = tNow;
        Vec3 p{};
        const bool ok = LocalPos(p);
        std::lock_guard<std::mutex> lk(g_LocalPosMtx);
        g_LocalPosOk = ok;
        g_LocalPosCache = p;
    }

    // Keep streamed roster + nicknames synced when menu KillAround tab is visible or KillAround is active
    const bool menuViewingKa = g_menuIsOpenAtomic.load(std::memory_order_relaxed) &&
                               g_activeTabAtomic.load(std::memory_order_relaxed) == 0 &&
                               g_activeSubTabAtomic.load(std::memory_order_relaxed) == 3;
    const bool burstNow = g_kaKillAllBurstUntilMs.load(std::memory_order_acquire) > tNow;
    if (menuViewingKa || g_cfgState.bKillAroundAura || g_cfgState.bKillAroundAutoRepeat || burstNow) {
        SyncStreamedRosterOnGameThread(tNow);
    }

    // New local ped = relog / respawn into new world -> reset active session safely
    {
        int64_t curPed = Morpeh::GetLocalPedEntity();
        static int64_t s_lastPed = 0;
        if (curPed > 0 && s_lastPed > 0 && curPed != s_lastPed) {
            g_cfgState.bKillAroundAutoRepeat = false;
            g_cfgState.bKillAroundAura = false;
            g_SelectedId = 0;
            g_AuraLast.clear();
            g_kaHasActiveGhost = false;
            g_KaClient = nullptr;
            g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
            g_LocalId.store(0, std::memory_order_release);
        }
        if (curPed > 0) s_lastPed = curPed;
    }

    // Learn element id at 2 Hz when unknown
    {
        static int64_t s_lastIdTry = 0;
        if (g_LocalId.load(std::memory_order_relaxed) < 1 && (tNow - s_lastIdTry) >= 500) {
            s_lastIdTry = tNow;
            EnsureLocalIdCached();
        }
    }

    if (!g_cfgState.bKillAroundAutoRepeat && !g_cfgState.bKillAroundAura && !burstNow && !g_kaHasActiveGhost) {
        return;
    }
    if (!EnsureLocalIdCached()) return;

    Vec3 me{};
    if (!LocalPos(me)) return;

    const float radius = std::clamp(g_cfgState.killAroundRadius, 5.0f, 120.0f);
    const bool aura = g_cfgState.bKillAroundAura || burstNow;
    auto cands = Candidates(me, radius, tNow);

    if (aura && !cands.empty() && (tNow - g_LastAuraMs) >= 60) {
        g_LastAuraMs = tNow;
        const size_t n = cands.size();
        for (size_t step = 0; step < n; ++step) {
            const size_t idx = (g_AuraCursor + step) % n;
            const Candidate& c = cands[idx];
            int64_t& last = g_AuraLast[c.id];
            if (tNow - last < 250) continue;
            if (c.dist > kMeleeReach) {
                if (!g_cfgState.bKillAroundGhostStep) continue;
                GhostTo(c.pos, me);
            } else if (g_kaHasActiveGhost) {
                GhostRestore(me);
            }
            Burst(c.id, c.gen, 6);
            last = tNow;
            g_AuraCursor = (idx + 1) % n;
            break;
        }
    } else if (burstNow && cands.empty()) {
        g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
    }

    if (g_cfgState.bKillAroundAutoRepeat && g_SelectedId >= 1) {
        if (IsDeadNow(g_SelectedId, tNow)) {
            g_cfgState.bKillAroundAutoRepeat = false;
        } else if ((tNow - g_LastFollowMs) >= 250) {
            g_LastFollowMs = tNow;
            Vec3 tp{};
            if (ElemPos(g_SelectedId, tp)) {
                g_CurTargetDist = std::sqrt((tp.x - me.x) * (tp.x - me.x) + (tp.z - me.z) * (tp.z - me.z));
                const float dx = tp.x - me.x, dy = tp.y - me.y, dz = tp.z - me.z;
                g_OutOfReach = std::sqrt(dx * dx + dy * dy + dz * dz) > kMeleeReach;
                if (!aura) {
                    if (!g_OutOfReach) {
                        if (g_kaHasActiveGhost) GhostRestore(me);
                        Burst(g_SelectedId, g_CurTargetGen, 6);
                    } else if (g_cfgState.bKillAroundGhostStep) {
                        GhostTo(tp, me);
                        Burst(g_SelectedId, g_CurTargetGen, 6);
                    }
                }
            }
        }
    }

    if (g_kaHasActiveGhost && tNow >= g_GhostHoldUntil && (!aura || cands.empty()) && !g_cfgState.bKillAroundAutoRepeat) {
        GhostRestore(me);
    }
}

// ---------------- Menu Panel (exact kill_around (2).h DrawSection) ----------------
inline void DrawPanel(float availW) {
    using Lang::T;
    g_cfgState.killAroundRadius = std::clamp(g_cfgState.killAroundRadius, 5.0f, 120.0f);

    ThemeEngine::ProToggle(T("Auto-repeat kill selected target", "تكرار قتل الهدف المحدد تلقائياً"), &g_cfgState.bKillAroundAutoRepeat);
    ThemeEngine::ProToggle(T("One-hit on my swings while active", "ضربة قاضية بضرباتي أثناء التفعيل"), &g_cfgState.bKillAroundOneHit);
    ThemeEngine::ProSlider(T("Radius (m)", "مدى القتل (متر)"), &g_cfgState.killAroundRadius, 5.0f, 120.0f, "%.0f m");
    ThemeEngine::ProToggle(T("Ghost step to far targets", "انتقال خفي للأهداف البعيدة"), &g_cfgState.bKillAroundGhostStep);

    const char* auraLbl = g_cfgState.bKillAroundAura
        ? T("KILL EVERYONE IN RANGE: ON  (tap to stop)##ka_btn", "قتل الجميع في المدى: مفعّل (اضغط للإيقاف)##ka_btn")
        : T("KILL EVERYONE IN RANGE: OFF (tap to start)##ka_btn", "قتل الجميع في المدى: متوقف (اضغط للتشغيل)##ka_btn");
    if (ThemeEngine::NeonActionButton(auraLbl, std::max(160.0f, availW - 4.0f), 40.0f, g_cfgState.bKillAroundAura)) {
        g_cfgState.bKillAroundAura = !g_cfgState.bKillAroundAura;
        if (!g_cfgState.bKillAroundAura) {
            g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
        }
    }

    ImGui::Spacing();
    const int64_t now = GetMonotonicMs();
    Vec3 me{};
    bool haveMe = false;
    {
        std::lock_guard<std::mutex> lk(g_LocalPosMtx);
        haveMe = g_LocalPosOk;
        me     = g_LocalPosCache;
    }

    auto all = haveMe ? Candidates(me, g_cfgState.killAroundRadius, now, true) : std::vector<Candidate>{};
    int alive = 0;
    for (const auto& c : all) if (!c.dead) ++alive;

    ImGui::Text("%s: %d  |  Elem: %d/%d  |  Hits: %d",
                T("Players in range", "اللاعبون في المدى"),
                alive,
                g_LocalId.load(std::memory_order_relaxed),
                g_LocalGen.load(std::memory_order_relaxed),
                g_AcceptedHits.load(std::memory_order_relaxed));

    if (g_LocalId.load(std::memory_order_relaxed) < 1) {
        ImGui::TextColored(ImVec4(1.00f, 0.80f, 0.20f, 1.00f), "%s",
            T("Reading your element id from the game. Nothing sends until then.",
              "جاري قراءة معرف اللاعب من اللعبة... انتظر لحظة."));
    }

    for (const auto& c : all) {
        ImGui::PushID(c.id);
        std::string nm;
        const bool named = LookupName(c.id, &nm);
        ImGui::Text("%s  #%d  [%.1fm]%s",
                    named ? nm.c_str() : "Player",
                    c.id, c.dist, c.dead ? " [DEAD]" : "");
        if (!c.dead) {
            ImGui::SameLine(std::max(180.0f, availW - 110.0f));
            if (ImGui::Button(T("KILL##kab", "قتل##kab"), ImVec2(96.0f, 30.0f))) {
                g_SelectedId = c.id;
                g_CurTargetGen = c.gen;
                g_cfgState.bKillAroundAutoRepeat = true;
                g_LastFollowMs = 0;
            }
        }
        ImGui::PopID();
    }

    if (g_SelectedId > 0) {
        ImGui::Spacing();
        ImGui::Text("%s: #%d  |  %s: %d",
                    T("Selected", "المحدد"), g_SelectedId,
                    T("Accepted hits", "الضربات المقبولة"), g_AcceptedHits.load(std::memory_order_relaxed));
        if (IsDeadNow(g_SelectedId, now)) {
            ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.00f), "%s",
                               T("Selected target is dead", "الهدف المحدد مات"));
        }
        if (g_OutOfReach) {
            ImGui::TextColored(ImVec4(1.00f, 0.80f, 0.20f, 1.00f), "%s (%.1f m / %.1f m)",
                               T("Target out of reach, move closer", "الهدف خارج المدى، اقترب أكثر"),
                               g_CurTargetDist, kMeleeReach);
        } else {
            ImGui::TextColored(ImVec4(0.30f, 1.00f, 0.50f, 1.00f), "%s",
                               T("Target in reach", "الهدف داخل المدى"));
        }
        if (ThemeEngine::SecondaryPillButton(T("Stop##kastp", "إيقاف##kastp"), std::max(160.0f, availW - 4.0f), 32.0f, true)) {
            g_cfgState.bKillAroundAutoRepeat = false;
            g_cfgState.bKillAroundAura = false;
            g_SelectedId = 0;
            g_kaKillAllBurstUntilMs.store(0, std::memory_order_release);
        }
    }
}

} // namespace KillAround
