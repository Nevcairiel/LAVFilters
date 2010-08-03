
class CDSStreamInfo
{
public:
  CDSStreamInfo(AVStream *avstream, const char* containerFormat = "");
  ~CDSStreamInfo();

  CMediaType mtype;

  STDMETHODIMP CreateAudioMediaType(AVStream *avstream);
  STDMETHODIMP CreateVideoMediaType(AVStream *avstream);
  STDMETHODIMP CreateSubtitleMediaType(AVStream *avstream);

private:
  char* m_containerFormat;
};
