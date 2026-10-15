#include "mealkit/editor/ImGuiLayer.h"

#include "mealkit/core/Log.h"
#include "mealkit/platform/Paths.h"
#include "mealkit/platform/Window.h"
#include "mealkit/render/Renderer.h"

#include <d3d12.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace mealkit::editor {

constexpr u32 kSrvCapacity = 64;

struct ImGuiLayerAccess {
    static void alloc(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
        auto* self = static_cast<ImGuiLayer*>(info->UserData);
        if (self->m_freeSlots.empty()) MEALKIT_FATAL("ImGui descriptor heap exhausted");
        const u32 slot = self->m_freeSlots.back();
        self->m_freeSlots.pop_back();
        cpu->ptr = static_cast<SIZE_T>(self->m_srvCpuStart + static_cast<u64>(slot) * self->m_srvStride);
        gpu->ptr = self->m_srvGpuStart + static_cast<u64>(slot) * self->m_srvStride;
    }
    static void free(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE) {
        auto* self = static_cast<ImGuiLayer*>(info->UserData);
        self->m_freeSlots.push_back(static_cast<u32>((cpu.ptr - self->m_srvCpuStart) / self->m_srvStride));
    }
};

namespace {
std::intptr_t messageHook(void*, void* hwnd, u32 msg, std::uintptr_t wparam, std::intptr_t lparam) {
    return ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(hwnd), msg, wparam, lparam);
}

void applyStyle(float scale) {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.TabRounding = 3.0f;
    s.WindowBorderSize = 1.0f;
    s.FramePadding = ImVec2(6.0f, 4.0f);
    s.ItemSpacing = ImVec2(8.0f, 5.0f);
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    c[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.12f, 0.14f, 0.98f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.19f, 0.20f, 0.23f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.26f, 0.30f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.30f, 0.35f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.26f, 0.36f, 0.52f, 0.80f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.42f, 0.60f, 0.80f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.33f, 0.46f, 0.66f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.22f, 0.24f, 0.28f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.42f, 0.60f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.33f, 0.46f, 0.66f, 1.00f);
    c[ImGuiCol_Tab] = ImVec4(0.15f, 0.16f, 0.19f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.42f, 0.60f, 1.00f);
    c[ImGuiCol_TabSelected] = ImVec4(0.22f, 0.30f, 0.43f, 1.00f);
    c[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
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
    io.IniFilename = m_iniPath.c_str();

    HWND hwnd = static_cast<HWND>(window.nativeHandle());
    const float scale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
    applyStyle(scale);

    // 한국어 UI: 맑은 고딕. 1.92부터 글리프는 필요할 때 만들어지므로 전체 한글 범위를 미리 굽지 않는다.
    const char* fontPath = "C:\\Windows\\Fonts\\malgun.ttf";
    if (fileExists(fontPath)) io.Fonts->AddFontFromFileTTF(fontPath, 16.0f);
    else io.Fonts->AddFontDefault();

    if (!ImGui_ImplWin32_Init(hwnd)) return false;
    window.setMessageHook(messageHook, nullptr);

    const render::NativeHandles native = renderer.native();
    auto* device = static_cast<ID3D12Device*>(native.device);
    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = kSrvCapacity;
    hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ID3D12DescriptorHeap* heap = nullptr;
    if (FAILED(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)))) return false;
    m_srvHeap = heap;
    m_srvCpuStart = heap->GetCPUDescriptorHandleForHeapStart().ptr;
    m_srvGpuStart = heap->GetGPUDescriptorHandleForHeapStart().ptr;
    m_srvStride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    for (u32 i = kSrvCapacity; i > 0; --i) m_freeSlots.push_back(i - 1);

    ImGui_ImplDX12_InitInfo info;
    info.Device = device;
    info.CommandQueue = static_cast<ID3D12CommandQueue*>(native.queue);
    info.NumFramesInFlight = static_cast<int>(native.framesInFlight);
    info.RTVFormat = static_cast<DXGI_FORMAT>(native.overlayFormat);
    info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    info.UserData = this;
    info.SrvDescriptorHeap = heap;
    info.SrvDescriptorAllocFn = ImGuiLayerAccess::alloc;
    info.SrvDescriptorFreeFn = ImGuiLayerAccess::free;
    if (!ImGui_ImplDX12_Init(&info)) return false;

    m_initialized = true;
    return true;
}

void ImGuiLayer::shutdown() {
    if (!m_initialized) return;
    m_renderer->waitIdle();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (m_window) m_window->setMessageHook(nullptr, nullptr);
    if (m_srvHeap) static_cast<ID3D12DescriptorHeap*>(m_srvHeap)->Release();
    m_srvHeap = nullptr;
    m_initialized = false;
}

void ImGuiLayer::newFrame() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::render(render::Renderer& renderer, bool frameActive) {
    ImGui::Render();
    if (!frameActive) return;
    const render::NativeHandles native = renderer.native();
    auto* cmd = static_cast<ID3D12GraphicsCommandList*>(native.commandList);
    if (!cmd) return;
    renderer.beginOverlay();
    ID3D12DescriptorHeap* heaps[] = {static_cast<ID3D12DescriptorHeap*>(m_srvHeap)};
    cmd->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);
}

} // namespace mealkit::editor
