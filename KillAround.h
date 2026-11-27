#pragma once
// ============================================================================
// KillAround.h — Direct iOS arm64 port of kill_around (3).h
//   - Byte-identical datagram structs & combat protocol from kill_around (3).h:
//       * SendModeStart -> optional SendOneHitRaw -> 6x SendComboHit(isKo=false) -> SendFinisher
//       * Rate limiter (g_UnackedSends > 120 -> 2s backoff)
//       * One-target-at-a-time Aura (3s stuck skip for 5s, 120ms follow cadence)
//       * 60s target visibility window, 120s dead hold + deadGen tracking
//       * GhostTo always enabled with 0.25f^2 dedupe + NextRemoteTime() synced via
//         ReliableTimeStorage.GetDouble() + incoming/outgoing displacement packets
//   - Dual iOS AOT transport: MetaClientWrapper.Send<T> (MakeGenericMethod + MethodBase.Invoke)
//     with automatic non-generic HostCommandWriter.WriteSend fallback if AOT strips a generic Send<T>
//   - Real player nicknames & digital IDs synced from Morpeh ESP + ServerPlayer roster
//   - Zero C++ exceptions (-fno-exceptions safe), zero Android APIs, TCFlyGuard + SafeMem protected
// ============================================================================

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

namespace KillAround {

// ---------------- Feature state (exact kill_around (3).h defaults) ----------------
static constexpr float   kMeleeReach   = 4.2f;     // server accepts melee hits inside 4.2m (3D)
static constexpr int64_t kDeadHoldMs   = 120000;   // 2 min dead hold after PedDeadDatagram
static constexpr int32_t kOneHitDamage = 100000;

inline bool                 g_OutOfReach = false;
inline int32_t              g_SelectedId = 0;
inline std::atomic<int64_t> g_BurstUntilMs{0};

// ---------------- Layouts (pack 1, exact offsets from dump1.4.1.cpp) ----------------
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

// ---------------- Clocks ----------------
inline int64_t MonoMs() {
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

// ---------------- Tracked elements & names ----------------
struct Tracked {
    Vec3    pos{0.0f, 0.0f, 0.0f};
    int32_t gen         = 1;
    int64_t seenMs      = 0;
    int64_t deadUntilMs = 0;
    int32_t deadGen     = 0;
};

inline std::mutex                              g_Mtx;
inline std::unordered_map<int32_t, Tracked>    g_Elems;
inline std::unordered_map<int32_t, std::string> g_ElemNames;
inline std::unordered_map<int32_t, int32_t>    g_ElemDigi;

inline std::atomic<int32_t> g_LocalId{0};
inline std::atomic<int32_t> g_LocalGen{1};
inline std::atomic<int32_t> g_MeleeOwnFlag{0};
inline int64_t              g_LastDeadMs = 0;
inline int32_t              g_LastDeadId = 0;
inline std::atomic<int32_t> g_AcceptedHits{0};
inline int32_t              g_UnackedSends = 0;
inline int32_t              g_LastAccSeen = 0;
inline int64_t              g_AuraPauseUntilMs = 0;

// Diagnostics
inline std::atomic<int32_t> g_DispHits{0};
inline std::atomic<int32_t> g_DeadHits{0};
inline std::atomic<int32_t> g_WriteHits{0};
inline std::atomic<int32_t> g_AttackWrites{0};
inline std::atomic<int32_t> g_SendCalls{0};
inline std::atomic<int32_t> g_SendOk{0};
inline std::atomic<int32_t> g_SendFail{0};
inline std::atomic<int32_t> g_TxMode{0}; // 0=init, 1=Gen, 2=Wire

inline uint8_t              g_MeleeTemplate[0x40]{};
inline std::atomic<bool>    g_HasMeleeTemplate{false};

// Client capture (Poll HWBP + GlobalContextCore.MetaClientWrapper + SimpleDImple)
inline std::atomic<void*>   g_metaClient{nullptr};
inline void*                g_KaClient = nullptr;
inline int64_t              g_KaClientTryMs = 0;

// ReliableTimeStorage (server reliable double clock)
inline void*                g_ReliableTimeInst = nullptr;
inline void*                g_ReliableTimeGetDoubleM = nullptr;
inline int64_t              g_ReliableTimeTryMs = 0;

// Cached datagram Il2CppClass* (also atomic copies for lock-free HWBP)
inline void*                g_ClsPedDisp  = nullptr;
inline void*                g_ClsPedDead  = nullptr;
inline void*                g_ClsMeleeAtk = nullptr;
inline void*                g_ClsMeleeReq = nullptr;
inline std::atomic<void*>   g_ClsPedDispAtomic{nullptr};
inline std::atomic<void*>   g_ClsMeleeReqAtomic{nullptr};

// ---------------- Remote time tracking (exact kill_around (3).h + ReliableTimeStorage) ----------------
inline std::atomic<double>  g_IncomingRemoteTime{0.0};
inline std::atomic<int64_t> g_IncomingRemoteMs{0};
inline double               g_LastRemoteTime = 0.0;
inline bool                 g_GhostActive    = false;
inline int64_t              g_GhostHoldUntil = 0;
inline Vec3                 g_LastGhostDest{};

// ---------------- Tick & Aura selection state (exact kill_around (3).h) ----------------
inline int64_t              g_LastFollowMs = 0;
inline bool                 g_AuraOwned    = false;
inline int64_t              g_AuraPickMs   = 0;
inline std::unordered_map<int32_t, int64_t> g_AuraSkipUntil;
inline int32_t              g_CurTargetGen = 1;
inline float                g_CurTargetDist = 0.0f;

// Local position cache (written on UnityMain Tick, read by menu)
inline std::mutex           g_LocalPosMtx;
inline bool                 g_LocalPosOk = false;
inline Vec3                 g_LocalPosCache{};
inline int64_t              g_LocalPosMs = 0;

// ---------------- Name & Digital ID helpers ----------------
inline bool LookupName(int32_t elemId, std::string* out) {
    std::lock_guard<std::mutex> lk(g_Mtx);
    auto it = g_ElemNames.find(elemId);
    if (it == g_ElemNames.end() || it->second.empty()) return false;
    if (out) *out = it->second;
    return true;
}

inline int32_t DigiForElem(int32_t elemId) {
    std::lock_guard<std::mutex> lk(g_Mtx);
    auto it = g_ElemDigi.find(elemId);
    return (it != g_ElemDigi.end()) ? it->second : 0;
}

// ---------------- Client & Clock resolution ----------------
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
    const int64_t now = MonoMs();
    if (now - g_KaClientTryMs < 400) return nullptr;
    g_KaClientTryMs = now;
    void* c = ResolveMetaClientFromGlobals();
    if (Il2Cpp::IsUnityObjectSafe(c)) {
        g_KaClient = c;
        if (!g_metaClient.load(std::memory_order_relaxed)) {
            g_metaClient.store(c, std::memory_order_release);
        }
    }
    return g_KaClient;
}

inline void PrecacheDatagramClassesOnce() {
    if (!Il2Cpp::Init()) return;
    if (!g_ClsPedDisp) {
        g_ClsPedDisp = Il2Cpp::FindClass(Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram");
        if (g_ClsPedDisp) g_ClsPedDispAtomic.store(g_ClsPedDisp, std::memory_order_release);
    }
    if (!g_ClsMeleeReq) {
        g_ClsMeleeReq = Il2Cpp::FindClass(Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackRequestDatagram");
        if (g_ClsMeleeReq) g_ClsMeleeReqAtomic.store(g_ClsMeleeReq, std::memory_order_release);
    }
}

inline void RecordIncomingRemoteTime(double rt, int64_t monoNow) {
    if (!std::isfinite(rt) || rt <= 10.0 || rt >= 1e9) return;
    g_IncomingRemoteTime.store(rt, std::memory_order_release);
    g_IncomingRemoteMs.store(monoNow, std::memory_order_release);
}

inline double ReadReliableTimeDouble() {
    if (!Il2Cpp::Init()) return 0.0;
    const int64_t now = MonoMs();
    if (!Il2Cpp::IsUnityObjectSafe(g_ReliableTimeInst) && (now - g_ReliableTimeTryMs) >= 500) {
        g_ReliableTimeTryMs = now;
        g_ReliableTimeInst = Morpeh::ResolveFromSimpleDImple(
            "dev.Scripts.ECS.Morpeh.Features.ReliableTimeFeature.Systems", "ReliableTimeStorage");
        if (!g_ReliableTimeGetDoubleM) {
            g_ReliableTimeGetDoubleM = Il2Cpp::ResolveMethod(
                "dev.Scripts.ECS.Morpeh.Features.ReliableTimeFeature.Systems", "ReliableTimeStorage", "GetDouble", 0);
        }
    }
    if (!Il2Cpp::IsUnityObjectSafe(g_ReliableTimeInst)) return 0.0;

    if (g_ReliableTimeGetDoubleM && Il2Cpp::g_api.object_unbox) {
        if (void* boxed = Il2Cpp::SafeInvoke(g_ReliableTimeGetDoubleM, g_ReliableTimeInst, nullptr)) {
            double v = 0.0;
            if (SafeMem::ReadVal(Il2Cpp::g_api.object_unbox(boxed), v) && std::isfinite(v) && v > 10.0 && v < 1e9) {
                RecordIncomingRemoteTime(v, now);
                return v;
            }
        }
    }
    double cached = 0.0;
    if (SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(g_ReliableTimeInst) + 0x20, cached) &&
        std::isfinite(cached) && cached > 10.0 && cached < 1e9) {
        RecordIncomingRemoteTime(cached, now);
        return cached;
    }
    return 0.0;
}

inline double NextRemoteTime() {
    const int64_t mono = MonoMs();
    double now = ReadReliableTimeDouble();
    if (now <= 10.0) {
        const int64_t incMs = g_IncomingRemoteMs.load(std::memory_order_acquire);
        const double  incRt = g_IncomingRemoteTime.load(std::memory_order_acquire);
        now = (incMs != 0 && incRt > 10.0)
            ? (incRt + static_cast<double>(mono - incMs) / 1000.0)
            : (static_cast<double>(mono) / 1000.0);
    }
    double t = g_LastRemoteTime + 0.001;
    if (now > t) t = now;
    g_LastRemoteTime = t;
    return t;
}

// ---------------- Dual Transport: Generic Send<T> + Non-Generic Wire Fallback ----------------
struct SendEntry {
    const char* name = nullptr;
    const char* ns = nullptr;
    uint32_t    methodGch = 0;
    void*       dgramKlass = nullptr;
    bool        genFailed = false;
    bool        wireReady = false;
    bool        wireFailed = false;
    uint64_t    packetId = 0;
    uint8_t     netChannel = 16; // NetChannel::Reliable
    void*       writeMethod = nullptr;
};
inline SendEntry g_Senders[8]{};
inline int       g_SenderCount = 0;
inline void*     g_ObjectKlass = nullptr;
inline void*     g_MethodBaseInvoke = nullptr;

struct WireCtx {
    bool     tried = false;
    bool     ready = false;
    uint32_t writerGch = 0;
    void*    writerResetM = nullptr;
    void*    writerToSegM = nullptr;
    void*    numSerWriteUlongM = nullptr;
    void*    cmdWriterWriteSendM = nullptr;
    size_t   offMcHandler = 0x28;
    size_t   offMcClient = 0x38;
    size_t   offFfPeerProv = 0x20;
    size_t   offFfCmdWriter = 0x30;
    size_t   offPpAssigned = 0x10;
    size_t   offPpPeer = 0x14;
};
inline WireCtx g_Wire{};

inline bool EnsureWireCtxReady() {
    if (g_Wire.ready) return true;
    if (g_Wire.tried) return false;
    g_Wire.tried = true;

    if (!Il2Cpp::Init() || !Il2Cpp::g_api.object_new || !Il2Cpp::g_api.gchandle_new ||
        !Il2Cpp::g_api.gchandle_get_target || !Il2Cpp::g_api.object_unbox) {
        return false;
    }

    void* mcK  = Il2Cpp::FindClass("FlakeFlow", "MetaClientWrapper");
    void* ffK  = Il2Cpp::FindClass("FlakeFlow", "FlakeFlowClient");
    void* ppK  = Il2Cpp::FindClass("FlakeFlow", "ClientPeerProvider");
    void* hcwK = Il2Cpp::FindClass("FlakeFlow", "HostCommandWriter");
    void* wK   = Il2Cpp::FindClass("dev.Scripts.Networking.IBNet", "IBNet_Writer");
    void* nsK  = Il2Cpp::FindClass("dev.Scripts.Networking.IBNet", "NumberSerializer");
    if (!mcK || !ffK || !ppK || !hcwK || !wK || !nsK) return false;

    g_Wire.offMcHandler   = Il2Cpp::FieldOffset(mcK, "_datagramHandler", 0x28);
    g_Wire.offMcClient    = Il2Cpp::FieldOffset(mcK, "_client", 0x38);
    g_Wire.offFfPeerProv  = Il2Cpp::FieldOffset(ffK, "_peerProvider", 0x20);
    g_Wire.offFfCmdWriter = Il2Cpp::FieldOffset(ffK, "_commandWriter", 0x30);
    g_Wire.offPpAssigned  = Il2Cpp::FieldOffset(ppK, "_isAssigned", 0x10);
    g_Wire.offPpPeer      = Il2Cpp::FieldOffset(ppK, "<MetaPeer>k__BackingField", 0x14);

    void* wCtor = Il2Cpp::g_api.class_get_method_from_name(wK, ".ctor", 1);
    g_Wire.writerResetM        = Il2Cpp::g_api.class_get_method_from_name(wK, "Reset", 0);
    g_Wire.writerToSegM        = Il2Cpp::g_api.class_get_method_from_name(wK, "ToArraySegment", 0);
    g_Wire.numSerWriteUlongM   = Il2Cpp::g_api.class_get_method_from_name(nsK, "WriteULong", 2);
    g_Wire.cmdWriterWriteSendM = Il2Cpp::g_api.class_get_method_from_name(hcwK, "WriteSend", 3);
    if (!wCtor || !g_Wire.writerResetM || !g_Wire.writerToSegM ||
        !g_Wire.numSerWriteUlongM || !g_Wire.cmdWriterWriteSendM) {
        return false;
    }

    void* writerObj = nullptr;
    void* exc = nullptr;
    TCFlyGuard::TryRun([&]() {
        Il2Cpp::EnsureThreadAttached();
        writerObj = Il2Cpp::g_api.object_new(wK);
        if (SafeMem::IsPlausiblePtr(writerObj)) {
            int32_t cap = 512;
            void* args[1] = { &cap };
            Il2Cpp::g_api.runtime_invoke(wCtor, writerObj, args, &exc);
        }
    });
    if (!SafeMem::IsPlausiblePtr(writerObj) || exc != nullptr) return false;

    g_Wire.writerGch = Il2Cpp::g_api.gchandle_new(writerObj, true);
    g_Wire.ready = (g_Wire.writerGch != 0);
    return g_Wire.ready;
}

inline bool LookupMetaTypeInfo(void* dgramKlass, const char* ns, const char* dgramName, uint64_t& outPacketId, uint8_t& outChannel) {
    if (!Il2Cpp::Init() || !dgramKlass) return false;

    if (Il2Cpp::g_api.class_get_type && Il2Cpp::g_api.type_get_object) {
        void* dgType    = Il2Cpp::g_api.class_get_type(dgramKlass);
        void* dgTypeObj = dgType ? Il2Cpp::g_api.type_get_object(dgType) : nullptr;
        void* dict = Il2Cpp::ResolveStaticClassFieldPtr("FlakeFlow", "MetaTypeInfo", "IdToInfo");
        if (!dict) {
            if (void* warmM = Il2Cpp::ResolveMethod("FlakeFlow", "MetaTypeInfo", "Warmup", 0)) {
                Il2Cpp::SafeInvoke(warmM, nullptr, nullptr);
                dict = Il2Cpp::ResolveStaticClassFieldPtr("FlakeFlow", "MetaTypeInfo", "IdToInfo");
            }
        }
        if (Il2Cpp::IsUnityObjectSafe(dict) && SafeMem::IsPlausiblePtr(dgTypeObj)) {
            void* dictK = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(dict) : nullptr;
            size_t offEntries = Il2Cpp::FieldOffset(dictK, "_entries", 0x18);
            size_t offCount   = Il2Cpp::FieldOffset(dictK, "_count", 0x20);
            void* entriesArr  = SafeMem::PtrAt(dict, offEntries);
            int32_t count     = 0;
            SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(dict) + offCount, count);
            if (Il2Cpp::IsUnityObjectSafe(entriesArr) && count > 0 && count < 4096) {
                int32_t arrLen = 0;
                SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(entriesArr) + 0x18, arrLen);
                int32_t stride = Il2Cpp::g_api.array_element_size ? Il2Cpp::g_api.array_element_size(entriesArr) : 24;
                if (stride >= 24 && stride <= 64 && arrLen > 0) {
                    const int32_t limit = std::min(count, arrLen);
                    void* mtiK = Il2Cpp::FindClass("FlakeFlow", "MetaTypeInfo");
                    size_t offType = Il2Cpp::FieldOffset(mtiK, "type", 0x10);
                    size_t offId   = Il2Cpp::FieldOffset(mtiK, "PacketId", 0x20);
                    size_t offChan = Il2Cpp::FieldOffset(mtiK, "NetChannel", 0x28);
                    const uint8_t* base = reinterpret_cast<const uint8_t*>(entriesArr) + 0x20;
                    for (int32_t i = 0; i < limit; ++i) {
                        const uint8_t* ent = base + static_cast<size_t>(i) * static_cast<size_t>(stride);
                        int32_t hc = -1;
                        SafeMem::ReadVal(ent, hc);
                        if (hc < 0) continue;
                        void* mtiObj = nullptr;
                        SafeMem::ReadVal(ent + 0x10, mtiObj);
                        if (!Il2Cpp::IsUnityObjectSafe(mtiObj)) continue;
                        if (SafeMem::PtrAt(mtiObj, offType) == dgTypeObj) {
                            uint64_t pid = 0;
                            uint8_t  ch  = 16;
                            SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(mtiObj) + offId, pid);
                            SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(mtiObj) + offChan, ch);
                            if (pid != 0) {
                                outPacketId = pid;
                                outChannel  = (ch == 16 || ch == 32) ? ch : 16;
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    if (void* fnvM = Il2Cpp::ResolveMethod("FlakeFlow", "HashUtility", "Fnv1A64", 1)) {
        if (Il2Cpp::g_api.string_new && Il2Cpp::g_api.object_unbox) {
            std::string fullName = (ns && ns[0]) ? (std::string(ns) + "." + dgramName) : std::string(dgramName);
            if (void* sObj = Il2Cpp::g_api.string_new(fullName.c_str())) {
                void* args[1] = { sObj };
                if (void* boxedHash = Il2Cpp::SafeInvoke(fnvM, nullptr, args)) {
                    uint64_t pid = 0;
                    if (SafeMem::ReadVal(Il2Cpp::g_api.object_unbox(boxedHash), pid) && pid != 0) {
                        outPacketId = pid;
                        outChannel  = (std::strcmp(dgramName, "PedDisplacementSyncDatagram") == 0) ? 32 : 16;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

inline bool SendDatagramRawWire(void* clientObj, SendEntry* e, uint8_t* box) {
    if (!e || e->wireFailed || !Il2Cpp::IsUnityObjectSafe(clientObj)) return false;
    if (!EnsureWireCtxReady()) return false;

    if (!e->dgramKlass) {
        e->dgramKlass = Il2Cpp::FindClass(e->ns, e->name);
        if (!e->dgramKlass) { e->wireFailed = true; return false; }
    }
    if (!e->wireReady) {
        uint64_t pid = 0;
        uint8_t  ch  = 16;
        if (!LookupMetaTypeInfo(e->dgramKlass, e->ns, e->name, pid, ch)) {
            e->wireFailed = true;
            return false;
        }
        void* writeM  = Il2Cpp::g_api.class_get_method_from_name(e->dgramKlass, "Write", 1);
        void* writeFn = writeM ? SafeMem::PtrAt(writeM, 0) : nullptr;
        if (!writeM || !SafeMem::IsPlausiblePtr(writeFn)) {
            e->wireFailed = true;
            return false;
        }
        e->packetId    = pid;
        e->netChannel  = ch;
        e->writeMethod = writeM;
        e->wireReady   = true;
    }

    void* ffClient = SafeMem::PtrAt(clientObj, g_Wire.offMcClient);
    if (!Il2Cpp::IsUnityObjectSafe(ffClient)) return false;
    void* peerProv  = SafeMem::PtrAt(ffClient, g_Wire.offFfPeerProv);
    void* cmdWriter = SafeMem::PtrAt(ffClient, g_Wire.offFfCmdWriter);
    if (!Il2Cpp::IsUnityObjectSafe(peerProv) || !Il2Cpp::IsUnityObjectSafe(cmdWriter)) return false;

    uint8_t isAssigned = 0;
    SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(peerProv) + g_Wire.offPpAssigned, isAssigned);
    if (!isAssigned) return false;

    struct MetaPeerRaw { int32_t Id; int32_t Gen; } peer{};
    if (!SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(peerProv) + g_Wire.offPpPeer, peer)) return false;

    void* writerObj = Il2Cpp::g_api.gchandle_get_target(g_Wire.writerGch);
    if (!Il2Cpp::IsUnityObjectSafe(writerObj)) return false;

    bool ok = false;
    TCFlyGuard::TryRun([&]() {
        Il2Cpp::EnsureThreadAttached();
        void* exc = nullptr;
        Il2Cpp::g_api.runtime_invoke(g_Wire.writerResetM, writerObj, nullptr, &exc);
        if (exc) return;

        uint64_t pktId = e->packetId;
        void* hdrArgs[2] = { writerObj, &pktId };
        Il2Cpp::g_api.runtime_invoke(g_Wire.numSerWriteUlongM, nullptr, hdrArgs, &exc);
        if (exc) return;

        void* writeFnPtr = SafeMem::PtrAt(e->writeMethod, 0);
        if (!SafeMem::IsPlausiblePtr(writeFnPtr)) return;
        using StructWriteFn = void (*)(void*, void*, void*);
        reinterpret_cast<StructWriteFn>(writeFnPtr)(box, writerObj, e->writeMethod);

        exc = nullptr;
        void* boxedSeg = Il2Cpp::g_api.runtime_invoke(g_Wire.writerToSegM, writerObj, nullptr, &exc);
        if (!SafeMem::IsPlausiblePtr(boxedSeg) || exc) return;

        void* segPtr = Il2Cpp::g_api.object_unbox(boxedSeg);
        if (!SafeMem::IsPlausiblePtr(segPtr)) return;

        uint8_t ch = e->netChannel;
        void* sendArgs[3] = { &peer, segPtr, &ch };
        exc = nullptr;
        Il2Cpp::g_api.runtime_invoke(g_Wire.cmdWriterWriteSendM, cmdWriter, sendArgs, &exc);
        ok = (exc == nullptr);
    });
    if (ok) g_TxMode.store(2, std::memory_order_release);
    return ok;
}

inline bool SendDatagramImpl(const char* ns, const char* dgramName, const void* payload, size_t payloadSize) {
    void* client = ClientForSend();
    if (!Il2Cpp::IsUnityObjectSafe(client) || payloadSize > 0x200) return false;
    if (!Il2Cpp::Init()) return false;

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
        e->ns   = ns;
    }

    uint8_t box[0x200]{};
    if (payload && payloadSize > 0) std::memcpy(box, payload, payloadSize);

    // Primary path: exact kill_around (3).h MetaClientWrapper.Send<T>
    if (!e->genFailed && Il2Cpp::g_api.value_box && Il2Cpp::g_api.array_new &&
        Il2Cpp::g_api.method_get_object && Il2Cpp::g_api.gchandle_new && Il2Cpp::g_api.gchandle_get_target) {
        bool ok = false;
        TCFlyGuard::TryRun([&]() {
            Il2Cpp::EnsureThreadAttached();
            void* exc = nullptr;
            if (e->methodGch == 0) {
                void* wrapperK = Il2Cpp::FindClass("FlakeFlow", "MetaClientWrapper");
                if (!e->dgramKlass) e->dgramKlass = Il2Cpp::FindClass(ns, dgramName);
                void* typeK = Il2Cpp::FindClass("System", "Type");
                if (!g_ObjectKlass) g_ObjectKlass = Il2Cpp::FindClass("System", "Object");
                void* miK   = Il2Cpp::FindClass("System.Reflection", "MethodInfo");
                void* rmiK  = Il2Cpp::FindClass("System.Reflection", "RuntimeMethodInfo");
                void* mbK   = Il2Cpp::FindClass("System.Reflection", "MethodBase");
                if (!wrapperK || !e->dgramKlass || !typeK || !g_ObjectKlass || !miK || !mbK) {
                    e->genFailed = true;
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
                    e->genFailed = true;
                    return;
                }

                *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(typeArr) + 0x20) = dgTypeObj;
                void* args[1] = { typeArr };
                void* concrete = Il2Cpp::g_api.runtime_invoke(makeGen, sendObj, args, &exc);
                if (!SafeMem::IsPlausiblePtr(concrete) || exc) {
                    e->genFailed = true;
                    return;
                }
                if (!g_MethodBaseInvoke) {
                    g_MethodBaseInvoke = Il2Cpp::g_api.class_get_method_from_name(mbK, "Invoke", 2);
                }
                e->methodGch = Il2Cpp::g_api.gchandle_new(concrete, false);
                if (!e->methodGch || !g_MethodBaseInvoke) {
                    e->genFailed = true;
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
                if (exc) e->genFailed = true;
            }
        });
        if (ok) {
            if (g_TxMode.load(std::memory_order_relaxed) == 0) {
                g_TxMode.store(1, std::memory_order_release);
            }
            return true;
        }
    }

    // Automatic non-generic HostCommandWriter.WriteSend fallback for iOS Full AOT
    return SendDatagramRawWire(client, e, box);
}

inline bool SendDatagram(const char* ns, const char* dgramName, const void* payload, size_t payloadSize) {
    g_SendCalls.fetch_add(1, std::memory_order_relaxed);
    const bool ok = SendDatagramImpl(ns, dgramName, payload, payloadSize);
    (ok ? g_SendOk : g_SendFail).fetch_add(1, std::memory_order_relaxed);
    return ok;
}

// ---------------- Combat packets (exact kill_around (3).h) ----------------
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
    bool ok = SendDatagram(Offsets::g_cfg.nsWeaponDatagrams.c_str(),
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
    m.element    = { g_LocalId.load(std::memory_order_relaxed), g_LocalGen.load(std::memory_order_relaxed) };
    m.target     = { targetId, targetGen > 0 ? targetGen : 1 };
    m.damage     = 1000;
    m.trace[0]   = dir[0];
    m.trace[1]   = dir[1];
    m.trace[2]   = dir[2];
    m.heightType = 0;
    m.direction  = 2;
    m.attackType = 2;
    m.isKo       = 1;
    m.timestamp  = ComboTimestamp();
    return SendDatagram(Offsets::g_cfg.nsWeaponDatagrams.c_str(),
                        "MeleeWeaponAttackDatagram", &m, sizeof(m));
}

inline bool SendDisplacement(const Vec3& at, double remoteTime) {
    DisplacementDatagram d{};
    d.remoteId   = { g_LocalId.load(std::memory_order_relaxed), g_LocalGen.load(std::memory_order_relaxed) };
    d.pos[0] = at.x; d.pos[1] = at.y; d.pos[2] = at.z;
    d.rot[3] = 1.0f;
    d.remoteTime = remoteTime;
    d.isClose    = 1;
    return SendDatagram(Offsets::g_cfg.nsIBNetElemSync.c_str(),
                        "PedDisplacementSyncDatagram", &d, sizeof(d));
}

// ---------------- Position, candidates, ghost step & burst ----------------
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
    const int32_t myId = g_LocalId.load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(g_Mtx);
    for (const auto& kv : g_Elems) {
        const int32_t id = kv.first;
        const Tracked& t = kv.second;
        if (id < 1 || id == myId) continue;
        if ((now - t.seenMs) > 60000) continue; // 60s window for stationary targets
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
    if (g_GhostActive) {
        const float dx = dest.x - g_LastGhostDest.x;
        const float dy = dest.y - g_LastGhostDest.y;
        const float dz = dest.z - g_LastGhostDest.z;
        if (dx * dx + dy * dy + dz * dz < 0.25f) {
            g_GhostHoldUntil = MonoMs() + 200;
            return;
        }
    }
    SendDisplacement(dest, NextRemoteTime());
    g_LastGhostDest  = dest;
    g_GhostActive    = true;
    g_GhostHoldUntil = MonoMs() + 200;
}

inline void GhostRestore(const Vec3& me) {
    if (!g_GhostActive) return;
    SendDisplacement(me, NextRemoteTime());
    g_GhostActive = false;
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
        // Exact kill_around (3).h: isKo = false on combo hits, knockout only on finisher/one-hit
        if (SendComboHit(id, gen, false, ComboTimestamp(), dir, (i % 3 == 2) ? 2 : 0)) ++sent;
    }
    float hitPos[3] = { tp.x, tp.y + 0.9f, tp.z };
    if (SendFinisher(id, gen, hitPos, dir)) ++sent;
    g_UnackedSends += sent;
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

// ---------------- Roster, Player Names & Digital IDs Sync (UnityMain) ----------------
inline void SyncRosterAndNamesOnGameThread(int64_t now) {
    static int64_t s_lastSyncMs = 0;
    if (now - s_lastSyncMs < 150) return;
    s_lastSyncMs = now;
    if (!SafeMem::IsPlausiblePtr(Morpeh::g_stashes.elementIdStash)) return;

    int64_t localPed = Morpeh::GetLocalPedEntity();
    int32_t localPedId = (localPed > 0) ? Morpeh::UnpackEntityId(localPed) : -1;

    std::vector<ESPPlayer> streamedSnap;
    std::vector<ServerPlayerEntry> serverSnap;
    {
        std::lock_guard<std::mutex> lk(::g_espMutex);
        streamedSnap = ::g_renderPlayers;
        serverSnap   = ::g_allServerPlayers;
    }

    std::lock_guard<std::mutex> lk(g_Mtx);
    for (const auto& sp : serverSnap) {
        if (sp.pedId < 0 || sp.pedId == localPedId) continue;
        EntityId eid{};
        void* ep = Morpeh::Get(Morpeh::g_stashes.elementIdStash, sp.pedId, sizeof(EntityId));
        if (!ep || !SafeMem::ReadVal(ep, eid) || eid.Id < 1) continue;
        if (!sp.name.empty()) g_ElemNames[eid.Id] = sp.name;
        if (sp.digitalId > 0) g_ElemDigi[eid.Id] = static_cast<int32_t>(sp.digitalId);
        if (sp.isStreamed && IsFiniteVec3(sp.worldPos) && (std::fabs(sp.worldPos.x) > 0.1f || std::fabs(sp.worldPos.z) > 0.1f)) {
            Tracked& t = g_Elems[eid.Id];
            t.pos = sp.worldPos;
            const int32_t newGen = eid.Gen > 0 ? eid.Gen : 1;
            if (t.deadGen != 0 && newGen != t.deadGen) {
                t.deadUntilMs = 0;
                t.deadGen = 0;
            }
            t.gen    = newGen;
            t.seenMs = now;
            if (sp.health <= 0 && t.deadUntilMs <= now) {
                t.deadUntilMs = now + 5000;
                t.deadGen = newGen;
            }
        }
    }

    for (const auto& pl : streamedSnap) {
        if (pl.entityId < 0 || pl.entityId == localPedId) continue;
        if (pl.playerId < 0 && !g_cfgState.bKillAroundIncludeNpcs) continue;
        EntityId eid{};
        void* ep = Morpeh::Get(Morpeh::g_stashes.elementIdStash, pl.entityId, sizeof(EntityId));
        if (!ep || !SafeMem::ReadVal(ep, eid) || eid.Id < 1) continue;

        Tracked& t = g_Elems[eid.Id];
        t.pos = pl.worldPos;
        const int32_t newGen = eid.Gen > 0 ? eid.Gen : 1;
        if (t.deadGen != 0 && newGen != t.deadGen) {
            t.deadUntilMs = 0;
            t.deadGen = 0;
        }
        t.gen    = newGen;
        t.seenMs = now;
        if ((pl.dead || pl.health <= 0) && t.deadUntilMs <= now) {
            t.deadUntilMs = now + 5000;
            t.deadGen = newGen;
        }
        if (!pl.name.empty()) {
            g_ElemNames[eid.Id] = pl.name;
        }
    }
}

// ---------------- Lock-free HWBP Capture Helpers (Mach Exception Thread Safe) ----------------
inline void OnPedDispWriteLockFree(uint64_t x0) {
    if (x0 < 0x100000000ULL) return;
    void* ptr = reinterpret_cast<void*>(x0);
    DisplacementDatagram d{};
    if (!SafeMem::ReadVal(ptr, d)) return;
    void* dispCls = g_ClsPedDispAtomic.load(std::memory_order_acquire);
    if (dispCls && SafeMem::PtrAt(ptr, 0) == dispCls) {
        if (!SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(ptr) + 0x10, d)) return;
    }
    if (d.remoteId.Id >= 1 && d.remoteId.Gen >= 1 && d.remoteId.Gen <= 1000000 &&
        std::isfinite(d.remoteTime) && d.remoteTime > 10.0 && d.remoteTime < 1e9) {
        g_LocalId.store(d.remoteId.Id, std::memory_order_release);
        g_LocalGen.store(d.remoteId.Gen, std::memory_order_release);
        RecordIncomingRemoteTime(d.remoteTime, MonoMs());
        g_DispHits.fetch_add(1, std::memory_order_relaxed);
    }
}

inline void OnMeleeReqWriteLockFree(uint64_t x0) {
    if (x0 < 0x100000000ULL) return;
    uint8_t* ptr = reinterpret_cast<uint8_t*>(x0);
    void* reqCls = g_ClsMeleeReqAtomic.load(std::memory_order_acquire);
    if (reqCls && SafeMem::PtrAt(ptr, 0) == reqCls) ptr += 0x10;
    RequestDatagram r{};
    if (!SafeMem::ReadVal(ptr, r)) return;
    g_WriteHits.fetch_add(1, std::memory_order_relaxed);
    if (r.element.Id >= 1 && r.element.Gen >= 1 && r.element.Gen <= 1000000) {
        g_LocalId.store(r.element.Id, std::memory_order_release);
        g_LocalGen.store(r.element.Gen, std::memory_order_release);
    }
    if (!g_cfgState.bMasterDisable && g_cfgState.bKillAroundOneHit &&
        g_MeleeOwnFlag.load(std::memory_order_relaxed) == 0 &&
        (g_cfgState.bKillAroundAutoRepeat || g_cfgState.bKillAroundAura ||
         g_BurstUntilMs.load(std::memory_order_relaxed) > 0)) {
        uint8_t ko = 1;
        SafeMem::Write(ptr + offsetof(RequestDatagram, isKo), &ko, 1);
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
                   g_BurstUntilMs.load(std::memory_order_relaxed) > 0);
    if (active) {
        outOverride = true;
        return kOneHitDamage;
    }
    return 0;
}

inline void OnPedDisplacementRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_ClsPedDisp, Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    DisplacementDatagram d{};
    if (!SafeMem::ReadVal(ptr, d) || d.remoteId.Id < 1) return;
    Vec3 p{ d.pos[0], d.pos[1], d.pos[2] };
    if (!IsFiniteVec3(p)) return;
    g_DispHits.fetch_add(1, std::memory_order_relaxed);
    const int64_t now = MonoMs();
    if (std::isfinite(d.remoteTime) && d.remoteTime > 10.0) {
        RecordIncomingRemoteTime(d.remoteTime, now);
    }
    std::lock_guard<std::mutex> lk(g_Mtx);
    Tracked& t = g_Elems[d.remoteId.Id];
    t.pos = p;
    const int32_t newGen = d.remoteId.Gen > 0 ? d.remoteId.Gen : 1;
    if (t.deadGen != 0 && newGen != t.deadGen) {
        t.deadUntilMs = 0;
        t.deadGen = 0;
    }
    t.gen    = newGen;
    t.seenMs = now;
}

inline void OnPedDeadRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_ClsPedDead, Offsets::g_cfg.nsIBNetPedSync.c_str(), "PedDeadDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    int32_t id = 0;
    if (!SafeMem::ReadVal(ptr, id)) return;
    g_DeadHits.fetch_add(1, std::memory_order_relaxed);
    if (id < 1) return;
    const int64_t now = MonoMs();
    {
        std::lock_guard<std::mutex> lk(g_Mtx);
        Tracked& t = g_Elems[id];
        if (t.deadUntilMs <= now) t.deadUntilMs = now + kDeadHoldMs;
        t.deadGen = t.gen;
    }
    g_LastDeadId = id;
    g_LastDeadMs = now;
}

inline void OnMeleeAttackRead(const void* rawPkt) {
    const void* ptr = NormalizeDatagramPtr(rawPkt, g_ClsMeleeAtk, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackDatagram");
    if (!SafeMem::IsPlausiblePtr(ptr)) return;
    MeleeHitDatagram m{};
    if (!SafeMem::ReadVal(ptr, m)) return;
    std::memcpy(g_MeleeTemplate, &m, sizeof(g_MeleeTemplate));
    g_HasMeleeTemplate.store(true, std::memory_order_release);
    if (m.target.Id < 1) return;
    const int32_t myId = g_LocalId.load(std::memory_order_relaxed);
    if (myId >= 1 && m.element.Id == myId) {
        g_AcceptedHits.fetch_add(1, std::memory_order_relaxed);
    }
}

inline void OnMeleeRequestWrite(void* rawPkt, void* /*writer*/ = nullptr) {
    void* ptr = NormalizeDatagramPtrMut(rawPkt, g_ClsMeleeReq, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackRequestDatagram");
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
         g_BurstUntilMs.load(std::memory_order_relaxed) > 0)) {
        uint8_t ko = 1;
        SafeMem::Write(reinterpret_cast<uint8_t*>(ptr) + offsetof(RequestDatagram, isKo), &ko, 1);
    }
}

inline void OnMeleeAttackWrite(void* rawPkt, void* /*writer*/ = nullptr) {
    void* ptr = NormalizeDatagramPtrMut(rawPkt, g_ClsMeleeAtk, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackDatagram");
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

// Reads EntityMetaDatagramHandler._items -> NetworkFilter<T> directly on UnityMain
inline void SyncNetworkFiltersOnGameThread(int64_t now) {
    static int64_t  s_lastPollMs   = 0;
    static bool     s_idsResolved  = false;
    static uint64_t s_pktDisp      = 0;
    static uint64_t s_pktDead      = 0;
    static uint64_t s_pktMeleeAtk  = 0;
    static uint64_t s_lastMeleeTs  = 0;
    static int32_t  s_lastDeadSeen = 0;

    if (now - s_lastPollMs < 60) return;
    s_lastPollMs = now;

    void* client = ClientForSend();
    if (!Il2Cpp::IsUnityObjectSafe(client) || !EnsureWireCtxReady()) return;

    if (!s_idsResolved) {
        s_idsResolved = true;
        uint8_t ch = 0;
        if (void* k = Il2Cpp::FindClass(Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram"))
            LookupMetaTypeInfo(k, Offsets::g_cfg.nsIBNetElemSync.c_str(), "PedDisplacementSyncDatagram", s_pktDisp, ch);
        if (void* k = Il2Cpp::FindClass(Offsets::g_cfg.nsIBNetPedSync.c_str(), "PedDeadDatagram"))
            LookupMetaTypeInfo(k, Offsets::g_cfg.nsIBNetPedSync.c_str(), "PedDeadDatagram", s_pktDead, ch);
        if (void* k = Il2Cpp::FindClass(Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackDatagram"))
            LookupMetaTypeInfo(k, Offsets::g_cfg.nsWeaponDatagrams.c_str(), "MeleeWeaponAttackDatagram", s_pktMeleeAtk, ch);
    }

    void* handler = SafeMem::PtrAt(client, g_Wire.offMcHandler);
    if (!Il2Cpp::IsUnityObjectSafe(handler)) return;
    void* dict = SafeMem::PtrAt(handler, 0x10);
    if (!Il2Cpp::IsUnityObjectSafe(dict)) return;

    void* dictK = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(dict) : nullptr;
    size_t offEntries = Il2Cpp::FieldOffset(dictK, "_entries", 0x18);
    size_t offCount   = Il2Cpp::FieldOffset(dictK, "_count", 0x20);
    void* entriesArr  = SafeMem::PtrAt(dict, offEntries);
    int32_t count     = 0;
    SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(dict) + offCount, count);
    if (!Il2Cpp::IsUnityObjectSafe(entriesArr) || count <= 0 || count > 1024) return;

    int32_t arrLen = 0;
    SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(entriesArr) + 0x18, arrLen);
    const int32_t limit = std::min(count, arrLen);
    const uint8_t* base = reinterpret_cast<const uint8_t*>(entriesArr) + 0x20;

    for (int32_t i = 0; i < limit; ++i) {
        const uint8_t* ent = base + static_cast<size_t>(i) * 24ULL;
        int32_t hc = -1;
        uint64_t key = 0;
        void* itemObj = nullptr;
        SafeMem::ReadVal(ent, hc);
        if (hc < 0) continue;
        SafeMem::ReadVal(ent + 0x08, key);
        SafeMem::ReadVal(ent + 0x10, itemObj);
        if (!Il2Cpp::IsUnityObjectSafe(itemObj)) continue;

        if (key != s_pktDisp && key != s_pktDead && key != s_pktMeleeAtk) continue;
        void* filterObj = SafeMem::PtrAt(itemObj, 0x18);
        if (!Il2Cpp::IsUnityObjectSafe(filterObj)) continue;

        void* arr = SafeMem::PtrAt(filterObj, 0x10);
        int32_t fCount = 0;
        SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(filterObj) + 0x18, fCount);
        if (!Il2Cpp::IsUnityObjectSafe(arr) || fCount <= 0 || fCount > 64) continue;

        const uint8_t* fBase = reinterpret_cast<const uint8_t*>(arr) + 0x20;
        if (key == s_pktDisp) {
            for (int32_t j = 0; j < fCount; ++j) {
                OnPedDisplacementRead(fBase + static_cast<size_t>(j) * sizeof(DisplacementDatagram));
            }
        } else if (key == s_pktDead) {
            for (int32_t j = 0; j < fCount; ++j) {
                const uint8_t* pd = fBase + static_cast<size_t>(j) * 0x50ULL;
                int32_t deadId = 0;
                if (SafeMem::ReadVal(pd, deadId) && deadId >= 1 && deadId != s_lastDeadSeen) {
                    s_lastDeadSeen = deadId;
                    OnPedDeadRead(pd);
                }
            }
        } else if (key == s_pktMeleeAtk) {
            for (int32_t j = 0; j < fCount; ++j) {
                MeleeHitDatagram mh{};
                const uint8_t* mp = fBase + static_cast<size_t>(j) * sizeof(MeleeHitDatagram);
                if (SafeMem::ReadVal(mp, mh) && mh.timestamp != s_lastMeleeTs) {
                    s_lastMeleeTs = mh.timestamp;
                    OnMeleeAttackRead(mp);
                }
            }
        }
    }
}

// ---------------- Tick (UnityMain — exact kill_around (3).h logic) ----------------
inline void Tick() {
    const int64_t tNow = MonoMs();
    PrecacheDatagramClassesOnce();

    // Refresh cached local position at 4 Hz for the menu (menu never touches engine)
    if (tNow - g_LocalPosMs >= 250) {
        g_LocalPosMs = tNow;
        Vec3 p{};
        const bool ok = LocalPos(p);
        std::lock_guard<std::mutex> lk(g_LocalPosMtx);
        g_LocalPosOk    = ok;
        g_LocalPosCache = p;
    }

    // Seed ReliableTimeStorage clock & MetaClientWrapper
    ClientForSend();
    if (g_IncomingRemoteMs.load(std::memory_order_acquire) == 0) {
        ReadReliableTimeDouble();
    }

    // Keep target roster, player names & network filters fresh when viewing or active
    const bool menuViewingKa = g_menuIsOpenAtomic.load(std::memory_order_relaxed) &&
                               g_activeTabAtomic.load(std::memory_order_relaxed) == 0 &&
                               g_activeSubTabAtomic.load(std::memory_order_relaxed) == 3;
    const bool burstNow = g_BurstUntilMs.load(std::memory_order_acquire) > tNow;
    if (menuViewingKa || g_cfgState.bKillAroundAura || g_cfgState.bKillAroundAutoRepeat || burstNow) {
        SyncRosterAndNamesOnGameThread(tNow);
        SyncNetworkFiltersOnGameThread(tNow);
    }

    // New local ped = relog / respawn into new world -> reset session & drop stale targets
    {
        int64_t curPed = Morpeh::GetLocalPedEntity();
        static int64_t s_lastPed = 0;
        if (curPed > 0 && s_lastPed > 0 && curPed != s_lastPed) {
            g_cfgState.bKillAroundAutoRepeat = false;
            g_cfgState.bKillAroundAura       = false;
            g_SelectedId   = 0;
            g_AuraOwned    = false;
            g_AuraSkipUntil.clear();
            g_GhostActive  = false;
            g_KaClient     = nullptr;
            g_ReliableTimeInst = nullptr;
            {
                std::lock_guard<std::mutex> lk(g_Mtx);
                g_Elems.clear();
            }
            g_BurstUntilMs.store(0, std::memory_order_release);
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

    if (!g_cfgState.bKillAroundAutoRepeat && !g_cfgState.bKillAroundAura && !burstNow && !g_GhostActive) {
        return;
    }
    if (!EnsureLocalIdCached()) return;

    const int64_t now = MonoMs();
    Vec3 me{};
    if (!LocalPos(me)) return;

    const float radius = std::clamp(g_cfgState.killAroundRadius, 5.0f, 120.0f);
    const bool burst   = now < g_BurstUntilMs.load(std::memory_order_acquire);
    const bool aura    = g_cfgState.bKillAroundAura || burst;
    auto cands         = Candidates(me, radius, now);

    // Rate limiter (exact kill_around (3).h): reset on accepted hit, pause 2s if >120 unacked sends
    const int32_t accNow = g_AcceptedHits.load(std::memory_order_relaxed);
    if (accNow != g_LastAccSeen) {
        g_LastAccSeen = accNow;
        g_UnackedSends = 0;
    }
    if (g_UnackedSends > 120) {
        g_UnackedSends = 0;
        g_AuraPauseUntilMs = now + 2000;
    }
    const bool paused = now < g_AuraPauseUntilMs;

    // Kill-all: one target at a time through the same selection & burst path as manual KILL
    if (aura) {
        const bool selGone  = g_SelectedId < 1 || IsDeadNow(g_SelectedId, now);
        const bool selStuck = g_AuraOwned && (now - g_AuraPickMs) > 3000;
        if ((selGone || selStuck) && !cands.empty()) {
            if (g_AuraOwned && g_SelectedId >= 1 && !IsDeadNow(g_SelectedId, now)) {
                g_AuraSkipUntil[g_SelectedId] = now + 5000;
            }
            const Candidate* pick = nullptr;
            for (const auto& c : cands) {
                auto it = g_AuraSkipUntil.find(c.id);
                if (it != g_AuraSkipUntil.end() && it->second > now) continue;
                pick = &c;
                break;
            }
            if (pick) {
                g_SelectedId                     = pick->id;
                g_CurTargetGen                   = pick->gen;
                g_cfgState.bKillAroundAutoRepeat = true;
                g_AuraOwned                      = true;
                g_AuraPickMs                     = now;
                g_LastFollowMs                   = 0;
            }
        }
    } else if (g_AuraOwned) {
        g_AuraOwned                      = false;
        g_cfgState.bKillAroundAutoRepeat = false;
        g_SelectedId                     = 0;
    }
    if (burst && cands.empty()) {
        g_BurstUntilMs.store(0, std::memory_order_release);
    }

    // Always keep ghost step enabled (exact kill_around (3).h)
    g_cfgState.bKillAroundGhostStep = true;

    if (g_cfgState.bKillAroundAutoRepeat && g_SelectedId >= 1 && !paused) {
        if (IsDeadNow(g_SelectedId, now)) {
            g_cfgState.bKillAroundAutoRepeat = false;
        } else if ((now - g_LastFollowMs) >= 120) {
            g_LastFollowMs = now;
            Vec3 tp{};
            if (ElemPos(g_SelectedId, tp)) {
                g_CurTargetDist = std::sqrt((tp.x - me.x) * (tp.x - me.x) + (tp.z - me.z) * (tp.z - me.z));
                const float dx = tp.x - me.x, dy = tp.y - me.y, dz = tp.z - me.z;
                g_OutOfReach = std::sqrt(dx * dx + dy * dy + dz * dz) > kMeleeReach;
                if (!g_OutOfReach) {
                    if (g_GhostActive) GhostRestore(me);
                    Burst(g_SelectedId, g_CurTargetGen, 6);
                } else {
                    GhostTo(tp, me);
                    Burst(g_SelectedId, g_CurTargetGen, 6);
                }
            }
        }
    }

    if (g_GhostActive && now >= g_GhostHoldUntil && (!aura || cands.empty()) && !g_cfgState.bKillAroundAutoRepeat) {
        GhostRestore(me);
    }
}

// ---------------- Menu Panel (exact kill_around (3).h DrawSection) ----------------
inline void DrawPanel(float availW) {
    using Lang::T;
    g_cfgState.killAroundRadius = std::clamp(g_cfgState.killAroundRadius, 5.0f, 120.0f);
    g_cfgState.bKillAroundGhostStep = true;

    ImGui::TextColored(ImVec4(1.00f, 0.22f, 0.42f, 1.00f), "DEATH WISH");
    ImGui::Separator();
    ThemeEngine::ProToggle(T("Auto-repeat kill selected target", "تكرار قتل الهدف المحدد تلقائياً"), &g_cfgState.bKillAroundAutoRepeat);
    ThemeEngine::ProToggle(T("One-hit on my swings while active", "ضربة قاضية بضرباتي أثناء التفعيل"), &g_cfgState.bKillAroundOneHit);
    ThemeEngine::ProSlider(T("Radius (m)", "مدى القتل (متر)"), &g_cfgState.killAroundRadius, 5.0f, 120.0f, "%.0f m");
    ImGui::TextColored(ImVec4(0.30f, 1.00f, 0.50f, 1.00f), "%s",
                       T("Ghost step: always on (near and far)", "الانتقال الخفي: مفعّل دائماً (قريب وبعيد)"));

    const char* auraLbl = g_cfgState.bKillAroundAura
        ? T("KILL EVERYONE IN RANGE: ON  (tap to stop)##ka_btn", "قتل الجميع في المدى: مفعّل (اضغط للإيقاف)##ka_btn")
        : T("KILL EVERYONE IN RANGE: OFF (tap to start)##ka_btn", "قتل الجميع في المدى: متوقف (اضغط للتشغيل)##ka_btn");
    if (ThemeEngine::NeonActionButton(auraLbl, std::max(160.0f, availW - 4.0f), 40.0f, g_cfgState.bKillAroundAura)) {
        g_cfgState.bKillAroundAura = !g_cfgState.bKillAroundAura;
        if (!g_cfgState.bKillAroundAura) {
            g_BurstUntilMs.store(0, std::memory_order_release);
        }
    }

    ImGui::Spacing();
    const int64_t now = MonoMs();
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

    const bool hasClient = Il2Cpp::IsUnityObjectSafe(g_KaClient) ||
                           Il2Cpp::IsUnityObjectSafe(g_metaClient.load(std::memory_order_relaxed));
    const int txMode = g_TxMode.load(std::memory_order_relaxed);
    const char* txTag = (txMode == 2) ? "Wire" : (txMode == 1 ? "Gen" : "-");
    ImGui::TextDisabled("Net:%s | Time:%s | Tx:%d/%d(%s) | Disp:%d",
                        hasClient ? "OK" : "Wait",
                        (g_IncomingRemoteMs.load(std::memory_order_relaxed) != 0) ? "OK" : "0",
                        g_SendOk.load(std::memory_order_relaxed),
                        g_SendFail.load(std::memory_order_relaxed),
                        txTag,
                        g_DispHits.load(std::memory_order_relaxed));

    if (g_LocalId.load(std::memory_order_relaxed) < 1) {
        ImGui::TextColored(ImVec4(1.00f, 0.80f, 0.20f, 1.00f), "%s",
            T("Reading your element id from the game. Nothing sends until then.",
              "جاري قراءة معرف اللاعب من اللعبة... لن يتم الإرسال حتى يظهر."));
    }

    for (const auto& c : all) {
        ImGui::PushID(c.id);
        std::string nm;
        const bool named = LookupName(c.id, &nm);
        const int32_t digi = DigiForElem(c.id);
        if (digi > 0) {
            ImGui::Text("%s  #%d (ID:%d)  [%.1fm]%s",
                        named ? nm.c_str() : "Player",
                        c.id, digi, c.dist, c.dead ? " [DEAD]" : "");
        } else {
            ImGui::Text("%s  #%d  [%.1fm]%s",
                        named ? nm.c_str() : "Player",
                        c.id, c.dist, c.dead ? " [DEAD]" : "");
        }
        if (!c.dead) {
            ImGui::SameLine(std::max(180.0f, availW - 110.0f));
            if (ImGui::Button(T("KILL##kab", "قتل##kab"), ImVec2(96.0f, 32.0f))) {
                g_SelectedId                     = c.id;
                g_CurTargetGen                   = c.gen;
                g_cfgState.bKillAroundAutoRepeat = true;
                g_AuraOwned                      = false;
                g_LastFollowMs                   = 0;
            }
        }
        ImGui::PopID();
    }

    if (g_SelectedId > 0) {
        ImGui::Spacing();
        std::string selNm;
        const bool selNamed = LookupName(g_SelectedId, &selNm);
        ImGui::Text("%s: %s #%d  |  %s: %d",
                    T("Selected", "المحدد"),
                    selNamed ? selNm.c_str() : "",
                    g_SelectedId,
                    T("Accepted hits", "الضربات المقبولة"),
                    g_AcceptedHits.load(std::memory_order_relaxed));
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
        if (ThemeEngine::SecondaryPillButton(T("Stop##kastp", "إيقاف##kastp"), std::max(160.0f, availW - 4.0f), 34.0f, true)) {
            g_cfgState.bKillAroundAutoRepeat = false;
            g_cfgState.bKillAroundAura       = false;
            g_SelectedId                     = 0;
            g_AuraOwned                      = false;
            g_BurstUntilMs.store(0, std::memory_order_release);
        }
    }
}

} // namespace KillAround
