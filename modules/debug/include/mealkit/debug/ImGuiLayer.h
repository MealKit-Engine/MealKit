// Vendored copy of module-debug v0.1.4; change it in that repo (tools\module.ps1 update), not here.
#pragma once

#include <functional>

#include "mealkit/core/Types.h"

#include <string>
#include <vector>

struct ImTextureData;

namespace mealkit {
class Window;
namespace render {
class Renderer;
}
} // namespace mealkit

namespace mealkit::debug {

// Connects Dear ImGui to the MealKit window/renderer (window side is platform/ImGuiPlatform.h, drawing via RHI). Used by the debug UI (MEALKIT_DEBUG_UI) of the editor and dev game builds.
// Not included in shipping builds (Export).
class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    // If iniPath is empty, window positions aren't saved. iconFont (the editor: <engine>/third_party/fluent-icons/FluentSystemIcons-Regular.ttf)
    // is merged into the default font for icon glyphs; empty = no icons (the dev game overlay)
    bool init(Window& window, render::Renderer& renderer, const std::string& iniPath, const std::string& iconFont = {});
    void shutdown();
    // Switches the window layout file (per-project layouts). The already loaded layout is written to the old file and the new file is read. true if the new file exists
    bool setIniPath(const std::string& iniPath);

    // inject (optional) adds test input after the platform's, so it wins
    void newFrame(const std::function<void()>& inject = {});
    // Draws the UI in if a renderer frame is open. Even if not, the ImGui frame is always closed.
    void render(render::Renderer& renderer, bool frameActive);

    // RGBA8 texture to draw with ImGui::Image. The ImGui backend uploads it to the GPU next frame. Call releaseTexture when done
    ImTextureData* createTexture(const u8* rgba, u32 width, u32 height);
    void releaseTexture(ImTextureData* texture); // deleted after the GPU is done with it (a few frames later)

private:
    bool m_initialized = false;
    Window* m_window = nullptr;
    render::Renderer* m_renderer = nullptr;
    std::string m_iniPath;
    std::vector<ImTextureData*> m_textures; // made with createTexture (until deleted)
};

} // namespace mealkit::debug
