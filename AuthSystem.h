#pragma once
// AuthSystem.h — Hardened Online License Gate & Redesigned Cyber-Glass Pro Login UI
// Target: One State RP 1.4.1 (iOS arm64, integrated inside OneStateFull)
// Backend: GoogleAppsScript_Code.gs (Sheet "Keys": Code | Expiry | Status | HWID | Note)
// Provenance of endpoint & secret: OneStateOffline/Tweak.xm (L13-20) & GoogleAppsScript_Code.gs (L14)

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <Security/Security.h>
#import <CommonCrypto/CommonDigest.h>
#import <CommonCrypto/CommonHMAC.h>
#include <sys/sysctl.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>

#include "imgui.h"
#include "Lang.h"
#include "Theme.h"

// ============================================================================
// 1. Compile-Time Rolling-XOR String Vault (Zero plaintext URL/secret in binary)
// ============================================================================
namespace AuthCrypto {

inline void SecureZero(void* ptr, size_t len) {
    if (!ptr || len == 0) return;
    volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(ptr);
    for (size_t i = 0; i < len; ++i) p[i] = 0;
}

template <size_t N, uint8_t K>
struct ObfStr {
    uint8_t enc[N]{};

    constexpr ObfStr(const char (&s)[N]) {
        for (size_t i = 0; i < N; ++i) {
            uint8_t mask = static_cast<uint8_t>((static_cast<uint32_t>(K) + static_cast<uint32_t>(i) * 37u +
                                                 static_cast<uint32_t>(i * i) * 13u) & 0xFFu);
            enc[i] = static_cast<uint8_t>(static_cast<uint8_t>(s[i]) ^ mask);
        }
    }

    __attribute__((noinline))
    std::string Decode() const {
        std::string out;
        out.resize(N > 0 ? (N - 1) : 0);
        for (size_t i = 0; i + 1 < N; ++i) {
            uint8_t mask = static_cast<uint8_t>((static_cast<uint32_t>(K) + static_cast<uint32_t>(i) * 37u +
                                                 static_cast<uint32_t>(i * i) * 13u) & 0xFFu);
            out[i] = static_cast<char>(enc[i] ^ mask);
        }
        return out;
    }

    NSString* DecodeNS() const {
        std::string s = Decode();
        NSString* ns = [NSString stringWithUTF8String:s.c_str()];
        if (!s.empty()) SecureZero(&s[0], s.size());
        return ns ?: @"";
    }
};

#define AUTH_OBF(str, key) ([]() -> const auto& { \
    static constexpr AuthCrypto::ObfStr<sizeof(str), static_cast<uint8_t>(key)> _v(str); \
    return _v; \
}())

inline uint64_t Fnv1a64(const char* s, size_t n) {
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= static_cast<uint8_t>(s[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

inline uint64_t Fnv1a64Str(const std::string& s) {
    return Fnv1a64(s.data(), s.size());
}

inline std::string Sha256Hex(const std::string& input) {
    uint8_t digest[CC_SHA256_DIGEST_LENGTH]{};
    CC_SHA256(input.data(), static_cast<CC_LONG>(input.size()), digest);
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(CC_SHA256_DIGEST_LENGTH * 2);
    for (size_t i = 0; i < CC_SHA256_DIGEST_LENGTH; ++i) {
        out[i * 2]     = kHex[(digest[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[digest[i] & 0x0F];
    }
    SecureZero(digest, sizeof(digest));
    return out;
}

inline void HmacSha256(const uint8_t* key, size_t keyLen, const std::string& msg, uint8_t out32[32]) {
    CCHmac(kCCHmacAlgSHA256, key, keyLen, msg.data(), msg.size(), out32);
}

inline uint64_t LoadU64LE(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (static_cast<uint64_t>(p[i]) << (i * 8));
    return v;
}

inline bool ConstantTimeEq64(uint64_t a, uint64_t b) {
    volatile uint64_t diff = a ^ b;
    return diff == 0;
}

} // namespace AuthCrypto

// ============================================================================
// 2. Hardened Anti-Proxy / Anti-MITM NSURLSession Delegate
// ============================================================================
@interface AuthPinSessionDelegate : NSObject <NSURLSessionTaskDelegate, NSURLSessionDelegate>
@property (nonatomic, assign) BOOL blockedMitm;
@end

@implementation AuthPinSessionDelegate

+ (BOOL)isAllowedHost:(NSString*)host {
    if (!host || host.length == 0) return NO;
    NSString* lo = host.lowercaseString;
    NSString* h1 = AUTH_OBF("script.google.com", 0x5A).DecodeNS();
    NSString* h2 = AUTH_OBF("script.googleusercontent.com", 0x73).DecodeNS();
    if ([lo isEqualToString:h1] || [lo isEqualToString:h2]) return YES;
    NSString* dot2 = [@"." stringByAppendingString:h2];
    return [lo hasSuffix:dot2];
}

- (void)URLSession:(NSURLSession*)session
              task:(NSURLSessionTask*)task
willPerformHTTPRedirection:(NSHTTPURLResponse*)response
        newRequest:(NSURLRequest*)request
 completionHandler:(void (^)(NSURLRequest*))completionHandler {
    NSString* scheme = request.URL.scheme.lowercaseString;
    if (![scheme isEqualToString:@"https"] || ![AuthPinSessionDelegate isAllowedHost:request.URL.host]) {
        self.blockedMitm = YES;
        completionHandler(nil);
        return;
    }
    NSMutableURLRequest* cleanReq = [request mutableCopy];
    cleanReq.cachePolicy = NSURLRequestReloadIgnoringLocalAndRemoteCacheData;
    completionHandler(cleanReq);
}

- (void)URLSession:(NSURLSession*)session
didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
 completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*))completionHandler {
    if (![challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
        return;
    }
    SecTrustRef trust = challenge.protectionSpace.serverTrust;
    if (!trust || ![AuthPinSessionDelegate isAllowedHost:challenge.protectionSpace.host]) {
        self.blockedMitm = YES;
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        return;
    }

    CFErrorRef err = NULL;
    bool trusted = SecTrustEvaluateWithError(trust, &err);
    if (err) CFRelease(err);
    if (!trusted) {
        self.blockedMitm = YES;
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        return;
    }

    // Reject known HTTPS MITM / debugging proxy root/leaf certificates
    CFArrayRef chainRef = SecTrustCopyCertificateChain(trust);
    if (chainRef) {
        NSArray* chain = (__bridge_transfer NSArray*)chainRef;
        NSString* badCerts[] = {
            AUTH_OBF("charles", 0x19).DecodeNS(),
            AUTH_OBF("proxyman", 0x2B).DecodeNS(),
            AUTH_OBF("portswigger", 0x3C).DecodeNS(),
            AUTH_OBF("burp", 0x4D).DecodeNS(),
            AUTH_OBF("fiddler", 0x5E).DecodeNS(),
            AUTH_OBF("mitmproxy", 0x6F).DecodeNS(),
            AUTH_OBF("httptoolkit", 0x71).DecodeNS(),
            AUTH_OBF("reqable", 0x82).DecodeNS(),
            AUTH_OBF("stream", 0x93).DecodeNS()
        };
        for (id certObj in chain) {
            SecCertificateRef cert = (__bridge SecCertificateRef)certObj;
            CFStringRef summaryRef = SecCertificateCopySubjectSummary(cert);
            if (summaryRef) {
                NSString* summary = ((__bridge_transfer NSString*)summaryRef).lowercaseString;
                for (NSString* bad : badCerts) {
                    if (bad.length > 0 && [summary containsString:bad]) {
                        self.blockedMitm = YES;
                        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
                        return;
                    }
                }
            }
        }
    }

    NSURLCredential* cred = [NSURLCredential credentialForTrust:trust];
    completionHandler(NSURLSessionAuthChallengeUseCredential, cred);
}

@end

// ============================================================================
// 3. Anti-Crack Online Authentication Core + Split Cryptographic Session Vault
// ============================================================================
namespace AuthSystem {

// Decoy honeypot globals — deliberately unused by real verification (bait for binary patchers)
inline volatile bool g_vipUnlocked = false;
inline volatile bool g_isAuthenticated = false;
inline volatile int  g_licenseTier = 0;

// Verified deployed config (OneStateOffline/Tweak.xm L13-19 & GoogleAppsScript_Code.gs L14), stored obfuscated
struct Config {
    static NSString* EndpointURL() {
        return AUTH_OBF("https://script.google.com/macros/s/AKfycbyrO5ry9JL_5BAxyc2R3CdpYyS2nWJc7ekyKVc-CfeP9p7ND31D0hdrbuQe75GnPv1S_w/exec", 0xA7).DecodeNS();
    }
    static NSString* AppSecret() {
        return AUTH_OBF("SAW_2026_SECRET_CHANGE_ME", 0x3D).DecodeNS();
    }
    static NSString* KeychainHwidService() {
        return AUTH_OBF("com.ladrraza.onestate.hwid", 0x4E).DecodeNS();
    }
    static NSString* KeychainHwidAccount() {
        return AUTH_OBF("device_hwid_v1", 0x5F).DecodeNS();
    }
    static NSString* KeychainEncKeyService() {
        return AUTH_OBF("com.ladrraza.onestate.vault", 0x62).DecodeNS();
    }
    static NSString* KeychainEncKeyAccount() {
        return AUTH_OBF("enc_key_v2", 0x74).DecodeNS();
    }
    static NSString* LegacyDefaultsKey() {
        return AUTH_OBF("saw_online_license_key_v1", 0x88).DecodeNS();
    }
    static NSString* SupportUrl() {
        return AUTH_OBF("https://t.me/LADRRAZA", 0x91).DecodeNS();
    }
    static constexpr double   kHeartbeatIntervalSec  = 600.0; // Re-verify online every 10 minutes
};

enum class StatusKind : int {
    Idle = 0,
    Verifying,
    Success,
    ErrEmptyKey,
    ErrInvalidCode,
    ErrPaused,
    ErrExpired,
    ErrDeviceMismatch,
    ErrUnauthorized,
    ErrNetwork,
    ErrBadResponse,
    ErrMitmProxy,
    ErrTamper,
    ErrClockSkew,
    InfoCopied,
    InfoPasted,
    InfoLoggedOut
};

// Obfuscated Split Cryptographic Session Vault (5 independent XOR-masked HMAC-SHA256 shards)
struct SessionVault {
    static constexpr uint64_t kMask0     = 0x9E3779B97F4A7C15ULL;
    static constexpr uint64_t kMask1     = 0xBF58476D1CE4E5B9ULL;
    static constexpr uint64_t kMask2     = 0x94D049BB133111EBULL;
    static constexpr uint64_t kMask3     = 0xD6E8FEB86659FD93ULL;
    static constexpr uint64_t kSealMagic = 0x517CC1B727220A95ULL;
    static constexpr uint32_t kStateOpen = 0xA5C37E91U;

    uint8_t runtimeKey[32]{};
    bool    runtimeKeyReady = false;

    std::atomic<uint64_t> shard0{0};
    std::atomic<uint64_t> shard1{0};
    std::atomic<uint64_t> shard2{0};
    std::atomic<uint64_t> shard3{0};
    std::atomic<uint64_t> sealXor{0};
    std::atomic<uint32_t> stateToken{0};
    std::atomic<uint32_t> tamperFlags{0};
    std::atomic<int64_t>  expiryDayNum{0};
    std::atomic<int64_t>  verifiedServerDay{0};
    std::atomic<double>   verifiedMonoAt{0.0};

    std::mutex  metaMutex;
    std::string activeHwid;
    std::string activeKeyHash;
    std::string activeExpiry;
    std::string inputKey;
    std::string serverRawError;

    std::atomic<bool>       inFlight{false};
    std::atomic<bool>       autoLoginStarted{false};
    std::atomic<bool>       maskKeyInput{false};
    std::atomic<StatusKind> status{StatusKind::Idle};
    std::atomic<double>     statusAt{0.0};
    std::atomic<double>     lastHeartbeatAt{0.0};
    std::atomic<uint32_t>   failShakeCounter{0};
    std::atomic<double>     lastShakeAt{0.0};
};

inline SessionVault g_vault{};

inline void EnsureRuntimeEntropy() {
    if (g_vault.runtimeKeyReady) return;
    if (SecRandomCopyBytes(kSecRandomDefault, sizeof(g_vault.runtimeKey), g_vault.runtimeKey) != errSecSuccess) {
        uint64_t seed = static_cast<uint64_t>(CACurrentMediaTime() * 1e9) ^ (static_cast<uint64_t>(getpid()) << 21) ^ 0xCAFEBABE1337C0DEULL;
        for (size_t i = 0; i < sizeof(g_vault.runtimeKey); ++i) {
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            g_vault.runtimeKey[i] = static_cast<uint8_t>((seed >> 24) & 0xFFu);
        }
    }
    g_vault.runtimeKeyReady = true;
}

inline uint64_t ComputeBrandFingerprint() {
    std::string bp = ThemeEngine::kBrandPill ? ThemeEngine::kBrandPill : "";
    std::string cu = ThemeEngine::kCommunityUrl ? ThemeEngine::kCommunityUrl : "";
    return AuthCrypto::Fnv1a64Str(bp) ^ (AuthCrypto::Fnv1a64Str(cu) * 0x100000001B3ULL);
}

// Runtime anti-debug & anti-cracking-tool scan (jailbreak-injector safe: NEVER flags ElleKit/Substitute/Substrate!)
inline uint32_t RunAntiTamperScan() {
    uint32_t flags = 0;

    // 1) Debugger check (P_TRACED via sysctl)
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    struct kinfo_proc info{};
    size_t sz = sizeof(info);
    if (sysctl(mib, 4, &info, &sz, nullptr, 0) == 0) {
        if ((info.kp_proc.p_flag & P_TRACED) != 0) flags |= 0x01u;
    }

    // 2) Cracking / SSL-unpinning / Frida dyld scan (strictly cracking/MITM tools, NOT ElleKit/Substitute)
    static const std::string kBadMods[7] = {
        AUTH_OBF("fridagadget", 0x11).Decode(),
        AUTH_OBF("frida-agent", 0x22).Decode(),
        AUTH_OBF("sslkillswitch", 0x33).Decode(),
        AUTH_OBF("cycript", 0x44).Decode(),
        AUTH_OBF("libcycript", 0x55).Decode(),
        AUTH_OBF("trustme", 0x66).Decode(),
        AUTH_OBF("objection", 0x77).Decode()
    };
    uint32_t count = _dyld_image_count();
    for (uint32_t i = 0; i < count; ++i) {
        const char* img = _dyld_get_image_name(i);
        if (!img) continue;
        std::string low(img);
        for (char& ch : low) {
            if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        }
        for (const auto& bm : kBadMods) {
            if (!bm.empty() && low.find(bm) != std::string::npos) {
                flags |= 0x02u;
                break;
            }
        }
        if (flags & 0x02u) break;
    }

    // 3) Brand integrity check (detects hex-edited rebrand cracks)
    static const uint64_t kExpectedPill = AuthCrypto::Fnv1a64Str(AUTH_OBF("SAW", 0xD1).Decode());
    static const uint64_t kExpectedComm = AuthCrypto::Fnv1a64Str(AUTH_OBF("https://t.me/LADRRAZA", 0xD2).Decode());
    if (!ThemeEngine::kBrandPill || AuthCrypto::Fnv1a64(ThemeEngine::kBrandPill, std::strlen(ThemeEngine::kBrandPill)) != kExpectedPill ||
        !ThemeEngine::kCommunityUrl || AuthCrypto::Fnv1a64(ThemeEngine::kCommunityUrl, std::strlen(ThemeEngine::kCommunityUrl)) != kExpectedComm) {
        flags |= 0x04u;
    }

    g_vault.tamperFlags.store(flags, std::memory_order_release);
    return flags;
}

inline int64_t ParseYmdToDayNum(const std::string& ymd) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(ymd.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return -1;
    if (y < 2020 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) return -1;
    return static_cast<int64_t>(y) * 10000LL + static_cast<int64_t>(m) * 100LL + static_cast<int64_t>(d);
}

inline int64_t DateToUtcDayNum(NSDate* date) {
    if (!date) return -1;
    NSCalendar* cal = [[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
    cal.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"GMT"];
    NSDateComponents* c = [cal components:(NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay) fromDate:date];
    return static_cast<int64_t>(c.year) * 10000LL + static_cast<int64_t>(c.month) * 100LL + static_cast<int64_t>(c.day);
}

// Persistent Device HWID (exact OneStateOffline/Tweak.xm format: IOS-<16 hex chars> in Keychain)
inline std::string GetOrCreateDeviceHWID() {
    static std::mutex s_hwidMu;
    static std::string s_cachedHwid;
    {
        std::lock_guard<std::mutex> lk(s_hwidMu);
        if (s_cachedHwid.size() >= 8) return s_cachedHwid;
    }

    NSString* svc = Config::KeychainHwidService();
    NSString* acc = Config::KeychainHwidAccount();
    NSDictionary* query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: svc,
        (__bridge id)kSecAttrAccount: acc,
        (__bridge id)kSecReturnData: @YES,
        (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitOne
    };
    CFTypeRef result = NULL;
    OSStatus st = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    NSString* found = nil;
    if (st == errSecSuccess && result != NULL) {
        NSData* data = (__bridge_transfer NSData*)result;
        NSString* existing = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        if (existing.length >= 8) found = existing;
    }

    if (!found) {
        NSString* idfv = [[UIDevice currentDevice].identifierForVendor UUIDString];
        NSString* raw = (idfv.length > 0) ? idfv : [[NSUUID UUID] UUIDString];
        NSString* clean = [raw stringByReplacingOccurrencesOfString:@"-" withString:@""];
        NSUInteger take = (clean.length < 16) ? clean.length : 16;
        found = [NSString stringWithFormat:@"IOS-%@", [clean substringToIndex:take]];

        NSData* hwidData = [found dataUsingEncoding:NSUTF8StringEncoding];
        NSDictionary* delQ = @{
            (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService: svc,
            (__bridge id)kSecAttrAccount: acc
        };
        SecItemDelete((__bridge CFDictionaryRef)delQ);
        NSDictionary* addQ = @{
            (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService: svc,
            (__bridge id)kSecAttrAccount: acc,
            (__bridge id)kSecValueData: hwidData,
            (__bridge id)kSecAttrAccessible: (__bridge id)kSecAttrAccessibleAfterFirstUnlock
        };
        SecItemAdd((__bridge CFDictionaryRef)addQ, NULL);
    }

    std::string res = found ? std::string(found.UTF8String) : "IOS-UNKNOWN000000";
    {
        std::lock_guard<std::mutex> lk(s_hwidMu);
        s_cachedHwid = res;
    }
    return res;
}

// Encrypted Keychain storage for saved license key (XOR stream derived from SHA-256(HWID + pepper))
inline std::string DeriveKeychainMask(const std::string& hwid) {
    std::string pep = AUTH_OBF("LDR_SAW_VAULT_2026_KDF", 0xC9).Decode();
    std::string raw = AuthCrypto::Sha256Hex(hwid + "|" + pep);
    if (!pep.empty()) AuthCrypto::SecureZero(&pep[0], pep.size());
    return raw;
}

inline void SaveEncryptedKeyToKeychain(const std::string& plainKey) {
    std::string hwid = GetOrCreateDeviceHWID();
    std::string mask = DeriveKeychainMask(hwid);
    NSMutableData* enc = [NSMutableData dataWithLength:plainKey.size()];
    uint8_t* dst = reinterpret_cast<uint8_t*>(enc.mutableBytes);
    for (size_t i = 0; i < plainKey.size(); ++i) {
        dst[i] = static_cast<uint8_t>(plainKey[i]) ^ static_cast<uint8_t>(mask[i % mask.size()]) ^ static_cast<uint8_t>((i * 29u + 0x5Au) & 0xFFu);
    }

    NSString* svc = Config::KeychainEncKeyService();
    NSString* acc = Config::KeychainEncKeyAccount();
    NSDictionary* delQ = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: svc,
        (__bridge id)kSecAttrAccount: acc
    };
    SecItemDelete((__bridge CFDictionaryRef)delQ);
    NSDictionary* addQ = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: svc,
        (__bridge id)kSecAttrAccount: acc,
        (__bridge id)kSecValueData: enc,
        (__bridge id)kSecAttrAccessible: (__bridge id)kSecAttrAccessibleAfterFirstUnlock
    };
    SecItemAdd((__bridge CFDictionaryRef)addQ, NULL);
    // Remove any legacy plaintext NSUserDefaults entry
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:Config::LegacyDefaultsKey()];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

inline std::string LoadEncryptedKeyFromKeychain() {
    NSString* svc = Config::KeychainEncKeyService();
    NSString* acc = Config::KeychainEncKeyAccount();
    NSDictionary* query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: svc,
        (__bridge id)kSecAttrAccount: acc,
        (__bridge id)kSecReturnData: @YES,
        (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitOne
    };
    CFTypeRef res = NULL;
    if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &res) == errSecSuccess && res != NULL) {
        NSData* data = (__bridge_transfer NSData*)res;
        if (data.length > 0 && data.length <= 256) {
            std::string hwid = GetOrCreateDeviceHWID();
            std::string mask = DeriveKeychainMask(hwid);
            const uint8_t* src = reinterpret_cast<const uint8_t*>(data.bytes);
            std::string out(data.length, '\0');
            for (size_t i = 0; i < data.length; ++i) {
                out[i] = static_cast<char>(src[i] ^ static_cast<uint8_t>(mask[i % mask.size()]) ^ static_cast<uint8_t>((i * 29u + 0x5Au) & 0xFFu));
            }
            return out;
        }
    }
    // Seamless one-time migration from legacy OneStateOffline NSUserDefaults key (then wipes plaintext)
    NSString* legacy = [[NSUserDefaults standardUserDefaults] stringForKey:Config::LegacyDefaultsKey()];
    if (legacy && legacy.length > 0) {
        std::string k = legacy.UTF8String;
        SaveEncryptedKeyToKeychain(k);
        return k;
    }
    return "";
}

inline void ClearSavedKeyFromKeychain() {
    NSDictionary* delQ = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: Config::KeychainEncKeyService(),
        (__bridge id)kSecAttrAccount: Config::KeychainEncKeyAccount()
    };
    SecItemDelete((__bridge CFDictionaryRef)delQ);
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:Config::LegacyDefaultsKey()];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

// Compute canonical vault message bound to HWID + keyHash + expiry + serverDay + brand fingerprint
inline std::string BuildVaultPayload(const std::string& hwid, const std::string& keyHash,
                                     const std::string& expiry, int64_t serverDay) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "|%lld|%016llx",
                  static_cast<long long>(serverDay),
                  static_cast<unsigned long long>(ComputeBrandFingerprint()));
    return hwid + "|" + keyHash + "|" + expiry + std::string(buf);
}

inline void RevokeSession(StatusKind reason) {
    g_vault.stateToken.store(0, std::memory_order_release);
    g_vault.shard0.store(0, std::memory_order_release);
    g_vault.shard1.store(0, std::memory_order_release);
    g_vault.shard2.store(0, std::memory_order_release);
    g_vault.shard3.store(0, std::memory_order_release);
    g_vault.sealXor.store(0, std::memory_order_release);
    g_vault.expiryDayNum.store(0, std::memory_order_release);
    g_vault.verifiedServerDay.store(0, std::memory_order_release);
    g_vipUnlocked = false;
    g_isAuthenticated = false;
    g_licenseTier = 0;
    g_vault.status.store(reason, std::memory_order_release);
    g_vault.statusAt.store(CACurrentMediaTime(), std::memory_order_release);
}

inline void SealVerifiedSession(const std::string& hwid, const std::string& plainKey,
                                const std::string& expiry, int64_t serverDay, int64_t expDay) {
    EnsureRuntimeEntropy();
    std::string keyHash = AuthCrypto::Sha256Hex(plainKey);
    std::string payload = BuildVaultPayload(hwid, keyHash, expiry, serverDay);
    uint8_t mac[32]{};
    AuthCrypto::HmacSha256(g_vault.runtimeKey, sizeof(g_vault.runtimeKey), payload, mac);

    uint64_t w0 = AuthCrypto::LoadU64LE(mac + 0);
    uint64_t w1 = AuthCrypto::LoadU64LE(mac + 8);
    uint64_t w2 = AuthCrypto::LoadU64LE(mac + 16);
    uint64_t w3 = AuthCrypto::LoadU64LE(mac + 24);
    AuthCrypto::SecureZero(mac, sizeof(mac));
    if (!payload.empty()) AuthCrypto::SecureZero(&payload[0], payload.size());

    {
        std::lock_guard<std::mutex> lk(g_vault.metaMutex);
        g_vault.activeHwid     = hwid;
        g_vault.activeKeyHash  = keyHash;
        g_vault.activeExpiry   = expiry;
        g_vault.serverRawError.clear();
    }

    g_vault.shard0.store(w0 ^ SessionVault::kMask0, std::memory_order_release);
    g_vault.shard1.store(w1 ^ SessionVault::kMask1, std::memory_order_release);
    g_vault.shard2.store(w2 ^ SessionVault::kMask2, std::memory_order_release);
    g_vault.shard3.store(w3 ^ SessionVault::kMask3, std::memory_order_release);
    g_vault.sealXor.store((w0 ^ w1 ^ w2 ^ w3) ^ SessionVault::kSealMagic, std::memory_order_release);
    g_vault.expiryDayNum.store(expDay, std::memory_order_release);
    g_vault.verifiedServerDay.store(serverDay, std::memory_order_release);
    double now = CACurrentMediaTime();
    g_vault.verifiedMonoAt.store(now, std::memory_order_release);
    g_vault.lastHeartbeatAt.store(now, std::memory_order_release);
    g_vault.stateToken.store(SessionVault::kStateOpen ^ static_cast<uint32_t>(w0 & 0xFFFFFFFFULL), std::memory_order_release);

    // Mirror to decoys solely so crackers inspecting them don't notice mismatch
    g_vipUnlocked = true;
    g_isAuthenticated = true;
    g_licenseTier = 3;

    g_vault.status.store(StatusKind::Success, std::memory_order_release);
    g_vault.statusAt.store(now, std::memory_order_release);
}

// Fast 5-checkpoint inline gate (called in UI, TickRuntime, DrawOverlayVisuals, KillAround)
__attribute__((always_inline))
inline bool IsVerifiedFast() {
    if (!g_vault.runtimeKeyReady) return false;
    if (g_vault.tamperFlags.load(std::memory_order_acquire) != 0) return false;
    uint64_t w0 = g_vault.shard0.load(std::memory_order_acquire) ^ SessionVault::kMask0;
    uint64_t w1 = g_vault.shard1.load(std::memory_order_acquire) ^ SessionVault::kMask1;
    uint64_t w2 = g_vault.shard2.load(std::memory_order_acquire) ^ SessionVault::kMask2;
    uint64_t w3 = g_vault.shard3.load(std::memory_order_acquire) ^ SessionVault::kMask3;
    if ((w0 | w1 | w2 | w3) == 0) return false;
    uint64_t expectedSeal = (w0 ^ w1 ^ w2 ^ w3) ^ SessionVault::kSealMagic;
    if (!AuthCrypto::ConstantTimeEq64(g_vault.sealXor.load(std::memory_order_acquire), expectedSeal)) return false;
    uint32_t expectedTok = SessionVault::kStateOpen ^ static_cast<uint32_t>(w0 & 0xFFFFFFFFULL);
    if (g_vault.stateToken.load(std::memory_order_acquire) != expectedTok) return false;
    int64_t expDay = g_vault.expiryDayNum.load(std::memory_order_acquire);
    int64_t srvDay = g_vault.verifiedServerDay.load(std::memory_order_acquire);
    if (expDay <= 0 || srvDay <= 0 || srvDay > expDay) return false;
    return true;
}

// Full cryptographic HMAC re-verification (runs periodically every ~3 seconds so memory flips are caught!)
inline bool VerifyFullCryptographicIntegrity() {
    if (!IsVerifiedFast()) return false;
    if (RunAntiTamperScan() != 0) {
        RevokeSession(StatusKind::ErrTamper);
        return false;
    }

    std::string hwid, keyHash, expiry;
    {
        std::lock_guard<std::mutex> lk(g_vault.metaMutex);
        hwid    = g_vault.activeHwid;
        keyHash = g_vault.activeKeyHash;
        expiry  = g_vault.activeExpiry;
    }
    if (hwid.empty() || keyHash.empty() || expiry.empty()) {
        RevokeSession(StatusKind::ErrTamper);
        return false;
    }
    int64_t srvDay = g_vault.verifiedServerDay.load(std::memory_order_acquire);
    std::string payload = BuildVaultPayload(hwid, keyHash, expiry, srvDay);
    uint8_t mac[32]{};
    AuthCrypto::HmacSha256(g_vault.runtimeKey, sizeof(g_vault.runtimeKey), payload, mac);
    uint64_t e0 = AuthCrypto::LoadU64LE(mac + 0);
    uint64_t e1 = AuthCrypto::LoadU64LE(mac + 8);
    uint64_t e2 = AuthCrypto::LoadU64LE(mac + 16);
    uint64_t e3 = AuthCrypto::LoadU64LE(mac + 24);
    AuthCrypto::SecureZero(mac, sizeof(mac));
    if (!payload.empty()) AuthCrypto::SecureZero(&payload[0], payload.size());

    uint64_t w0 = g_vault.shard0.load(std::memory_order_acquire) ^ SessionVault::kMask0;
    uint64_t w1 = g_vault.shard1.load(std::memory_order_acquire) ^ SessionVault::kMask1;
    uint64_t w2 = g_vault.shard2.load(std::memory_order_acquire) ^ SessionVault::kMask2;
    uint64_t w3 = g_vault.shard3.load(std::memory_order_acquire) ^ SessionVault::kMask3;

    bool ok = AuthCrypto::ConstantTimeEq64(w0, e0) &&
              AuthCrypto::ConstantTimeEq64(w1, e1) &&
              AuthCrypto::ConstantTimeEq64(w2, e2) &&
              AuthCrypto::ConstantTimeEq64(w3, e3);
    if (!ok) {
        RevokeSession(StatusKind::ErrTamper);
        return false;
    }
    return true;
}

inline StatusKind MapServerErrorCode(NSString* code) {
    if (!code) return StatusKind::ErrBadResponse;
    if ([code isEqualToString:AUTH_OBF("INVALID_CODE", 0x12).DecodeNS()])      return StatusKind::ErrInvalidCode;
    if ([code isEqualToString:AUTH_OBF("PAUSED", 0x23).DecodeNS()])            return StatusKind::ErrPaused;
    if ([code isEqualToString:AUTH_OBF("EXPIRED", 0x34).DecodeNS()])           return StatusKind::ErrExpired;
    if ([code isEqualToString:AUTH_OBF("DEVICE_MISMATCH", 0x45).DecodeNS()])   return StatusKind::ErrDeviceMismatch;
    if ([code isEqualToString:AUTH_OBF("UNAUTHORIZED", 0x56).DecodeNS()])      return StatusKind::ErrUnauthorized;
    if ([code isEqualToString:AUTH_OBF("MISSING_FIELDS", 0x67).DecodeNS()])    return StatusKind::ErrEmptyKey;
    return StatusKind::ErrBadResponse;
}

inline void TriggerShake() {
    g_vault.failShakeCounter.fetch_add(1, std::memory_order_relaxed);
    g_vault.lastShakeAt.store(CACurrentMediaTime(), std::memory_order_relaxed);
}

// Online verification against GoogleAppsScript_Code.gs (anti-proxy + anti-MITM + server-date check)
inline void VerifyOnline(const std::string& rawKey, bool isSilentHeartbeat = false) {
    if (g_vault.inFlight.exchange(true, std::memory_order_acq_rel)) return;

    EnsureRuntimeEntropy();
    if (RunAntiTamperScan() != 0) {
        g_vault.inFlight.store(false, std::memory_order_release);
        RevokeSession(StatusKind::ErrTamper);
        TriggerShake();
        return;
    }

    NSString* nsRaw = [NSString stringWithUTF8String:rawKey.c_str()] ?: @"";
    NSString* trimmed = [nsRaw stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (trimmed.length == 0) {
        g_vault.inFlight.store(false, std::memory_order_release);
        if (!isSilentHeartbeat) {
            g_vault.status.store(StatusKind::ErrEmptyKey, std::memory_order_release);
            g_vault.statusAt.store(CACurrentMediaTime(), std::memory_order_release);
            TriggerShake();
        }
        return;
    }

    if (!isSilentHeartbeat) {
        g_vault.status.store(StatusKind::Verifying, std::memory_order_release);
        g_vault.statusAt.store(CACurrentMediaTime(), std::memory_order_release);
    }

    std::string hwidStd = GetOrCreateDeviceHWID();
    NSString* hwidNS = [NSString stringWithUTF8String:hwidStd.c_str()] ?: @"";
    NSString* endpoint = Config::EndpointURL();
    NSString* secret   = Config::AppSecret();

    NSURLComponents* comps = [NSURLComponents componentsWithString:endpoint];
    comps.queryItems = @[
        [NSURLQueryItem queryItemWithName:AUTH_OBF("code", 0x1A).DecodeNS()   value:trimmed],
        [NSURLQueryItem queryItemWithName:AUTH_OBF("hwid", 0x2B).DecodeNS()   value:hwidNS],
        [NSURLQueryItem queryItemWithName:AUTH_OBF("secret", 0x3C).DecodeNS() value:secret]
    ];
    NSURL* url = comps.URL;
    if (!url) {
        g_vault.inFlight.store(false, std::memory_order_release);
        if (!isSilentHeartbeat) {
            g_vault.status.store(StatusKind::ErrBadResponse, std::memory_order_release);
            TriggerShake();
        }
        return;
    }

    // Ephemeral session bypassing ALL system HTTP/HTTPS/SOCKS proxies (blocks Charles/Proxyman/Burp)
    NSURLSessionConfiguration* cfg = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    cfg.connectionProxyDictionary = @{};
    cfg.URLCache = nil;
    cfg.HTTPCookieStorage = nil;
    cfg.requestCachePolicy = NSURLRequestReloadIgnoringLocalAndRemoteCacheData;
    cfg.timeoutIntervalForRequest = 14.0;
    cfg.timeoutIntervalForResource = 22.0;

    AuthPinSessionDelegate* del = [[AuthPinSessionDelegate alloc] init];
    NSURLSession* session = [NSURLSession sessionWithConfiguration:cfg delegate:del delegateQueue:nil];

    NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:url
                                                       cachePolicy:NSURLRequestReloadIgnoringLocalAndRemoteCacheData
                                                   timeoutInterval:14.0];
    req.HTTPMethod = @"GET";
    [req setValue:@"no-cache" forHTTPHeaderField:@"Cache-Control"];

    std::string cleanKeyStd = trimmed.UTF8String ? std::string(trimmed.UTF8String) : std::string();

    NSURLSessionDataTask* task = [session dataTaskWithRequest:req
                                            completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
        [session finishTasksAndInvalidate];

        dispatch_async(dispatch_get_main_queue(), ^{
            g_vault.inFlight.store(false, std::memory_order_release);

            if (del.blockedMitm) {
                RevokeSession(StatusKind::ErrMitmProxy);
                TriggerShake();
                return;
            }
            if (error || !data || ![response isKindOfClass:[NSHTTPURLResponse class]]) {
                if (!isSilentHeartbeat) {
                    g_vault.status.store(StatusKind::ErrNetwork, std::memory_order_release);
                    g_vault.statusAt.store(CACurrentMediaTime(), std::memory_order_release);
                    TriggerShake();
                } else {
                    // On silent heartbeat transient network glitch, retry later without booting active player
                    g_vault.lastHeartbeatAt.store(CACurrentMediaTime(), std::memory_order_release);
                }
                return;
            }

            NSHTTPURLResponse* httpResp = (NSHTTPURLResponse*)response;
            if (![AuthPinSessionDelegate isAllowedHost:httpResp.URL.host]) {
                RevokeSession(StatusKind::ErrMitmProxy);
                TriggerShake();
                return;
            }

            // Parse authoritative UTC server date from HTTP Date header (blocks device clock rollback!)
            int64_t serverDay = DateToUtcDayNum([NSDate date]);
            NSString* dateHdr = httpResp.allHeaderFields[@"Date"];
            if (!dateHdr) dateHdr = httpResp.allHeaderFields[@"date"];
            if ([dateHdr isKindOfClass:[NSString class]] && dateHdr.length > 0) {
                NSDateFormatter* df = [[NSDateFormatter alloc] init];
                df.locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US_POSIX"];
                df.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"GMT"];
                df.dateFormat = @"EEE, dd MMM yyyy HH:mm:ss zzz";
                NSDate* srvDate = [df dateFromString:dateHdr];
                if (srvDate) {
                    serverDay = DateToUtcDayNum(srvDate);
                }
            }

            NSError* jsonErr = nil;
            id obj = [NSJSONSerialization JSONObjectWithData:data options:0 error:&jsonErr];
            if (jsonErr || ![obj isKindOfClass:[NSDictionary class]]) {
                if (!isSilentHeartbeat) {
                    g_vault.status.store(StatusKind::ErrBadResponse, std::memory_order_release);
                    g_vault.statusAt.store(CACurrentMediaTime(), std::memory_order_release);
                    TriggerShake();
                }
                return;
            }

            NSDictionary* dict = (NSDictionary*)obj;
            NSString* kOk     = AUTH_OBF("ok", 0x41).DecodeNS();
            NSString* kExp    = AUTH_OBF("expiry", 0x42).DecodeNS();
            NSString* kHwid   = AUTH_OBF("hwid", 0x43).DecodeNS();
            NSString* kErr    = AUTH_OBF("error", 0x44).DecodeNS();

            BOOL ok = [dict[kOk] isKindOfClass:[NSNumber class]] && [dict[kOk] boolValue];
            NSString* expNS = [dict[kExp] isKindOfClass:[NSString class]] ? dict[kExp] : @"";
            NSString* respHwid = [dict[kHwid] isKindOfClass:[NSString class]] ? dict[kHwid] : @"";

            if (!ok) {
                NSString* errCode = [dict[kErr] isKindOfClass:[NSString class]] ? dict[kErr] : @"ERR";
                {
                    std::lock_guard<std::mutex> lk(g_vault.metaMutex);
                    g_vault.serverRawError = errCode.UTF8String ?: "";
                }
                StatusKind sk = MapServerErrorCode(errCode);
                RevokeSession(sk);
                TriggerShake();
                return;
            }

            // Cross-verify server returned HWID matches our hardware Keychain HWID (stops static replay)
            if (respHwid.length > 0 && ![respHwid isEqualToString:hwidNS]) {
                RevokeSession(StatusKind::ErrDeviceMismatch);
                TriggerShake();
                return;
            }

            std::string expStd = expNS.UTF8String ?: "";
            int64_t expDay = ParseYmdToDayNum(expStd);
            if (expDay <= 0 || serverDay > expDay) {
                RevokeSession(StatusKind::ErrExpired);
                TriggerShake();
                return;
            }

            // All checks passed -> encrypt key in Keychain & seal 5-shard HMAC-SHA256 session vault!
            SaveEncryptedKeyToKeychain(cleanKeyStd);
            {
                std::lock_guard<std::mutex> lk(g_vault.metaMutex);
                g_vault.inputKey = cleanKeyStd;
            }
            SealVerifiedSession(hwidStd, cleanKeyStd, expStd, serverDay, expDay);
        });
    }];
    [task resume];
}

// Startup auto-login: loads encrypted key from Keychain and ALWAYS verifies online
inline void BootAutoLoginOnce() {
    if (g_vault.autoLoginStarted.exchange(true, std::memory_order_acq_rel)) return;
    EnsureRuntimeEntropy();
    RunAntiTamperScan();
    std::string saved = LoadEncryptedKeyFromKeychain();
    if (!saved.empty()) {
        {
            std::lock_guard<std::mutex> lk(g_vault.metaMutex);
            g_vault.inputKey = saved;
        }
        VerifyOnline(saved, false);
    }
}

// Periodic background integrity + online heartbeat check (called from Tweak.mm)
inline void TickIntegrityAndHeartbeat() {
    static double s_lastFullCheck = 0.0;
    double now = CACurrentMediaTime();
    if (IsVerifiedFast()) {
        if (now - s_lastFullCheck >= 3.0) {
            s_lastFullCheck = now;
            if (!VerifyFullCryptographicIntegrity()) return;
        }
        double lastHb = g_vault.lastHeartbeatAt.load(std::memory_order_relaxed);
        if (lastHb > 0.0 && (now - lastHb) >= Config::kHeartbeatIntervalSec && !g_vault.inFlight.load(std::memory_order_relaxed)) {
            std::string saved = LoadEncryptedKeyFromKeychain();
            if (!saved.empty()) {
                VerifyOnline(saved, true);
            } else {
                RevokeSession(StatusKind::InfoLoggedOut);
            }
        }
    }
}

inline void Logout() {
    ClearSavedKeyFromKeychain();
    {
        std::lock_guard<std::mutex> lk(g_vault.metaMutex);
        g_vault.inputKey.clear();
        g_vault.activeExpiry.clear();
        g_vault.activeKeyHash.clear();
    }
    RevokeSession(StatusKind::InfoLoggedOut);
}

inline std::string GetActiveExpiry() {
    std::lock_guard<std::mutex> lk(g_vault.metaMutex);
    return g_vault.activeExpiry;
}

// Native iOS Dark Alert Keyboard Prompt (seamless touch typing for ImGui key field)
inline void PromptNativeKeyInput(UIViewController* hostVC) {
    if (!hostVC) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (hostVC.presentedViewController) return;
        bool ar = Lang::IsArabic();
        NSString* title = ar ? @"إدخال مفتاح التفعيل" : @"Enter License Key";
        NSString* msg   = ar ? @"أدخل كود الاشتراك الخاص بك أو الصقه مباشرة" : @"Type or paste your online license key below";
        NSString* ph    = ar ? @"مثال: SAW-XXXX-XXXX" : @"e.g. SAW-XXXX-XXXX";
        NSString* okTxt = ar ? @"تفعيل الآن" : @"Activate";
        NSString* svTxt = ar ? @"حفظ فقط" : @"Done";
        NSString* clTxt = ar ? @"إلغاء" : @"Cancel";

        std::string curKey;
        {
            std::lock_guard<std::mutex> lk(g_vault.metaMutex);
            curKey = g_vault.inputKey;
        }

        UIAlertController* ac = [UIAlertController alertControllerWithTitle:title
                                                                    message:msg
                                                             preferredStyle:UIAlertControllerStyleAlert];
        if (@available(iOS 13.0, *)) {
            ac.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
        }
        [ac addTextFieldWithConfigurationHandler:^(UITextField* tf) {
            tf.placeholder = ph;
            tf.text = [NSString stringWithUTF8String:curKey.c_str()] ?: @"";
            tf.autocapitalizationType = UITextAutocapitalizationTypeNone;
            tf.autocorrectionType = UITextAutocorrectionTypeNo;
            tf.spellCheckingType = UITextSpellCheckingTypeNo;
            tf.clearButtonMode = UITextFieldViewModeWhileEditing;
            tf.font = [UIFont monospacedSystemFontOfSize:14.0 weight:UIFontWeightSemibold];
        }];

        [ac addAction:[UIAlertAction actionWithTitle:clTxt style:UIAlertActionStyleCancel handler:nil]];
        [ac addAction:[UIAlertAction actionWithTitle:svTxt style:UIAlertActionStyleDefault handler:^(UIAlertAction* action) {
            NSString* txt = ac.textFields.firstObject.text ?: @"";
            NSString* trimmed = [txt stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
            std::lock_guard<std::mutex> lk(g_vault.metaMutex);
            g_vault.inputKey = trimmed.UTF8String ?: "";
        }]];
        [ac addAction:[UIAlertAction actionWithTitle:okTxt style:UIAlertActionStyleDefault handler:^(UIAlertAction* action) {
            NSString* txt = ac.textFields.firstObject.text ?: @"";
            NSString* trimmed = [txt stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
            std::string k = trimmed.UTF8String ?: "";
            {
                std::lock_guard<std::mutex> lk(g_vault.metaMutex);
                g_vault.inputKey = k;
            }
            VerifyOnline(k, false);
        }]];

        [hostVC presentViewController:ac animated:YES completion:nil];
    });
}

// ============================================================================
// 4. Redesigned Luxury Cyber-Glass Pro Login Portal (ImGui + Metal Vector Art)
// ============================================================================
inline void DrawCyberShieldEmblem(ImDrawList* dl, ImVec2 center, float radius, double now, bool verifying, bool isErr) {
    float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now * 3.2));
    ImU32 mainC = isErr ? IM_COL32(255, 72, 98, 240)
                        : (verifying ? IM_COL32(255, 198, 65, 240) : ThemeEngine::GetSecondaryNeon(0.95f));
    ImU32 glowC = isErr ? IM_COL32(255, 55, 85, static_cast<int>(55 + 45 * pulse))
                        : (verifying ? IM_COL32(255, 185, 50, static_cast<int>(55 + 45 * pulse))
                                     : ThemeEngine::GetAccent(0.20f + 0.16f * pulse));

    // Outer ambient bloom rings
    dl->AddCircleFilled(center, radius + 9.0f, glowC, 36);
    dl->AddCircle(center, radius + 5.0f, mainC, 36, 1.1f);

    // Rotating tech arc when verifying, subtle steady orbit when idle
    float ang0 = static_cast<float>(now * (verifying ? 4.8 : 1.1));
    dl->PathClear();
    dl->PathArcTo(center, radius + 9.0f, ang0, ang0 + (verifying ? 2.4f : 1.35f), 20);
    dl->PathStroke(mainC, 0, verifying ? 2.6f : 1.6f);

    // 6-sided Cyber Hexagon Shield
    ImVec2 hex[6];
    for (int i = 0; i < 6; ++i) {
        float a = -1.5707963f + i * 1.0471975f;
        hex[i] = ImVec2(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius);
    }
    dl->AddConvexPolyFilled(hex, 6, IM_COL32(14, 17, 30, 245));
    dl->AddPolyline(hex, 6, mainC, ImDrawFlags_Closed, 2.0f);

    // Inner padlock / key glyph inside shield
    float rLock = radius * 0.34f;
    ImVec2 shackleC(center.x, center.y - rLock * 0.45f);
    dl->PathClear();
    dl->PathArcTo(shackleC, rLock * 0.68f, 3.1415926f, 6.2831853f, 12);
    dl->PathStroke(IM_COL32(244, 248, 255, 240), 0, 1.9f);
    dl->AddRectFilled(ImVec2(center.x - rLock, center.y - rLock * 0.25f),
                      ImVec2(center.x + rLock, center.y + rLock * 1.10f), mainC, 3.0f);
    dl->AddCircleFilled(ImVec2(center.x, center.y + rLock * 0.38f), 2.3f, IM_COL32(12, 14, 24, 255));
}

inline const char* LocalizedStatusText(StatusKind sk, bool& outIsErr, bool& outIsOk) {
    using Lang::T;
    outIsErr = false;
    outIsOk  = false;
    switch (sk) {
        case StatusKind::Idle:
            return T("Enter your online VIP key to unlock OneStateFull",
                     "أدخل مفتاح الاشتراك الأونلاين لتفعيل القائمة");
        case StatusKind::Verifying:
            return T("Verifying encrypted license with server...",
                     "جاري التحقق المشفر من الاشتراك مع السيرفر...");
        case StatusKind::Success:
            outIsOk = true;
            return T("License verified — access granted!",
                     "تم التحقق بنجاح — مرحباً بك!");
        case StatusKind::ErrEmptyKey:
            outIsErr = true;
            return T("Please enter or paste a license key first",
                     "يرجى إدخال أو لصق مفتاح التفعيل أولاً");
        case StatusKind::ErrInvalidCode:
            outIsErr = true;
            return T("Invalid license key — check your code",
                     "مفتاح التفعيل غير صحيح — تأكد من الكود");
        case StatusKind::ErrPaused:
            outIsErr = true;
            return T("This license key is paused by admin",
                     "هذا المفتاح موقوف مؤقتاً من الإدارة");
        case StatusKind::ErrExpired:
            outIsErr = true;
            return T("License key has expired — please renew",
                     "انتهت صلاحية الاشتراك — يرجى التجديد");
        case StatusKind::ErrDeviceMismatch:
            outIsErr = true;
            return T("Key is locked to another iPhone (HWID mismatch)",
                     "المفتاح مرتبط بجهاز آيفون آخر");
        case StatusKind::ErrUnauthorized:
            outIsErr = true;
            return T("Client secret rejected by server",
                     "خطأ في مفتاح التحقق مع السيرفر");
        case StatusKind::ErrNetwork:
            outIsErr = true;
            return T("Connection failed — check internet & retry",
                     "فشل الاتصال — تأكد من الإنترنت وحاول مجدداً");
        case StatusKind::ErrBadResponse:
            outIsErr = true;
            return T("Unexpected server response — try again",
                     "استجابة غير صالحة من السيرفر — أعد المحاولة");
        case StatusKind::ErrMitmProxy:
            outIsErr = true;
            return T("Blocked: Proxy / MITM inspection detected",
                     "تم الحظر: اكتشاف بروكسي أو محاولة فحص الاتصال");
        case StatusKind::ErrTamper:
            outIsErr = true;
            return T("Blocked: Security / binary integrity alert",
                     "تم الحظر: تنبيه حماية وفحص سلامة الملف");
        case StatusKind::ErrClockSkew:
            outIsErr = true;
            return T("Device date mismatch — enable automatic time",
                     "تاريخ الجهاز غير صحيح — فعّل التوقيت التلقائي");
        case StatusKind::InfoCopied:
            outIsOk = true;
            return T("Device HWID copied to clipboard",
                     "تم نسخ معرف الجهاز بنجاح");
        case StatusKind::InfoPasted:
            outIsOk = true;
            return T("Key pasted from clipboard — tap Activate",
                     "تم لصق الكود — اضغط تفعيل الاشتراك");
        case StatusKind::InfoLoggedOut:
            return T("Signed out — enter a license key to continue",
                     "تم تسجيل الخروج — أدخل كود التفعيل للمتابعة");
    }
    return "";
}

// Returns true if the user tapped Close "X"
inline bool DrawLoginDeck(float screenW, float screenH, int& languageMode, UIViewController* hostVC) {
    using Lang::T;
    Lang::SetLanguage(languageMode);
    ThemeEngine::ApplyProStyle();

    double now = CACurrentMediaTime();
    bool verifying = g_vault.inFlight.load(std::memory_order_acquire);
    StatusKind sk  = g_vault.status.load(std::memory_order_acquire);
    bool isErr = false, isOk = false;
    const char* statusMsg = LocalizedStatusText(sk, isErr, isOk);

    // Subtle horizontal shake offset on failed login attempt (350ms damped sine)
    float shakeX = 0.0f;
    double shakeElapsed = now - g_vault.lastShakeAt.load(std::memory_order_relaxed);
    if (shakeElapsed >= 0.0 && shakeElapsed < 0.36) {
        float damp = 1.0f - static_cast<float>(shakeElapsed / 0.36);
        shakeX = std::sin(static_cast<float>(shakeElapsed) * 52.0f) * 9.0f * damp;
    }

    float cardW = std::min(screenW * 0.90f, 446.0f);
    float cardH = std::min(screenH * 0.88f, 386.0f);
    ImVec2 winPos((screenW - cardW) * 0.5f + shakeX, (screenH - cardH) * 0.5f);

    // Dimmed cinematic backdrop behind login portal
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    bg->AddRectFilled(ImVec2(0, 0), ImVec2(screenW, screenH), IM_COL32(4, 6, 14, 158));

    ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    ImGuiWindowFlags wf = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;

    bool closeRequested = false;
    if (!ImGui::Begin("##SawProAuthPortal", nullptr, wf)) {
        ImGui::End();
        return false;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    // 1. Flagship iOS 18 Frosted Glass Backdrop (clean dark obsidian + subtle ambient tint)
    ThemeEngine::DrawNeon3DBackdrop(dl, wp, cardW, cardH, 0.0f, now, true);

    // 2. Top Header Row: EN/AR Glass Pill + Clean Close "X"
    ImGui::SetCursorScreenPos(ImVec2(wp.x + cardW - 96.0f, wp.y + 14.0f));
    static const std::string s_arAuthPill = Lang::ShapeArabicForImGui("عربي##authlang");
    const char* langPill = Lang::IsArabic() ? "EN##authlang" : s_arAuthPill.c_str();
    if (ThemeEngine::SecondaryPillButton(langPill, 52.0f, 28.0f, false)) {
        languageMode = (languageMode == 0) ? 1 : 0;
        Lang::SetLanguage(languageMode);
    }
    ImGui::SetCursorScreenPos(ImVec2(wp.x + cardW - 38.0f, wp.y + 14.0f));
    if (ThemeEngine::SecondaryPillButton("X##authcls", 28.0f, 28.0f, true)) {
        closeRequested = true;
    }

    // 3. Center Shield Emblem + Brand Header
    ImVec2 shieldCenter(wp.x + cardW * 0.5f, wp.y + 52.0f);
    DrawCyberShieldEmblem(dl, shieldCenter, 21.0f, now, verifying, isErr);

    const char* titleTxt = "LDR@SAW  •  VIP ACCESS";
    ImVec2 tSz = ImGui::CalcTextSize(titleTxt);
    dl->AddText(ImVec2(wp.x + (cardW - tSz.x) * 0.5f, wp.y + 84.0f), ThemeEngine::GetText(), titleTxt);

    // Security Telemetry Chip
    const char* badgeTxt = "ENCRYPTED  •  ANTI-MITM  •  HWID LOCKED";
    ImVec2 bSz = ImGui::CalcTextSize(badgeTxt);
    float chipW = std::min(cardW - 36.0f, bSz.x + 24.0f);
    ImVec2 chipA(wp.x + (cardW - chipW) * 0.5f, wp.y + 104.0f);
    ImVec2 chipB(chipA.x + chipW, chipA.y + 20.0f);
    dl->AddRectFilled(chipA, chipB, ThemeEngine::GetAccent(0.16f), 10.0f);
    dl->AddText(ImVec2(chipA.x + (chipW - bSz.x) * 0.5f, chipA.y + 3.0f), ThemeEngine::GetAccent(1.0f), badgeTxt);

    // 4. License Key Card (Tap-to-Type Native iOS Keyboard + One-Tap Paste + Show/Hide + Clear)
    float padX = 22.0f;
    float innerW = cardW - padX * 2.0f;
    float curY = wp.y + 136.0f;

    std::string curKey;
    {
        std::lock_guard<std::mutex> lk(g_vault.metaMutex);
        curKey = g_vault.inputKey;
    }

    // Key Input Well (iOS Grouped Glass Input — tapping opens native iOS keyboard)
    float wellH = 44.0f;
    ImVec2 wellA(wp.x + padX, curY);
    ImVec2 wellB(wellA.x + innerW, wellA.y + wellH);
    ThemeEngine::DrawNeon3DCard(dl, wellA, wellB, 11.0f);
    ImGui::SetCursorScreenPos(wellA);
    if (ImGui::InvisibleButton("##tap_key_well", ImVec2(innerW, wellH)) && !verifying) {
        PromptNativeKeyInput(hostVC);
    }

    std::string shownKey;
    bool emptyKey = curKey.empty();
    if (emptyKey) {
        shownKey = T("Tap here to type or paste license key...", "اضغط هنا لكتابة أو لصق مفتاح التفعيل...");
    } else if (g_vault.maskKeyInput.load(std::memory_order_relaxed)) {
        shownKey.assign(std::min<size_t>(curKey.size(), 28), '*');
    } else {
        shownKey = curKey;
        if (shownKey.size() > 32) shownKey = shownKey.substr(0, 29) + "...";
    }
    ImVec2 kSz = ImGui::CalcTextSize(shownKey.c_str());
    ImU32 kCol = emptyKey ? ThemeEngine::GetMutedText() : IM_COL32(246, 250, 255, 255);
    dl->AddText(ImVec2(wellA.x + (innerW - kSz.x) * 0.5f, wellA.y + (wellH - kSz.y) * 0.5f), kCol, shownKey.c_str());

    // Quick Action Row under Key Well: [Type] [Paste] [Show/Hide] [Clear]
    curY += wellH + 10.0f;
    float gap = 8.0f;
    float btnW = (innerW - gap * 3.0f) * 0.25f;
    ImGui::SetCursorScreenPos(ImVec2(wp.x + padX, curY));
    if (ThemeEngine::SecondaryPillButton(T("Type##aktype", "كتابة##aktype"), btnW, 32.0f, false) && !verifying) {
        PromptNativeKeyInput(hostVC);
    }
    ImGui::SetCursorScreenPos(ImVec2(wp.x + padX + (btnW + gap) * 1.0f, curY));
    if (ThemeEngine::SecondaryPillButton(T("Paste##akpst", "لصق##akpst"), btnW, 32.0f, false) && !verifying) {
        NSString* clip = [UIPasteboard generalPasteboard].string;
        if (clip && clip.length > 0) {
            NSString* tr = [clip stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
            if (tr.length > 0) {
                {
                    std::lock_guard<std::mutex> lk(g_vault.metaMutex);
                    g_vault.inputKey = tr.UTF8String ?: "";
                }
                g_vault.status.store(StatusKind::InfoPasted, std::memory_order_release);
                g_vault.statusAt.store(now, std::memory_order_release);
            }
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(wp.x + padX + (btnW + gap) * 2.0f, curY));
    bool masked = g_vault.maskKeyInput.load(std::memory_order_relaxed);
    if (ThemeEngine::SecondaryPillButton(masked ? T("Show##akmsk", "إظهار##akmsk") : T("Hide##akmsk", "إخفاء##akmsk"), btnW, 32.0f, false)) {
        g_vault.maskKeyInput.store(!masked, std::memory_order_relaxed);
    }
    ImGui::SetCursorScreenPos(ImVec2(wp.x + padX + (btnW + gap) * 3.0f, curY));
    if (ThemeEngine::SecondaryPillButton(T("Clear##akclr", "مسح##akclr"), btnW, 32.0f, true) && !verifying) {
        std::lock_guard<std::mutex> lk(g_vault.metaMutex);
        g_vault.inputKey.clear();
        g_vault.status.store(StatusKind::Idle, std::memory_order_release);
    }

    // 5. Device HWID Chip + One-Tap Copy Button
    curY += 40.0f;
    float hwidRowH = 34.0f;
    float copyBtnW = 86.0f;
    ImVec2 hwA(wp.x + padX, curY);
    ImVec2 hwB(hwA.x + innerW - copyBtnW - 8.0f, hwA.y + hwidRowH);
    dl->AddRectFilled(hwA, hwB, ThemeEngine::GetCardBg(0.95f), 10.0f);
    dl->AddRect(hwA, hwB, IM_COL32(255, 255, 255, 18), 10.0f, 0, 1.0f);
    std::string hwidLine = "ID: " + GetOrCreateDeviceHWID();
    ImVec2 hwSz = ImGui::CalcTextSize(hwidLine.c_str());
    dl->AddText(ImVec2(hwA.x + 12.0f, hwA.y + (hwidRowH - hwSz.y) * 0.5f), IM_COL32(195, 204, 222, 255), hwidLine.c_str());

    ImGui::SetCursorScreenPos(ImVec2(hwB.x + 8.0f, curY));
    if (ThemeEngine::SecondaryPillButton(T("Copy##akcpy", "نسخ##akcpy"), copyBtnW, hwidRowH, false)) {
        std::string hw = GetOrCreateDeviceHWID();
        [UIPasteboard generalPasteboard].string = [NSString stringWithUTF8String:hw.c_str()] ?: @"";
        g_vault.status.store(StatusKind::InfoCopied, std::memory_order_release);
        g_vault.statusAt.store(now, std::memory_order_release);
    }

    // 6. Primary iOS CTA: ACTIVATE LICENSE
    curY += hwidRowH + 12.0f;
    ImGui::SetCursorScreenPos(ImVec2(wp.x + padX, curY));
    const char* ctaLabel = verifying
        ? T("VERIFYING ONLINE...##akgo", "جاري التحقق أونلاين...##akgo")
        : T("ACTIVATE LICENSE##akgo", "تفعيل الاشتراك##akgo");
    if (ThemeEngine::NeonActionButton(ctaLabel, innerW, 44.0f, false) && !verifying) {
        VerifyOnline(curKey, false);
    }

    // 7. Live Status / Error Strip
    curY += 52.0f;
    float stH = 30.0f;
    ImVec2 stA(wp.x + padX, curY);
    ImVec2 stB(stA.x + innerW, stA.y + stH);
    ImU32 stBg  = isErr ? IM_COL32(76, 22, 28, 220)
                        : (isOk ? IM_COL32(18, 66, 44, 220)
                                : (verifying ? IM_COL32(68, 52, 18, 220) : IM_COL32(26, 30, 40, 210)));
    ImU32 stRim = isErr ? IM_COL32(255, 82, 92, 180)
                        : (isOk ? IM_COL32(48, 209, 88, 180)
                                : (verifying ? IM_COL32(255, 195, 62, 180) : IM_COL32(255, 255, 255, 20)));
    ImU32 stTxt = isErr ? IM_COL32(255, 205, 210, 255)
                        : (isOk ? IM_COL32(185, 255, 215, 255) : IM_COL32(222, 228, 244, 255));
    dl->AddRectFilled(stA, stB, stBg, 9.0f);
    dl->AddRect(stA, stB, stRim, 9.0f, 0, 1.0f);
    ImVec2 mSz = ImGui::CalcTextSize(statusMsg);
    dl->AddText(ImVec2(stA.x + (innerW - mSz.x) * 0.5f, stA.y + (stH - mSz.y) * 0.5f), stTxt, statusMsg);

    // 8. Footer: Telegram Buy/Support + Build Stamp
    curY += stH + 10.0f;
    float footW = 154.0f;
    ImGui::SetCursorScreenPos(ImVec2(wp.x + padX, curY));
    if (ThemeEngine::SecondaryPillButton(T("Telegram / Buy##aktg", "تيليجرام / شراء##aktg"), footW, 28.0f, false)) {
        NSURL* tgUrl = [NSURL URLWithString:Config::SupportUrl()];
        if (tgUrl) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [[UIApplication sharedApplication] openURL:tgUrl options:@{} completionHandler:nil];
            });
        }
    }
    const char* footRight = "One State RP 1.4.1  •  LDR@SAW";
    ImVec2 frSz = ImGui::CalcTextSize(footRight);
    dl->AddText(ImVec2(wp.x + cardW - padX - frSz.x, curY + (28.0f - frSz.y) * 0.5f), ThemeEngine::GetMutedText(), footRight);

    ImGui::End();
    return closeRequested;
}

} // namespace AuthSystem
