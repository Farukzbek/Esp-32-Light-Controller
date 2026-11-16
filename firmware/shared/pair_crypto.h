#pragma once
#include "pair_proto.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"

static inline void pair_derive_key(uint8_t key[32], const uint8_t shared[32], const uint8_t nodeMac[6], const uint8_t bridgeMac[6]) {
  uint8_t buf[32 + 6 + 6 + 17];
  memcpy(buf, shared, 32); memcpy(buf + 32, nodeMac, 6); memcpy(buf + 38, bridgeMac, 6); memcpy(buf + 44, "nowlight-pair-v1", 17);
  mbedtls_sha256(buf, sizeof(buf), key, 0);
}

/// GCM ile sifrele; ct = PairSecret boyutunda, tag 16 bayt.
static inline bool pair_encrypt(const uint8_t key[32], const uint8_t iv[12], const uint8_t nodeMac[6], const uint8_t bridgeMac[6],
                                const PairSecret &in, uint8_t *ct, uint8_t tag[16]) {
  uint8_t aad[12]; memcpy(aad, nodeMac, 6); memcpy(aad + 6, bridgeMac, 6);
  mbedtls_gcm_context c; mbedtls_gcm_init(&c);
  bool ok = mbedtls_gcm_setkey(&c, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
            mbedtls_gcm_crypt_and_tag(&c, MBEDTLS_GCM_ENCRYPT, sizeof(PairSecret), iv, 12, aad, 12, (const uint8_t *)&in, ct, 16, tag) == 0;
  mbedtls_gcm_free(&c);
  return ok;
}

static inline bool pair_decrypt(const uint8_t key[32], const uint8_t iv[12], const uint8_t nodeMac[6], const uint8_t bridgeMac[6],
                                const uint8_t *ct, const uint8_t tag[16], PairSecret &out) {
  uint8_t aad[12]; memcpy(aad, nodeMac, 6); memcpy(aad + 6, bridgeMac, 6);
  mbedtls_gcm_context c; mbedtls_gcm_init(&c);
  bool ok = mbedtls_gcm_setkey(&c, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
            mbedtls_gcm_auth_decrypt(&c, sizeof(PairSecret), iv, 12, aad, 12, tag, 16, ct, (uint8_t *)&out) == 0;
  mbedtls_gcm_free(&c);
  return ok;
}
