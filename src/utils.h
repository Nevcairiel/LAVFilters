#ifndef __UTILS_H__FFSPLITTER__
#define __UTILS_H__FFSPLITTER__

#define SAFE_DELETE(pPtr) { delete pPtr; pPtr = NULL; }

#define CHECK_HR(hr) if (FAILED(hr)) { goto done; }

#define QI(i) (riid == __uuidof(i)) ? GetInterface((i*)this, ppv) :
#define QI2(i) (riid == IID_##i) ? GetInterface((i*)this, ppv) :

#define countof( array ) ( sizeof( array )/sizeof( array[0] ) )

#endif