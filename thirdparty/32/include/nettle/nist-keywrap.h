/* nist-keywrap.h

   AES Key Wrap function.
   implements RFC 3394
   https://tools.ietf.org/html/rfc3394

   Copyright (C) 2021 Nicolas Mora
                 2021 Niels Möller

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

#ifndef NETTLE_NIST_KEYWRAP_H_INCLUDED
#define NETTLE_NIST_KEYWRAP_H_INCLUDED

#include "nettle-types.h"
#include "aes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Name mangling */
#define nist_keywrap16 nettle_nist_keywrap16
#define nist_keyunwrap16 nettle_nist_keyunwrap16
#define aes128_keywrap nettle_aes128_keywrap
#define aes192_keywrap nettle_aes192_keywrap
#define aes256_keywrap nettle_aes256_keywrap
#define aes128_keyunwrap nettle_aes128_keyunwrap
#define aes192_keyunwrap nettle_aes192_keyunwrap
#define aes256_keyunwrap nettle_aes256_keyunwrap

void
nist_keywrap16 (const void *ctx, nettle_cipher_func *encrypt,
		const uint8_t *iv, size_t ciphertext_length,
		uint8_t *ciphertext, const uint8_t *cleartext);

int
nist_keyunwrap16 (const void *ctx, nettle_cipher_func *decrypt,
        const uint8_t *iv, size_t cleartext_length,
        uint8_t *cleartext, const uint8_t *ciphertext);

void
aes128_keywrap (struct aes128_ctx *ctx,
		const uint8_t *iv, size_t ciphertext_length,
		uint8_t *ciphertext, const uint8_t *cleartext);

void
aes192_keywrap (struct aes192_ctx *ctx,
		const uint8_t *iv, size_t ciphertext_length,
		uint8_t *ciphertext, const uint8_t *cleartext);

void
aes256_keywrap (struct aes256_ctx *ctx,
		const uint8_t *iv, size_t ciphertext_length,
		uint8_t *ciphertext, const uint8_t *cleartext);

int
aes128_keyunwrap (struct aes128_ctx *ctx,
		  const uint8_t *iv, size_t cleartext_length,
		  uint8_t *cleartext, const uint8_t *ciphertext);

int
aes192_keyunwrap (struct aes192_ctx *ctx,
		  const uint8_t *iv, size_t cleartext_length,
		  uint8_t *cleartext, const uint8_t *ciphertext);

int
aes256_keyunwrap (struct aes256_ctx *ctx,
		  const uint8_t *iv, size_t cleartext_length,
		  uint8_t *cleartext, const uint8_t *ciphertext);

#ifdef __cplusplus
}
#endif

#endif				/* NETTLE_NIST_KEYWRAP_H_INCLUDED */
