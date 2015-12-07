// -----------------------------------------------------------------
// IMediaSideData interface and data structure definitions
// -----------------------------------------------------------------

#pragma once

// -----------------------------------------------------------------
// Interface to exchange binary side data
// -----------------------------------------------------------------
// This interface should be implemented in IMediaSample objects and accessed through IUnknown
// It allows binary side data to be attached to the media samples and delivered with them
// Restrictions: Only one side data per type can be attached
[uuid("F940AE7F-48EB-4377-806C-8FC48CAB2292")]
interface IMediaSideData : public IUnknown
{
  // Set the side data identified by guidType to the data provided
  // The provided data will be copied and stored internally
  STDMETHOD(SetSideData)(GUID guidType, const BYTE *pData, size_t size) PURE;

  // Get the side data identified by guidType
  // The caller receives pointers to the internal data, and the pointers shall stay
  // valid for the lifetime of the object
  STDMETHOD(GetSideData)(GUID guidType, const BYTE **pData, size_t *pSize) PURE;
};


// -----------------------------------------------------------------
// High-Dynamic-Range (HDR) Side Data
// -----------------------------------------------------------------

// {53820DBC-A7B8-49C4-B17B-E511591A790C}
DEFINE_GUID(IID_MediaSideDataHDR,
0x53820dbc, 0xa7b8, 0x49c4, 0xb1, 0x7b, 0xe5, 0x11, 0x59, 0x1a, 0x79, 0xc);

#pragma pack(push, 1)
struct MediaSideDataHDR
{
  // coordinates of the primaries, in B-G-R order
  double display_primaries_x[3];
  double display_primaries_y[3];
  // white point
  double white_point_x;
  double white_point_y;
  // luminance
  double max_display_mastering_luminance;
  double min_display_mastering_luminance;
};
#pragma pack(pop)
