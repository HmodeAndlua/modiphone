#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// Theme.h — Flagship iOS 18 Frosted Glass UI Engine (Apple Switches, Big-Finger Sliders + Steppers, Spotlight Search)
namespace ThemeEngine {

inline constexpr const char* kBrandTitle   = "ONESTATE // iOS PRO 1.4.1";
inline constexpr const char* kBrandPill    = "SAW";
inline constexpr const char* kCommunityUrl = "https://t.me/LADRRAZA";

enum class Preset : int {
    MidnightIndigo = 0, // iOS Blue (#0A84FF)
    ObsidianCyan,       // Indigo Pro (#5E5CE6)
    HyperMagenta,       // Emerald Mint (#30D158)
    EmeraldMatrix,      // Crimson Pro (#FF375F)
    SolarGold,          // Amber Gold (#FF9F0A)
    ArcticFrost,        // Titanium Ice (#64D2FF)
    Custom,
    Count
};

struct State {
    float hue        = 0.58f; // Apple iOS Blue (#0A84FF)
    float sat        = 0.88f;
    float val        = 1.00f;
    float glassAlpha = 0.95f;
    float uiScale    = 1.00f;
    float btnScale   = 1.00f;
    Preset preset    = Preset::MidnightIndigo;
    int langIndex    = 0;
};

inline State g_state{};
inline bool  g_sliderDragging = false; // Prevents touch-swipe scroll from stealing active slider drags

struct PresetSpec {
    const char* name;
    float h, s, v;
};

inline constexpr PresetSpec kPresets[] = {
    { "iOS Blue",      0.58f, 0.88f, 1.00f },
    { "Indigo Pro",    0.67f, 0.62f, 0.96f },
    { "Emerald Mint",  0.38f, 0.78f, 0.86f },
    { "Crimson Pro",   0.97f, 0.78f, 1.00f },
    { "Amber Gold",    0.10f, 0.92f, 1.00f },
    { "Titanium Ice",  0.54f, 0.56f, 0.98f },
};

inline void ApplyPreset(Preset p) {
    int idx = static_cast<int>(p);
    if (idx >= 0 && idx < 6) {
        g_state.preset = p;
        g_state.hue = kPresets[idx].h;
        g_state.sat = kPresets[idx].s;
        g_state.val = kPresets[idx].v;
    }
}

inline ImVec4 HsvToVec4(float h, float s, float v, float a = 1.0f) {
    float r = 0, g = 0, b = 0;
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
    return ImVec4(r, g, b, a);
}

inline ImU32 GetAccent(float alpha = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(HsvToVec4(g_state.hue, g_state.sat, g_state.val, alpha));
}
inline ImU32 GetSecondaryNeon(float alpha = 1.0f) {
    // Refined tonal companion (subtle iOS highlight tint rather than loud rainbow shift)
    float h2 = std::fmod(g_state.hue + 0.045f, 1.0f);
    return ImGui::ColorConvertFloat4ToU32(HsvToVec4(h2, std::max(0.35f, g_state.sat * 0.78f), 1.0f, alpha));
}
inline ImU32 GetBg(float alpha = -1.0f) {
    float a = (alpha < 0.0f) ? g_state.glassAlpha : alpha;
    return IM_COL32(16, 18, 24, static_cast<int>(a * 255.0f));
}
inline ImU32 GetSidebar(float alpha = -1.0f) {
    float a = (alpha < 0.0f) ? g_state.glassAlpha : alpha;
    return IM_COL32(11, 13, 18, static_cast<int>(a * 255.0f));
}
inline ImU32 GetSubBar(float alpha = 0.96f) {
    return IM_COL32(22, 25, 34, static_cast<int>(alpha * 255.0f));
}
inline ImU32 GetCardBg(float alpha = 0.94f) {
    return IM_COL32(26, 29, 38, static_cast<int>(alpha * 255.0f));
}
inline ImU32 GetBorder(float alpha = 0.55f) {
    return IM_COL32(255, 255, 255, static_cast<int>(alpha * 44.0f));
}
inline ImU32 GetText()      { return IM_COL32(246, 248, 252, 255); }
inline ImU32 GetMutedText() { return IM_COL32(146, 154, 172, 255); }

// Clean iOS 18 Frosted Glass Outer Rim & Soft Elevation Shadow
inline void DrawNeonGlowRect(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding = 18.0f, float intensity = 1.0f) {
    // Soft ambient Apple elevation shadow
    dl->AddRect(ImVec2(a.x - 2, a.y - 1), ImVec2(b.x + 2, b.y + 4), IM_COL32(0, 0, 0, static_cast<int>(70 * intensity)), rounding + 2.0f, 0, 4.0f);
    // Crisp hairline titanium glass border
    dl->AddRect(a, b, IM_COL32(255, 255, 255, static_cast<int>(30 + 18 * intensity)), rounding, 0, 1.15f);
}

inline void DrawNeon3DBackdrop(ImDrawList* dl, ImVec2 wp, float winW, float winH, float railW, double /*timeSec*/, bool showAmbient) {
    int aVal = static_cast<int>(g_state.glassAlpha * 255.0f);
    ImU32 topBg = IM_COL32(20, 23, 31, aVal);
    ImU32 botBg = IM_COL32(13, 15, 21, aVal);

    // Main frosted obsidian glass sheet (18px iOS sheet corner radius)
    dl->AddRectFilled(wp, ImVec2(wp.x + winW, wp.y + winH), botBg, 18.0f);
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x + railW, wp.y + 1.0f), ImVec2(wp.x + winW - 1.0f, wp.y + winH * 0.48f),
        topBg, topBg, botBg, botBg);

    if (showAmbient) {
        // Subtle Apple VisionOS-style top-edge ambient tint glow (clean & calm, zero wireframe clutter)
        dl->PushClipRect(ImVec2(wp.x + railW, wp.y), ImVec2(wp.x + winW, wp.y + 92.0f), true);
        dl->AddRectFilledMultiColor(
            ImVec2(wp.x + railW, wp.y), ImVec2(wp.x + winW, wp.y + 82.0f),
            GetAccent(0.11f), GetSecondaryNeon(0.08f),
            GetSecondaryNeon(0.0f), GetAccent(0.0f));
        dl->PopClipRect();
    }

    // Specular top glass hairline
    dl->AddLine(ImVec2(wp.x + 18.0f, wp.y + 1.0f), ImVec2(wp.x + winW - 18.0f, wp.y + 1.0f), IM_COL32(255, 255, 255, 42), 1.0f);
    DrawNeonGlowRect(dl, wp, ImVec2(wp.x + winW, wp.y + winH), 18.0f, showAmbient ? 1.0f : 0.6f);
}

inline void DrawNeon3DCard(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding = 12.0f) {
    dl->AddRectFilled(a, b, GetCardBg(0.95f), rounding);
    dl->AddRect(a, b, IM_COL32(255, 255, 255, 20), rounding, 0, 1.0f);
}

inline bool CaseInsensitiveContains(const char* haystack, const char* needle) {
    if (!needle || !needle[0]) return true;
    if (!haystack) return false;
    size_t nLen = std::strlen(needle);
    for (const char* p = haystack; *p; ++p) {
        size_t i = 0;
        while (i < nLen && p[i] && std::tolower((unsigned char)p[i]) == std::tolower((unsigned char)needle[i])) ++i;
        if (i == nLen) return true;
    }
    return false;
}

inline void ApplyProStyle() {
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding    = 18.0f;
    st.ChildRounding     = 12.0f;
    st.FrameRounding     = 9.0f;
    st.PopupRounding     = 12.0f;
    st.ScrollbarRounding = 12.0f;
    st.GrabRounding      = 8.0f;
    st.WindowBorderSize  = 0.0f;
    st.ChildBorderSize   = 0.0f;
    st.FrameBorderSize   = 0.0f;
    st.WindowPadding     = ImVec2(0.0f, 0.0f);
    st.ItemSpacing       = ImVec2(8.0f, 8.0f);
    st.FramePadding      = ImVec2(10.0f, 6.0f);
    st.ScrollbarSize     = 26.0f; // Extra-wide comfortable thumb scrollbar for iPhone (touch-swipe disabled)

    ImVec4 acc    = HsvToVec4(g_state.hue, g_state.sat, g_state.val, 1.0f);
    ImVec4 accDim = HsvToVec4(g_state.hue, g_state.sat, g_state.val * 0.85f, 0.38f);

    ImVec4* c = st.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(15/255.f, 17/255.f, 23/255.f, g_state.glassAlpha);
    c[ImGuiCol_ChildBg]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]         = ImVec4(24/255.f, 27/255.f, 36/255.f, 0.98f);
    c[ImGuiCol_Border]          = ImVec4(1.0f, 1.0f, 1.0f, 0.09f);
    c[ImGuiCol_FrameBg]         = ImVec4(32/255.f, 36/255.f, 48/255.f, 0.96f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(42/255.f, 47/255.f, 62/255.f, 1.00f);
    c[ImGuiCol_FrameBgActive]   = accDim;
    c[ImGuiCol_Button]          = ImVec4(36/255.f, 40/255.f, 54/255.f, 0.96f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(acc.x * 0.65f, acc.y * 0.65f, acc.z * 0.65f, 0.92f);
    c[ImGuiCol_ButtonActive]    = acc;
    c[ImGuiCol_Header]          = ImVec4(acc.x, acc.y, acc.z, 0.24f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(acc.x, acc.y, acc.z, 0.38f);
    c[ImGuiCol_HeaderActive]    = ImVec4(acc.x, acc.y, acc.z, 0.56f);
    c[ImGuiCol_SliderGrab]      = ImVec4(1, 1, 1, 1);
    c[ImGuiCol_SliderGrabActive]= ImVec4(1, 1, 1, 1);
    c[ImGuiCol_CheckMark]       = acc;
    c[ImGuiCol_Text]            = ImVec4(246/255.f, 248/255.f, 252/255.f, 1.0f);
    c[ImGuiCol_TextDisabled]    = ImVec4(146/255.f, 154/255.f, 172/255.f, 1.0f);
    c[ImGuiCol_ScrollbarBg]     = ImVec4(1.0f, 1.0f, 1.0f, 0.11f);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(acc.x * 0.55f + 0.35f, acc.y * 0.55f + 0.35f, acc.z * 0.55f + 0.35f, 0.78f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(acc.x, acc.y, acc.z, 0.92f);
    c[ImGuiCol_ScrollbarGrabActive]  = acc;
}

// Clean iOS 18 Tactile Filled Button (Rounded Pill/Card)
inline bool NeonActionButton(const char* label, float width = -1.0f, float height = 36.0f, bool danger = false) {
    float avail = ImGui::GetContentRegionAvail().x;
    float w = (width <= 0.0f) ? avail : width;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, ImVec2(w, height));
    bool hovered = ImGui::IsItemHovered();
    bool held    = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float r = std::min(11.0f, height * 0.38f);
    ImU32 fill = danger
        ? IM_COL32(255, 69, 58, held ? 245 : (hovered ? 225 : 205))
        : GetAccent(held ? 0.98f : (hovered ? 0.90f : 0.82f));
    ImU32 rim  = danger ? IM_COL32(255, 130, 124, 90) : IM_COL32(255, 255, 255, 42);

    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + height), fill, r);
    dl->AddRect(p, ImVec2(p.x + w, p.y + height), rim, r, 0, 1.0f);

    const char* endTxt = std::strstr(label, "##");
    ImVec2 ts = endTxt ? ImGui::CalcTextSize(label, endTxt) : ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (height - ts.y) * 0.5f), IM_COL32(255, 255, 255, 255), label, endTxt);
    return clicked;
}

// Clean Secondary / Frosted Glass Pill Button (for utility actions like Paste/Copy/Clear/Lang)
inline bool SecondaryPillButton(const char* label, float width = -1.0f, float height = 34.0f, bool dangerTint = false) {
    float avail = ImGui::GetContentRegionAvail().x;
    float w = (width <= 0.0f) ? avail : width;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, ImVec2(w, height));
    bool hovered = ImGui::IsItemHovered();
    bool held    = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float r = std::min(10.0f, height * 0.42f);
    ImU32 fill = dangerTint
        ? IM_COL32(62, 24, 30, held ? 250 : (hovered ? 235 : 215))
        : IM_COL32(36, 40, 52, held ? 250 : (hovered ? 235 : 215));
    ImU32 rim = dangerTint ? IM_COL32(255, 85, 95, 105) : IM_COL32(255, 255, 255, 28);
    ImU32 txt = dangerTint ? IM_COL32(255, 130, 135, 255) : GetText();

    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + height), fill, r);
    dl->AddRect(p, ImVec2(p.x + w, p.y + height), rim, r, 0, 1.0f);

    const char* endTxt = std::strstr(label, "##");
    ImVec2 ts = endTxt ? ImGui::CalcTextSize(label, endTxt) : ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (height - ts.y) * 0.5f), txt, label, endTxt);
    return clicked;
}

inline void ApplyObsidianGlassStyle() { ApplyProStyle(); }

// ============================================================================
// Clean Apple iOS Sidebar Navigation Icons (Rounded Pill Active Selection)
// ============================================================================
inline bool RailIconButton(const char* id, int iconIndex, bool active, float railW, float itemH) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(railW, itemH));
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 c(p.x + railW * 0.5f, p.y + itemH * 0.5f);
    if (active) {
        ImVec2 pillA(c.x - 22.0f, c.y - 20.0f);
        ImVec2 pillB(c.x + 22.0f, c.y + 20.0f);
        dl->AddRectFilled(pillA, pillB, GetAccent(0.24f), 12.0f);
        dl->AddRect(pillA, pillB, GetAccent(0.65f), 12.0f, 0, 1.15f);
        // Subtle Apple-style left indicator pill
        dl->AddRectFilled(ImVec2(p.x + 3.0f, c.y - 11.0f), ImVec2(p.x + 6.5f, c.y + 11.0f), GetAccent(1.0f), 2.0f);
    } else if (hovered) {
        dl->AddRectFilled(ImVec2(c.x - 22.0f, c.y - 20.0f), ImVec2(c.x + 22.0f, c.y + 20.0f), IM_COL32(255, 255, 255, 14), 12.0f);
    }

    ImU32 col = active ? GetAccent(1.0f) : (hovered ? IM_COL32(200, 206, 220, 240) : IM_COL32(128, 136, 156, 230));
    float t = 1.9f;

    if (iconIndex == 0) {
        // Crosshair reticle
        dl->AddCircle(c, 11.5f, col, 32, t);
        dl->AddCircleFilled(c, 2.4f, col);
        dl->AddLine(ImVec2(c.x, c.y - 16), ImVec2(c.x, c.y - 8), col, t);
        dl->AddLine(ImVec2(c.x, c.y + 8),  ImVec2(c.x, c.y + 16), col, t);
        dl->AddLine(ImVec2(c.x - 16, c.y), ImVec2(c.x - 8, c.y), col, t);
        dl->AddLine(ImVec2(c.x + 8, c.y),  ImVec2(c.x + 16, c.y), col, t);
    } else if (iconIndex == 1) {
        // Eye
        ImVec2 ptsTop[5] = { {c.x - 15, c.y}, {c.x - 8, c.y - 8}, {c.x, c.y - 10}, {c.x + 8, c.y - 8}, {c.x + 15, c.y} };
        ImVec2 ptsBot[5] = { {c.x - 15, c.y}, {c.x - 8, c.y + 8}, {c.x, c.y + 10}, {c.x + 8, c.y + 8}, {c.x + 15, c.y} };
        dl->AddPolyline(ptsTop, 5, col, 0, t);
        dl->AddPolyline(ptsBot, 5, col, 0, t);
        dl->AddCircle(c, 5.2f, col, 24, t);
        dl->AddCircleFilled(c, 2.2f, col);
    } else if (iconIndex == 2) {
        // 4-tile grid (Player / Map / Vehicle)
        float g = 2.2f, s = 10.5f;
        dl->AddRect(ImVec2(c.x - g - s, c.y - g - s), ImVec2(c.x - g, c.y - g), col, 3.0f, 0, t);
        dl->AddRect(ImVec2(c.x + g, c.y - g - s),     ImVec2(c.x + g + s, c.y - g), col, 3.0f, 0, t);
        dl->AddRect(ImVec2(c.x - g - s, c.y + g),     ImVec2(c.x - g, c.y + g + s), col, 3.0f, 0, t);
        dl->AddCircle(ImVec2(c.x + g + s * 0.5f, c.y + g + s * 0.5f), s * 0.5f, col, 24, t);
    } else if (iconIndex == 3) {
        // Users / List / Wardrobe
        ImVec2 pg[5] = { {c.x - 9, c.y - 13}, {c.x + 4, c.y - 13}, {c.x + 10, c.y - 7}, {c.x + 10, c.y + 13}, {c.x - 9, c.y + 13} };
        dl->AddPolyline(pg, 5, col, ImDrawFlags_Closed, t);
        dl->AddLine(ImVec2(c.x - 5, c.y - 2), ImVec2(c.x + 5, c.y - 2), col, 1.6f);
        dl->AddLine(ImVec2(c.x - 5, c.y + 3), ImVec2(c.x + 6, c.y + 3), col, 1.6f);
        dl->AddLine(ImVec2(c.x - 5, c.y + 8), ImVec2(c.x + 2, c.y + 8), col, 1.6f);
    } else {
        // Settings / Gear Sliders
        dl->AddLine(ImVec2(c.x - 13, c.y - 7), ImVec2(c.x + 13, c.y - 7), col, t);
        dl->AddCircleFilled(ImVec2(c.x - 5, c.y - 7), 3.4f, col);
        dl->AddLine(ImVec2(c.x - 13, c.y),     ImVec2(c.x + 13, c.y),     col, t);
        dl->AddCircleFilled(ImVec2(c.x + 6, c.y),     3.4f, col);
        dl->AddLine(ImVec2(c.x - 13, c.y + 7), ImVec2(c.x + 13, c.y + 7), col, t);
        dl->AddCircleFilled(ImVec2(c.x - 2, c.y + 7), 3.4f, col);
    }
    return clicked;
}

// ============================================================================
// iOS 18 Native-Feel Touch Controls
// ============================================================================

// Authentic Apple iOS Pill Switch inside a clean 40pt grouped inset row
inline bool ProToggle(const char* label, bool* v, float* optColorRgb = nullptr) {
    ImGui::PushID(label);
    float avail = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float rowH = 40.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Grouped iOS inset row card
    ImVec2 rowB(p.x + avail, p.y + rowH);
    dl->AddRectFilled(p, rowB, GetCardBg(0.95f), 11.0f);
    dl->AddRect(p, rowB, (*v) ? GetAccent(0.32f) : IM_COL32(255, 255, 255, 16), 11.0f, 0, 1.0f);

    const char* endTxt = std::strstr(label, "##");
    ImVec2 ts = endTxt ? ImGui::CalcTextSize(label, endTxt) : ImGui::CalcTextSize(label);
    ImU32 txtCol = (*v) ? GetText() : IM_COL32(198, 204, 218, 245);
    dl->AddText(ImVec2(p.x + 12.0f, p.y + (rowH - ts.y) * 0.5f), txtCol, label, endTxt);

    // Authentic iOS Pill Switch (46x26)
    float swW = 46.0f, swH = 26.0f;
    float swX = p.x + avail - swW - 10.0f;
    float swY = p.y + (rowH - swH) * 0.5f;

    if (optColorRgb) {
        float dotX = swX - 20.0f;
        float dotY = p.y + rowH * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(dotX - 13, dotY - 13));
        if (ImGui::InvisibleButton("##drop", ImVec2(26, 26))) {
            ImGui::OpenPopup("##colpop");
        }
        ImU32 cFill = IM_COL32((int)(optColorRgb[0] * 255), (int)(optColorRgb[1] * 255), (int)(optColorRgb[2] * 255), 255);
        dl->AddCircleFilled(ImVec2(dotX, dotY), 9.0f, cFill, 20);
        dl->AddCircle(ImVec2(dotX, dotY), 9.5f, IM_COL32(255, 255, 255, 190), 20, 1.3f);
        if (ImGui::BeginPopup("##colpop")) {
            ImGui::ColorPicker3("##cp", optColorRgb, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs);
            ImGui::EndPopup();
        }
    }

    ImGui::SetCursorScreenPos(p);
    float clickW = optColorRgb ? std::max(40.0f, avail - 92.0f) : avail;
    bool changed = false;
    if (ImGui::InvisibleButton("##row", ImVec2(clickW, rowH))) {
        *v = !(*v);
        changed = true;
    }
    if (optColorRgb) {
        ImGui::SetCursorScreenPos(ImVec2(swX - 4.0f, swY - 4.0f));
        if (ImGui::InvisibleButton("##sw", ImVec2(swW + 8.0f, swH + 8.0f))) {
            *v = !(*v);
            changed = true;
        }
    }

    // Draw iOS Switch Track + White Sliding Knob
    ImU32 trackCol = (*v) ? GetAccent(1.0f) : IM_COL32(54, 58, 72, 255);
    dl->AddRectFilled(ImVec2(swX, swY), ImVec2(swX + swW, swY + swH), trackCol, swH * 0.5f);
    float knobR = 10.5f;
    float knobCx = (*v) ? (swX + swW - knobR - 2.5f) : (swX + knobR + 2.5f);
    float knobCy = swY + swH * 0.5f;
    dl->AddCircleFilled(ImVec2(knobCx, knobCy + 1.0f), knobR, IM_COL32(0, 0, 0, 70), 24);
    dl->AddCircleFilled(ImVec2(knobCx, knobCy), knobR, IM_COL32(255, 255, 255, 255), 24);

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + rowH + 6.0f));
    ImGui::PopID();
    return changed;
}

// Clean iOS Grouped Combo Row
inline bool ProCombo(const char* label, int* currentItem, const char* itemsSeparatedByZeros) {
    ImGui::PushID(label);
    float avail = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float rowH = 40.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p, ImVec2(p.x + avail, p.y + rowH), GetCardBg(0.95f), 11.0f);
    dl->AddRect(p, ImVec2(p.x + avail, p.y + rowH), IM_COL32(255, 255, 255, 16), 11.0f, 0, 1.0f);

    const char* endTxt = std::strstr(label, "##");
    ImVec2 ts = endTxt ? ImGui::CalcTextSize(label, endTxt) : ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + 12.0f, p.y + (rowH - ts.y) * 0.5f), IM_COL32(198, 204, 218, 245), label, endTxt);

    float comboW = std::min(168.0f, avail * 0.48f);
    ImGui::SetCursorScreenPos(ImVec2(p.x + avail - comboW - 8.0f, p.y + 6.0f));
    ImGui::SetNextItemWidth(comboW);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 5.0f));
    bool changed = ImGui::Combo("##cmb", currentItem, itemsSeparatedByZeros);
    ImGui::PopStyleVar();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + rowH + 6.0f));
    ImGui::PopID();
    return changed;
}

// Big-Finger iPhone Touch Slider: 64pt iOS Card + Wide 36pt Drag Zone + 26pt White Thumb + Tactile [-] / [+] Steppers!
inline bool ProSlider(const char* label, float* v, float vMin, float vMax, const char* fmt = "%.1f") {
    ImGui::PushID(label);
    float avail = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float cardH = 66.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Grouped iOS inset card
    dl->AddRectFilled(p, ImVec2(p.x + avail, p.y + cardH), GetCardBg(0.95f), 12.0f);
    dl->AddRect(p, ImVec2(p.x + avail, p.y + cardH), IM_COL32(255, 255, 255, 18), 12.0f, 0, 1.0f);

    // Top row: label on left, tinted value pill on right
    const char* endTxt = std::strstr(label, "##");
    dl->AddText(ImVec2(p.x + 12.0f, p.y + 8.0f), IM_COL32(215, 222, 236, 255), label, endTxt);

    char valBuf[48];
    snprintf(valBuf, sizeof(valBuf), fmt, *v);
    ImVec2 vs = ImGui::CalcTextSize(valBuf);
    float pillW = vs.x + 16.0f;
    ImVec2 vpA(p.x + avail - pillW - 10.0f, p.y + 5.0f);
    ImVec2 vpB(vpA.x + pillW, vpA.y + 20.0f);
    dl->AddRectFilled(vpA, vpB, GetAccent(0.20f), 8.0f);
    dl->AddText(ImVec2(vpA.x + 8.0f, vpA.y + 3.0f), GetAccent(1.0f), valBuf);

    // Compute smart fine step for [-] / [+] buttons
    float span = std::max(1e-4f, vMax - vMin);
    float step = (span <= 2.0f) ? 0.05f : ((span <= 15.0f) ? 0.5f : ((span <= 120.0f) ? 1.0f : 5.0f));

    // Bottom row: [-] (34x30)  |  Wide 36pt Finger Drag Track  |  [+] (34x30)
    float ctrlY = p.y + 28.0f;
    float ctrlH = 34.0f;
    float stepBtnW = 34.0f;
    float stepBtnH = 30.0f;
    float stepBtnY = ctrlY + (ctrlH - stepBtnH) * 0.5f;
    bool changed = false;

    // Minus button
    ImVec2 minA(p.x + 10.0f, stepBtnY);
    ImGui::SetCursorScreenPos(minA);
    bool minClick = ImGui::InvisibleButton("##sl_min", ImVec2(stepBtnW, stepBtnH));
    bool minHeld  = ImGui::IsItemActive();
    dl->AddRectFilled(minA, ImVec2(minA.x + stepBtnW, minA.y + stepBtnH),
                      minHeld ? GetAccent(0.55f) : IM_COL32(40, 45, 60, 245), 9.0f);
    dl->AddRect(minA, ImVec2(minA.x + stepBtnW, minA.y + stepBtnH), IM_COL32(255, 255, 255, 24), 9.0f, 0, 1.0f);
    dl->AddLine(ImVec2(minA.x + 11.0f, minA.y + stepBtnH * 0.5f),
                ImVec2(minA.x + stepBtnW - 11.0f, minA.y + stepBtnH * 0.5f), GetText(), 2.2f);
    if (minClick && vMax > vMin) {
        *v = std::clamp(*v - step, vMin, vMax);
        changed = true;
    }

    // Plus button
    ImVec2 plsA(p.x + avail - 10.0f - stepBtnW, stepBtnY);
    ImGui::SetCursorScreenPos(plsA);
    bool plsClick = ImGui::InvisibleButton("##sl_pls", ImVec2(stepBtnW, stepBtnH));
    bool plsHeld  = ImGui::IsItemActive();
    dl->AddRectFilled(plsA, ImVec2(plsA.x + stepBtnW, plsA.y + stepBtnH),
                      plsHeld ? GetAccent(0.55f) : IM_COL32(40, 45, 60, 245), 9.0f);
    dl->AddRect(plsA, ImVec2(plsA.x + stepBtnW, plsA.y + stepBtnH), IM_COL32(255, 255, 255, 24), 9.0f, 0, 1.0f);
    float pcx = plsA.x + stepBtnW * 0.5f, pcy = plsA.y + stepBtnH * 0.5f;
    dl->AddLine(ImVec2(pcx - 6.0f, pcy), ImVec2(pcx + 6.0f, pcy), GetText(), 2.2f);
    dl->AddLine(ImVec2(pcx, pcy - 6.0f), ImVec2(pcx, pcy + 6.0f), GetText(), 2.2f);
    if (plsClick && vMax > vMin) {
        *v = std::clamp(*v + step, vMin, vMax);
        changed = true;
    }

    // Center wide touch slider area
    float hitX0   = minA.x + stepBtnW + 8.0f;
    float hitX1   = plsA.x - 8.0f;
    float hitW    = std::max(50.0f, hitX1 - hitX0);
    float trackX0 = hitX0 + 12.0f;
    float trackX1 = hitX1 - 12.0f;
    float trackW  = std::max(30.0f, trackX1 - trackX0);
    float trackCy = ctrlY + ctrlH * 0.5f;

    ImGui::SetCursorScreenPos(ImVec2(hitX0, ctrlY));
    ImGui::InvisibleButton("##tsl", ImVec2(hitW, ctrlH));
    bool active  = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();
    if (active) g_sliderDragging = true;
    if ((active || clicked) && vMax > vMin) {
        float mx = ImGui::GetIO().MousePos.x;
        float t  = std::clamp((mx - trackX0) / trackW, 0.0f, 1.0f);
        float nv = vMin + t * (vMax - vMin);
        if (std::fabs(nv - *v) > 1e-4f) {
            *v = nv;
            changed = true;
        }
    }

    float norm  = (vMax > vMin) ? std::clamp((*v - vMin) / (vMax - vMin), 0.0f, 1.0f) : 0.0f;
    float fillX = trackX0 + norm * trackW;

    // Thick 10pt rounded iOS slider track
    float halfTrack = 5.0f;
    dl->AddRectFilled(ImVec2(trackX0, trackCy - halfTrack), ImVec2(trackX1, trackCy + halfTrack), IM_COL32(42, 46, 62, 255), halfTrack);
    if (fillX > trackX0 + 2.0f) {
        dl->AddRectFilled(ImVec2(trackX0, trackCy - halfTrack), ImVec2(fillX, trackCy + halfTrack), GetAccent(1.0f), halfTrack);
    }

    // Generous 13pt-radius (26pt diameter) pure white Apple elevated thumb knob
    float thumbR = active ? 14.0f : 12.0f;
    if (active) {
        dl->AddCircleFilled(ImVec2(fillX, trackCy), thumbR + 5.0f, GetAccent(0.28f), 28);
    }
    dl->AddCircleFilled(ImVec2(fillX, trackCy + 1.5f), thumbR, IM_COL32(0, 0, 0, 85), 28);
    dl->AddCircleFilled(ImVec2(fillX, trackCy), thumbR, IM_COL32(255, 255, 255, 255), 28);
    dl->AddCircle(ImVec2(fillX, trackCy), thumbR, active ? GetAccent(0.95f) : IM_COL32(200, 210, 225, 90), 28, 1.4f);

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + cardH + 6.0f));
    ImGui::PopID();
    return changed;
}

inline bool ProSliderInt(const char* label, int* v, int vMin, int vMax) {
    float fv = static_cast<float>(*v);
    bool ch = ProSlider(label, &fv, static_cast<float>(vMin), static_cast<float>(vMax), "%.0f");
    if (ch) *v = std::clamp(static_cast<int>(std::lround(fv)), vMin, vMax);
    return ch;
}

// iOS Spotlight Search Bar Pill — Returns: 1 = Tap to open iOS keyboard, 2 = Tap Clear 'X', 0 = None
inline int SearchPillButton(const char* id, const char* placeholder, const char* currentText) {
    ImGui::PushID(id);
    float avail = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = 38.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool hasText = (currentText && currentText[0] != '\0');
    float clearW = hasText ? 34.0f : 0.0f;

    dl->AddRectFilled(p, ImVec2(p.x + avail, p.y + h), IM_COL32(28, 32, 43, 245), 11.0f);
    dl->AddRect(p, ImVec2(p.x + avail, p.y + h), hasText ? GetAccent(0.55f) : IM_COL32(255, 255, 255, 20), 11.0f, 0, 1.0f);

    // Vector Magnifying Glass icon
    ImVec2 mc(p.x + 18.0f, p.y + h * 0.5f - 1.0f);
    ImU32 iconCol = hasText ? GetAccent(0.95f) : GetMutedText();
    dl->AddCircle(mc, 5.5f, iconCol, 18, 1.7f);
    dl->AddLine(ImVec2(mc.x + 4.0f, mc.y + 4.0f), ImVec2(mc.x + 8.5f, mc.y + 8.5f), iconCol, 1.9f);

    const char* shown = hasText ? currentText : placeholder;
    ImVec2 ts = ImGui::CalcTextSize(shown);
    dl->PushClipRect(ImVec2(p.x + 32.0f, p.y), ImVec2(p.x + avail - clearW - 8.0f, p.y + h), true);
    dl->AddText(ImVec2(p.x + 34.0f, p.y + (h - ts.y) * 0.5f), hasText ? GetText() : GetMutedText(), shown);
    dl->PopClipRect();

    int action = 0;
    ImGui::SetCursorScreenPos(p);
    if (ImGui::InvisibleButton("##srch_tap", ImVec2(std::max(40.0f, avail - clearW), h))) {
        action = 1;
    }

    if (hasText) {
        ImVec2 cA(p.x + avail - clearW, p.y);
        ImGui::SetCursorScreenPos(cA);
        if (ImGui::InvisibleButton("##srch_clr", ImVec2(clearW, h))) {
            action = 2;
        }
        ImVec2 cc(cA.x + clearW * 0.5f, cA.y + h * 0.5f);
        dl->AddCircleFilled(cc, 9.5f, IM_COL32(62, 68, 86, 235), 18);
        dl->AddLine(ImVec2(cc.x - 3.5f, cc.y - 3.5f), ImVec2(cc.x + 3.5f, cc.y + 3.5f), GetText(), 1.6f);
        dl->AddLine(ImVec2(cc.x + 3.5f, cc.y - 3.5f), ImVec2(cc.x - 3.5f, cc.y + 3.5f), GetText(), 1.6f);
    }

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 6.0f));
    ImGui::PopID();
    return action;
}

// Tappable iOS Input Value Row (triggers native iOS numeric/text keyboard)
inline bool InputValueRow(const char* label, const char* valueText) {
    ImGui::PushID(label);
    float avail = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = 40.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p, ImVec2(p.x + avail, p.y + h), GetCardBg(0.95f), 11.0f);
    dl->AddRect(p, ImVec2(p.x + avail, p.y + h), IM_COL32(255, 255, 255, 18), 11.0f, 0, 1.0f);

    const char* endTxt = std::strstr(label, "##");
    ImVec2 ts = endTxt ? ImGui::CalcTextSize(label, endTxt) : ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + 12.0f, p.y + (h - ts.y) * 0.5f), IM_COL32(198, 204, 218, 245), label, endTxt);

    const char* vStr = (valueText && valueText[0]) ? valueText : "-";
    ImVec2 vs = ImGui::CalcTextSize(vStr);
    float pillW = std::max(72.0f, vs.x + 22.0f);
    ImVec2 vpA(p.x + avail - pillW - 10.0f, p.y + 6.0f);
    ImVec2 vpB(vpA.x + pillW, p.y + h - 6.0f);
    dl->AddRectFilled(vpA, vpB, IM_COL32(38, 42, 56, 255), 8.0f);
    dl->AddRect(vpA, vpB, GetAccent(0.45f), 8.0f, 0, 1.0f);
    dl->AddText(ImVec2(vpA.x + (pillW - vs.x) * 0.5f, vpA.y + ((vpB.y - vpA.y) - vs.y) * 0.5f), GetText(), vStr);

    ImGui::SetCursorScreenPos(p);
    bool clicked = ImGui::InvisibleButton("##ivr", ImVec2(avail, h));
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 6.0f));
    ImGui::PopID();
    return clicked;
}

inline void SectionSpacer(const char* title) {
    ImGui::Spacing();
    ImGui::TextColored(HsvToVec4(g_state.hue, g_state.sat * 0.78f, 1.0f), "%s", title);
    ImGui::Spacing();
}

// Legacy compatibility wrappers
inline void BeginGlassCard(const char* title, const char* id) {
    SectionSpacer(title);
    ImGui::PushID(id);
}
inline void EndGlassCard() {
    ImGui::PopID();
    ImGui::Spacing();
}
inline bool GlassToggle(const char* label, bool* v) { return ProToggle(label, v, nullptr); }

// ============================================================================
// Live Interactive ESP Preview Widget
// ============================================================================
struct PreviewConfig {
    bool box = true;
    int  boxStyle = 0;
    bool health = true;
    bool armor = true;
    bool name = true;
    bool playerId = true;
    bool skeleton = true;
    bool weapon = true;
    bool distance = true;
    bool snapline = false;
    int  lineOrigin = 1;
    ImU32 boxCol = IM_COL32(10, 132, 255, 255);
    ImU32 skelCol = IM_COL32(20, 22, 28, 220);
};

inline void RenderInteractivePreview(const PreviewConfig& cfg, int& previewPose) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float availW = ImGui::GetContentRegionAvail().x;
    float availH = std::max(270.0f, ImGui::GetContentRegionAvail().y - 6.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Clean iOS elevated card backdrop for preview
    dl->AddRectFilled(p, ImVec2(p.x + availW, p.y + availH), GetCardBg(0.88f), 14.0f);
    dl->AddRect(p, ImVec2(p.x + availW, p.y + availH), IM_COL32(255, 255, 255, 18), 14.0f, 0, 1.0f);

    const char* hdr = "ESP Preview";
    ImVec2 hSz = ImGui::CalcTextSize(hdr);
    dl->AddText(ImVec2(p.x + (availW - hSz.x) * 0.5f, p.y + 10.0f), GetMutedText(), hdr);

    float cx = p.x + availW * 0.47f;
    float topY = p.y + 48.0f;
    float figH = std::min(availH - 86.0f, 194.0f);
    float figW = figH * 0.46f;
    float x0 = cx - figW * 0.5f;
    float y0 = topY;
    float yBot = topY + figH;

    dl->AddCircleFilled(ImVec2(cx, yBot - 6.0f), figW * 0.55f, IM_COL32(10, 12, 18, 130), 28);

    float headR = figH * 0.085f;
    ImVec2 headC(cx, y0 + headR + 4.0f);
    ImVec2 neckP(cx, headC.y + headR);
    ImVec2 shL(cx - figW * 0.30f, y0 + figH * 0.22f);
    ImVec2 shR(cx + figW * 0.30f, y0 + figH * 0.22f);
    ImVec2 elL(cx - figW * 0.48f, y0 + figH * 0.40f);
    ImVec2 elR(cx + figW * 0.48f, y0 + figH * 0.40f);
    ImVec2 hnL(cx - figW * 0.62f, y0 + figH * 0.56f);
    ImVec2 hnR(cx + figW * 0.62f, y0 + figH * 0.56f);
    ImVec2 hipC(cx, y0 + figH * 0.52f);
    ImVec2 knL(cx - figW * 0.20f, y0 + figH * 0.74f);
    ImVec2 knR(cx + figW * 0.20f, y0 + figH * 0.74f);
    ImVec2 ftL(cx - figW * 0.24f, yBot - 6.0f);
    ImVec2 ftR(cx + figW * 0.24f, yBot - 6.0f);

    ImU32 skinCol  = IM_COL32(168, 132, 98, 240);
    ImU32 armCol   = (previewPose == 0) ? IM_COL32(152, 128, 38, 220) : IM_COL32(72, 148, 200, 220);
    ImU32 torsoCol = (previewPose == 0) ? IM_COL32(176, 44, 38, 230)  : IM_COL32(58, 125, 220, 230);
    ImU32 legCol   = IM_COL32(62, 118, 174, 235);
    ImU32 shoeCol  = IM_COL32(92, 178, 240, 245);

    dl->AddLine(shL, elL, armCol, 9.0f); dl->AddLine(elL, hnL, armCol, 7.5f);
    dl->AddLine(shR, elR, armCol, 9.0f); dl->AddLine(elR, hnR, armCol, 7.5f);
    ImVec2 torso[4] = { {shL.x + 2, shL.y - 3}, {shR.x - 2, shR.y - 3}, {cx + figW * 0.24f, hipC.y}, {cx - figW * 0.24f, hipC.y} };
    dl->AddConvexPolyFilled(torso, 4, torsoCol);
    dl->AddLine(ImVec2(cx - figW * 0.14f, hipC.y), knL, legCol, 13.0f);
    dl->AddLine(knL, ftL, legCol, 11.0f);
    dl->AddLine(ImVec2(cx + figW * 0.14f, hipC.y), knR, legCol, 13.0f);
    dl->AddLine(knR, ftR, legCol, 11.0f);
    dl->AddCircleFilled(ftL, 6.5f, shoeCol);
    dl->AddCircleFilled(ftR, 6.5f, shoeCol);
    dl->AddCircleFilled(headC, headR, skinCol, 24);
    dl->AddCircleFilled(ImVec2(headC.x, headC.y - headR * 0.42f), headR * 0.72f, IM_COL32(48, 36, 28, 240), 20);

    if (cfg.skeleton) {
        ImU32 sC = IM_COL32(238, 242, 252, 220);
        dl->AddLine(headC, neckP, sC, 1.4f);
        dl->AddLine(neckP, hipC, sC, 1.4f);
        dl->AddLine(neckP, shL, sC, 1.4f); dl->AddLine(shL, elL, sC, 1.4f); dl->AddLine(elL, hnL, sC, 1.4f);
        dl->AddLine(neckP, shR, sC, 1.4f); dl->AddLine(shR, elR, sC, 1.4f); dl->AddLine(elR, hnR, sC, 1.4f);
        dl->AddLine(hipC, knL, sC, 1.4f);  dl->AddLine(knL, ftL, sC, 1.4f);
        dl->AddLine(hipC, knR, sC, 1.4f);  dl->AddLine(knR, ftR, sC, 1.4f);
    }

    if (cfg.box) {
        if (cfg.boxStyle == 1) {
            float cw = figW * 0.26f, ch = figH * 0.18f;
            auto seg = [&](ImVec2 a, ImVec2 b) { dl->AddLine(a, b, cfg.boxCol, 1.6f); };
            seg({x0, y0}, {x0 + cw, y0}); seg({x0, y0}, {x0, y0 + ch});
            seg({x0 + figW - cw, y0}, {x0 + figW, y0}); seg({x0 + figW, y0}, {x0 + figW, y0 + ch});
            seg({x0, yBot}, {x0 + cw, yBot}); seg({x0, yBot - ch}, {x0, yBot});
            seg({x0 + figW - cw, yBot}, {x0 + figW, yBot}); seg({x0 + figW, yBot - ch}, {x0 + figW, yBot});
        } else {
            dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + figW, yBot), cfg.boxCol, 3.0f, 0, 1.5f);
        }
    }

    if (cfg.health) {
        float bx = x0 - 11.0f;
        dl->AddRectFilled(ImVec2(bx, y0), ImVec2(bx + 3.0f, yBot), IM_COL32(255, 255, 255, 45), 1.5f);
        dl->AddRectFilled(ImVec2(bx, y0 + figH * 0.30f), ImVec2(bx + 3.0f, yBot), IM_COL32(48, 209, 88, 255), 1.5f);
    }

    if (cfg.armor) {
        dl->AddRectFilled(ImVec2(x0 + 2, yBot + 7.0f), ImVec2(x0 + figW - 2, yBot + 10.0f), IM_COL32(10, 132, 255, 255), 1.5f);
    }

    if (cfg.name) {
        const char* nm = (previewPose == 0) ? "OneStatePlayer" : "ClanMate";
        ImVec2 nSz = ImGui::CalcTextSize(nm);
        dl->AddText(ImVec2(cx - nSz.x * 0.5f, y0 - 18.0f), GetText(), nm);
    }

    if (cfg.playerId) {
        dl->AddText(ImVec2(x0 + 4.0f, y0 + 2.0f), IM_COL32(220, 224, 238, 225), "ID: 1");
    }

    if (cfg.weapon || cfg.distance) {
        const char* bt = (cfg.weapon && cfg.distance) ? "AK-47 · 24m" : (cfg.weapon ? "AK-47" : "24m");
        ImVec2 bSz = ImGui::CalcTextSize(bt);
        dl->AddText(ImVec2(cx - bSz.x * 0.5f, yBot + 15.0f), IM_COL32(196, 205, 228, 230), bt);
    }

    if (cfg.snapline) {
        float sy = (cfg.lineOrigin == 0) ? (p.y + 28.0f) : ((cfg.lineOrigin == 1) ? (yBot + 32.0f) : (y0 + figH * 0.5f));
        dl->AddLine(ImVec2(cx, sy), ImVec2(cx, yBot), cfg.boxCol, 1.2f);
    }

    float pillX = p.x + availW - 20.0f;
    float pillY = y0 + figH * 0.36f;
    dl->AddRectFilled(ImVec2(pillX - 9, pillY - 22), ImVec2(pillX + 9, pillY + 22), IM_COL32(36, 40, 52, 235), 9.0f);
    dl->AddCircleFilled(ImVec2(pillX, pillY - 8), 3.6f, (previewPose == 0) ? GetAccent(1.0f) : IM_COL32(98, 106, 130, 210));
    dl->AddCircleFilled(ImVec2(pillX, pillY + 8), 3.6f, (previewPose == 1) ? GetAccent(1.0f) : IM_COL32(98, 106, 130, 210));
    ImGui::SetCursorScreenPos(ImVec2(pillX - 10, pillY - 22));
    if (ImGui::InvisibleButton("##posePill", ImVec2(20, 44))) {
        previewPose = 1 - previewPose;
    }
}

// Dedicated iPhone Vertical Scrollbar Rail: [Up] + Wide Draggable Track/Thumb + [Down]
inline bool DrawTouchScrollRail(const char* id, ImVec2 pos, float railW, float railH, float curScrollY, float maxScrollY, float& outNewScrollY) {
    ImGui::PushID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool changed = false;
    outNewScrollY = curScrollY;

    const float btnH   = 32.0f;
    const float gap    = 5.0f;
    const float trackY = pos.y + btnH + gap;
    const float trackH = std::max(40.0f, railH - (btnH + gap) * 2.0f);
    const float downY  = trackY + trackH + gap;
    const bool  canScroll = (maxScrollY > 1.0f);
    const float stepPx = std::max(36.0f, railH * 0.16f);

    // 1. Up Button [▲]
    ImGui::SetCursorScreenPos(pos);
    bool upClick = ImGui::InvisibleButton("##sc_up", ImVec2(railW, btnH));
    bool upHeld  = ImGui::IsItemActive();
    dl->AddRectFilled(pos, ImVec2(pos.x + railW, pos.y + btnH),
                      upHeld ? GetAccent(0.85f) : IM_COL32(36, 40, 54, 240), 9.0f);
    dl->AddRect(pos, ImVec2(pos.x + railW, pos.y + btnH), IM_COL32(255, 255, 255, 28), 9.0f, 0, 1.0f);
    {
        float cx = pos.x + railW * 0.5f, cy = pos.y + btnH * 0.5f;
        ImU32 ic = canScroll ? IM_COL32(255, 255, 255, 245) : IM_COL32(130, 138, 158, 120);
        dl->AddTriangleFilled(ImVec2(cx, cy - 5.5f), ImVec2(cx - 6.5f, cy + 4.5f), ImVec2(cx + 6.5f, cy + 4.5f), ic);
    }
    if (canScroll && (upClick || (upHeld && ImGui::GetIO().MouseDownDuration[0] > 0.18f))) {
        float delta = upClick ? stepPx : (stepPx * 0.22f);
        outNewScrollY = std::clamp(curScrollY - delta, 0.0f, maxScrollY);
        changed = true;
    }

    // 2. Wide Finger-Draggable Scrollbar Track + High-Contrast Thumb
    ImVec2 trkA(pos.x, trackY);
    ImVec2 trkB(pos.x + railW, trackY + trackH);
    ImGui::SetCursorScreenPos(trkA);
    ImGui::InvisibleButton("##sc_trk", ImVec2(railW, trackH));
    bool trkHeld = ImGui::IsItemActive();
    bool trkHov  = ImGui::IsItemHovered();

    dl->AddRectFilled(trkA, trkB, IM_COL32(24, 28, 38, 225), railW * 0.45f);
    dl->AddRect(trkA, trkB, IM_COL32(255, 255, 255, 24), railW * 0.45f, 0, 1.0f);

    float visibleRatio = canScroll ? std::clamp(railH / (railH + maxScrollY), 0.16f, 0.92f) : 1.0f;
    float thumbH = canScroll ? std::max(44.0f, trackH * visibleRatio) : (trackH - 6.0f);
    float travel = std::max(1.0f, trackH - thumbH);

    if (canScroll && trkHeld) {
        g_sliderDragging = true;
        float my = ImGui::GetIO().MousePos.y;
        float frac = std::clamp((my - trackY - thumbH * 0.5f) / travel, 0.0f, 1.0f);
        outNewScrollY = frac * maxScrollY;
        changed = true;
    }

    float norm = canScroll ? std::clamp(outNewScrollY / maxScrollY, 0.0f, 1.0f) : 0.0f;
    float thumbY = trackY + (canScroll ? (norm * travel) : 3.0f);
    ImVec2 thA(pos.x + 3.0f, thumbY);
    ImVec2 thB(pos.x + railW - 3.0f, thumbY + thumbH);
    ImU32 thCol = !canScroll
        ? IM_COL32(90, 98, 118, 85)
        : (trkHeld ? GetAccent(1.0f) : (trkHov ? GetAccent(0.90f) : GetAccent(0.78f)));
    dl->AddRectFilled(thA, thB, thCol, (railW - 6.0f) * 0.45f);
    dl->AddRect(thA, thB, IM_COL32(255, 255, 255, canScroll ? 90 : 25), (railW - 6.0f) * 0.45f, 0, 1.0f);

    // Tactile 3-bar grip indicator inside thumb
    if (thumbH >= 28.0f) {
        float gcx = pos.x + railW * 0.5f;
        float gcy = thumbY + thumbH * 0.5f;
        ImU32 gc  = IM_COL32(255, 255, 255, canScroll ? 210 : 75);
        dl->AddLine(ImVec2(gcx - 5.5f, gcy - 4.0f), ImVec2(gcx + 5.5f, gcy - 4.0f), gc, 1.5f);
        dl->AddLine(ImVec2(gcx - 5.5f, gcy),        ImVec2(gcx + 5.5f, gcy),        gc, 1.5f);
        dl->AddLine(ImVec2(gcx - 5.5f, gcy + 4.0f), ImVec2(gcx + 5.5f, gcy + 4.0f), gc, 1.5f);
    }

    // 3. Down Button [▼]
    ImVec2 dnA(pos.x, downY);
    ImGui::SetCursorScreenPos(dnA);
    bool dnClick = ImGui::InvisibleButton("##sc_dn", ImVec2(railW, btnH));
    bool dnHeld  = ImGui::IsItemActive();
    dl->AddRectFilled(dnA, ImVec2(dnA.x + railW, dnA.y + btnH),
                      dnHeld ? GetAccent(0.85f) : IM_COL32(36, 40, 54, 240), 9.0f);
    dl->AddRect(dnA, ImVec2(dnA.x + railW, dnA.y + btnH), IM_COL32(255, 255, 255, 28), 9.0f, 0, 1.0f);
    {
        float cx = dnA.x + railW * 0.5f, cy = dnA.y + btnH * 0.5f;
        ImU32 ic = canScroll ? IM_COL32(255, 255, 255, 245) : IM_COL32(130, 138, 158, 120);
        dl->AddTriangleFilled(ImVec2(cx, cy + 5.5f), ImVec2(cx - 6.5f, cy - 4.5f), ImVec2(cx + 6.5f, cy - 4.5f), ic);
    }
    if (canScroll && (dnClick || (dnHeld && ImGui::GetIO().MouseDownDuration[0] > 0.18f))) {
        float delta = dnClick ? stepPx : (stepPx * 0.22f);
        outNewScrollY = std::clamp(curScrollY + delta, 0.0f, maxScrollY);
        changed = true;
    }

    ImGui::PopID();
    return changed;
}

} // namespace ThemeEngine
