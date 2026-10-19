#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <cstddef>
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>
#include "crabe_plugin.h"
#include "fingerprint.hpp"
#include "glyph_fingerprints.hpp"
using Microsoft::WRL::ComPtr;
namespace {
const CrabePluginApi* api = nullptr;
HMODULE module = nullptr;
std::atomic<bool> enabled{false};
std::atomic<ULONGLONG> nextPoll{0};
std::atomic<unsigned> matched{0}, substitutions{0};
// COM metadata releases replacement textures with their original resources, avoiding stale pointers.
constexpr GUID replacementGuid{0xa863fac3,0xca2d,0x4c54,{0x9c,0x63,0x7d,0x6f,0x77,0x04,0x1a,0x33}};
struct Image { UINT width = 0, height = 0; std::vector<unsigned char> pixels; };
std::array<Image, std::size(kGlyphFiles)> images;
using CreateTexture = HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const D3D11_TEXTURE2D_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Texture2D**);
using SetResources = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,ID3D11ShaderResourceView* const*);
using UpdateResource = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*,UINT,UINT);
CreateTexture createTexture = nullptr;
std::array<SetResources,2> setResources{};
std::array<UpdateResource,2> updateResource{};
void log(int level, const char* line) { if (api) api->log(api,level,line); }

// Keep hardware exception handling in a function without C++ unwind objects.
std::uint64_t safeHash(const void* data, UINT pitch) noexcept
{
    __try { return pixelFingerprint(data,pitch,45,45); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
int findGlyph(const D3D11_TEXTURE2D_DESC& desc, const void* data, UINT pitch) noexcept
{
    if (desc.Width != 45 || desc.Height != 45 || desc.ArraySize != 1 || !(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)) return -1;
    switch (desc.Format) {
        case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_B8G8R8A8_TYPELESS: break;
        default: return -1;
    }
    const auto hash = safeHash(data,pitch);
    for (const auto& entry : kGlyphFingerprints) if (entry.hash == hash) return static_cast<int>(entry.cell);
    return -1;
}
bool loadImages(const std::filesystem::path& directory)
{
    const HRESULT com = CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ComPtr<IWICImagingFactory> factory;
    bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    for (size_t i = 0; ok && i < images.size(); ++i) {
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;
        ComPtr<IWICFormatConverter> converter;
        auto& image = images[i];
        const auto path = directory / L"glyphs" / kGlyphFiles[i];
        ok = SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))
          && SUCCEEDED(decoder->GetFrame(0,&frame)) && SUCCEEDED(frame->GetSize(&image.width,&image.height))
          && image.width > 0 && image.height > 0 && image.width <= 1024 && image.height <= 1024
          && SUCCEEDED(factory->CreateFormatConverter(&converter))
          && SUCCEEDED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
        if (ok) {
            image.pixels.resize(image.width*image.height*4);
            ok = SUCCEEDED(converter->CopyPixels(nullptr,image.width*4,static_cast<UINT>(image.pixels.size()),image.pixels.data()));
        }
    }
    factory.Reset();
    if (SUCCEEDED(com)) CoUninitialize();
    return ok;
}
void tagTexture(ID3D11Device* device, ID3D11Texture2D* original, const D3D11_TEXTURE2D_DESC& source, int cell)
{
    if (!original || cell < 0) return;
    UINT size = sizeof(ID3D11ShaderResourceView*);
    ID3D11ShaderResourceView* existing = nullptr;
    if (SUCCEEDED(original->GetPrivateData(replacementGuid,&size,&existing)) && existing) { existing->Release(); return; }
    const auto& image = images[static_cast<size_t>(cell)];
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = image.width; desc.Height = image.height; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = source.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || source.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
                ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{image.pixels.data(),image.width*4,0};
    ComPtr<ID3D11Texture2D> replacement;
    ComPtr<ID3D11ShaderResourceView> view;
    if (FAILED(createTexture(device,&desc,&initial,&replacement)) || FAILED(device->CreateShaderResourceView(replacement.Get(),nullptr,&view))
        || FAILED(original->SetPrivateDataInterface(replacementGuid,view.Get()))) {
        log(CRABE_LOG_WARNING,"Could not create a replacement glyph; retaining the original."); return;
    }
    const unsigned count = matched.fetch_add(1)+1;
    const auto text = std::to_string(count);
    api->setValue(api,"di3.ps4.matched",text.c_str());
    if (count <= 64) log(CRABE_LOG_INFO,("Matched vanilla glyph: " + std::to_string(cell)).c_str());
}
HRESULT STDMETHODCALLTYPE hookedCreateTexture(ID3D11Device* device, const D3D11_TEXTURE2D_DESC* desc,
                                             const D3D11_SUBRESOURCE_DATA* initial, ID3D11Texture2D** texture)
{
    const auto result = createTexture(device,desc,initial,texture);
    if (SUCCEEDED(result) && desc && initial && texture && *texture) {
        const int cell = findGlyph(*desc,initial[0].pSysMem,initial[0].SysMemPitch);
        if (cell >= 0) {
            try { tagTexture(device,*texture,*desc,cell); }
            catch (...) { log(CRABE_LOG_WARNING,"Replacement allocation failed; retaining original glyph."); }
        }
    }
    return result;
}
void STDMETHODCALLTYPE hookedUpdateResource(ID3D11DeviceContext* context, ID3D11Resource* resource, UINT subresource,
                                             const D3D11_BOX* box, const void* data, UINT pitch, UINT depthPitch)
{
    updateResource[context->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED ? 1 : 0](context,resource,subresource,box,data,pitch,depthPitch);
    if (!resource || subresource != 0 || box || !data) return;
    D3D11_RESOURCE_DIMENSION dimension{}; resource->GetType(&dimension);
    if (dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D) return;
    auto* texture = static_cast<ID3D11Texture2D*>(resource);
    D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
    const int cell = findGlyph(desc,data,pitch);
    if (cell < 0) return;
    try { ComPtr<ID3D11Device> device; context->GetDevice(&device); tagTexture(device.Get(),texture,desc,cell); }
    catch (...) { log(CRABE_LOG_WARNING,"Glyph upload interception failed; retaining original."); }
}
void pollSetting()
{
    const auto now = GetTickCount64();
    auto next = nextPoll.load(std::memory_order_relaxed);
    if (now < next || !nextPoll.compare_exchange_strong(next,now+50)) return;
    char value[8]{};
    api->getValue(api,"di3.ps4.enabled",value,sizeof(value));
    enabled.store(value[0]=='1',std::memory_order_relaxed);
    api->setValue(api,"di3.ps4.active",value[0]=='1'?"1":"0");
    const auto count = std::to_string(substitutions.load(std::memory_order_relaxed));
    api->setValue(api,"di3.ps4.substitutions",count.c_str());
}
void STDMETHODCALLTYPE hookedSetResources(ID3D11DeviceContext* context, UINT start, UINT count,
                                           ID3D11ShaderResourceView* const* views)
{
    pollSetting();
    if (!enabled.load(std::memory_order_relaxed) || !views || count > D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
        setResources[context->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED ? 1 : 0](context,start,count,views); return;
    }
    std::array<ID3D11ShaderResourceView*,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> selected{};
    std::array<ComPtr<ID3D11ShaderResourceView>,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> owned{};
    for (UINT i=0; i<count; ++i) {
        selected[i] = views[i];
        if (!views[i]) continue;
        ComPtr<ID3D11Resource> resource; views[i]->GetResource(&resource);
        UINT size = sizeof(ID3D11ShaderResourceView*);
        ID3D11ShaderResourceView* replacement = nullptr;
        if (resource && SUCCEEDED(resource->GetPrivateData(replacementGuid,&size,&replacement)) && replacement) {
            owned[i].Attach(replacement); selected[i] = replacement;
            substitutions.fetch_add(1,std::memory_order_relaxed);
        }
    }
    setResources[context->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED ? 1 : 0](context,start,count,selected.data());
}
bool install()
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context, deferred;
    D3D_FEATURE_LEVEL level{};
    if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context))) return false;
    if (FAILED(device->CreateDeferredContext(0,&deferred))) return false;
    auto** dv = *reinterpret_cast<void***>(device.Get());
    auto** cv = *reinterpret_cast<void***>(context.Get());
    auto** dc = *reinterpret_cast<void***>(deferred.Get());
    if (!api->installHook(api,dv[5],reinterpret_cast<void*>(&hookedCreateTexture),reinterpret_cast<void**>(&createTexture),"CreateTexture2D")) return false;
    if (!api->installHook(api,cv[48],reinterpret_cast<void*>(&hookedUpdateResource),reinterpret_cast<void**>(&updateResource[0]),"UpdateSubresource")) return false;
    if (!api->installHook(api,cv[8],reinterpret_cast<void*>(&hookedSetResources),reinterpret_cast<void**>(&setResources[0]),"PSSetShaderResources")) return false;
    // Deferred contexts record commands through different runtime entry points. Hooking only
    // immediate contexts observes overlays while the game's command lists retain Xbox views.
    if (dc[48] == cv[48]) updateResource[1] = updateResource[0];
    else if (!api->installHook(api,dc[48],reinterpret_cast<void*>(&hookedUpdateResource),reinterpret_cast<void**>(&updateResource[1]),"DeferredUpdateSubresource")) return false;
    if (dc[8] == cv[8]) setResources[1] = setResources[0];
    else if (!api->installHook(api,dc[8],reinterpret_cast<void*>(&hookedSetResources),reinterpret_cast<void**>(&setResources[1]),"DeferredPSSetShaderResources")) return false;
    return true;
}

}
BOOL APIENTRY DllMain(HMODULE handle, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { module = handle; DisableThreadLibraryCalls(handle); }
    return TRUE;
}
extern "C" __declspec(dllexport) void __cdecl CrabePluginInit(const CrabePluginApi* host)
{
    // Newer loaders append to the table; accept any version that has every v1 function this plugin calls.
    if (!host || host->version < 1 || host->size < offsetof(CrabePluginApi, onPresent)) return;
    api = host;
    api->setValue(api,"di3.ps4.ready","0");
    try {
        wchar_t path[32768]{};
        const DWORD length = GetModuleFileNameW(module,path,static_cast<DWORD>(std::size(path)));
        if (!length || length >= std::size(path) || !loadImages(std::filesystem::path(path).parent_path()) || !install()) {
            api->removeHooks(api);
            log(CRABE_LOG_ERROR,"Initialization failed (glyph PNGs or D3D11 hooks); mod remains off."); return;
        }
        api->setValue(api,"di3.ps4.ready","1");
        log(CRABE_LOG_INFO,"PS4 glyph renderer ready; live toggle enabled, originals preserved.");
    } catch (...) {
        api->removeHooks(api);
        log(CRABE_LOG_ERROR,"Initialization failed; mod remains off.");
    }
}
