#pragma once
#include <d3d11.h>
#include <cstdint>
#include <cstddef>
namespace NativeDepthCapture {
constexpr std::size_t function=0x390, getSrv=0x398, setSrv=0x3A0, dimensions=0x3A8, data=0xA40;
struct Cache {
 ID3D11DepthStencilView* view;
 ID3D11Texture2D* source;
 ID3D11Texture2D* copy;
 ID3D11ShaderResourceView* srv;
 std::uint32_t frame,bits,copies,failure;
};
static_assert(offsetof(Cache,srv)==24);
static_assert(data+sizeof(Cache)<0x1000);
}
extern "C" unsigned NativePrepareDepth(void*,ID3D11DepthStencilView*,unsigned);
