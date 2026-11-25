#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// Lang.h — Bilingual English / Arabic Localization + Pure C++ Arabic Contextual Shaper & BiDi Reorder for Dear ImGui
namespace Lang {

inline int g_lang = 0; // 0 = English, 1 = Arabic

inline bool IsArabic() { return g_lang == 1; }
inline void SetLanguage(int mode) { g_lang = (mode == 1) ? 1 : 0; }

// UTF-8 decode / encode
inline std::vector<uint32_t> DecodeUtf8(const char* s, size_t len) {
    std::vector<uint32_t> out;
    out.reserve(len);
    size_t i = 0;
    while (i < len) {
        uint8_t c = static_cast<uint8_t>(s[i]);
        if (c < 0x80) {
            out.push_back(c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < len) {
            uint32_t cp = ((c & 0x1F) << 6) | (static_cast<uint8_t>(s[i + 1]) & 0x3F);
            out.push_back(cp);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < len) {
            uint32_t cp = ((c & 0x0F) << 12) |
                          ((static_cast<uint8_t>(s[i + 1]) & 0x3F) << 6) |
                          (static_cast<uint8_t>(s[i + 2]) & 0x3F);
            out.push_back(cp);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < len) {
            uint32_t cp = ((c & 0x07) << 18) |
                          ((static_cast<uint8_t>(s[i + 1]) & 0x3F) << 12) |
                          ((static_cast<uint8_t>(s[i + 2]) & 0x3F) << 6) |
                          (static_cast<uint8_t>(s[i + 3]) & 0x3F);
            out.push_back(cp);
            i += 4;
        } else {
            i += 1;
        }
    }
    return out;
}

inline void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

struct ArabicForm {
    uint16_t iso, fin, ini, med;
    bool dualJoin; // true = joins both right & left; false = right-joining only
};

inline bool LookupArabicForm(uint32_t cp, ArabicForm& out) {
    switch (cp) {
        case 0x0621: out = {0xFE80, 0xFE80, 0xFE80, 0xFE80, false}; return true; // Hamza
        case 0x0622: out = {0xFE81, 0xFE82, 0xFE81, 0xFE82, false}; return true; // Alef Madda
        case 0x0623: out = {0xFE83, 0xFE84, 0xFE83, 0xFE84, false}; return true; // Alef Hamza Above
        case 0x0624: out = {0xFE85, 0xFE86, 0xFE85, 0xFE86, false}; return true; // Waw Hamza
        case 0x0625: out = {0xFE87, 0xFE88, 0xFE87, 0xFE88, false}; return true; // Alef Hamza Below
        case 0x0626: out = {0xFE89, 0xFE8A, 0xFE8B, 0xFE8C, true};  return true; // Yeh Hamza
        case 0x0627: out = {0xFE8D, 0xFE8E, 0xFE8D, 0xFE8E, false}; return true; // Alef
        case 0x0628: out = {0xFE8F, 0xFE90, 0xFE91, 0xFE92, true};  return true; // Beh
        case 0x0629: out = {0xFE93, 0xFE94, 0xFE93, 0xFE94, false}; return true; // Teh Marbuta
        case 0x062A: out = {0xFE95, 0xFE96, 0xFE97, 0xFE98, true};  return true; // Teh
        case 0x062B: out = {0xFE99, 0xFE9A, 0xFE9B, 0xFE9C, true};  return true; // Theh
        case 0x062C: out = {0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0, true};  return true; // Jeem
        case 0x062D: out = {0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4, true};  return true; // Hah
        case 0x062E: out = {0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8, true};  return true; // Khah
        case 0x062F: out = {0xFEA9, 0xFEAA, 0xFEA9, 0xFEAA, false}; return true; // Dal
        case 0x0630: out = {0xFEAB, 0xFEAC, 0xFEAB, 0xFEAC, false}; return true; // Thal
        case 0x0631: out = {0xFEAD, 0xFEAE, 0xFEAD, 0xFEAE, false}; return true; // Reh
        case 0x0632: out = {0xFEAF, 0xFEB0, 0xFEAF, 0xFEB0, false}; return true; // Zain
        case 0x0633: out = {0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4, true};  return true; // Seen
        case 0x0634: out = {0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8, true};  return true; // Sheen
        case 0x0635: out = {0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC, true};  return true; // Sad
        case 0x0636: out = {0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0, true};  return true; // Dad
        case 0x0637: out = {0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4, true};  return true; // Tah
        case 0x0638: out = {0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8, true};  return true; // Zah
        case 0x0639: out = {0xFEC9, 0xFECA, 0xFECB, 0xFECC, true};  return true; // Ain
        case 0x063A: out = {0xFECD, 0xFECE, 0xFECF, 0xFED0, true};  return true; // Ghain
        case 0x0640: out = {0x0640, 0x0640, 0x0640, 0x0640, true};  return true; // Tatweel
        case 0x0641: out = {0xFED1, 0xFED2, 0xFED3, 0xFED4, true};  return true; // Feh
        case 0x0642: out = {0xFED5, 0xFED6, 0xFED7, 0xFED8, true};  return true; // Qaf
        case 0x0643: out = {0xFED9, 0xFEDA, 0xFEDB, 0xFEDC, true};  return true; // Kaf
        case 0x0644: out = {0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0, true};  return true; // Lam
        case 0x0645: out = {0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4, true};  return true; // Meem
        case 0x0646: out = {0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8, true};  return true; // Noon
        case 0x0647: out = {0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC, true};  return true; // Heh
        case 0x0648: out = {0xFEED, 0xFEEE, 0xFEED, 0xFEEE, false}; return true; // Waw
        case 0x0649: out = {0xFEEF, 0xFEF0, 0xFEEF, 0xFEF0, false}; return true; // Alef Maksura
        case 0x064A: out = {0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4, true};  return true; // Yeh
        default: return false;
    }
}

inline bool IsArabicDiacritic(uint32_t cp) {
    return (cp >= 0x064B && cp <= 0x065F) || cp == 0x0670;
}

inline bool IsArabicChar(uint32_t cp) {
    return (cp >= 0x0600 && cp <= 0x06FF) || (cp >= 0xFB50 && cp <= 0xFDFF) || (cp >= 0xFE70 && cp <= 0xFEFF);
}

inline bool IsLtrTokenChar(uint32_t cp) {
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9') ||
           cp == '.' || cp == '%' || cp == '_' || cp == '+' || cp == '-' || cp == ':' || cp == '#';
}

// Shapes logical UTF-8 Arabic text into connected Presentation Forms-B and visual LTR order for Dear ImGui.
// Preserves ImGui '##id' suffixes intact at the end of the string.
inline std::string ShapeArabicForImGui(const char* utf8) {
    if (!utf8 || !utf8[0]) return {};
    const char* idSplit = std::strstr(utf8, "##");
    size_t textLen = idSplit ? static_cast<size_t>(idSplit - utf8) : std::strlen(utf8);
    std::vector<uint32_t> cps = DecodeUtf8(utf8, textLen);

    bool hasArabic = false;
    for (uint32_t c : cps) {
        if (IsArabicChar(c)) { hasArabic = true; break; }
    }
    if (!hasArabic) return std::string(utf8);

    // Strip combining harakat so glyph width stays exact in ImGui
    std::vector<uint32_t> clean;
    clean.reserve(cps.size());
    for (uint32_t c : cps) {
        if (!IsArabicDiacritic(c)) clean.push_back(c);
    }

    // Contextual shaping + Lam-Alef ligatures
    std::vector<uint32_t> shaped;
    shaped.reserve(clean.size());
    size_t n = clean.size();
    for (size_t i = 0; i < n; ++i) {
        uint32_t cur = clean[i];
        ArabicForm cf{};
        if (!LookupArabicForm(cur, cf)) {
            // Mirror parentheses in RTL visual display
            if (cur == '(') cur = ')';
            else if (cur == ')') cur = '(';
            else if (cur == '[') cur = ']';
            else if (cur == ']') cur = '[';
            shaped.push_back(cur);
            continue;
        }
        bool prevJoinsLeft = false;
        if (i > 0) {
            ArabicForm pf{};
            if (LookupArabicForm(clean[i - 1], pf) && pf.dualJoin) prevJoinsLeft = true;
        }
        // Check Lam-Alef ligature (Lam + Alef/AlefMadda/AlefHamzaAbove/AlefHamzaBelow)
        if (cur == 0x0644 && i + 1 < n) {
            uint32_t nxt = clean[i + 1];
            uint16_t ligIso = 0, ligFin = 0;
            if      (nxt == 0x0622) { ligIso = 0xFEF5; ligFin = 0xFEF6; }
            else if (nxt == 0x0623) { ligIso = 0xFEF7; ligFin = 0xFEF8; }
            else if (nxt == 0x0625) { ligIso = 0xFEF9; ligFin = 0xFEFA; }
            else if (nxt == 0x0627) { ligIso = 0xFEFB; ligFin = 0xFEFC; }
            if (ligIso != 0) {
                shaped.push_back(prevJoinsLeft ? ligFin : ligIso);
                ++i;
                continue;
            }
        }
        bool nextJoinsRight = false;
        if (i + 1 < n && cf.dualJoin) {
            ArabicForm nf{};
            if (LookupArabicForm(clean[i + 1], nf) && clean[i + 1] != 0x0621) nextJoinsRight = true;
        }
        uint16_t glyph = cf.iso;
        if (prevJoinsLeft && nextJoinsRight)      glyph = cf.med;
        else if (prevJoinsLeft && !nextJoinsRight) glyph = cf.fin;
        else if (!prevJoinsLeft && nextJoinsRight) glyph = cf.ini;
        shaped.push_back(glyph);
    }

    // Visual RTL reorder while keeping embedded Latin/number runs LTR
    std::reverse(shaped.begin(), shaped.end());
    size_t i = 0;
    while (i < shaped.size()) {
        if (IsLtrTokenChar(shaped[i])) {
            size_t start = i;
            while (i < shaped.size() && (IsLtrTokenChar(shaped[i]) ||
                   (shaped[i] == ' ' && i + 1 < shaped.size() && IsLtrTokenChar(shaped[i + 1])))) {
                ++i;
            }
            size_t end = i;
            while (end > start && shaped[end - 1] == ' ') --end;
            if (end > start + 1) std::reverse(shaped.begin() + start, shaped.begin() + end);
        } else {
            ++i;
        }
    }

    std::string out;
    out.reserve(shaped.size() * 3 + (idSplit ? std::strlen(idSplit) : 0));
    for (uint32_t cp : shaped) AppendUtf8(out, cp);
    if (idSplit) out.append(idSplit);
    return out;
}

// Fast O(1) cached bilingual lookup: T("Silent Aim", "سايلنت إيم")
inline const char* T(const char* en, const char* ar) {
    if (g_lang == 0 || !ar || !ar[0]) return en;
    static std::unordered_map<const char*, std::string> s_cache;
    auto it = s_cache.find(ar);
    if (it != s_cache.end()) return it->second.c_str();
    auto res = s_cache.emplace(ar, ShapeArabicForImGui(ar));
    return res.first->second.c_str();
}

// Shapes a multi-item ImGui combo list ("A\0B\0") when in Arabic mode
inline const char* TCombo(const char* enList, const char* arList) {
    if (g_lang == 0 || !arList) return enList;
    static std::unordered_map<const char*, std::string> s_comboCache;
    auto it = s_comboCache.find(arList);
    if (it != s_comboCache.end()) return it->second.c_str();
    std::string out;
    const char* p = arList;
    while (*p) {
        std::string item = ShapeArabicForImGui(p);
        out.append(item);
        out.push_back('\0');
        p += std::strlen(p) + 1;
    }
    out.push_back('\0');
    auto res = s_comboCache.emplace(arList, std::move(out));
    return res.first->second.c_str();
}

} // namespace Lang
