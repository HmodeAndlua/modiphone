#pragma once
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#include <mach/mach.h>
#include <mach/thread_status.h>
#include <dlfcn.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Offsets.h"
#include "Catalogs.h"

extern "C" {
kern_return_t mach_vm_read_overwrite(vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size,
                                     mach_vm_address_t data, mach_vm_size_t* outsize);
kern_return_t mach_vm_write(vm_map_t target_task, mach_vm_address_t address, vm_offset_t data,
                            mach_msg_type_number_t dataCnt);
}

// ============================================================================
// 1. Safe Memory Layer
// ============================================================================
namespace SafeMem {

inline bool IsPlausiblePtr(const void* p) {
    uintptr_t u = reinterpret_cast<uintptr_t>(p);
    return (u > 0x100000000ULL && u < 0xFFFFFFFF00000000ULL);
}

inline bool Read(const void* src, void* dst, size_t size) {
    if (!IsPlausiblePtr(src) || !dst || size == 0) return false;
    mach_vm_size_t outSize = 0;
    kern_return_t kr = mach_vm_read_overwrite(mach_task_self(),
                                              reinterpret_cast<mach_vm_address_t>(src),
                                              size,
                                              reinterpret_cast<mach_vm_address_t>(dst),
                                              &outSize);
    return (kr == KERN_SUCCESS && outSize == size);
}

inline bool Write(void* dst, const void* src, size_t size) {
    if (!IsPlausiblePtr(dst) || !src || size == 0) return false;
    kern_return_t kr = mach_vm_write(mach_task_self(),
                                     reinterpret_cast<mach_vm_address_t>(dst),
                                     reinterpret_cast<vm_offset_t>(src),
                                     static_cast<mach_msg_type_number_t>(size));
    return (kr == KERN_SUCCESS);
}

template <typename T>
inline bool ReadVal(const void* src, T& out) {
    return Read(src, &out, sizeof(T));
}

inline void* PtrAt(const void* base, size_t offset) {
    if (!IsPlausiblePtr(base)) return nullptr;
    void* val = nullptr;
    if (!Read(reinterpret_cast<const uint8_t*>(base) + offset, &val, sizeof(void*))) return nullptr;
    return IsPlausiblePtr(val) ? val : nullptr;
}

} // namespace SafeMem

// ============================================================================
// 1B. TCFlyGuard — Exact 6-Signal Nested Stack Crash Guard (matches StateScript.dylib)
// ============================================================================
namespace TCFlyGuard {

inline constexpr int kMaxDepth = 8;
inline constexpr int kSigCount = 6;
inline constexpr int kSignals[kSigCount] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP, SIGABRT };

inline thread_local sigjmp_buf s_jmpStack[kMaxDepth];
inline thread_local volatile sig_atomic_t s_depth = 0;

inline struct sigaction s_prevActions[kSigCount]{};
inline std::atomic<bool> s_installed{false};

inline int FindSigIndex(int sig) {
    for (int i = 0; i < kSigCount; ++i) {
        if (kSignals[i] == sig) return i;
    }
    return -1;
}

inline void SignalHandler(int sig, siginfo_t* info, void* uctx) {
    int d = static_cast<int>(s_depth);
    if (d > 0 && d <= kMaxDepth) {
        s_depth = d - 1;
        siglongjmp(s_jmpStack[d - 1], sig ? sig : 1);
    }
    int idx = FindSigIndex(sig);
    if (idx >= 0) {
        const struct sigaction& prev = s_prevActions[idx];
        if ((prev.sa_flags & SA_SIGINFO) && prev.sa_sigaction && prev.sa_sigaction != SignalHandler) {
            prev.sa_sigaction(sig, info, uctx);
            return;
        }
        if (!(prev.sa_flags & SA_SIGINFO) && prev.sa_handler != SIG_DFL && prev.sa_handler != SIG_IGN && prev.sa_handler != nullptr) {
            prev.sa_handler(sig);
            return;
        }
        sigaction(sig, &prev, nullptr);
    } else {
        signal(sig, SIG_DFL);
    }
    raise(sig);
}

inline void InstallOnce() {
    if (s_installed.load(std::memory_order_acquire)) return;
    bool expected = false;
    if (!s_installed.compare_exchange_strong(expected, true)) return;

    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO; // Exact StateScript.dylib flags (0x40)
    sa.sa_sigaction = SignalHandler;
    for (int i = 0; i < kSigCount; ++i) {
        struct sigaction oldSa{};
        sigaction(kSignals[i], &sa, &oldSa);
        if (oldSa.sa_sigaction != SignalHandler) {
            s_prevActions[i] = oldSa;
        }
    }
}

template <typename Fn>
inline bool TryRun(Fn&& fn) {
    InstallOnce();
    int d = static_cast<int>(s_depth);
    if (d < 0 || d >= kMaxDepth) {
        return false;
    }
    if (sigsetjmp(s_jmpStack[d], 1) == 0) {
        s_depth = d + 1;
        fn();
        s_depth = d;
        return true;
    }
    s_depth = d;
    return false;
}

} // namespace TCFlyGuard

// ============================================================================
// 2. Geometry & Shared Runtime State (all __DATA,__data defaults preserved)
// ============================================================================
struct Vec2 { float x = 0, y = 0; };
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };

inline float Vec3Dist(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline Vec3 Vec3Norm(Vec3 v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-6f) return {0, 0, 1};
    float inv = 1.0f / len;
    return { v.x * inv, v.y * inv, v.z * inv };
}

inline bool IsFiniteVec3(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
           std::fabs(v.x) < 1e6f && std::fabs(v.y) < 1e6f && std::fabs(v.z) < 1e6f;
}

inline Vec3 QuatForward(const Quat& q) {
    return Vec3Norm({
        2.0f * (q.x * q.z + q.w * q.y),
        2.0f * (q.y * q.z - q.w * q.x),
        1.0f - 2.0f * (q.x * q.x + q.y * q.y)
    });
}

inline Vec3 QuatRight(const Quat& q) {
    return Vec3Norm({
        1.0f - 2.0f * (q.y * q.y + q.z * q.z),
        2.0f * (q.x * q.y + q.w * q.z),
        2.0f * (q.x * q.z - q.w * q.y)
    });
}

inline Quat LookRotationQuat(Vec3 forward) {
    Vec3 f = Vec3Norm(forward);
    Vec3 up{0.0f, 1.0f, 0.0f};
    if (std::fabs(f.y) > 0.995f) up = {0.0f, 0.0f, 1.0f};
    // right = normalize(cross(up, f))
    Vec3 r = Vec3Norm({
        up.y * f.z - up.z * f.y,
        up.z * f.x - up.x * f.z,
        up.x * f.y - up.y * f.x
    });
    // true up = cross(f, r)
    Vec3 u{
        f.y * r.z - f.z * r.y,
        f.z * r.x - f.x * r.z,
        f.x * r.y - f.y * r.x
    };
    float m00 = r.x, m01 = u.x, m02 = f.x;
    float m10 = r.y, m11 = u.y, m12 = f.y;
    float m20 = r.z, m21 = u.z, m22 = f.z;
    float tr = m00 + m11 + m22;
    Quat q{};
    if (tr > 0.0f) {
        float s = std::sqrt(tr + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    return q;
}

struct ESPPlayer {
    int32_t entityId = -1;
    uint32_t entityGen = 0;
    int32_t viewId = -1;
    int32_t playerId = -1;
    int64_t pedEntityVal = 0;
    int64_t viewEntityVal = 0;
    int64_t playerEntityVal = 0;
    void* headTransform = nullptr;
    void* chestTransform = nullptr;
    void* rootTransform = nullptr;
    void* boneTransforms[15]{};
    bool hasBoneTransforms = false;
    int32_t skinId = 0;
    uint16_t clothId = 0;
    Vec3 worldPos{};
    Vec3 headPos{};
    Vec3 chestPos{};
    Vec3 velocity{};
    Vec3 bonePoints[15]{};
    bool boneValid[15]{};
    bool hasBones = false;
    int health = 0;
    int armor = 0;
    int armorMax = 1000;
    int roleCategory = 2; // 0=Mates..10=Admin
    std::string name;
    std::string weaponName;
    bool visible = true;
    bool dead = false;
    bool inVehicle = false;
};

struct SprayNode {
    Vec3 worldPos{};
    float distance = 0.0f;
    bool sprayable = false;
    bool clash = false;
    bool nearby = false;
    bool district = false;
    std::string label;
};

struct CheckpointEntry {
    int32_t entityId = -1;
    Vec3 pos{};
    float distance = 0.0f;
    bool isLivraison = false;
    bool isJob = false;
};

struct RuntimeSettings {
    // Visuals / ESP
    bool bEnableESP          = false;
    bool bEspBox             = true;
    bool bEspHealth          = true;
    bool bEspArmor           = true;
    bool bEspName            = true;
    bool bEspPlayerId        = false;
    bool bEspStatus          = false;
    bool bEspDistance        = true;
    bool bEspLine            = false;
    int  lineOrigin          = 1; // 0=Top, 1=Bottom, 2=Center
    int  lineTarget          = 1;
    float lineThickness      = 1.5f;
    bool bSkeletonESP        = false;
    bool bEspCount           = true;
    bool bEspWeapon          = true;
    bool bShowNpcs           = false;
    bool bMiniMapEsp         = false;
    int  espBoxStyle         = 0; // 0=Full, 1=Corners
    float espRadiusValue     = 200.0f;
    float espBoxCol[3]       = { 0.42f, 0.50f, 1.00f };
    float espLineCol[3]      = { 0.35f, 0.72f, 1.00f };
    float espSkelCol[3]      = { 0.92f, 0.94f, 1.00f };

    // Spray ESP & Auto-TP
    bool bSprayESP             = false;
    bool bSprayEspOnlySprayable= true;
    bool bSprayEspOnlyClash    = false;
    bool bSprayEspOnlyNear     = false;
    bool bSprayEspOnlyDistrict = false;
    bool bSprayEspLines        = false;
    bool bSprayEspBox          = true;
    float sprayEspDistance     = 100.0f;
    float sprayEspCol[3]       = { 1.0f, 0.549f, 0.0f };
    bool bSprayAutoTp          = false;
    float sprayAutoTpDelay     = 2.5f;

    // 11 Role Categories + RGB defaults (0x55d138..0x55d1bb)
    bool bShowRole[11]  = { true,true,true,true,true,true,true,true,true,true,true };
    bool bHideRole[11]  = { false,false,false,false,false,false,false,false,false,false,false };
    uint8_t roleR[11]   = { 0x39, 0xff, 0xaa, 0x14, 0x80, 0xfd, 0xff, 0xff, 0xaa, 0x40, 0x69 };
    uint8_t roleG[11]   = { 0xff, 0xbe, 0xaa, 0x3c, 0x8c, 0xfd, 0x91, 0xaf, 0x19, 0xe0, 0x1e };
    uint8_t roleB[11]   = { 0x14, 0x32, 0xaa, 0xb4, 0x3c, 0x96, 0x00, 0xaf, 0x19, 0xd0, 0xa0 };

    // Aimbot / Silent Aim / Weapon Mods
    bool bEnableAimbot       = false;
    bool bSilentAim          = false;
    bool bSilentWallbang     = true;  // Wallbang via LayersMask PedCombination on RayCastAttackRanged HWBP
    bool bSmoothCamAim       = true;
    bool bDrawFov            = true;
    int  aimBoneMode         = 0; // 0=Head, 1=Body
    bool bAimIndicator       = true;
    bool bVisCheck           = true;
    bool bPrediction         = false;
    float aimFov             = 400.0f;
    float aimSmoothing       = 0.33f;
    float saRadius           = 101.0f;
    float bulletSpeed        = 800.0f;
    float predictionFactor   = 0.0f;
    float saVehDepth         = 0.1f;
    bool bLockRole[11]       = { true,true,true,true,true,true,true,true,true,true,true };
    bool bInstantReload      = false;
    bool bNoRecoil           = false;
    bool bRapidFire          = false;
    float rapidFireMult      = 2.0f;

    // Player & World
    bool bInfStamina         = false;
    bool bAutoHeal           = false;
    int  autoHealUnderHp     = 65;
    bool bAutoVest           = false;
    int  autoVestUnderPct    = 50;
    bool bNoFall             = false;
    bool bAntiTaser          = false;
    bool bAntiFaint          = false;
    bool bGreenZoneBypass    = false;
    bool bZoneGuard          = false;
    bool bSuperJump          = false;
    float superJumpForce     = 15.0f;
    bool bSuperPunch         = false;
    float superPunchSpeed    = 2.0f;
    bool bFlyHack            = false;
    float flySpeed           = 10.0f;
    bool bShowJoystick       = false;
    float joystickSpeed      = 15.0f;
    float joyStickX          = 0.0f;
    float joyStickY          = 0.0f;
    float joyStickZ          = 0.0f;
    bool bSpeedHack          = false;
    float speedMultiplier    = 2.0f;
    bool bScaleEnable        = false;
    bool bBigHead            = true;
    bool bBigChest           = false;
    bool bBigBody            = false;
    float playerScale        = 3.0f;
    bool bHeadHitInvis       = false;
    bool bAntiTp             = false;
    bool bAntiTpCar          = false;
    bool bAntiAfk            = false;
    bool bFreeCam            = false;
    float freeCamSpeed       = 24.0f;
    bool bFreeCamGhostVoice  = true;   // Ghost Proxy underground anchor + FMOD 3D/Mode2D listener for far-player voice
    float freeCamGhostDepthY = -19.5f; // Underground anchor Y offset (<= 25m voice bubble radius, safely beneath ground)
    float freeCamOrbitDist   = 4.2f;   // Spectate orbit distance (m)
    float freeCamOrbitYaw    = 0.0f;   // Spectate horizontal orbit angle (deg)
    float freeCamOrbitPitch  = 14.0f;  // Spectate vertical orbit angle (deg)
    bool bFreeCamAutoOrbit   = false;
    bool bFreeCamHeadCam     = false;
    bool bSpectateActive     = false;
    int64_t spectateTargetPedEnt = -1;
    std::string spectateTargetName;
    bool bAutoMapMarkerTp    = false;

    // Kill Around & Single-Target Melee Finisher
    bool bKillAroundAutoRepeat     = false;
    bool bKillAroundAura           = false;
    bool bKillAroundGhostStep      = true;
    bool bKillAroundMicroStep      = false;
    bool bKillAroundIncludeNpcs    = false;
    bool bKillAroundIncludeVehicles= true;
    bool bKillAroundOneHit         = true;
    bool bKillAroundFilterNear     = false;
    float killAroundRadius         = 120.0f;
    bool bNickBypass         = false;
    bool bVoiceGuard         = false;
    bool bDeviceIdSpoof      = false;
    bool bMasterDisable      = false;

    // Crosshair / FX / Camera / DayNight
    bool bCrosshairEnable    = false;
    bool bRemoveGameCrosshair= false;
    int  crosshairType       = 1;
    float crosshairColor[3]  = { 1.0f, 0.0f, 0.0f };
    float crosshairSize      = 18.0f;
    float crosshairThickness = 8.0f;
    float crosshairOpacity   = 255.0f;
    float crosshairGap       = 3.0f;
    bool crosshairOutline    = true;

    bool bDayNightEnable     = false;
    bool bDayNightFreeze     = true;
    float dayNightTime       = 12.0f;

    bool bShotPartsEnable    = false;
    int  shotPalSel          = -1;
    int  shotColR = 200, shotColG = 25, shotColB = 25;
    float cfTint[3]          = { 1.0f, 1.0f, 1.0f };

    bool bDmgNumEnable       = false;
    int  dmgNumStyle         = 0;

    bool bFovEnable          = false;
    float cameraFov          = 100.0f;
    bool bResolutionEnable   = false;
    float aspectScale        = 1.0f;
    float nativeAspect       = 1.0f;

    // Vehicle
    bool bCarHack            = false;
    float carSpeed           = 10.0f;
    bool bCarFly             = false;
    float carFlySpeed        = 30.0f;
    bool bAutoRepair         = false;
    bool bVehSpawnCapture    = false;

    // Radio & Customization Inputs
    bool bRadioEnable        = false;
    int  radioChannel        = 2001; // 0x7d1
    bool bRadioIndicator     = true;
    int  pendingSkinId       = 0;
    int  pendingSkinGender   = 0;
    int  pendingClothId      = 0;
    int  pendingClothGender  = 0;
    int  pendingAccId        = 0;
    int  pendingAccSlot      = 0; // 0=Hat,1=Mask,2=Backpack
    int  pendingEmoteId      = 0;
    int  pendingCarId        = 0;
    int  pendingWeaponId     = 5000;
    float carPaintRgb[3]     = { 0.1f, 0.6f, 0.9f };
    float carWinRgba[4]      = { 0.05f, 0.05f, 0.05f, 0.85f };
    int  pendingRimId        = 0;
    float rimColorRgba[4]    = { 0.9f, 0.9f, 0.9f, 1.0f };
    int  vehTuneExt[10]      = { 0,0,0,0,0,0,0,0,0,0 };
    bool bAutoDanse          = false;
    int  autoDanseId         = 0;

    // Automation & Tracker
    bool bMoneyGlitch        = false;
    float moneyGlitchSpeed   = 12.0f;
    bool bStealEvent         = false;
    bool bJobTp              = false;
    bool bWalkCp             = false;
    bool bVehAiDrive         = false;
    bool bGemsFarm           = false;
    bool bFollowTarget       = false;
    bool bKillFly            = false;
    bool bCarKill            = false;
    bool bTalkFreeCam        = false;
    int64_t targetPlayerPid  = -1;

    // System, Language & 3D Neon Overlay
    float targetFPS          = 60.0f;
    bool bEnableNotifications= true;
    bool bStreamerMode       = false;
    int  thermalEcoLevel     = 1;
    int  languageMode        = 0;    // 0 = English, 1 = Arabic
    bool bNeon3DGrid         = true; // 3D Neon perspective backdrop & glow
    int  neonPreset          = 0;    // 0=Cyber Indigo, 1=Electric Cyan, 2=Hyper Magenta, 3=Emerald, 4=Solar Gold
    std::string activeProfileName = "default";
    std::string lastStatusBanner  = "Ready";
};

struct MapMarkerEntry {
    int32_t entityId = -1;
    Vec3 worldPos{};
    float distance = 0.0f;
    int priority = 0;         // 100=User HudMapCheckpoint pin, 80=GPS route goal, 40=Cached, 15=Mission/World blip
    bool isUserPin = false;
    bool isExactMapPin = false;
    std::string label;
};

struct ServerPlayerEntry {
    int32_t  playerId = -1;
    int32_t  pedId = -1;
    int64_t  pedEntityVal = 0;
    uint32_t digitalId = 0;
    int32_t  health = 100;
    float    distance = -1.0f;
    bool     isStreamed = false;
    Vec3     worldPos{};
    std::string name;
};

struct UnityRay {
    Vec3 origin;
    Vec3 direction;
};

struct UnityRaycastHit {
    Vec3 point;
    Vec3 normal;
    uint32_t faceId;
    float distance;
    Vec2 uv;
    int32_t collider;
};

inline RuntimeSettings g_cfgState{};
inline std::mutex g_espMutex;
inline std::vector<ESPPlayer> g_renderPlayers;
inline std::vector<ServerPlayerEntry> g_allServerPlayers;
inline std::vector<SprayNode> g_sprayNodes;
inline std::vector<CheckpointEntry> g_checkpoints;
inline std::vector<MapMarkerEntry> g_mapMarkers;
inline std::atomic<int64_t> g_localPedEntityAtomic{ -1 };
inline std::atomic<bool>    g_menuIsOpenAtomic{ false };
inline std::atomic<int>     g_activeTabAtomic{ 1 };
inline std::atomic<int>     g_activeSubTabAtomic{ 0 };

// ============================================================================
// 3. IL2CPP Dynamic API Bridge
// ============================================================================
namespace Il2Cpp {

using DomainGetFn         = void* (*)();
using ThreadAttachFn      = void* (*)(void*);
using DomainGetAsmsFn     = void** (*)(void*, size_t*);
using AsmGetImageFn       = void* (*)(void*);
using ClassFromNameFn     = void* (*)(void*, const char*, const char*);
using ClassGetMethodFn    = void* (*)(void*, const char*, int);
using ClassGetFieldFn     = void* (*)(void*, const char*);
using FieldGetOffsetFn    = size_t (*)(void*);
using FieldStaticGetValFn = void (*)(void*, void*);
using RuntimeInvokeFn     = void* (*)(void*, void*, void**, void**);
using ObjectGetClassFn    = void* (*)(void*);
using ObjectUnboxFn       = void* (*)(void*);
using ValueBoxFn          = void* (*)(void*, void*);
using ClassGetTypeFn      = void* (*)(void*);
using TypeGetObjectFn     = void* (*)(void*);
using StringNewFn         = void* (*)(const char*);
using ResolveIcallFn      = void* (*)(const char*);
using ObjectNewFn         = void* (*)(void*);
using ClassArrElemSizeFn  = int32_t (*)(const void*);
using ClassGetElemClsFn   = void* (*)(void*);
using ArrayElemSizeFn     = int32_t (*)(const void*);
using ClassIsValTypeFn    = bool (*)(const void*);
using MethodGetObjFn      = void* (*)(const void*, void*);
using ArrayNewFn          = void* (*)(void*, uintptr_t);
using GcHandleNewFn       = uint32_t (*)(void*, bool);
using GcHandleTargetFn    = void* (*)(uint32_t);
using GcHandleFreeFn      = void (*)(uint32_t);
using ClassStaticDataFn   = void* (*)(void*);

struct Api {
    bool ready = false;
    DomainGetFn         domain_get = nullptr;
    ThreadAttachFn      thread_attach = nullptr;
    DomainGetAsmsFn     domain_get_assemblies = nullptr;
    AsmGetImageFn       assembly_get_image = nullptr;
    ClassFromNameFn     class_from_name = nullptr;
    ClassGetMethodFn    class_get_method_from_name = nullptr;
    ClassGetFieldFn     class_get_field_from_name = nullptr;
    FieldGetOffsetFn    field_get_offset = nullptr;
    FieldStaticGetValFn field_static_get_value = nullptr;
    RuntimeInvokeFn     runtime_invoke = nullptr;
    ObjectGetClassFn    object_get_class = nullptr;
    ObjectUnboxFn       object_unbox = nullptr;
    ValueBoxFn          value_box = nullptr;
    ClassGetTypeFn      class_get_type = nullptr;
    TypeGetObjectFn     type_get_object = nullptr;
    StringNewFn         string_new = nullptr;
    ResolveIcallFn      resolve_icall = nullptr;
    ObjectNewFn         object_new = nullptr;
    ClassArrElemSizeFn  class_array_element_size = nullptr;
    ClassGetElemClsFn   class_get_element_class = nullptr;
    ArrayElemSizeFn     array_element_size = nullptr;
    ClassIsValTypeFn    class_is_valuetype = nullptr;
    MethodGetObjFn      method_get_object = nullptr;
    ArrayNewFn          array_new = nullptr;
    GcHandleNewFn       gchandle_new = nullptr;
    GcHandleTargetFn    gchandle_get_target = nullptr;
    GcHandleFreeFn      gchandle_free = nullptr;
    ClassStaticDataFn   class_get_static_field_data = nullptr;
} g_api;

inline void* g_clsString = nullptr;

inline bool Init() {
    if (g_api.ready) return true;
    auto sym = [](const char* n) -> void* { return dlsym(RTLD_DEFAULT, n); };
    g_api.domain_get                  = (DomainGetFn)sym("il2cpp_domain_get");
    g_api.thread_attach               = (ThreadAttachFn)sym("il2cpp_thread_attach");
    g_api.domain_get_assemblies       = (DomainGetAsmsFn)sym("il2cpp_domain_get_assemblies");
    g_api.assembly_get_image          = (AsmGetImageFn)sym("il2cpp_assembly_get_image");
    g_api.class_from_name             = (ClassFromNameFn)sym("il2cpp_class_from_name");
    g_api.class_get_method_from_name  = (ClassGetMethodFn)sym("il2cpp_class_get_method_from_name");
    g_api.class_get_field_from_name   = (ClassGetFieldFn)sym("il2cpp_class_get_field_from_name");
    g_api.field_get_offset            = (FieldGetOffsetFn)sym("il2cpp_field_get_offset");
    g_api.field_static_get_value      = (FieldStaticGetValFn)sym("il2cpp_field_static_get_value");
    g_api.runtime_invoke              = (RuntimeInvokeFn)sym("il2cpp_runtime_invoke");
    g_api.object_get_class            = (ObjectGetClassFn)sym("il2cpp_object_get_class");
    g_api.object_unbox                = (ObjectUnboxFn)sym("il2cpp_object_unbox");
    g_api.value_box                   = (ValueBoxFn)sym("il2cpp_value_box");
    g_api.class_get_type              = (ClassGetTypeFn)sym("il2cpp_class_get_type");
    g_api.type_get_object             = (TypeGetObjectFn)sym("il2cpp_type_get_object");
    g_api.string_new                  = (StringNewFn)sym("il2cpp_string_new");
    g_api.resolve_icall               = (ResolveIcallFn)sym("il2cpp_resolve_icall");
    g_api.object_new                  = (ObjectNewFn)sym("il2cpp_object_new");
    g_api.class_array_element_size    = (ClassArrElemSizeFn)sym("il2cpp_class_array_element_size");
    g_api.class_get_element_class     = (ClassGetElemClsFn)sym("il2cpp_class_get_element_class");
    g_api.array_element_size          = (ArrayElemSizeFn)sym("il2cpp_array_element_size");
    g_api.class_is_valuetype          = (ClassIsValTypeFn)sym("il2cpp_class_is_valuetype");
    g_api.method_get_object           = (MethodGetObjFn)sym("il2cpp_method_get_object");
    g_api.array_new                   = (ArrayNewFn)sym("il2cpp_array_new");
    g_api.gchandle_new                = (GcHandleNewFn)sym("il2cpp_gchandle_new");
    g_api.gchandle_get_target         = (GcHandleTargetFn)sym("il2cpp_gchandle_get_target");
    g_api.gchandle_free               = (GcHandleFreeFn)sym("il2cpp_gchandle_free");
    g_api.class_get_static_field_data = (ClassStaticDataFn)sym("il2cpp_class_get_static_field_data");
    g_api.ready = (g_api.domain_get && g_api.class_from_name && g_api.class_get_method_from_name && g_api.runtime_invoke);
    return g_api.ready;
}

inline bool IsUnityObjectSafe(const void* obj) {
    if (!SafeMem::IsPlausiblePtr(obj)) return false;
    void* klass = nullptr;
    if (!SafeMem::ReadVal(obj, klass)) return false;
    return SafeMem::IsPlausiblePtr(klass);
}

inline bool IsUnityNativeAlive(const void* unityObj) {
    if (!IsUnityObjectSafe(unityObj)) return false;
    uintptr_t cachedPtr = 0;
    if (!SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(unityObj) + 0x10, cachedPtr)) return false;
    return (cachedPtr > 0x10000ULL && cachedPtr < 0xFFFFFFFF00000000ULL);
}

// Verifies BOTH the exact managed Il2CppClass* AND non-null native m_CachedPtr (+0x10)
// before calling any UnityEngine.*_Injected icall (prevents native type-confusion crashes).
inline bool IsExactUnityTypeAlive(const void* unityObj, void* expectedCls, void* altCls = nullptr) {
    if (!SafeMem::IsPlausiblePtr(unityObj)) return false;
    void* objCls = nullptr;
    if (!SafeMem::ReadVal(unityObj, objCls) || !SafeMem::IsPlausiblePtr(objCls)) return false;
    if (expectedCls) {
        if (objCls != expectedCls && (!altCls || objCls != altCls)) return false;
    }
    uintptr_t cachedPtr = 0;
    if (!SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(unityObj) + 0x10, cachedPtr)) return false;
    return (cachedPtr > 0x10000ULL && cachedPtr < 0xFFFFFFFF00000000ULL);
}

inline void EnsureThreadAttached() {
    static thread_local bool attached = false;
    if (attached || !Init() || !g_api.thread_attach) return;
    if (void* dom = g_api.domain_get()) {
        g_api.thread_attach(dom);
        attached = true;
    }
}

inline void* FindClass(const char* nsName, const char* clsName) {
    if (!Init() || !clsName || !clsName[0] || !g_api.domain_get_assemblies || !g_api.assembly_get_image) return nullptr;
    void* dom = g_api.domain_get();
    if (!dom) return nullptr;
    size_t count = 0;
    void** asms = g_api.domain_get_assemblies(dom, &count);
    if (!asms) return nullptr;
    const char* ns = nsName ? nsName : "";
    for (size_t i = 0; i < count; ++i) {
        void* img = g_api.assembly_get_image(asms[i]);
        if (!img) continue;
        if (void* klass = g_api.class_from_name(img, ns, clsName)) return klass;
    }
    return nullptr;
}

inline void* ResolveMethod(const char* nsName, const char* clsName, const char* methodName, int argc) {
    void* klass = FindClass(nsName, clsName);
    if (!klass) return nullptr;
    return g_api.class_get_method_from_name(klass, methodName, argc);
}

inline size_t FieldOffset(void* klass, const char* fieldName, size_t fallback = 0) {
    if (!SafeMem::IsPlausiblePtr(klass) || !fieldName || !g_api.class_get_field_from_name || !g_api.field_get_offset)
        return fallback;
    size_t out = fallback;
    TCFlyGuard::TryRun([&]() {
        if (void* fld = g_api.class_get_field_from_name(klass, fieldName)) {
            size_t off = g_api.field_get_offset(fld);
            if (off != 0) out = off;
        }
    });
    return out;
}

// Unboxed C# struct field offset: il2cpp_field_get_offset always includes +0x10 boxed Il2CppObject header
// on value types, whereas Morpeh Stash<T>.data stores T[] unboxed at 0-based offsets.
inline size_t StructFieldOffset(void* klass, const char* fieldName, size_t fallback = 0) {
    if (!SafeMem::IsPlausiblePtr(klass) || !fieldName || !g_api.class_get_field_from_name || !g_api.field_get_offset)
        return fallback;
    size_t out = fallback;
    bool found = false;
    TCFlyGuard::TryRun([&]() {
        if (void* fld = g_api.class_get_field_from_name(klass, fieldName)) {
            size_t rawOff = g_api.field_get_offset(fld);
            bool isVal = g_api.class_is_valuetype ? g_api.class_is_valuetype(klass) : true;
            if (isVal && rawOff >= 0x10) {
                out = rawOff - 0x10;
                found = true;
            } else if (!isVal && rawOff != 0) {
                out = rawOff;
                found = true;
            }
        }
    });
    return found ? out : fallback;
}

inline void* Unbox(void* obj) {
    if (!SafeMem::IsPlausiblePtr(obj)) return nullptr;
    return g_api.object_unbox ? g_api.object_unbox(obj) : (reinterpret_cast<uint8_t*>(obj) + 0x10);
}

inline std::string ReadManagedString(void* strObj) {
    if (!SafeMem::IsPlausiblePtr(strObj)) return {};
    if (g_clsString) {
        void* objCls = nullptr;
        if (!SafeMem::ReadVal(strObj, objCls) || objCls != g_clsString) return {};
    }
    int32_t len = 0;
    if (!SafeMem::ReadVal(reinterpret_cast<const uint8_t*>(strObj) + 0x10, len) || len <= 0 || len > 512)
        return {};
    std::string out;
    out.reserve(static_cast<size_t>(len));
    const uint16_t* chars = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(strObj) + 0x14);
    for (int32_t i = 0; i < len; ++i) {
        uint16_t ch = 0;
        if (!SafeMem::ReadVal(&chars[i], ch)) break;
        if (ch < 0x80) {
            out.push_back(static_cast<char>(ch));
        } else if (ch < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (ch >> 6)));
            out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (ch >> 12)));
            out.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
    }
    return out;
}

inline bool SwapMethodPointer(void* methodInfo, void* replacement, void** outOrig) {
    if (!SafeMem::IsPlausiblePtr(methodInfo) || !replacement) return false;
    void* orig = SafeMem::PtrAt(methodInfo, 0);
    if (!SafeMem::IsPlausiblePtr(orig) || orig == replacement) return false;
    if (outOrig) *outOrig = orig;
    return SafeMem::Write(methodInfo, &replacement, sizeof(void*));
}

// Patches BOTH MethodInfo->methodPointer AND every matching VirtualInvokeData {orig, methodInfo} slot
// inside Il2CppClass->vtable[] (writable heap memory — zero __TEXT patches, works on jailed & jailbroken iOS).
inline bool SwapVirtualMethod(const char* nsName, const char* clsName, const char* methodName, int argc,
                              void* replacement, void** outOrig) {
    if (!clsName || !methodName || !replacement || !Init()) return false;
    void* klass = FindClass(nsName, clsName);
    if (!SafeMem::IsPlausiblePtr(klass) || !g_api.class_get_method_from_name) return false;
    void* methodInfo = nullptr;
    TCFlyGuard::TryRun([&]() {
        methodInfo = g_api.class_get_method_from_name(klass, methodName, argc);
    });
    if (!SafeMem::IsPlausiblePtr(methodInfo)) return false;
    void* orig = SafeMem::PtrAt(methodInfo, 0);
    if (!SafeMem::IsPlausiblePtr(orig) || orig == replacement) return false;
    if (outOrig) *outOrig = orig;

    bool patchedMethod = SafeMem::Write(methodInfo, &replacement, sizeof(void*));
    uint8_t* kBase = reinterpret_cast<uint8_t*>(klass);
    for (size_t off = 0x100; off < 0x3800; off += sizeof(void*)) {
        void* pair[2]{};
        if (!SafeMem::Read(kBase + off, pair, sizeof(pair))) break;
        if (pair[0] == orig && pair[1] == methodInfo) {
            SafeMem::Write(kBase + off, &replacement, sizeof(void*));
        }
    }
    return patchedMethod;
}

inline void* SafeInvoke(void* methodInfo, void* instance, void** params, void** outExc = nullptr) {
    if (!SafeMem::IsPlausiblePtr(methodInfo) || !g_api.runtime_invoke) return nullptr;
    if (instance && !SafeMem::IsPlausiblePtr(instance)) return nullptr;
    void* exc = nullptr;
    void* ret = nullptr;
    bool ok = TCFlyGuard::TryRun([&]() {
        ret = g_api.runtime_invoke(methodInfo, instance, params, &exc);
    });
    if (outExc) *outExc = exc;
    return (ok && !exc) ? ret : nullptr;
}

inline void* ResolveStaticClassFieldPtr(const char* nsName, const char* clsName, const char* fieldName) {
    if (!clsName || !fieldName || !Init() || !g_api.class_get_field_from_name || !g_api.field_static_get_value)
        return nullptr;
    void* k = FindClass(nsName, clsName);
    if (!k) return nullptr;
    void* fld = g_api.class_get_field_from_name(k, fieldName);
    if (!fld) return nullptr;
    void* outPtr = nullptr;
    TCFlyGuard::TryRun([&]() { g_api.field_static_get_value(fld, &outPtr); });
    return SafeMem::IsPlausiblePtr(outPtr) ? outPtr : nullptr;
}

// ============================================================================
// 3B. Direct Unity Internal Calls (ZERO Boehm GC Heap Allocations per frame)
// ============================================================================
struct UnityIcalls {
    bool resolved = false;
    void* (*Camera_get_main)() = nullptr;
    void  (*Camera_WorldToViewportPoint_Injected)(void* cam, const Vec3* pos, int32_t eye, Vec3* outVp) = nullptr;
    void  (*Camera_get_worldToCameraMatrix_Injected)(void* cam, float* outMat16) = nullptr;
    void  (*Camera_get_projectionMatrix_Injected)(void* cam, float* outMat16) = nullptr;
    float (*Camera_get_fov)(void* cam) = nullptr;
    void  (*Camera_set_fov)(void* cam, float fov) = nullptr;
    float (*Camera_get_aspect)(void* cam) = nullptr;
    void  (*Camera_set_aspect)(void* cam, float aspect) = nullptr;
    void* (*Comp_get_transform)(void* comp) = nullptr;
    void* (*Comp_get_gameObject)(void* comp) = nullptr;
    void  (*GameObject_SetActive)(void* go, bool active) = nullptr;
    void* (*GameObject_CreatePrimitive)(int32_t primType) = nullptr;
    void  (*GameObject_set_layer)(void* go, int32_t layer) = nullptr;
    void  (*Renderer_set_enabled)(void* rend, bool enabled) = nullptr;
    void  (*Transform_get_position_Injected)(void* tr, Vec3* outPos) = nullptr;
    void  (*Transform_set_position_Injected)(void* tr, const Vec3* pos) = nullptr;
    void  (*Transform_get_rotation_Injected)(void* tr, Quat* outRot) = nullptr;
    void  (*Transform_set_rotation_Injected)(void* tr, const Quat* rot) = nullptr;
    void  (*Transform_get_localScale_Injected)(void* tr, Vec3* outScale) = nullptr;
    void  (*Transform_set_localScale_Injected)(void* tr, const Vec3* scale) = nullptr;
    void  (*Rigidbody_get_velocity_Injected)(void* rb, Vec3* outVel) = nullptr;
    void  (*Rigidbody_set_velocity_Injected)(void* rb, const Vec3* vel) = nullptr;
    void  (*Rigidbody_set_position_Injected)(void* rb, const Vec3* pos) = nullptr;
    void  (*Rigidbody_set_rotation_Injected)(void* rb, const Quat* rot) = nullptr;
    void  (*Rigidbody_set_isKinematic)(void* rb, bool val) = nullptr;
    void  (*Rigidbody_set_useGravity)(void* rb, bool val) = nullptr;
    void  (*Rigidbody_set_detectCollisions)(void* rb, bool val) = nullptr;
    void  (*Behaviour_set_enabled)(void* beh, bool enabled) = nullptr;
    void  (*Physics_SyncTransforms)() = nullptr;
    void  (*Physics_get_defaultPhysicsScene_Injected)(int32_t* outScene) = nullptr;
    bool  (*PhysicsScene_Internal_Raycast_Injected)(const int32_t* scene, const UnityRay* ray, float maxDist,
                                                    UnityRaycastHit* outHit, int32_t layerMask, int32_t queryTrigger) = nullptr;
    void  (*Animator_set_speed)(void* anim, float spd) = nullptr;
    void* (*Animator_GetBoneTransformInternal)(void* anim, int32_t humanBoneId) = nullptr;
    void  (*Time_set_timeScale)(float ts) = nullptr;
    void  (*App_set_targetFrameRate)(int32_t fps) = nullptr;
} g_icall;

template <typename FnT>
inline void ResolveOneIcall(FnT& dst, const char* sig) {
    if (dst || !g_api.resolve_icall) return;
    void* p = nullptr;
    TCFlyGuard::TryRun([&]() { p = g_api.resolve_icall(sig); });
    if (SafeMem::IsPlausiblePtr(p)) dst = reinterpret_cast<FnT>(p);
}

inline void ResolveUnityIcalls() {
    if (!Init() || !g_api.resolve_icall) return;
    ResolveOneIcall(g_icall.Camera_get_main,                     "UnityEngine.Camera::get_main");
    ResolveOneIcall(g_icall.Camera_WorldToViewportPoint_Injected,"UnityEngine.Camera::WorldToViewportPoint_Injected");
    ResolveOneIcall(g_icall.Camera_get_worldToCameraMatrix_Injected, "UnityEngine.Camera::get_worldToCameraMatrix_Injected");
    ResolveOneIcall(g_icall.Camera_get_projectionMatrix_Injected,    "UnityEngine.Camera::get_projectionMatrix_Injected");
    ResolveOneIcall(g_icall.Camera_get_fov,                      "UnityEngine.Camera::get_fieldOfView");
    ResolveOneIcall(g_icall.Camera_set_fov,                      "UnityEngine.Camera::set_fieldOfView");
    ResolveOneIcall(g_icall.Camera_get_aspect,                   "UnityEngine.Camera::get_aspect");
    ResolveOneIcall(g_icall.Camera_set_aspect,                   "UnityEngine.Camera::set_aspect");
    ResolveOneIcall(g_icall.Comp_get_transform,                  "UnityEngine.Component::get_transform");
    ResolveOneIcall(g_icall.Comp_get_gameObject,                 "UnityEngine.Component::get_gameObject");
    ResolveOneIcall(g_icall.GameObject_SetActive,                "UnityEngine.GameObject::SetActive");
    ResolveOneIcall(g_icall.GameObject_CreatePrimitive,          "UnityEngine.GameObject::CreatePrimitive");
    ResolveOneIcall(g_icall.GameObject_set_layer,                "UnityEngine.GameObject::set_layer");
    ResolveOneIcall(g_icall.Renderer_set_enabled,                "UnityEngine.Renderer::set_enabled");
    ResolveOneIcall(g_icall.Transform_get_position_Injected,     "UnityEngine.Transform::get_position_Injected");
    ResolveOneIcall(g_icall.Transform_set_position_Injected,     "UnityEngine.Transform::set_position_Injected");
    ResolveOneIcall(g_icall.Transform_get_rotation_Injected,     "UnityEngine.Transform::get_rotation_Injected");
    ResolveOneIcall(g_icall.Transform_set_rotation_Injected,     "UnityEngine.Transform::set_rotation_Injected");
    ResolveOneIcall(g_icall.Transform_get_localScale_Injected,   "UnityEngine.Transform::get_localScale_Injected");
    ResolveOneIcall(g_icall.Transform_set_localScale_Injected,   "UnityEngine.Transform::set_localScale_Injected");
    ResolveOneIcall(g_icall.Rigidbody_get_velocity_Injected,     "UnityEngine.Rigidbody::get_velocity_Injected");
    ResolveOneIcall(g_icall.Rigidbody_set_velocity_Injected,     "UnityEngine.Rigidbody::set_velocity_Injected");
    ResolveOneIcall(g_icall.Rigidbody_set_position_Injected,     "UnityEngine.Rigidbody::set_position_Injected");
    ResolveOneIcall(g_icall.Rigidbody_set_rotation_Injected,     "UnityEngine.Rigidbody::set_rotation_Injected");
    ResolveOneIcall(g_icall.Rigidbody_set_isKinematic,           "UnityEngine.Rigidbody::set_isKinematic");
    ResolveOneIcall(g_icall.Rigidbody_set_useGravity,            "UnityEngine.Rigidbody::set_useGravity");
    ResolveOneIcall(g_icall.Rigidbody_set_detectCollisions,      "UnityEngine.Rigidbody::set_detectCollisions");
    ResolveOneIcall(g_icall.Behaviour_set_enabled,               "UnityEngine.Behaviour::set_enabled");
    ResolveOneIcall(g_icall.Physics_SyncTransforms,              "UnityEngine.Physics::SyncTransforms");
    ResolveOneIcall(g_icall.Physics_get_defaultPhysicsScene_Injected, "UnityEngine.Physics::get_defaultPhysicsScene_Injected");
    ResolveOneIcall(g_icall.PhysicsScene_Internal_Raycast_Injected,   "UnityEngine.PhysicsScene::Internal_Raycast_Injected");
    ResolveOneIcall(g_icall.Animator_set_speed,                  "UnityEngine.Animator::set_speed");
    ResolveOneIcall(g_icall.Animator_GetBoneTransformInternal,   "UnityEngine.Animator::GetBoneTransformInternal");
    ResolveOneIcall(g_icall.Time_set_timeScale,                  "UnityEngine.Time::set_timeScale");
    ResolveOneIcall(g_icall.App_set_targetFrameRate,             "UnityEngine.Application::set_targetFrameRate");
    g_icall.resolved = (g_icall.Camera_get_main != nullptr && g_icall.Transform_get_position_Injected != nullptr);
}

inline void* GetSingletonStaticInstance(const char* nsName, const char* clsName) {
    if (!clsName || !clsName[0] || !Init() || !g_api.class_get_field_from_name || !g_api.field_static_get_value)
        return nullptr;
    void* klass = FindClass(nsName, clsName);
    if (!klass) return nullptr;
    const char* flds[] = { "_instance", "instance", "Instance", "<Instance>k__BackingField", "s_instance" };
    for (const char* f : flds) {
        if (void* fld = g_api.class_get_field_from_name(klass, f)) {
            void* val = nullptr;
            g_api.field_static_get_value(fld, &val);
            if (SafeMem::IsPlausiblePtr(val)) return val;
        }
    }
    return nullptr;
}

inline void* GetTypeObject(const char* nsName, const char* clsName) {
    if (!Init() || !g_api.class_get_type || !g_api.type_get_object) return nullptr;
    void* k = FindClass(nsName, clsName);
    if (!k) return nullptr;
    void* t = g_api.class_get_type(k);
    return t ? g_api.type_get_object(t) : nullptr;
}

struct UnitySlots {
    bool resolved = false;
    void* clsTransform = nullptr;
    void* clsRectTransform = nullptr;
    void* clsCamera = nullptr;
    void* clsRigidbody = nullptr;
    void* clsAnimator = nullptr;
    void* clsGameObject = nullptr;
    void* Camera_get_main = nullptr;
    void* Camera_get_current = nullptr;
    void* Camera_get_fov = nullptr;
    void* Camera_set_fov = nullptr;
    void* Camera_get_aspect = nullptr;
    void* Camera_set_aspect = nullptr;
    void* Camera_get_worldToCameraMatrix = nullptr;
    void* Camera_get_projectionMatrix = nullptr;
    void* Camera_WorldToScreenPoint = nullptr;
    void* Camera_WorldToViewportPoint = nullptr;
    void* Comp_get_transform = nullptr;
    void* Comp_get_gameObject = nullptr;
    void* GameObject_SetActive = nullptr;
    void* GameObject_CreatePrimitive = nullptr;
    void* GameObject_GetComponentByName = nullptr;
    void* GameObject_set_layer = nullptr;
    void* Renderer_set_enabled = nullptr;
    void* Transform_get_position = nullptr;
    void* Transform_set_position = nullptr;
    void* Transform_get_forward = nullptr;
    void* Transform_set_forward = nullptr;
    void* Transform_get_right = nullptr;
    void* Transform_set_rotation = nullptr;
    void* Transform_get_localScale = nullptr;
    void* Transform_set_localScale = nullptr;
    void* Rigidbody_set_velocity = nullptr;
    void* Rigidbody_get_velocity = nullptr;
    void* Rigidbody_set_isKinematic = nullptr;
    void* Rigidbody_set_useGravity = nullptr;
    void* Rigidbody_set_position = nullptr;
    void* Rigidbody_set_rotation = nullptr;
    void* Animator_set_speed = nullptr;
    void* Object_FindObjectsOfType = nullptr;
    void* Resources_FindObjectsOfTypeAll = nullptr;
    void* Physics_SyncTransforms = nullptr;
    void* Physics_Linecast = nullptr;
    void* Time_set_timeScale = nullptr;
    void* App_set_targetFrameRate = nullptr;

    void* mWorld_getDefault = nullptr;
    void* mWorld_CreateEntity = nullptr;
    void* mWorld_GetEntityAtIndex = nullptr;
    void* mWorldStash_GetReflectionStash = nullptr;
    void* mSimpleDImple_Get = nullptr;
    void* mMainCam_GetCamera = nullptr;
    void* mMainCam_SetLookDir = nullptr;
    void* mIntSlotMap_TryGetIndex = nullptr;
    void* mIntSlotMap_GetKeyBySlotIndex = nullptr;
    void* mPedSP_Construct = nullptr;
    void* mPedSP_Deconstruct = nullptr;
    void* mVehSP_Construct = nullptr;
    void* mVehSP_Deconstruct = nullptr;
    void* mEnviro_SetTime = nullptr;
    void* mPedFSM_ForceEnter = nullptr;
    void* mPlayerStorage_TryGetLocal = nullptr;
    void* mCtrlElem_TryGetControlledPed = nullptr;
    void* mCharActor_SweepAndTeleport = nullptr;
    void* mCharActor_ForceNotGrounded = nullptr;
    void* mPhysActor_Teleport2 = nullptr;   // PhysicsActor::Teleport(Vector3, Quaternion) (L1341860)
    void* mPhysActor_SetPos = nullptr;      // PhysicsActor::set_Position(Vector3) (L1341812)
    void* mPhysActor_SetVel = nullptr;      // PhysicsActor::set_Velocity(Vector3) (L1341770)
    void* mPhysActor_ResetInterp = nullptr; // PhysicsActor::ResetInterpolationPosition() (L1341970)
    void* mPhysActor_GetVertVel = nullptr;
    void* mPhysActor_SetVertVel = nullptr;
    void* mTempWeapon_SetLocal = nullptr;
    void* mItemSvc_TryUseLocal = nullptr;
    void* mItemSvc_TryUseOnEntity = nullptr;
    void* mElementStorage_getInstance = nullptr;
    void* mPlayerDist_ByDigitalId = nullptr;
    void* mPlayerDist_ByEntity = nullptr;
    void* mVoiceSvc_SetAllMuted = nullptr;
    void* mVoiceSvc_SetAllVolume = nullptr;
} g_unity;

inline void ResolveUnityAndGameMethods() {
    if (g_unity.resolved || !Init()) return;
    const char* u = Offsets::g_cfg.nsUnity.c_str();
    const char* mp = Offsets::g_cfg.nsMorpeh.c_str();

    g_clsString              = FindClass("System", "String");
    g_unity.clsTransform     = FindClass(u, "Transform");
    g_unity.clsRectTransform = FindClass(u, "RectTransform");
    g_unity.clsCamera        = FindClass(u, "Camera");
    g_unity.clsRigidbody     = FindClass(u, "Rigidbody");
    g_unity.clsAnimator      = FindClass(u, "Animator");
    g_unity.clsGameObject    = FindClass(u, "GameObject");

    g_unity.Camera_get_main                = ResolveMethod(u, "Camera", "get_main", 0);
    g_unity.Camera_get_current             = ResolveMethod(u, "Camera", "get_current", 0);
    g_unity.Camera_get_fov                 = ResolveMethod(u, "Camera", "get_fieldOfView", 0);
    g_unity.Camera_set_fov                 = ResolveMethod(u, "Camera", "set_fieldOfView", 1);
    g_unity.Camera_get_aspect              = ResolveMethod(u, "Camera", "get_aspect", 0);
    g_unity.Camera_set_aspect              = ResolveMethod(u, "Camera", "set_aspect", 1);
    g_unity.Camera_get_worldToCameraMatrix = ResolveMethod(u, "Camera", "get_worldToCameraMatrix", 0);
    g_unity.Camera_get_projectionMatrix    = ResolveMethod(u, "Camera", "get_projectionMatrix", 0);
    g_unity.Camera_WorldToScreenPoint      = ResolveMethod(u, "Camera", "WorldToScreenPoint", 1);
    g_unity.Camera_WorldToViewportPoint    = ResolveMethod(u, "Camera", "WorldToViewportPoint", 1);
    g_unity.Comp_get_transform             = ResolveMethod(u, "Component", "get_transform", 0);
    g_unity.Comp_get_gameObject            = ResolveMethod(u, "Component", "get_gameObject", 0);
    g_unity.GameObject_SetActive           = ResolveMethod(u, "GameObject", "SetActive", 1);
    g_unity.GameObject_CreatePrimitive     = ResolveMethod(u, "GameObject", "CreatePrimitive", 1);
    g_unity.GameObject_GetComponentByName  = ResolveMethod(u, "GameObject", "GetComponentByName", 1);
    g_unity.GameObject_set_layer           = ResolveMethod(u, "GameObject", "set_layer", 1);
    g_unity.Renderer_set_enabled           = ResolveMethod(u, "Renderer", "set_enabled", 1);
    g_unity.Transform_get_position         = ResolveMethod(u, "Transform", "get_position", 0);
    g_unity.Transform_set_position         = ResolveMethod(u, "Transform", "set_position", 1);
    g_unity.Transform_get_forward          = ResolveMethod(u, "Transform", "get_forward", 0);
    g_unity.Transform_set_forward          = ResolveMethod(u, "Transform", "set_forward", 1);
    g_unity.Transform_get_right            = ResolveMethod(u, "Transform", "get_right", 0);
    g_unity.Transform_set_rotation         = ResolveMethod(u, "Transform", "set_rotation", 1);
    g_unity.Transform_get_localScale       = ResolveMethod(u, "Transform", "get_localScale", 0);
    g_unity.Transform_set_localScale       = ResolveMethod(u, "Transform", "set_localScale", 1);
    g_unity.Rigidbody_set_velocity         = ResolveMethod(u, "Rigidbody", "set_velocity", 1);
    g_unity.Rigidbody_get_velocity         = ResolveMethod(u, "Rigidbody", "get_velocity", 0);
    g_unity.Rigidbody_set_isKinematic    = ResolveMethod(u, "Rigidbody", "set_isKinematic", 1);
    g_unity.Rigidbody_set_useGravity     = ResolveMethod(u, "Rigidbody", "set_useGravity", 1);
    g_unity.Rigidbody_set_position         = ResolveMethod(u, "Rigidbody", "set_position", 1);
    g_unity.Rigidbody_set_rotation         = ResolveMethod(u, "Rigidbody", "set_rotation", 1);
    g_unity.Animator_set_speed             = ResolveMethod(u, "Animator", "set_speed", 1);
    g_unity.Object_FindObjectsOfType       = ResolveMethod(u, "Object", "FindObjectsOfType", 1);
    g_unity.Resources_FindObjectsOfTypeAll = ResolveMethod(u, "Resources", "FindObjectsOfTypeAll", 1);
    g_unity.Physics_SyncTransforms         = ResolveMethod(u, "Physics", "SyncTransforms", 0);
    g_unity.Physics_Linecast               = ResolveMethod(u, "Physics", "Linecast", 2);
    g_unity.Time_set_timeScale             = ResolveMethod(u, "Time", "set_timeScale", 1);
    g_unity.App_set_targetFrameRate        = ResolveMethod(u, "Application", "set_targetFrameRate", 1);

    g_unity.mWorld_getDefault              = ResolveMethod(mp, "World", "get_Default", 0);
    g_unity.mWorld_CreateEntity            = ResolveMethod(mp, "WorldEntityExtensions", "CreateEntity", 1);
    g_unity.mWorld_GetEntityAtIndex        = ResolveMethod(mp, "WorldEntityExtensions", "GetEntityAtIndex", 2);
    g_unity.mWorldStash_GetReflectionStash = ResolveMethod(mp, "WorldStashExtensions", "GetReflectionStash", 2);
    g_unity.mSimpleDImple_Get              = ResolveMethod("", "SimpleDImple", "Get", 1);
    g_unity.mMainCam_GetCamera             = ResolveMethod(Offsets::g_cfg.nsRealMainCamStorage.c_str(), Offsets::g_cfg.clsRealMainCamStorage.c_str(), "GetCamera", 0);
    g_unity.mMainCam_SetLookDir            = ResolveMethod(Offsets::g_cfg.nsRealMainCamStorage.c_str(), Offsets::g_cfg.clsRealMainCamStorage.c_str(), "SetCameraLookDirection", 1);
    g_unity.mIntSlotMap_TryGetIndex        = ResolveMethod(mp, "IntSlotMap", "TryGetIndex", 2);
    g_unity.mIntSlotMap_GetKeyBySlotIndex  = ResolveMethod(mp, "IntSlotMap", "GetKeyBySlotIndex", 1);

    const char* ss = Offsets::g_cfg.nsStreaming.c_str();
    if (!Offsets::g_cfg.clsPedSp.empty()) {
        g_unity.mPedSP_Construct   = ResolveMethod(ss, Offsets::g_cfg.clsPedSp.c_str(), "Construct", 1);
        g_unity.mPedSP_Deconstruct = ResolveMethod(ss, Offsets::g_cfg.clsPedSp.c_str(), "Deconstruct", 1);
    }
    if (!Offsets::g_cfg.clsVehSp.empty()) {
        g_unity.mVehSP_Construct   = ResolveMethod(ss, Offsets::g_cfg.clsVehSp.c_str(), "Construct", 1);
        g_unity.mVehSP_Deconstruct = ResolveMethod(ss, Offsets::g_cfg.clsVehSp.c_str(), "Deconstruct", 1);
    }
    if (!Offsets::g_cfg.clsEnviroTimeModule.empty()) {
        g_unity.mEnviro_SetTime = ResolveMethod(Offsets::g_cfg.nsEnviro.c_str(), Offsets::g_cfg.clsEnviroTimeModule.c_str(), "SetTimeOfDay", 1);
    }
    if (!g_unity.mEnviro_SetTime && !Offsets::g_cfg.clsEnviroMgr.empty()) {
        g_unity.mEnviro_SetTime = ResolveMethod(Offsets::g_cfg.nsEnviro.c_str(), Offsets::g_cfg.clsEnviroMgr.c_str(), "SetTimeOfDay", 1);
    }
    g_unity.mPedFSM_ForceEnter          = ResolveMethod(Offsets::g_cfg.nsPedStateMach.c_str(), Offsets::g_cfg.clsPedStateMach.c_str(), "ForceEnter", 2);
    g_unity.mPlayerStorage_TryGetLocal  = ResolveMethod(Offsets::g_cfg.nsPlayerStorages.c_str(), Offsets::g_cfg.clsRealPlayerStorage.c_str(), "TryGetLocal", 1);
    g_unity.mCtrlElem_TryGetControlledPed = ResolveMethod(Offsets::g_cfg.nsCtrlElemStorage.c_str(), Offsets::g_cfg.clsCtrlElemStorage.c_str(), "TryGetControlledPed", 2);
    g_unity.mCharActor_SweepAndTeleport = ResolveMethod(Offsets::g_cfg.nsCharacterController.c_str(), "CharacterActor", "SweepAndTeleport", 1);
    g_unity.mCharActor_ForceNotGrounded = ResolveMethod(Offsets::g_cfg.nsCharacterController.c_str(), "CharacterActor", "ForceNotGrounded", 1);
    g_unity.mPhysActor_Teleport2        = ResolveMethod(Offsets::g_cfg.nsCharacterController.c_str(), "PhysicsActor", "Teleport", 2);
    g_unity.mPhysActor_SetPos           = ResolveMethod(Offsets::g_cfg.nsCharacterController.c_str(), "PhysicsActor", "set_Position", 1);
    g_unity.mPhysActor_SetVel           = ResolveMethod(Offsets::g_cfg.nsCharacterController.c_str(), "PhysicsActor", "set_Velocity", 1);
    g_unity.mPhysActor_ResetInterp      = ResolveMethod(Offsets::g_cfg.nsCharacterController.c_str(), "PhysicsActor", "ResetInterpolationPosition", 0);
    g_unity.mPhysActor_GetVertVel       = ResolveMethod(Offsets::g_cfg.nsCharacterController.c_str(), "PhysicsActor", "get_VerticalVelocity", 0);
    g_unity.mPhysActor_SetVertVel       = ResolveMethod(Offsets::g_cfg.nsCharacterController.c_str(), "PhysicsActor", "set_VerticalVelocity", 1);
    g_unity.mTempWeapon_SetLocal        = ResolveMethod(Offsets::g_cfg.nsWeaponSys.c_str(), Offsets::g_cfg.clsTempWeapSvc.c_str(), "SetTempWeaponToLocalPlayer", 3);
    g_unity.mItemSvc_TryUseLocal        = ResolveMethod(Offsets::g_cfg.nsItemsSvc.c_str(), Offsets::g_cfg.clsItemSvc.c_str(), "TryUseItemOnLocalPlayer", 2);
    g_unity.mItemSvc_TryUseOnEntity     = ResolveMethod(Offsets::g_cfg.nsItemsSvc.c_str(), Offsets::g_cfg.clsItemSvc.c_str(), "TryUseItemOnEntity", 3);
    g_unity.mElementStorage_getInstance = ResolveMethod(Offsets::g_cfg.nsElementsSys.c_str(), Offsets::g_cfg.clsElementStorage.c_str(), "get_Instance", 0);
    g_unity.mVoiceSvc_SetAllMuted       = ResolveMethod(Offsets::g_cfg.nsVoiceSystems.c_str(), Offsets::g_cfg.clsVoicePlaybackSvc.c_str(), "SetAllChatTypesMuted", 1);
    g_unity.mVoiceSvc_SetAllVolume      = ResolveMethod(Offsets::g_cfg.nsVoiceSystems.c_str(), Offsets::g_cfg.clsVoicePlaybackSvc.c_str(), "SetAllChatTypesVolume", 1);

    ResolveUnityIcalls();
    g_unity.resolved = ((g_unity.Camera_get_main != nullptr || g_icall.Camera_get_main != nullptr) && g_unity.mWorld_getDefault != nullptr);
}

// Lazily resolves PlayerDistanceUtils overloads ONLY when far-player Spectate actually queries them (never at startup, never calls libc std::free on IL2CPP memory)
inline void ResolvePlayerDistMethodsLazy() {
    static std::atomic<bool> s_tried{false};
    if (s_tried.load(std::memory_order_acquire)) return;
    if (!Init()) return;
    s_tried.store(true, std::memory_order_release);

    TCFlyGuard::TryRun([&]() {
        void* pdCls = FindClass(Offsets::g_cfg.nsPlayerUtils.c_str(), Offsets::g_cfg.clsPlayerDistUtils.c_str());
        if (!SafeMem::IsPlausiblePtr(pdCls)) return;
        if (g_api.class_get_method_from_name) {
            g_unity.mPlayerDist_ByDigitalId = g_api.class_get_method_from_name(pdCls, "TryGetDistanceFromLocalPlayerToOther", 2);
        }
        using IterFn          = void* (*)(void*, void**);
        using GetParamCountFn = uint32_t (*)(const void*);
        using GetParamTypeFn  = void* (*)(const void*, uint32_t);
        using TypeGetNameFn   = char* (*)(const void*);
        using GetNameFn       = const char* (*)(const void*);
        using Il2CppFreeFn    = void (*)(void*);
        static IterFn          s_iter = (IterFn)dlsym(RTLD_DEFAULT, "il2cpp_class_get_methods");
        static GetParamCountFn s_pcnt = (GetParamCountFn)dlsym(RTLD_DEFAULT, "il2cpp_method_get_param_count");
        static GetParamTypeFn  s_ptyp = (GetParamTypeFn)dlsym(RTLD_DEFAULT, "il2cpp_method_get_param");
        static TypeGetNameFn   s_tnam = (TypeGetNameFn)dlsym(RTLD_DEFAULT, "il2cpp_type_get_name");
        static GetNameFn       s_mnam = (GetNameFn)dlsym(RTLD_DEFAULT, "il2cpp_method_get_name");
        static Il2CppFreeFn    s_free = (Il2CppFreeFn)dlsym(RTLD_DEFAULT, "il2cpp_free");
        if (s_iter && s_pcnt && s_ptyp && s_tnam && s_mnam) {
            void* iter = nullptr;
            while (void* m = s_iter(pdCls, &iter)) {
                const char* mn = s_mnam(m);
                if (!mn || std::strcmp(mn, "TryGetDistanceFromLocalPlayerToOther") != 0) continue;
                if (s_pcnt(m) != 2) continue;
                void* pt0 = s_ptyp(m, 0);
                char* tn0 = pt0 ? s_tnam(pt0) : nullptr;
                if (tn0) {
                    if (std::strstr(tn0, "UInt32")) g_unity.mPlayerDist_ByDigitalId = m;
                    else if (std::strstr(tn0, "EntityIdModel") == nullptr && std::strstr(tn0, "Entity")) g_unity.mPlayerDist_ByEntity = m;
                    if (s_free) s_free(tn0);
                }
            }
        }
    });
}

inline bool IsTransformAlive(const void* tr)   { return IsExactUnityTypeAlive(tr, g_unity.clsTransform, g_unity.clsRectTransform); }
inline bool IsCameraAlive(const void* cam)     { return IsExactUnityTypeAlive(cam, g_unity.clsCamera); }
inline bool IsRigidbodyAlive(const void* rb)   { return IsExactUnityTypeAlive(rb, g_unity.clsRigidbody); }
inline bool IsAnimatorAlive(const void* anim)  { return IsExactUnityTypeAlive(anim, g_unity.clsAnimator); }
inline bool IsGameObjectAlive(const void* go)  { return IsExactUnityTypeAlive(go, g_unity.clsGameObject); }

// Guarded, zero-GC-allocation Unity wrappers (exact Il2CppClass* verified before every native icall)
inline void* SafeCameraGetMain() {
    if (g_icall.Camera_get_main) {
        void* cam = nullptr;
        if (TCFlyGuard::TryRun([&]() { cam = g_icall.Camera_get_main(); }) && IsCameraAlive(cam))
            return cam;
        return nullptr;
    }
    void* cam = g_unity.Camera_get_main ? SafeInvoke(g_unity.Camera_get_main, nullptr, nullptr) : nullptr;
    return IsCameraAlive(cam) ? cam : nullptr;
}

inline void* SafeGetTransform(void* comp) {
    if (!IsUnityNativeAlive(comp)) return nullptr;
    if (g_icall.Comp_get_transform) {
        void* tr = nullptr;
        if (TCFlyGuard::TryRun([&]() { tr = g_icall.Comp_get_transform(comp); }) && IsTransformAlive(tr))
            return tr;
        return nullptr;
    }
    void* tr = g_unity.Comp_get_transform ? SafeInvoke(g_unity.Comp_get_transform, comp, nullptr) : nullptr;
    return IsTransformAlive(tr) ? tr : nullptr;
}

inline void* SafeGetGameObject(void* comp) {
    if (!IsUnityNativeAlive(comp)) return nullptr;
    if (g_icall.Comp_get_gameObject) {
        void* go = nullptr;
        if (TCFlyGuard::TryRun([&]() { go = g_icall.Comp_get_gameObject(comp); }) && IsGameObjectAlive(go))
            return go;
        return nullptr;
    }
    void* go = g_unity.Comp_get_gameObject ? SafeInvoke(g_unity.Comp_get_gameObject, comp, nullptr) : nullptr;
    return IsGameObjectAlive(go) ? go : nullptr;
}

inline bool SafeGetTransformPos(void* tr, Vec3& outPos) {
    if (!IsTransformAlive(tr)) return false;
    Vec3 tmp{};
    if (g_icall.Transform_get_position_Injected) {
        if (!TCFlyGuard::TryRun([&]() { g_icall.Transform_get_position_Injected(tr, &tmp); }))
            return false;
        if (!IsFiniteVec3(tmp)) return false;
        outPos = tmp;
        return true;
    }
    if (!g_unity.Transform_get_position) return false;
    void* ret = SafeInvoke(g_unity.Transform_get_position, tr, nullptr);
    void* u = Unbox(ret);
    if (!u || !SafeMem::ReadVal(u, tmp) || !IsFiniteVec3(tmp)) return false;
    outPos = tmp;
    return true;
}

inline bool SafeSetTransformPos(void* tr, const Vec3& pos) {
    if (!IsTransformAlive(tr) || !IsFiniteVec3(pos)) return false;
    if (g_icall.Transform_set_position_Injected) {
        return TCFlyGuard::TryRun([&]() { g_icall.Transform_set_position_Injected(tr, &pos); });
    }
    if (!g_unity.Transform_set_position) return false;
    Vec3 p = pos;
    void* a[1] = { &p };
    SafeInvoke(g_unity.Transform_set_position, tr, a);
    return true;
}

inline bool SafeGetTransformRot(void* tr, Quat& outRot) {
    if (!IsTransformAlive(tr)) return false;
    Quat q{};
    if (g_icall.Transform_get_rotation_Injected) {
        if (!TCFlyGuard::TryRun([&]() { g_icall.Transform_get_rotation_Injected(tr, &q); }))
            return false;
        if (!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) || !std::isfinite(q.w)) return false;
        outRot = q;
        return true;
    }
    return false;
}

inline bool SafeSetTransformRot(void* tr, const Quat& rot) {
    if (!IsTransformAlive(tr)) return false;
    if (g_icall.Transform_set_rotation_Injected) {
        return TCFlyGuard::TryRun([&]() { g_icall.Transform_set_rotation_Injected(tr, &rot); });
    }
    if (!g_unity.Transform_set_rotation) return false;
    Quat q = rot;
    void* a[1] = { &q };
    SafeInvoke(g_unity.Transform_set_rotation, tr, a);
    return true;
}

inline bool SafeGetTransformForward(void* tr, Vec3& outFwd) {
    Quat q{};
    if (SafeGetTransformRot(tr, q)) {
        outFwd = QuatForward(q);
        return IsFiniteVec3(outFwd);
    }
    if (!IsTransformAlive(tr) || !g_unity.Transform_get_forward) return false;
    void* u = Unbox(SafeInvoke(g_unity.Transform_get_forward, tr, nullptr));
    Vec3 v{};
    if (!u || !SafeMem::ReadVal(u, v) || !IsFiniteVec3(v)) return false;
    outFwd = v;
    return true;
}

inline bool SafeGetTransformRight(void* tr, Vec3& outRight) {
    Quat q{};
    if (SafeGetTransformRot(tr, q)) {
        outRight = QuatRight(q);
        return IsFiniteVec3(outRight);
    }
    if (!IsTransformAlive(tr) || !g_unity.Transform_get_right) return false;
    void* u = Unbox(SafeInvoke(g_unity.Transform_get_right, tr, nullptr));
    Vec3 v{};
    if (!u || !SafeMem::ReadVal(u, v) || !IsFiniteVec3(v)) return false;
    outRight = v;
    return true;
}

inline bool SafeSetTransformScale(void* tr, const Vec3& scale) {
    if (!IsTransformAlive(tr) || !IsFiniteVec3(scale)) return false;
    if (g_icall.Transform_set_localScale_Injected) {
        return TCFlyGuard::TryRun([&]() { g_icall.Transform_set_localScale_Injected(tr, &scale); });
    }
    if (!g_unity.Transform_set_localScale) return false;
    Vec3 s = scale;
    void* a[1] = { &s };
    SafeInvoke(g_unity.Transform_set_localScale, tr, a);
    return true;
}

inline bool SafeWorldToViewport(void* cam, const Vec3& world, float screenW, float screenH, Vec2& out) {
    if (!IsCameraAlive(cam) || !IsFiniteVec3(world)) return false;
    Vec3 vp{};
    if (g_icall.Camera_WorldToViewportPoint_Injected) {
        // MonoOrStereoscopicEye::Mono = 2
        if (!TCFlyGuard::TryRun([&]() { g_icall.Camera_WorldToViewportPoint_Injected(cam, &world, 2, &vp); }))
            return false;
    } else if (g_unity.Camera_WorldToViewportPoint) {
        Vec3 p = world;
        void* args[1] = { &p };
        void* u = Unbox(SafeInvoke(g_unity.Camera_WorldToViewportPoint, cam, args));
        if (!u || !SafeMem::ReadVal(u, vp)) return false;
    } else {
        return false;
    }
    if (vp.z <= 0.05f || !IsFiniteVec3(vp)) return false;
    out.x = vp.x * screenW;
    out.y = (1.0f - vp.y) * screenH;
    return std::isfinite(out.x) && std::isfinite(out.y);
}

struct FrameCameraVP {
    float vp[16]{};
    bool  valid = false;
};

// Reads Camera worldToCameraMatrix & projectionMatrix ONCE per frame (zero GC!) and computes VP = P * V
inline FrameCameraVP BuildFrameCameraVP(void* cam) {
    FrameCameraVP out{};
    if (!IsCameraAlive(cam)) return out;
    float v[16]{}, p[16]{};
    bool ok = false;
    if (g_icall.Camera_get_worldToCameraMatrix_Injected && g_icall.Camera_get_projectionMatrix_Injected) {
        ok = TCFlyGuard::TryRun([&]() {
            g_icall.Camera_get_worldToCameraMatrix_Injected(cam, v);
            g_icall.Camera_get_projectionMatrix_Injected(cam, p);
        });
    } else if (g_unity.Camera_get_worldToCameraMatrix && g_unity.Camera_get_projectionMatrix) {
        void* vUnb = Unbox(SafeInvoke(g_unity.Camera_get_worldToCameraMatrix, cam, nullptr));
        void* pUnb = Unbox(SafeInvoke(g_unity.Camera_get_projectionMatrix, cam, nullptr));
        if (vUnb && pUnb && SafeMem::Read(vUnb, v, sizeof(v)) && SafeMem::Read(pUnb, p, sizeof(p))) ok = true;
    }
    if (!ok) return out;
    // Unity Matrix4x4 is column-major: m[col*4 + row]
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = p[0 * 4 + row] * v[col * 4 + 0]
                      + p[1 * 4 + row] * v[col * 4 + 1]
                      + p[2 * 4 + row] * v[col * 4 + 2]
                      + p[3 * 4 + row] * v[col * 4 + 3];
            if (!std::isfinite(sum)) return out;
            out.vp[col * 4 + row] = sum;
        }
    }
    out.valid = true;
    return out;
}

// Pure C++ zero-icall projection using cached FrameCameraVP (falls back to SafeWorldToViewport if unavailable)
inline bool ProjectWorldToScreenFast(const FrameCameraVP& fvp, void* cam, const Vec3& world, float screenW, float screenH, Vec2& out) {
    if (!IsFiniteVec3(world)) return false;
    if (fvp.valid) {
        const float* m = fvp.vp;
        float cx = m[0] * world.x + m[4] * world.y + m[8]  * world.z + m[12];
        float cy = m[1] * world.x + m[5] * world.y + m[9]  * world.z + m[13];
        float cw = m[3] * world.x + m[7] * world.y + m[11] * world.z + m[15];
        if (cw <= 0.05f || !std::isfinite(cw)) return false;
        float invW = 1.0f / cw;
        float ndcX = cx * invW;
        float ndcY = cy * invW;
        out.x = (ndcX * 0.5f + 0.5f) * screenW;
        out.y = (0.5f - ndcY * 0.5f) * screenH;
        return std::isfinite(out.x) && std::isfinite(out.y);
    }
    return SafeWorldToViewport(cam, world, screenW, screenH, out);
}

inline void* SafeCreateInvisibleSkyFloor(int32_t groundLayer, void*& outTransform) {
    outTransform = nullptr;
    void* go = nullptr;
    int32_t cubeType = 3; // UnityEngine.PrimitiveType.Cube = 3
    if (g_icall.GameObject_CreatePrimitive) {
        TCFlyGuard::TryRun([&]() { go = g_icall.GameObject_CreatePrimitive(cubeType); });
    } else if (g_unity.GameObject_CreatePrimitive) {
        void* a[1] = { &cubeType };
        go = SafeInvoke(g_unity.GameObject_CreatePrimitive, nullptr, a);
    }
    if (!IsGameObjectAlive(go)) return nullptr;

    // Set layer to solid ground layer so CharacterActor walks on it naturally
    if (groundLayer >= 0 && groundLayer < 32) {
        if (g_icall.GameObject_set_layer) {
            TCFlyGuard::TryRun([&]() { g_icall.GameObject_set_layer(go, groundLayer); });
        } else if (g_unity.GameObject_set_layer) {
            void* a[1] = { &groundLayer };
            SafeInvoke(g_unity.GameObject_set_layer, go, a);
        }
    }

    // Hide MeshRenderer so the sky floor is 100% invisible while its BoxCollider remains solid!
    if (g_unity.GameObject_GetComponentByName && g_api.string_new) {
        void* nameStr = g_api.string_new("MeshRenderer");
        if (SafeMem::IsPlausiblePtr(nameStr)) {
            void* a[1] = { nameStr };
            void* rend = SafeInvoke(g_unity.GameObject_GetComponentByName, go, a);
            if (IsUnityNativeAlive(rend)) {
                bool off = false;
                if (g_icall.Renderer_set_enabled) {
                    TCFlyGuard::TryRun([&]() { g_icall.Renderer_set_enabled(rend, false); });
                } else if (g_unity.Renderer_set_enabled) {
                    void* ra[1] = { &off };
                    SafeInvoke(g_unity.Renderer_set_enabled, rend, ra);
                }
            }
        }
    }

    outTransform = SafeGetTransform(go);
    if (IsTransformAlive(outTransform)) {
        SafeSetTransformScale(outTransform, { 16.0f, 0.40f, 16.0f });
    }
    return go;
}

inline bool SafeGetRbVelocity(void* rb, Vec3& outVel) {
    if (!IsRigidbodyAlive(rb)) return false;
    Vec3 v{};
    if (g_icall.Rigidbody_get_velocity_Injected) {
        if (!TCFlyGuard::TryRun([&]() { g_icall.Rigidbody_get_velocity_Injected(rb, &v); })) return false;
        if (!IsFiniteVec3(v)) return false;
        outVel = v;
        return true;
    }
    if (!g_unity.Rigidbody_get_velocity) return false;
    void* u = Unbox(SafeInvoke(g_unity.Rigidbody_get_velocity, rb, nullptr));
    if (!u || !SafeMem::ReadVal(u, v) || !IsFiniteVec3(v)) return false;
    outVel = v;
    return true;
}

inline bool SafeSetRbVelocity(void* rb, const Vec3& vel) {
    if (!IsRigidbodyAlive(rb) || !IsFiniteVec3(vel)) return false;
    if (g_icall.Rigidbody_set_velocity_Injected) {
        return TCFlyGuard::TryRun([&]() { g_icall.Rigidbody_set_velocity_Injected(rb, &vel); });
    }
    if (!g_unity.Rigidbody_set_velocity) return false;
    Vec3 v = vel;
    void* a[1] = { &v };
    SafeInvoke(g_unity.Rigidbody_set_velocity, rb, a);
    return true;
}

inline bool SafeSetRbPosition(void* rb, const Vec3& pos) {
    if (!IsRigidbodyAlive(rb) || !IsFiniteVec3(pos)) return false;
    if (g_icall.Rigidbody_set_position_Injected) {
        return TCFlyGuard::TryRun([&]() { g_icall.Rigidbody_set_position_Injected(rb, &pos); });
    }
    if (!g_unity.Rigidbody_set_position) return false;
    Vec3 p = pos;
    void* a[1] = { &p };
    SafeInvoke(g_unity.Rigidbody_set_position, rb, a);
    return true;
}

inline bool SafeSetRbRotation(void* rb, const Quat& rot) {
    if (!IsRigidbodyAlive(rb)) return false;
    if (g_icall.Rigidbody_set_rotation_Injected) {
        return TCFlyGuard::TryRun([&]() { g_icall.Rigidbody_set_rotation_Injected(rb, &rot); });
    }
    if (!g_unity.Rigidbody_set_rotation) return false;
    Quat q = rot;
    void* a[1] = { &q };
    SafeInvoke(g_unity.Rigidbody_set_rotation, rb, a);
    return true;
}

inline void SafeSetRbKinematic(void* rb, bool val) {
    if (!IsRigidbodyAlive(rb)) return;
    if (g_icall.Rigidbody_set_isKinematic) {
        TCFlyGuard::TryRun([&]() { g_icall.Rigidbody_set_isKinematic(rb, val); });
        return;
    }
    if (g_unity.Rigidbody_set_isKinematic) {
        void* a[1] = { &val };
        SafeInvoke(g_unity.Rigidbody_set_isKinematic, rb, a);
    }
}

inline void SafeSetRbGravity(void* rb, bool val) {
    if (!IsRigidbodyAlive(rb)) return;
    if (g_icall.Rigidbody_set_useGravity) {
        TCFlyGuard::TryRun([&]() { g_icall.Rigidbody_set_useGravity(rb, val); });
        return;
    }
    if (g_unity.Rigidbody_set_useGravity) {
        void* a[1] = { &val };
        SafeInvoke(g_unity.Rigidbody_set_useGravity, rb, a);
    }
}

inline void SafeSetRbDetectCollisions(void* rb, bool val) {
    if (!IsRigidbodyAlive(rb)) return;
    if (g_icall.Rigidbody_set_detectCollisions) {
        TCFlyGuard::TryRun([&]() { g_icall.Rigidbody_set_detectCollisions(rb, val); });
    }
}

inline void SafeSetBehaviourEnabled(void* beh, bool enabled) {
    if (!IsUnityNativeAlive(beh)) return;
    if (g_icall.Behaviour_set_enabled) {
        TCFlyGuard::TryRun([&]() { g_icall.Behaviour_set_enabled(beh, enabled); });
    }
}

inline bool ReadStaticIntField(const char* nsName, const char* clsName, const char* fieldName, int32_t& outVal) {
    if (!Init() || !g_api.class_get_field_from_name || !g_api.field_static_get_value) return false;
    void* klass = FindClass(nsName, clsName);
    if (!SafeMem::IsPlausiblePtr(klass)) return false;
    bool ok = false;
    int32_t tmp = 0;
    TCFlyGuard::TryRun([&]() {
        if (void* fld = g_api.class_get_field_from_name(klass, fieldName)) {
            g_api.field_static_get_value(fld, &tmp);
            ok = true;
        }
    });
    if (ok) outVal = tmp;
    return ok;
}

inline bool SafePhysicsRaycast(const Vec3& origin, const Vec3& dir, float maxDist, int32_t layerMask, UnityRaycastHit& outHit) {
    if (!g_icall.Physics_get_defaultPhysicsScene_Injected || !g_icall.PhysicsScene_Internal_Raycast_Injected) return false;
    if (!IsFiniteVec3(origin) || !IsFiniteVec3(dir) || maxDist <= 0.0f) return false;
    int32_t scene = 0;
    bool hit = false;
    UnityRay ray{ origin, Vec3Norm(dir) };
    UnityRaycastHit tmp{};
    bool ok = TCFlyGuard::TryRun([&]() {
        g_icall.Physics_get_defaultPhysicsScene_Injected(&scene);
        // QueryTriggerInteraction::Ignore = 1
        hit = g_icall.PhysicsScene_Internal_Raycast_Injected(&scene, &ray, maxDist, &tmp, layerMask, 1);
    });
    if (ok && hit && IsFiniteVec3(tmp.point)) {
        outHit = tmp;
        return true;
    }
    return false;
}

inline void SafePhysicsSyncTransforms() {
    if (g_icall.Physics_SyncTransforms) {
        TCFlyGuard::TryRun([&]() { g_icall.Physics_SyncTransforms(); });
        return;
    }
    if (g_unity.Physics_SyncTransforms) SafeInvoke(g_unity.Physics_SyncTransforms, nullptr, nullptr);
}

inline void* SafeGetAnimatorBone(void* anim, int32_t humanBoneId) {
    if (!IsAnimatorAlive(anim) || !g_icall.Animator_GetBoneTransformInternal) return nullptr;
    void* tr = nullptr;
    if (!TCFlyGuard::TryRun([&]() { tr = g_icall.Animator_GetBoneTransformInternal(anim, humanBoneId); }))
        return nullptr;
    return IsTransformAlive(tr) ? tr : nullptr;
}

inline void SafeSetAnimatorSpeed(void* anim, float spd) {
    if (!IsAnimatorAlive(anim) || !std::isfinite(spd)) return;
    if (g_icall.Animator_set_speed) {
        TCFlyGuard::TryRun([&]() { g_icall.Animator_set_speed(anim, spd); });
    } else if (g_unity.Animator_set_speed) {
        void* a[1] = { &spd };
        SafeInvoke(g_unity.Animator_set_speed, anim, a);
    }
}

inline void SafeSetGameObjectActive(void* go, bool active) {
    if (!IsGameObjectAlive(go)) return;
    if (g_icall.GameObject_SetActive) {
        TCFlyGuard::TryRun([&]() { g_icall.GameObject_SetActive(go, active); });
    } else if (g_unity.GameObject_SetActive) {
        void* a[1] = { &active };
        SafeInvoke(g_unity.GameObject_SetActive, go, a);
    }
}

} // namespace Il2Cpp

// ============================================================================
// 4. Native Morpeh ECS Stash Accessor & Local Ped Lookup
// ============================================================================
namespace Morpeh {

struct IntHashMapSlot {
    int32_t key;
    int32_t nextIdx;
};

struct Stashes {
    void* cachedWorld          = nullptr;
    void* worldClass           = nullptr;
    void* entityClass          = nullptr;
    void* mEntity_get_Id       = nullptr;
    bool  idSelfTestDone       = false;
    bool  idUnboxedInvokeWorks = false;

    // Live system instances resolved directly from dev.Scripts.Initializer._container (SimpleDImple)
    void* realInitializer      = nullptr;
    void* realContainer        = nullptr; // SimpleDImple (InjectionAsmdef.dll)
    void* realWorldFromInit    = nullptr; // Initializer._world (0x48)
    void* realLocalInfoStorage = nullptr;
    void* realPlayerStorage    = nullptr;
    void* realCtrlElemStorage  = nullptr;
    void* realPedViewStorage   = nullptr;
    void* realMainCamStorage   = nullptr;
    void* realVehFactorySys    = nullptr;
    void* realPedSP            = nullptr;
    void* realVehSP            = nullptr;
    void* realTempWeaponSvc    = nullptr;
    void* realItemSvc          = nullptr;
    void* realWeaponStorage    = nullptr;
    void* realGestureSvc       = nullptr;
    void* realFactionUtils     = nullptr;
    void* realClanStorage      = nullptr;
    void* realJobSearchSys     = nullptr;
    void* realMetaClient       = nullptr;
    void* realPlayerDistUtils  = nullptr;
    void* realVoicePlayback    = nullptr;
    void* realElementStorage   = nullptr;
    void* realHudMapNavService = nullptr; // HudMapNavigationService (IInjectable, owns NavigationGraph @ 0x50)
    void* realHudMapNavSystem  = nullptr; // HudMapNavigationSystem (IInjectable, owns _points @ 0x38)
    bool  worldReady           = false;

    // Dynamic Morpeh Stashes (<Component>::GetStash(World))
    void* elementIdStash          = nullptr; // ElementId (ElementIdModel)
    void* localPlayerStash        = nullptr;
    void* playerStash             = nullptr;
    void* playerIdStash           = nullptr;
    void* digitalIdStash          = nullptr;
    void* factionResStash         = nullptr; // FactionResource
    void* playerClanStash         = nullptr; // PlayerClan
    void* clanDiplomacyStash      = nullptr; // ClanDiplomacyRelations
    void* nickPrefixStash         = nullptr; // NicknamePrefix
    void* pedStash                = nullptr;
    void* vehicleStash            = nullptr;
    void* dimensionStash          = nullptr; // Dimension
    void* pedToLogicLinkStash     = nullptr; // LogicEntityLink
    void* viewLinkStash           = nullptr; // ViewEntityLink
    void* controlsEntityStash     = nullptr; // ControlsEntity
    void* controlledByStash       = nullptr; // ControlledBy
    void* pedSeatStash            = nullptr; // PedSeat
    void* positionStash           = nullptr; // Position
    void* velocityStash           = nullptr; // Velocity
    void* unityGoStash            = nullptr; // UnityGameObject
    void* unityTransformStash     = nullptr; // UnityTransform
    void* unityRigidbodyStash     = nullptr; // UnityRigidbody
    void* setPosRotStash          = nullptr; // SetPositionAndRotation
    void* setVelocityStash        = nullptr; // SetVelocity
    void* healthStash             = nullptr; // Health
    void* armorStash              = nullptr; // Armor
    void* nickTextStash           = nullptr; // NicknameText
    void* pedViewStash            = nullptr; // PedView
    void* pedViewLocalStash       = nullptr; // PedViewLocal
    void* pedViewDeadStash        = nullptr; // PedViewDead
    void* pedViewKoStash          = nullptr; // PedViewKnockout
    void* pedViewFsmStash         = nullptr; // PedViewFsm
    void* pedViewWeaponLinkStash  = nullptr; // PedViewWeaponLink
    void* pedViewLastVehStash     = nullptr; // PedViewLastVehicle
    void* pedSkinStash            = nullptr; // Model
    void* pedGenderStash          = nullptr; // PedView
    void* pedClothesStash         = nullptr; // PedClothesValue
    void* pedClothesChangedStash  = nullptr; // PedClothesChanged
    void* pedInZoneStash          = nullptr; // PedInZone
    void* pedBlockedInputStash    = nullptr; // PedBlockedInput
    void* elementFrozenStash      = nullptr; // ElementFrozen
    void* staminaStash            = nullptr; // Stamina
    void* staminaInfStash         = nullptr; // StaminaInfinite
    void* ignoreStaminaLossStash  = nullptr; // IgnoreStaminaConsume
    void* elementInvincibleStash  = nullptr; // ElementInvincible
    void* weaponNotAllowedStash   = nullptr; // WeaponAttackNotAllowed
    void* weaponAttackCdStash     = nullptr; // WeaponAttackCooldown
    void* weaponViewParamsStash   = nullptr; // WeaponViewParameters
    void* weaponRecoilStash       = nullptr; // WeaponViewRangeRecoil
    void* camWeaponRecoilStash    = nullptr; // CameraWeaponRecoil
    void* pedWeaponStash          = nullptr; // PedWeapon
    void* pedWeaponAmmoStash      = nullptr; // PedWeaponAmmo
    void* itemIdStash             = nullptr; // ItemId
    void* itemNameStash           = nullptr; // ItemName
    void* itemCountStash          = nullptr; // ItemCount
    void* localItemStash          = nullptr; // LocalItem
    void* timeSetReqStash         = nullptr; // TimeCycleSetWorldTimeRequest
    void* timeFrozenReqStash      = nullptr; // TimeCycleSetWorldFrozenRequest
    void* timeWorldStash          = nullptr; // TimeCycleWorld
    void* vehInternalsStash       = nullptr; // VehicleViewInternals
    void* vehAddTorqueStash       = nullptr; // VehicleViewAdditionalTorque
    void* vehSpeedRestrictStash   = nullptr; // VehicleViewSpeedRestriction
    void* vehViewInputsStash      = nullptr; // VehicleViewInputs
    void* inputsDataStash         = nullptr; // InputsData
    void* vehPaintStash           = nullptr; // VehicleBodyColor
    void* vehPaintDirtyStash      = nullptr; // VehicleBodyColorUpdate
    void* vehWinStash             = nullptr; // VehicleWindowsColor
    void* vehWinDirtyStash        = nullptr; // VehicleWindowsColorUpdate
    void* vehWheelsStash          = nullptr; // VehicleWheels
    void* vehRimIdStash           = nullptr; // WheelCustomId
    void* vehRimColStash          = nullptr; // VehicleWheelsColor
    void* vehRimColDirtyStash     = nullptr; // VehicleWheelsColorUpdate
    void* vehRimDirtyStash        = nullptr; // VehicleWheelsUpdate
    void* vehTuneExtStash         = nullptr; // VehicleTuningExternal
    void* vehTuneExtDirtyStash    = nullptr; // VehicleTuningExternalUpdate
    void* checkpointTagStash      = nullptr; // Checkpoint
    void* checkpointPosStash      = nullptr; // Position
    void* jobCpStash              = nullptr; // JobCheckpoint
    void* currentJobStash         = nullptr; // CurrentJob
    void* jobSearchCreateStash    = nullptr; // JobSearchAreaCreate
    void* tempMiniStash           = nullptr; // TemporaryMini
    void* acceptPressedStash      = nullptr; // AcceptPressed
    void* graffitiCpStash         = nullptr; // CheckpointClanGraffiti
    void* graffitiStateStash      = nullptr; // CheckpointGraffitiState
    void* voiceChanStash          = nullptr; // VoiceChannel
    void* voiceChanReqStash       = nullptr; // VoiceChannelChangeRequest
    void* voiceChatTypeStash      = nullptr; // VoiceChannelChatType
    void* camSettingsStash        = nullptr; // CameraSettings
    void* freeCamActiveStash      = nullptr; // FreeCamActive
    void* freeCamTargetStash      = nullptr; // FreeCamTarget
    void* freeCamActivateStash    = nullptr; // FreeCamTargetActivate
    void* freeCamDeactivateStash  = nullptr; // FreeCamDeactivate
    void* navMarkerBlipStash      = nullptr; // NavMarkerBlip
    void* navMarkerWithBlipStash  = nullptr; // NavMarkerWithBlip
    void* navMarkerAttachedStash  = nullptr; // NavMarkerAttachedToElement
    void* navMarkerVisibStash     = nullptr; // NavMarkerVisibility
    void* navMarkerTypeStash      = nullptr; // ElementNavMarkerType
    void* hudMapCachedMarkerStash = nullptr; // HudMapCachedNavMarker
    void* hudMapCreateCpStash     = nullptr; // HudMapCreateCheckpoint
    void* hudMapCheckpointStash   = nullptr; // HudMapCheckpoint (persistent user-placed map pin entity)
    // Anti-Taser & Anti-Faint (LoseConsciousness) Stashes (verified dump1.4.1.cpp)
    void* weaponTizerTimerStash   = nullptr; // WeaponTizerTimer
    void* pedViewTaserKoStash     = nullptr; // PedViewTaserKnockout
    void* pedViewTaserKoVehStash  = nullptr; // PedViewTaserKnockoutInVehicle
    void* pedViewTaserPreKoStash  = nullptr; // PedViewTaserPreKnockout
    void* pedViewKoTimerStash     = nullptr; // PedViewKnockoutTimer
    void* pedViewIgnoreKoStash    = nullptr; // PedViewIgnoreKnockout
    void* pedViewMeleeKoStash     = nullptr; // PedViewMeleeKnockout
    void* loseConsciousnessStash  = nullptr; // LoseConsciousness (TagStash)
} g_stashes;

inline bool IsWorldAlive(void* world) {
    if (!Il2Cpp::IsUnityObjectSafe(world)) return false;
    uint8_t disposed = 1;
    if (!SafeMem::ReadVal(reinterpret_cast<uint8_t*>(world) + 0x18, disposed)) return false;
    return (disposed == 0);
}

inline void* ResolveFromSimpleDImple(const char* ns, const char* cls) {
    if (!Il2Cpp::IsUnityObjectSafe(g_stashes.realContainer) || !Il2Cpp::g_unity.mSimpleDImple_Get)
        return nullptr;
    void* tObj = Il2Cpp::GetTypeObject(ns, cls);
    if (!SafeMem::IsPlausiblePtr(tObj)) return nullptr;
    void* args[1] = { tObj };
    void* inst = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mSimpleDImple_Get, g_stashes.realContainer, args);
    return Il2Cpp::IsUnityObjectSafe(inst) ? inst : nullptr;
}

inline void DiscoverInitializerAndServices() {
    Il2Cpp::ResolveUnityAndGameMethods();
    if (!Il2Cpp::g_unity.Object_FindObjectsOfType && !Il2Cpp::g_unity.Resources_FindObjectsOfTypeAll)
        return;

    // Validate existing Initializer first
    if (Il2Cpp::IsUnityNativeAlive(g_stashes.realInitializer)) {
        void* initCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(g_stashes.realInitializer) : nullptr;
        size_t offCont = Il2Cpp::FieldOffset(initCls, "_container", 0x28);
        size_t offWld  = Il2Cpp::FieldOffset(initCls, "_world", 0x48);
        void* w = SafeMem::PtrAt(g_stashes.realInitializer, offWld);
        void* c = SafeMem::PtrAt(g_stashes.realInitializer, offCont);
        if (IsWorldAlive(w) && Il2Cpp::IsUnityObjectSafe(c)) {
            g_stashes.realWorldFromInit = w;
            g_stashes.realContainer = c;
            if (g_stashes.realPedViewStorage && g_stashes.realPlayerStorage && g_stashes.realPedSP)
                return;
        } else {
            g_stashes.realInitializer = nullptr;
            g_stashes.realContainer = nullptr;
            g_stashes.realWorldFromInit = nullptr;
        }
    }

    // Find active dev.Scripts.Initializer MonoBehaviour (exact StateScript.dylib L20379 strategy)
    void* initType = Il2Cpp::GetTypeObject("dev.Scripts", "Initializer");
    if (!SafeMem::IsPlausiblePtr(initType)) return;
    void* args[1] = { initType };
    void* arr = nullptr;
    if (Il2Cpp::g_unity.Object_FindObjectsOfType)
        arr = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.Object_FindObjectsOfType, nullptr, args);
    if (!SafeMem::IsPlausiblePtr(arr) && Il2Cpp::g_unity.Resources_FindObjectsOfTypeAll)
        arr = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.Resources_FindObjectsOfTypeAll, nullptr, args);
    if (!SafeMem::IsPlausiblePtr(arr)) return;

    int32_t len = 0;
    if (!SafeMem::ReadVal(reinterpret_cast<uint8_t*>(arr) + Offsets::Morpeh::kIl2CppArrayLengthOff, len) || len <= 0)
        return;
    uint8_t* base = reinterpret_cast<uint8_t*>(arr) + Offsets::Morpeh::kIl2CppArrayDataOff;
    for (int i = 0; i < std::min(len, 16); ++i) {
        void* obj = SafeMem::PtrAt(base, static_cast<size_t>(i) * sizeof(void*));
        if (!Il2Cpp::IsUnityNativeAlive(obj)) continue;
        void* initCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(obj) : nullptr;
        size_t offCont = Il2Cpp::FieldOffset(initCls, "_container", 0x28);
        size_t offWld  = Il2Cpp::FieldOffset(initCls, "_world", 0x48);
        void* cont = SafeMem::PtrAt(obj, offCont);
        void* wld  = SafeMem::PtrAt(obj, offWld);
        if (Il2Cpp::IsUnityObjectSafe(cont) && IsWorldAlive(wld)) {
            g_stashes.realInitializer   = obj;
            g_stashes.realContainer     = cont;
            g_stashes.realWorldFromInit = wld;
            break;
        }
    }
    if (!Il2Cpp::IsUnityObjectSafe(g_stashes.realContainer)) return;

    // Resolve all 14 live game singletons directly from SimpleDImple (ZERO hooks required!)
    const auto& cfg = Offsets::g_cfg;
    if (!g_stashes.realMainCamStorage)   g_stashes.realMainCamStorage   = ResolveFromSimpleDImple(cfg.nsRealMainCamStorage.c_str(), cfg.clsRealMainCamStorage.c_str());
    if (!g_stashes.realPlayerStorage)    g_stashes.realPlayerStorage    = ResolveFromSimpleDImple(cfg.nsPlayerStorages.c_str(), cfg.clsRealPlayerStorage.c_str());
    if (!g_stashes.realPedViewStorage)   g_stashes.realPedViewStorage   = ResolveFromSimpleDImple(cfg.nsRealPedViewStorage.c_str(), cfg.clsRealPedViewStorage.c_str());
    if (!g_stashes.realPedSP)            g_stashes.realPedSP            = ResolveFromSimpleDImple(cfg.nsStreaming.c_str(), cfg.clsPedSp.c_str());
    if (!g_stashes.realVehSP)            g_stashes.realVehSP            = ResolveFromSimpleDImple(cfg.nsStreaming.c_str(), cfg.clsVehSp.c_str());
    if (!g_stashes.realTempWeaponSvc)    g_stashes.realTempWeaponSvc    = ResolveFromSimpleDImple(cfg.nsWeaponSys.c_str(), cfg.clsTempWeapSvc.c_str());
    if (!g_stashes.realItemSvc)          g_stashes.realItemSvc          = ResolveFromSimpleDImple(cfg.nsItemsSvc.c_str(), cfg.clsItemSvc.c_str());
    if (!g_stashes.realWeaponStorage)    g_stashes.realWeaponStorage    = ResolveFromSimpleDImple(cfg.nsWeaponSys.c_str(), cfg.clsWeaponStorage.c_str());
    if (!g_stashes.realCtrlElemStorage)  g_stashes.realCtrlElemStorage  = ResolveFromSimpleDImple(cfg.nsCtrlElemStorage.c_str(), cfg.clsCtrlElemStorage.c_str());
    if (!g_stashes.realFactionUtils)     g_stashes.realFactionUtils     = ResolveFromSimpleDImple(cfg.nsRealFactionUtils.c_str(), cfg.clsRealFactionUtils.c_str());
    if (!g_stashes.realClanStorage)      g_stashes.realClanStorage      = ResolveFromSimpleDImple(cfg.nsRealClanStorage.c_str(), cfg.clsRealClanStorage.c_str());
    if (!g_stashes.realGestureSvc)       g_stashes.realGestureSvc       = ResolveFromSimpleDImple(cfg.nsGestureSvc.c_str(), cfg.clsGestureSvc.c_str());
    if (!g_stashes.realLocalInfoStorage) g_stashes.realLocalInfoStorage = ResolveFromSimpleDImple(cfg.nsRealLocalInfo.c_str(), cfg.clsRealLocalInfo.c_str());
    if (!g_stashes.realJobSearchSys)     g_stashes.realJobSearchSys     = ResolveFromSimpleDImple(cfg.nsRealJobSearchSys.c_str(), cfg.clsRealJobSearchSys.c_str());
}

// On-demand lazy service resolvers (never invoked during startup / menu boot)
inline void* EnsureMetaClientResolved() {
    if (!Il2Cpp::IsUnityObjectSafe(g_stashes.realMetaClient)) {
        if (void* gmc = Il2Cpp::ResolveStaticClassFieldPtr("dev.Scripts.GlobalContexts", "GlobalContextCore", "MetaClientWrapper")) {
            g_stashes.realMetaClient = gmc;
        }
    }
    if (!Il2Cpp::IsUnityObjectSafe(g_stashes.realMetaClient) && Il2Cpp::IsUnityObjectSafe(g_stashes.realContainer)) {
        g_stashes.realMetaClient = ResolveFromSimpleDImple(Offsets::g_cfg.nsFlakeFlow.c_str(), Offsets::g_cfg.clsMetaClientWrapper.c_str());
    }
    return Il2Cpp::IsUnityObjectSafe(g_stashes.realMetaClient) ? g_stashes.realMetaClient : nullptr;
}

inline void* EnsurePlayerDistUtilsResolved() {
    if (!Il2Cpp::IsUnityObjectSafe(g_stashes.realPlayerDistUtils) && Il2Cpp::IsUnityObjectSafe(g_stashes.realContainer)) {
        g_stashes.realPlayerDistUtils = ResolveFromSimpleDImple(Offsets::g_cfg.nsPlayerUtils.c_str(), Offsets::g_cfg.clsPlayerDistUtils.c_str());
    }
    return Il2Cpp::IsUnityObjectSafe(g_stashes.realPlayerDistUtils) ? g_stashes.realPlayerDistUtils : nullptr;
}

inline void* EnsureVoicePlaybackResolved() {
    if (!Il2Cpp::IsUnityObjectSafe(g_stashes.realVoicePlayback) && Il2Cpp::IsUnityObjectSafe(g_stashes.realContainer)) {
        g_stashes.realVoicePlayback = ResolveFromSimpleDImple(Offsets::g_cfg.nsVoiceSystems.c_str(), Offsets::g_cfg.clsVoicePlaybackSvc.c_str());
    }
    return Il2Cpp::IsUnityObjectSafe(g_stashes.realVoicePlayback) ? g_stashes.realVoicePlayback : nullptr;
}

inline void* EnsureElementStorageResolved() {
    if (!Il2Cpp::IsUnityObjectSafe(g_stashes.realElementStorage) && Il2Cpp::g_unity.mElementStorage_getInstance) {
        if (void* es = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mElementStorage_getInstance, nullptr, nullptr)) {
            if (Il2Cpp::IsUnityObjectSafe(es)) g_stashes.realElementStorage = es;
        }
    }
    return Il2Cpp::IsUnityObjectSafe(g_stashes.realElementStorage) ? g_stashes.realElementStorage : nullptr;
}

inline void* EnsureHudMapNavServiceResolved() {
    if (!Il2Cpp::IsUnityObjectSafe(g_stashes.realHudMapNavService) && Il2Cpp::IsUnityObjectSafe(g_stashes.realContainer)) {
        g_stashes.realHudMapNavService = ResolveFromSimpleDImple(
            Offsets::g_cfg.nsHudMapSystems.c_str(), Offsets::g_cfg.clsHudMapNavService.c_str());
    }
    if (Il2Cpp::IsUnityObjectSafe(g_stashes.realHudMapNavService) && !Il2Cpp::IsUnityObjectSafe(g_stashes.hudMapCheckpointStash)) {
        void* svcCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(g_stashes.realHudMapNavService) : nullptr;
        size_t offCp = Il2Cpp::FieldOffset(svcCls, "_hudMapCheckpoint", Offsets::g_cfg.offHudMapNavSvcCpStash);
        void* st = SafeMem::PtrAt(g_stashes.realHudMapNavService, offCp);
        if (Il2Cpp::IsUnityObjectSafe(st)) g_stashes.hudMapCheckpointStash = st;
    }
    return Il2Cpp::IsUnityObjectSafe(g_stashes.realHudMapNavService) ? g_stashes.realHudMapNavService : nullptr;
}

inline void* EnsureHudMapNavSystemResolved() {
    if (!Il2Cpp::IsUnityObjectSafe(g_stashes.realHudMapNavSystem) && Il2Cpp::IsUnityObjectSafe(g_stashes.realContainer)) {
        g_stashes.realHudMapNavSystem = ResolveFromSimpleDImple(
            Offsets::g_cfg.nsHudMapSystems.c_str(), Offsets::g_cfg.clsHudMapNavSystem.c_str());
    }
    return Il2Cpp::IsUnityObjectSafe(g_stashes.realHudMapNavSystem) ? g_stashes.realHudMapNavSystem : nullptr;
}

inline void* EnsureNavGraphResolved() {
    // Verified dump1.4.1.cpp L540186-540203: NavigationGraph lives on HudMapNavigationService.<NavigationGraph>k__BackingField (0x50)
    if (void* svc = EnsureHudMapNavServiceResolved()) {
        void* svcCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(svc) : nullptr;
        size_t offG = Il2Cpp::FieldOffset(svcCls, "<NavigationGraph>k__BackingField", Offsets::g_cfg.offHudMapNavSvcGraph);
        void* g = SafeMem::PtrAt(svc, offG);
        if (Il2Cpp::IsUnityObjectSafe(g)) return g;
    }
    if (Il2Cpp::IsUnityObjectSafe(g_stashes.realContainer)) {
        return ResolveFromSimpleDImple(Offsets::g_cfg.nsMap.c_str(), Offsets::g_cfg.clsNavigationGraph.c_str());
    }
    return nullptr;
}

inline bool TryReadHudMapRouteEnd(Vec3& outEnd) {
    void* sys = EnsureHudMapNavSystemResolved();
    if (!Il2Cpp::IsUnityObjectSafe(sys)) return false;
    void* sysCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(sys) : nullptr;
    size_t offPts = Il2Cpp::FieldOffset(sysCls, "_points", Offsets::g_cfg.offHudMapNavSysPoints);
    void* listObj = SafeMem::PtrAt(sys, offPts);
    if (!Il2Cpp::IsUnityObjectSafe(listObj)) return false;
    // System.Collections.Generic.List<Vector3>: _items @ +0x10 (Vector3[]), _size @ +0x18 (int32)
    void* itemsArr = SafeMem::PtrAt(listObj, 0x10);
    int32_t count = 0;
    if (!SafeMem::IsPlausiblePtr(itemsArr) || !SafeMem::ReadVal(reinterpret_cast<uint8_t*>(listObj) + 0x18, count)) return false;
    if (count <= 0 || count > 8192) return false;
    int32_t arrLen = 0;
    if (!SafeMem::ReadVal(reinterpret_cast<uint8_t*>(itemsArr) + Offsets::Morpeh::kIl2CppArrayLengthOff, arrLen) || arrLen < count)
        return false;
    Vec3 lastPt{};
    const uint8_t* elemAddr = reinterpret_cast<const uint8_t*>(itemsArr) + Offsets::Morpeh::kIl2CppArrayDataOff
                              + static_cast<size_t>(count - 1) * sizeof(Vec3);
    if (!SafeMem::ReadVal(elemAddr, lastPt) || !IsFiniteVec3(lastPt)) return false;
    if (std::fabs(lastPt.x) < 0.25f && std::fabs(lastPt.z) < 0.25f) return false;
    if (std::fabs(lastPt.x) > 25000.0f || std::fabs(lastPt.z) > 25000.0f) return false;
    outEnd = lastPt;
    return true;
}

inline void* GetWorld() {
    Il2Cpp::ResolveUnityAndGameMethods();
    if (IsWorldAlive(g_stashes.realWorldFromInit)) return g_stashes.realWorldFromInit;
    if (Il2Cpp::g_unity.mWorld_getDefault) {
        void* w = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mWorld_getDefault, nullptr, nullptr);
        if (IsWorldAlive(w)) return w;
    }
    return nullptr;
}

inline int64_t GetEntityById(int32_t entityId) {
    if (entityId < 0) return 0;
    void* world = GetWorld();
    if (!SafeMem::IsPlausiblePtr(world) || !Il2Cpp::g_unity.mWorld_GetEntityAtIndex) return 0;
    void* args[2] = { world, &entityId };
    void* ret = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mWorld_GetEntityAtIndex, nullptr, args);
    void* u = Il2Cpp::Unbox(ret);
    if (!u) return 0;
    int64_t packed = 0;
    SafeMem::ReadVal(u, packed);
    return packed;
}

inline int32_t UnpackEntityId(int64_t packedEntity) {
    if (packedEntity == 0) return -1;
    // Verified from StateScript.dylib (GetLocalPedEntity @ 0x1f2ba8 / 0x1f2cd8: asr x8, x8, #32)
    // and dump1.4.1.cpp Scellecs.Morpeh.Entity: upper 32 bits = Id, bits 16..31 = Generation, bits 0..15 = WorldId.
    return static_cast<int32_t>(packedEntity >> 32);
}

inline uint16_t UnpackEntityGen(int64_t packedEntity) {
    return static_cast<uint16_t>((static_cast<uint64_t>(packedEntity) >> 16) & 0xFFFFULL);
}

struct StashMeta {
    bool   isTagStash = false;
    size_t mapOff     = Offsets::Morpeh::kStashMapOffset;
    size_t dataOff    = Offsets::Morpeh::kStashDataArrayOffset;
    size_t elemStride = 0;
};

inline std::mutex g_stashMetaMutex;
inline std::unordered_map<void*, StashMeta> g_stashMetaCache;
inline std::atomic<uint32_t> g_worldGen{0};

inline void* FetchComponentStash(void* world, const char* ns, const char* cls) {
    if (!SafeMem::IsPlausiblePtr(world) || !ns || !cls || !cls[0]) return nullptr;
    // Primary (exact StateScript.dylib strategy): WorldStashExtensions::GetReflectionStash(World, System.Type)
    // Works for 100% of Morpeh components including PedView (which has NO static GetStash(World) method in 1.4.1!).
    if (Il2Cpp::g_unity.mWorldStash_GetReflectionStash) {
        if (void* tObj = Il2Cpp::GetTypeObject(ns, cls)) {
            void* args[2] = { world, tObj };
            void* st = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mWorldStash_GetReflectionStash, nullptr, args);
            if (Il2Cpp::IsUnityObjectSafe(st)) return st;
        }
    }
    // Secondary fallback: generated static <Component>::GetStash(World)
    if (void* m = Il2Cpp::ResolveMethod(ns, cls, "GetStash", 1)) {
        void* args[1] = { world };
        void* st = Il2Cpp::SafeInvoke(m, nullptr, args);
        if (Il2Cpp::IsUnityObjectSafe(st)) return st;
    }
    return nullptr;
}

inline void BindStorageDirectStashes() {
    // Exact StateScript.dylib direct field extraction from PedViewStorage, PedStreamingProvider, PlayerStorage
    if (Il2Cpp::IsUnityObjectSafe(g_stashes.realPedViewStorage)) {
        void* k = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(g_stashes.realPedViewStorage) : nullptr;
        if (!g_stashes.pedViewStash)     g_stashes.pedViewStash     = SafeMem::PtrAt(g_stashes.realPedViewStorage, Il2Cpp::FieldOffset(k, "_pedView", 0x68));
        if (!g_stashes.pedSeatStash)     g_stashes.pedSeatStash     = SafeMem::PtrAt(g_stashes.realPedViewStorage, Il2Cpp::FieldOffset(k, "_pedSeat", 0x70));
        if (!g_stashes.pedViewKoStash)   g_stashes.pedViewKoStash   = SafeMem::PtrAt(g_stashes.realPedViewStorage, Il2Cpp::FieldOffset(k, "_pedViewKnockout", 0x78));
        if (!g_stashes.pedViewDeadStash) g_stashes.pedViewDeadStash = SafeMem::PtrAt(g_stashes.realPedViewStorage, Il2Cpp::FieldOffset(k, "_pedViewDead", 0x80));
    }
    if (Il2Cpp::IsUnityObjectSafe(g_stashes.realPedSP)) {
        void* k = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(g_stashes.realPedSP) : nullptr;
        if (!g_stashes.pedSkinStash)        g_stashes.pedSkinStash        = SafeMem::PtrAt(g_stashes.realPedSP, Il2Cpp::FieldOffset(k, "_model", Offsets::PedProvFallback::_model));
        if (!g_stashes.pedViewStash)        g_stashes.pedViewStash        = SafeMem::PtrAt(g_stashes.realPedSP, Il2Cpp::FieldOffset(k, "_pedView", Offsets::PedProvFallback::_pedView));
        if (!g_stashes.unityGoStash)        g_stashes.unityGoStash        = SafeMem::PtrAt(g_stashes.realPedSP, Il2Cpp::FieldOffset(k, "_unityGameObject", Offsets::PedProvFallback::_unityGameObject));
        if (!g_stashes.unityTransformStash) g_stashes.unityTransformStash = SafeMem::PtrAt(g_stashes.realPedSP, Il2Cpp::FieldOffset(k, "_unityTransform", Offsets::PedProvFallback::_unityTransform));
        if (!g_stashes.unityRigidbodyStash) g_stashes.unityRigidbodyStash = SafeMem::PtrAt(g_stashes.realPedSP, Il2Cpp::FieldOffset(k, "_unityRigidbody", Offsets::PedProvFallback::_unityRigidbody));
        if (!g_stashes.pedSeatStash)        g_stashes.pedSeatStash        = SafeMem::PtrAt(g_stashes.realPedSP, Il2Cpp::FieldOffset(k, "_pedSeat", Offsets::PedProvFallback::_pedSeat));
        if (!g_stashes.pedViewFsmStash)     g_stashes.pedViewFsmStash     = SafeMem::PtrAt(g_stashes.realPedSP, Il2Cpp::FieldOffset(k, "_pedViewFsm", 0x210));
    }
    if (Il2Cpp::IsUnityObjectSafe(g_stashes.realPlayerStorage)) {
        void* k = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(g_stashes.realPlayerStorage) : nullptr;
        if (!g_stashes.playerIdStash)    g_stashes.playerIdStash    = SafeMem::PtrAt(g_stashes.realPlayerStorage, Il2Cpp::FieldOffset(k, "_playerId", 0x38));
        if (!g_stashes.digitalIdStash)   g_stashes.digitalIdStash   = SafeMem::PtrAt(g_stashes.realPlayerStorage, Il2Cpp::FieldOffset(k, "_digitalId", 0x40));
        if (!g_stashes.localPlayerStash) g_stashes.localPlayerStash = SafeMem::PtrAt(g_stashes.realPlayerStorage, Il2Cpp::FieldOffset(k, "_localPlayer", 0x48));
    }
}

inline void RefreshStoragesIfNeeded() {
    Il2Cpp::ResolveUnityAndGameMethods();

    static double s_lastDiscovery = 0.0;
    double now = CACurrentMediaTime();
    bool needRetry = (!g_stashes.pedViewStash || !g_stashes.pedToLogicLinkStash ||
                      !g_stashes.controlsEntityStash || !g_stashes.realContainer);
    if (needRetry && (now - s_lastDiscovery >= 1.2)) {
        s_lastDiscovery = now;
        DiscoverInitializerAndServices();
    }

    void* world = GetWorld();
    if (!SafeMem::IsPlausiblePtr(world)) {
        g_stashes.worldReady = false;
        return;
    }

    bool worldChanged = (world != g_stashes.cachedWorld);
    if (!worldChanged && !needRetry) return;
    if (!worldChanged && (now - s_lastDiscovery < 1.15) && g_stashes.pedViewStash != nullptr) return;

    if (worldChanged) {
        g_stashes.cachedWorld = world;
        g_stashes.worldClass  = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(world) : nullptr;
        g_worldGen.fetch_add(1, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(g_stashMetaMutex);
            g_stashMetaCache.clear();
        }
        {
            std::lock_guard<std::mutex> lk(g_espMutex);
            g_renderPlayers.clear();
        }
        // Reset stash pointers so they bind fresh to the new world
        g_stashes.elementIdStash = nullptr;
        g_stashes.realPlayerDistUtils = nullptr;
        g_stashes.realVoicePlayback = nullptr;
        g_stashes.realElementStorage = nullptr;
        g_stashes.realHudMapNavService = nullptr;
        g_stashes.realHudMapNavSystem = nullptr;
        g_stashes.hudMapCheckpointStash = nullptr;
        g_stashes.weaponTizerTimerStash = nullptr;
        g_stashes.pedViewTaserKoStash = nullptr;
        g_stashes.pedViewTaserKoVehStash = nullptr;
        g_stashes.pedViewTaserPreKoStash = nullptr;
        g_stashes.pedViewKoTimerStash = nullptr;
        g_stashes.pedViewIgnoreKoStash = nullptr;
        g_stashes.pedViewMeleeKoStash = nullptr;
        g_stashes.loseConsciousnessStash = nullptr;
        g_stashes.pedViewStash = nullptr;
        g_stashes.pedToLogicLinkStash = nullptr;
        g_stashes.controlsEntityStash = nullptr;
        g_stashes.controlledByStash = nullptr;
        g_stashes.pedViewLocalStash = nullptr;
        g_stashes.pedViewDeadStash = nullptr;
        g_stashes.pedViewKoStash = nullptr;
        g_stashes.positionStash = nullptr;
        g_stashes.healthStash = nullptr;
        g_stashes.armorStash = nullptr;
        g_stashes.nickTextStash = nullptr;
        DiscoverInitializerAndServices();
    }

    const auto& c = Offsets::g_cfg;
    auto bindStash = [&](void*& slot, const std::string& ns, const std::string& cls) {
        if (!Il2Cpp::IsUnityObjectSafe(slot)) {
            slot = FetchComponentStash(world, ns.c_str(), cls.c_str());
        }
    };

    bindStash(g_stashes.localPlayerStash,       c.nsPlayersComponents, c.compLocalPlayer);
    bindStash(g_stashes.elementIdStash,         c.nsElementsComponents, c.compElementId);
    bindStash(g_stashes.playerStash,            c.nsPlayersComponents, c.compPlayer);
    bindStash(g_stashes.playerIdStash,          c.nsPlayersComponents, c.compPlayerId);
    bindStash(g_stashes.digitalIdStash,         c.nsPlayersComponents, c.compDigitalId);
    bindStash(g_stashes.factionResStash,        c.nsPlayersResComponents, c.compFactionResource);
    bindStash(g_stashes.playerClanStash,        c.nsClanComponents, c.compPlayerClan);
    bindStash(g_stashes.clanDiplomacyStash,     c.nsClanDiplomacy, c.compClanDiplomacy);
    bindStash(g_stashes.nickPrefixStash,        c.nsNicknameComponents, c.compNicknamePrefix);
    bindStash(g_stashes.pedStash,               c.nsElementsComponents, c.compPed);
    bindStash(g_stashes.vehicleStash,           c.nsElementsComponents, c.compVehicle);
    bindStash(g_stashes.dimensionStash,         c.nsDimensionComponents, c.compDimension);
    bindStash(g_stashes.pedToLogicLinkStash,    c.nsUnityViewComponents, c.compLogicEntityLink);
    bindStash(g_stashes.viewLinkStash,          c.nsUnityViewComponents, c.compViewEntityLink);
    bindStash(g_stashes.controlsEntityStash,    c.nsControlsComponents, c.compControlsEntity);
    bindStash(g_stashes.controlledByStash,      c.nsControlsComponents, c.compControlledBy);
    bindStash(g_stashes.pedSeatStash,           c.nsPedsComponents, c.compPedSeat);
    bindStash(g_stashes.positionStash,          c.nsUnityViewComponents, c.compPosition);
    if (!Il2Cpp::IsUnityObjectSafe(g_stashes.velocityStash))
        g_stashes.velocityStash = FetchComponentStash(world, c.nsUnityViewComponents.c_str(), "Velocity");
    bindStash(g_stashes.unityGoStash,           c.nsUnityViewComponents, c.compUnityGameObject);
    bindStash(g_stashes.unityTransformStash,    c.nsUnityViewComponents, c.compUnityTransform);
    bindStash(g_stashes.unityRigidbodyStash,    c.nsUnityViewComponents, c.compUnityRigidbody);
    bindStash(g_stashes.setPosRotStash,         c.nsUnityViewComponents, c.compSetPosRot);
    bindStash(g_stashes.setVelocityStash,       c.nsUnityViewComponents, c.compSetVelocity);
    bindStash(g_stashes.healthStash,            c.nsElementsComponents, c.compHealth);
    bindStash(g_stashes.armorStash,             c.nsElementsComponents, c.compArmor);
    bindStash(g_stashes.nickTextStash,          c.nsNicknameComponents, c.compNicknameText);
    bindStash(g_stashes.pedViewStash,           c.nsPedViewComponents, c.compPedView);
    bindStash(g_stashes.pedViewLocalStash,      c.nsPedViewComponents, c.compPedViewLocal);
    bindStash(g_stashes.pedViewDeadStash,       c.nsPedViewComponents, c.compPedViewDead);
    bindStash(g_stashes.pedViewKoStash,         c.nsPedViewComponents, c.compPedViewKnockout);
    if (!Il2Cpp::IsUnityObjectSafe(g_stashes.pedViewFsmStash))
        g_stashes.pedViewFsmStash = FetchComponentStash(world, c.nsPedViewComponents.c_str(), "PedViewFsm");
    bindStash(g_stashes.pedViewWeaponLinkStash, c.nsPedViewWeapons, c.compPedViewWeaponLink);
    bindStash(g_stashes.pedViewLastVehStash,    c.nsPedViewVehicles, c.compPedViewLastVeh);
    bindStash(g_stashes.pedSkinStash,           c.nsElementsComponents, c.compPedSkin);
    g_stashes.pedGenderStash = g_stashes.pedViewStash;
    bindStash(g_stashes.pedClothesStash,        c.nsPedClothesComponents, c.compPedClothes);
    bindStash(g_stashes.pedClothesChangedStash, c.nsPedClothesComponents, c.compPedClothesChanged);
    bindStash(g_stashes.pedInZoneStash,         c.nsZoneComponents, c.compPedInZone);
    bindStash(g_stashes.pedBlockedInputStash,   c.nsPedStatesComponents, c.compPedBlockedInput);
    bindStash(g_stashes.elementFrozenStash,     c.nsElementsComponents, c.compElementFrozen);
    bindStash(g_stashes.staminaStash,           c.nsPedsComponents, c.compStamina);
    bindStash(g_stashes.staminaInfStash,        c.nsPedsComponents, c.compStaminaInfinite);
    bindStash(g_stashes.ignoreStaminaLossStash, c.nsPedsComponents, c.compIgnoreStaminaLoss);
    bindStash(g_stashes.elementInvincibleStash, c.nsElementsComponents, c.compElementInvincible);
    bindStash(g_stashes.weaponNotAllowedStash,  c.nsWeaponComponents, c.compWeaponNotAllowed);
    bindStash(g_stashes.weaponAttackCdStash,    c.nsWeaponComponents, c.compWeaponAttackCd);
    bindStash(g_stashes.weaponViewParamsStash,  c.nsWeaponComponents, c.compWeaponViewParams);
    bindStash(g_stashes.weaponRecoilStash,      c.nsWeaponComponents, c.compWeaponRecoil);
    bindStash(g_stashes.camWeaponRecoilStash,   c.nsWeaponComponents, c.compCamWeaponRecoil);
    bindStash(g_stashes.pedWeaponStash,         c.nsPedWeaponComponents, c.compPedWeapon);
    bindStash(g_stashes.pedWeaponAmmoStash,     c.nsPedWeaponComponents, c.compPedWeaponAmmo);
    bindStash(g_stashes.itemIdStash,            c.nsItemComponents, c.compItemId);
    bindStash(g_stashes.itemNameStash,          c.nsItemComponents, c.compItemName);
    bindStash(g_stashes.itemCountStash,         c.nsItemComponents, c.compItemCount);
    bindStash(g_stashes.localItemStash,         c.nsItemComponents, c.compLocalItem);
    bindStash(g_stashes.timeSetReqStash,        c.nsTimeCycle, c.compTimeSetReq);
    bindStash(g_stashes.timeFrozenReqStash,     c.nsTimeCycle, c.compTimeFrozenReq);
    bindStash(g_stashes.timeWorldStash,         c.nsTimeCycle, c.compTimeCycleWorld);
    bindStash(g_stashes.vehInternalsStash,      c.nsVehicleBodyComps, c.compVehInternals);
    bindStash(g_stashes.vehAddTorqueStash,      c.nsVehicleBodyComps, c.compVehAddTorque);
    bindStash(g_stashes.vehSpeedRestrictStash,  c.nsVehicleBodyComps, c.compVehSpeedRestrict);
    bindStash(g_stashes.vehViewInputsStash,     c.nsVehicleBodyComps, c.compVehViewInputs);
    bindStash(g_stashes.inputsDataStash,        c.nsInputsComponents, c.compInputsData);
    bindStash(g_stashes.vehPaintStash,          c.nsVehicleComponents, c.compVehPaint);
    bindStash(g_stashes.vehPaintDirtyStash,     c.nsVehicleComponents, c.compVehPaintDirty);
    bindStash(g_stashes.vehWinStash,            c.nsVehicleComponents, c.compVehWindowTint);
    bindStash(g_stashes.vehWinDirtyStash,       c.nsVehicleComponents, c.compVehWindowTintDirty);
    bindStash(g_stashes.vehWheelsStash,         c.nsVehicleComponents, c.compVehWheels);
    bindStash(g_stashes.vehRimIdStash,          c.nsVehicleWheelComps, c.compVehRimId);
    bindStash(g_stashes.vehRimColStash,         c.nsVehicleComponents, c.compVehRimColor);
    bindStash(g_stashes.vehRimColDirtyStash,    c.nsVehicleComponents, c.compVehWheelsColorUpd);
    bindStash(g_stashes.vehRimDirtyStash,       c.nsVehicleComponents, c.compVehRimUpdate);
    bindStash(g_stashes.vehTuneExtStash,        c.nsVehicleTuningComps, c.compVehTuneExternal);
    bindStash(g_stashes.vehTuneExtDirtyStash,   c.nsVehicleTuningComps, c.compVehTuneExtDirty);
    bindStash(g_stashes.checkpointTagStash,     c.nsCheckpointComponents, c.compCheckpointTag);
    g_stashes.checkpointPosStash = g_stashes.positionStash;
    bindStash(g_stashes.jobCpStash,             c.nsJobCommonComponents, c.compJobCheckpoint);
    bindStash(g_stashes.currentJobStash,        c.nsPlayersResComponents, c.compCurrentJob);
    bindStash(g_stashes.jobSearchCreateStash,   c.nsJobCommonComponents, c.compJobSearchCreate);
    bindStash(g_stashes.tempMiniStash,          c.nsTempMiniComponents, c.compTemporaryMini);
    bindStash(g_stashes.acceptPressedStash,     c.nsTempMiniComponents, c.compAcceptPressed);
    bindStash(g_stashes.graffitiCpStash,        c.nsGraffitiComponents, c.compGraffitiCp);
    bindStash(g_stashes.graffitiStateStash,     c.nsGraffitiComponents, c.compGraffitiState);
    bindStash(g_stashes.voiceChanStash,         c.nsVoiceComponents, c.compVoiceChannel);
    bindStash(g_stashes.voiceChanReqStash,      c.nsVoiceComponents, c.compVoiceChanChangeReq);
    bindStash(g_stashes.voiceChatTypeStash,     c.nsVoiceComponents, c.compVoiceChatType);
    bindStash(g_stashes.camSettingsStash,       c.nsMainCamComponents, c.compCamSettings);
    bindStash(g_stashes.freeCamActiveStash,     c.nsFreeCamComponents, c.compFreeCamActive);
    bindStash(g_stashes.freeCamTargetStash,     c.nsFreeCamComponents, c.compFreeCamTarget);
    bindStash(g_stashes.freeCamActivateStash,   c.nsFreeCamComponents, c.compFreeCamActivate);
    bindStash(g_stashes.freeCamDeactivateStash, c.nsFreeCamComponents, c.compFreeCamDeactivate);
    bindStash(g_stashes.navMarkerBlipStash,     c.nsNavMarkersComponents, c.compNavMarkerBlip);
    bindStash(g_stashes.navMarkerWithBlipStash, c.nsNavMarkersComponents, c.compNavMarkerWithBlip);
    bindStash(g_stashes.navMarkerAttachedStash, c.nsNavMarkersComponents, c.compNavMarkerAttached);
    bindStash(g_stashes.navMarkerVisibStash,    c.nsNavMarkersComponents, c.compNavMarkerVisib);
    bindStash(g_stashes.navMarkerTypeStash,     c.nsNavMarkersComponents, c.compElementNavType);
    bindStash(g_stashes.hudMapCachedMarkerStash,c.nsHudMapComponents,     c.compHudMapCachedMarker);
    bindStash(g_stashes.hudMapCreateCpStash,    c.nsHudMapComponents,     c.compHudMapCreateCp);
    bindStash(g_stashes.hudMapCheckpointStash,  c.nsHudMapComponents,     c.compHudMapCheckpoint);
    bindStash(g_stashes.weaponTizerTimerStash,  c.nsWeaponComponents,     c.compWeaponTizerTimer);
    bindStash(g_stashes.pedViewTaserKoStash,    c.nsPedViewComponents,    c.compPedViewTaserKo);
    bindStash(g_stashes.pedViewTaserKoVehStash, c.nsPedViewComponents,    c.compPedViewTaserKoVeh);
    bindStash(g_stashes.pedViewTaserPreKoStash, c.nsPedViewComponents,    c.compPedViewTaserPreKo);
    bindStash(g_stashes.pedViewKoTimerStash,    c.nsPedViewComponents,    c.compPedViewKoTimer);
    bindStash(g_stashes.pedViewIgnoreKoStash,   c.nsPedViewComponents,    c.compPedViewIgnoreKo);
    bindStash(g_stashes.pedViewMeleeKoStash,    c.nsMeleeCombatComps,     c.compPedViewMeleeKo);
    bindStash(g_stashes.loseConsciousnessStash, c.nsDeathProcComps,       c.compLoseConsciousness);

    BindStorageDirectStashes();
    g_stashes.worldReady = (g_stashes.pedViewStash != nullptr);
}

// Computes true element stride of an IL2CPP SZARRAY (T[]), passing element class T (NOT array class T[])
// to il2cpp_class_array_element_size; guards against bogus pointer-size (8) for large value-type structs.
inline size_t ResolveArrayElementStride(void* dataArr, size_t fallbackSize) {
    if (!SafeMem::IsPlausiblePtr(dataArr)) return fallbackSize;
    int32_t sz = 0;
    TCFlyGuard::TryRun([&]() {
        void* arrCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(dataArr) : nullptr;
        if (!SafeMem::IsPlausiblePtr(arrCls)) return;
        if (Il2Cpp::g_api.class_get_element_class && Il2Cpp::g_api.class_array_element_size) {
            if (void* elemCls = Il2Cpp::g_api.class_get_element_class(arrCls)) {
                sz = Il2Cpp::g_api.class_array_element_size(elemCls);
            }
        }
        if (sz <= 0 && Il2Cpp::g_api.array_element_size) {
            sz = Il2Cpp::g_api.array_element_size(arrCls);
        }
    });
    if (sz <= 0 || sz >= 8192) return fallbackSize;
    if (fallbackSize > 8 && sz == 8) return fallbackSize;
    return static_cast<size_t>(sz);
}

inline StashMeta ResolveStashMeta(void* stash) {
    {
        std::lock_guard<std::mutex> lk(g_stashMetaMutex);
        auto it = g_stashMetaCache.find(stash);
        if (it != g_stashMetaCache.end()) return it->second;
    }
    StashMeta meta{};
    TCFlyGuard::TryRun([&]() {
        void* k = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(stash) : nullptr;
        if (SafeMem::IsPlausiblePtr(k) && Il2Cpp::g_api.class_get_field_from_name && Il2Cpp::g_api.field_get_offset) {
            if (void* mapFld = Il2Cpp::g_api.class_get_field_from_name(k, "map")) {
                meta.mapOff = Il2Cpp::g_api.field_get_offset(mapFld);
                meta.isTagStash = false;
            } else if (void* setFld = Il2Cpp::g_api.class_get_field_from_name(k, "set")) {
                meta.mapOff = Il2Cpp::g_api.field_get_offset(setFld);
                meta.isTagStash = true;
            }
            if (void* dataFld = Il2Cpp::g_api.class_get_field_from_name(k, "data")) {
                meta.dataOff = Il2Cpp::g_api.field_get_offset(dataFld);
            }
        }
    });
    if (!meta.isTagStash) {
        if (void* dataArr = SafeMem::PtrAt(stash, meta.dataOff)) {
            meta.elemStride = ResolveArrayElementStride(dataArr, 0);
        }
    }
    {
        std::lock_guard<std::mutex> lk(g_stashMetaMutex);
        g_stashMetaCache[stash] = meta;
    }
    return meta;
}

// Auto-detects managed Il2CppArray (+0x20 payload) vs raw pinned native buffer (+0x00),
// matching StateScript.dylib Morpeh_Get @ 0x42584..0x4263c.
inline uint8_t* ResolveMorpehBufferBase(void* ptr, int32_t* outArrLen = nullptr) {
    if (!SafeMem::IsPlausiblePtr(ptr)) return nullptr;
    void* headerKlass = nullptr;
    if (SafeMem::ReadVal(ptr, headerKlass) && SafeMem::IsPlausiblePtr(headerKlass)) {
        if (outArrLen) {
            int32_t len = 0;
            SafeMem::ReadVal(reinterpret_cast<uint8_t*>(ptr) + Offsets::Morpeh::kIl2CppArrayLengthOff, len);
            *outArrLen = len;
        }
        return reinterpret_cast<uint8_t*>(ptr) + Offsets::Morpeh::kIl2CppArrayDataOff;
    }
    if (outArrLen) *outArrLen = 0;
    return reinterpret_cast<uint8_t*>(ptr);
}

inline void* Get(void* stash, int32_t entityId, size_t componentSize) {
    if (!SafeMem::IsPlausiblePtr(stash) || entityId < 0 || componentSize == 0) return nullptr;
    StashMeta meta = ResolveStashMeta(stash);
    if (meta.isTagStash) return nullptr;

    void* slotMap = SafeMem::PtrAt(stash, meta.mapOff);
    void* dataArr = SafeMem::PtrAt(stash, meta.dataOff);
    if (!slotMap || !dataArr) return nullptr;

    int32_t arrLen = 0;
    if (!SafeMem::ReadVal(reinterpret_cast<uint8_t*>(dataArr) + Offsets::Morpeh::kIl2CppArrayLengthOff, arrLen) || arrLen <= 0)
        return nullptr;

    size_t elemStride = meta.elemStride;
    if (elemStride == 0 || (componentSize > 8 && elemStride == 8)) {
        elemStride = ResolveArrayElementStride(dataArr, componentSize);
    }

    int32_t capacityMask = 0;
    if (!SafeMem::ReadVal(reinterpret_cast<uint8_t*>(slotMap) + Offsets::Morpeh::kMapCapacityMaskOffset, capacityMask) || capacityMask < 0)
        return nullptr;

    void* bucketsObj = SafeMem::PtrAt(slotMap, Offsets::Morpeh::kMapBucketsOffset);
    void* slotsObj   = SafeMem::PtrAt(slotMap, Offsets::Morpeh::kMapSlotsOffset);
    if (!bucketsObj || !slotsObj) return nullptr;

    int32_t bucketsLen = 0, slotsLen = 0;
    uint8_t* bucketsBase = ResolveMorpehBufferBase(bucketsObj, &bucketsLen);
    uint8_t* slotsBase   = ResolveMorpehBufferBase(slotsObj, &slotsLen);
    if (!bucketsBase || !slotsBase) return nullptr;

    int32_t bucketIdx = entityId & capacityMask;
    if (bucketIdx < 0 || (bucketsLen > 0 && bucketIdx >= bucketsLen)) return nullptr;

    int32_t rawBucket = 0;
    if (!SafeMem::ReadVal(bucketsBase + static_cast<size_t>(bucketIdx) * 4, rawBucket) || rawBucket <= 0)
        return nullptr;

    int32_t slotIdx = rawBucket - 1;
    uint8_t* dataBase = reinterpret_cast<uint8_t*>(dataArr) + Offsets::Morpeh::kIl2CppArrayDataOff;

    for (int step = 0; step < Offsets::Morpeh::kMaxHashChainSteps && slotIdx >= 0; ++step) {
        if (slotsLen > 0 && slotIdx >= slotsLen) break;
        IntHashMapSlot slot{};
        if (!SafeMem::ReadVal(slotsBase + static_cast<size_t>(slotIdx) * sizeof(IntHashMapSlot), slot)) break;
        if (slot.key - 1 == entityId) {
            if (slotIdx >= arrLen) return nullptr;
            return dataBase + static_cast<size_t>(slotIdx) * elemStride;
        }
        slotIdx = slot.nextIdx;
    }
    return nullptr;
}

// 100% Native zero-GC Has() supporting both Stash<T> (IntSlotMap) and TagStash (IntHashSet)
inline bool HasNative(void* stash, int32_t entityId) {
    if (!SafeMem::IsPlausiblePtr(stash) || entityId < 0) return false;
    StashMeta meta = ResolveStashMeta(stash);
    void* mapOrSet = SafeMem::PtrAt(stash, meta.mapOff);
    if (!mapOrSet) return false;

    int32_t capacityMask = 0;
    if (!SafeMem::ReadVal(reinterpret_cast<uint8_t*>(mapOrSet) + 0x18, capacityMask) || capacityMask < 0)
        return false;

    size_t bucketsOff = 0x28;
    size_t slotsOff   = meta.isTagStash ? 0x30 : Offsets::Morpeh::kMapSlotsOffset;

    void* bucketsObj = SafeMem::PtrAt(mapOrSet, bucketsOff);
    void* slotsObj   = SafeMem::PtrAt(mapOrSet, slotsOff);
    if (!bucketsObj || !slotsObj) return false;

    int32_t bucketsLen = 0, slotsLen = 0;
    uint8_t* bucketsBase = ResolveMorpehBufferBase(bucketsObj, &bucketsLen);
    uint8_t* slotsBase   = ResolveMorpehBufferBase(slotsObj, &slotsLen);
    if (!bucketsBase || !slotsBase) return false;

    int32_t bucketIdx = entityId & capacityMask;
    if (bucketIdx < 0 || (bucketsLen > 0 && bucketIdx >= bucketsLen)) return false;

    int32_t rawBucket = 0;
    if (!SafeMem::ReadVal(bucketsBase + static_cast<size_t>(bucketIdx) * 4, rawBucket) || rawBucket <= 0)
        return false;

    int32_t maxSlots = (slotsLen > 0) ? (meta.isTagStash ? (slotsLen / 2) : slotsLen) : 0;
    int32_t slotIdx = rawBucket - 1;
    for (int step = 0; step < Offsets::Morpeh::kMaxHashChainSteps && slotIdx >= 0; ++step) {
        if (maxSlots > 0 && slotIdx >= maxSlots) break;
        IntHashMapSlot slot{};
        if (!SafeMem::ReadVal(slotsBase + static_cast<size_t>(slotIdx) * sizeof(IntHashMapSlot), slot)) break;
        if (slot.key - 1 == entityId) return true;
        slotIdx = slot.nextIdx;
    }
    return false;
}

template <typename Fn>
inline void ForEachSlotEntityId(void* stash, Fn&& cb) {
    if (!SafeMem::IsPlausiblePtr(stash)) return;
    StashMeta meta = ResolveStashMeta(stash);
    void* slotMap = SafeMem::PtrAt(stash, meta.mapOff);
    if (!SafeMem::IsPlausiblePtr(slotMap)) return;

    int32_t lastIndex = 0;
    if (!SafeMem::ReadVal(reinterpret_cast<uint8_t*>(slotMap) + 0x1C, lastIndex) || lastIndex <= 0)
        return;

    size_t slotsOff = meta.isTagStash ? 0x30 : Offsets::Morpeh::kMapSlotsOffset;
    void* slotsObj = SafeMem::PtrAt(slotMap, slotsOff);
    if (!SafeMem::IsPlausiblePtr(slotsObj)) return;

    int32_t slotsLen = 0;
    uint8_t* sBase = ResolveMorpehBufferBase(slotsObj, &slotsLen);
    if (!sBase) return;

    int maxSlots = (slotsLen > 0) ? (meta.isTagStash ? (slotsLen / 2) : slotsLen) : lastIndex;
    int limit = std::min(lastIndex, std::min(maxSlots, 4096));
    if (limit <= 0) return;

    // Bulk-read slots in 256-slot (2KB) chunks -> cuts kernel syscalls up to 256x (eliminates UI/game lag!)
    IntHashMapSlot chunk[256];
    int idx = 0;
    while (idx < limit) {
        int n = std::min(256, limit - idx);
        size_t bytes = static_cast<size_t>(n) * sizeof(IntHashMapSlot);
        if (!SafeMem::Read(sBase + static_cast<size_t>(idx) * sizeof(IntHashMapSlot), chunk, bytes)) {
            // Fallback per-slot if trailing page boundary is unmapped
            for (int k = 0; k < n; ++k) {
                IntHashMapSlot slot{};
                if (!SafeMem::ReadVal(sBase + static_cast<size_t>(idx + k) * sizeof(IntHashMapSlot), slot)) return;
                if (slot.key > 0) cb(slot.key - 1, idx + k);
            }
            return;
        }
        for (int k = 0; k < n; ++k) {
            if (chunk[k].key > 0) cb(chunk[k].key - 1, idx + k);
        }
        idx += n;
    }
}

inline int64_t CreateEntity() {
    void* world = GetWorld();
    if (!SafeMem::IsPlausiblePtr(world) || !Il2Cpp::g_unity.mWorld_CreateEntity) return 0;
    void* args[1] = { world };
    void* ret = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mWorld_CreateEntity, nullptr, args);
    void* u = Il2Cpp::Unbox(ret);
    int64_t ent = 0;
    return (u && SafeMem::ReadVal(u, ent)) ? ent : 0;
}

inline void* GetEntityTransform(int64_t viewOrLogicEnt) {
    if (viewOrLogicEnt <= 0) return nullptr;
    int32_t id = UnpackEntityId(viewOrLogicEnt);
    if (id < 0) return nullptr;
    if (void* p = Get(g_stashes.unityTransformStash, id, 8)) {
        void* tr = SafeMem::PtrAt(p, 0);
        if (Il2Cpp::IsTransformAlive(tr)) return tr;
    }
    if (void* pv = Get(g_stashes.pedViewStash, id, 384)) {
        void* tr = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewRoot);
        if (Il2Cpp::IsTransformAlive(tr)) return tr;
    }
    return nullptr;
}

inline void* GetEntityRigidbody(int64_t viewOrLogicEnt) {
    if (viewOrLogicEnt <= 0) return nullptr;
    int32_t id = UnpackEntityId(viewOrLogicEnt);
    if (id < 0) return nullptr;
    if (void* p = Get(g_stashes.unityRigidbodyStash, id, 8)) {
        void* rb = SafeMem::PtrAt(p, 0);
        if (Il2Cpp::IsRigidbodyAlive(rb)) return rb;
    }
    if (void* pv = Get(g_stashes.pedViewStash, id, 384)) {
        void* rb = SafeMem::PtrAt(pv, Offsets::g_cfg.offPedViewRigidbody);
        if (Il2Cpp::IsRigidbodyAlive(rb)) return rb;
    }
    return nullptr;
}

inline bool Has(void* stash, int64_t packedEntity) {
    if (!SafeMem::IsPlausiblePtr(stash) || packedEntity == 0) return false;
    int32_t id = UnpackEntityId(packedEntity);
    if (id >= 0) return HasNative(stash, id);
    return false;
}

inline bool SetEmpty(void* stash, int64_t packedEntity) {
    if (!SafeMem::IsPlausiblePtr(stash) || packedEntity == 0) return false;
    void* klass = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(stash) : nullptr;
    void* m = klass ? Il2Cpp::g_api.class_get_method_from_name(klass, "Set", 1) : nullptr;
    if (!m && klass) m = Il2Cpp::g_api.class_get_method_from_name(klass, "Add", 1);
    if (!m) return false;
    void* args[1] = { &packedEntity };
    Il2Cpp::SafeInvoke(m, stash, args);
    return true;
}

inline bool SetComponent(void* stash, int64_t packedEntity, const void* componentData, size_t size) {
    if (!SafeMem::IsPlausiblePtr(stash) || packedEntity == 0 || !componentData || size == 0) return false;
    int32_t entId = UnpackEntityId(packedEntity);
    void* slotPtr = (entId >= 0) ? Get(stash, entId, size) : nullptr;
    if (!slotPtr) {
        if (!Has(stash, packedEntity)) SetEmpty(stash, packedEntity);
        entId = UnpackEntityId(packedEntity);
        slotPtr = (entId >= 0) ? Get(stash, entId, size) : nullptr;
    }
    return slotPtr ? SafeMem::Write(slotPtr, componentData, size) : false;
}

inline bool Remove(void* stash, int64_t packedEntity) {
    if (!SafeMem::IsPlausiblePtr(stash) || packedEntity == 0) return false;
    void* klass = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(stash) : nullptr;
    void* rm = klass ? Il2Cpp::g_api.class_get_method_from_name(klass, "Remove", 1) : nullptr;
    if (!rm) return false;
    void* args[1] = { &packedEntity };
    Il2Cpp::SafeInvoke(rm, stash, args);
    return true;
}

inline void RemoveAll(void* stash) {
    if (!SafeMem::IsPlausiblePtr(stash)) return;
    void* klass = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(stash) : nullptr;
    void* rmAll = klass ? Il2Cpp::g_api.class_get_method_from_name(klass, "RemoveAll", 0) : nullptr;
    if (rmAll) Il2Cpp::SafeInvoke(rmAll, stash, nullptr);
}

inline int32_t GetLocalPlayerId() {
    RefreshStoragesIfNeeded();
    if (SafeMem::IsPlausiblePtr(g_stashes.realPlayerStorage)) {
        void* psCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(g_stashes.realPlayerStorage) : nullptr;
        size_t lpOff = Il2Cpp::FieldOffset(psCls, "_localPlayerEntity", Offsets::g_cfg.offLocalPlayerEntityRaw);
        int64_t ent = 0;
        if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(g_stashes.realPlayerStorage) + lpOff, ent) && ent != 0) {
            int32_t pid = UnpackEntityId(ent);
            if (pid >= 0) return pid;
        }
    }
    int32_t foundId = -1;
    ForEachSlotEntityId(g_stashes.localPlayerStash, [&](int32_t entId, int) {
        if (foundId < 0 && entId >= 0) foundId = entId;
    });
    return foundId;
}

inline int32_t GetLocalViewId() {
    RefreshStoragesIfNeeded();
    int32_t foundView = -1;
    ForEachSlotEntityId(g_stashes.pedViewLocalStash, [&](int32_t viewId, int) {
        if (foundView < 0 && viewId >= 0) foundView = viewId;
    });
    return foundView;
}

inline int64_t GetLocalPlayerEntity() {
    RefreshStoragesIfNeeded();
    if (SafeMem::IsPlausiblePtr(g_stashes.realPlayerStorage)) {
        void* psCls = Il2Cpp::g_api.object_get_class ? Il2Cpp::g_api.object_get_class(g_stashes.realPlayerStorage) : nullptr;
        size_t lpOff = Il2Cpp::FieldOffset(psCls, "_localPlayerEntity", Offsets::g_cfg.offLocalPlayerEntityRaw);
        int64_t ent = 0;
        if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(g_stashes.realPlayerStorage) + lpOff, ent) && ent != 0)
            return ent;
    }
    int32_t lpId = GetLocalPlayerId();
    return (lpId >= 0) ? GetEntityById(lpId) : 0;
}

inline int64_t GetLocalPedEntity() {
    RefreshStoragesIfNeeded();
    // Path 1: Fast zero-GC PedViewLocal -> LogicEntityLink (always matches the active local character)
    int64_t foundPed = 0;
    ForEachSlotEntityId(g_stashes.pedViewLocalStash, [&](int32_t viewId, int) {
        if (foundPed == 0 && SafeMem::IsPlausiblePtr(g_stashes.pedToLogicLinkStash)) {
            if (void* linkPtr = Get(g_stashes.pedToLogicLinkStash, viewId, 8)) {
                int64_t ped = 0;
                if (SafeMem::ReadVal(linkPtr, ped) && ped != 0 && UnpackEntityId(ped) >= 0)
                    foundPed = ped;
            }
        }
    });
    if (foundPed != 0) return foundPed;

    // Path 2: LocalPlayerId -> ControlsEntity
    int32_t pid = GetLocalPlayerId();
    if (pid >= 0 && SafeMem::IsPlausiblePtr(g_stashes.controlsEntityStash)) {
        if (void* ctrl = Get(g_stashes.controlsEntityStash, pid, 8)) {
            int64_t pedEnt = 0;
            if (SafeMem::ReadVal(ctrl, pedEnt) && pedEnt != 0 && UnpackEntityId(pedEnt) >= 0)
                return pedEnt;
        }
    }
    return -1;
}

inline int64_t GetLocalVehicleLogicEntity(int64_t pedEntity) {
    if (pedEntity <= 0) pedEntity = GetLocalPedEntity();
    if (pedEntity <= 0 || !SafeMem::IsPlausiblePtr(g_stashes.pedSeatStash)) return -1;
    int32_t pedId = UnpackEntityId(pedEntity);
    // PedSeat: ushort index (0x0), Entity logicVehicle (0x8)
    if (void* seatPtr = Get(g_stashes.pedSeatStash, pedId, 16)) {
        int64_t vehEnt = 0;
        if (SafeMem::ReadVal(reinterpret_cast<uint8_t*>(seatPtr) + 0x8, vehEnt) && vehEnt != 0)
            return vehEnt;
    }
    return -1;
}

inline int64_t GetViewEntity(int64_t logicEntity) {
    if (logicEntity <= 0 || !SafeMem::IsPlausiblePtr(g_stashes.viewLinkStash)) return 0;
    int32_t id = UnpackEntityId(logicEntity);
    if (void* p = Get(g_stashes.viewLinkStash, id, 8)) {
        int64_t v = 0;
        if (SafeMem::ReadVal(p, v)) return v;
    }
    return 0;
}

inline void ResolvePedProvOffsets(void* pedProvKlass, size_t outOff[7]) {
    const char* names[7] = { "_model", "_pedView", "_unityGameObject", "_unityTransform", "_unityRigidbody", "_pedSeat", "_pedDead" };
    const size_t fallback[7] = {
        Offsets::PedProvFallback::_model,
        Offsets::PedProvFallback::_pedView,
        Offsets::PedProvFallback::_unityGameObject,
        Offsets::PedProvFallback::_unityTransform,
        Offsets::PedProvFallback::_unityRigidbody,
        Offsets::PedProvFallback::_pedSeat,
        Offsets::PedProvFallback::_pedDead,
    };
    for (int i = 0; i < 7; ++i) {
        outOff[i] = Il2Cpp::FieldOffset(pedProvKlass, names[i], fallback[i]);
    }
}

struct ManagedDistanceResult {
    int64_t localPlayer;
    int64_t otherPlayer;
    Vec3    otherPlayerPosition;
    int32_t otherPlayerDimension;
    float   distance;
};

// Resolves accurate world position for NEAR and FAR players via PlayerDistanceUtils or Morpeh positionStash
inline bool QueryRemotePlayerPosition(int32_t playerId, uint32_t digitalId, int64_t pedEnt,
                                      Vec3& outPos, int32_t& outDim, float& outDist) {
    Il2Cpp::ResolvePlayerDistMethodsLazy();
    void* pdu = EnsurePlayerDistUtilsResolved();
    if (SafeMem::IsPlausiblePtr(pdu)) {
        if (digitalId > 0 && Il2Cpp::g_unity.mPlayerDist_ByDigitalId) {
            ManagedDistanceResult res{};
            uint32_t d = digitalId;
            void* args[2] = { &d, &res };
            if (void* ret = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPlayerDist_ByDigitalId, pdu, args)) {
                void* u = Il2Cpp::Unbox(ret);
                uint8_t ok = 0;
                if (u && SafeMem::ReadVal(u, ok) && ok != 0 && IsFiniteVec3(res.otherPlayerPosition) &&
                    (std::fabs(res.otherPlayerPosition.x) > 0.25f || std::fabs(res.otherPlayerPosition.z) > 0.25f)) {
                    outPos  = res.otherPlayerPosition;
                    outDim  = res.otherPlayerDimension;
                    outDist = res.distance;
                    return true;
                }
            }
        }
        if (playerId >= 0 && Il2Cpp::g_unity.mPlayerDist_ByEntity) {
            int64_t plEnt = GetEntityById(playerId);
            if (plEnt > 0) {
                ManagedDistanceResult res{};
                void* args[2] = { &plEnt, &res };
                if (void* ret = Il2Cpp::SafeInvoke(Il2Cpp::g_unity.mPlayerDist_ByEntity, g_stashes.realPlayerDistUtils, args)) {
                    void* u = Il2Cpp::Unbox(ret);
                    uint8_t ok = 0;
                    if (u && SafeMem::ReadVal(u, ok) && ok != 0 && IsFiniteVec3(res.otherPlayerPosition) &&
                        (std::fabs(res.otherPlayerPosition.x) > 0.25f || std::fabs(res.otherPlayerPosition.z) > 0.25f)) {
                        outPos  = res.otherPlayerPosition;
                        outDim  = res.otherPlayerDimension;
                        outDist = res.distance;
                        return true;
                    }
                }
            }
        }
    }
    int32_t pedId = (pedEnt > 0) ? UnpackEntityId(pedEnt) : -1;
    if (pedId >= 0 && SafeMem::IsPlausiblePtr(g_stashes.positionStash)) {
        if (void* pPtr = Get(g_stashes.positionStash, pedId, sizeof(Vec3))) {
            Vec3 p{};
            if (SafeMem::ReadVal(pPtr, p) && IsFiniteVec3(p) &&
                (std::fabs(p.x) > 0.25f || std::fabs(p.z) > 0.25f)) {
                outPos = p;
                return true;
            }
        }
    }
    return false;
}

} // namespace Morpeh

// Column-major 4x4 projection/view-proj W2S (Y flipped for iOS Metal)
inline bool ESPWorldToScreen(const float m[16], const Vec3& world, float screenW, float screenH, Vec2& out) {
    float x = m[0] * world.x + m[4] * world.y + m[8]  * world.z + m[12];
    float y = m[1] * world.x + m[5] * world.y + m[9]  * world.z + m[13];
    float w = m[3] * world.x + m[7] * world.y + m[11] * world.z + m[15];
    if (w < 0.001f) return false;
    float invW = 1.0f / w;
    out.x = (x * invW + 1.0f) * 0.5f * screenW;
    out.y = (1.0f - y * invW) * 0.5f * screenH;
    return std::isfinite(out.x) && std::isfinite(out.y);
}

// ============================================================================
// 5. JSON Profiles & Runtime Hot-Reloadable offsets.json
// ============================================================================
namespace ConfigStore {

inline NSString* BaseDir() {
    NSString* docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString* dir = [docs stringByAppendingPathComponent:@"OneStateFull"];
    [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    return dir;
}

inline NSString* ProfilesDir() {
    NSString* dir = [BaseDir() stringByAppendingPathComponent:@"profiles"];
    [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    return dir;
}

inline void LoadOffsetsJson() {
    NSString* path = [BaseDir() stringByAppendingPathComponent:@"offsets.json"];
    NSData* data = [NSData dataWithContentsOfFile:path];
    if (!data) return;
    NSDictionary* d = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    if (![d isKindOfClass:[NSDictionary class]]) return;

    auto sVal = [&](NSString* k, std::string& dst) {
        if ([d[k] isKindOfClass:[NSString class]]) dst = [d[k] UTF8String];
    };
    auto uVal = [&](NSString* k, size_t& dst) {
        if ([d[k] isKindOfClass:[NSNumber class]]) dst = [d[k] unsignedLongValue];
    };

    sVal(@"nsEnviro", Offsets::g_cfg.nsEnviro);
    sVal(@"nsStreaming", Offsets::g_cfg.nsStreaming);
    sVal(@"nsPlayerStorages", Offsets::g_cfg.nsPlayerStorages);
    sVal(@"nsVehicleComponents", Offsets::g_cfg.nsVehicleComponents);
    sVal(@"nsTimeCycle", Offsets::g_cfg.nsTimeCycle);
    sVal(@"nsWeaponComponents", Offsets::g_cfg.nsWeaponComponents);
    sVal(@"nsPedWeaponComponents", Offsets::g_cfg.nsPedWeaponComponents);
    sVal(@"nsFallDamageSys", Offsets::g_cfg.nsFallDamageSys);
    sVal(@"clsFallDamageSys", Offsets::g_cfg.clsFallDamageSys);
    sVal(@"clsRealPlayerStorage", Offsets::g_cfg.clsRealPlayerStorage);
    sVal(@"clsRealPedViewStorage", Offsets::g_cfg.clsRealPedViewStorage);
    sVal(@"clsRealMainCamStorage", Offsets::g_cfg.clsRealMainCamStorage);
    sVal(@"clsRealLocalInfo", Offsets::g_cfg.clsRealLocalInfo);
    sVal(@"clsWeaponStorage", Offsets::g_cfg.clsWeaponStorage);
    sVal(@"compPosition", Offsets::g_cfg.compPosition);
    sVal(@"compHealth", Offsets::g_cfg.compHealth);
    sVal(@"compArmor", Offsets::g_cfg.compArmor);
    sVal(@"compPedView", Offsets::g_cfg.compPedView);
    sVal(@"compLogicEntityLink", Offsets::g_cfg.compLogicEntityLink);
    sVal(@"compPedClothes", Offsets::g_cfg.compPedClothes);
    sVal(@"compVehPaint", Offsets::g_cfg.compVehPaint);
    sVal(@"compVehPaintDirty", Offsets::g_cfg.compVehPaintDirty);
    sVal(@"compVehWindowTint", Offsets::g_cfg.compVehWindowTint);
    sVal(@"compVehRimId", Offsets::g_cfg.compVehRimId);
    sVal(@"compVehRimColor", Offsets::g_cfg.compVehRimColor);
    uVal(@"offCamViewProjMatrix", Offsets::g_cfg.offCamViewProjMatrix);
    uVal(@"offBoneArray", Offsets::g_cfg.offBoneArray);
}

inline void SaveOffsetsTemplateIfMissing() {
    NSString* path = [BaseDir() stringByAppendingPathComponent:@"offsets.json"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:path]) return;
    NSDictionary* tmpl = @{
        @"nsEnviro": @(Offsets::g_cfg.nsEnviro.c_str()),
        @"nsStreaming": @(Offsets::g_cfg.nsStreaming.c_str()),
        @"nsPlayerStorages": @(Offsets::g_cfg.nsPlayerStorages.c_str()),
        @"nsVehicleComponents": @(Offsets::g_cfg.nsVehicleComponents.c_str()),
        @"nsTimeCycle": @(Offsets::g_cfg.nsTimeCycle.c_str()),
        @"nsWeaponComponents": @(Offsets::g_cfg.nsWeaponComponents.c_str()),
        @"nsPedWeaponComponents": @(Offsets::g_cfg.nsPedWeaponComponents.c_str()),
        @"nsFallDamageSys": @(Offsets::g_cfg.nsFallDamageSys.c_str()),
        @"clsFallDamageSys": @(Offsets::g_cfg.clsFallDamageSys.c_str()),
        @"clsRealPlayerStorage": @(Offsets::g_cfg.clsRealPlayerStorage.c_str()),
        @"clsRealPedViewStorage": @(Offsets::g_cfg.clsRealPedViewStorage.c_str()),
        @"clsRealMainCamStorage": @(Offsets::g_cfg.clsRealMainCamStorage.c_str()),
        @"clsRealLocalInfo": @(Offsets::g_cfg.clsRealLocalInfo.c_str()),
        @"clsWeaponStorage": @(Offsets::g_cfg.clsWeaponStorage.c_str()),
        @"compPosition": @(Offsets::g_cfg.compPosition.c_str()),
        @"compHealth": @(Offsets::g_cfg.compHealth.c_str()),
        @"compArmor": @(Offsets::g_cfg.compArmor.c_str()),
        @"compPedView": @(Offsets::g_cfg.compPedView.c_str()),
        @"compLogicEntityLink": @(Offsets::g_cfg.compLogicEntityLink.c_str()),
        @"compPedClothes": @(Offsets::g_cfg.compPedClothes.c_str()),
        @"compVehPaint": @(Offsets::g_cfg.compVehPaint.c_str()),
        @"compVehPaintDirty": @(Offsets::g_cfg.compVehPaintDirty.c_str()),
        @"compVehWindowTint": @(Offsets::g_cfg.compVehWindowTint.c_str()),
        @"compVehRimId": @(Offsets::g_cfg.compVehRimId.c_str()),
        @"compVehRimColor": @(Offsets::g_cfg.compVehRimColor.c_str()),
        @"offCamViewProjMatrix": @(Offsets::g_cfg.offCamViewProjMatrix),
        @"offBoneArray": @(Offsets::g_cfg.offBoneArray),
    };
    if (NSData* json = [NSJSONSerialization dataWithJSONObject:tmpl options:NSJSONWritingPrettyPrinted error:nil]) {
        [json writeToFile:path atomically:YES];
    }
}

inline void SaveProfile(const char* name) {
    NSString* path = [ProfilesDir() stringByAppendingPathComponent:[NSString stringWithFormat:@"%s.json", name]];
    NSDictionary* d = @{
        @"bEnableESP": @(g_cfgState.bEnableESP),
        @"bEspBox": @(g_cfgState.bEspBox),
        @"bEspHealth": @(g_cfgState.bEspHealth),
        @"bEspArmor": @(g_cfgState.bEspArmor),
        @"bEspName": @(g_cfgState.bEspName),
        @"bSkeletonESP": @(g_cfgState.bSkeletonESP),
        @"bSprayESP": @(g_cfgState.bSprayESP),
        @"bEnableAimbot": @(g_cfgState.bEnableAimbot),
        @"bSilentAim": @(g_cfgState.bSilentAim),
        @"bSilentWallbang": @(g_cfgState.bSilentWallbang),
        @"bAutoMapMarkerTp": @(g_cfgState.bAutoMapMarkerTp),
        @"bFreeCamGhostVoice": @(g_cfgState.bFreeCamGhostVoice),
        @"freeCamGhostDepthY": @(g_cfgState.freeCamGhostDepthY),
        @"freeCamOrbitDist": @(g_cfgState.freeCamOrbitDist),
        @"freeCamSpeed": @(g_cfgState.freeCamSpeed),
        @"bKillAroundGhostStep": @(g_cfgState.bKillAroundGhostStep),
        @"bKillAroundIncludeVehicles": @(g_cfgState.bKillAroundIncludeVehicles),
        @"bKillAroundOneHit": @(g_cfgState.bKillAroundOneHit),
        @"killAroundRadius": @(g_cfgState.killAroundRadius),
        @"languageMode": @(g_cfgState.languageMode),
        @"bNeon3DGrid": @(g_cfgState.bNeon3DGrid),
        @"neonPreset": @(g_cfgState.neonPreset),
        @"bInstantReload": @(g_cfgState.bInstantReload),
        @"bNoRecoil": @(g_cfgState.bNoRecoil),
        @"bRapidFire": @(g_cfgState.bRapidFire),
        @"aimFov": @(g_cfgState.aimFov),
        @"aimSmoothing": @(g_cfgState.aimSmoothing),
        @"bNoFall": @(g_cfgState.bNoFall),
        @"bAntiTaser": @(g_cfgState.bAntiTaser),
        @"bAntiFaint": @(g_cfgState.bAntiFaint),
        @"bInfStamina": @(g_cfgState.bInfStamina),
        @"bAutoHeal": @(g_cfgState.bAutoHeal),
        @"bAutoVest": @(g_cfgState.bAutoVest),
        @"bGreenZoneBypass": @(g_cfgState.bGreenZoneBypass),
        @"bZoneGuard": @(g_cfgState.bZoneGuard),
        @"bSuperJump": @(g_cfgState.bSuperJump),
        @"bSuperPunch": @(g_cfgState.bSuperPunch),
        @"bScaleEnable": @(g_cfgState.bScaleEnable),
        @"bAntiTp": @(g_cfgState.bAntiTp),
        @"bAntiTpCar": @(g_cfgState.bAntiTpCar),
        @"bAntiAfk": @(g_cfgState.bAntiAfk),
        @"bNickBypass": @(g_cfgState.bNickBypass),
        @"bVoiceGuard": @(g_cfgState.bVoiceGuard),
        @"bDeviceIdSpoof": @(g_cfgState.bDeviceIdSpoof),
        @"bCarHack": @(g_cfgState.bCarHack),
        @"bAutoRepair": @(g_cfgState.bAutoRepair),
        @"carSpeed": @(g_cfgState.carSpeed),
        @"bDayNightEnable": @(g_cfgState.bDayNightEnable),
        @"dayNightTime": @(g_cfgState.dayNightTime),
        @"bFovEnable": @(g_cfgState.bFovEnable),
        @"cameraFov": @(g_cfgState.cameraFov),
        @"targetFPS": @(g_cfgState.targetFPS),
        @"bStreamerMode": @(g_cfgState.bStreamerMode),
        @"themeH": @(ThemeEngine::g_state.hue),
        @"themeS": @(ThemeEngine::g_state.sat),
        @"themeV": @(ThemeEngine::g_state.val),
        @"glassAlpha": @(ThemeEngine::g_state.glassAlpha),
        @"uiScale": @(ThemeEngine::g_state.uiScale),
        @"btnScale": @(ThemeEngine::g_state.btnScale),
    };
    if (NSData* json = [NSJSONSerialization dataWithJSONObject:d options:NSJSONWritingPrettyPrinted error:nil]) {
        [json writeToFile:path atomically:YES];
        g_cfgState.lastStatusBanner = std::string("Saved profile: ") + name;
    }
}

inline void LoadProfile(const char* name) {
    NSString* path = [ProfilesDir() stringByAppendingPathComponent:[NSString stringWithFormat:@"%s.json", name]];
    NSData* data = [NSData dataWithContentsOfFile:path];
    if (!data) return;
    NSDictionary* d = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    if (![d isKindOfClass:[NSDictionary class]]) return;
    auto bVal = [&](NSString* k, bool& dst) { if (d[k]) dst = [d[k] boolValue]; };
    auto fVal = [&](NSString* k, float& dst) { if (d[k]) dst = [d[k] floatValue]; };
    auto iVal = [&](NSString* k, int& dst) { if (d[k]) dst = [d[k] intValue]; };
    bVal(@"bEnableESP", g_cfgState.bEnableESP);
    bVal(@"bEspBox", g_cfgState.bEspBox);
    bVal(@"bEspHealth", g_cfgState.bEspHealth);
    bVal(@"bEspArmor", g_cfgState.bEspArmor);
    bVal(@"bEspName", g_cfgState.bEspName);
    bVal(@"bSkeletonESP", g_cfgState.bSkeletonESP);
    bVal(@"bSprayESP", g_cfgState.bSprayESP);
    bVal(@"bEnableAimbot", g_cfgState.bEnableAimbot);
    bVal(@"bSilentAim", g_cfgState.bSilentAim);
    bVal(@"bSilentWallbang", g_cfgState.bSilentWallbang);
    bVal(@"bAutoMapMarkerTp", g_cfgState.bAutoMapMarkerTp);
    bVal(@"bFreeCamGhostVoice", g_cfgState.bFreeCamGhostVoice);
    fVal(@"freeCamGhostDepthY", g_cfgState.freeCamGhostDepthY);
    fVal(@"freeCamOrbitDist", g_cfgState.freeCamOrbitDist);
    bVal(@"bKillAroundGhostStep", g_cfgState.bKillAroundGhostStep);
    bVal(@"bKillAroundIncludeVehicles", g_cfgState.bKillAroundIncludeVehicles);
    bVal(@"bKillAroundOneHit", g_cfgState.bKillAroundOneHit);
    fVal(@"killAroundRadius", g_cfgState.killAroundRadius);
    fVal(@"freeCamSpeed", g_cfgState.freeCamSpeed);
    iVal(@"languageMode", g_cfgState.languageMode);
    bVal(@"bNeon3DGrid", g_cfgState.bNeon3DGrid);
    iVal(@"neonPreset", g_cfgState.neonPreset);
    bVal(@"bInstantReload", g_cfgState.bInstantReload);
    bVal(@"bNoRecoil", g_cfgState.bNoRecoil);
    bVal(@"bRapidFire", g_cfgState.bRapidFire);
    fVal(@"aimFov", g_cfgState.aimFov);
    fVal(@"aimSmoothing", g_cfgState.aimSmoothing);
    bVal(@"bNoFall", g_cfgState.bNoFall);
    bVal(@"bAntiTaser", g_cfgState.bAntiTaser);
    bVal(@"bAntiFaint", g_cfgState.bAntiFaint);
    bVal(@"bInfStamina", g_cfgState.bInfStamina);
    bVal(@"bAutoHeal", g_cfgState.bAutoHeal);
    bVal(@"bAutoVest", g_cfgState.bAutoVest);
    bVal(@"bGreenZoneBypass", g_cfgState.bGreenZoneBypass);
    bVal(@"bZoneGuard", g_cfgState.bZoneGuard);
    bVal(@"bSuperJump", g_cfgState.bSuperJump);
    bVal(@"bSuperPunch", g_cfgState.bSuperPunch);
    bVal(@"bScaleEnable", g_cfgState.bScaleEnable);
    bVal(@"bAntiTp", g_cfgState.bAntiTp);
    bVal(@"bAntiTpCar", g_cfgState.bAntiTpCar);
    bVal(@"bAntiAfk", g_cfgState.bAntiAfk);
    bVal(@"bNickBypass", g_cfgState.bNickBypass);
    bVal(@"bVoiceGuard", g_cfgState.bVoiceGuard);
    bVal(@"bDeviceIdSpoof", g_cfgState.bDeviceIdSpoof);
    bVal(@"bCarHack", g_cfgState.bCarHack);
    bVal(@"bAutoRepair", g_cfgState.bAutoRepair);
    fVal(@"carSpeed", g_cfgState.carSpeed);
    bVal(@"bDayNightEnable", g_cfgState.bDayNightEnable);
    fVal(@"dayNightTime", g_cfgState.dayNightTime);
    bVal(@"bFovEnable", g_cfgState.bFovEnable);
    fVal(@"cameraFov", g_cfgState.cameraFov);
    fVal(@"targetFPS", g_cfgState.targetFPS);
    bVal(@"bStreamerMode", g_cfgState.bStreamerMode);
    fVal(@"themeH", ThemeEngine::g_state.hue);
    fVal(@"themeS", ThemeEngine::g_state.sat);
    fVal(@"themeV", ThemeEngine::g_state.val);
    fVal(@"glassAlpha", ThemeEngine::g_state.glassAlpha);
    fVal(@"uiScale", ThemeEngine::g_state.uiScale);
    fVal(@"btnScale", ThemeEngine::g_state.btnScale);
    g_cfgState.lastStatusBanner = std::string("Loaded profile: ") + name;
}

} // namespace ConfigStore
