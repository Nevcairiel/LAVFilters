#ifndef __FF_SPLITTER_H__
#define __FF_SPLITTER_H__

#include <string>

#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg

[uuid("B98D13E7-55DB-4385-A33D-09FD1BA26338")]
class CFFSplitter : public CBaseFilter, public CCritSec, protected CAMThread, public IFileSourceFilter
{
public:
  // constructor method used by class factory
  static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);

  // IUnknown
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // CBaseFilter methods
  int GetPinCount();
  CBasePin *GetPin(int n);

  STDMETHODIMP Stop();
  STDMETHODIMP Pause();
  STDMETHODIMP Run(REFERENCE_TIME tStart);

  // IFileSourceFilter
  STDMETHODIMP Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE * pmt);
  STDMETHODIMP GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt);
protected:
  // CAMThread
  enum {CMD_EXIT, CMD_SEEK};
  DWORD ThreadProc();

  void DeliverBeginFlush();
  void DeliverEndFlush();

private:
  // construct only via class factory
  CFFSplitter(LPUNKNOWN pUnk, HRESULT* phr);
  ~CFFSplitter();

  STDMETHODIMP CreateOutputs();
  STDMETHODIMP DeleteOutputs();

private:
  WCHAR *m_fileName;

  AVFormatContext *m_avFormat;

  // flushing
  bool m_fFlushing;
  CAMEvent m_eEndFlush;
};

#endif
