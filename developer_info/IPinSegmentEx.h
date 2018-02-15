#pragma once

// -----------------------------------------------------------------
// IPin Segmenting Extension
// -----------------------------------------------------------------
// This extension allows upstream filters to cleanly terminate segments,
// indicating to downstream components that any data should be flushed
// to the renderer.
// This call should be followed by a call to IPin::NewSegment to start a
// new segment afterwards.
interface __declspec(uuid("8B81E022-52C7-4B89-9F11-ACFD063AABB4")) IPinSegmentEx : public IUnknown
{
  virtual HRESULT STDMETHODCALLTYPE EndOfSegment(void) = 0;
};
