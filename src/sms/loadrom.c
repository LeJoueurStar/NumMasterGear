/*
    loadrom.c --
    ROM setup. The ROM is read straight from flash (NumWorks "external data"),
    so it costs no RAM and needs neither malloc() nor stdio.
*/

#include "shared.h"

typedef struct {
    uint32_t crc;
    int mapper;
    int display;
    int territory;
    char *name;
} rominfo_t;

static const rominfo_t game_list[] = {
    {0x17AB6883, MAPPER_NONE  , DISPLAY_NTSC, TERRITORY_EXPORT, "FA Tetris (KR)"                },
    {0x61E8806F, MAPPER_NONE  , DISPLAY_NTSC, TERRITORY_EXPORT, "Flash Point (KR)"              },
    {0x192949D5, MAPPER_KOREA2, DISPLAY_NTSC, TERRITORY_EXPORT, "Janggun-iuo Adeul (KR)"        },
    {0xA05258F5, MAPPER_KOREA , DISPLAY_NTSC, TERRITORY_EXPORT, "Won-Si-In (KR)"                },
    {0x83F0EEDE, MAPPER_KOREA , DISPLAY_NTSC, TERRITORY_EXPORT, "Street Master (KR)"            },
    {0x445525E2, MAPPER_KOREA , DISPLAY_NTSC, TERRITORY_EXPORT, "Penguin Adventure (KR)"        },
    {0x29822980, MAPPER_CODIES, DISPLAY_PAL,  TERRITORY_EXPORT, "Cosmic Spacehead"              },
    {0xB9664AE1, MAPPER_CODIES, DISPLAY_PAL,  TERRITORY_EXPORT, "Fantastic Dizzy"               },
    {0xA577CE46, MAPPER_CODIES, DISPLAY_PAL,  TERRITORY_EXPORT, "Micro Machines"                },
    {0x8813514B, MAPPER_CODIES, DISPLAY_PAL,  TERRITORY_EXPORT, "Excellent Dizzy (Proto)"       },
    {0xAA140C9C, MAPPER_CODIES, DISPLAY_PAL,  TERRITORY_EXPORT, "Excellent Dizzy (Proto - GG)"  },
    {0         , -1           , -1         ,  -1               , NULL                           },
};

uint32_t crc32(uint32_t crc, const void *buf, size_t size)
{
    static const uint32_t tab[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };
    const uint8_t *p = (const uint8_t *)buf;

    crc = ~crc;
    while(size--)
    {
        crc ^= *p++;
        crc = (crc >> 4) ^ tab[crc & 15];
        crc = (crc >> 4) ^ tab[crc & 15];
    }
    return ~crc;
}

/* Sega header ("TMR SEGA" + region/size byte), used to tell a Game Gear
   ROM from a Master System one. The CPU always sees it at address 0x7FF0;
   which *file* offset that lands on depends on how many 16K pages the ROM
   has, since low-page ROMs get mirrored into that address:
     - 32K+           -> the header sits in the real 2nd page: file 0x7FF0
     - 16K (1 page)    -> page 1 mirrors page 0: file 0x3FF0
     - 8K              -> mirrors down further: file 0x1FF0
   Falls back to Master System if no valid header is found. */
static int detect_console(const uint8_t *rom, size_t size)
{
    static const size_t offsets[] = { 0x7FF0, 0x3FF0, 0x1FF0 };
    static const size_t min_size[] = { 0x8000, 0x4000, 0x2000 };
    int i;

    for(i = 0; i < 3; i++)
    {
        size_t off = offsets[i];
        if(size < min_size[i]) continue;
        if(memcmp(rom + off, "TMR SEGA", 8) != 0) continue;

        {
            uint8_t region = rom[off + 0x0F] >> 4;
            if(region == 5 || region == 6 || region == 7) return CONSOLE_GG;
            if(region == 3 || region == 4) return CONSOLE_SMS;
        }
    }
    return CONSOLE_SMS;
}

int load_rom_flash(const uint8_t *rom, size_t size)
{
    int i;

    /* Don't load games smaller than 16K */
    if(size < 0x4000) return 0;

    /* Take care of image header, if present */
    if((size / 512) & 1)
    {
        rom  += 512;
        size -= 512;
    }

    cart.rom   = (uint8_t *)rom;
    cart.pages = (uint8_t)(size / 0x4000);
    cart.crc   = crc32(0, rom, size);

    /* Assign default settings (US NTSC machine) */
    cart.mapper     = MAPPER_SEGA;
    sms.display     = DISPLAY_NTSC;
    sms.territory   = TERRITORY_EXPORT;
    sms.console     = detect_console(rom, size);

    /* Look up mapper in game list */
    for(i = 0; game_list[i].name != NULL; i++)
    {
        if(cart.crc == game_list[i].crc)
        {
            cart.mapper     = game_list[i].mapper;
            sms.display     = game_list[i].display;
            sms.territory   = game_list[i].territory;
        }
    }

    system_assign_device(PORT_A, DEVICE_PAD2B);
    system_assign_device(PORT_B, DEVICE_PAD2B);

    return 1;
}
