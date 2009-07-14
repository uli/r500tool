/* watches the headphone jack of a Toshiba Portege R500 for changes
   and calls an applescript that changes the sound output */
/* (C) 2009 Ulrich Hecht */
/* Licensed under the terms of the GNU Public License v2 */
/* Requires DirectIO driver found at http://code.coreboot.org/p/directio/ */

#include <unistd.h>
#include <stdlib.h>
#include "darwinio.h"
#include "audio_switch.h"

/* constants borrowed from ALSA HDA driver */
#define ICH6_REG_IC                     0x60
#define ICH6_REG_IR                     0x64
#define ICH6_REG_IRS                    0x68
#define   ICH6_IRS_VALID        (1<<1)
#define   ICH6_IRS_BUSY         (1<<0)

#define AC_VERB_GET_PIN_SENSE			0x0f09
#define AC_VERB_SET_PIN_WIDGET_CONTROL		0x707

#define AC_PINCTL_OUT_EN		(1<<6)
#define AC_PINCTL_HP_EN			(1<<7)

#define PIN_OUT		(AC_PINCTL_OUT_EN)
#define PIN_HP                  (AC_PINCTL_OUT_EN | AC_PINCTL_HP_EN)

#define DEBUG(x...)

unsigned long hda_base_addr = 0xffc3c000; /* massive FIXME... */
volatile void *hda_base;

int g_codec_addr = 0;	/* guessed; 15 seems to work as well */

void azx_writew(uint32_t reg, uint16_t val)
{
  *((uint16_t*)(hda_base + reg)) = val;
}

void azx_writel(uint32_t reg, uint32_t val)
{
  *((uint32_t*)(hda_base + reg)) = val;
}

uint16_t azx_readw(uint32_t reg)
{
  return *((uint16_t*)(hda_base + reg));
}

uint32_t azx_readl(uint32_t reg)
{
  return *((uint32_t*)(hda_base + reg));
}

int azx_single_send_cmd(uint32_t val)
{
  unsigned short w;
  unsigned int i;
  int timeout = 50;
  DEBUG("%s: sending cmd 0x%x\n", __FUNCTION__, val);
  while(timeout--) {
      w = azx_readw(ICH6_REG_IRS);
      if (w & ICH6_IRS_BUSY) {
        usleep(1);
        continue;
      }
      azx_writew(ICH6_REG_IRS, azx_readw(ICH6_REG_IRS) | ICH6_IRS_VALID);
      azx_writel(ICH6_REG_IC, val);
      azx_writew(ICH6_REG_IRS, azx_readw(ICH6_REG_IRS) | ICH6_IRS_BUSY);
      return 0;
  }
  return -1;
}

unsigned int azx_single_get_response(void)
{
  int timeout = 50;
  DEBUG("%s: waiting for response\n", __FUNCTION__);
  while(timeout--) {
    if (azx_readw(ICH6_REG_IRS) & ICH6_IRS_VALID) {
      return azx_readl(ICH6_REG_IR);
    }
  }
  return -1;
}

/*
 * Compose a 32bit command word to be sent to the HD-audio controller
 */
static inline unsigned int
make_codec_cmd(uint8_t codec_addr, uint32_t nid, int direct,
               unsigned int verb, unsigned int parm)
{
        uint32_t val;

        val = (uint32_t)(codec_addr & 0x0f) << 28;
        val |= (uint32_t)direct << 27;
        val |= (uint32_t)nid << 20;
        val |= verb << 8;
        val |= parm;
        return val;
}

unsigned int snd_hda_codec_read(uint32_t nid, int direct, unsigned int verb, unsigned int parm)
{
  unsigned int cmd = make_codec_cmd(g_codec_addr, nid, direct, verb, parm);
  DEBUG("sending command 0x%x\n", cmd);
  unsigned int r;
  if (!(r = azx_single_send_cmd(cmd))) {
    DEBUG("%s: send cmd returned 0x%x\n", __FUNCTION__, r);
    r = azx_single_get_response();
    DEBUG("%s: response 0x%x\n", __FUNCTION__, r);
    return r;
  }
  else {
    DEBUG("%s: command failed, r 0x%x\n", __FUNCTION__, r);
    return -1;
  }
  
}

int snd_hda_codec_write(uint32_t nid, int direct, unsigned int verb, unsigned int parm)
{
  unsigned int cmd = make_codec_cmd(g_codec_addr, nid, direct, verb, parm);
  int err = azx_single_send_cmd(cmd);
  return err;
}

static int oldpresent;

int init_watch_headphone_jack(void)
{
  hda_base = map_physical(hda_base_addr, 255);
  azx_writew(ICH6_REG_IRS, azx_readw(ICH6_REG_IRS) & ~ICH6_IRS_BUSY);
  oldpresent = snd_hda_codec_read(0x15, 0, AC_VERB_GET_PIN_SENSE, 0) & 0x80000000;
}

int read_headphone_jack(void)
{
  return (snd_hda_codec_read(0x15, 0, AC_VERB_GET_PIN_SENSE, 0) & 0x80000000) ? 1 : 0;
}

void set_output(char *which)
{
  ASDeviceType chosenDeviceID = getRequestedDeviceID(which, kAudioTypeOutput);
  setDevice(chosenDeviceID, kAudioTypeOutput);
}

void watch_headphone_jack(void)
{
  uint32_t bits;
  int present = read_headphone_jack();
  if (present != oldpresent) {
    //DEBUG("present 0x%x\n", present);
    if (present) {
      set_output("Built-in Headphone");
    }
    else {
      set_output("Built-in Speaker");
    }
    oldpresent = present;
  }
  return;
}
