#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include "crabe_plugin.h"
using Microsoft::WRL::ComPtr;
std::map<std::string,std::string> values;
std::map<std::string,void*> hooks;
void __cdecl logLine(const CrabePluginApi*,int,const char* text) { std::cout << text << '\n'; }
int __cdecl install(const CrabePluginApi*,void* target,void* detour,void** original,const char* name) { *original=target;hooks[name]=detour;return 1; }
int __cdecl remove(const CrabePluginApi*) { hooks.clear();return 5; }
int __cdecl runLua(const CrabePluginApi*,const char*) { return 1; }
int __cdecl setValue(const CrabePluginApi*,const char* key,const char* value) { values[key]=value?value:"";return 1; }
int __cdecl getValue(const CrabePluginApi*,const char* key,char* buffer,uint32_t size) { auto i=values.find(key);if(i==values.end())return -1;strncpy_s(buffer,size,i->second.c_str(),_TRUNCATE);return static_cast<int>(i->second.size()); }
void check(bool ok,const char* why) { if(!ok){std::cerr << why << '\n';std::exit(1);} }
int wmain(int argc,wchar_t** argv)
{
    check(argc==3,"Usage: test_renderer plugin.asi original-glyph.tbody");
    HMODULE plugin=LoadLibraryW(std::filesystem::absolute(argv[1]).c_str());check(plugin!=nullptr,"Load plugin failed");
    auto init=reinterpret_cast<CrabePluginInitFn>(GetProcAddress(plugin,CRABE_PLUGIN_INIT_SYMBOL));check(init!=nullptr,"Missing SDK entry point");
    CrabePluginApi host{sizeof(CrabePluginApi),CRABE_PLUGIN_API_VERSION,"1.1.0","PS4Glyphs.asi",logLine,install,remove,runLua,setValue,getValue};
    init(&host);check(values["di3.ps4.ready"]=="1","Renderer initialization failed");
    std::ifstream file(argv[2],std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
    check(bytes.size()==8228 && std::memcmp(bytes.data(),"DDS ",4)==0,"Expected a vanilla 45x45 BGRA glyph DDS");
    const auto center=128+(22*45+22)*4;
    const std::array<unsigned char,4> expectedXbox{bytes[center+2],bytes[center+1],bytes[center],bytes[center+3]};
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Create D3D11 device failed");
    const char* vsSource="float4 main(uint id:SV_VertexID):SV_Position {float2 p=float2((id<<1)&2,id&2);return float4(p*float2(2,-2)+float2(-1,1),0,1);}";
    const char* psSource="Texture2D<float4> glyph:register(t0);float4 main():SV_Target {uint w,h;glyph.GetDimensions(w,h);return glyph.Load(int3(w/2,h/2,0));}";
    ComPtr<ID3DBlob> vsCode,psCode,errors;
    check(SUCCEEDED(D3DCompile(vsSource,strlen(vsSource),nullptr,nullptr,nullptr,"main","vs_4_0",0,0,&vsCode,&errors)),"Compile vertex shader failed");
    check(SUCCEEDED(D3DCompile(psSource,strlen(psSource),nullptr,nullptr,nullptr,"main","ps_4_0",0,0,&psCode,&errors)),"Compile pixel shader failed");
    ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
    check(SUCCEEDED(device->CreateVertexShader(vsCode->GetBufferPointer(),vsCode->GetBufferSize(),nullptr,&vs)),"Create vertex shader failed");
    check(SUCCEEDED(device->CreatePixelShader(psCode->GetBufferPointer(),psCode->GetBufferSize(),nullptr,&ps)),"Create pixel shader failed");
    D3D11_TEXTURE2D_DESC outputDesc{};outputDesc.Width=outputDesc.Height=outputDesc.MipLevels=outputDesc.ArraySize=1;
    outputDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;outputDesc.SampleDesc.Count=1;outputDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> output,readback;ComPtr<ID3D11RenderTargetView> rtv;
    check(SUCCEEDED(device->CreateTexture2D(&outputDesc,nullptr,&output)),"Create output failed");
    check(SUCCEEDED(device->CreateRenderTargetView(output.Get(),nullptr,&rtv)),"Create RTV failed");
    outputDesc.BindFlags=0;outputDesc.Usage=D3D11_USAGE_STAGING;outputDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    check(SUCCEEDED(device->CreateTexture2D(&outputDesc,nullptr,&readback)),"Create readback failed");
    using Create=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const D3D11_TEXTURE2D_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Texture2D**);
    using Bind=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,ID3D11ShaderResourceView* const*);
    using Upload=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*,UINT,UINT);
    auto create=reinterpret_cast<Create>(hooks.at("CreateTexture2D"));
    D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=45;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{bytes.data()+128,180,0};
    std::array<unsigned char,4> expectedPS{};
    for(bool deferredContext:{false,true}) {
        ComPtr<ID3D11DeviceContext> recorder;
        if (deferredContext) check(SUCCEEDED(device->CreateDeferredContext(0,&recorder)),"Create deferred context failed");else recorder=context;
        auto bind=reinterpret_cast<Bind>(hooks.at(deferredContext?"DeferredPSSetShaderResources":"PSSetShaderResources"));
        auto upload=reinterpret_cast<Upload>(hooks.at(deferredContext?"DeferredUpdateSubresource":"UpdateSubresource"));
        for(bool deferredUpload:{false,true}) {
            ComPtr<ID3D11Texture2D> texture;
            check(SUCCEEDED(create(device.Get(),&desc,deferredUpload?nullptr:&initial,&texture)),"Intercepted create failed");
            if(deferredUpload) upload(recorder.Get(),texture.Get(),0,nullptr,initial.pSysMem,180,0);
            ComPtr<ID3D11ShaderResourceView> original;
            check(SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&original)),"Create original SRV failed");
            for(bool on:{true,false}) {
                values["di3.ps4.enabled"]=on?"1":"0";Sleep(120);
                ID3D11ShaderResourceView* incoming=original.Get();bind(recorder.Get(),0,1,&incoming);
                ID3D11RenderTargetView* target=rtv.Get();recorder->OMSetRenderTargets(1,&target,nullptr);
                recorder->VSSetShader(vs.Get(),nullptr,0);recorder->PSSetShader(ps.Get(),nullptr,0);
                D3D11_VIEWPORT viewport{0,0,1,1,0,1};recorder->RSSetViewports(1,&viewport);
                recorder->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);recorder->Draw(3,0);
                if(deferredContext) {
                    ComPtr<ID3D11CommandList> commands;
                    check(SUCCEEDED(recorder->FinishCommandList(FALSE,&commands)),"FinishCommandList failed");
                    context->ExecuteCommandList(commands.Get(),FALSE);
                }
                context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
                check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)),"GPU readback failed");
                std::array<unsigned char,4> pixel{};memcpy(pixel.data(),mapped.pData,4);context->Unmap(readback.Get(),0);
                if(on) {
                    check(pixel!=expectedXbox,"Enabled GPU draw still sampled the Xbox image");
                    if(!deferredContext && !deferredUpload) expectedPS=pixel;
                    check(pixel==expectedPS,"Deferred rendering differs from immediate replacement pixels");
                } else check(pixel==expectedXbox,"Disabled GPU draw did not restore Xbox pixels");
            }
            std::cout << (deferredContext?"Deferred command list":"Immediate context") << ", " << (deferredUpload?"later upload":"initial data") << ": GPU pixels passed for on/off.\n";
        }
    }
    std::cout << "Real GPU renderer passed for both context types and both upload paths.\n";
    return 0;
}
