// Class template: Re-sizable array. 

// To grow or shrink the array, call SetSize(). 
// To pre-allocate the array, call Allocate(). 

// Notes:
// Copy constructor and assignment operator are private, to avoid throwing exceptions. (One could easily modify this.)
// It is the caller's responsibility to release the objects in the array. The array's destuctor does not release them.
// The array does not actually shrink when SetSize is called with a smaller size. Only the reported size changes.

#pragma once

#include <assert.h>
#include "DShowUtil.h"

template <class T>
class GrowableArray
{
public:
  GrowableArray() : m_count(0), m_allocated(0), m_pArray(NULL)
  {

  }
  virtual ~GrowableArray()
  {
    free(m_pArray);
  }

  // Allocate: Reserves memory for the array, but does not increase the count.
  HRESULT Allocate(DWORD alloc)
  {
    HRESULT hr = S_OK;
    if (alloc > m_allocated)
    {
      m_pArray = (T *)realloc(m_pArray, sizeof(T) * alloc);
      ZeroMemory(m_pArray+m_allocated, (alloc - m_allocated) * sizeof(T));
      m_allocated = alloc;
    }
    return hr;
  }

  // SetSize: Changes the count, and grows the array if needed.
  HRESULT SetSize(DWORD count)
  {
    HRESULT hr = S_OK;
    if (count > m_allocated)
    {
      hr = Allocate(count);
    }
    if (SUCCEEDED(hr))
    {
      m_count = count;
    }
    return hr;
  }

  HRESULT Append(GrowableArray<T> *other)
  {
    return Append(other->Ptr(), other->GetCount());
  }

  HRESULT Append(const T *other, DWORD dwSize)
  {
    HRESULT hr = S_OK;
    DWORD old = GetCount();
    hr = SetSize(old + dwSize);
    if (SUCCEEDED(hr))
      memcpy(m_pArray + old, other, dwSize);

    return S_OK;
  }

  DWORD GetCount() const { return m_count; }

  // Accessor.
  T& operator[](DWORD index)
  {
    assert(index < m_count);
    return m_pArray[index];
  }

  // Const accessor.
  const T& operator[](DWORD index) const
  {
    assert(index < m_count);
    return m_pArray[index];
  }

  // Return the underlying array.
  T* Ptr() { return m_pArray; }

protected:
  GrowableArray& operator=(const GrowableArray& r);
  GrowableArray(const GrowableArray &r);

  T       *m_pArray;
  DWORD   m_count;        // Nominal count.
  DWORD   m_allocated;    // Actual allocation size.
};
