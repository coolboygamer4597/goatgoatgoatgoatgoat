#include "NativeDepthCapture.h"
extern "C" __declspec(noinline) unsigned NativePrepareDepth(void* page,ID3D11DepthStencilView* view,unsigned capture) {
 auto* bytes=static_cast<unsigned char*>(page);
 auto* cache=reinterpret_cast<NativeDepthCapture::Cache*>(bytes+NativeDepthCapture::data);
 if(!view) {
  if(cache->srv)cache->srv->Release();if(cache->copy)cache->copy->Release();
  if(cache->source)cache->source->Release();if(cache->view)cache->view->Release();
  cache->view=nullptr;cache->source=nullptr;cache->copy=nullptr;cache->srv=nullptr;cache->bits=0;return 0;
 }
 auto* ctx=*reinterpret_cast<ID3D11DeviceContext**>(bytes+0x18);
 auto* device=*reinterpret_cast<ID3D11Device**>(bytes+0x10);
 if(!ctx || !device || !*reinterpret_cast<void**>(bytes+NativeDepthCapture::getSrv) || !*reinterpret_cast<void**>(bytes+NativeDepthCapture::setSrv))return 0;
 unsigned n=1;D3D11_VIEWPORT viewport;
 ctx->RSGetViewports(&n,&viewport);
 auto* dimensions=reinterpret_cast<float*>(bytes+NativeDepthCapture::dimensions);
 if(n!=1 || viewport.Width!=dimensions[0] || viewport.Height!=dimensions[1] || viewport.TopLeftX!=0 || viewport.TopLeftY!=0 || viewport.MinDepth!=0 || *reinterpret_cast<unsigned*>(&viewport.MaxDepth)!=0x3f800000u)return 0;
 if(cache->view!=view) {
  if(cache->srv)cache->srv->Release();if(cache->copy)cache->copy->Release();
  if(cache->source)cache->source->Release();if(cache->view)cache->view->Release();
  cache->view=view;view->AddRef();cache->source=nullptr;cache->copy=nullptr;cache->srv=nullptr;cache->bits=0;cache->failure=0;cache->frame=0xffffffffu;
  D3D11_DEPTH_STENCIL_VIEW_DESC vd;view->GetDesc(&vd);
  if(vd.ViewDimension!=D3D11_DSV_DIMENSION_TEXTURE2D){cache->failure=1;return 0;}
  DXGI_FORMAT typed,typeless;unsigned bits;
  if(vd.Format==DXGI_FORMAT_D32_FLOAT){typed=DXGI_FORMAT_R32_FLOAT;typeless=DXGI_FORMAT_R32_TYPELESS;bits=32;}
  else if(vd.Format==DXGI_FORMAT_D24_UNORM_S8_UINT){typed=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;typeless=DXGI_FORMAT_R24G8_TYPELESS;bits=24;}
  else {cache->failure=2;return 0;}
  ID3D11Resource* resource;view->GetResource(&resource);
  if(!resource){cache->failure=3;return 0;}
  cache->source=static_cast<ID3D11Texture2D*>(resource);
  D3D11_TEXTURE2D_DESC desc;cache->source->GetDesc(&desc);
  if(desc.SampleDesc.Count!=1 || desc.MipLevels!=1 || desc.ArraySize!=1 || desc.Width!=unsigned(viewport.Width) || desc.Height!=unsigned(viewport.Height)){cache->failure=4;return 0;}

  if(bits==32 && desc.Format!=DXGI_FORMAT_R32_TYPELESS && desc.Format!=DXGI_FORMAT_D32_FLOAT){cache->failure=5;return 0;}
  if(bits==24 && desc.Format!=DXGI_FORMAT_R24G8_TYPELESS && desc.Format!=DXGI_FORMAT_D24_UNORM_S8_UINT){cache->failure=5;return 0;}
  desc.Format=typeless;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.CPUAccessFlags=0;desc.MiscFlags=0;
  if(FAILED(device->CreateTexture2D(&desc,nullptr,&cache->copy))){cache->failure=6;return 0;}
  D3D11_SHADER_RESOURCE_VIEW_DESC sd;sd.Format=typed;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MostDetailedMip=0;sd.Texture2D.MipLevels=1;
  if(FAILED(device->CreateShaderResourceView(cache->copy,&sd,&cache->srv))){cache->failure=7;return 0;}
  cache->bits=bits;
 }
 if(!cache->bits || !cache->srv || !cache->copy)return 0;
 if(!capture)return cache->bits;
 const auto frame=*reinterpret_cast<std::uint32_t*>(bytes+0xA8);
 if(cache->frame!=frame){ctx->CopyResource(cache->copy,cache->source);cache->frame=frame;++cache->copies;}
 return cache->bits|0x100;
}
