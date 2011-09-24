/*
 *      Copyright (C) 2011 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 *  Assembled from parts of the NVIDIA CUDA SDK, Copyright by NVIDIA, All rights reserved.
 */

#pragma once

////////////////////////////////////////////////////
/// CUDA functions
////////////////////////////////////////////////////
typedef CUresult CUDAAPI tcuInit(unsigned int Flags);
typedef CUresult CUDAAPI tcuCtxCreate(CUcontext *pctx, unsigned int flags, CUdevice dev );
typedef CUresult CUDAAPI tcuCtxDestroy( CUcontext ctx );
typedef CUresult CUDAAPI tcuCtxPushCurrent( CUcontext ctx );
typedef CUresult CUDAAPI tcuCtxPopCurrent( CUcontext *pctx );
typedef CUresult CUDAAPI tcuMemAllocHost(void **pp, unsigned int bytesize);
typedef CUresult CUDAAPI tcuMemFreeHost(void *p);
typedef CUresult CUDAAPI tcuMemcpyDtoH (void *dstHost, CUdeviceptr srcDevice, unsigned int ByteCount );
typedef CUresult CUDAAPI tcuMemcpyDtoHAsync(void *dstHost, CUdeviceptr srcDevice, unsigned int ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuStreamCreate(CUstream *phStream, unsigned int Flags);
typedef CUresult CUDAAPI tcuStreamDestroy(CUstream hStream);
typedef CUresult CUDAAPI tcuStreamQuery(CUstream hStream);

////////////////////////////////////////////////////
/// D3D Interop
////////////////////////////////////////////////////
typedef CUresult CUDAAPI tcuD3D9CtxCreate( CUcontext *pCtx, CUdevice *pCudaDevice, unsigned int Flags, IDirect3DDevice9 *pD3DDevice );

////////////////////////////////////////////////////
/// CUVID functions
////////////////////////////////////////////////////
typedef CUresult CUDAAPI tcuvidCtxLockCreate(CUvideoctxlock *pLock, CUcontext ctx);
typedef CUresult CUDAAPI tcuvidCtxLockDestroy(CUvideoctxlock lck);
typedef CUresult CUDAAPI tcuvidCtxLock(CUvideoctxlock lck, unsigned int reserved_flags);
typedef CUresult CUDAAPI tcuvidCtxUnlock(CUvideoctxlock lck, unsigned int reserved_flags);

typedef CUresult CUDAAPI tcuvidCreateVideoParser(CUvideoparser *pObj, CUVIDPARSERPARAMS *pParams);
typedef CUresult CUDAAPI tcuvidParseVideoData(CUvideoparser obj, CUVIDSOURCEDATAPACKET *pPacket);
typedef CUresult CUDAAPI tcuvidDestroyVideoParser(CUvideoparser obj);

// Create/Destroy the decoder object
typedef CUresult CUDAAPI tcuvidCreateDecoder(CUvideodecoder *phDecoder, CUVIDDECODECREATEINFO *pdci);
typedef CUresult CUDAAPI tcuvidDestroyDecoder(CUvideodecoder hDecoder);

// Decode a single picture (field or frame)
typedef CUresult CUDAAPI tcuvidDecodePicture(CUvideodecoder hDecoder, CUVIDPICPARAMS *pPicParams);

// Post-process and map a video frame for use in cuda
typedef CUresult CUDAAPI tcuvidMapVideoFrame(CUvideodecoder hDecoder, int nPicIdx, unsigned int *pDevPtr, unsigned int *pPitch, CUVIDPROCPARAMS *pVPP);
// Unmap a previously mapped video frame
typedef CUresult CUDAAPI tcuvidUnmapVideoFrame(CUvideodecoder hDecoder, unsigned int DevPtr);
