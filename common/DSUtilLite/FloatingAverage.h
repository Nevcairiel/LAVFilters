#include <assert.h>
#include "DShowUtil.h"

class FloatingAverage
{
public:
  FloatingAverage(unsigned int iNumSamples = 10) : m_NumSamples(iNumSamples), m_CurrentSample(0) {
    m_fSamples = (float *)calloc(iNumSamples, sizeof(float));
  }

  ~FloatingAverage() {
    free(m_fSamples);
  }

  void Sample(float fSample) {
    m_fSamples[m_CurrentSample] = fSample;
    if (++m_CurrentSample >= m_NumSamples) {
      m_CurrentSample = 0;
    }
  }

  float Average() const {
    float fAverage = 0;
    for(unsigned int i = 0; i < m_NumSamples; ++i) {
      fAverage += m_fSamples[i] / m_NumSamples;
    }
    return fAverage;
  }

private:
  float *m_fSamples;
  unsigned int m_NumSamples;
  unsigned int m_CurrentSample;
};
