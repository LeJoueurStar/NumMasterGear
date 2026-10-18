#include <stdint.h>

/* Information read by the NumWorks installer (name, API level; the icon is
   added by the Makefile from icon.png). The diagnostic builds (see the
   Makefile) use a different name so they can sit next to the real one on
   the calculator without being confused with it. */
#ifndef NW_APP_NAME
#define NW_APP_NAME "NumMasterGear"
#endif

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = NW_APP_NAME;
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
