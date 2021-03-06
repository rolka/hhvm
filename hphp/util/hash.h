/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2016 Facebook, Inc. (http://www.facebook.com)     |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifndef incl_HPHP_HASH_H_
#define incl_HPHP_HASH_H_

#include <stdint.h>
#include <cstring>

#include "hphp/util/portability.h"

#if defined __x86_64__
#  if (!defined USE_SSECRC)
#    define USE_SSECRC
#  endif
#else
#  undef USE_SSECRC
#endif

// Killswitch
#if NO_SSECRC
#  undef USE_SSECRC
#endif

namespace HPHP {

bool IsSSEHashSupported();

///////////////////////////////////////////////////////////////////////////////

using strhash_t = int32_t;
using inthash_t = int32_t;
const strhash_t STRHASH_MASK = 0x7fffffff;
const strhash_t STRHASH_MSB  = 0x80000000;

inline long long hash_int64(long long key) {
  // "64 bit Mix Functions", from Thomas Wang's "Integer Hash Function."
  // http://www.concentric.net/~ttwang/tech/inthash.htm
  key = (~key) + (key << 21); // key = (key << 21) - key - 1;
  key = key ^ ((unsigned long long)key >> 24);
  key = (key + (key << 3)) + (key << 8); // key * 265
  key = key ^ ((unsigned long long)key >> 14);
  key = (key + (key << 2)) + (key << 4); // key * 21
  key = key ^ ((unsigned long long)key >> 28);
  key = key + (key << 31);
  return key < 0 ? -key : key;
}

inline long long hash_int64_pair(long long k1, long long k2) {
  // Shift the first key, so (a,b) hashes somewhere other than (b,a)
  return (hash_int64(k1) << 1) ^ hash_int64(k2);
}

namespace MurmurHash3 {
///////////////////////////////////////////////////////////////////////////////
// The following code is based on MurmurHash3:
//   http://code.google.com/p/smhasher/wiki/MurmurHash3
//
// The case-insensitive version converts lowercase characters to uppercase
// under the assumption that character data are 7-bit ASCII. This should work
// as identifiers usually only contain alphanumeric characters and the
// underscore. Although PHP allows higher ASCII characters (> 127) in an
// identifier, they should be very rare, and do not change the correctness.

#define ROTL64(x,y) rotl64(x,y)
#define BIG_CONSTANT(x) (x##LLU)

ALWAYS_INLINE uint64_t rotl64(uint64_t x, int8_t r) {
  return (x << r) | (x >> (64 - r));
}

template <bool caseSensitive>
ALWAYS_INLINE uint64_t getblock64(const uint64_t *p, int i) {
  uint64_t block = p[i];
  if (!caseSensitive) {
    block &= 0xdfdfdfdfdfdfdfdfLLU; // a-z => A-Z
  }
  return block;
}

template <bool caseSensitive>
ALWAYS_INLINE uint8_t getblock8(const uint8_t *p, int i) {
  uint8_t block = p[i];
  if (!caseSensitive) {
    block &= 0xdfU; // a-z => A-Z
  }
  return block;
}

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche
ALWAYS_INLINE uint64_t fmix64(uint64_t k) {
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xff51afd7ed558ccd);
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
  k ^= k >> 33;
  return k;
}

// Optimized for 64-bit architectures.  MurmurHash3 also implements a 128-bit
// hash that is optimized for 32-bit architectures (omitted here).
template <bool caseSensitive>
ALWAYS_INLINE void hash128(const void *key, size_t len, uint64_t seed,
                           uint64_t out[2]) {
  const uint8_t *data = (const uint8_t *)key;
  const size_t nblocks = len / 16;
  uint64_t h1 = seed;
  uint64_t h2 = seed;
  const uint64_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
  const uint64_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);

  //----------
  // body
  const uint64_t *blocks = (const uint64_t *)(data);
  for(size_t i = 0; i < nblocks; i++)
  {
    uint64_t k1 = getblock64<caseSensitive>(blocks,i*2+0);
    uint64_t k2 = getblock64<caseSensitive>(blocks,i*2+1);
    k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;
    h1 = ROTL64(h1,27); h1 += h2; h1 = h1*5+0x52dce729;
    k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;
    h2 = ROTL64(h2,31); h2 += h1; h2 = h2*5+0x38495ab5;
  }

  //----------
  // tail
  const uint8_t *tail = (const uint8_t*)(data + nblocks*16);
  uint64_t k1 = 0;
  uint64_t k2 = 0;
  switch(len & 15)
  {
  case 15: k2 ^= uint64_t(getblock8<caseSensitive>(tail, 14)) << 48;
  case 14: k2 ^= uint64_t(getblock8<caseSensitive>(tail, 13)) << 40;
  case 13: k2 ^= uint64_t(getblock8<caseSensitive>(tail, 12)) << 32;
  case 12: k2 ^= uint64_t(getblock8<caseSensitive>(tail, 11)) << 24;
  case 11: k2 ^= uint64_t(getblock8<caseSensitive>(tail, 10)) << 16;
  case 10: k2 ^= uint64_t(getblock8<caseSensitive>(tail,  9)) << 8;
  case  9: k2 ^= uint64_t(getblock8<caseSensitive>(tail,  8)) << 0;
           k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

  case  8: k1 ^= uint64_t(getblock8<caseSensitive>(tail,  7)) << 56;
  case  7: k1 ^= uint64_t(getblock8<caseSensitive>(tail,  6)) << 48;
  case  6: k1 ^= uint64_t(getblock8<caseSensitive>(tail,  5)) << 40;
  case  5: k1 ^= uint64_t(getblock8<caseSensitive>(tail,  4)) << 32;
  case  4: k1 ^= uint64_t(getblock8<caseSensitive>(tail,  3)) << 24;
  case  3: k1 ^= uint64_t(getblock8<caseSensitive>(tail,  2)) << 16;
  case  2: k1 ^= uint64_t(getblock8<caseSensitive>(tail,  1)) << 8;
  case  1: k1 ^= uint64_t(getblock8<caseSensitive>(tail,  0)) << 0;
           k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;
  };

  //----------
  // finalization
  h1 ^= len; h2 ^= len;
  h1 += h2;
  h2 += h1;
  h1 = fmix64(h1);
  h2 = fmix64(h2);
  h1 += h2;
  h2 += h1;

  ((uint64_t*)out)[0] = h1;
  ((uint64_t*)out)[1] = h2;
}

#undef ROTL64
#undef BIG_CONSTANT
///////////////////////////////////////////////////////////////////////////////
} // namespace MurmurHash3

// Four functions for hashing: hash_string_(cs|i)(_unsafe)?.
//   cs: case-sensitive;
//   i: case-insensitive;
//   unsafe: safe for strings aligned at 8-byte boundary;

// Fallback versions uses CRC hash when supported, and use MurmurHash otherwise.
strhash_t hash_string_cs_fallback(const char *arKey, uint32_t nKeyLength);
strhash_t hash_string_i_fallback(const char *arKey, uint32_t nKeyLength);

#if FACEBOOK && defined USE_SSECRC

strhash_t hash_string_cs_unsafe(const char *arKey, uint32_t nKeyLength);
strhash_t hash_string_i_unsafe(const char *arKey, uint32_t nKeyLength);
strhash_t hash_string_cs(const char *arKey, uint32_t nKeyLength);
strhash_t hash_string_i(const char *arKey, uint32_t nKeyLength);

#else

inline strhash_t hash_string_cs(const char *arKey, uint32_t nKeyLength) {
  return hash_string_cs_fallback(arKey, nKeyLength);
}

inline
strhash_t hash_string_cs_unsafe(const char *arKey, uint32_t nKeyLength) {
  return hash_string_cs_fallback(arKey, nKeyLength);
}

inline strhash_t hash_string_i(const char *arKey, uint32_t nKeyLength) {
  return hash_string_i_fallback(arKey, nKeyLength);
}

inline
strhash_t hash_string_i_unsafe(const char *arKey, uint32_t nKeyLength) {
  return hash_string_i_fallback(arKey, nKeyLength);
}

#endif

inline strhash_t hash_string(const char *arKey, uint32_t nKeyLength) {
  return hash_string_i(arKey, nKeyLength);
}

inline strhash_t hash_string_unsafe(const char *arKey, uint32_t nKeyLength) {
  return hash_string_i_unsafe(arKey, nKeyLength);
}

/**
 * We probably should get rid of this, so to detect code generation errors,
 * where a binary string is treated as a NULL-terminated literal. Do we ever
 * allow binary strings as array keys or symbol names?
 */
inline strhash_t hash_string(const char *arKey) {
  return hash_string(arKey, strlen(arKey));
}

inline strhash_t hash_string_i(const char *arKey) {
  return hash_string_i(arKey, strlen(arKey));
}

// This function returns true and sets the res parameter if arKey
// is a non-empty string that matches one of the following conditions:
//   1) The string is "0".
//   2) The string starts with a non-zero digit, followed by at most
//      18 more digits, and is less than or equal to 2^63 - 1.
//   3) The string starts with a negative sign, followed by a non-zero
//      digit, followed by at most 18 more digits, and is greater than
//      or equal to -2^63.
inline bool is_strictly_integer(const char* arKey, size_t nKeyLength,
                                int64_t& res) {
  if ((unsigned char)(arKey[0] - '-') > ('9' - '-'))
    return false;
  if (nKeyLength <= 19 ||
      (arKey[0] == '-' && nKeyLength == 20)) {
    unsigned long long num = 0;
    bool neg = false;
    unsigned i = 0;
    if (arKey[0] == '-') {
      neg = true;
      i = 1;
      // The string "-" is NOT strictly an integer
      if (nKeyLength == 1)
        return false;
      // A string that starts with "-0" is NOT strictly an integer
      if (arKey[1] == '0')
        return false;
    } else if (arKey[0] == '0') {
      // The string "0" is strictly an integer
      if (nKeyLength == 1) {
        res = 0;
        return true;
      }
      // A string that starts with "0" followed by at least one digit
      // is NOT strictly an integer
      return false;
    }
    bool good = true;
    for (; i < nKeyLength; ++i) {
      if (arKey[i] >= '0' && arKey[i] <= '9') {
        num = 10*num + (arKey[i] - '0');
      }
      else {
        good = false;
        break;
      }
    }
    if (good) {
      if (num <= 0x7FFFFFFFFFFFFFFFULL ||
          (neg && num == 0x8000000000000000ULL)) {
        res = neg ? 0 - num : (long long)num;
        return true;
      }
    }
  }
  return false;
}

struct StringData;
///////////////////////////////////////////////////////////////////////////////
}

#ifdef USE_SSECRC
// The following functions are implemented in ASM directly for x86_64.
extern "C" {
  HPHP::strhash_t hash_string_cs_crc(const char*, uint32_t);
  HPHP::strhash_t hash_string_i_crc(const char*, uint32_t);
  HPHP::strhash_t hash_string_cs_unaligned_crc(const char*, uint32_t);
  HPHP::strhash_t hash_string_i_unaligned_crc(const char*, uint32_t);
  HPHP::strhash_t g_hashHelper_crc(const HPHP::StringData*);
}
#endif

#endif // incl_HPHP_HASH_H_
