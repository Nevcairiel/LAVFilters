// -----------------------------------------------------------------
// ID3DVideoMemoryConfiguration interface and data structure definitions
// -----------------------------------------------------------------
#pragma once

// -----------------------------------------------------------------
// Control D3D11 Hardware Decoding between decoder and renderer
// -----------------------------------------------------------------
// A video renderer can implement this interface on its input pin
// to signal to a decoder that its capable of accepting D3D11 texture
// samples directly, without copying to system memory.
//
// The decoder will create the D3D11 device and a mutex to protect it,
// and share it with the renderer in this interface.
//
// To facilitate dynamic switching of the adapter used for decoding, the
// renderer should disconnect the decoder and re-connect it. At that
// point the decoder should query GetD3D11AdapterIndex() again and
// create a new decoder on the new device, as appropriate.
interface __declspec(uuid("2BB66002-46B7-4F13-9036-7053328742BE")) ID3D11DecoderConfiguration : public IUnknown
{
  // Set the surface format the decoder is going to send.
  // If the renderer is not ready to accept this format, an error will be returned.
  virtual HRESULT STDMETHODCALLTYPE ActivateD3D11Decoding(ID3D11Device *pDevice, ID3D11DeviceContext *pContext, HANDLE hMutex, UINT nFlags) = 0;

  // Get the currently preferred D3D11 adapter index (to be used with IDXGIFactory1::EnumAdapters1)
  virtual UINT STDMETHODCALLTYPE GetD3D11AdapterIndex() = 0;
};

// -----------------------------------------------------------------
// Media Sample to hold a D3D11 texture
// -----------------------------------------------------------------
// D3D11 textures used for decoding are typically array-textures,
// a single ID3D11Texture2D object containing an array of textures
// individually addressable by the ArraySlice index.
//
// The texture lifetime is bound to the media samples lifetime. The
// media sample can only be released when the texture is no longer in
// use, otherwise the texture will be re-used by the decoder.
//
// The texture is AddRef'ed when retrieved through this interface,
// and should be Released when no longer needed.
interface __declspec(uuid("BC8753F5-0AC8-4806-8E5F-A12B2AFE153E")) IMediaSampleD3D11 : public IUnknown
{
  // Get the D3D11 texture for the specified view.
  // 2D images with only one view always use view 0. For 3D, view 0 specifies the base view, view 1 the extension view.
  virtual HRESULT STDMETHODCALLTYPE GetD3D11Texture(int nView, ID3D11Texture2D **ppTexture, UINT *pArraySlice) = 0;
};
