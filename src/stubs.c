#include "sms/shared.h"

/* Battery-backed SRAM saving is not supported (yet). */
void system_manage_sram(uint8_t *sram, int slot, int mode)
{
    (void)sram; (void)slot; (void)mode;
}
