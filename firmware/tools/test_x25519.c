// Bilgisayarda calistir: cc -x c++ -std=c++17 -I../shared test_x25519.c -o /tmp/tx && /tmp/tx
#include <stdio.h>
#include <stdlib.h>
#include "x25519.h"
static void hex(uint8_t *o, const char *s) { for (int i = 0; i < 32; i++) { unsigned v; sscanf(s + 2 * i, "%2x", &v); o[i] = v; } }
static int eq(const uint8_t *a, const char *s) { uint8_t b[32]; hex(b, s); return memcmp(a, b, 32) == 0; }
int main() {
  int bad = 0; uint8_t k[32], u[32], out[32];
  // RFC 7748 5.2
  hex(k, "a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4");
  hex(u, "e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c");
  x25519_impl::scalarmult(out, k, u);
  if (!eq(out, "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552")) { puts("FAIL 5.2"); bad++; } else puts("ok 5.2");
  // RFC 7748 6.1
  uint8_t ap[32], bp[32], apub[32], bpub[32], s1[32], s2[32];
  hex(ap, "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
  hex(bp, "5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb");
  x25519_public(apub, ap); x25519_public(bpub, bp);
  if (!eq(apub, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a")) { puts("FAIL alice pub"); bad++; } else puts("ok alice pub");
  if (!eq(bpub, "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f")) { puts("FAIL bob pub"); bad++; } else puts("ok bob pub");
  x25519_shared(s1, ap, bpub); x25519_shared(s2, bp, apub);
  if (!eq(s1, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742") || memcmp(s1, s2, 32)) { puts("FAIL shared"); bad++; } else puts("ok shared");
  return bad;
}
