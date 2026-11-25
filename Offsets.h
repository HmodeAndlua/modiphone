#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

// Offsets.h — Central IL2CPP / Morpeh ECS / UnityEngine Target Configuration
// Target: One State RP (com.Chillgaming.oneState2 / com.Chillgaming.oneState / com.Chillgaming.OneState)
//
// Any value hydrated dynamically by StateScript's closed AuthV4 server (SlotTable::FetchH / Auth::SdStr)
// is represented as a named config field below and can be overridden at runtime from:
//   Documents/OneStateFull/offsets.json
// without recompiling the dylib.

namespace Offsets {

inline int ArmorMaxForType(uint8_t armorType) {
    switch (armorType) {
        case 1:  return 1500;
        case 2:  return 2000;
        case 3:  return 2500;
        default: return 1000;
    }
}

// Verified native Morpeh Stash<T> / IntHashMap layout (Morpeh_Get @ 0x42390)
namespace Morpeh {
    inline constexpr size_t kStashMapOffset        = 0x28; // IntHashMap*
    inline constexpr size_t kStashDataArrayOffset  = 0x30; // T[] (IL2CPP array)
    inline constexpr size_t kMapCapacityMaskOffset = 0x18; // int32 capacityMask
    inline constexpr size_t kMapBucketsOffset      = 0x28; // int32[]* buckets
    inline constexpr size_t kMapSlotsOffset        = 0x40; // Slot[]* (8B/slot: {int32 key+1, int32 next})
    inline constexpr size_t kIl2CppArrayLengthOff  = 0x18; // int32 length
    inline constexpr size_t kIl2CppArrayDataOff    = 0x20; // T[0]
    inline constexpr int    kMaxHashChainSteps     = 100;
}

// Verified PedProv field fallbacks (PedProv_ResolveOffsets @ 0x3b5240)
namespace PedProvFallback {
    inline constexpr size_t _model           = 0x98;
    inline constexpr size_t _pedView         = 0xA0;
    inline constexpr size_t _unityGameObject = 0xC0;
    inline constexpr size_t _unityTransform  = 0xC8;
    inline constexpr size_t _unityRigidbody  = 0xD0;
    inline constexpr size_t _pedSeat         = 0x118;
    inline constexpr size_t _pedDead         = 0x128;
}

// Verified CarHack tuning limits (CarHack_Tick)
namespace CarTuningLimits {
    inline constexpr float kInternalMax      = 100.0f;   // lround(clamp((Carspeed - 1) * 10, 0, 100))
    inline constexpr float kMaxTorqueBoost   = 20000.0f; // clamp((Carspeed - 1) * 800, 0, 20000)
    inline constexpr float kSpeedRestrictMax = 9999.0f;  // VehicleViewSpeedRestriction
}

// Verified GreenZone system stash offsets (GZ_InstallHooks)
namespace GreenZoneOffsets {
    inline constexpr size_t kSyncSystemStashOffset  = 40;  // WeaponAttackNotAllowedSyncSystem + 40
    inline constexpr size_t kInputFireStashOffset   = 56;  // InputFireHandleSystem + 56
    inline constexpr size_t kMeleeConfirmStashOff   = 208; // MeleeAttackInputConfirmSystem + 208
}

struct TargetConfig {
    // --- Verified Engine / Third-Party Namespaces ---
    std::string nsUnity          = "UnityEngine";
    std::string nsMorpeh         = "Scellecs.Morpeh";
    std::string nsAnimancer      = "Animancer";

    // --- Verified Class / Method Identifiers + Namespaces (OneState 1.4.1 dump) ---
    std::string clsGreenZoneSys  = "WeaponAttackNotAllowedSyncSystem";
    std::string nsGreenZoneSys   = "dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Systems";
    std::string clsInputFireSys  = "InputFireHandleSystem";
    std::string nsInputFireSys   = "dev.Scripts.ECS.Morpeh.Features.InputsFeature.Systems";
    std::string clsMeleeConfirm  = "MeleeAttackInputConfirmSystem";
    std::string nsMeleeConfirm   = "dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Systems";
    std::string clsParkSpawnDg   = "PlayerTrySpawnVehicleParkingDatagram";
    std::string nsParkSpawnDg    = "dev.Scripts.GameScripts.Parking.Datagrams";
    std::string clsVehFactorySys = "VehicleFactorySystem";
    std::string nsVehFactorySys  = "dev.Scripts.ECS.Morpeh.Features.VehiclesFeature.Systems";
    std::string clsNickUtils     = "NicknameUtils";
    std::string nsNickUtils      = "dev.Scripts.GlobalUtils";
    std::string clsVoiceWorker   = "VoiceRecorderWorker";
    std::string nsVoiceWorker    = "WhisperRuntime.Capture";
    std::string clsPedStateMach  = "PedStateMachine";
    std::string nsPedStateMach   = "dev.Scripts.ECS.Morpeh.Features.PedViewFeature.StateMachine";
    std::string clsEnviroMgr     = "EnviroManager";
    std::string clsPedSp         = "PedStreamingProvider";
    std::string clsVehSp         = "VehiclesStreamingProvider";
    std::string nsStreaming      = "dev.Scripts.ECS.Morpeh.Features.StreamingFeature.StreamingProviders";
    std::string clsMetaClient    = "MetaClientWrapper";
    std::string nsMetaClient     = "FlakeFlow";
    std::string clsGestureSvc    = "GestureService";
    std::string nsGestureSvc     = "dev.Scripts.ECS.Morpeh.Features.GestureFeature.Systems";
    std::string clsTempWeapSvc   = "TempWeaponService";
    std::string nsWeaponSys      = "dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Systems";
    std::string clsItemSvc       = "ItemService";
    std::string clsWeaponBeltSvc = "WeaponBeltService";
    std::string nsItemsSvc       = "dev.Scripts.ECS.Morpeh.Features.ItemsFeature.Systems";
    std::string clsGlobalCfg     = "GlobalConfigStorage";
    std::string nsGlobalCfg      = "dev.Scripts.Configs";
    std::string clsAccDatagram   = "PedCustomizationApplyAccessoryDatagram";
    std::string nsAccDatagram    = "dev.Scripts.ECS.Morpeh.Features.PedCustomizationFeature.Datagrams";
    std::string mthRayCastRanged = "RayCastAttackRanged";

    // --- Resolved Namespaces & Service Classes (OneState 1.4.1 dump1.4.1.cpp) ---
    std::string nsEnviro               = "Enviro";
    std::string nsPlayerStorages       = "dev.Scripts.ECS.Morpeh.Features.PlayersFeature.Systems";
    std::string nsPlayersComponents    = "dev.Scripts.ECS.Morpeh.Features.PlayersFeature.Components";
    std::string nsPlayersResComponents = "dev.Scripts.ECS.Morpeh.Features.PlayersResourcesFeature.Components";
    std::string nsUnityViewComponents  = "dev.Scripts.ECS.Morpeh.Features.UnityViewFeature.Components";
    std::string nsElementsComponents   = "dev.Scripts.ECS.Morpeh.Features.ElementsFeature.Components";
    std::string nsPedsComponents       = "dev.Scripts.ECS.Morpeh.Features.PedsFeature.Components";
    std::string nsPedStatesComponents  = "dev.Scripts.ECS.Morpeh.Features.PedsFeature.Components.States";
    std::string nsPedClothesComponents = "dev.Scripts.ECS.Morpeh.Features.PedsFeature.Components.Clothes";
    std::string nsControlsComponents   = "dev.Scripts.ECS.Morpeh.Features.ControlsEntityFeature.Components";
    std::string nsVehicleComponents    = "dev.Scripts.ECS.Morpeh.Features.VehiclesFeature.ComponentsExtended";
    std::string nsVehicleBodyComps     = "dev.Scripts.ECS.Morpeh.Features.VehicleViewFeature.Components.VehicleBody";
    std::string nsVehicleWheelComps    = "dev.Scripts.ECS.Morpeh.Features.VehicleViewFeature.Components.VehicleWheel";
    std::string nsVehicleTuningComps   = "dev.Scripts.ECS.Morpeh.Features.VehicleTuningFeature.Components";
    std::string nsTimeCycle            = "dev.Scripts.ECS.Morpeh.Features.TimeCycleFeature.Components";
    std::string nsWeaponComponents     = "dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Components";
    std::string nsPedWeaponComponents  = "dev.Scripts.ECS.Morpeh.Features.PedWeaponFeature.Components";
    std::string nsPedViewComponents    = "dev.Scripts.ECS.Morpeh.Features.PedViewFeature.Components";
    std::string nsPedViewWeapons       = "dev.Scripts.ECS.Morpeh.Features.PedViewFeature.Components.Weapons";
    std::string nsPedViewVehicles      = "dev.Scripts.ECS.Morpeh.Features.PedViewFeature.Components.Vehicles";
    std::string nsItemComponents       = "dev.Scripts.ECS.Morpeh.Features.ItemsFeature.Components.Item";
    std::string nsSoundComponents      = "dev.Scripts.ECS.Morpeh.Features.SoundFmodFeature.Components";
    std::string nsVoiceComponents      = "dev.Scripts.ECS.Morpeh.Features.VoiceFeature.Components";
    std::string nsZoneComponents       = "dev.Scripts.ECS.Morpeh.Features.ZoneFeature.Components";
    std::string nsInputsComponents     = "dev.Scripts.ECS.Morpeh.Features.InputsFeature.Components";
    std::string nsTempMiniComponents   = "dev.Scripts.ECS.Morpeh.Features.TemporaryMiniWindowFeature.Components";
    std::string nsClanComponents       = "dev.Scripts.ECS.Morpeh.Features.ClansFeature.Components";
    std::string nsClanDiplomacy        = "dev.Scripts.ECS.Morpeh.Features.ClanDiplomacyFeature.Components";
    std::string nsGraffitiComponents   = "dev.Scripts.ECS.Morpeh.Features.ClanGraffitiFeature.Components.Checkpoint";
    std::string nsFreeCamComponents    = "dev.Scripts.ECS.Morpeh.Features.FreeCamFeature.Components";
    std::string nsCheckpointComponents = "dev.Scripts.ECS.Morpeh.Features.CheckpointsFeature.Components";
    std::string nsJobCommonComponents  = "dev.Scripts.ECS.Morpeh.Features.JobFeature.Components.Common";
    std::string nsNicknameComponents   = "dev.Scripts.ECS.Morpeh.Features.NicknameFeature.Components.NicknameValues";
    std::string nsMainCamComponents    = "dev.Scripts.ECS.Morpeh.Features.MainCameraFeature.Components";
    std::string nsSettings             = "dev.Scripts.Settings";
    std::string nsCharacterController  = "Lightbug.CharacterControllerPro.Core";
    std::string nsDimensionComponents  = "dev.Scripts.ECS.Morpeh.Features.DimensionFeature.Components";

    std::string clsRealPlayerStorage   = "PlayerStorage";
    std::string clsRealPedViewStorage  = "PedViewStorage";
    std::string nsRealPedViewStorage   = "dev.Scripts.ECS.Morpeh.Features.PedViewFeature.Systems";
    std::string clsRealMainCamStorage  = "MainCameraStorage";
    std::string nsRealMainCamStorage   = "dev.Scripts.ECS.Morpeh.Features.MainCameraFeature.Systems";
    std::string clsRealFactionUtils    = "PlayerFactionUtils";
    std::string nsRealFactionUtils     = "dev.Scripts.ECS.Morpeh.Features.FactionsFeature.Systems";
    std::string clsRealClanStorage     = "ClanStorage";
    std::string nsRealClanStorage      = "dev.Scripts.ECS.Morpeh.Features.ClansFeature.Systems";
    std::string clsRealLocalInfo       = "LocalPlayerInfoStorage";
    std::string nsRealLocalInfo        = "dev.Scripts.ECS.Morpeh.Features.PlayersFeature";
    std::string clsRealJobSearchSys    = "JobSearchAreaCreateSystem";
    std::string nsRealJobSearchSys     = "dev.Scripts.ECS.Morpeh.Features.JobFeature.Systems.Common.SearchArea";
    std::string clsWeaponStorage       = "WeaponStorage";
    std::string clsGuiHudWeaponCtrls   = "GuiHudWeaponControlsComponent";
    std::string nsGuiHudWeaponCtrls    = "dev.Scripts.GUI.MonoBehaviourComponents.HUD.Controls";
    std::string clsFallDamageSys       = "PedViewKnockoutService"; // Owns ApplyLocalFallDamage(Entity, float)
    std::string nsFallDamageSys        = "dev.Scripts.ECS.Morpeh.Features.PedViewFeature.Systems";
    std::string clsCtrlElemStorage     = "ControlledElementStorageSystem";
    std::string nsCtrlElemStorage      = "dev.Scripts.ECS.Morpeh.Features.ElementsFeature.Systems";
    std::string clsEnviroTimeModule    = "EnviroTimeModule";

    // --- Morpeh Component Names (resolved from dump1.4.1.cpp; Stash fetched via static <Comp>::GetStash(World)) ---
    std::string compPosition           = "Position";
    std::string compRotation           = "Rotation";
    std::string compHealth             = "Health";
    std::string compArmor              = "Armor";
    std::string compPed                = "Ped";
    std::string compVehicle            = "Vehicle";
    std::string compElementId          = "ElementId";
    std::string compElementInvincible  = "ElementInvincible";
    std::string compPedView            = "PedView";
    std::string compPedViewLocal       = "PedViewLocal";
    std::string compPedViewDead        = "PedViewDead";
    std::string compPedViewKnockout    = "PedViewKnockout";
    std::string compPedSeat            = "PedSeat";
    std::string compLogicEntityLink    = "LogicEntityLink";
    std::string compViewEntityLink     = "ViewEntityLink";
    std::string compControlsEntity     = "ControlsEntity";
    std::string compControlledBy       = "ControlledBy";
    std::string compPlayer             = "Player";
    std::string compPlayerId           = "PlayerId";
    std::string compDigitalId          = "DigitalId";
    std::string compLocalPlayer        = "LocalPlayer";
    std::string compNicknameText       = "NicknameText";
    std::string compPedSkin            = "Model";            // ElementModel on Ped entity
    std::string compPedGender          = "PedView";          // PedView.pedGender (0x0) / ControlledElementStorageSystem.GetPedGender
    std::string compPedClothes         = "PedClothesValue";  // dev.Scripts.ECS.Morpeh.Features.PedsFeature.Components.Clothes
    std::string compPedClothesChanged  = "PedClothesChanged";
    std::string compStamina            = "Stamina";
    std::string compStaminaInfinite    = "StaminaInfinite";
    std::string compIgnoreStaminaLoss  = "IgnoreStaminaConsume";
    std::string compPedWeapon          = "PedWeapon";        // PedWeaponFeature.Components
    std::string compPedWeaponAmmo      = "PedWeaponAmmo";    // PedWeaponFeature.Components
    std::string compPedTempWeapon      = "PedTempWeapon";    // PedWeaponFeature.Components
    std::string compWeaponAttackCd     = "WeaponAttackCooldown";
    std::string compWeaponNotAllowed   = "WeaponAttackNotAllowed";
    std::string compWeaponViewParams   = "WeaponViewParameters";
    std::string compWeaponRecoil       = "WeaponViewRangeRecoil";
    std::string compCamWeaponRecoil    = "CameraWeaponRecoil";
    std::string compItemId             = "ItemId";
    std::string compItemName           = "ItemName";
    std::string compItemCount          = "ItemCount";
    std::string compLocalItem          = "LocalItem";
    std::string compVehInternals       = "VehicleViewInternals";
    std::string compVehAddTorque       = "VehicleViewAdditionalTorque";
    std::string compVehSpeedRestrict   = "VehicleViewSpeedRestriction";
    std::string compVehRunOutMult      = "VehicleViewRunOutMultiplier";
    std::string compVehPaint           = "VehicleBodyColor";
    std::string compVehPaintDirty      = "VehicleBodyColorUpdate";
    std::string compVehWindowTint      = "VehicleWindowsColor";
    std::string compVehWindowTintDirty = "VehicleWindowsColorUpdate";
    std::string compVehRimId           = "WheelCustomId";
    std::string compVehRimColor        = "VehicleWheelsColor";
    std::string compVehRimUpdate       = "VehicleWheelsUpdate";
    std::string compVehTuneExternal    = "VehicleTuningExternal";
    std::string compVehTuneExtDirty    = "VehicleTuningExternalUpdate";
    std::string compTimeSetReq         = "TimeCycleSetWorldTimeRequest";
    std::string compTimeFrozenReq      = "TimeCycleSetWorldFrozenRequest";
    std::string compTimeCycleWorld     = "TimeCycleWorld";
    std::string compFreeCamActive      = "FreeCamActive";
    std::string compFreeCamTarget      = "FreeCamTarget";
    std::string compFreeCamActivate    = "FreeCamTargetActivate";
    std::string compFreeCamDeactivate  = "FreeCamDeactivate";
    std::string compCheckpointTag      = "Checkpoint";
    std::string compCheckpointPos      = "Position"; // Checkpoint entities carry UnityViewFeature.Components.Position
    std::string compJobSearchCreate    = "JobSearchAreaCreate";
    std::string compJobCheckpoint      = "JobCheckpoint";
    std::string compCurrentJob         = "CurrentJob";
    std::string compUnityGameObject    = "UnityGameObject";
    std::string compUnityTransform     = "UnityTransform";
    std::string compUnityRigidbody     = "UnityRigidbody";
    std::string compSetPosRot          = "SetPositionAndRotation";
    std::string compSetVelocity        = "SetVelocity";
    std::string compPedInZone          = "PedInZone";
    std::string compPedBlockedInput    = "PedBlockedInput";
    std::string compElementFrozen      = "ElementFrozen";
    std::string compVehWheels          = "VehicleWheels";
    std::string compVehWheelsColorUpd  = "VehicleWheelsColorUpdate";
    std::string compVehViewInputs      = "VehicleViewInputs";
    std::string compInputsData         = "InputsData";
    std::string compTemporaryMini      = "TemporaryMini";
    std::string compAcceptPressed      = "AcceptPressed";
    std::string compVoiceChannel       = "VoiceChannel";
    std::string compVoiceChanChangeReq = "VoiceChannelChangeRequest";
    std::string compVoiceChatType      = "VoiceChannelChatType";
    std::string compPedViewWeaponLink  = "PedViewWeaponLink";
    std::string compPedViewLastVeh     = "PedViewLastVehicle";
    std::string compFactionResource    = "FactionResource";
    std::string compPlayerClan         = "PlayerClan";
    std::string compClanDiplomacy      = "ClanDiplomacyRelations";
    std::string compNicknamePrefix     = "NicknamePrefix";
    std::string compGraffitiCp         = "CheckpointClanGraffiti";
    std::string compGraffitiState      = "CheckpointGraffitiState";
    std::string compCamSettings        = "CameraSettings";
    std::string compDimension          = "Dimension";

    // --- Global LayersMask & Map / NavMarker Components (verified dump1.4.1.cpp) ---
    std::string nsGlobalContexts       = "dev.Scripts.GlobalContexts";
    std::string clsLayersMask          = "LayersMask";
    std::string nsNavMarkersComponents = "dev.Scripts.ECS.Morpeh.Features.NavMarkersFeature.Components";
    std::string nsHudMapComponents     = "dev.Scripts.ECS.Morpeh.Features.HudMapFeature.Components";
    std::string compNavMarkerBlip      = "NavMarkerBlip";
    std::string compNavMarkerWithBlip  = "NavMarkerWithBlip";
    std::string compNavMarkerAttached  = "NavMarkerAttachedToElement";
    std::string compNavMarkerVisib     = "NavMarkerVisibility";
    std::string compElementNavType     = "ElementNavMarkerType";
    std::string compHudMapCachedMarker = "HudMapCachedNavMarker";
    std::string compHudMapCreateCp     = "HudMapCreateCheckpoint";
    std::string compHudMapCheckpoint   = "HudMapCheckpoint"; // User-placed map pin entity ref (dump1.4.1.cpp L542739)
    std::string nsHudMapSystems        = "dev.Scripts.ECS.Morpeh.Features.HudMapFeature.Systems";
    std::string clsHudMapNavService    = "HudMapNavigationService"; // IInjectable owner of NavigationGraph @ 0x50 (L540186)
    std::string clsHudMapNavSystem     = "HudMapNavigationSystem";  // IInjectable owner of _points List<Vector3> @ 0x38 (L540328)
    size_t      offHudMapNavSvcGraph   = 0x50; // HudMapNavigationService.<NavigationGraph>k__BackingField (L540203)
    size_t      offHudMapNavSvcCpStash = 0x80; // HudMapNavigationService._hudMapCheckpoint (L540209)
    size_t      offHudMapNavSysPoints  = 0x38; // HudMapNavigationSystem._points List<Vector3> (L540341)
    // FreeCam systems whose IsEnabled() is patched to return true for Far-Player Spectate + Voice
    std::string nsFreeCamSystems       = "dev.Scripts.ECS.Morpeh.Features.FreeCamFeature.Systems";
    std::string clsFreeCamActivateSys  = "FreeCamTargetActivateSystem";
    std::string clsFreeCamDeactSys     = "FreeCamDeactivateSystem";
    std::string clsFreeCamSeatAddSys   = "FreeCamPedSeatAddedSystem";
    std::string clsFreeCamSeatRemSys   = "FreeCamPedSeatRemovedSystem";

    // Map NavigationGraph (big-map GPS route target = _lastGoal @ 0x50, dump1.4.1.cpp L35138)
    std::string nsMap                  = "dev.Scripts.Map";
    std::string clsNavigationGraph     = "NavigationGraph";
    std::string mthFindPath            = "FindPath";
    size_t      offNavGraphLastGoal    = 0x50;

    // WeaponProduceAttackRangedSystem Wallbang Penetration (dump1.4.1.cpp L99802-99886)
    std::string clsWeapProduceRanged   = "WeaponProduceAttackRangedSystem";
    std::string mthTryGetPenDist       = "TryGetPenetrationDistance";
    std::string mthCanDmgSecondary     = "CanDamageSecondaryEntity";

    // Far-player tracking & FMOD/Whisper Voice + Camera + Ghost Proxy Immunity (verified dump1.4.1.cpp)
    std::string nsPlayerUtils          = "dev.Scripts.ECS.Morpeh.Features.PlayersFeature.Utils";
    std::string clsPlayerDistUtils     = "PlayerDistanceUtils";
    std::string nsFmodUnity            = "FMODUnity";
    std::string clsStudioListener      = "StudioListener";
    std::string clsRuntimeManager      = "RuntimeManager";
    std::string nsWhisperPlayback      = "WhisperRuntime.Playback";
    std::string clsSpatialParameters   = "SpatialParameters";
    std::string nsVoiceSystems         = "dev.Scripts.ECS.Morpeh.Features.VoiceFeature.Systems";
    std::string clsVoicePlaybackSvc    = "VoicePlaybackService";
    std::string nsMainCamSys           = "dev.Scripts.ECS.Morpeh.Features.MainCameraFeature.Systems";
    std::string clsCameraPlacementSys  = "CameraPlacementSystem";
    std::string nsUniversalCamSys      = "dev.Scripts.ECS.Morpeh.Features.MainCameraFeature.Systems.Universal";
    std::string clsUniversalCamApply   = "UniversalCameraApplyToMainSystem";
    std::string nsPedViewSys           = "dev.Scripts.ECS.Morpeh.Features.PedViewFeature.Systems";
    std::string clsPedViewGravitySys   = "PedViewGravitySystem";
    std::string clsPedViewKoWaterSys   = "PedViewKnockoutInWaterSystem";
    std::string nsPedsSys              = "dev.Scripts.ECS.Morpeh.Features.PedsFeature.Systems";
    std::string clsPedPosOffMapSys     = "PedPositionOffMapSystem";
    std::string clsPedKillSys          = "PedKillSystem";
    std::string nsPedsValSys           = "dev.Scripts.ECS.Morpeh.Features.PedsFeature.Systems.PedValues";
    std::string clsPedOxygenSys        = "PedOxygenSystem";
    std::string nsElementsSys          = "dev.Scripts.ECS.Morpeh.Features.ElementsFeature.Systems";
    std::string clsElementStorage      = "ElementStorage";
    std::string clsElemSubmergeSys     = "ElementSubmergenceUpdateSystem";
    std::string nsElementsSyncSys      = "dev.Scripts.ECS.Morpeh.Features.ElementsFeature.Systems.SyncSystems";
    std::string clsElemUnderwaterSys   = "ElementUnderwaterReactiveSystem";
    std::string clsElemPosSyncSys      = "ElementPositionSyncSystem";
    std::string nsWater                = "dev.Scripts.Water";
    std::string clsWaterArea           = "WaterArea";

    // Network & KillAround Datagrams (dump1.4.1.cpp L15164, L47216, L47804, L101718, L101744, L426243..L426271)
    std::string nsFlakeFlow            = "FlakeFlow";
    std::string clsMetaClientWrapper   = "MetaClientWrapper";
    std::string nsIBNetPedSync         = "dev.Scripts.Networking.IBNet.IBNet_Datagrams.PedSync";
    std::string nsIBNetElemSync        = "dev.Scripts.Networking.IBNet.IBNet_Datagrams.ElementSync";
    std::string nsMeleeDatagrams       = "dev.Scripts.ECS.Morpeh.Features.MeleeCombatFeature.Datagrams";
    std::string nsWeaponDatagrams      = "dev.Scripts.ECS.Morpeh.Features.WeaponFeature.Datagrams";

    size_t offStudioListenerAttenObj   = 0x28;  // StudioListener.attenuationObject
    size_t offStudioListenerRb         = 0x40;  // StudioListener.rigidBody
    size_t offSpatialMode              = 0x10;  // SpatialParameters.Mode (FmodMode)
    size_t offSpatialRolloff           = 0x14;  // SpatialParameters.Rolloff
    size_t offSpatialMinDist           = 0x18;  // SpatialParameters.MinDistance
    size_t offSpatialMaxDist           = 0x1C;  // SpatialParameters.MaxDistance
    uint32_t kFmodMode2D               = 281022472U; // FmodMode::Mode2D (0x10C00008, dump L1444969)
    size_t offPhysActorStartPos        = 0x40;  // PhysicsActor.startingPosition (dump1.4.1.cpp L1341694)
    size_t offPhysActorTargetPos       = 0x4C;  // PhysicsActor.targetPosition (dump1.4.1.cpp L1341695)
    size_t offCharActorRemovePen       = 0x190; // CharacterActor._removePenetrationAction
    size_t offCharActorGrounded        = 0x1A0; // CharacterActor.<IsGrounded>k__BackingField
    size_t offCharActorStable          = 0x1A1; // CharacterActor.<IsStable>k__BackingField
    size_t offCharActorInVel           = 0x240; // CharacterActor.<InputVelocity>k__BackingField
    size_t offCharActorPreVel          = 0x24C; // CharacterActor.<PreSimulationVelocity>k__BackingField
    size_t offCharActorPostVel         = 0x258; // CharacterActor.<PostSimulationVelocity>k__BackingField
    size_t offCharActorExtVel          = 0x264; // CharacterActor.<ExternalVelocity>k__BackingField
    size_t offPedViewGravParams        = 0x18;  // PedViewGravitySystem._pedPhysics
    size_t offPedPhysicsGravInAir      = 0x18;  // PedPhysicsParams.gravityInAir
    size_t offElemStorageLastLocalId   = 0x20;  // ElementStorage._lastLocalElementId
    size_t offElemStorageLastLocalGen  = 0x24;  // ElementStorage._lastLocalElementId.Gen
    // dev.Scripts.Map.MapElement / Blip verified field offsets (dump1.4.1.cpp L34456-34510)
    size_t offMapElementPosition       = 0x28; // MapElement._position (Vector3)
    size_t offMapElementHidden         = 0x52; // MapElement.<Hidden>k__BackingField (bool)
    size_t offMapElementLegendTitle    = 0x58; // MapElement.<LegendTitle>k__BackingField (string)
    size_t offBlipSpriteName           = 0x88; // Blip.SpriteName (string)
    size_t offBlipLabel                = 0x90; // Blip.Label (string)

    // --- Managed / Native Struct Field Offsets (from dump1.4.1.cpp; resolved via il2cpp_field_get_offset at runtime too) ---
    size_t offLocalPlayerEntityRaw     = 0x20; // PlayerStorage._localPlayerEntity (0x20)
    size_t offLogicLinkInnerPtr        = 0x18; // Verified in GetLocalPedEntity (+0x18 -> +0x18)
    size_t offCamViewProjMatrix        = 0x00; // W2S uses UnityEngine.Camera.WorldToScreenPoint + worldToCameraMatrix/projectionMatrix
    float  offPedHeadBoneYOffset       = 1.65f; // Fallback head height above root when bone transform is absent
    size_t offPedViewGender            = 0x00; // PedView.pedGender
    size_t offPedViewAnimancer         = 0x08; // PedView.animancer
    size_t offPedViewActor             = 0x10; // PedView.actor (CharacterActor)
    size_t offPedViewRoot              = 0x18; // PedView.root (Transform)
    size_t offPedViewHips              = 0x28; // PedView.hips (Transform)
    size_t offPedViewChest             = 0x30; // PedView.chest (Transform)
    size_t offBoneArray                = 0x40; // PedView.allBones (Transform[])
    size_t offPedViewRightHandIk       = 0x58; // PedView.RightHandIkBones (LimbIkBones: Top/Mid/Bot = 24B)
    size_t offPedViewLeftHandIk        = 0x70; // PedView.LeftHandIkBones
    size_t offPedViewRightLegIk        = 0x88; // PedView.RightLegIkBones
    size_t offPedViewLeftLegIk         = 0xA0; // PedView.LeftLegIkBones
    size_t offPedViewRigidbody         = 0xB8; // PedView.rigidbody (Rigidbody)
    size_t offPedViewHeadBone          = 0xC0; // PedView.headBone (Transform)
    size_t offPhysicsActorAnimator     = 0x90; // PhysicsActor.<Animator>k__BackingField
    size_t offWeapLvlReloadTime        = 0x18; // WeaponLevelParameters.ReloadTime
    size_t offWeapLvlHeadMult          = 0x1C; // WeaponLevelParameters.HeadshotDamageMultiplier
    size_t offWeapLvlMaxAmmo           = 0x20; // WeaponLevelParameters.MaxAmmo
    size_t offWeapLvlAttackSpeed       = 0x24; // WeaponLevelParameters.AttackSpeed
    size_t offWeapLvlDistance          = 0x30; // WeaponLevelParameters.Distance
    size_t offCamSettingsHFov          = 0x60; // CameraSettings.HorizontalFieldOfView
    size_t offEnviroIsNight            = 0xF0; // EnviroManager.isNight (0xF0)
    size_t offEnviroSolarTime          = 0xF4; // EnviroManager.solarTime (0xF4)
    size_t offEnviroLunarTime          = 0xF8; // EnviroManager.lunarTime (0xF8)
    size_t offDamageTextPrimaryColor   = 0x38; // Verified in DmgNum427_Restore (+56, 16B RGBA)
    size_t offDamageTextOutlineColor   = 0x48; // Verified in DmgNum427_Restore (+72, 16B RGBA)

    // --- Optional CDN Thumbnail Base URLs (encrypted in Auth::SdStr; UI falls back to clean card badges when empty) ---
    std::string cdnCarThumbBase        = ""; // Optional: CDN base URL for car thumbs
    std::string cdnRimThumbBase        = ""; // Optional: CDN base URL for rim thumbs
    std::string cdnWeapThumbBase       = ""; // Optional: CDN base URL for weapon thumbs
};

inline TargetConfig g_cfg{};

} // namespace Offsets
