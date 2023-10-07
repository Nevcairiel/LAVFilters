/* ocb.h

   OCB AEAD mode, RFC 7253

   Copyright (C) 2021 Niels Möller

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

#ifndef NETTLE_OCB_H_INCLUDED
#define NETTLE_OCB_H_INCLUDED

#include "nettle-types.h"
#include "aes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Name mangling */
#define ocb_set_key nettle_ocb_set_key
#define ocb_set_nonce nettle_ocb_set_nonce
#define ocb_update nettle_ocb_update
#define ocb_encrypt nettle_ocb_encrypt
#define ocb_decrypt nettle_ocb_decrypt
#define ocb_digest nettle_ocb_digest
#define ocb_encrypt_message nettle_ocb_encrypt_message
#define ocb_decrypt_message nettle_ocb_decrypt_message
#define ocb_aes128_set_encrypt_key nettle_ocb_aes128_set_encrypt_key
#define ocb_aes128_set_decrypt_key nettle_ocb_aes128_set_decrypt_key
#define ocb_aes128_set_nonce nettle_ocb_aes128_set_nonce
#define ocb_aes128_update nettle_ocb_aes128_update
#define ocb_aes128_encrypt nettle_ocb_aes128_encrypt
#define ocb_aes128_decrypt nettle_ocb_aes128_decrypt
#define ocb_aes128_digest nettle_ocb_aes128_digest
#define ocb_aes128_encrypt_message nettle_ocb_aes128_encrypt_message
#define ocb_aes128_decrypt_message nettle_ocb_aes128_decrypt_message

#define OCB_BLOCK_SIZE 16
#define OCB_DIGEST_SIZE 16
#define OCB_MAX_NONCE_SIZE 15

struct ocb_key {
  /* L_*, L_$ and L_0, and one reserved entry */
  union nettle_block16 L[4];
};

struct ocb_ctx {
  /* Initial offset, Offset_0 in the spec. */
  union nettle_block16 initial;
  /* Offset, updated per block. */
  union nettle_block16 offset;
  /* Authentication for the associated data */
  union nettle_block16 sum;
  /* Authentication for the message */
  union nettle_block16 checksum;
  /* Count of processed blocks. */
  size_t data_count;
  size_t message_count;
};

void
ocb_set_key (struct ocb_key *key, const void *cipher, nettle_cipher_func *f);

void
ocb_set_nonce (struct ocb_ctx *ctx,
	       const void *cipher, nettle_cipher_func *f,
	       size_t tag_length, size_t nonce_length, const uint8_t *nonce);

void
ocb_update (struct ocb_ctx *ctx, const struct ocb_key *key,
	    const void *cipher, nettle_cipher_func *f,
	    size_t length, const uint8_t *data);

void
ocb_encrypt (struct ocb_ctx *ctx, const struct ocb_key *key,
	     const void *cipher, nettle_cipher_func *f,
	     size_t length, uint8_t *dst, const uint8_t *src);

void
ocb_decrypt (struct ocb_ctx *ctx, const struct ocb_key *key,
	     const void *encrypt_ctx, nettle_cipher_func *encrypt,
	     const void *decrypt_ctx, nettle_cipher_func *decrypt,
	     size_t length, uint8_t *dst, const uint8_t *src);

void
ocb_digest (const struct ocb_ctx *ctx, const struct ocb_key *key,
	    const void *cipher, nettle_cipher_func *f,
	    size_t length, uint8_t *digest);


void
ocb_encrypt_message (const struct ocb_key *ocb_key,
		     const void *cipher, nettle_cipher_func *f,
		     size_t nlength, const uint8_t *nonce,
		     size_t alength, const uint8_t *adata,
		     size_t tlength,
		     size_t clength, uint8_t *dst, const uint8_t *src);

int
ocb_decrypt_message (const struct ocb_key *ocb_key,
		     const void *encrypt_ctx, nettle_cipher_func *encrypt,
		     const void *decrypt_ctx, nettle_cipher_func *decrypt,
		     size_t nlength, const uint8_t *nonce,
		     size_t alength, const uint8_t *adata,
		     size_t tlength,
		     size_t mlength, uint8_t *dst, const uint8_t *src);

/* OCB-AES */
/* This struct represents an expanded key for ocb-aes encryption. For
   decryption, a separate decryption context is needed as well. */
struct ocb_aes128_encrypt_key
{
  struct ocb_key ocb;
  struct aes128_ctx encrypt;
};

void
ocb_aes128_set_encrypt_key (struct ocb_aes128_encrypt_key *ocb, const uint8_t *key);

void
ocb_aes128_set_decrypt_key (struct ocb_aes128_encrypt_key *ocb, struct aes128_ctx *decrypt,
			    const uint8_t *key);

void
ocb_aes128_set_nonce (struct ocb_ctx *ctx, const struct ocb_aes128_encrypt_key *key,
		      size_t tag_length, size_t nonce_length, const uint8_t *nonce);

void
ocb_aes128_update (struct ocb_ctx *ctx, const struct ocb_aes128_encrypt_key *key,
		   size_t length, const uint8_t *data);

void
ocb_aes128_encrypt(struct ocb_ctx *ctx, const struct ocb_aes128_encrypt_key *key,
		   size_t length, uint8_t *dst, const uint8_t *src);

void
ocb_aes128_decrypt(struct ocb_ctx *ctx, const struct ocb_aes128_encrypt_key *key,
		   const struct aes128_ctx *decrypt,
		   size_t length, uint8_t *dst, const uint8_t *src);

void
ocb_aes128_digest(struct ocb_ctx *ctx, const struct ocb_aes128_encrypt_key *key,
		  size_t length, uint8_t *digest);

void
ocb_aes128_encrypt_message (const struct ocb_aes128_encrypt_key *key,
			    size_t nlength, const uint8_t *nonce,
			    size_t alength, const uint8_t *adata,
			    size_t tlength,
			    size_t clength, uint8_t *dst, const uint8_t *src);

int
ocb_aes128_decrypt_message (const struct ocb_aes128_encrypt_key *key,
			    const struct aes128_ctx *decrypt,
			    size_t nlength, const uint8_t *nonce,
			    size_t alength, const uint8_t *adata,
			    size_t tlength,
			    size_t mlength, uint8_t *dst, const uint8_t *src);

#ifdef __cplusplus
}
#endif

#endif /* NETTLE_OCB_H_INCLUDED */
