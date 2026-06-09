/* drbg-ctr.h

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

#ifndef NETTLE_DRBG_CTR_H_INCLUDED
#define NETTLE_DRBG_CTR_H_INCLUDED

#include "nettle-types.h"

#include "aes.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Namespace mangling */
#define drbg_ctr_aes256_init nettle_drbg_ctr_aes256_init
#define drbg_ctr_aes256_random nettle_drbg_ctr_aes256_random

#define DRBG_CTR_AES256_SEED_SIZE (AES_BLOCK_SIZE + AES256_KEY_SIZE)

struct drbg_ctr_aes256_ctx
{
  struct aes256_ctx key;
  union nettle_block16 V;
};

/* Initialize using DRBG_CTR_AES256_SEED_SIZE bytes of
   SEED_MATERIAL.  */
void
drbg_ctr_aes256_init (struct drbg_ctr_aes256_ctx *ctx,
		      uint8_t *seed_material);

/* Output N bytes of random data into DST.  */
void
drbg_ctr_aes256_random (struct drbg_ctr_aes256_ctx *ctx,
			size_t n, uint8_t *dst);

#ifdef __cplusplus
}
#endif

#endif /* NETTLE_DRBG_CTR_H_INCLUDED */
