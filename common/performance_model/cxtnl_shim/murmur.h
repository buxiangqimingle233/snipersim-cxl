//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#ifndef _MURMURHASH3_H_
#define _MURMURHASH3_H_

#include <stdint.h>

void MurmurHash3_x64_128 ( const void * key, int len, uint32_t seed, void * out );

#define SALT_CONSTANT 0x97c29b3a

#endif // _MURMURHASH3_H_
