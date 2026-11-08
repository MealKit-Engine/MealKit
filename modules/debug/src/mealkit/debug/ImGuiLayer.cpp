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
    // Panels are separated by space and a darker gutter, not by lines
    s.WindowRounding = 6.0f;
    s.ChildRounding = 4.0f;
    s.PopupRounding = 6.0f;
    s.FrameRounding = 4.0f;
    s.GrabRounding = 3.0f;
    s.TabRounding = 4.0f;
    s.ScrollbarRounding = 6.0f;
    s.ScrollbarSize = 10.0f;
    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize = 1.0f;
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
    const ImVec4 clear(0.0f, 0.0f, 0.0f, 0.0f);
    const ImVec4 gutter(0.075f, 0.078f, 0.086f, 1.00f); // behind the panels: menu bar, status bar, gaps, tab strips
    const ImVec4 panel(0.118f, 0.122f, 0.133f, 1.00f);
    const auto white = [](f32 alpha) { return ImVec4(1.0f, 1.0f, 1.0f, alpha); };
    const auto accent = [](f32 alpha) { return ImVec4(0.36f, 0.56f, 0.86f, alpha); }; // only for selection and active states
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = ImVec4(0.88f, 0.89f, 0.91f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.56f, 1.00f);
    c[ImGuiCol_TextSelectedBg] = accent(0.35f);
    c[ImGuiCol_WindowBg] = panel;
    c[ImGuiCol_ChildBg] = clear;
    c[ImGuiCol_PopupBg] = ImVec4(0.135f, 0.14f, 0.153f, 0.99f);
    c[ImGuiCol_Border] = white(0.06f);
    c[ImGuiCol_BorderShadow] = clear;
    c[ImGuiCol_MenuBarBg] = gutter;
    c[ImGuiCol_TitleBg] = gutter;
    c[ImGuiCol_TitleBgActive] = gutter;
    c[ImGuiCol_TitleBgCollapsed] = gutter;
    c[ImGuiCol_FrameBg] = ImVec4(0.165f, 0.17f, 0.185f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.207f, 0.225f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.235f, 0.243f, 0.265f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.20f, 0.31f, 0.47f, 1.00f);
    c[ImGuiCol_HeaderHovered] = white(0.06f);
    c[ImGuiCol_HeaderActive] = accent(0.55f);
    c[ImGuiCol_Button] = ImVec4(0.165f, 0.17f, 0.185f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.235f, 0.243f, 0.265f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.29f, 0.30f, 0.325f, 1.00f);
    c[ImGuiCol_CheckMark] = ImVec4(0.62f, 0.76f, 0.98f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.42f, 0.44f, 0.48f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = accent(1.0f);
    c[ImGuiCol_Separator] = white(0.07f);
    c[ImGuiCol_SeparatorHovered] = accent(0.55f);
    c[ImGuiCol_SeparatorActive] = accent(0.90f);
    c[ImGuiCol_ResizeGrip] = clear;
    c[ImGuiCol_ResizeGripHovered] = accent(0.40f);
    c[ImGuiCol_ResizeGripActive] = accent(0.80f);
    c[ImGuiCol_ScrollbarBg] = clear;
    c[ImGuiCol_ScrollbarGrab] = white(0.13f);
    c[ImGuiCol_ScrollbarGrabHovered] = white(0.22f);
    c[ImGuiCol_ScrollbarGrabActive] = white(0.30f);
    // The selected tab joins its panel; the others sit on the strip
    c[ImGuiCol_Tab] = clear;
    c[ImGuiCol_TabHovered] = white(0.07f);
    c[ImGuiCol_TabSelected] = panel;
    c[ImGuiCol_TabSelectedOverline] = clear;
    c[ImGuiCol_TabDimmed] = clear;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_TabDimmedSelectedOverline] = clear;
    c[ImGuiCol_TableHeaderBg] = white(0.04f);
    c[ImGuiCol_TableBorderStrong] = white(0.07f);
    c[ImGuiCol_TableBorderLight] = white(0.05f);
    c[ImGuiCol_TableRowBgAlt] = white(0.02f);
    c[ImGuiCol_DockingPreview] = accent(0.50f);
    c[ImGuiCol_DockingEmptyBg] = clear;
    c[ImGuiCol_NavCursor] = accent(0.90f);
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
