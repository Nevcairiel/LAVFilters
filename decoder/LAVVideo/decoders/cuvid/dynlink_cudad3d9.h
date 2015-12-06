/*
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

#ifndef CUDAD3D9_H
#define CUDAD3D9_H

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    #define WINDOWS_LEAN_AND_MEAN
    #include <windows.h>
    #include <windowsx.h>
#endif
// #include <d3dx9.h>

#if __CUDA_API_VERSION >= 3020
//    #define cuD3D9CtxCreate                    cuD3D9CtxCreate_v2
//    #define cuD3D9ResourceGetSurfaceDimensions cuD3D9ResourceGetSurfaceDimensions_v2
//    #define cuD3D9ResourceGetMappedPointer     cuD3D9ResourceGetMappedPointer_v2
//    #define cuD3D9ResourceGetMappedSize        cuD3D9ResourceGetMappedSize_v2
//    #define cuD3D9ResourceGetMappedPitch       cuD3D9ResourceGetMappedPitch_v2
//    #define cuD3D9MapVertexBuffer              cuD3D9MapVertexBuffer_v2
#endif /* __CUDA_API_VERSION >= 3020 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file dynlink_cudaD3D9.h
 * \brief Header file for the Direct3D 9 interoperability functions of the
 * low-level CUDA driver application programming interface.
 */

/**
 * \defgroup CUDA_D3D9 Direct3D 9 Interoperability
 * \ingroup CUDA_DRIVER
 *
 * ___MANBRIEF___ Direct3D 9 interoperability functions of the low-level CUDA
 * driver API (___CURRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the Direct3D 9 interoperability functions of the
 * low-level CUDA driver application programming interface. Note that mapping 
 * of Direct3D 9 resources is performed with the graphics API agnostic, resource 
 * mapping interface described in \ref CUDA_GRAPHICS "Graphics Interoperability".
 *
 * @{
 */

/**
 * CUDA devices corresponding to a D3D9 device
 */
typedef enum CUd3d9DeviceList_enum {
    CU_D3D9_DEVICE_LIST_ALL            = 0x01, /**< The CUDA devices for all GPUs used by a D3D9 device */
    CU_D3D9_DEVICE_LIST_CURRENT_FRAME  = 0x02, /**< The CUDA devices for the GPUs used by a D3D9 device in its currently rendering frame */
    CU_D3D9_DEVICE_LIST_NEXT_FRAME     = 0x03, /**< The CUDA devices for the GPUs to be used by a D3D9 device in the next frame */
} CUd3d9DeviceList;

typedef CUresult CUDAAPI tcuD3D9GetDevice(CUdevice *pCudaDevice, const char *pszAdapterName);
typedef CUresult CUDAAPI tcuD3D9GetDevices(unsigned int *pCudaDeviceCount, CUdevice *pCudaDevices, unsigned int cudaDeviceCount, IDirect3DDevice9 *pD3D9Device, CUd3d9DeviceList deviceList);

#if __CUDA_API_VERSION >= 3020
typedef CUresult CUDAAPI tcuD3D9CtxCreate(CUcontext *pCtx, CUdevice *pCudaDevice, unsigned int Flags, IDirect3DDevice9 *pD3DDevice);
#endif

#if __CUDA_API_VERSION >= 3020
typedef CUresult CUDAAPI tcuD3D9CtxCreate_v2(CUcontext *pCtx, CUdevice *pCudaDevice, unsigned int Flags, IDirect3DDevice9 *pD3DDevice);
typedef CUresult CUDAAPI tcuD3D9CtxCreateOnDevice(CUcontext *pCtx, unsigned int flags, IDirect3DDevice9 *pD3DDevice, CUdevice cudaDevice);
#endif /* __CUDA_API_VERSION >= 3020 */

typedef CUresult CUDAAPI tcuD3D9GetDirect3DDevice(IDirect3DDevice9 **ppD3DDevice);
typedef CUresult CUDAAPI tcuGraphicsD3D9RegisterResource(CUgraphicsResource *pCudaResource, IDirect3DResource9 *pD3DResource, unsigned int Flags);

/**
 * \defgroup CUDA_D3D9_DEPRECATED Direct3D 9 Interoperability [DEPRECATED]
 *
 * ___MANBRIEF___ deprecated Direct3D 9 interoperability functions of the
 * low-level CUDA driver API (___CURRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes deprecated Direct3D 9 interoperability functionality.
 * @{
 */

/** Flags to register a resource */
typedef enum CUd3d9register_flags_enum {
    CU_D3D9_REGISTER_FLAGS_NONE  = 0x00,
    CU_D3D9_REGISTER_FLAGS_ARRAY = 0x01,
} CUd3d9register_flags;

/** Flags to map or unmap a resource */
typedef enum CUd3d9map_flags_enum {
    CU_D3D9_MAPRESOURCE_FLAGS_NONE         = 0x00,
    CU_D3D9_MAPRESOURCE_FLAGS_READONLY     = 0x01,
    CU_D3D9_MAPRESOURCE_FLAGS_WRITEDISCARD = 0x02,
} CUd3d9map_flags;

typedef CUresult CUDAAPI tcuD3D9RegisterResource(IDirect3DResource9 *pResource, unsigned int Flags);
typedef CUresult CUDAAPI tcuD3D9UnregisterResource(IDirect3DResource9 *pResource);
typedef CUresult CUDAAPI tcuD3D9MapResources(unsigned int count, IDirect3DResource9 **ppResource);
typedef CUresult CUDAAPI tcuD3D9UnmapResources(unsigned int count, IDirect3DResource9 **ppResource);
typedef CUresult CUDAAPI tcuD3D9ResourceSetMapFlags(IDirect3DResource9 *pResource, unsigned int Flags);

#if __CUDA_API_VERSION >= 3020
typedef CUresult CUDAAPI tcuD3D9ResourceGetSurfaceDimensions(size_t *pWidth, size_t *pHeight, size_t *pDepth, IDirect3DResource9 *pResource, unsigned int Face, unsigned int Level);
#endif /* __CUDA_API_VERSION >= 3020 */

typedef CUresult CUDAAPI tcuD3D9ResourceGetMappedArray(CUarray *pArray, IDirect3DResource9 *pResource, unsigned int Face, unsigned int Level);

#if __CUDA_API_VERSION >= 3020
typedef CUresult CUDAAPI tcuD3D9ResourceGetMappedPointer(CUdeviceptr *pDevPtr, IDirect3DResource9 *pResource, unsigned int Face, unsigned int Level);
typedef CUresult CUDAAPI tcuD3D9ResourceGetMappedSize(size_t *pSize, IDirect3DResource9 *pResource, unsigned int Face, unsigned int Level);
typedef CUresult CUDAAPI tcuD3D9ResourceGetMappedPitch(size_t *pPitch, size_t *pPitchSlice, IDirect3DResource9 *pResource, unsigned int Face, unsigned int Level);
#endif /* __CUDA_API_VERSION >= 3020 */

/* CUDA 1.x compatibility API. These functions are deprecated, please use the ones above. */
typedef CUresult CUDAAPI tcuD3D9Begin(IDirect3DDevice9 *pDevice);
typedef CUresult CUDAAPI tcuD3D9End(void);
typedef CUresult CUDAAPI tcuD3D9RegisterVertexBuffer(IDirect3DVertexBuffer9 *pVB);
#if __CUDA_API_VERSION >= 3020
typedef CUresult CUDAAPI tcuD3D9MapVertexBuffer(CUdeviceptr *pDevPtr, size_t *pSize, IDirect3DVertexBuffer9 *pVB);
#endif /* __CUDA_API_VERSION >= 3020 */
typedef CUresult CUDAAPI tcuD3D9UnmapVertexBuffer(IDirect3DVertexBuffer9 *pVB);
typedef CUresult CUDAAPI tcuD3D9UnregisterVertexBuffer(IDirect3DVertexBuffer9 *pVB);

/** @} */ /* END CUDA_D3D9_DEPRECATED */
/** @} */ /* END CUDA_D3D9 */

/**
 * CUDA API versioning support
 */
#if defined(__CUDA_API_VERSION_INTERNAL)
    #undef tcuD3D9CtxCreate
    #undef tcuD3D9ResourceGetSurfaceDimensions
    #undef tcuD3D9ResourceGetMappedPointer
    #undef tcuD3D9ResourceGetMappedSize
    #undef tcuD3D9ResourceGetMappedPitch
    #undef tcuD3D9MapVertexBuffer
#endif /* __CUDA_API_VERSION_INTERNAL */


#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#include <Windows.h>
typedef HMODULE CUDADRIVER;
#else
typedef void *CUDADRIVER;
#endif
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#include <Windows.h>
typedef HMODULE CUDADRIVER;
#endif

extern tcuD3D9Begin                        *cuD3D9Begin;
extern tcuD3D9End                          *cuD3D9End;
extern tcuD3D9RegisterVertexBuffer         *cuD3D9RegisterVertexBuffer;
extern tcuD3D9MapVertexBuffer              *cuD3D9MapVertexBuffer;
extern tcuD3D9UnmapVertexBuffer            *cuD3D9UnmapVertexBuffer;
extern tcuD3D9UnregisterVertexBuffer       *cuD3D9UnregisterVertexBuffer;

extern tcuD3D9GetDevice                    *cuD3D9GetDevice;
extern tcuD3D9GetDevice                    *cuD3D9GetDevices;
extern tcuD3D9GetDevice                    *cuD3D9GetDevices_v2;
extern tcuD3D9CtxCreate                    *cuD3D9CtxCreate;
extern tcuD3D9CtxCreate                    *cuD3D9CtxCreate_v2;
extern tcuD3D9CtxCreateOnDevice            *cuD3D9CtxCreateOnDevice;
extern tcuD3D9ResourceGetSurfaceDimensions *cuD3D9ResourceGetSurfaceDimensions;
extern tcuD3D9ResourceGetMappedPointer     *cuD3D9ResourceGetMappedPointer;
extern tcuD3D9ResourceGetMappedArray       *cuD3D9ResourceGetMappedArray;
extern tcuD3D9ResourceGetMappedSize        *cuD3D9ResourceGetMappedSize;
extern tcuD3D9ResourceGetMappedPitch       *cuD3D9ResourceGetMappedPitch;
extern tcuD3D9MapVertexBuffer              *cuD3D9MapVertexBuffer;

extern tcuD3D9RegisterResource             *cuD3D9RegisterResource;
extern tcuD3D9UnregisterResource           *cuD3D9UnregisterResource;
extern tcuD3D9MapResources                 *cuD3D9MapResources;
extern tcuD3D9UnmapResources               *cuD3D9UnmapResources;
extern tcuD3D9ResourceSetMapFlags          *cuD3D9ResourceSetMapFlags;

extern tcuGraphicsD3D9RegisterResource     *cuGraphicsD3D9RegisterResource;
extern tcuGraphicsD3D9RegisterResource     *cuGraphicsD3D9RegisterResource_v2;


/************************************/
CUresult CUDAAPI cuInitD3D9(unsigned int Flags, int cudaVersion, CUDADRIVER &CudaDrvLib);
/************************************/

#ifdef __cplusplus
};
#endif

#endif
