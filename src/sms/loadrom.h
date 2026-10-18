#ifndef _LOADROM_H_
#define _LOADROM_H_

#include <stddef.h>
#include <stdint.h>

/* Use a ROM image that already sits in memory (flash): nothing is copied. */
int load_rom_flash(const uint8_t *rom, size_t size);

/* Standard CRC-32 (same result as zlib's crc32) */
uint32_t crc32(uint32_t crc, const void *buf, size_t size);

#endif /* _LOADROM_H_ */
