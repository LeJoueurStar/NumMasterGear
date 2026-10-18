/*
    rle.c -- small, defensive PackBits-style byte compressor.

    Used to shrink full-state save snapshots (VRAM/RAM compress well: lots
    of repeated bytes) so they have a better chance of fitting in the
    small, shared storage area. rle_decode() is deliberately paranoid: it
    never writes past dst_cap or reads past src_len, and returns failure
    on any malformed, truncated, or oversized stream rather than trusting
    partial data. That matters here specifically because a record under
    our filename could be leftover data from an older, incompatible save
    format -- it must be safely rejected, never partially applied.
*/

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "rle.h"

size_t rle_encode(const uint8_t *src, size_t n, uint8_t *dst, size_t dst_cap)
{
    size_t i = 0, o = 0;
    while(i < n)
    {
        size_t run = 1;
        while(i + run < n && src[i + run] == src[i] && run < 129) run++;
        if(run >= 2)
        {
            if(o + 2 > dst_cap) return (size_t)-1;
            dst[o++] = (uint8_t)(128 + (run - 2));
            dst[o++] = src[i];
            i += run;
        }
        else
        {
            size_t lit = 1;
            while(i + lit < n && lit < 128)
            {
                if(i + lit + 1 < n && src[i + lit] == src[i + lit + 1]) break;
                lit++;
            }
            if(o + 1 + lit > dst_cap) return (size_t)-1;
            dst[o++] = (uint8_t)(lit - 1);
            memcpy(dst + o, src + i, lit);
            o += lit;
            i += lit;
        }
    }
    return o;
}

size_t rle_decode(const uint8_t *src, size_t n, uint8_t *dst, size_t dst_cap)
{
    size_t i = 0, o = 0;
    while(i < n)
    {
        uint8_t c = src[i++];
        if(c < 128)
        {
            size_t lit = (size_t)c + 1;
            if(i + lit > n) return (size_t)-1;
            if(o + lit > dst_cap) return (size_t)-1;
            memcpy(dst + o, src + i, lit);
            i += lit; o += lit;
        }
        else
        {
            size_t run = (size_t)(c - 128) + 2;
            if(i + 1 > n) return (size_t)-1;
            if(o + run > dst_cap) return (size_t)-1;
            memset(dst + o, src[i], run);
            i += 1; o += run;
        }
    }
    return o;
}

/* NOT a bound on every conceivable input: a deliberately adversarial
   byte pattern (short, scattered 2-byte repeats fragmenting what would
   otherwise be long literal runs) can push this encoder's output past
   2x the input -- rle_encode() always detects that itself and returns
   failure rather than overflow, so going over this size is safe, just
   refused. This number instead targets realistic data (emulator RAM and
   VRAM: large uniform/blank regions plus some noise), confirmed by
   testing to need well under this much room, while staying far smaller
   than a worst-case allocation would -- the point is keeping peak RAM
   usage down on a device with very little spare, not reacting to every
   theoretical input. */
size_t rle_worst_case_size(size_t n)
{
    return n + n / 4 + 16;
}
