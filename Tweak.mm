// Tweak.mm — Standalone Metal/ImGui Pro Overlay for One State RP 1.4.1
// Target: com.Chillgaming.oneState2 / com.Chillgaming.oneState / com.Chillgaming.OneState
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

#include "imgui.h"
#include "imgui_impl_metal.h"
#include "Offsets.h"
#include "Catalogs.h"
#include "Lang.h"
#include "Theme.h"
#include "AuthSystem.h"
#include "CoreEngine.h"
#include "FeatureModules.h"

@class ImGuiTouchSinkView;

@interface OneStateGlassOverlay : NSObject <MTKViewDelegate>
@property (nonatomic, strong) UIWindow* overlayWindow;
@property (nonatomic, strong) MTKView* mtkView;
@property (nonatomic, strong) ImGuiTouchSinkView* touchSink;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) UIButton* togglePill;
@property (nonatomic, strong) UITextField* streamerHost;
@property (nonatomic, assign) BOOL menuOpen;
@property (nonatomic, assign) int activeTab;      // 0=Combat, 1=Visuals(default), 2=Player/Veh, 3=Wardrobe/Auto, 4=Config
@property (nonatomic, assign) int wardrobeSubTab;
@property (nonatomic, assign) int previewPose;
- (void)presentWhenReady;
- (void)applyStreamerMode:(BOOL)enabled;
- (void)promptNativeInputWithTitle:(NSString*)title placeholder:(NSString*)placeholder initial:(NSString*)initial numeric:(BOOL)numeric onSubmit:(void (^)(NSString* text))onSubmit;
@end

static OneStateGlassOverlay* g_overlay = nil;

@interface PassthroughOverlayWindow : UIWindow
@end

@implementation PassthroughOverlayWindow
- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
    if (g_overlay && g_overlay.togglePill && CGRectContainsPoint(CGRectInset(g_overlay.togglePill.frame, -8, -8), point)) {
        return g_overlay.togglePill;
    }
    if (!g_overlay || !g_overlay.menuOpen) return nil;
    UIView* hit = [super hitTest:point withEvent:event];
    return (hit == self) ? nil : hit;
}
@end

@interface ImGuiTouchSinkView : UIView
- (void)flushToImGui;
@end

struct ImGuiQueuedTouchEvt {
    int   type; // 0 = Pos, 1 = Down, 2 = Up
    float x;
    float y;
};

@implementation ImGuiTouchSinkView {
    ImGuiQueuedTouchEvt _queue[128];
    int     _queueCount;
    CGPoint _lastPos;
    __weak UITouch* _activeTouch;
    int     _clearPosFrames;
}
- (void)pushEvt:(int)type x:(float)x y:(float)y {
    if (_queueCount < 128) {
        _queue[_queueCount++] = { type, x, y };
    }
}
- (void)flushToImGui {
    ImGuiIO& io = ImGui::GetIO();
    if (_queueCount > 0) {
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        for (int i = 0; i < _queueCount; ++i) {
            const ImGuiQueuedTouchEvt& ev = _queue[i];
            if (ev.type == 0) {
                io.AddMousePosEvent(ev.x, ev.y);
            } else if (ev.type == 1) {
                io.AddMouseButtonEvent(0, true);
            } else if (ev.type == 2) {
                io.AddMouseButtonEvent(0, false);
            }
        }
        _queueCount = 0;
    } else if (_clearPosFrames > 0) {
        --_clearPosFrames;
        if (_clearPosFrames == 0 && _activeTouch == nil) {
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        }
    }
}
- (void)touchesBegan:(NSSet<UITouch*>*)t withEvent:(UIEvent*)e {
    UITouch* touch = t.anyObject;
    if (!touch) return;
    _activeTouch = touch;
    _clearPosFrames = 0;
    CGPoint p = [touch locationInView:self];
    _lastPos = p;
    [self pushEvt:0 x:(float)p.x y:(float)p.y];
    [self pushEvt:1 x:(float)p.x y:(float)p.y];
}
- (void)touchesMoved:(NSSet<UITouch*>*)t withEvent:(UIEvent*)e {
    UITouch* touch = (_activeTouch && [t containsObject:_activeTouch]) ? _activeTouch : t.anyObject;
    if (!touch) return;
    CGPoint p = [touch locationInView:self];
    if (std::fabs((double)(p.x - _lastPos.x)) > 0.2 || std::fabs((double)(p.y - _lastPos.y)) > 0.2) {
        _lastPos = p;
        [self pushEvt:0 x:(float)p.x y:(float)p.y];
    }
}
- (void)touchesEnded:(NSSet<UITouch*>*)t withEvent:(UIEvent*)e {
    UITouch* touch = (_activeTouch && [t containsObject:_activeTouch]) ? _activeTouch : t.anyObject;
    CGPoint p = touch ? [touch locationInView:self] : _lastPos;
    [self pushEvt:0 x:(float)p.x y:(float)p.y];
    [self pushEvt:2 x:(float)p.x y:(float)p.y];
    _activeTouch = nil;
    _clearPosFrames = 2;
}
- (void)touchesCancelled:(NSSet<UITouch*>*)t withEvent:(UIEvent*)e {
    [self pushEvt:2 x:(float)_lastPos.x y:(float)_lastPos.y];
    _activeTouch = nil;
    _clearPosFrames = 2;
}
@end

@implementation OneStateGlassOverlay {
    char  _searchBuf[64];
    char  _playerSearchBuf[64];
    char  _freeCamSearchBuf[64];
    int   _subTabs[5];
    int   _lastScrollTab;
    int   _lastScrollSub;
    float _pendingScrollY;
    BOOL  _overlayWasDrawn;
}

- (instancetype)init {
    if ((self = [super init])) {
        _menuOpen = NO;
        _activeTab = 1; // Default to Visuals -> Player + Interactive Preview (matches reference)
        for (int i = 0; i < 5; ++i) _subTabs[i] = 0;
        _lastScrollTab = -1;
        _lastScrollSub = -1;
        _pendingScrollY = -1.0f;
        _wardrobeSubTab = 0;
        _previewPose = 0;
        _searchBuf[0] = '\0';
        _playerSearchBuf[0] = '\0';
        _freeCamSearchBuf[0] = '\0';
        _overlayWasDrawn = YES;
    }
    return self;
}

- (UIWindowScene*)findActiveScene {
    for (UIScene* s in UIApplication.sharedApplication.connectedScenes) {
        if ([s isKindOfClass:[UIWindowScene class]] && s.activationState == UISceneActivationStateForegroundActive) {
            return (UIWindowScene*)s;
        }
    }
    for (UIScene* s in UIApplication.sharedApplication.connectedScenes) {
        if ([s isKindOfClass:[UIWindowScene class]]) return (UIWindowScene*)s;
    }
    return nil;
}

- (void)presentWhenReady {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.overlayWindow) return;
        UIWindowScene* scene = [self findActiveScene];
        if (!scene) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                [self presentWhenReady];
            });
            return;
        }

        CGRect bounds = scene.coordinateSpace.bounds;
        self.overlayWindow = [[PassthroughOverlayWindow alloc] initWithWindowScene:scene];
        self.overlayWindow.frame = bounds;
        self.overlayWindow.windowLevel = UIWindowLevelAlert + 50.0f;
        self.overlayWindow.backgroundColor = UIColor.clearColor;
        self.overlayWindow.rootViewController = [UIViewController new];
        self.overlayWindow.rootViewController.view.backgroundColor = UIColor.clearColor;

        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        self.commandQueue = [dev newCommandQueue];
        self.mtkView = [[MTKView alloc] initWithFrame:bounds device:dev];
        self.mtkView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        self.mtkView.delegate = self;
        self.mtkView.clearColor = MTLClearColorMake(0, 0, 0, 0);
        self.mtkView.backgroundColor = UIColor.clearColor;
        self.mtkView.opaque = NO;
        self.mtkView.userInteractionEnabled = NO;
        [self.overlayWindow.rootViewController.view addSubview:self.mtkView];

        self.touchSink = [[ImGuiTouchSinkView alloc] initWithFrame:bounds];
        self.touchSink.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        self.touchSink.backgroundColor = UIColor.clearColor;
        [self.overlayWindow.rootViewController.view addSubview:self.touchSink];

        // Flagship iOS 18 Frosted Glass Floating Capsule Pill (no fake flight joystick!)
        self.togglePill = [UIButton buttonWithType:UIButtonTypeCustom];
        self.togglePill.frame = CGRectMake(20, 86, 58, 38);
        self.togglePill.layer.cornerRadius = 19.0f;
        self.togglePill.layer.borderWidth = 1.25f;
        self.togglePill.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:0.24].CGColor;
        self.togglePill.backgroundColor = [UIColor colorWithRed:0.08 green:0.09 blue:0.13 alpha:0.90];
        self.togglePill.layer.shadowColor = [UIColor blackColor].CGColor;
        self.togglePill.layer.shadowOffset = CGSizeMake(0, 4);
        self.togglePill.layer.shadowRadius = 10.0f;
        self.togglePill.layer.shadowOpacity = 0.55f;
        [self.togglePill setTitle:@(ThemeEngine::kBrandPill) forState:UIControlStateNormal];
        [self.togglePill setTitleColor:[UIColor colorWithRed:0.96 green:0.97 blue:1.0 alpha:1.0] forState:UIControlStateNormal];
        self.togglePill.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightBold];
        [self.togglePill addTarget:self action:@selector(toggleMenu) forControlEvents:UIControlEventTouchUpInside];
        [self.togglePill addGestureRecognizer:[[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(handlePillPan:)]];
        [self.overlayWindow.rootViewController.view addSubview:self.togglePill];

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // Optimized Crisp Retina Font Atlas + iOS Arabic Font Merge (capped at 2x / 1x oversample for fast Metal GPU perf)
        {
            ImGuiIO& io = ImGui::GetIO();
            float sc = std::clamp(static_cast<float>(UIScreen.mainScreen.scale), 1.0f, 2.0f);
            float fontPx = 14.5f * sc;
            NSFileManager* fm = [NSFileManager defaultManager];

            const char* latinPaths[] = {
                "/System/Library/Fonts/CoreUI/SFUI.ttf",
                "/System/Library/Fonts/Core/SFUI.ttf",
                "/System/Library/Fonts/Core/HelveticaNeue.ttc",
                "/System/Library/Fonts/CoreAddition/Arial.ttf",
                "/System/Library/Fonts/Core/Arial.ttf"
            };
            const char* foundLatin = nullptr;
            for (const char* lp : latinPaths) {
                if ([fm fileExistsAtPath:@(lp)]) { foundLatin = lp; break; }
            }

            ImFontConfig baseCfg;
            baseCfg.OversampleH = 1;
            baseCfg.OversampleV = 1;
            baseCfg.PixelSnapH  = true;
            if (foundLatin) {
                io.Fonts->AddFontFromFileTTF(foundLatin, fontPx, &baseCfg, io.Fonts->GetGlyphRangesDefault());
            } else {
                baseCfg.SizePixels = fontPx;
                io.Fonts->AddFontDefault(&baseCfg);
            }

            const char* arabicPaths[] = {
                "/System/Library/Fonts/Core/SFArabic.ttf",
                "/System/Library/Fonts/LanguageSupport/GeezaPro.ttc",
                "/System/Library/Fonts/Core/GeezaPro.ttc",
                "/System/Library/Fonts/CoreAddition/ArialHB.ttc",
                "/System/Library/Fonts/Core/ArialHB.ttc"
            };
            const char* foundArabic = nullptr;
            for (const char* ap : arabicPaths) {
                if ([fm fileExistsAtPath:@(ap)]) { foundArabic = ap; break; }
            }
            if (foundArabic) {
                static const ImWchar kArabicRanges[] = {
                    0x0600, 0x06FF, // Basic Arabic
                    0x0750, 0x077F, // Arabic Supplement
                    0xFB50, 0xFDFF, // Arabic Presentation Forms-A
                    0xFE70, 0xFEFF, // Arabic Presentation Forms-B (Shaped glyphs & Lam-Alef)
                    0
                };
                ImFontConfig arCfg;
                arCfg.MergeMode   = true;
                arCfg.OversampleH = 1;
                arCfg.OversampleV = 1;
                arCfg.PixelSnapH  = true;
                io.Fonts->AddFontFromFileTTF(foundArabic, fontPx * 1.06f, &arCfg, kArabicRanges);
            }
            io.FontGlobalScale = 1.0f / sc;
        }

        Lang::SetLanguage(g_cfgState.languageMode);
        ImGui_ImplMetal_Init(dev);
        ThemeEngine::ApplyProStyle();

        self.overlayWindow.hidden = NO;
        [self.overlayWindow makeKeyAndVisible];
        if (g_cfgState.bStreamerMode) [self applyStreamerMode:YES];

        // Boot encrypted Keychain auto-login online; if no saved key exists, open Pro Login Portal smoothly
        AuthSystem::BootAutoLoginOnce();
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.6 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (!AuthSystem::IsVerifiedFast() && !self.menuOpen) {
                self.menuOpen = YES;
            }
        });
    });
}

- (void)promptNativeInputWithTitle:(NSString*)title
                       placeholder:(NSString*)placeholder
                           initial:(NSString*)initial
                           numeric:(BOOL)numeric
                          onSubmit:(void (^)(NSString* text))onSubmit {
    UIViewController* hostVC = self.overlayWindow.rootViewController;
    if (!hostVC) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (hostVC.presentedViewController) return;
        bool ar = Lang::IsArabic();
        NSString* okTxt  = ar ? @"تطبيق" : @"Apply";
        NSString* clrTxt = ar ? @"مسح"   : @"Clear";
        NSString* clTxt  = ar ? @"إلغاء" : @"Cancel";

        UIAlertController* ac = [UIAlertController alertControllerWithTitle:title
                                                                    message:nil
                                                             preferredStyle:UIAlertControllerStyleAlert];
        if (@available(iOS 13.0, *)) {
            ac.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
        }
        [ac addTextFieldWithConfigurationHandler:^(UITextField* tf) {
            tf.placeholder = placeholder ?: @"";
            tf.text = initial ?: @"";
            tf.autocapitalizationType = UITextAutocapitalizationTypeNone;
            tf.autocorrectionType = UITextAutocorrectionTypeNo;
            tf.spellCheckingType = UITextSpellCheckingTypeNo;
            tf.clearButtonMode = UITextFieldViewModeWhileEditing;
            tf.keyboardType = numeric ? UIKeyboardTypeNumbersAndPunctuation : UIKeyboardTypeDefault;
            tf.returnKeyType = UIReturnKeyDone;
            tf.font = [UIFont systemFontOfSize:15.0 weight:UIFontWeightMedium];
        }];
        [ac addAction:[UIAlertAction actionWithTitle:clTxt style:UIAlertActionStyleCancel handler:nil]];
        if (!numeric) {
            [ac addAction:[UIAlertAction actionWithTitle:clrTxt style:UIAlertActionStyleDestructive handler:^(UIAlertAction* a) {
                if (onSubmit) onSubmit(@"");
            }]];
        }
        [ac addAction:[UIAlertAction actionWithTitle:okTxt style:UIAlertActionStyleDefault handler:^(UIAlertAction* a) {
            NSString* txt = ac.textFields.firstObject.text ?: @"";
            if (onSubmit) onSubmit(txt);
        }]];
        [hostVC presentViewController:ac animated:YES completion:nil];
    });
}

- (void)toggleMenu {
    self.menuOpen = !self.menuOpen;
}

- (void)handlePillPan:(UIPanGestureRecognizer*)pan {
    CGPoint tr = [pan translationInView:self.overlayWindow];
    CGPoint c = self.togglePill.center;
    CGRect b = self.overlayWindow.bounds;
    float s = ThemeEngine::g_state.btnScale;
    self.togglePill.transform = CGAffineTransformMakeScale(s, s);
    self.togglePill.center = CGPointMake(std::clamp((float)(c.x + tr.x), 28.0f, (float)b.size.width - 28.0f),
                                         std::clamp((float)(c.y + tr.y), 28.0f, (float)b.size.height - 28.0f));
    [pan setTranslation:CGPointZero inView:self.overlayWindow];
}

- (void)applyStreamerMode:(BOOL)enabled {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!self.overlayWindow) return;
        if (enabled && !self.streamerHost) {
            self.streamerHost = [[UITextField alloc] initWithFrame:self.overlayWindow.bounds];
            self.streamerHost.secureTextEntry = YES;
            self.streamerHost.userInteractionEnabled = NO;
            UIView* secContainer = self.streamerHost.subviews.firstObject;
            if (secContainer) {
                [self.mtkView removeFromSuperview];
                [secContainer addSubview:self.mtkView];
                [self.overlayWindow.rootViewController.view insertSubview:secContainer atIndex:0];
            }
        } else if (!enabled && self.streamerHost) {
            [self.mtkView removeFromSuperview];
            [self.overlayWindow.rootViewController.view insertSubview:self.mtkView atIndex:0];
            [self.streamerHost.subviews.firstObject removeFromSuperview];
            self.streamerHost = nil;
        }
    });
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {}

- (void)drawInMTKView:(MTKView*)view {
    AuthSystem::TickIntegrityAndHeartbeat();
    BOOL isAuthed = AuthSystem::IsVerifiedFast() ? YES : NO;

    // Reflect license state on floating iOS pill rim (iOS Blue = Unlocked, Apple Red = Locked)
    static int s_lastPillAuthState = -1;
    int curPillState = isAuthed ? 1 : 0;
    if (curPillState != s_lastPillAuthState && self.togglePill) {
        s_lastPillAuthState = curPillState;
        self.togglePill.layer.borderColor = isAuthed
            ? [UIColor colorWithRed:0.04 green:0.52 blue:1.0 alpha:0.75].CGColor
            : [UIColor colorWithRed:1.0 green:0.27 blue:0.23 alpha:0.85].CGColor;
    }

    int safeTab = std::clamp(self.activeTab, 0, 4);
    g_menuIsOpenAtomic.store((isAuthed && self.menuOpen) ? true : false, std::memory_order_relaxed);
    g_activeTabAtomic.store(safeTab, std::memory_order_relaxed);
    g_activeSubTabAtomic.store(_subTabs[safeTab], std::memory_order_relaxed);

    BOOL needOverlayDraw = self.menuOpen ||
        (isAuthed && !g_cfgState.bMasterDisable &&
         (g_cfgState.bEnableESP || g_cfgState.bMiniMapEsp || g_cfgState.bSprayESP ||
          g_cfgState.bCrosshairEnable || g_cfgState.bDmgNumEnable || g_cfgState.bShotPartsEnable ||
          g_cfgState.bEnableAimbot || g_cfgState.bSilentAim));

    BOOL anyBgFeature = isAuthed && !g_cfgState.bMasterDisable &&
        (g_cfgState.bFlyHack || g_cfgState.bCarFly || g_cfgState.bFreeCam || g_cfgState.bSpectateActive ||
         g_cfgState.bCarHack || g_cfgState.bAutoRepair || g_cfgState.bAutoMapMarkerTp || Features::g_mapTpLanding.active ||
         g_cfgState.bSilentWallbang || g_cfgState.bKillAroundAura || g_cfgState.bKillAroundAutoRepeat ||
         KillAround::g_kaKillAllBurstUntilMs.load(std::memory_order_relaxed) > 0 || KillAround::g_kaHasActiveGhost ||
         g_cfgState.bSuperJump || g_cfgState.bSuperPunch || g_cfgState.bSpeedHack || g_cfgState.bInfStamina ||
         g_cfgState.bAutoHeal || g_cfgState.bAutoVest || g_cfgState.bZoneGuard || g_cfgState.bGreenZoneBypass ||
         g_cfgState.bInstantReload || g_cfgState.bNoRecoil || g_cfgState.bRapidFire || g_cfgState.bScaleEnable ||
         g_cfgState.bAntiTp || g_cfgState.bAntiTpCar || g_cfgState.bAntiAfk || g_cfgState.bMoneyGlitch ||
         g_cfgState.bStealEvent || g_cfgState.bJobTp || g_cfgState.bWalkCp || g_cfgState.bVehAiDrive ||
         g_cfgState.bGemsFarm || g_cfgState.bSprayAutoTp || g_cfgState.bFollowTarget || g_cfgState.bKillFly ||
         g_cfgState.bCarKill || g_cfgState.bAutoDanse || g_cfgState.bDayNightEnable || g_cfgState.bFovEnable ||
         g_cfgState.bResolutionEnable || g_cfgState.bRemoveGameCrosshair || g_cfgState.bRadioEnable);

    if (isAuthed) {
        if (needOverlayDraw || anyBgFeature) {
            Features::TickRuntime();
        } else {
            // Delay first background IL2CPP tick slightly after overlay launch so the floating pill & menu ALWAYS appear first
            static double s_firstDrawAt = 0.0;
            static double s_lastIdleTick = 0.0;
            double now = CACurrentMediaTime();
            if (s_firstDrawAt <= 0.0) s_firstDrawAt = now;
            if ((now - s_firstDrawAt) >= 2.0 && (now - s_lastIdleTick) >= 0.12) {
                s_lastIdleTick = now;
                Features::TickRuntime();
            }
        }
    }

    // Zero-Idle-Lag Metal Gate: skip GPU command buffer when menu is closed & no HUD visuals are active
    if (!needOverlayDraw) {
        if (_overlayWasDrawn) {
            MTLRenderPassDescriptor* rpd = view.currentRenderPassDescriptor;
            id<CAMetalDrawable> drawable = view.currentDrawable;
            if (rpd && drawable) {
                id<MTLCommandBuffer> cb = [self.commandQueue commandBuffer];
                id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rpd];
                [enc endEncoding];
                [cb presentDrawable:drawable];
                [cb commit];
                _overlayWasDrawn = NO;
            }
        }
        return;
    }
    _overlayWasDrawn = YES;

    MTLRenderPassDescriptor* rpd = view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!rpd || !drawable) return;

    ImGuiIO& io = ImGui::GetIO();
    CGSize pts = view.bounds.size;
    CGFloat sc = view.contentScaleFactor;
    io.DisplaySize = ImVec2((float)pts.width, (float)pts.height);
    io.DisplayFramebufferScale = ImVec2((float)sc, (float)sc);
    if (self.touchSink) [self.touchSink flushToImGui];

    ImGui_ImplMetal_NewFrame(rpd);
    ImGui::NewFrame();
    ThemeEngine::g_sliderDragging = false;

    if (isAuthed && AuthSystem::IsVerifiedFast()) {
        Features::DrawOverlayVisuals(ImGui::GetBackgroundDrawList(), (float)pts.width, (float)pts.height);
        if (self.menuOpen) [self renderProDeckWithWidth:(float)pts.width height:(float)pts.height];
    } else if (self.menuOpen) {
        if (AuthSystem::DrawLoginDeck((float)pts.width, (float)pts.height,
                                      g_cfgState.languageMode, self.overlayWindow.rootViewController)) {
            self.menuOpen = NO;
        }
    }

    ImGui::Render();
    id<MTLCommandBuffer> cb = [self.commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rpd];
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), cb, enc);
    [enc endEncoding];
    [cb presentDrawable:drawable];
    [cb commit];
}

- (void)renderCatalogListIdName:(const Catalogs::IdNameEntry*)items count:(int)count tag:(const char*)tag onSelect:(void (^)(const Catalogs::IdNameEntry&))onSelect {
    int srchAct = ThemeEngine::SearchPillButton(tag, Lang::T("Search catalog...", "ابحث في القائمة..."), _searchBuf);
    if (srchAct == 1) {
        bool ar = Lang::IsArabic();
        [self promptNativeInputWithTitle:(ar ? @"بحث في القائمة" : @"Search Catalog")
                             placeholder:(ar ? @"اكتب الاسم للبحث..." : @"Type name to search...")
                                 initial:[NSString stringWithUTF8String:_searchBuf]
                                 numeric:NO
                                onSubmit:^(NSString* text) {
            const char* utf = text ? text.UTF8String : "";
            std::snprintf(_searchBuf, sizeof(_searchBuf), "%s", utf ? utf : "");
        }];
    } else if (srchAct == 2) {
        _searchBuf[0] = '\0';
    }
    ImGui::BeginChild(tag, ImVec2(0, 185), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (int i = 0; i < count; ++i) {
        if (!ThemeEngine::CaseInsensitiveContains(items[i].name, _searchBuf)) continue;
        char row[128];
        snprintf(row, sizeof(row), "%s  [#%lld]", items[i].name, (long long)items[i].id);
        if (ImGui::Selectable(row)) onSelect(items[i]);
    }
    ImGui::EndChild();
}

- (void)renderCatalogListSlot:(const Catalogs::IdNameSlotEntry*)items count:(int)count tag:(const char*)tag onSelect:(void (^)(const Catalogs::IdNameSlotEntry&))onSelect {
    int srchAct = ThemeEngine::SearchPillButton(tag, Lang::T("Search catalog...", "ابحث في القائمة..."), _searchBuf);
    if (srchAct == 1) {
        bool ar = Lang::IsArabic();
        [self promptNativeInputWithTitle:(ar ? @"بحث في القائمة" : @"Search Catalog")
                             placeholder:(ar ? @"اكتب الاسم للبحث..." : @"Type name to search...")
                                 initial:[NSString stringWithUTF8String:_searchBuf]
                                 numeric:NO
                                onSubmit:^(NSString* text) {
            const char* utf = text ? text.UTF8String : "";
            std::snprintf(_searchBuf, sizeof(_searchBuf), "%s", utf ? utf : "");
        }];
    } else if (srchAct == 2) {
        _searchBuf[0] = '\0';
    }
    ImGui::BeginChild(tag, ImVec2(0, 185), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (int i = 0; i < count; ++i) {
        if (!ThemeEngine::CaseInsensitiveContains(items[i].name, _searchBuf)) continue;
        char row[128];
        snprintf(row, sizeof(row), "%s  [#%lld | S:%lld]", items[i].name, (long long)items[i].id, (long long)items[i].slotOrGender);
        if (ImGui::Selectable(row)) onSelect(items[i]);
    }
    ImGui::EndChild();
}

// Authentic Apple iOS Segmented Control Pill Tray
- (void)drawSubTabBar:(const char* const*)labels count:(int)count active:(int&)activeRef barWidth:(float)barW {
    if (activeRef >= count) activeRef = 0;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float barH = 36.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + barW, p.y + barH), ThemeEngine::GetSubBar(0.98f), 11.0f);
    dl->AddRect(p, ImVec2(p.x + barW, p.y + barH), IM_COL32(255, 255, 255, 18), 11.0f, 0, 1.0f);

    float itemW = barW / static_cast<float>(std::max(1, count));
    for (int i = 0; i < count; ++i) {
        ImVec2 ip(p.x + i * itemW, p.y);
        ImGui::SetCursorScreenPos(ip);
        char id[24];
        snprintf(id, sizeof(id), "##sub_%d", i);
        if (ImGui::InvisibleButton(id, ImVec2(itemW, barH))) activeRef = i;
        bool isSel = (activeRef == i);
        if (isSel) {
            ImVec2 segA(ip.x + 3.0f, ip.y + 3.0f);
            ImVec2 segB(ip.x + itemW - 3.0f, ip.y + barH - 3.0f);
            dl->AddRectFilled(segA, segB, ThemeEngine::GetAccent(0.92f), 8.5f);
            dl->AddRect(segA, segB, IM_COL32(255, 255, 255, 45), 8.5f, 0, 1.0f);
        }
        ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        ImU32 tc = isSel ? IM_COL32(255, 255, 255, 255) : ThemeEngine::GetMutedText();
        dl->AddText(ImVec2(ip.x + (itemW - ts.x) * 0.5f, ip.y + (barH - ts.y) * 0.5f), tc, labels[i]);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + barH + 12.0f));
}

- (void)renderProDeckWithWidth:(float)w height:(float)h {
    if (!AuthSystem::IsVerifiedFast()) return;
    using Lang::T;
    using Lang::TCombo;
    Lang::SetLanguage(g_cfgState.languageMode);
    ThemeEngine::ApplyProStyle();

    float winW = std::min(w * 0.94f, 730.0f) * ThemeEngine::g_state.uiScale;
    float winH = std::min(h * 0.89f, 470.0f) * ThemeEngine::g_state.uiScale;
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2((w - winW) * 0.5f, (h - winH) * 0.5f), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags wf = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove;
    if (!ImGui::Begin("##ProDeck", nullptr, wf)) { ImGui::End(); return; }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    float railW = 68.0f;

    // 1. Flagship iOS 18 Frosted Glass Backdrop
    ThemeEngine::DrawNeon3DBackdrop(dl, wp, winW, winH, railW, CACurrentMediaTime(), g_cfgState.bNeon3DGrid);

    // 2. Clean iOS Left Sidebar + Subtle Hairline Divider
    dl->AddRectFilled(wp, ImVec2(wp.x + railW, wp.y + winH), ThemeEngine::GetSidebar(), 18.0f, ImDrawFlags_RoundCornersLeft);
    dl->AddLine(ImVec2(wp.x + railW, wp.y + 10.0f), ImVec2(wp.x + railW, wp.y + winH - 10.0f), IM_COL32(255, 255, 255, 18), 1.0f);

    // 3. Left 5-Icon iOS Navigation Rail
    float iconH = (winH - 16.0f) / 5.0f;
    for (int i = 0; i < 5; ++i) {
        ImGui::SetCursorScreenPos(ImVec2(wp.x, wp.y + 8.0f + i * iconH));
        char rid[24];
        snprintf(rid, sizeof(rid), "##rail_%d", i);
        if (ThemeEngine::RailIconButton(rid, i, self.activeTab == i, railW, iconH)) {
            self.activeTab = i;
        }
    }

    // 4. Right Content Area + Clean iOS Header Controls (EN/عربي + Close X)
    float contentX = wp.x + railW + 18.0f;
    float contentW = winW - railW - 36.0f;

    ImGui::SetCursorScreenPos(ImVec2(wp.x + winW - 96.0f, wp.y + 16.0f));
    static const std::string s_arPill = Lang::ShapeArabicForImGui("عربي##langsw");
    const char* langPill = Lang::IsArabic() ? "EN##langsw" : s_arPill.c_str();
    if (ThemeEngine::SecondaryPillButton(langPill, 52.0f, 30.0f, false)) {
        g_cfgState.languageMode = (g_cfgState.languageMode == 0) ? 1 : 0;
        Lang::SetLanguage(g_cfgState.languageMode);
    }

    ImGui::SetCursorScreenPos(ImVec2(wp.x + winW - 38.0f, wp.y + 16.0f));
    if (ThemeEngine::SecondaryPillButton("X##cls", 28.0f, 30.0f, true)) self.menuOpen = NO;

    // Segmented Sub-tab bar
    ImGui::SetCursorScreenPos(ImVec2(contentX, wp.y + 13.0f));
    float barW = std::min(contentW - 96.0f, 500.0f);
    int& curSub = _subTabs[self.activeTab];

    if (self.activeTab == 0) {
        const char* subs[] = {
            T("Aimbot", "الأيم بوت"),
            T("Weapons", "الأسلحة"),
            T("Crosshair & FX", "المؤثرات"),
            T("Kill Around", "القتل المحيط")
        };
        [self drawSubTabBar:subs count:4 active:curSub barWidth:barW];
    } else if (self.activeTab == 1) {
        const char* subs[] = {
            T("Player", "اللاعبين"),
            T("FreeCam", "فري كام"),
            T("Spray", "البخاخات"),
            T("World", "العالم"),
            T("Roles", "الرتب")
        };
        [self drawSubTabBar:subs count:5 active:curSub barWidth:barW];
    } else if (self.activeTab == 2) {
        const char* subs[] = {
            T("Movement", "الحركة"),
            T("Map TP", "الخريطة"),
            T("Player", "اللاعب"),
            T("Vehicle", "السيارة"),
            T("Garage", "الكراج")
        };
        [self drawSubTabBar:subs count:5 active:curSub barWidth:barW];
    } else if (self.activeTab == 3) {
        const char* subs[] = {
            T("Wardrobe", "الملابس"),
            T("Automation", "الأتمتة"),
            T("Live Players", "المتصلين")
        };
        [self drawSubTabBar:subs count:3 active:curSub barWidth:barW];
    } else {
        const char* subs[] = {
            T("Profiles", "الحفظ"),
            T("Theme & Lang", "التصميم واللغة")
        };
        [self drawSubTabBar:subs count:2 active:curSub barWidth:barW];
    }

    // Main Scrollable Body below Sub-Tab Bar (driven exclusively by the right-side Up/Down Scrollbar Rail!)
    float bodyH = winH - 74.0f;
    const float scrollRailW = 34.0f;
    const float scrollRailGap = 8.0f;
    const bool isEspPreviewSub = (self.activeTab == 1 && curSub == 0);
    const float espPreviewW = isEspPreviewSub ? std::min(210.0f, (contentW - scrollRailW - scrollRailGap) * 0.40f) : 0.0f;
    const float bodyW = contentW - scrollRailW - scrollRailGap - (isEspPreviewSub ? (espPreviewW + 12.0f) : 0.0f);

    if (_lastScrollTab != self.activeTab || _lastScrollSub != curSub) {
        _lastScrollTab = self.activeTab;
        _lastScrollSub = curSub;
        _pendingScrollY = 0.0f;
    }

    ImGui::SetCursorScreenPos(ImVec2(contentX, wp.y + 62.0f));
    ImGui::BeginChild("##MainBody", ImVec2(bodyW, bodyH), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (_pendingScrollY >= 0.0f) {
        ImGui::SetScrollY(_pendingScrollY);
        _pendingScrollY = -1.0f;
    }

    // ========================================================================
    // TAB 0: COMBAT (Crosshair Icon)
    // ========================================================================
    if (self.activeTab == 0) {
        if (curSub == 0) {
            ThemeEngine::ProToggle(T("Enable Aimbot", "تفعيل الأيم بوت"), &g_cfgState.bEnableAimbot);
            ThemeEngine::ProToggle(T("Silent Aim (HWBP)", "سايلنت إيم (HWBP)"), &g_cfgState.bSilentAim);
            ThemeEngine::ProToggle(T("Bullet Wallbang", "اختراق الجدران للرصاص"), &g_cfgState.bSilentWallbang);
            ThemeEngine::ProToggle(T("Aim Lock Indicator", "مؤشر القفل على الهدف"), &g_cfgState.bAimIndicator);
            ThemeEngine::ProToggle(T("Smooth Camera Track", "تتبع كاميرا ناعم"), &g_cfgState.bSmoothCamAim);
            ThemeEngine::ProToggle(T("Draw FOV Ring", "إظهار دائرة المجال"), &g_cfgState.bDrawFov);
            ThemeEngine::ProToggle(T("Visibility Linecast", "فحص الرؤية"), &g_cfgState.bVisCheck);
            ThemeEngine::ProToggle(T("Velocity Prediction", "توقع حركة الهدف"), &g_cfgState.bPrediction);
            ThemeEngine::ProCombo(T("Target Bone", "عظمة التصويب"), &g_cfgState.aimBoneMode,
                                  TCombo("Head\0Body\0", "الرأس\0الجسم\0"));
            ThemeEngine::ProSlider(T("FOV Radius", "حجم الدائرة"), &g_cfgState.aimFov, 40.0f, 800.0f, "%.0f px");
            ThemeEngine::ProSlider(T("Silent Aim Range", "مدى السايلنت إيم"), &g_cfgState.saRadius, 30.0f, 400.0f, "%.0f m");
            ThemeEngine::ProSlider(T("Smoothing", "نعومة الأيم"), &g_cfgState.aimSmoothing, 0.05f, 1.0f, "%.2f");
            ThemeEngine::ProSlider(T("Bullet Speed", "سرعة الرصاصة"), &g_cfgState.bulletSpeed, 100.0f, 2000.0f, "%.0f");
            ThemeEngine::ProSlider(T("Prediction", "معامل التوقع"), &g_cfgState.predictionFactor, 0.0f, 5.0f, "%.2f");
        } else if (curSub == 1) {
            ThemeEngine::ProToggle(T("Instant Reload", "تعمير فوري"), &g_cfgState.bInstantReload);
            ThemeEngine::ProToggle(T("No Recoil", "ثبات السلاح"), &g_cfgState.bNoRecoil);
            ThemeEngine::ProToggle(T("Rapid Fire", "إطلاق نار سريع"), &g_cfgState.bRapidFire);
            ThemeEngine::ProSlider(T("Rapid Fire Speed", "سرعة الإطلاق"), &g_cfgState.rapidFireMult, 1.0f, 6.0f, "x%.1f");
            ImGui::Spacing();
            ThemeEngine::ProToggle(T("GreenZone Shoot Bypass", "إطلاق النار بالمنطقة الآمنة"), &g_cfgState.bGreenZoneBypass);
            ThemeEngine::ProToggle(T("ZoneGuard Unlock", "فك قيود المناطق"), &g_cfgState.bZoneGuard);
        } else if (curSub == 2) {
            ThemeEngine::ProToggle(T("Custom Crosshair", "نقطة تصويب مخصصة"), &g_cfgState.bCrosshairEnable, g_cfgState.crosshairColor);
            ThemeEngine::ProToggle(T("Hide Game Crosshair", "إخفاء تصويب اللعبة"), &g_cfgState.bRemoveGameCrosshair);
            ThemeEngine::ProCombo(T("Crosshair Shape", "شكل التصويب"), &g_cfgState.crosshairType,
                                  TCombo("Dot\0Cross\0", "نقطة\0صليب\0"));
            ThemeEngine::ProSlider(T("Size", "الحجم"), &g_cfgState.crosshairSize, 4.0f, 48.0f, "%.1f");
            ThemeEngine::ProSlider(T("Gap", "المسافة"), &g_cfgState.crosshairGap, 0.0f, 20.0f, "%.1f");
            ThemeEngine::ProToggle(T("Damage Numbers FX", "أرقام الضرر"), &g_cfgState.bDmgNumEnable);
            ThemeEngine::ProToggle(T("Shot Particles (31 Styles)", "تأثيرات الطلقات"), &g_cfgState.bShotPartsEnable);
            if (g_cfgState.bShotPartsEnable) {
                const char* curStyle = (g_cfgState.shotPalSel >= 0 && g_cfgState.shotPalSel < Catalogs::kShotPartStyleCount)
                    ? Catalogs::kShotPartStyles[g_cfgState.shotPalSel].name : "Custom RGB";
                if (ImGui::BeginCombo(T("CF Style", "ستايل التأثير"), curStyle)) {
                    for (int i = 0; i < Catalogs::kShotPartStyleCount; ++i) {
                        if (ImGui::Selectable(Catalogs::kShotPartStyles[i].name, g_cfgState.shotPalSel == i))
                            g_cfgState.shotPalSel = i;
                    }
                    ImGui::EndCombo();
                }
            }
        } else {
            KillAround::DrawPanel(bodyW);
        }
    }
    // ========================================================================
    // TAB 1: VISUALS (Eye Icon — ESP + Interactive Preview + FreeCam/Spectate)
    // ========================================================================
    else if (self.activeTab == 1) {
        if (curSub == 0) {
            ThemeEngine::ProToggle(T("Enable Player ESP", "تفعيل كشف اللاعبين ESP"), &g_cfgState.bEnableESP);
            ThemeEngine::ProToggle(T("Bounding Box", "مربع حول اللاعب"), &g_cfgState.bEspBox, g_cfgState.espBoxCol);
            ThemeEngine::ProCombo(T("Box Style", "شكل المربع"), &g_cfgState.espBoxStyle,
                                  TCombo("Full Box\0Cornered\0", "مربع كامل\0زوايا\0"));
            ThemeEngine::ProToggle(T("15-Bone Skeleton", "الهيكل العظمي"), &g_cfgState.bSkeletonESP, g_cfgState.espSkelCol);
            ThemeEngine::ProToggle(T("Snaplines", "خطوط للاعبين"), &g_cfgState.bEspLine, g_cfgState.espLineCol);
            ThemeEngine::ProCombo(T("Snap Origin", "بداية الخط"), &g_cfgState.lineOrigin,
                                  TCombo("Top\0Bottom\0Center\0", "أعلى\0أسفل\0وسط\0"));
            ThemeEngine::ProToggle(T("Health Bar", "شريط الصحة"), &g_cfgState.bEspHealth);
            ThemeEngine::ProToggle(T("Armor Bar", "شريط الدرع"), &g_cfgState.bEspArmor);
            ThemeEngine::ProToggle(T("Name Tags", "أسماء اللاعبين"), &g_cfgState.bEspName);
            ThemeEngine::ProToggle(T("Player ID", "أيدي اللاعب"), &g_cfgState.bEspPlayerId);
            ThemeEngine::ProToggle(T("Weapon Held", "السلاح المحمول"), &g_cfgState.bEspWeapon);
            ThemeEngine::ProToggle(T("Distance", "المسافة"), &g_cfgState.bEspDistance);
            ThemeEngine::ProToggle(T("Target Counter Pill", "عداد اللاعبين"), &g_cfgState.bEspCount);
            ThemeEngine::ProToggle(T("MiniMap Radar", "رادار مصغر"), &g_cfgState.bMiniMapEsp);
            ThemeEngine::ProToggle(T("Include Street NPCs", "إظهار البوتات"), &g_cfgState.bShowNpcs);
            ThemeEngine::ProSlider(T("ESP Radius", "مدى الكشف"), &g_cfgState.espRadiusValue, 25.0f, 500.0f, "%.0f m");
        } else if (curSub == 1) {
            // Dedicated FreeCam & All-Server Spectate + Ghost Proxy Underground Voice Sub-Tab
            ThemeEngine::ProToggle(T("Enable FreeCam (Free-Fly)", "تفعيل الكاميرا الحرة (فري كام)"), &g_cfgState.bFreeCam);
            ThemeEngine::ProSlider(T("FreeCam Speed", "سرعة الكاميرا الحرة"), &g_cfgState.freeCamSpeed, 3.0f, 120.0f, "%.0f m/s");
            ThemeEngine::ProToggle(T("Ghost Proxy Voice (Hear Far Players)", "صوت المراقبة عن بعد (غوست بروكسي)"), &g_cfgState.bFreeCamGhostVoice);
            if (g_cfgState.bFreeCamGhostVoice) {
                ThemeEngine::ProSlider(T("Underground Depth", "عمق التخفي تحت الأرض"), &g_cfgState.freeCamGhostDepthY, -45.0f, -14.0f, "%.1f m");
            }
            if (g_cfgState.bFreeCam) {
                if (ThemeEngine::NeonActionButton(T("Teleport Player to FreeCam##tpfc", "انقل اللاعب لمكان الكاميرا##tpfc"), -1.0f, 34.0f, false)) {
                    Features::TeleportLocalToFreeCam();
                }
            }
            ImGui::Spacing();
            std::vector<ServerPlayerEntry> serverList;
            {
                std::lock_guard<std::mutex> lk(g_espMutex);
                serverList = g_allServerPlayers;
            }
            char hdrBuf[96];
            snprintf(hdrBuf, sizeof(hdrBuf), "%s (%zu)",
                     T("All Server Players — Near + Far + Voice", "جميع لاعبي السيرفر — قريب وبعيد + صوت"),
                     serverList.size());
            ThemeEngine::SectionSpacer(hdrBuf);

            if (g_cfgState.bSpectateActive) {
                bool ghostOn = Hooks::g_fcGhostAnchorActive.load(std::memory_order_relaxed);
                ImGui::TextColored(ImVec4(0.30f, 0.95f, 1.0f, 1.0f), "%s: %s  [Ghost:%s | Audio:%s]",
                                   T("Spectating + Audio", "تراقب + تسمع الآن"),
                                   g_cfgState.spectateTargetName.c_str(),
                                   ghostOn ? "ON" : "WAIT",
                                   Hooks::g_fcHasSurfaceAudio.load(std::memory_order_relaxed) ? "OK" : "-");
                ThemeEngine::ProSlider(T("Orbit Angle", "زاوية الكاميرا"), &g_cfgState.freeCamOrbitYaw, 0.0f, 360.0f, "%.0f deg");
                ThemeEngine::ProSlider(T("Orbit Distance", "بعد الكاميرا"), &g_cfgState.freeCamOrbitDist, 1.5f, 28.0f, "%.1f m");
                ThemeEngine::ProToggle(T("Auto Orbit Target", "دوران تلقائي حول اللاعب"), &g_cfgState.bFreeCamAutoOrbit);
                ThemeEngine::ProToggle(T("Head Cam (Close Audio)", "كاميرا الرأس القريبة"), &g_cfgState.bFreeCamHeadCam);
                if (ThemeEngine::NeonActionButton(T("Stop Spectate##stopsp", "إيقاف المراقبة والعودة للاعب##stopsp"), -1.0f, 34.0f, true)) {
                    Features::StopSpectate();
                }
                ImGui::Spacing();
            }

            int fcSrch = ThemeEngine::SearchPillButton("##fc_pl_search",
                                                       T("Tap to search player name or ID...", "اضغط للبحث بالاسم أو الأيدي..."),
                                                       _freeCamSearchBuf);
            if (fcSrch == 1) {
                bool ar = Lang::IsArabic();
                [self promptNativeInputWithTitle:(ar ? @"بحث عن لاعب للمراقبة" : @"Search Player to Spectate")
                                     placeholder:(ar ? @"اكتب اسم اللاعب أو الأيدي..." : @"Enter player name or ID...")
                                         initial:[NSString stringWithUTF8String:_freeCamSearchBuf]
                                         numeric:NO
                                        onSubmit:^(NSString* text) {
                    const char* utf = text ? text.UTF8String : "";
                    std::snprintf(_freeCamSearchBuf, sizeof(_freeCamSearchBuf), "%s", utf ? utf : "");
                }];
            } else if (fcSrch == 2) {
                _freeCamSearchBuf[0] = '\0';
            }
            ImGui::Spacing();

            // Hide all player names by default in FreeCam — show matching results ONLY when searching!
            if (_freeCamSearchBuf[0] == '\0') {
                ImGui::TextDisabled("%s", T("Search by player name or ID above to show matching results.",
                                            "ابحث بالاسم أو الأيدي أعلاه لإظهار اللاعبين المطابقين فقط."));
            } else if (serverList.empty()) {
                ImGui::TextDisabled("%s", T("Scanning server players... join world first.",
                                            "جاري مسح لاعبي السيرفر... ادخل العالم أولاً."));
            } else {
                int matchCount = 0;
                for (const auto& sp : serverList) {
                    char idStr[32];
                    char pidStr[32];
                    snprintf(idStr, sizeof(idStr), "%u", sp.digitalId);
                    snprintf(pidStr, sizeof(pidStr), "%d", sp.playerId);
                    if (!ThemeEngine::CaseInsensitiveContains(sp.name.c_str(), _freeCamSearchBuf) &&
                        !ThemeEngine::CaseInsensitiveContains(idStr, _freeCamSearchBuf) &&
                        !ThemeEngine::CaseInsensitiveContains(pidStr, _freeCamSearchBuf)) {
                        continue;
                    }
                    ++matchCount;
                    ImGui::PushID(sp.playerId);
                    bool isCur = (g_cfgState.bSpectateActive && sp.pedEntityVal > 0 &&
                                  g_cfgState.spectateTargetPedEnt == sp.pedEntityVal);
                    ImVec4 rowCol = isCur
                        ? ImVec4(0.25f, 1.0f, 0.70f, 1.0f)
                        : (sp.isStreamed ? ImVec4(0.95f, 0.97f, 1.0f, 1.0f) : ImVec4(1.0f, 0.84f, 0.45f, 1.0f));

                    char distTag[32];
                    if (sp.isStreamed) snprintf(distTag, sizeof(distTag), "%.0fm", sp.distance);
                    else if (sp.distance > 0.0f) snprintf(distTag, sizeof(distTag), "FAR %.0fm", sp.distance);
                    else snprintf(distTag, sizeof(distTag), "FAR");

                    uint32_t shownId = (sp.digitalId > 0) ? sp.digitalId : (uint32_t)std::max(0, sp.playerId);
                    ImGui::TextColored(rowCol, "%s [#%u] [%s]", sp.name.c_str(), shownId, distTag);

                    float btnArea = sp.isStreamed ? 182.0f : 124.0f;
                    ImGui::SameLine(std::max(180.0f, bodyW - btnArea));
                    if (sp.pedEntityVal > 0) {
                        if (ImGui::Button(T("Spectate##sp", "مراقبة##sp"), ImVec2(114.0f, 26.0f))) {
                            Features::StartSpectatePlayer(sp.pedEntityVal, sp.name);
                        }
                    }
                    if (sp.isStreamed) {
                        ImGui::SameLine();
                        if (ImGui::Button(T("TP##sptp", "انتقال##sptp"), ImVec2(54.0f, 26.0f))) {
                            Features::TeleportLocalTo({ sp.worldPos.x, sp.worldPos.y + 1.0f, sp.worldPos.z });
                        }
                    }
                    ImGui::PopID();
                }
                if (matchCount == 0) {
                    ImGui::TextDisabled("%s", T("No matching player found.", "لم يتم العثور على لاعب مطابق."));
                }
            }
        } else if (curSub == 2) {
            ThemeEngine::ProToggle(T("Enable Spray ESP", "كشف البخاخات"), &g_cfgState.bSprayESP, g_cfgState.sprayEspCol);
            ThemeEngine::ProToggle(T("Only Sprayable", "القابلة للرش فقط"), &g_cfgState.bSprayEspOnlySprayable);
            ThemeEngine::ProToggle(T("Only Clash Zones", "مناطق النزاع فقط"), &g_cfgState.bSprayEspOnlyClash);
            ThemeEngine::ProToggle(T("Only Nearby", "القريبة فقط"), &g_cfgState.bSprayEspOnlyNear);
            ThemeEngine::ProToggle(T("Only District", "نفس الحي فقط"), &g_cfgState.bSprayEspOnlyDistrict);
            ThemeEngine::ProToggle(T("Spray Boxes", "مربعات البخاخات"), &g_cfgState.bSprayEspBox);
            ThemeEngine::ProToggle(T("Spray Lines", "خطوط البخاخات"), &g_cfgState.bSprayEspLines);
            ThemeEngine::ProSlider(T("Max Spray Dist", "أقصى مسافة"), &g_cfgState.sprayEspDistance, 20.0f, 400.0f, "%.0f m");
            ImGui::Spacing();
            ThemeEngine::ProToggle(T("Spray Auto-TP", "انتقال تلقائي للبخاخات"), &g_cfgState.bSprayAutoTp);
            ThemeEngine::ProSlider(T("Spray TP Delay", "مهلة الانتقال"), &g_cfgState.sprayAutoTpDelay, 1.0f, 15.0f, "%.1f s");
        } else if (curSub == 3) {
            ThemeEngine::ProToggle(T("Day / Night Override", "تغيير وقت اليوم"), &g_cfgState.bDayNightEnable);
            ThemeEngine::ProToggle(T("Freeze World Time", "تجميد الوقت"), &g_cfgState.bDayNightFreeze);
            ThemeEngine::ProSlider(T("Hour of Day", "الساعة"), &g_cfgState.dayNightTime, 0.0f, 24.0f, "%.1f h");
            ImGui::Spacing();
            ThemeEngine::ProToggle(T("Camera FOV Override", "تغيير منظور الكاميرا"), &g_cfgState.bFovEnable);
            ThemeEngine::ProSlider(T("Camera FOV", "زاوية الرؤية FOV"), &g_cfgState.cameraFov, 45.0f, 130.0f, "%.0f deg");
            ThemeEngine::ProToggle(T("Aspect Ratio Override", "تغيير أبعاد الشاشة"), &g_cfgState.bResolutionEnable);
            ThemeEngine::ProSlider(T("Aspect Scale", "نسبة الأبعاد"), &g_cfgState.aspectScale, 0.5f, 1.8f, "%.2f");
        } else {
            static const char* kRoleNames[11] = {
                "Mates", "Factions", "Civil", "Police", "Army", "Medic", "Gang", "War", "Combat", "Alliance", "Admin"
            };
            for (int r = 0; r < 11; ++r) {
                ImGui::PushID(r);
                float rgb[3] = { g_cfgState.roleR[r] / 255.0f, g_cfgState.roleG[r] / 255.0f, g_cfgState.roleB[r] / 255.0f };
                if (ImGui::ColorEdit3("##rc", rgb, ImGuiColorEditFlags_NoInputs)) {
                    g_cfgState.roleR[r] = (uint8_t)(rgb[0] * 255.0f);
                    g_cfgState.roleG[r] = (uint8_t)(rgb[1] * 255.0f);
                    g_cfgState.roleB[r] = (uint8_t)(rgb[2] * 255.0f);
                }
                ImGui::SameLine(); ImGui::Checkbox(kRoleNames[r], &g_cfgState.bShowRole[r]);
                ImGui::SameLine(180.0f); ImGui::Checkbox(T("Hide##rh", "إخفاء##rh"), &g_cfgState.bHideRole[r]);
                ImGui::SameLine(265.0f); ImGui::Checkbox(T("AimLock##rl", "قفل##rl"), &g_cfgState.bLockRole[r]);
                ImGui::PopID();
            }
        }
    }
    // ========================================================================
    // TAB 2: PLAYER, MAP TELEPORT & VEHICLE (4-Tile Grid Icon)
    // ========================================================================
    else if (self.activeTab == 2) {
        if (curSub == 0) {
            ThemeEngine::ProToggle(T("AirWalk (Walk In Sky)", "مشي بالسماء باتجاه الكاميرا"), &g_cfgState.bFlyHack);
            if (g_cfgState.bFlyHack) {
                ImGui::TextDisabled("%s", T("Look UP to rise, DOWN to descend — walk, aim & shoot normally",
                                            "ارفع الكاميرا للصعود وأنزلها للنزول — امشِ وصوّب وأطلق طبيعياً"));
            }
            ThemeEngine::ProSlider(T("AirWalk Speed", "سرعة المشي بالسماء"), &g_cfgState.flySpeed, 2.0f, 35.0f, "%.1f m/s");
            ImGui::Spacing();
            ThemeEngine::ProToggle(T("Super Jump", "القفز الخارق"), &g_cfgState.bSuperJump);
            ThemeEngine::ProSlider(T("Jump Force", "قوة القفز"), &g_cfgState.superJumpForce, 5.0f, 50.0f, "%.1f");
            ThemeEngine::ProToggle(T("Super Punch Speed", "سرعة اللكمة"), &g_cfgState.bSuperPunch);
            ThemeEngine::ProSlider(T("Punch Multiplier", "مضاعف اللكمة"), &g_cfgState.superPunchSpeed, 1.0f, 10.0f, "x%.1f");
            ThemeEngine::ProToggle(T("SpeedHack (TimeScale)", "تسريع اللعبة"), &g_cfgState.bSpeedHack);
            ThemeEngine::ProSlider(T("Speed Scale", "مضاعف السرعة"), &g_cfgState.speedMultiplier, 0.5f, 8.0f, "x%.1f");
        } else if (curSub == 1) {
            // Dedicated Map Marker / Waypoint Teleport Sub-Tab (Solo + Inside Vehicle Together)
            float tpBtnW = std::max(130.0f, bodyW - 128.0f);
            if (ThemeEngine::NeonActionButton(T("Teleport to Map Marker (Solo / Car)##tpbtn", "الانتقال لعلامة الخريطة (فردي / بالسيارة)##tpbtn"), tpBtnW, 38.0f, false)) {
                Features::TeleportToNearestUserMapPin();
            }
            ImGui::SameLine();
            if (ThemeEngine::NeonActionButton(T("Refresh##rfmk", "تحديث##rfmk"), 120.0f, 38.0f, false)) {
                Features::ForceRefreshMapMarkers();
            }
            ImGui::Spacing();
            ThemeEngine::ProToggle(T("Auto-TP When Placing Map Pin", "انتقال تلقائي عند وضع علامة بالخريطة"), &g_cfgState.bAutoMapMarkerTp);
        } else if (curSub == 2) {
            ThemeEngine::ProToggle(T("NoFall Damage", "منع ضرر السقوط"), &g_cfgState.bNoFall);
            ThemeEngine::ProToggle(T("Anti-Taser (No Stun)", "منع التيزر والصعق"), &g_cfgState.bAntiTaser);
            ThemeEngine::ProToggle(T("Anti-Faint (No Knockout)", "منع الإغماء وفقدان الوعي"), &g_cfgState.bAntiFaint);
            ThemeEngine::ProToggle(T("Infinite Stamina", "طاقة جري لا نهائية"), &g_cfgState.bInfStamina);
            ThemeEngine::ProToggle(T("Auto Heal (Inventory)", "علاج تلقائي"), &g_cfgState.bAutoHeal);
            ThemeEngine::ProSliderInt(T("Heal Under HP", "علاج تحت صحة"), &g_cfgState.autoHealUnderHp, 5, 95);
            ThemeEngine::ProToggle(T("Auto Vest (HG1..HG4)", "درع تلقائي"), &g_cfgState.bAutoVest);
            ThemeEngine::ProSliderInt(T("Vest Under %", "درع تحت نسبة"), &g_cfgState.autoVestUnderPct, 5, 95);
            ThemeEngine::ProToggle(T("Hitbox Scale", "تكبير حجم الأعداء"), &g_cfgState.bScaleEnable);
            if (g_cfgState.bScaleEnable) {
                ImGui::Checkbox(T("Big Head##bh", "رأس كبير##bh"), &g_cfgState.bBigHead); ImGui::SameLine();
                ImGui::Checkbox(T("Big Chest##bc", "صدر كبير##bc"), &g_cfgState.bBigChest); ImGui::SameLine();
                ImGui::Checkbox(T("Big Body##bb", "جسم كبير##bb"), &g_cfgState.bBigBody);
                ThemeEngine::ProSlider(T("Scale Factor", "نسبة التكبير"), &g_cfgState.playerScale, 1.0f, 8.0f, "x%.1f");
            }
            ThemeEngine::ProToggle(T("Anti-TP Guard (Ped)", "حماية ضد سحب اللاعب"), &g_cfgState.bAntiTp);
            ThemeEngine::ProToggle(T("Anti-TP Guard (Car)", "حماية ضد سحب السيارة"), &g_cfgState.bAntiTpCar);
            ThemeEngine::ProToggle(T("Anti-AFK Pulse", "منع الطرد للخمول"), &g_cfgState.bAntiAfk);
            ThemeEngine::ProToggle(T("Nickname Regex Bypass", "تخطي قيود الاسم"), &g_cfgState.bNickBypass);
            ThemeEngine::ProToggle(T("Whisper VoiceGuard", "حماية الصوت"), &g_cfgState.bVoiceGuard);
        } else if (curSub == 3) {
            ThemeEngine::ProToggle(T("Car Speed & Torque", "تسريع وعزم السيارة"), &g_cfgState.bCarHack);
            ThemeEngine::ProSlider(T("Car Speed Scale", "سرعة السيارة"), &g_cfgState.carSpeed, 1.0f, 25.0f, "x%.1f");
            ThemeEngine::ProToggle(T("Car Fly Mode (Game Controls)", "طيران السيارة (بأزرار اللعبة)"), &g_cfgState.bCarFly);
            if (g_cfgState.bCarFly) {
                ImGui::TextDisabled("%s", T("Gas/Brake + Steer + Camera pitch to climb/dive • Handbrake=Hover Stop",
                                            "بنزين/فرامل + التفاف + الكاميرا للصعود/النزول • فرامل اليد=تثبيت"));
            }
            ThemeEngine::ProSlider(T("Car Fly Speed", "سرعة طيران السيارة"), &g_cfgState.carFlySpeed, 5.0f, 120.0f, "%.1f m/s");
            ThemeEngine::ProToggle(T("Auto Repair", "تصليح تلقائي للسيارة"), &g_cfgState.bAutoRepair);
            ThemeEngine::ProToggle(T("Parking Spawn Capture (10s)", "استبدال سيارة الموقف"), &g_cfgState.bVehSpawnCapture);
            ImGui::Spacing();
            ImGui::ColorEdit3(T("Body Paint##bpcol", "لون السيارة##bpcol"), g_cfgState.carPaintRgb); ImGui::SameLine();
            if (ImGui::Button(T("Apply##bp", "تطبيق##bp"))) {
                float rgba[4] = { g_cfgState.carPaintRgb[0], g_cfgState.carPaintRgb[1], g_cfgState.carPaintRgb[2], 1.0f };
                Features::ApplyCarPaintColor(Morpeh::GetLocalPedEntity(), rgba);
            }
            ImGui::ColorEdit4(T("Window Tint##wtcol", "تظليل الزجاج##wtcol"), g_cfgState.carWinRgba); ImGui::SameLine();
            if (ImGui::Button(T("Apply##wt", "تطبيق##wt"))) Features::ApplyCarWindowTint(Morpeh::GetLocalPedEntity(), g_cfgState.carWinRgba);
            ImGui::ColorEdit4(T("Rim Color##rmcol", "لون الجنوط##rmcol"), g_cfgState.rimColorRgba);
            if (ImGui::TreeNode(T("Exterior Bodykit (10 Parts)##bk", "تعديل الهيكل الخارجي##bk"))) {
                static const char* kTN[10] = { "Front Bumper","Rear Bumper","Side Skirt","Hood","Trunk","Spoiler","Moto Front","Moto Rear","Moto Body","Exhaust" };
                for (int t = 0; t < 10; ++t) ImGui::SliderInt(kTN[t], &g_cfgState.vehTuneExt[t], 0, 15);
                if (ImGui::Button(T("Apply Bodykit##abk", "تطبيق التعديل##abk"))) Features::ApplyVehTuneExternal(Morpeh::GetLocalPedEntity(), g_cfgState.vehTuneExt);
                ImGui::TreePop();
            }
        } else {
            static int gCat = 0;
            ThemeEngine::ProCombo(T("Catalog", "القائمة"), &gCat,
                                  TCombo("Cars (413)\0Car Skins (414)\0Rims (41)\0", "سيارات (413)\0سكنات سيارات (414)\0جنوط (41)\0"));
            if (gCat == 0) {
                [self renderCatalogListIdName:Catalogs::kCars count:Catalogs::kCarsCount tag:"##cars_l" onSelect:^(const Catalogs::IdNameEntry& e) {
                    g_cfgState.pendingCarId = (int)e.id;
                    Features::ApplyCarLocal((int32_t)e.id);
                }];
            } else if (gCat == 1) {
                [self renderCatalogListIdName:Catalogs::kSkCars count:Catalogs::kSkCarsCount tag:"##skcars_l" onSelect:^(const Catalogs::IdNameEntry& e) {
                    g_cfgState.pendingCarId = (int)e.id;
                    Features::ApplyCarLocal((int32_t)e.id);
                }];
            } else {
                [self renderCatalogListIdName:Catalogs::kSkRims count:Catalogs::kSkRimsCount tag:"##rims_l" onSelect:^(const Catalogs::IdNameEntry& e) {
                    g_cfgState.pendingRimId = (int)e.id;
                    Features::ApplyRimIdAndColor(Morpeh::GetLocalPedEntity(), (int32_t)e.id, g_cfgState.rimColorRgba);
                }];
            }
        }
    }
    // ========================================================================
    // TAB 3: WARDROBE, AUTOMATION & LIVE PLAYERS (Document Icon)
    // ========================================================================
    else if (self.activeTab == 3) {
        if (curSub == 0) {
            const char* wSubs[] = {
                T("Skins##w0", "سكنات##w0"),
                T("Clothes##w1", "ملابس##w1"),
                T("Accs##w2", "إكسسوار##w2"),
                T("Emotes##w3", "رقصات##w3"),
                T("Weapons##w4", "أسلحة##w4")
            };
            for (int i = 0; i < 5; ++i) {
                if (i > 0) ImGui::SameLine();
                if (ImGui::Button(wSubs[i], ImVec2(84, 24))) self.wardrobeSubTab = i;
            }
            ImGui::Spacing();
            if (self.wardrobeSubTab == 0) {
                [self renderCatalogListIdName:Catalogs::kSkins count:Catalogs::kSkinsCount tag:"##skins_l" onSelect:^(const Catalogs::IdNameEntry& e) {
                    g_cfgState.pendingSkinId = (int)e.id;
                    Features::ApplySkinLocal((int32_t)e.id, (uint16_t)g_cfgState.pendingSkinGender);
                }];
            } else if (self.wardrobeSubTab == 1) {
                [self renderCatalogListSlot:Catalogs::kClothes count:Catalogs::kClothesCount tag:"##clothes_l" onSelect:^(const Catalogs::IdNameSlotEntry& e) {
                    g_cfgState.pendingClothId = (int)e.id;
                    Features::ApplyClothesLocal((uint16_t)e.id, (int32_t)e.slotOrGender);
                }];
            } else if (self.wardrobeSubTab == 2) {
                ThemeEngine::ProCombo(T("Category", "التصنيف"), &g_cfgState.pendingAccSlot,
                                      TCombo("Hats (78)\0Masks (48)\0Backpacks (18)\0", "قبعات (78)\0أقنعة (48)\0حقائب (18)\0"));
                if (ImGui::Button(T("Strip All Accessories##strp", "إزالة كل الإكسسوارات##strp"))) Features::StripAllAccessoriesLocal();
                if (g_cfgState.pendingAccSlot == 0) {
                    [self renderCatalogListSlot:Catalogs::kHats count:Catalogs::kHatsCount tag:"##hats_l" onSelect:^(const Catalogs::IdNameSlotEntry& e) {
                        g_cfgState.pendingAccId = (int)e.id;
                        Features::ApplyAccessoryLocal((int32_t)e.id, (int32_t)e.slotOrGender);
                    }];
                } else if (g_cfgState.pendingAccSlot == 1) {
                    [self renderCatalogListSlot:Catalogs::kMasks count:Catalogs::kMasksCount tag:"##masks_l" onSelect:^(const Catalogs::IdNameSlotEntry& e) {
                        g_cfgState.pendingAccId = (int)e.id;
                        Features::ApplyAccessoryLocal((int32_t)e.id, (int32_t)e.slotOrGender);
                    }];
                } else {
                    [self renderCatalogListSlot:Catalogs::kBackpacks count:Catalogs::kBackpacksCount tag:"##bags_l" onSelect:^(const Catalogs::IdNameSlotEntry& e) {
                        g_cfgState.pendingAccId = (int)e.id;
                        Features::ApplyAccessoryLocal((int32_t)e.id, (int32_t)e.slotOrGender);
                    }];
                }
            } else if (self.wardrobeSubTab == 3) {
                ThemeEngine::ProToggle(T("Auto-Danse Loop", "تكرار الرقصة تلقائياً"), &g_cfgState.bAutoDanse);
                [self renderCatalogListIdName:Catalogs::kEmotes count:Catalogs::kEmotesCount tag:"##emotes_l" onSelect:^(const Catalogs::IdNameEntry& e) {
                    g_cfgState.pendingEmoteId = (int)e.id;
                    g_cfgState.autoDanseId = (int)e.id;
                    Features::ApplyEmoteLocal((int32_t)e.id);
                }];
            } else {
                ImGui::BeginChild("##weaps_l", ImVec2(0, 195), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
                for (int i = 0; i < Catalogs::kWeaponCount; ++i) {
                    char wb[96];
                    snprintf(wb, sizeof(wb), "%s (%s) [#%lld]", Catalogs::kWeapons[i].displayName, Catalogs::kWeapons[i].internalName, (long long)Catalogs::kWeapons[i].id);
                    if (ImGui::Selectable(wb, g_cfgState.pendingWeaponId == Catalogs::kWeapons[i].id)) {
                        g_cfgState.pendingWeaponId = (int)Catalogs::kWeapons[i].id;
                        Features::ApplyWeaponLocal((int32_t)Catalogs::kWeapons[i].id);
                    }
                }
                ImGui::EndChild();
            }
            ImGui::Spacing();
            ThemeEngine::ProToggle(T("Radio Override", "تغيير تردد الراديو"), &g_cfgState.bRadioEnable);
            char rchBuf[32];
            snprintf(rchBuf, sizeof(rchBuf), "%d", g_cfgState.radioChannel);
            if (ThemeEngine::InputValueRow(T("Radio Channel##rch", "رقم القناة##rch"), rchBuf)) {
                bool ar = Lang::IsArabic();
                [self promptNativeInputWithTitle:(ar ? @"تردد الراديو" : @"Radio Channel")
                                     placeholder:@"2001"
                                         initial:[NSString stringWithUTF8String:rchBuf]
                                         numeric:YES
                                        onSubmit:^(NSString* text) {
                    if (text.length > 0) g_cfgState.radioChannel = (int)[text integerValue];
                }];
            }
        } else if (curSub == 1) {
            ThemeEngine::ProToggle(T("Auto Job / Delivery Glitch", "قلتش الوظيفة التلقائي"), &g_cfgState.bMoneyGlitch);
            ThemeEngine::ProSlider(T("Action Interval", "الفاصل الزمني"), &g_cfgState.moneyGlitchSpeed, 2.0f, 30.0f, "%.1f s");
            ThemeEngine::ProToggle(T("Steal Event Pull", "سحب حدث السرقة"), &g_cfgState.bStealEvent);
            ThemeEngine::ProToggle(T("Job Checkpoint Pull", "سحب نقاط الوظيفة"), &g_cfgState.bJobTp);
            ThemeEngine::ProToggle(T("Walk Checkpoints", "المشي للنقاط تلقائياً"), &g_cfgState.bWalkCp);
            ThemeEngine::ProToggle(T("VehAi Auto Drive", "قيادة تلقائية بالسيارة"), &g_cfgState.bVehAiDrive);
            ThemeEngine::ProToggle(T("Gems Route Farm", "تجميع الجواهر تلقائياً"), &g_cfgState.bGemsFarm);
            ImGui::Spacing();
            char pidBuf[32];
            snprintf(pidBuf, sizeof(pidBuf), "%lld", (long long)g_cfgState.targetPlayerPid);
            if (ThemeEngine::InputValueRow(T("Target PID##tpid", "أيدي الهدف##tpid"), pidBuf)) {
                bool ar = Lang::IsArabic();
                [self promptNativeInputWithTitle:(ar ? @"أيدي اللاعب الهدف" : @"Target Player ID")
                                     placeholder:@"-1"
                                         initial:[NSString stringWithUTF8String:pidBuf]
                                         numeric:YES
                                        onSubmit:^(NSString* text) {
                    if (text.length > 0) g_cfgState.targetPlayerPid = (int64_t)[text longLongValue];
                }];
            }
            ThemeEngine::ProToggle(T("Follow Target PID", "ملاحقة الهدف"), &g_cfgState.bFollowTarget);
            ThemeEngine::ProToggle(T("KillFly Target PID", "طيران القتل للهدف"), &g_cfgState.bKillFly);
            ThemeEngine::ProToggle(T("CarKill Target PID", "دهس الهدف بالسيارة"), &g_cfgState.bCarKill);
        } else {
            // All Server Players (Near + Far) with Native iOS Keyboard Search, Spectate+Voice, Lock, TP & Outfit Copy
            std::vector<ServerPlayerEntry> serverList;
            std::vector<ESPPlayer> streamedList;
            {
                std::lock_guard<std::mutex> lk(g_espMutex);
                serverList   = g_allServerPlayers;
                streamedList = g_renderPlayers;
            }
            int plSrch = ThemeEngine::SearchPillButton("##pl_all_search",
                                                       T("Tap to search players (name / ID)...", "اضغط للبحث عن لاعب (الاسم / الأيدي)..."),
                                                       _playerSearchBuf);
            if (plSrch == 1) {
                bool ar = Lang::IsArabic();
                [self promptNativeInputWithTitle:(ar ? @"بحث في لاعبي السيرفر" : @"Search All Server Players")
                                     placeholder:(ar ? @"اكتب الاسم أو الأيدي..." : @"Type player name or ID...")
                                         initial:[NSString stringWithUTF8String:_playerSearchBuf]
                                         numeric:NO
                                        onSubmit:^(NSString* text) {
                    const char* utf = text ? text.UTF8String : "";
                    std::snprintf(_playerSearchBuf, sizeof(_playerSearchBuf), "%s", utf ? utf : "");
                }];
            } else if (plSrch == 2) {
                _playerSearchBuf[0] = '\0';
            }
            ImGui::TextDisabled("%s: %zu", T("Total Server Players", "مجموع لاعبي السيرفر"), serverList.size());
            ImGui::Spacing();

            if (serverList.empty()) {
                ImGui::TextDisabled("%s", T("Scanning server players... join world to populate list.",
                                            "جاري مسح لاعبي السيرفر... ادخل العالم لتحديث القائمة."));
            } else {
                for (const auto& sp : serverList) {
                    if (_playerSearchBuf[0] != '\0') {
                        char idStr[32];
                        snprintf(idStr, sizeof(idStr), "%u", sp.digitalId);
                        if (!ThemeEngine::CaseInsensitiveContains(sp.name.c_str(), _playerSearchBuf) &&
                            !ThemeEngine::CaseInsensitiveContains(idStr, _playerSearchBuf)) {
                            continue;
                        }
                    }
                    ImGui::PushID(sp.playerId);
                    int64_t pidVal = (sp.digitalId > 0) ? (int64_t)sp.digitalId : (int64_t)sp.playerId;
                    ImVec4 col = sp.isStreamed ? ImVec4(0.95f, 0.97f, 1.0f, 1.0f) : ImVec4(1.0f, 0.84f, 0.45f, 1.0f);
                    if (sp.isStreamed) {
                        ImGui::TextColored(col, "%s [#%lld] HP:%d (%.0fm)", sp.name.c_str(), (long long)pidVal, sp.health, sp.distance);
                    } else {
                        ImGui::TextColored(col, "%s [#%lld] [FAR]", sp.name.c_str(), (long long)pidVal);
                    }
                    ImGui::SameLine(std::max(185.0f, bodyW - 236.0f));
                    if (sp.pedEntityVal > 0 && ImGui::Button(T("Spec##plsp", "مراقبة##plsp"), ImVec2(64.0f, 25.0f))) {
                        Features::StartSpectatePlayer(sp.pedEntityVal, sp.name);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(T("Lock##pllk", "قفل##pllk"), ImVec2(52.0f, 25.0f))) {
                        g_cfgState.targetPlayerPid = pidVal;
                    }
                    if (sp.isStreamed) {
                        ImGui::SameLine();
                        if (ImGui::Button(T("TP##pltp", "انتقال##pltp"), ImVec2(48.0f, 25.0f))) {
                            Features::TeleportLocalTo({ sp.worldPos.x, sp.worldPos.y + 1.0f, sp.worldPos.z });
                        }
                        ImGui::SameLine();
                        if (ImGui::Button(T("Copy##plcp", "نسخ##plcp"), ImVec2(52.0f, 25.0f))) {
                            for (const auto& ep : streamedList) {
                                if (ep.entityId == sp.pedId) { Features::CopyPlayerOutfit(ep); break; }
                            }
                        }
                    }
                    ImGui::PopID();
                }
            }
        }
    }
    // ========================================================================
    // TAB 4: CONFIG, 3D NEON THEME & LANGUAGE (Cloud Icon)
    // ========================================================================
    else {
        if (curSub == 0) {
            ImGui::TextDisabled("%s: %s  |  %.0f FPS", T("Status", "الحالة"), g_cfgState.lastStatusBanner.c_str(), ImGui::GetIO().Framerate);
            ImGui::Spacing();
            if (ThemeEngine::NeonActionButton(T("Save Profile##svp", "حفظ الإعدادات##svp"), 190.0f, 30.0f, false))
                ConfigStore::SaveProfile(g_cfgState.activeProfileName.c_str());
            ImGui::SameLine();
            if (ThemeEngine::NeonActionButton(T("Load Profile##ldp", "استعادة الإعدادات##ldp"), 190.0f, 30.0f, false))
                ConfigStore::LoadProfile(g_cfgState.activeProfileName.c_str());
            ImGui::Spacing();
            if (ImGui::Button(T("Reload offsets.json##rlo", "تحديث ملف الأوفست##rlo"), ImVec2(190, 28))) {
                ConfigStore::LoadOffsetsJson();
                g_cfgState.lastStatusBanner = "Reloaded offsets.json";
            }
            ImGui::Spacing();
            ThemeEngine::SectionSpacer(T("Online VIP License", "حالة الاشتراك الأونلاين"));
            std::string expStr = AuthSystem::GetActiveExpiry();
            std::string hwStr  = AuthSystem::GetOrCreateDeviceHWID();
            ImGui::TextColored(ImVec4(0.28f, 0.98f, 0.65f, 1.0f), "%s: ACTIVE  |  %s: %s",
                               T("License", "الاشتراك"),
                               T("Expires", "ينتهي في"),
                               expStr.empty() ? "-" : expStr.c_str());
            ImGui::TextDisabled("HWID: %s", hwStr.c_str());
            if (ThemeEngine::NeonActionButton(T("Logout / Switch Key##authout", "تسجيل الخروج / تغيير المفتاح##authout"), 240.0f, 30.0f, true)) {
                AuthSystem::Logout();
            }
            ImGui::Spacing();
            ImGui::TextDisabled("Community: %s", ThemeEngine::kCommunityUrl);
        } else {
            if (ThemeEngine::ProCombo(T("Language / اللغة", "اللغة / Language"), &g_cfgState.languageMode,
                                      TCombo("English\0Arabic\0", "English\0العربية\0"))) {
                Lang::SetLanguage(g_cfgState.languageMode);
            }
            ThemeEngine::ProToggle(T("Ambient Glass Glow", "إضاءة زجاجية ناعمة"), &g_cfgState.bNeon3DGrid);
            int pIdx = std::clamp(g_cfgState.neonPreset, 0, 5);
            if (ImGui::BeginCombo(T("iOS Accent Theme", "ثيم الآيفون"), ThemeEngine::kPresets[pIdx].name)) {
                for (int i = 0; i < 6; ++i) {
                    if (ImGui::Selectable(ThemeEngine::kPresets[i].name, pIdx == i)) {
                        g_cfgState.neonPreset = i;
                        ThemeEngine::ApplyPreset((ThemeEngine::Preset)i);
                    }
                }
                ImGui::EndCombo();
            }
            ThemeEngine::ProSlider(T("Accent Color", "لون الثيم"), &ThemeEngine::g_state.hue, 0.0f, 1.0f, "%.2f");
            ThemeEngine::ProSlider(T("Window Opacity", "شفافية القائمة"), &ThemeEngine::g_state.glassAlpha, 0.65f, 1.0f, "%.2f");
            ThemeEngine::ProSlider(T("Menu Scale", "حجم القائمة"), &ThemeEngine::g_state.uiScale, 0.75f, 1.25f, "%.2f");
            ThemeEngine::ProSlider(T("Floating Pill Scale", "حجم الزر العائم"), &ThemeEngine::g_state.btnScale, 0.70f, 1.50f, "%.2f");
            ImGui::Spacing();
            ThemeEngine::ProToggle(T("MASTER PANIC OFF", "إيقاف طوارئ لكل الميزات"), &g_cfgState.bMasterDisable);
            ThemeEngine::ProToggle(T("Device ID Spoofer", "تغيير أيدي الجهاز"), &g_cfgState.bDeviceIdSpoof);
            if (ThemeEngine::ProToggle(T("Streamer Capture Hide", "إخفاء القائمة بالتصوير"), &g_cfgState.bStreamerMode))
                [self applyStreamerMode:g_cfgState.bStreamerMode];
            ThemeEngine::ProSlider(T("Target FPS", "فريمات اللعبة"), &g_cfgState.targetFPS, 30.0f, 120.0f, "%.0f fps");
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    float curScrollY = ImGui::GetScrollY();
    float maxScrollY = ImGui::GetScrollMaxY();
    ImGui::EndChild();

    // Pinned ESP Interactive Preview Card on Tab 1 Sub 0 (between MainBody and Right Scrollbar Rail)
    if (isEspPreviewSub) {
        ImGui::SetCursorScreenPos(ImVec2(contentX + bodyW + 12.0f, wp.y + 62.0f));
        ImGui::BeginChild("##vis_prev", ImVec2(espPreviewW, bodyH), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ThemeEngine::PreviewConfig pc{};
        pc.box        = g_cfgState.bEspBox;
        pc.boxStyle   = g_cfgState.espBoxStyle;
        pc.health     = g_cfgState.bEspHealth;
        pc.armor      = g_cfgState.bEspArmor;
        pc.name       = g_cfgState.bEspName;
        pc.playerId   = g_cfgState.bEspPlayerId;
        pc.skeleton   = g_cfgState.bSkeletonESP;
        pc.weapon     = g_cfgState.bEspWeapon;
        pc.distance   = g_cfgState.bEspDistance;
        pc.snapline   = g_cfgState.bEspLine;
        pc.lineOrigin = g_cfgState.lineOrigin;
        pc.boxCol     = IM_COL32((int)(g_cfgState.espBoxCol[0] * 255), (int)(g_cfgState.espBoxCol[1] * 255), (int)(g_cfgState.espBoxCol[2] * 255), 255);
        int pose = self.previewPose;
        ThemeEngine::RenderInteractivePreview(pc, pose);
        self.previewPose = pose;
        ImGui::EndChild();
    }

    // Master Right-Side Comfortable Up/Down Scrollbar Rail ([Up] + Wide Drag Thumb + [Down])
    float nextScrollY = curScrollY;
    if (ThemeEngine::DrawTouchScrollRail("##deck_scroll_rail",
                                         ImVec2(contentX + contentW - scrollRailW, wp.y + 62.0f),
                                         scrollRailW, bodyH, curScrollY, maxScrollY, nextScrollY)) {
        _pendingScrollY = nextScrollY;
    }

    ImGui::End();
}

@end

__attribute__((constructor))
static void OneStateFullInit() {
    ConfigStore::SaveOffsetsTemplateIfMissing();
    ConfigStore::LoadOffsetsJson();
    ConfigStore::LoadProfile("default");

    g_overlay = [[OneStateGlassOverlay alloc] init];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [g_overlay presentWhenReady];
    });
}
