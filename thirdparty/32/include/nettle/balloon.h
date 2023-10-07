/* balloon.h

   Balloon password-hashing algorithm.

   Copyright (C) 2022 Zoltan Fridrich
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

/* For a description of the algorithm, see:
 * Boneh, D., Corrigan-Gibbs, H., Schechter, S. (2017, May 12). Balloon Hashing:
 * A Memory-Hard Function Providing Provable Protection Against Sequential Attacks.
 * Retrieved Sep 1, 2022, from https://eprint.iacr.org/2016/027.pdf
 */

#ifndef NETTLE_BALLOON_H_INCLUDED
#define NETTLE_BALLOON_H_INCLUDED

#include "nettle-types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Name mangling */
#define balloon nettle_balloon
#define balloon_itch nettle_balloon_itch
#define balloon_sha1 nettle_balloon_sha1
#define balloon_sha256 nettle_balloon_sha256
#define balloon_sha384 nettle_balloon_sha384
#define balloon_sha512 nettle_balloon_sha512

void
balloon(void *hash_ctx,
        nettle_hash_update_func *update,
        nettle_hash_digest_func *digest,
        size_t digest_size, size_t s_cost, size_t t_cost,
        size_t passwd_length, const uint8_t *passwd,
        size_t salt_length, const uint8_t *salt,
        uint8_t *scratch, uint8_t *dst);

size_t
balloon_itch(size_t digest_size, size_t s_cost);

void
balloon_sha1(size_t s_cost, size_t t_cost,
             size_t passwd_length, const uint8_t *passwd,
             size_t salt_length, const uint8_t *salt,
             uint8_t *scratch, uint8_t *dst);

void
balloon_sha256(size_t s_cost, size_t t_cost,
               size_t passwd_length, const uint8_t *passwd,
               size_t salt_length, const uint8_t *salt,
               uint8_t *scratch, uint8_t *dst);

void
balloon_sha384(size_t s_cost, size_t t_cost,
               size_t passwd_length, const uint8_t *passwd,
               size_t salt_length, const uint8_t *salt,
               uint8_t *scratch, uint8_t *dst);

void
balloon_sha512(size_t s_cost, size_t t_cost,
               size_t passwd_length, const uint8_t *passwd,
               size_t salt_length, const uint8_t *salt,
               uint8_t *scratch, uint8_t *dst);

#ifdef __cplusplus
}
#endif

#endif /* NETTLE_BALLOON_H_INCLUDED */
