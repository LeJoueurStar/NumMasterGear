#ifndef NW_RLE_H_
#define NW_RLE_H_

#include <stdint.h>
#include <stddef.h>

/* Encodes n bytes from src into dst (capacity dst_cap). Returns the
   encoded length, or (size_t)-1 if it wouldn't fit in dst_cap. */
size_t rle_encode(const uint8_t *src, size_t n, uint8_t *dst, size_t dst_cap);

/* Decodes n bytes from src into dst (capacity dst_cap). Returns the
   decoded length, or (size_t)-1 if the stream is truncated, malformed, or
   would overflow dst_cap -- never writes past dst_cap in that case. */
size_t rle_decode(const uint8_t *src, size_t n, uint8_t *dst, size_t dst_cap);

/* Safe upper bound on rle_encode()'s output size for an n-byte input. */
size_t rle_worst_case_size(size_t n);

#endif
