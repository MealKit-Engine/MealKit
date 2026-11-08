#include "mealkit/platform/Image.h"

#include "mealkit/core/Log.h"

#include <windows.h>
#include <wincodec.h>

namespace mealkit {
namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

template <class T> struct ComPtr {
    T* p = nullptr;
    ~ComPtr() {
        if (p) p->Release();
    }
    T** operator&() { return &p; }
    T* operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};

IWICImagingFactory* factory() {
    static IWICImagingFactory* f = [] {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        IWICImagingFactory* created = nullptr;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&created))))
            MEALKIT_WARN("WIC imaging factory unavailable");
        return created;
    }();
    return f;
}

bool decode(IWICBitmapDecoder* decoder, u32 maxSize, Image& out) {
    // For multiple images (ICO), the largest
    UINT frames = 0;
    decoder->GetFrameCount(&frames);
    ComPtr<IWICBitmapFrameDecode> best;
    UINT bestWidth = 0;
    for (UINT i = 0; i < frames; ++i) {
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(i, &frame))) continue;
        UINT w = 0, h = 0;
        frame->GetSize(&w, &h);
        if (w > bestWidth) {
            bestWidth = w;
            if (best.p) best.p->Release();
            best.p = frame.p;
            frame.p = nullptr;
        }
    }
    if (!best) return false;

    UINT w = 0, h = 0;
    best->GetSize(&w, &h);
    if (w == 0 || h == 0) return false;
    IWICBitmapSource* source = best.p;
    ComPtr<IWICBitmapScaler> scaler;
    if (maxSize && (w > maxSize || h > maxSize)) {
        const double s = static_cast<double>(maxSize) / (w > h ? w : h);
        const UINT sw = w * s < 1.0 ? 1 : static_cast<UINT>(w * s), sh = h * s < 1.0 ? 1 : static_cast<UINT>(h * s);
        if (FAILED(factory()->CreateBitmapScaler(&scaler)) ||
            FAILED(scaler->Initialize(source, sw, sh, WICBitmapInterpolationModeHighQualityCubic)))
            return false;
        source = scaler.p;
        w = sw;
        h = sh;
    }
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory()->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(source, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom)))
        return false;
    out.width = w;
    out.height = h;
    out.rgba.resize(static_cast<size_t>(w) * h * 4);
    return SUCCEEDED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(out.rgba.size()), out.rgba.data()));
}

} // namespace

bool loadImage(const std::string& path, u32 maxSize, Image& out) {
    if (!factory()) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory()->CreateDecoderFromFilename(widen(path).c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnDemand, &decoder)))
        return false;
    return decode(decoder.p, maxSize, out);
}

bool loadImageResource(int resourceId, u32 maxSize, Image& out) {
    if (!factory()) return false;
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(module, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10)); // RT_RCDATA
    if (!res) return false;
    HGLOBAL data = LoadResource(module, res);
    const DWORD size = SizeofResource(module, res);
    if (!data || size == 0) return false;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory()->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromMemory(static_cast<BYTE*>(LockResource(data)), size)) ||
        FAILED(factory()->CreateDecoderFromStream(stream.p, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)))
        return false;
    return decode(decoder.p, maxSize, out);
}

bool savePng(const std::string& path, const Image& image) {
    if (!factory() || image.rgba.empty()) return false;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    if (FAILED(factory()->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromFilename(widen(path).c_str(), GENERIC_WRITE)) ||
        FAILED(factory()->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
        FAILED(encoder->Initialize(stream.p, WICBitmapEncoderNoCache)) ||
        FAILED(encoder->CreateNewFrame(&frame, nullptr)) || FAILED(frame->Initialize(nullptr)) ||
        FAILED(frame->SetSize(image.width, image.height)))
        return false;
    // If the PNG encoder doesn't accept RGBA, convert to BGRA
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
    if (FAILED(frame->SetPixelFormat(&format))) return false;
    std::vector<u8> pixels = image.rgba;
    if (IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)) {
        for (size_t i = 0; i + 3 < pixels.size(); i += 4) std::swap(pixels[i], pixels[i + 2]);
    } else if (!IsEqualGUID(format, GUID_WICPixelFormat32bppRGBA)) {
        return false;
    }
    return SUCCEEDED(frame->WritePixels(image.height, image.width * 4, static_cast<UINT>(pixels.size()), pixels.data())) &&
           SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
}

} // namespace mealkit
