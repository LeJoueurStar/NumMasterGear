/*
    sound.c --
    Silent replacement of the original sound code (SN76489 + YM2413).
    The NumWorks calculator has no speaker, so all audio writes are ignored.
    This removes ~0.5 MB of emulation tables and every use of malloc().
*/

#include "shared.h"

snd_t snd;

void psg_write(int data)              { (void)data; }
void psg_stereo_w(int data)           { (void)data; }

/* FM unit detection registers are kept so that games probing for it behave
   exactly as on a console without FM unit. */
int  fmunit_detect_r(void)            { return sms.fm_detect; }
void fmunit_detect_w(int data)        { sms.fm_detect = data; }
void fmunit_write(int offset, int data) { (void)offset; (void)data; }

int  sound_init(void)                 { snd.enabled = 0; return 0; }
void sound_shutdown(void)             { }
void sound_reset(void)                { }
void sound_update(int line)           { (void)line; }
void sound_mixer_callback(int16_t **stream, int16_t **output, int length)
{
    (void)stream; (void)output; (void)length;
}
