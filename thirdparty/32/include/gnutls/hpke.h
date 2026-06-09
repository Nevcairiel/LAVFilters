/*
 * Copyright © 2026 David Dudas
 *
 * Author: David Dudas <david.dudas03@e-uvt.ro>
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 */

#ifndef GNUTLS_HPKE_H
#define GNUTLS_HPKE_H

#include <gnutls/gnutls.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * gnutls_hpke_kem_t:
 * @GNUTLS_HPKE_KEM_DHKEM_P256: DHKEM using P-256
 * @GNUTLS_HPKE_KEM_DHKEM_P384: DHKEM using P-384
 * @GNUTLS_HPKE_KEM_DHKEM_P521: DHKEM using P-521
 * @GNUTLS_HPKE_KEM_DHKEM_X25519: DHKEM using X25519
 * @GNUTLS_HPKE_KEM_DHKEM_X448: DHKEM using X448
 * Enumeration of HPKE KEM algorithms.
 */
typedef enum gnutls_hpke_kem_t {
	GNUTLS_HPKE_KEM_DHKEM_P256 = 0x0010,
	GNUTLS_HPKE_KEM_DHKEM_P384 = 0x0011,
	GNUTLS_HPKE_KEM_DHKEM_P521 = 0x0012,
	GNUTLS_HPKE_KEM_DHKEM_X25519 = 0x0020,
	GNUTLS_HPKE_KEM_DHKEM_X448 = 0x0021
} gnutls_hpke_kem_t;

/**
 * gnutls_hpke_kdf_t:
 * @GNUTLS_HPKE_KDF_HKDF_SHA256: HKDF using SHA-256
 * @GNUTLS_HPKE_KDF_HKDF_SHA384: HKDF using SHA-384
 * @GNUTLS_HPKE_KDF_HKDF_SHA512: HKDF using SHA-512
 * Enumeration of HPKE KDF algorithms.
 */
typedef enum gnutls_hpke_kdf_t {
	GNUTLS_HPKE_KDF_HKDF_SHA256 = 0x0001,
	GNUTLS_HPKE_KDF_HKDF_SHA384 = 0x0002,
	GNUTLS_HPKE_KDF_HKDF_SHA512 = 0x0003
} gnutls_hpke_kdf_t;

/**
 * gnutls_hpke_aead_t:
 * @GNUTLS_HPKE_AEAD_AES_128_GCM: AES-128-GCM
 * @GNUTLS_HPKE_AEAD_AES_256_GCM: AES-256-GCM
 * @GNUTLS_HPKE_AEAD_CHACHA20_POLY1305: ChaCha20-Poly1305
 * @GNUTLS_HPKE_AEAD_EXPORT_ONLY: AEAD is unused, export operation only
 * Enumeration of HPKE AEAD algorithms.
 */
typedef enum gnutls_hpke_aead_t {
	GNUTLS_HPKE_AEAD_AES_128_GCM = 0x0001,
	GNUTLS_HPKE_AEAD_AES_256_GCM = 0x0002,
	GNUTLS_HPKE_AEAD_CHACHA20_POLY1305 = 0x0003,
	GNUTLS_HPKE_AEAD_EXPORT_ONLY = 0xFFFF
} gnutls_hpke_aead_t;

/**
 * gnutls_hpke_mode_t:
 * @GNUTLS_HPKE_MODE_BASE: HPKE base mode
 * @GNUTLS_HPKE_MODE_PSK: HPKE psk mode
 * @GNUTLS_HPKE_MODE_AUTH: HPKE auth mode
 * @GNUTLS_HPKE_MODE_AUTH_PSK: HPKE auth+psk mode
 * Enumeration of HPKE modes.
 */
typedef enum gnutls_hpke_mode_t {
	GNUTLS_HPKE_MODE_BASE = 0,
	GNUTLS_HPKE_MODE_PSK = 1,
	GNUTLS_HPKE_MODE_AUTH = 2,
	GNUTLS_HPKE_MODE_AUTH_PSK = 3
} gnutls_hpke_mode_t;

/**
 * gnutls_hpke_role_t:
 * @GNUTLS_HPKE_ROLE_SENDER: HPKE sender role
 * @GNUTLS_HPKE_ROLE_RECEIVER: HPKE receiver role
 * Enumeration of HPKE roles.
 */
typedef enum gnutls_hpke_role_t {
	GNUTLS_HPKE_ROLE_SENDER = 0,
	GNUTLS_HPKE_ROLE_RECEIVER = 1
} gnutls_hpke_role_t;

typedef struct gnutls_hpke_context_st *gnutls_hpke_context_t;

int gnutls_hpke_init(gnutls_hpke_context_t *ctx, gnutls_hpke_mode_t mode,
		     gnutls_hpke_role_t role, gnutls_hpke_kem_t kem,
		     gnutls_hpke_kdf_t kdf, gnutls_hpke_aead_t aead);

int gnutls_hpke_deinit(gnutls_hpke_context_t ctx);

int gnutls_hpke_encap(gnutls_hpke_context_t ctx, const gnutls_datum_t *info,
		      gnutls_datum_t *enc,
		      const gnutls_pubkey_t receiver_pubkey,
		      const gnutls_privkey_t sender_privkey,
		      const gnutls_datum_t *psk, const gnutls_datum_t *psk_id);

int gnutls_hpke_seal(gnutls_hpke_context_t ctx, const gnutls_datum_t *aad,
		     const gnutls_datum_t *plaintext,
		     gnutls_datum_t *ciphertext);

int gnutls_hpke_decap(gnutls_hpke_context_t ctx, const gnutls_datum_t *info,
		      const gnutls_datum_t *enc,
		      const gnutls_privkey_t receiver_privkey,
		      const gnutls_pubkey_t sender_pubkey,
		      const gnutls_datum_t *psk, const gnutls_datum_t *psk_id);

int gnutls_hpke_open(gnutls_hpke_context_t ctx, const gnutls_datum_t *aad,
		     const gnutls_datum_t *ciphertext,
		     gnutls_datum_t *plaintext);

int gnutls_hpke_derive_keypair(gnutls_hpke_kem_t kem, const gnutls_datum_t *ikm,
			       gnutls_privkey_t privkey,
			       gnutls_pubkey_t pubkey);

int gnutls_hpke_export(gnutls_hpke_context_t ctx,
		       const gnutls_datum_t *exporter_context, size_t length,
		       gnutls_datum_t *secret);

#ifdef __cplusplus
}
#endif

#endif /* GNUTLS_HPKE_H */
