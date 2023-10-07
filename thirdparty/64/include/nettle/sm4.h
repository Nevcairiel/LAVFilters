/* sm4.h

   Copyright (C) 2022 Tianjia Zhang <tianjia.zhang@linux.alibaba.com>

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#ifndef NETTLE_SM4_H_INCLUDED
#define NETTLE_SM4_H_INCLUDED

#include "nettle-types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Name mangling */
#define sm4_set_encrypt_key nettle_sm4_set_encrypt_key
#define sm4_set_decrypt_key nettle_sm4_set_decrypt_key
#define sm4_crypt nettle_sm4_crypt

#define SM4_BLOCK_SIZE 16
#define SM4_KEY_SIZE 16

struct sm4_ctx
{
  uint32_t rkey[32];
};

void
sm4_set_encrypt_key(struct sm4_ctx *ctx, const uint8_t *key);

void
sm4_set_decrypt_key(struct sm4_ctx *ctx, const uint8_t *key);

void
sm4_crypt(const struct sm4_ctx *context,
	  size_t length, uint8_t *dst,
	  const uint8_t *src);

#ifdef __cplusplus
}
#endif

#endif /* NETTLE_SM4_H_INCLUDED */
