#include <assert.h>
#include "DShowUtil.h"

template <class T>
class FloatingAverage
{
public:
  FloatingAverage(unsigned int iNumSamples = 10) : m_NumSamples(0), m_CurrentSample(0), m_Samples(NULL), m_NumSamplesAlloc(0) {
    SetNumSamples(iNumSamples);
  }

  ~FloatingAverage() {
    free(m_Samples);
  }

  void SetNumSamples(unsigned int iNumSamples) {
    if (iNumSamples > m_NumSamplesAlloc) {
      m_Samples = (T *)realloc(m_Samples, iNumSamples * sizeof(T));
      m_NumSamplesAlloc = iNumSamples;
    }
    if (iNumSamples > m_NumSamples)
      memset(m_Samples + m_NumSamples, 0, sizeof(T) * (iNumSamples - m_NumSamples));
    m_NumSamples = iNumSamples;
  }

  void Sample(T fSample) {
    m_Samples[m_CurrentSample] = fSample;
    if (++m_CurrentSample >= m_NumSamples) {
      m_CurrentSample = 0;
    }
  }

  T Average() const {
    T fAverage = 0;
    for(unsigned int i = 0; i < m_NumSamples; ++i) {
      fAverage += m_Samples[i] / m_NumSamples;
    }
    return fAverage;
  }

  T Minimum() const {
    T min = m_Samples[0];
    for(unsigned int i = 1; i < m_NumSamples; ++i) {
      if (m_Samples[i] < min)
        min = m_Samples[i];
    }
    return min;
  }

  T AbsMinimum() const {
    T min = m_Samples[0];
    for(unsigned int i = 1; i < m_NumSamples; ++i) {
      if (abs(m_Samples[i]) < abs(min))
        min = m_Samples[i];
    }
    return min;
  }

  T Maximum() const {
    T max = m_Samples[0];
    for(unsigned int i = 1; i < m_NumSamples; ++i) {
      if (m_Samples[i] > max)
        max = m_Samples[i];
    }
    return max;
  }

  T AbsMaximum() const {
    T max = m_Samples[0];
    for(unsigned int i = 1; i < m_NumSamples; ++i) {
      if (abs(m_Samples[i]) > abs(max))
        max = m_Samples[i];
    }
    return max;
  }

  void OffsetValues(T value) const {
    for(unsigned int i = 0; i < m_NumSamples; ++i) {
      m_Samples[i] += value;
    }
  }

  unsigned int CurrentSample() const {
    return m_CurrentSample;
  }

private:
  T *m_Samples;
  unsigned int m_NumSamples;
  unsigned int m_NumSamplesAlloc;
  unsigned int m_CurrentSample;
};
