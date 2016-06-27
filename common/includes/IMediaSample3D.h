// -----------------------------------------------------------------
// IMediaSample3D interface and data structure definitions
// -----------------------------------------------------------------

#pragma once

interface __declspec(uuid("E92D790E-BF54-43C4-B394-8CA0A41BF9EC")) IMediaSample3D : public IMediaSample2
{
  STDMETHOD(Enable3D)() = 0;
  STDMETHOD(GetPointer3D)(BYTE **ppBuffer) = 0;
};
