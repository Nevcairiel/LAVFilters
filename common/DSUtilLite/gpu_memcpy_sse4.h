/*
 *      Copyright (C) 2011 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  Taken from the QuickSync decoder by Eric Gur
 */

// gpu_memcpy is a memcpy style function that copied data very fast from a
// GPU tiled memory (write back)
// Performance tip: page offset (12 lsb) of both addresses should be different
//  optimally use a 2K offset between them.
inline void* gpu_memcpy(void* d, const void* s, size_t size)
{
    if (d == NULL || s == NULL) return NULL;

    //if memory is not aligned, use memcpy
    bool isAligned = (((size_t)(s) | (size_t)(d)) & 0xF) == 0;
    if (!isAligned)
    {
        return memcpy(d, s, size);
    }

    size_t reminder = size & 127; // copy 128 bytes every loop
    size_t end = 0;

    __m128i* pTrg = (__m128i*)d;
    __m128i* pTrgEnd = pTrg + ((size - reminder) >> 4);
    __m128i* pSrc = (__m128i*)s;
    
    // make sure source is synced - doesn't hurt if not needed.
    _mm_sfence();

// split main loop to 32 and 64 bit code
// 64 doesn't have inline assembly and assembly code is faster.
// TODO: write a pure ASM function for the 64 bit verrsion.
#ifdef _M_X64 
    while (pTrg < pTrgEnd)
    {
        // Emits the Streaming SIMD Extensions 4 (SSE4.1) instruction movntdqa
        // fastest method for copying GPU RAM. Availble since Penryn (45nm Core 2 Dou/Quad)
        pTrg[0] = _mm_stream_load_si128(&pSrc[0]);
        pTrg[1] = _mm_stream_load_si128(&pSrc[1]);
        pTrg[2] = _mm_stream_load_si128(&pSrc[2]);
        pTrg[3] = _mm_stream_load_si128(&pSrc[3]);
        pTrg[4] = _mm_stream_load_si128(&pSrc[4]);
        pTrg[5] = _mm_stream_load_si128(&pSrc[5]);
        pTrg[6] = _mm_stream_load_si128(&pSrc[6]);
        pTrg[7] = _mm_stream_load_si128(&pSrc[7]);
        pSrc += 8;
        pTrg += 8;
    }
#else // end of 64 bit code
    __asm
    {
        mov ecx, pSrc;
        mov edx, pTrg;
        mov eax, pTrgEnd;

        // test ending condition
        cmp edx, eax; // if (pTrgEnd >= pTrg) goto endLoop
        jae endLoop;
startLoop:
        movntdqa  xmm0,    [ecx];
        movdqa    [edx],       xmm0;
        movntdqa  xmm1,    [ecx + 16];
        movdqa    [edx + 16],  xmm1;
        movntdqa  xmm2,       [ecx + 32];
        movdqa    [edx + 32],  xmm2;
        movntdqa  xmm3,       [ecx + 48];
        movdqa    [edx + 48],  xmm3;
        movntdqa  xmm4,       [ecx + 64];
        movdqa    [edx + 64],  xmm4;
        movntdqa  xmm5,       [ecx + 80];
        movdqa    [edx + 80],  xmm5;
        movntdqa  xmm6,       [ecx + 96];
        movdqa    [edx + 96],  xmm6;
        movntdqa  xmm7,       [ecx + 112];
        movdqa    [edx + 112], xmm7;
        add  edx, 128;
        add  ecx, 128;
        cmp edx, eax; // if (pTrgEnd > pTrg) goto startLoop
        jb  startLoop;
endLoop:
    }

    pTrg = pTrgEnd;
    pSrc += (size - reminder) >> 4;

#endif // end of 32 bit code

    // copy in 16 byte steps
    if (reminder >= 16)
    {
        size = reminder;
        reminder = size & 15;
        end = size >> 4;
        for (size_t i = 0; i < end; ++i)
        {
            pTrg[i] = _mm_stream_load_si128(&pSrc[i]);
        }
    }

    // copy last bytes
    if (reminder)
    {
        __m128i temp = _mm_stream_load_si128(pSrc + end);

        char* ps = (char*)(&temp);
        char* pt = (char*)(pTrg + end);

        for (size_t i = 0; i < reminder; ++i)
        {
            pt[i] = ps[i];
        }
    }

    return d;
}
