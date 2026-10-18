/*
    render.c --
    Display rendering.
*/

#include "shared.h"

uint8_t sms_cram_expand_table[4];
uint8_t gg_cram_expand_table[16];

/* Background drawing function */
void (*render_bg)(int line) = NULL;
void (*render_obj)(int line) = NULL;

/* Pointer to output buffer */
uint8_t *linebuf;

/* Internal buffer for drawing non 8-bit displays */
uint8_t internal_buffer[0x100];

/* Precalculated pixel table */
uint16_t pixel[PALETTE_SIZE];

/* Dirty pattern info */
uint8_t bg_name_dirty[0x200];     /* 1= This pattern is dirty */
uint16_t bg_name_list[0x200];     /* List of modified pattern indices */
uint16_t bg_list_index;           /* # of modified patterns in list */

/* One scanline of output, native resolution, RGB565 */
uint16_t native_row[256];

/* Bitplane to packed pixel tables: one bitplane byte -> 8 nibbles (bit 0 of
   each nibble), normal and horizontally flipped. Replaces the two 256 KB
   lookup tables of the original code with 2 KB of tables. */
static uint32_t bp_norm[256];
static uint32_t bp_flip[256];

/* Sprite over background pixel priority. Replaces the 64 KB lookup table
   of the original code; the result is identical for every (bx, sx) pair. */
static inline uint8_t sprite_compose(uint8_t bx, uint8_t sx)
{
    int b  = (bx & 0x0F);
    int bp = (bx & 0x20) ? 1 : 0;
    int bf = (bx & 0x7F);
    int s  = (sx & 0x0F);
    int sf = (sx & 0x0F) | 0x10 | 0x40;

    if(bx & 0x40) return (uint8_t)bf;
    if(bp)
    {
        if(b) return (uint8_t)(bf | 0x40);
        return (uint8_t)(s ? sf : bf);
    }
    return (uint8_t)(s ? sf : bf);
}

/* Macros to access memory 32-bits at a time (from MAME's drawgfx.c) */

#ifdef ALIGN_DWORD

static inline uint32_t read_dword(void *address)
{
    if ((uint32_t)address & 3)
	{
#ifdef LSB_FIRST  /* little endian version */
        return ( *((uint8_t *)address) +
                (*((uint8_t *)address+1) << 8)  +
                (*((uint8_t *)address+2) << 16) +
                (*((uint8_t *)address+3) << 24) );
#else             /* big endian version */
        return ( *((uint8_t *)address+3) +
                (*((uint8_t *)address+2) << 8)  +
                (*((uint8_t *)address+1) << 16) +
                (*((uint8_t *)address)   << 24) );
#endif
	}
	else
        return *(uint32_t *)address;
}


static inline void write_dword(void *address, uint32_t data)
{
    if ((uint32_t)address & 3)
	{
#ifdef LSB_FIRST
            *((uint8_t *)address) =    data;
            *((uint8_t *)address+1) = (data >> 8);
            *((uint8_t *)address+2) = (data >> 16);
            *((uint8_t *)address+3) = (data >> 24);
#else
            *((uint8_t *)address+3) =  data;
            *((uint8_t *)address+2) = (data >> 8);
            *((uint8_t *)address+1) = (data >> 16);
            *((uint8_t *)address)   = (data >> 24);
#endif
		return;
  	}
  	else
        *(uint32_t *)address = data;
}
#else
#define read_dword(address) *(uint32_t *)address
#define write_dword(address,data) *(uint32_t *)address=data
#endif


/****************************************************************************/


void render_shutdown(void)
{
}

/* Initialize the rendering data */
void render_init(void)
{
    int i, x;

    make_tms_tables();

    /* Make bitplane to pixel tables */
    for(i = 0; i < 0x100; i++)
    {
        uint32_t n = 0;
        uint32_t r = 0;
        for(x = 0; x < 8; x++)
        {
            if(i & (0x80 >> x))       n |= (uint32_t)1 << (x << 2);
            if(i & (0x80 >> (7 - x))) r |= (uint32_t)1 << (x << 2);
        }
        bp_norm[i] = n;
        bp_flip[i] = r;
    }

    for(i = 0; i < 4; i++)
    {
        uint8_t c = i << 6 | i << 4 | i << 2 | i;
        sms_cram_expand_table[i] = c;
    }

    for(i = 0; i < 16; i++)
    {
        uint8_t c = i << 4 | i;
        gg_cram_expand_table[i] = c;        
    }

    render_reset();

}


/* Reset the rendering data */
void render_reset(void)
{
    int i;

    /* Clear output band */
    memset(native_row, 0, sizeof(native_row));

    /* Clear palette */
    for(i = 0; i < PALETTE_SIZE; i++)
    {
        palette_sync(i, 1);
    }

    /* Pick render routine */
    render_bg = render_bg_sms;
    render_obj = render_obj_sms;
}


/* Draw a line of the display */
void render_line(int line)
{
    /* Ensure we're within the viewport range */
    if(line >= vdp.height)
        return;

    /* Point to current line in output buffer */
    linebuf = &internal_buffer[0];

    /* Blank line (full width) */
    if(!(vdp.reg[1] & 0x40))
    {
        memset(linebuf, BACKDROP_COLOR, bitmap.width);
    }
    else
    {
        /* Draw background */
        if(render_bg != NULL)
            render_bg(line);

        /* Draw sprites */
        if(render_obj != NULL)
            render_obj(line);

        /* Blank leftmost column of display */
        if(vdp.reg[0] & 0x20)
        {
            memset(linebuf, BACKDROP_COLOR, 8);
        }
    }

    remap_internal_to_host(line);
}


static inline uint32_t get_tile_row(uint16_t attr, int row)
{
    int tile_row = (attr & 0x0400) ? (row ^ 7) : (row);
    const uint32_t *t = (attr & 0x200) ? bp_flip : bp_norm;
    uint32_t key = *(uint32_t *)&vdp.vram[((attr & 0x01FF) << 5) | (tile_row << 2)];
    return t[key & 0xFF] | (t[(key >> 8) & 0xFF] << 1) | (t[(key >> 16) & 0xFF] << 2) | (t[key >> 24] << 3);
}

static inline uint32_t get_sprite_tile_row(uint16_t attr, int row)
{
    uint32_t key = *(uint32_t *)&vdp.vram[((attr & 0x01FF) << 5) | (row << 2)];
    return bp_norm[key & 0xFF] | (bp_norm[(key >> 8) & 0xFF] << 1) | (bp_norm[(key >> 16) & 0xFF] << 2) | (bp_norm[key >> 24] << 3);
}

/* Draw the Master System background */
void render_bg_sms(int line)
{
    int locked = 0;
    int yscroll_mask = (vdp.extended) ? 256 : 224;
    int v_line = (line + vdp.reg[9]) % yscroll_mask;
    int v_row  = (v_line & 7);
    int hscroll = ((vdp.reg[0] & 0x40) && (line < 0x10)) ? 0 : (0x100 - vdp.reg[8]);
    int column = 0;
    uint16_t attr;
    uint16_t *nt = (uint16_t *)&vdp.vram[vdp.ntab + ((v_line >> 3) << 6)];
    int nt_scroll = (hscroll >> 3);
    int shift = (hscroll & 7);
    uint8_t *linebuf_ptr = (uint8_t *)&linebuf[0 - shift];

    /* Draw first column (clipped) */
    if(shift)
    {
        int x;
        for(x = shift; x < 8; x++)
            linebuf[(0 - shift) + (x)] = 0;
        column++;
    }
    
    linebuf_ptr = &linebuf_ptr[(column << 3)];

    /* Draw a line of the background */
    for(; column < 32; column++)
    {
        /* Stop vertical scrolling for leftmost eight columns */
        if((vdp.reg[0] & 0x80) && (!locked) && (column >= 24))
        {
            locked = 1;
            v_row = (line & 7);
            nt = (uint16_t *)&vdp.vram[((vdp.reg[2] << 10) & 0x3800) + ((line >> 3) << 6)];
        }

        /* Get name table attribute word */
        attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        uint8_t atex = ((attr >> 11) & 3) << 4;
        uint32_t temp = get_tile_row(attr, v_row);

        *linebuf_ptr++ = (temp & 0x0F) | atex; temp >>= 4;
        *linebuf_ptr++ = (temp & 0x0F) | atex; temp >>= 4;
        *linebuf_ptr++ = (temp & 0x0F) | atex; temp >>= 4;
        *linebuf_ptr++ = (temp & 0x0F) | atex; temp >>= 4;
        *linebuf_ptr++ = (temp & 0x0F) | atex; temp >>= 4;
        *linebuf_ptr++ = (temp & 0x0F) | atex; temp >>= 4;
        *linebuf_ptr++ = (temp & 0x0F) | atex; temp >>= 4;
        *linebuf_ptr++ = (temp & 0x0F) | atex;
    }

    /* Draw last column (clipped) */
    if(shift)
    {
        int x;
        uint8_t *p = &linebuf[(0 - shift) + (column << 3)];
        attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        uint8_t atex = ((attr >> 11) & 3) << 4;
        uint32_t temp = get_tile_row(attr, v_row);

        for(x = 0; x < shift; x++)
        {
            p[x] = (temp & 0x0F) | atex;
            temp >>= 4;
        }
    }
}




/* Draw sprites */
void render_obj_sms(int line)
{
    int i;
    uint8_t collision_buffer = 0;

    /* Sprite count for current line (8 max.) */
    int count = 0;

    /* Sprite dimensions */
    int width = 8;
    int height = (vdp.reg[1] & 0x02) ? 16 : 8;

    /* Pointer to sprite attribute table */
    uint8_t *st = (uint8_t *)&vdp.vram[vdp.satb];

    /* Adjust dimensions for double size sprites */
    if(vdp.reg[1] & 0x01)
    {
        width *= 2;
        height *= 2;
    }

    /* Draw sprites in front-to-back order */
    for(i = 0; i < 64; i++)
    {
        /* Sprite Y position */
        int yp = st[i];

        /* Found end of sprite list marker for non-extended modes? */
        if(vdp.extended == 0 && yp == 208)
            goto end;

        /* Actual Y position is +1 */
        yp++;

        /* Wrap Y coordinate for sprites > 240 */
        if(yp > 240) yp -= 256;

        /* Check if sprite falls on current line */
        if((line >= yp) && (line < (yp + height)))
        {
            uint8_t *linebuf_ptr;

            /* Width of sprite */
            int start = 0;
            int end = width;

            /* Sprite X position */
            int xp = st[0x80 + (i << 1)];

            /* Pattern name */
            int n = st[0x81 + (i << 1)];

            /* Bump sprite count */
            count++;

            /* Too many sprites on this line ? */
            if(count == 9)
            {
                vdp.status |= 0x40;                
                goto end;
            }

            /* X position shift */
            if(vdp.reg[0] & 0x08) xp -= 8;

            /* Add MSB of pattern name */
            if(vdp.reg[6] & 0x04) n |= 0x0100;

            /* Mask LSB for 8x16 sprites */
            if(vdp.reg[1] & 0x02) n &= 0x01FE;

            /* Point to offset in line buffer */
            linebuf_ptr = (uint8_t *)&linebuf[xp];

            /* Clip sprites on left edge */
            if(xp < 0)
            {
                start = (0 - xp);
            }

            /* Clip sprites on right edge */
            if((xp + width) > 256)        
            {
                end = (256 - xp);
            }

            /* Draw double size sprite */
            if(vdp.reg[1] & 0x01)
            {
                int toggle = 0;
                int x;
                uint32_t temp = get_sprite_tile_row(n, (line - yp) >> 1);

                /* Pre-shift line */
                temp >>= ((start >> 1) << 2);

                /* Draw sprite line */
                for(x = start; x < end; x++)
                {
                    /* Source pixel */
                    uint8_t sp = (temp & 0x0F);
                    if(toggle)
                        temp >>= 4;
                    toggle ^= 1;
    
                    /* Only draw opaque sprite pixels */
                    if(sp)
                    {
                        /* Background pixel from line buffer */
                        uint8_t bg = linebuf_ptr[x];
    
                        /* Look up result */
                        linebuf_ptr[x] = sprite_compose(bg, sp);
    
                        /* Update collision buffer */
                        collision_buffer |= bg;
                    }
                }
            }
            else /* Regular size sprite (8x8 / 8x16) */
            {
                int x;
                uint32_t temp = get_sprite_tile_row(n, line - yp);

                /* Pre-shift line */
                temp >>= (start << 2);

                /* Draw sprite line */
                for(x = start; x < end; x++)
                {
                    /* Source pixel */
                    uint8_t sp = (temp & 0x0F);
                    temp >>= 4;
    
                    /* Only draw opaque sprite pixels */
                    if(sp)
                    {
                        /* Background pixel from line buffer */
                        uint8_t bg = linebuf_ptr[x];
    
                        /* Look up result */
                        linebuf_ptr[x] = sprite_compose(bg, sp);
    
                        /* Update collision buffer */
                        collision_buffer |= bg;
                    }
                }
            }
        }
    }
end:
    /* Set sprite collision flag */
    if(collision_buffer & 0x40)
        vdp.status |= 0x20;
}


/* Update a palette entry */
void palette_sync(int index, int force)
{
    int r, g, b;

    // unless we are forcing an update,
    // if not in mode 4, exit


    if(IS_SMS && !force && ((vdp.reg[0] & 4) == 0) )
        return;

    if(IS_GG)
    {
        /* ----BBBBGGGGRRRR */
        r = (vdp.cram[(index << 1) | (0)] >> 0) & 0x0F;
        g = (vdp.cram[(index << 1) | (0)] >> 4) & 0x0F;
        b = (vdp.cram[(index << 1) | (1)] >> 0) & 0x0F;
    
        r = gg_cram_expand_table[r];
        g = gg_cram_expand_table[g];
        b = gg_cram_expand_table[b];
    }
    else
    {
        /* --BBGGRR */
        r = (vdp.cram[index] >> 0) & 3;
        g = (vdp.cram[index] >> 2) & 3;
        b = (vdp.cram[index] >> 4) & 3;
    
        r = sms_cram_expand_table[r];
        g = sms_cram_expand_table[g];
        b = sms_cram_expand_table[b];
    }

    pixel[index] = MAKE_PIXEL(r, g, b);
}

void remap_internal_to_host(int line)
{
    int i;

    for(i = bitmap.viewport.x; i < bitmap.viewport.w + bitmap.viewport.x; i++)
    {
        native_row[i] = pixel[ internal_buffer[i] & PIXEL_MASK ];
    }

    /* Hand the finished line straight to the host. The visible pixels start
       at native_row[bitmap.viewport.x] (0 for SMS, 48 for Game Gear, which
       renders the same 256-wide VDP but only displays the centered 160x144
       part of it). nw_push_line() (in main.c) crops to viewport.y/h,
       upscales, and pushes to the NumWorks screen. */
    nw_push_line(line, native_row);
}
