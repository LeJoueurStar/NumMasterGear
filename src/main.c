/*
    Master System / Game Gear (SMS Plus) for the NumWorks calculator.

    - The ROM is not inside the app: it is sent together with the .nwa file
      on https://my.numworks.com/apps ("external data") and read directly
      from flash. Both .sms and .gg files work; the console is auto-detected
      from the ROM's own header.

    Controls (same for both Master System and Game Gear):
      arrows      = D-pad            OK   = button 1        Back = button 2
      Backspace or EXE = Start / Pause (whichever the game expects)
      pi          = reset
      + / -       = fill screen / original console pixel size
      1 / 2 / 3 / 4 / .  = speed x1 / x2 / x3 / x4 / x1.5
      0           = save (if the game uses battery RAM) and quit
      Home        = quit without saving
*/

#include <eadk.h>
#include "sms/shared.h"
#include "common/epsilon_files.h"   /* record storage + Home protection (NumPlay headers) */
#include "rle.h"

/* Defined in sms/vdp.c, not declared in its header. */
void set_tms_palette(void);

/* Build switches (set from the Makefile):
     NW_ENABLE_STORAGE=1  include the persistent-save code (storage.c / rle.c).
                          0 builds a version with NO storage code at all.
     NW_BARE=1            diagnostic build: no emulation, just a text screen
                          that waits for Home -- used to tell apart a crash
                          caused by the emulator from one caused by the
                          app's size/build. */
#ifndef NW_ENABLE_STORAGE
#define NW_ENABLE_STORAGE 1
#endif
#ifndef NW_BARE
#define NW_BARE 0
#endif

#define SCREEN_W 320
#define MESSAGE_MS 250
#define SCREEN_H 240

/* ---------------------------------------------------------------- zoom */

/* Integer-only scaling (no float/double anywhere in this app): the output
   is scaled by the exact ratio zoom_num / zoom_den. */
static int   zoom_fit = 1;                 /* 1 = fill screen, 0 = native */
static int   zoom_num = 1, zoom_den = 1;
static int   zoom_out_w, zoom_out_h, zoom_off_x, zoom_off_y;
static int   zoom_col_src[SCREEN_W];

static void recompute_zoom(void)
{
    int vw = bitmap.viewport.w, vh = bitmap.viewport.h;
    int x;

    if(vw <= 0) vw = 256;
    if(vh <= 0) vh = 192;

    if(zoom_fit)
    {
        /* scale = min(SCREEN_W / vw, SCREEN_H / vh), compared exactly by
           cross-multiplying instead of dividing. */
        if(SCREEN_W * vh <= SCREEN_H * vw) { zoom_num = SCREEN_W; zoom_den = vw; }
        else                               { zoom_num = SCREEN_H; zoom_den = vh; }
    }
    else
    {
        zoom_num = 1;
        zoom_den = 1;
    }

    /* round to nearest */
    zoom_out_w = (vw * zoom_num * 2 + zoom_den) / (2 * zoom_den);
    zoom_out_h = (vh * zoom_num * 2 + zoom_den) / (2 * zoom_den);
    if(zoom_out_w > SCREEN_W) zoom_out_w = SCREEN_W;
    if(zoom_out_h > SCREEN_H) zoom_out_h = SCREEN_H;
    zoom_off_x = (SCREEN_W - zoom_out_w) / 2;
    zoom_off_y = (SCREEN_H - zoom_out_h) / 2;

    for(x = 0; x < zoom_out_w; x++)
    {
        int src = (x * zoom_den) / zoom_num;
        if(src >= vw) src = vw - 1;
        zoom_col_src[x] = src;
    }
}

/* Called by render.c once per native scanline. `row` holds the finished
   line (RGB565); the visible pixels start at row[bitmap.viewport.x]. */
void nw_push_line(int line, const uint16_t *row)
{
    int local, y0, y1, y;
    static uint16_t out_row[SCREEN_W];
    const uint16_t *vis = row + bitmap.viewport.x;

    if(line < bitmap.viewport.y || line >= bitmap.viewport.y + bitmap.viewport.h)
        return;
    local = line - bitmap.viewport.y;

    y0 = (local * zoom_num) / zoom_den;
    y1 = ((local + 1) * zoom_num) / zoom_den;
    if(y1 > zoom_out_h) y1 = zoom_out_h;
    if(y0 >= y1) return;

    if(zoom_num == zoom_den)
    {
        eadk_rect_t r = { (uint16_t)zoom_off_x, (uint16_t)(zoom_off_y + y0),
                           (uint16_t)zoom_out_w, 1 };
        eadk_display_push_rect(r, (const eadk_color_t *)vis);
        return;
    }

    {
        int x;
        for(x = 0; x < zoom_out_w; x++) out_row[x] = vis[zoom_col_src[x]];
    }
    for(y = y0; y < y1; y++)
    {
        eadk_rect_t r = { (uint16_t)zoom_off_x, (uint16_t)(zoom_off_y + y),
                           (uint16_t)zoom_out_w, 1 };
        eadk_display_push_rect(r, (const eadk_color_t *)out_row);
    }
}

/* ------------------------------------------------------------- display */

static void clear_screen(void)
{
    eadk_display_push_rect_uniform(eadk_screen_rect, eadk_color_black);
}

static void flash_message(const char *l1, const char *l2)
{
    eadk_rect_t box = { 20, 100, 280, 46 };
    eadk_display_push_rect_uniform(box, eadk_color_black);
    eadk_display_draw_string(l1, (eadk_point_t){ 28, 106 }, true, eadk_color_white, eadk_color_black);
    if(l2)
        eadk_display_draw_string(l2, (eadk_point_t){ 28, 126 }, false, eadk_color_white, eadk_color_black);
    eadk_timing_msleep(MESSAGE_MS);   /* how long a save/erase message stays on screen */
    /* In the zoomed-out view the black border around the game is not redrawn
       by the game each frame, so the message's text would stay there: clear
       the whole screen after the message. (In the filled view the game covers
       it on the next frame anyway.) */
    clear_screen();
}

/* ----------------------------------------------------------- save/load */

/* Saves are Epsilon records (see common/epsilon_files.h), written and read
   with the same careful helpers NumPlay uses. Record name: "sms" + 4 hex
   digits of the ROM's CRC + ".sav" (Epsilon names need a base and an
   extension). The full CRC is also stored inside the record and checked on
   load, so a name clash between two ROMs can never load the wrong state.

   Record layout:  [tag 4][ROM crc 4][raw length 4][RLE-compressed state]
   Raw state:      [vdp][sms][cart.fcr x4][Z80 context][trimmed cart.sram] */
#if NW_ENABLE_STORAGE

#define STATE_FORMAT_TAG 0x534D5333u   /* "SMS3" -- bump if the layout changes */

static void save_name(char *out)
{
    static const char hex[] = "0123456789ABCDEF";
    uint32_t crc = cart.crc;
    out[0] = 's'; out[1] = 'm'; out[2] = 's';
    out[3] = hex[(crc >> 12) & 0xF]; out[4] = hex[(crc >> 8) & 0xF];
    out[5] = hex[(crc >> 4) & 0xF];  out[6] = hex[crc & 0xF];
    out[7] = '.'; out[8] = 's'; out[9] = 'a'; out[10] = 'v'; out[11] = 0;
}

static uint16_t sram_used_length(void)
{
    size_t i = sizeof(cart.sram);
    while(i > 0 && cart.sram[i - 1] == 0) i--;
    return (uint16_t)i;
}

size_t state_fixed_size(void)
{
    return sizeof(vdp) + sizeof(sms) + sizeof(cart.fcr) + sizeof(Z80_Regs);
}

void load_full_state(void)
{
    char name[16];
    uint32_t reclen = 0, tag, crc, raw_len;
    const uint8_t *rec;
    size_t fixed, decoded;
    uint8_t *raw;
    int (*irq_cb)(int);
    const uint8_t *p;

    memset(cart.sram, 0, sizeof(cart.sram));

    save_name(name);
    rec = ef_read(name, &reclen);
    if(!rec || reclen < 12) return;

    memcpy(&tag, rec, 4);
    memcpy(&crc, rec + 4, 4);
    memcpy(&raw_len, rec + 8, 4);
    if(tag != STATE_FORMAT_TAG || crc != cart.crc) return;   /* other format, or another ROM: ignore */

    fixed = state_fixed_size();
    if(raw_len < fixed || raw_len > fixed + sizeof(cart.sram)) return;

    raw = (uint8_t *)malloc(raw_len);
    if(!raw) return;

    decoded = rle_decode(rec + 12, (size_t)reclen - 12, raw, raw_len);
    if(decoded != raw_len) { free(raw); return; }

    p = raw;
    memcpy(&vdp, p, sizeof(vdp));          p += sizeof(vdp);
    memcpy(&sms, p, sizeof(sms));          p += sizeof(sms);
    memcpy(cart.fcr, p, sizeof(cart.fcr)); p += sizeof(cart.fcr);

    /* The Z80 memory map is derived from cart.fcr and is not saved: rebuild it,
       or the CPU keeps reading the ROM banks of the start of the session. */
    sms_restore_mapping();

    /* Keep THIS session's interrupt callback, not the one from the snapshot. */
    irq_cb = Z80_Context->irq_callback;
    z80_set_context((void *)p);            p += sizeof(Z80_Regs);
    Z80_Context->irq_callback = irq_cb;

    {
        size_t sram_len = raw_len - fixed;
        if(sram_len) memcpy(cart.sram, p, sram_len);
    }
    free(raw);

    /* pixel[] (the colour table used for drawing) is derived from CRAM and is
       NOT part of the snapshot: rebuild it now, otherwise the restored picture
       keeps the colours of the start of the session (glitched textures). */
    if(IS_GG || (vdp.reg[0] & 4))
    {
        int i;
        for(i = 0; i < PALETTE_SIZE; i++) palette_sync(i, 1);
    }
    else
    {
        set_tms_palette();
    }
}

void save_full_state_and_report(void)
{
    char name[16];
    uint16_t sram_len = sram_used_length();
    size_t raw_len = state_fixed_size() + sram_len;
    uint8_t *raw = (uint8_t *)malloc(raw_len);
    uint8_t *p, *record;
    size_t comp_cap, comp_len;
    uint32_t tag = STATE_FORMAT_TAG, crc = cart.crc, raw_len32 = (uint32_t)raw_len;

    if(!raw)
    {
        flash_message("Sauvegarde impossible", "(memoire insuffisante)");
        return;
    }

    p = raw;
    memcpy(p, &vdp, sizeof(vdp));         p += sizeof(vdp);
    memcpy(p, &sms, sizeof(sms));         p += sizeof(sms);
    memcpy(p, cart.fcr, sizeof(cart.fcr)); p += sizeof(cart.fcr);
    z80_get_context(p);                    p += sizeof(Z80_Regs);
    if(sram_len) memcpy(p, cart.sram, sram_len);

    comp_cap = 12 + rle_worst_case_size(raw_len);
    record = (uint8_t *)malloc(comp_cap);
    if(!record)
    {
        free(raw);
        flash_message("Sauvegarde impossible", "(memoire insuffisante)");
        return;
    }
    memcpy(record, &tag, 4);
    memcpy(record + 4, &crc, 4);
    memcpy(record + 8, &raw_len32, 4);
    comp_len = rle_encode(raw, raw_len, record + 12, comp_cap - 12);
    free(raw);

    if(comp_len == (size_t)-1)
    {
        free(record);
        flash_message("Sauvegarde impossible", "(etat trop volumineux)");
        return;
    }

    save_name(name);
    if(ef_write(name, record, (uint32_t)(12 + comp_len)))
        flash_message("Sauvegarde", "enregistree.");
    else
        flash_message("Sauvegarde impossible", "(stockage plein)");

    free(record);
}

/* Erases the save of the game being played, and only that one. */
static void wipe_save_and_report(void)
{
    char name[16];
    uint32_t len = 0;

    save_name(name);
    if(!ef_read(name, &len))
    {
        flash_message("Rien a effacer", "(pas de sauvegarde)");
        return;
    }
    if(ef_remove(name))
        flash_message("Sauvegarde effacee", "(ce jeu seulement)");
    else
        flash_message("Effacement impossible", "(voir README)");
}

#else  /* NW_ENABLE_STORAGE == 0: no storage code is built or run at all */

size_t state_fixed_size(void) { return 0; }

void load_full_state(void)
{
    memset(cart.sram, 0, sizeof(cart.sram));
}

void save_full_state_and_report(void)
{
    flash_message("Sauvegarde desactivee", "(version de test)");
}

static void wipe_save_and_report(void)
{
    flash_message("Effacement desactive", "(version de test)");
}

#endif /* NW_ENABLE_STORAGE */

/* --------------------------------------------------------------- misc */

static void show_no_rom(void)
{
    clear_screen();
    eadk_display_draw_string("Aucune ROM", (eadk_point_t){ 10, 90 }, true, eadk_color_white, eadk_color_black);
    eadk_display_draw_string("Envoyez un fichier .sms ou .gg", (eadk_point_t){ 10, 120 }, false, eadk_color_white, eadk_color_black);
    eadk_display_draw_string("avec l'appli sur my.numworks.com/apps", (eadk_point_t){ 10, 140 }, false, eadk_color_white, eadk_color_black);
    while(eadk_keyboard_scan() == 0) eadk_timing_msleep(30);
}

static uint32_t millis32(void) { return (uint32_t)eadk_timing_millis(); }


#if NW_BARE
/* Diagnostic build: draws a couple of lines of text and waits for Home.
   No emulation code runs at all (but the whole emulator is still linked in,
   so the binary has the same size / memory layout as the real thing). */
static void draw_number(int value, int x, int y)
{
    char buf[16];
    int n = 0, i;
    char tmp[16];
    if(value == 0) tmp[n++] = '0';
    while(value > 0 && n < 15) { tmp[n++] = (char)('0' + value % 10); value /= 10; }
    for(i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
    buf[n] = 0;
    eadk_display_draw_string(buf, (eadk_point_t){ (uint16_t)x, (uint16_t)y }, false, eadk_color_white, eadk_color_black);
}

static int bare_test(void)
{
    clear_screen();
    eadk_display_draw_string("Test minimal (sans emulation)", (eadk_point_t){ 10, 40 }, true, eadk_color_white, eadk_color_black);
    eadk_display_draw_string("Taille de la ROM (octets) :", (eadk_point_t){ 10, 80 }, false, eadk_color_white, eadk_color_black);
    draw_number((int)eadk_external_data_size, 10, 100);
    eadk_display_draw_string("Appuyez sur Accueil pour quitter", (eadk_point_t){ 10, 140 }, false, eadk_color_white, eadk_color_black);
    while(1)
    {
        eadk_keyboard_state_t k = eadk_keyboard_scan();
        if(eadk_keyboard_key_down(k, eadk_key_home)) break;
        eadk_timing_msleep(20);
    }
    return 0;
}
#endif

/* ---------------------------------------------------------------- main */

int main(int argc, char *argv[])
{
    const uint8_t *rom = (const uint8_t *)eadk_external_data;
    size_t size = eadk_external_data_size;
    int speed2 = 2;   /* speed in half-units: 2=x1, 3=x1.5, 4=x2, 6=x3, 8=x4 */
    uint32_t t0, frames;
    int skip;
    uint32_t toolbox_held_since = 0;
    int toolbox_action_done = 0;
    (void)argc; (void)argv;

    np_app_begin();   /* Home is now a key, not an exit: see common/epsilon_app.h */
#if NW_BARE
    bare_test();
    return np_app_end();
#endif

    if(!load_rom_flash(rom, size))
    {
        show_no_rom();
        return np_app_end();
    }

    /* bitmap.width feeds the renderer's "blank line" fast path; the rest of
       the legacy bitmap_t fields are unused now that nw_push_line() handles
       every scanline directly. */
    bitmap.width = 256;

    memset(&input, 0, sizeof(input));

    system_init();
    system_reset();
    load_full_state();
    recompute_zoom();
    clear_screen();

    t0 = millis32();
    frames = 0;
    skip = 0;

    while(1)
    {
        eadk_keyboard_state_t k = eadk_keyboard_scan();
        uint32_t pad;
        int fps;
        uint32_t deadline;
        int32_t ahead;

        if(eadk_keyboard_key_down(k, eadk_key_home))
            break;

        /* Keyboard ghosting: holding "." and moving the arrows can make the
           keyboard report "0" (or "sqrt") that was not pressed. A save-and-quit
           must not happen by accident, so these keys only count when no other
           key is down. */
        {
            eadk_keyboard_state_t others_down_for_0 = k & ~((eadk_keyboard_state_t)1 << eadk_key_zero);
            eadk_keyboard_state_t others_down_for_sqrt = k & ~((eadk_keyboard_state_t)1 << eadk_key_sqrt);
            if(eadk_keyboard_key_down(k, eadk_key_zero) && others_down_for_0 == 0)
            {
                save_full_state_and_report();
                break;
            }
            if(eadk_keyboard_key_down(k, eadk_key_sqrt) && others_down_for_sqrt == 0)
            {
                save_full_state_and_report();
            }
        }

        if(eadk_keyboard_key_down(k, eadk_key_toolbox))
        {
            if(toolbox_held_since == 0) toolbox_held_since = millis32();
            else if(!toolbox_action_done && millis32() - toolbox_held_since >= 1500)
            {
                wipe_save_and_report();
                toolbox_action_done = 1;
            }
        }
        else
        {
            toolbox_held_since = 0;
            toolbox_action_done = 0;
        }

        if(eadk_keyboard_key_down(k, eadk_key_pi))
        {
            system_reset();
        }

        if(eadk_keyboard_key_down(k, eadk_key_plus) && !zoom_fit)
        {
            zoom_fit = 1; recompute_zoom(); clear_screen();
        }
        if(eadk_keyboard_key_down(k, eadk_key_minus) && zoom_fit)
        {
            zoom_fit = 0; recompute_zoom(); clear_screen();
        }

        {
            int new_speed2 = speed2;
            if(eadk_keyboard_key_down(k, eadk_key_one))   new_speed2 = 2;
            if(eadk_keyboard_key_down(k, eadk_key_two))   new_speed2 = 4;
            if(eadk_keyboard_key_down(k, eadk_key_three)) new_speed2 = 6;
            if(eadk_keyboard_key_down(k, eadk_key_four))  new_speed2 = 8;
            if(eadk_keyboard_key_down(k, eadk_key_dot))   new_speed2 = 3;
            if(new_speed2 != speed2)
            {
                /* Restart the pacing clock: otherwise the next deadline is
                   computed against the OLD speed's frame count, which can
                   land far in the past (freezes) or far in the future
                   (multi-second stall) as soon as the speed changes. */
                speed2 = new_speed2;
                t0 = millis32();
                frames = 0;
            }
        }

        pad = 0;
        if(eadk_keyboard_key_down(k, eadk_key_up))        pad |= INPUT_UP;
        if(eadk_keyboard_key_down(k, eadk_key_down))      pad |= INPUT_DOWN;
        if(eadk_keyboard_key_down(k, eadk_key_left))      pad |= INPUT_LEFT;
        if(eadk_keyboard_key_down(k, eadk_key_right))     pad |= INPUT_RIGHT;
        if(eadk_keyboard_key_down(k, eadk_key_ok))        pad |= INPUT_BUTTON1;
        if(eadk_keyboard_key_down(k, eadk_key_back))      pad |= INPUT_BUTTON2;
        input.pad[0]  = pad;

        /* Pause (Master System) and Start (Game Gear) are both read from
           input.system, not input.pad[] -- writing Start into pad[0], as
           an earlier version did, meant it was silently never seen by the
           Game Gear I/O code no matter which key triggered it. */
        input.system = 0;
        /* Retour arriere is the pause/Start key on both consoles: on the Master
           System it is the pause (NMI); on the Game Gear it is Start. EXE is not used. */
        if(eadk_keyboard_key_down(k, eadk_key_backspace))
            input.system |= IS_GG ? INPUT_START : INPUT_PAUSE;

        if(bitmap.viewport.changed)
        {
            recompute_zoom();
            bitmap.viewport.changed = 0;
        }

        system_frame(skip);

        frames++;
        fps = (sms.display == DISPLAY_PAL) ? FPS_PAL : FPS_NTSC;
        deadline = t0 + (uint32_t)(((uint64_t)frames * 2000u) / ((uint64_t)fps * (uint64_t)speed2));
        ahead = (int32_t)(deadline - millis32());
        if(ahead > 0)
        {
            eadk_timing_msleep((uint32_t)ahead);
            skip = 0;
        }
        else if(ahead < -200)
        {
            t0 = millis32();
            frames = 0;
            skip = 0;
        }
        else
        {
            skip = (ahead < -20) ? 1 : 0;
        }
    }

    /* Leave through the NumPlay protocol: np_app_end() clears this app's RAM
       and only then gives Home back to Epsilon, so the calculator does not
       inherit a broken RAM state (the likely cause of the crashes on exit). */
    return np_app_end();
}
