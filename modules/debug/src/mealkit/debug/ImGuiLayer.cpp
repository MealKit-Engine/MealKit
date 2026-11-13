#include "mealkit/debug/ImGuiLayer.h"

#include "mealkit/core/Log.h"
#include "mealkit/platform/ImGuiPlatform.h"
#include "mealkit/platform/Paths.h"
#include "mealkit/platform/Window.h"
#include "mealkit/render/Renderer.h"
#include "mealkit/rhi/Rhi.h"

#include <cstring>
#include <imgui.h>
#include <imgui_internal.h> // RegisterUserTexture

namespace mealkit::debug {
namespace {
void applyStyle(float scale) {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    // Spectrum 2 shape (engine docs/TOOL_DESIGN.md 3.1): 8 px fields, 10 px panels and popups.
    // Panels are separated by space and a darker gutter, not by lines
    s.WindowRounding = 10.0f;
    s.ChildRounding = 10.0f;
    s.PopupRounding = 10.0f;
    s.FrameRounding = 8.0f;
    s.GrabRounding = 4.0f;
    s.TabRounding = 8.0f;
    s.ScrollbarRounding = 6.0f;
    s.ScrollbarSize = 10.0f;
    s.WindowBorderSize = 0.0f; // layers are told apart by color, not outlines
    s.ChildBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.PopupBorderSize = 1.0f;
    s.TabBarBorderSize = 0.0f;
    s.TabBarOverlineSize = 0.0f;
    s.DockingSeparatorSize = 4.0f;
    s.DockingNodeHasCloseButton = false; // each tab has its own close button
    s.TabCloseButtonMinWidthSelected = 0.0f; // tabs show it only while hovered
    s.TabCloseButtonMinWidthUnselected = 0.0f;
    s.WindowPadding = ImVec2(10.0f, 10.0f);
    s.FramePadding = ImVec2(7.0f, 4.0f);
    s.ItemSpacing = ImVec2(8.0f, 6.0f);
    s.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    s.CellPadding = ImVec2(6.0f, 3.0f);
    s.IndentSpacing = 16.0f;
    s.WindowMenuButtonPosition = ImGuiDir_None; // no triangle on every dock tab bar: tabs are all visible and can be dragged
    // Tokens of engine docs/TOOL_DESIGN.md 3.1: Spectrum 2 dark gray-25..gray-900 (gray 1..12) and blue. Pick a step by its job, never a color by eye
    const auto rgb = [](u32 v, f32 a = 1.0f) {
        return ImVec4(((v >> 16) & 0xFF) / 255.0f, ((v >> 8) & 0xFF) / 255.0f, (v & 0xFF) / 255.0f, a);
    };
    const auto gray = [&](int step) {
        static constexpr u32 kGray[13] = {0, 0x111111, 0x1B1B1B, 0x222222, 0x2C2C2C, 0x323232, 0x393939,
                                          0x444444, 0x6D6D6D, 0x8A8A8A, 0x909090, 0xAFAFAF, 0xEBEBEB};
        return rgb(kGray[step]);
    };
    const ImVec4 clear(0.0f, 0.0f, 0.0f, 0.0f);
    const ImVec4 focus = rgb(0x5D89FF);  // keyboard focus, hovered splitters
    const ImVec4 accent = rgb(0x3B63FB); // selection, check marks, active grabs
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = gray(12);
    c[ImGuiCol_TextDisabled] = gray(11); // secondary text: paths, dates, hints
    c[ImGuiCol_TextSelectedBg] = rgb(0x3B63FB, 0.45f);
    c[ImGuiCol_WindowBg] = gray(2);
    c[ImGuiCol_ChildBg] = clear;
    c[ImGuiCol_PopupBg] = gray(3);
    c[ImGuiCol_Border] = gray(6);
    c[ImGuiCol_BorderShadow] = clear;
    c[ImGuiCol_MenuBarBg] = gray(1);
    c[ImGuiCol_TitleBg] = gray(1);
    c[ImGuiCol_TitleBgActive] = gray(1);
    c[ImGuiCol_TitleBgCollapsed] = gray(1);
    // Hover is one neutral step up, pressed and selected one more; the accent never fills a hovered control
    c[ImGuiCol_FrameBg] = gray(4); // fields and buttons share a step, so a combo and its arrow read as one control
    c[ImGuiCol_FrameBgHovered] = gray(5);
    c[ImGuiCol_FrameBgActive] = gray(6);
    c[ImGuiCol_Header] = gray(5);
    c[ImGuiCol_HeaderHovered] = gray(4);
    c[ImGuiCol_HeaderActive] = gray(6);
    c[ImGuiCol_Button] = gray(4);
    c[ImGuiCol_ButtonHovered] = gray(6);
    c[ImGuiCol_ButtonActive] = gray(7);
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = gray(9);
    c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_Separator] = gray(5);
    c[ImGuiCol_SeparatorHovered] = focus;
    c[ImGuiCol_SeparatorActive] = accent;
    c[ImGuiCol_ResizeGrip] = clear;
    c[ImGuiCol_ResizeGripHovered] = focus;
    c[ImGuiCol_ResizeGripActive] = accent;
    c[ImGuiCol_ScrollbarBg] = clear;
    c[ImGuiCol_ScrollbarGrab] = gray(6);
    c[ImGuiCol_ScrollbarGrabHovered] = gray(7);
    c[ImGuiCol_ScrollbarGrabActive] = gray(8);
    // The selected tab joins its panel; the others sit on the strip
    c[ImGuiCol_Tab] = clear;
    c[ImGuiCol_TabHovered] = gray(4);
    c[ImGuiCol_TabSelected] = gray(2);
    c[ImGuiCol_TabSelectedOverline] = clear;
    c[ImGuiCol_TabDimmed] = clear;
    c[ImGuiCol_TabDimmedSelected] = gray(2);
    c[ImGuiCol_TabDimmedSelectedOverline] = clear;
    c[ImGuiCol_TableHeaderBg] = clear; // no header band: quiet header text over a line
    c[ImGuiCol_TableBorderStrong] = gray(6);
    c[ImGuiCol_TableBorderLight] = gray(5);
    c[ImGuiCol_TableRowBgAlt] = clear; // no zebra rows
    c[ImGuiCol_DockingPreview] = rgb(0x3B63FB, 0.5f);
    c[ImGuiCol_DockingEmptyBg] = clear;
    c[ImGuiCol_NavCursor] = focus;
    s.ScaleAllSizes(scale);
    s.FontScaleDpi = scale;
}
} // namespace

ImGuiLayer::~ImGuiLayer() { shutdown(); }

bool ImGuiLayer::init(Window& window, render::Renderer& renderer, const std::string& iniPath) {
    m_window = &window;
    m_renderer = &renderer;
    m_iniPath = iniPath;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    io.IniFilename = m_iniPath.empty() ? nullptr : m_iniPath.c_str();

    const float scale = imguiDpiScale(window);
    applyStyle(scale);

    // Merges Segoe UI for Latin and Malgun Gothic for Korean (Malgun Gothic alone shows backslash as ₩).
    // Since 1.92 glyphs are built on demand, so the full Hangul range isn't prebaked.
    bool haveFont = false;
    for (const char* fontPath : {"C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\malgun.ttf"}) {
        if (!fileExists(fontPath)) continue;
        ImFontConfig config;
        config.MergeMode = haveFont;
        io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, &config);
        haveFont = true;
    }
    if (!haveFont) io.Fonts->AddFontDefault();
    // Icon glyphs (icon::): Segoe Fluent Icons on Windows 11, Segoe MDL2 Assets on 10. Same code points
    for (const char* iconPath : {"C:\\Windows\\Fonts\\SegoeIcons.ttf", "C:\\Windows\\Fonts\\segmdl2.ttf"}) {
        if (!fileExists(iconPath)) continue;
        ImFontConfig merge;
        merge.MergeMode = true;
        merge.GlyphOffset = ImVec2(0.0f, 3.0f); // the icon font has a high baseline, so it's shifted down to the middle of Korean text lines
        io.Fonts->AddFontFromFileTTF(iconPath, 16.0f, &merge);
        break;
    }

    if (!imguiPlatformInit(window)) return false;
    if (!rhi::imguiInit(renderer.device())) return false;

    m_initialized = true;
    return true;
}

void ImGuiLayer::shutdown() {
    if (!m_initialized) return;
    rhi::imguiShutdown(m_renderer->device()); // also frees GPU resources of user textures
    if (m_window) imguiPlatformShutdown(*m_window);
    for (ImTextureData* tex : m_textures) {
        ImGui::UnregisterUserTexture(tex);
        IM_DELETE(tex);
    }
    m_textures.clear();
    ImGui::DestroyContext();
    m_initialized = false;
}

ImTextureData* ImGuiLayer::createTexture(const u8* rgba, u32 width, u32 height) {
    auto* tex = IM_NEW(ImTextureData)();
    tex->Create(ImTextureFormat_RGBA32, static_cast<int>(width), static_cast<int>(height));
    std::memcpy(tex->GetPixels(), rgba, static_cast<size_t>(width) * height * 4);
    ImGui::RegisterUserTexture(tex);
    m_textures.push_back(tex);
    return tex;
}

void ImGuiLayer::releaseTexture(ImTextureData* texture) {
    if (texture) texture->WantDestroyNextFrame = true; // the backend changes it to Destroyed after GPU use ends (deleted in newFrame)
}

bool ImGuiLayer::setIniPath(const std::string& iniPath) {
    const bool exists = fileExists(iniPath);
    if (!m_initialized || iniPath == m_iniPath) return exists;
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::GetCurrentContext()->SettingsLoaded) { // before the first frame, NewFrame reads the new file
        if (!m_iniPath.empty()) ImGui::SaveIniSettingsToDisk(m_iniPath.c_str());
        ImGui::ClearIniSettings();
        if (exists) ImGui::LoadIniSettingsFromDisk(iniPath.c_str());
    }
    m_iniPath = iniPath;
    io.IniFilename = m_iniPath.c_str();
    return exists;
}

void ImGuiLayer::newFrame(const std::function<void()>& inject) {
    for (size_t i = 0; i < m_textures.size();) {
        ImTextureData* tex = m_textures[i];
        if (tex->WantDestroyNextFrame && tex->Status == ImTextureStatus_Destroyed) {
            ImGui::UnregisterUserTexture(tex);
            IM_DELETE(tex);
            m_textures.erase(m_textures.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
    rhi::imguiNewFrame();
    imguiPlatformNewFrame();
    if (inject) inject();
    ImGui::NewFrame();
}

void ImGuiLayer::render(render::Renderer& renderer, bool frameActive) {
    ImGui::Render();
    if (!frameActive) return;
    renderer.beginOverlay();
    rhi::imguiRender(renderer.device());
}

} // namespace mealkit::debug
