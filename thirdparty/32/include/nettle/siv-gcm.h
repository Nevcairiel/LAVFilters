/* siv-gcm.h

   AES-GCM-SIV, RFC8452

   Copyright (C) 2022 Red Hat, Inc.

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

#ifndef NETTLE_SIV_GCM_H_INCLUDED
#define NETTLE_SIV_GCM_H_INCLUDED

#include "nettle-types.h"
#include "nettle-meta.h"
#include "aes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Name mangling */
#define siv_gcm_encrypt_message nettle_siv_gcm_encrypt_message
#define siv_gcm_decrypt_message nettle_siv_gcm_decrypt_message
#define siv_gcm_aes128_encrypt_message nettle_siv_gcm_aes128_encrypt_message
#define siv_gcm_aes128_decrypt_message nettle_siv_gcm_aes128_decrypt_message
#define siv_gcm_aes256_encrypt_message nettle_siv_gcm_aes256_encrypt_message
#define siv_gcm_aes256_decrypt_message nettle_siv_gcm_aes256_decrypt_message

/* For AES-GCM-SIV, the block size of the underlying cipher shall be 128 bits. */
#define SIV_GCM_BLOCK_SIZE 16
#define SIV_GCM_DIGEST_SIZE 16
#define SIV_GCM_NONCE_SIZE 12

/* Generic interface.  NC must be a block cipher with 128-bit block
   size, and keysize that is a multiple of 64 bits, such as AES-128 or
   AES-256.  */
void
siv_gcm_encrypt_message (const struct nettle_cipher *nc,
			 const void *ctx,
			 void *ctr_ctx,
			 size_t nlength, const uint8_t *nonce,
			 size_t alength, const uint8_t *adata,
			 size_t clength, uint8_t *dst, const uint8_t *src);

int
siv_gcm_decrypt_message (const struct nettle_cipher *nc,
			 const void *ctx,
			 void *ctr_ctx,
			 size_t nlength, const uint8_t *nonce,
			 size_t alength, const uint8_t *adata,
			 size_t mlength, uint8_t *dst, const uint8_t *src);

/* AEAD_AES_128_GCM_SIV */
void
siv_gcm_aes128_encrypt_message (const struct aes128_ctx *ctx,
				size_t nlength, const uint8_t *nonce,
				size_t alength, const uint8_t *adata,
				size_t clength, uint8_t *dst, const uint8_t *src);

int
siv_gcm_aes128_decrypt_message (const struct aes128_ctx *ctx,
				size_t nlength, const uint8_t *nonce,
				size_t alength, const uint8_t *adata,
				size_t mlength, uint8_t *dst, const uint8_t *src);

/* AEAD_AES_256_GCM_SIV */
void
siv_gcm_aes256_encrypt_message (const struct aes256_ctx *ctx,
				size_t nlength, const uint8_t *nonce,
				size_t alength, const uint8_t *adata,
				size_t clength, uint8_t *dst, const uint8_t *src);

int
siv_gcm_aes256_decrypt_message (const struct aes256_ctx *ctx,
				size_t nlength, const uint8_t *nonce,
				size_t alength, const uint8_t *adata,
				size_t mlength, uint8_t *dst, const uint8_t *src);

#ifdef __cplusplus
}
#endif

#endif /* NETTLE_SIV_H_INCLUDED */
