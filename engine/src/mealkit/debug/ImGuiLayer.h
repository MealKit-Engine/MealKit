#pragma once

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

// Dear ImGui를 MealKit 창/렌더러에 연결한다 (Win32 + DX12 백엔드). 에디터와 개발용 게임 빌드의 디버그 UI(MEALKIT_DEBUG_UI)가 쓴다.
// 출시 빌드(Export)에는 들어가지 않는다.
class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    // iniPath가 비어 있으면 창 위치를 저장하지 않는다
    bool init(Window& window, render::Renderer& renderer, const std::string& iniPath);
    void shutdown();

    void newFrame();
    // 렌더러 프레임이 열려 있으면 UI를 그려 넣는다. 열려 있지 않아도 ImGui 프레임은 반드시 닫는다.
    void render(render::Renderer& renderer, bool frameActive);

    // ImGui::Image로 그릴 RGBA8 텍스처. ImGui 백엔드가 다음 프레임에 GPU에 올린다. 다 쓰면 releaseTexture
    ImTextureData* createTexture(const u8* rgba, u32 width, u32 height);
    void releaseTexture(ImTextureData* texture); // GPU가 다 쓴 뒤(몇 프레임 뒤) 지운다

private:
    bool m_initialized = false;
    Window* m_window = nullptr;
    render::Renderer* m_renderer = nullptr;
    std::string m_iniPath;
    void* m_srvHeap = nullptr; // ID3D12DescriptorHeap*
    u64 m_srvCpuStart = 0;
    u64 m_srvGpuStart = 0;
    u32 m_srvStride = 0;
    std::vector<u32> m_freeSlots;
    std::vector<ImTextureData*> m_textures; // createTexture로 만든 것 (지워질 때까지)

    friend struct ImGuiLayerAccess;
};

} // namespace mealkit::debug
